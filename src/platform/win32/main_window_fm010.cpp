// FM-010 extends the accepted FM-008 native window with the evolutionary
// specimen tray, durable favourites, typed crossover selection/breeding and
// lineage navigation. The FM-008 implementation remains the single owner of
// file/editor/canvas behavior; this translation unit composes the exploration
// surface around the same SessionModel and D3D11 renderer.

#include "platform/win32/main_window.hpp"

#include "faultmine/determinism.hpp"
#include "faultmine/image.hpp"
#include "faultmine/session.hpp"
#include "faultmine/specimen_tray.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define private public
#define run_application run_application_fm008
#include "main_window.cpp"
#undef run_application
#undef private

namespace faultmine::platform::win32 {
namespace {

constexpr wchar_t kExploreClassName[] = L"FAULTMINE.ExploreLineage";
constexpr wchar_t kExplorePropertyName[] = L"FAULTMINE.FM010.Explore";
constexpr int kExploreHeight = 220;
constexpr UINT kExploreRenderMessage = WM_APP + 32U;
constexpr int kCardWidth = 112;
constexpr int kCardGap = 6;
constexpr int kCardTop = 42;

enum ExploreCommandId : UINT {
    command_explore_generate = 1601U,
    command_explore_reroll = 1602U,
    command_explore_promote = 1603U,
    command_explore_favourite = 1604U,
    command_explore_parent = 1605U,
    command_explore_breed = 1606U,
    command_lineage_parent = 1607U,
    command_lineage_child = 1608U,
    command_lineage_next_favourite = 1609U,
    command_lineage_provenance = 1610U,
    command_radius_low = 1611U,
    command_radius_medium = 1612U,
    command_radius_high = 1613U,

    control_seed = 1651U,
    control_radius = 1652U,
    control_count = 1653U,
    control_generate = 1654U,
    control_reroll = 1655U,
    control_promote = 1656U,
    control_favourite = 1657U,
    control_parent = 1658U,
    control_breed = 1659U,
};

enum ExploreHotkeyId : int {
    hotkey_generate = 1701,
    hotkey_reroll = 1702,
    hotkey_promote = 1703,
    hotkey_favourite = 1704,
    hotkey_parent = 1705,
    hotkey_breed = 1706,
    hotkey_lineage_parent = 1707,
    hotkey_lineage_child = 1708,
    hotkey_next_favourite = 1709,
    hotkey_provenance = 1710,
    hotkey_radius_down = 1711,
    hotkey_radius_up = 1712,
};

[[nodiscard]] std::wstring widen_ascii(const std::string_view text) {
    return std::wstring{text.begin(), text.end()};
}

class ExploreLineagePanel {
public:
    explicit ExploreLineagePanel(MainWindow& owner) : owner_(owner) {}

    ~ExploreLineagePanel() {
        unregister_hotkeys();
        if (owner_.hwnd_ != nullptr && IsWindow(owner_.hwnd_) != FALSE) {
            RemovePropW(owner_.hwnd_, kExplorePropertyName);
            if (old_parent_proc_ != nullptr) {
                SetWindowLongPtrW(owner_.hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_parent_proc_));
            }
        }
    }

    [[nodiscard]] bool attach(std::string* error = nullptr) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = static_cast<UINT>(sizeof(window_class));
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &ExploreLineagePanel::panel_proc;
        window_class.hInstance = owner_.instance_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        window_class.lpszClassName = kExploreClassName;
        const ATOM atom = RegisterClassExW(&window_class);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            if (error != nullptr) *error = "could not register FM-010 exploration window class";
            return false;
        }

        panel_ = CreateWindowExW(
            WS_EX_CLIENTEDGE, kExploreClassName, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 100, kExploreHeight,
            owner_.hwnd_, nullptr, owner_.instance_, this);
        if (panel_ == nullptr) {
            if (error != nullptr) *error = "could not create FM-010 exploration panel";
            return false;
        }
        create_controls();
        create_menu_items();
        sync_controls_from_config();
        layout();

        SetPropW(owner_.hwnd_, kExplorePropertyName, reinterpret_cast<HANDLE>(this));
        SetLastError(ERROR_SUCCESS);
        old_parent_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            owner_.hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ExploreLineagePanel::parent_proc)));
        if (old_parent_proc_ == nullptr && GetLastError() != ERROR_SUCCESS) {
            RemovePropW(owner_.hwnd_, kExplorePropertyName);
            if (error != nullptr) *error = "could not subclass FAULTMINE main window for FM-010 exploration";
            return false;
        }
        register_hotkeys();
        seen_source_identity_ = owner_.session_.source_identity();
        return true;
    }

    [[nodiscard]] bool smoke_explore(std::string* error) {
        if (!owner_.session_.has_source()) {
            if (error != nullptr) *error = "FM-010 smoke exploration requires the smoke source";
            return false;
        }
        app::SpecimenTrayConfig config = tray_.config();
        config.population_size = 6U;
        config.radius = core::MutationRadius::medium;
        if (!tray_.generate(owner_.session_.genome(), owner_.session_.locks(), owner_.session_.registry(), config, error)) {
            return false;
        }
        const std::uint64_t token = tray_.generation_token();
        while (tray_.busy()) {
            std::string render_error;
            if (!tray_.render_next(
                    *owner_.session_.full_source(), owner_.session_.source_identity(),
                    owner_.session_.registry(), token, &render_error)) {
                if (error != nullptr) *error = render_error.empty() ? "FM-010 smoke tray render stalled" : render_error;
                return false;
            }
        }

        std::optional<std::size_t> first;
        std::optional<std::size_t> second;
        std::string first_identity;
        for (std::size_t index = 0U; index < tray_.items().size(); ++index) {
            if (!tray_.items()[index].preview.has_value()) continue;
            const std::string identity = core::genome_identity_hex(tray_.items()[index].genome);
            if (!first.has_value()) {
                first = index;
                first_identity = identity;
            } else if (identity != first_identity) {
                second = index;
                break;
            }
        }
        if (!first.has_value() || !second.has_value()) {
            if (error != nullptr) *error = "FM-010 smoke tray did not produce two distinct rendered descendants";
            return false;
        }

        const app::SpecimenTrayItem& a = tray_.items()[*first];
        const app::SpecimenTrayItem& b = tray_.items()[*second];
        if (!owner_.session_.set_mutation_specimen_favourite(a.genome, a.provenance, true, error)) return false;
        bool selected = false;
        if (!owner_.session_.toggle_crossover_parent(a.genome, a.provenance, &selected, error) || !selected) return false;
        if (!owner_.session_.toggle_crossover_parent(b.genome, b.provenance, &selected, error) || !selected) return false;

        core::RootSeed breed_seed = config.mutation_seed;
        bool bred = false;
        std::string breed_error;
        for (std::size_t attempt = 0U; attempt < 8U && !bred; ++attempt) {
            breed_error.clear();
            bred = owner_.session_.breed_selected(breed_seed, &breed_error);
            if (!bred) breed_seed = app::SpecimenTrayModel::reroll_seed(breed_seed);
        }
        if (!bred) {
            if (error != nullptr) *error = "FM-010 smoke crossover failed: " + breed_error;
            return false;
        }
        if (owner_.session_.lineage().parents_of(owner_.session_.genome_identity()).size() < 2U) {
            if (error != nullptr) *error = "FM-010 smoke crossover did not record multi-parent lineage";
            return false;
        }
        return owner_.session_.ensure_preview(error);
    }

private:
    static LRESULT CALLBACK panel_proc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        ExploreLineagePanel* self = reinterpret_cast<ExploreLineagePanel*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            self = static_cast<ExploreLineagePanel*>(create->lpCreateParams);
            if (self != nullptr) {
                self->panel_ = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
        }
        return self != nullptr
            ? self->handle_panel_message(message, w_param, l_param)
            : DefWindowProcW(window, message, w_param, l_param);
    }

    static LRESULT CALLBACK parent_proc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        auto* self = reinterpret_cast<ExploreLineagePanel*>(GetPropW(window, kExplorePropertyName));
        if (self == nullptr || self->old_parent_proc_ == nullptr) {
            return DefWindowProcW(window, message, w_param, l_param);
        }
        if (message == WM_COMMAND && self->handle_explore_command(static_cast<UINT>(LOWORD(w_param)))) return 0;
        if (message == WM_HOTKEY && self->handle_hotkey(static_cast<int>(w_param))) return 0;
        if (message == WM_DESTROY) {
            self->unregister_hotkeys();
            RemovePropW(window, kExplorePropertyName);
            return CallWindowProcW(self->old_parent_proc_, window, message, w_param, l_param);
        }

        const LRESULT result = CallWindowProcW(self->old_parent_proc_, window, message, w_param, l_param);
        if (message == WM_SIZE) {
            self->layout();
        } else if (message == WM_COMMAND) {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (command == command_open_image || command == command_open_project) {
                self->reset_after_open();
            }
            self->restore_selected_comparison();
        } else if (message == WM_KEYDOWN || message == kRenderMessage) {
            self->restore_selected_comparison();
        }
        return result;
    }

    LRESULT handle_panel_message(const UINT message, const WPARAM w_param, const LPARAM l_param) {
        switch (message) {
            case WM_COMMAND:
                if (handle_control_command(static_cast<UINT>(LOWORD(w_param)), static_cast<UINT>(HIWORD(w_param)))) return 0;
                break;
            case WM_LBUTTONDOWN: {
                const int x = static_cast<short>(LOWORD(l_param));
                const int y = static_cast<short>(HIWORD(l_param));
                const auto hit = item_at(x, y);
                if (hit.has_value() && tray_.select(*hit)) {
                    restore_selected_comparison();
                    InvalidateRect(panel_, nullptr, FALSE);
                    update_status("Selected descendant " + std::to_string(tray_.items()[*hit].provenance.descendant_index) + ".");
                }
                return 0;
            }
            case WM_PAINT: paint(); return 0;
            case WM_SIZE: layout_controls(); InvalidateRect(panel_, nullptr, FALSE); return 0;
            case kExploreRenderMessage: render_one(static_cast<std::uint64_t>(w_param)); return 0;
            case WM_ERASEBKGND: return 1;
            default: break;
        }
        return DefWindowProcW(panel_, message, w_param, l_param);
    }

    void create_controls() {
        const auto make = [&](const wchar_t* klass, const wchar_t* text, const DWORD style, const UINT id) {
            HWND control = CreateWindowExW(
                0, klass, text, WS_CHILD | WS_VISIBLE | style,
                0, 0, 80, 24, panel_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), owner_.instance_, nullptr);
            owner_.set_default_font(control);
            return control;
        };
        seed_edit_ = make(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, control_seed);
        radius_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST, control_radius);
        count_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST, control_count);
        generate_button_ = make(L"BUTTON", L"Generate", BS_PUSHBUTTON, control_generate);
        reroll_button_ = make(L"BUTTON", L"Reroll", BS_PUSHBUTTON, control_reroll);
        promote_button_ = make(L"BUTTON", L"Promote", BS_PUSHBUTTON, control_promote);
        favourite_button_ = make(L"BUTTON", L"Favourite", BS_PUSHBUTTON, control_favourite);
        parent_button_ = make(L"BUTTON", L"Parent", BS_PUSHBUTTON, control_parent);
        breed_button_ = make(L"BUTTON", L"Breed", BS_PUSHBUTTON, control_breed);
        SendMessageW(radius_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"Low"));
        SendMessageW(radius_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"Medium"));
        SendMessageW(radius_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"High"));
        SendMessageW(count_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"4"));
        SendMessageW(count_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"8"));
        SendMessageW(count_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"12"));
    }

    void create_menu_items() {
        HMENU explore = CreatePopupMenu();
        AppendMenuW(explore, MF_STRING, command_explore_generate, L"&Generate tray\tCtrl+Alt+G");
        AppendMenuW(explore, MF_STRING, command_explore_reroll, L"&Reroll descendants\tCtrl+Alt+R");
        AppendMenuW(explore, MF_STRING, command_explore_promote, L"&Promote selected\tCtrl+Alt+Enter");
        AppendMenuW(explore, MF_STRING, command_explore_favourite, L"Toggle &favourite\tCtrl+Alt+P");
        AppendMenuW(explore, MF_STRING, command_explore_parent, L"Toggle crossover &parent\tCtrl+Alt+M");
        AppendMenuW(explore, MF_STRING, command_explore_breed, L"&Breed selected parents\tCtrl+Alt+B");
        AppendMenuW(explore, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(explore, MF_STRING, command_lineage_parent, L"Go to first &ancestor\tCtrl+Alt+Up");
        AppendMenuW(explore, MF_STRING, command_lineage_child, L"Go to first &descendant\tCtrl+Alt+Down");
        AppendMenuW(explore, MF_STRING, command_lineage_next_favourite, L"Next favourite\tCtrl+Alt+F");
        AppendMenuW(explore, MF_STRING, command_lineage_provenance, L"Inspect provenance\tCtrl+Alt+I");
        AppendMenuW(explore, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(explore, MF_STRING, command_radius_low, L"Radius: &Low");
        AppendMenuW(explore, MF_STRING, command_radius_medium, L"Radius: &Medium");
        AppendMenuW(explore, MF_STRING, command_radius_high, L"Radius: &High");
        AppendMenuW(owner_.menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(explore), L"E&xplore");
        DrawMenuBar(owner_.hwnd_);
    }

    void register_hotkeys() {
        constexpr UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
        RegisterHotKey(owner_.hwnd_, hotkey_generate, modifiers, 'G');
        RegisterHotKey(owner_.hwnd_, hotkey_reroll, modifiers, 'R');
        RegisterHotKey(owner_.hwnd_, hotkey_promote, modifiers, VK_RETURN);
        RegisterHotKey(owner_.hwnd_, hotkey_favourite, modifiers, 'P');
        RegisterHotKey(owner_.hwnd_, hotkey_parent, modifiers, 'M');
        RegisterHotKey(owner_.hwnd_, hotkey_breed, modifiers, 'B');
        RegisterHotKey(owner_.hwnd_, hotkey_lineage_parent, modifiers, VK_UP);
        RegisterHotKey(owner_.hwnd_, hotkey_lineage_child, modifiers, VK_DOWN);
        RegisterHotKey(owner_.hwnd_, hotkey_next_favourite, modifiers, 'F');
        RegisterHotKey(owner_.hwnd_, hotkey_provenance, modifiers, 'I');
        RegisterHotKey(owner_.hwnd_, hotkey_radius_down, modifiers, VK_LEFT);
        RegisterHotKey(owner_.hwnd_, hotkey_radius_up, modifiers, VK_RIGHT);
        hotkeys_registered_ = true;
    }

    void unregister_hotkeys() noexcept {
        if (!hotkeys_registered_ || owner_.hwnd_ == nullptr) return;
        for (const int id : std::array<int, 12U>{
                 hotkey_generate, hotkey_reroll, hotkey_promote, hotkey_favourite,
                 hotkey_parent, hotkey_breed, hotkey_lineage_parent, hotkey_lineage_child,
                 hotkey_next_favourite, hotkey_provenance, hotkey_radius_down, hotkey_radius_up}) {
            UnregisterHotKey(owner_.hwnd_, id);
        }
        hotkeys_registered_ = false;
    }

    void layout() {
        if (panel_ == nullptr || owner_.hwnd_ == nullptr) return;
        RECT client{};
        GetClientRect(owner_.hwnd_, &client);
        const int full_width = std::max<LONG>(1, client.right - client.left);
        const int full_height = std::max<LONG>(1, client.bottom - client.top);
        const int width = std::max(300, full_width - kEditorWidth - 16);
        const int y = std::max(0, full_height - kStatusHeight - kExploreHeight - 4);
        MoveWindow(panel_, 6, y, width, kExploreHeight, TRUE);
        layout_controls();
    }

    void layout_controls() {
        if (panel_ == nullptr) return;
        RECT client{};
        GetClientRect(panel_, &client);
        const int width = std::max<LONG>(1, client.right - client.left);
        int x = 8;
        const int y = 7;
        MoveWindow(seed_edit_, x, y, 126, 24, TRUE); x += 132;
        MoveWindow(radius_combo_, x, y, 84, 160, TRUE); x += 90;
        MoveWindow(count_combo_, x, y, 52, 160, TRUE); x += 58;
        for (HWND control : std::array<HWND, 6U>{
                 generate_button_, reroll_button_, promote_button_, favourite_button_, parent_button_, breed_button_}) {
            if (x + 76 > width - 6) break;
            MoveWindow(control, x, y, 72, 24, TRUE);
            x += 76;
        }
    }

    [[nodiscard]] bool handle_control_command(const UINT id, const UINT notification) {
        if (notification == BN_CLICKED) {
            switch (id) {
                case control_generate: generate(false); return true;
                case control_reroll: generate(true); return true;
                case control_promote: promote_selected(); return true;
                case control_favourite: favourite_selected(); return true;
                case control_parent: toggle_parent_selected(); return true;
                case control_breed: breed_selected(); return true;
                default: break;
            }
        }
        if ((id == control_radius || id == control_count) && notification == CBN_SELCHANGE) {
            return true;
        }
        return false;
    }

    [[nodiscard]] bool handle_explore_command(const UINT command) {
        switch (command) {
            case command_explore_generate: generate(false); return true;
            case command_explore_reroll: generate(true); return true;
            case command_explore_promote: promote_selected(); return true;
            case command_explore_favourite: favourite_selected(); return true;
            case command_explore_parent: toggle_parent_selected(); return true;
            case command_explore_breed: breed_selected(); return true;
            case command_lineage_parent: navigate_parent(); return true;
            case command_lineage_child: navigate_child(); return true;
            case command_lineage_next_favourite: activate_next_favourite(); return true;
            case command_lineage_provenance: show_provenance(); return true;
            case command_radius_low: set_radius(core::MutationRadius::low); return true;
            case command_radius_medium: set_radius(core::MutationRadius::medium); return true;
            case command_radius_high: set_radius(core::MutationRadius::high); return true;
            default: return false;
        }
    }

    [[nodiscard]] bool handle_hotkey(const int id) {
        switch (id) {
            case hotkey_generate: generate(false); return true;
            case hotkey_reroll: generate(true); return true;
            case hotkey_promote: promote_selected(); return true;
            case hotkey_favourite: favourite_selected(); return true;
            case hotkey_parent: toggle_parent_selected(); return true;
            case hotkey_breed: breed_selected(); return true;
            case hotkey_lineage_parent: navigate_parent(); return true;
            case hotkey_lineage_child: navigate_child(); return true;
            case hotkey_next_favourite: activate_next_favourite(); return true;
            case hotkey_provenance: show_provenance(); return true;
            case hotkey_radius_down: adjust_radius(-1); return true;
            case hotkey_radius_up: adjust_radius(1); return true;
            default: return false;
        }
    }

    [[nodiscard]] app::SpecimenTrayConfig controls_config(std::string* error) const {
        app::SpecimenTrayConfig config = tray_.config();
        wchar_t seed_text[64]{};
        GetWindowTextW(seed_edit_, seed_text, static_cast<int>(std::size(seed_text)));
        std::string narrow;
        for (const wchar_t ch : std::wstring_view{seed_text}) narrow.push_back(static_cast<char>(ch));
        const auto seed = core::RootSeed::parse(narrow);
        if (!seed.has_value()) {
            if (error != nullptr) *error = "mutation/crossover seed must be exactly 16 hexadecimal digits";
            return config;
        }
        config.mutation_seed = *seed;
        const LRESULT radius = SendMessageW(radius_combo_, CB_GETCURSEL, 0U, 0U);
        config.radius = radius == 0 ? core::MutationRadius::low : radius == 2 ? core::MutationRadius::high : core::MutationRadius::medium;
        const LRESULT count = SendMessageW(count_combo_, CB_GETCURSEL, 0U, 0U);
        config.population_size = count == 0 ? 4U : count == 2 ? 12U : 8U;
        return config;
    }

    void sync_controls_from_config() {
        const app::SpecimenTrayConfig& config = tray_.config();
        const std::wstring seed = widen_ascii(config.mutation_seed.to_string());
        SetWindowTextW(seed_edit_, seed.c_str());
        SendMessageW(radius_combo_, CB_SETCURSEL,
            config.radius == core::MutationRadius::low ? 0U : config.radius == core::MutationRadius::high ? 2U : 1U, 0U);
        SendMessageW(count_combo_, CB_SETCURSEL,
            config.population_size <= 4U ? 0U : config.population_size >= 12U ? 2U : 1U, 0U);
    }

    void generate(const bool reroll) {
        if (!owner_.session_.has_source()) {
            update_status("ERROR: load a source image before generating specimens.");
            return;
        }
        std::string error;
        app::SpecimenTrayConfig config = controls_config(&error);
        if (!error.empty()) {
            update_status("ERROR: " + error);
            return;
        }
        if (reroll) config.mutation_seed = app::SpecimenTrayModel::reroll_seed(config.mutation_seed);
        if (!tray_.generate(owner_.session_.genome(), owner_.session_.locks(), owner_.session_.registry(), config, &error)) {
            update_status("ERROR: specimen generation failed: " + error);
            return;
        }
        sync_controls_from_config();
        InvalidateRect(panel_, nullptr, FALSE);
        PostMessageW(panel_, kExploreRenderMessage, static_cast<WPARAM>(tray_.generation_token()), 0);
        update_status(
            "Generated " + std::to_string(config.population_size) + " independently addressed " +
            std::string{core::mutation_radius_name(config.radius)} + " descendants from seed " + config.mutation_seed.to_string() + ".");
    }

    void render_one(const std::uint64_t token) {
        if (token != tray_.generation_token() || !owner_.session_.has_source()) return;
        std::string error;
        if (!tray_.render_next(
                *owner_.session_.full_source(), owner_.session_.source_identity(),
                owner_.session_.registry(), token, &error)) {
            if (!error.empty()) update_status("ERROR: specimen thumbnail render failed: " + error);
        }
        InvalidateRect(panel_, nullptr, FALSE);
        restore_selected_comparison();
        if (tray_.busy()) PostMessageW(panel_, kExploreRenderMessage, static_cast<WPARAM>(token), 0);
    }

    void promote_selected() {
        const app::SpecimenTrayItem* item = tray_.selected_item();
        if (item == nullptr) {
            update_status("ERROR: select a specimen before promotion.");
            return;
        }
        std::string error;
        if (!owner_.session_.promote_mutation_specimen(item->genome, item->provenance, &error)) {
            update_status("ERROR: specimen promotion failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        update_status("Promoted descendant with mutation provenance retained in the project lineage.");
    }

    void favourite_selected() {
        const auto selected_index = tray_.selected_index();
        const app::SpecimenTrayItem* item = tray_.selected_item();
        if (!selected_index.has_value() || item == nullptr) {
            update_status("ERROR: select a specimen before changing favourite state.");
            return;
        }
        const std::string identity = core::genome_identity_hex(item->genome);
        const app::SpecimenRecord* existing = owner_.session_.lineage().find(identity);
        const bool wanted = existing == nullptr || !existing->favourite;
        std::string error;
        if (!owner_.session_.set_mutation_specimen_favourite(item->genome, item->provenance, wanted, &error)) {
            update_status("ERROR: favourite update failed: " + error);
            return;
        }
        if (item->pinned != wanted) (void)tray_.toggle_pin(*selected_index);
        InvalidateRect(panel_, nullptr, FALSE);
        update_status(wanted ? "Retained specimen as a durable project favourite." : "Removed favourite mark; lineage ancestry remains retained.");
    }

    void toggle_parent_selected() {
        const app::SpecimenTrayItem* item = tray_.selected_item();
        if (item == nullptr) {
            update_status("ERROR: select a specimen before choosing crossover parents.");
            return;
        }
        bool selected = false;
        std::string error;
        if (!owner_.session_.toggle_crossover_parent(item->genome, item->provenance, &selected, &error)) {
            update_status("ERROR: crossover parent selection failed: " + error);
            return;
        }
        InvalidateRect(panel_, nullptr, FALSE);
        update_status(
            std::string{selected ? "Selected" : "Unselected"} + " crossover parent; " +
            std::to_string(owner_.session_.crossover_parent_selection().size()) + " parent(s) selected.");
    }

    void breed_selected() {
        std::string config_error;
        const app::SpecimenTrayConfig config = controls_config(&config_error);
        if (!config_error.empty()) {
            update_status("ERROR: " + config_error);
            return;
        }
        std::string error;
        if (!owner_.session_.breed_selected(config.mutation_seed, &error)) {
            update_status("ERROR: crossover failed: " + error);
            return;
        }
        tray_.reset();
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        InvalidateRect(panel_, nullptr, FALSE);
        update_status("Bred selected retained parents using crossover policy v1; child lineage and full canonical validation recorded.");
    }

    void navigate_parent() {
        std::string error;
        if (!owner_.session_.navigate_lineage_parent(&error)) {
            update_status("ERROR: lineage parent navigation failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        update_status("Activated retained parent specimen; manual undo history was not traversed.");
    }

    void navigate_child() {
        std::string error;
        if (!owner_.session_.navigate_lineage_child(&error)) {
            update_status("ERROR: lineage child navigation failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        update_status("Activated retained descendant specimen; manual undo history was not traversed.");
    }

    void activate_next_favourite() {
        const std::vector<std::string> favourites = owner_.session_.lineage().favourites();
        if (favourites.empty()) {
            update_status("No durable favourites are retained in this project.");
            return;
        }
        const std::string active = owner_.session_.genome_identity();
        auto current = std::find(favourites.begin(), favourites.end(), active);
        const std::string& wanted = current == favourites.end() || ++current == favourites.end() ? favourites.front() : *current;
        std::string error;
        if (!owner_.session_.activate_lineage_specimen(wanted, &error)) {
            update_status("ERROR: favourite activation failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        update_status("Activated next durable favourite from project lineage.");
    }

    void show_provenance() {
        update_status(owner_.session_.active_provenance_summary());
    }

    void set_radius(const core::MutationRadius radius) {
        const LRESULT index = radius == core::MutationRadius::low ? 0 : radius == core::MutationRadius::high ? 2 : 1;
        SendMessageW(radius_combo_, CB_SETCURSEL, static_cast<WPARAM>(index), 0U);
        update_status("Mutation radius set to " + std::string{core::mutation_radius_name(radius)} + ".");
    }

    void adjust_radius(const int direction) {
        LRESULT index = SendMessageW(radius_combo_, CB_GETCURSEL, 0U, 0U);
        if (index == CB_ERR) index = 1;
        index = std::clamp<LRESULT>(index + direction, 0, 2);
        SendMessageW(radius_combo_, CB_SETCURSEL, static_cast<WPARAM>(index), 0U);
    }

    void reset_after_open() {
        const std::string identity = owner_.session_.source_identity();
        tray_.reset();
        seen_source_identity_ = identity;
        sync_controls_from_config();
        InvalidateRect(panel_, nullptr, FALSE);
    }

    void update_status(std::string message) {
        owner_.transient_status_ = std::move(message);
        owner_.update_status();
    }

    void restore_selected_comparison() {
        if (owner_.session_.view_state().show_before) return;
        const app::SpecimenTrayItem* item = tray_.selected_item();
        if (item == nullptr || !item->preview.has_value()) return;
        if (auto error = owner_.renderer_.upload_image(*item->preview); error.has_value()) {
            update_status("ERROR: specimen comparison upload failed: " + *error);
            return;
        }
        InvalidateRect(owner_.hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] std::optional<std::size_t> item_at(const int x, const int y) const noexcept {
        if (y < kCardTop) return std::nullopt;
        const int relative = x - 8;
        if (relative < 0) return std::nullopt;
        const int stride = kCardWidth + kCardGap;
        const std::size_t index = static_cast<std::size_t>(relative / stride);
        if (index >= tray_.items().size() || relative % stride >= kCardWidth) return std::nullopt;
        return index;
    }

    void draw_thumbnail(HDC dc, const RECT& rect, const core::ImageBuffer& image) const {
        if (image.bytes.empty() || image.width == 0U || image.height == 0U) return;
        std::vector<std::uint8_t> bgra(image.bytes.size());
        for (std::size_t pixel = 0U; pixel < image.bytes.size() / 4U; ++pixel) {
            bgra[pixel * 4U + 0U] = image.bytes[pixel * 4U + 2U];
            bgra[pixel * 4U + 1U] = image.bytes[pixel * 4U + 1U];
            bgra[pixel * 4U + 2U] = image.bytes[pixel * 4U + 0U];
            bgra[pixel * 4U + 3U] = image.bytes[pixel * 4U + 3U];
        }
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = static_cast<LONG>(image.width);
        info.bmiHeader.biHeight = -static_cast<LONG>(image.height);
        info.bmiHeader.biPlanes = 1U;
        info.bmiHeader.biBitCount = 32U;
        info.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(
            dc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            0, 0, static_cast<int>(image.width), static_cast<int>(image.height),
            bgra.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    }

    void paint() {
        PAINTSTRUCT paint_state{};
        HDC dc = BeginPaint(panel_, &paint_state);
        RECT client{};
        GetClientRect(panel_, &client);
        FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));
        SetBkMode(dc, TRANSPARENT);

        const auto selected_index = tray_.selected_index();
        const auto& selected_parents = owner_.session_.crossover_parent_selection();
        for (std::size_t index = 0U; index < tray_.items().size(); ++index) {
            const int left = 8 + static_cast<int>(index) * (kCardWidth + kCardGap);
            if (left >= client.right) break;
            RECT card{left, kCardTop, left + kCardWidth, client.bottom - 6};
            FillRect(dc, &card, GetSysColorBrush(COLOR_WINDOW));
            FrameRect(dc, &card, GetSysColorBrush(selected_index == index ? COLOR_HIGHLIGHT : COLOR_WINDOWFRAME));
            const app::SpecimenTrayItem& item = tray_.items()[index];
            RECT image_rect{card.left + 3, card.top + 3, card.right - 3, std::min(card.bottom - 34, card.top + 116)};
            if (item.preview.has_value()) draw_thumbnail(dc, image_rect, *item.preview);

            const std::string identity = core::genome_identity_hex(item.genome);
            const app::SpecimenRecord* record = owner_.session_.lineage().find(identity);
            const bool favourite = record != nullptr && record->favourite;
            const bool parent = std::find(selected_parents.begin(), selected_parents.end(), identity) != selected_parents.end();
            std::string label = "#" + std::to_string(item.provenance.descendant_index);
            if (favourite || item.pinned) label += " [FAV]";
            if (parent) label += " [PARENT]";
            const std::wstring wide = widen_ascii(label);
            RECT text_rect{card.left + 4, card.bottom - 30, card.right - 4, card.bottom - 4};
            DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &text_rect, DT_LEFT | DT_END_ELLIPSIS | DT_SINGLELINE | DT_VCENTER);
        }
        EndPaint(panel_, &paint_state);
    }

    MainWindow& owner_;
    app::SpecimenTrayModel tray_;
    HWND panel_{};
    WNDPROC old_parent_proc_{};
    std::string seen_source_identity_;
    HWND seed_edit_{};
    HWND radius_combo_{};
    HWND count_combo_{};
    HWND generate_button_{};
    HWND reroll_button_{};
    HWND promote_button_{};
    HWND favourite_button_{};
    HWND parent_button_{};
    HWND breed_button_{};
    bool hotkeys_registered_{};
};

}  // namespace

int run_application(const HINSTANCE instance, const int show_command, const bool smoke_test) {
    MainWindow window{instance};
    if (!window.create()) return EXIT_FAILURE;

    ExploreLineagePanel explore{window};
    std::string explore_error;
    if (!explore.attach(&explore_error)) {
        const std::wstring message = widen_ascii("Could not initialize FM-010 exploration UI: " + explore_error);
        MessageBoxW(window.hwnd_, message.c_str(), L"FAULTMINE", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        std::string smoke_error;
        if (!window.smoke_present(&smoke_error)) return EXIT_FAILURE;
        if (!explore.smoke_explore(&smoke_error)) return EXIT_FAILURE;
        return EXIT_SUCCESS;
    }

    ShowWindow(window.hwnd_, show_command);
    UpdateWindow(window.hwnd_);
    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) break;
        if (result == -1) return EXIT_FAILURE;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

}  // namespace faultmine::platform::win32
