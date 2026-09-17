// FM-010 extends the accepted FM-008 window and FM-009 specimen tray while
// preserving their tested native rendering path. The panel now owns only
// presentation state; durable lineage/favourites live in SessionModel/project v2.

#include "platform/win32/main_window.hpp"

#include "faultmine/colour.hpp"
#include "faultmine/editor.hpp"
#include "faultmine/image.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/specimen_tray.hpp"
#include "faultmine/wic_io.hpp"
#include "render/d3d11/canvas_renderer.hpp"

#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
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

constexpr wchar_t kTrayClassName[] = L"FAULTMINE.SpecimenTray";
constexpr wchar_t kTrayPropertyName[] = L"FAULTMINE.FM010.Tray";
constexpr int kTrayHeight = 242;
constexpr UINT kTrayRenderMessage = WM_APP + 31U;

enum TrayCommandId : UINT {
    command_tray_generate = 1401U,
    command_tray_reroll = 1402U,
    command_tray_promote = 1403U,
    command_tray_pin = 1404U,
    command_tray_radius_low = 1405U,
    command_tray_radius_medium = 1406U,
    command_tray_radius_high = 1407U,
    command_tray_breed = 1408U,
    command_lineage_activate = 1409U,
    command_lineage_provenance = 1410U,

    control_tray_seed = 1451U,
    control_tray_radius = 1452U,
    control_tray_count = 1453U,
    control_tray_generate = 1454U,
    control_tray_reroll = 1455U,
    control_tray_promote = 1456U,
    control_tray_pin = 1457U,
    control_tray_breed = 1458U,
    control_lineage_combo = 1459U,
    control_lineage_activate = 1460U,
    control_lineage_provenance = 1461U,
};

enum TrayHotkeyId : int {
    hotkey_tray_generate = 1501,
    hotkey_tray_reroll = 1502,
    hotkey_tray_promote = 1503,
    hotkey_tray_pin = 1504,
    hotkey_tray_radius_down = 1505,
    hotkey_tray_radius_up = 1506,
    hotkey_tray_breed = 1507,
};

class ExplorerTrayPanel {
public:
    explicit ExplorerTrayPanel(MainWindow& owner) : owner_(owner) {}

    ~ExplorerTrayPanel() {
        unregister_hotkeys();
        if (owner_.hwnd_ != nullptr && IsWindow(owner_.hwnd_) != FALSE) {
            RemovePropW(owner_.hwnd_, kTrayPropertyName);
            if (old_parent_proc_ != nullptr) {
                SetWindowLongPtrW(owner_.hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_parent_proc_));
            }
        }
    }

    [[nodiscard]] bool attach(std::string* error = nullptr) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = static_cast<UINT>(sizeof(window_class));
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &ExplorerTrayPanel::panel_proc;
        window_class.hInstance = owner_.instance_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        window_class.lpszClassName = kTrayClassName;
        const ATOM atom = RegisterClassExW(&window_class);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            if (error != nullptr) *error = "could not register specimen tray window class";
            return false;
        }

        panel_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            kTrayClassName,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0,
            100,
            kTrayHeight,
            owner_.hwnd_,
            nullptr,
            owner_.instance_,
            this);
        if (panel_ == nullptr) {
            if (error != nullptr) *error = "could not create specimen tray panel";
            return false;
        }
        create_controls();
        create_menu_items();
        sync_controls_from_config();
        sync_lineage_controls();
        layout();

        SetPropW(owner_.hwnd_, kTrayPropertyName, reinterpret_cast<HANDLE>(this));
        SetLastError(ERROR_SUCCESS);
        old_parent_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            owner_.hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ExplorerTrayPanel::parent_proc)));
        if (old_parent_proc_ == nullptr && GetLastError() != ERROR_SUCCESS) {
            RemovePropW(owner_.hwnd_, kTrayPropertyName);
            if (error != nullptr) *error = "could not subclass the FAULTMINE main window for exploration commands";
            return false;
        }
        register_hotkeys();
        seen_source_identity_ = owner_.session_.source_identity();
        return true;
    }

    [[nodiscard]] bool smoke_explore(std::string* error) {
        if (!owner_.session_.has_source()) {
            if (error != nullptr) *error = "smoke exploration requires the smoke source";
            return false;
        }
        const std::string root_identity = owner_.session_.genome_identity();
        app::SpecimenTrayConfig config = tray_.config();
        config.population_size = 4U;
        config.radius = core::MutationRadius::medium;
        if (!tray_.generate(
                owner_.session_.genome(), owner_.session_.locks(), owner_.session_.registry(), config, error)) {
            return false;
        }
        const std::uint64_t token = tray_.generation_token();
        while (tray_.busy()) {
            std::string render_error;
            if (!tray_.render_next(
                    *owner_.session_.full_source(), owner_.session_.source_identity(),
                    owner_.session_.registry(), token, &render_error)) {
                if (error != nullptr) *error = render_error.empty() ? "smoke tray render stalled" : render_error;
                return false;
            }
        }
        if (tray_.items().size() < 2U || !tray_.items()[0].preview.has_value() || !tray_.items()[1].preview.has_value()) {
            if (error != nullptr) *error = "smoke tray did not produce two usable previews";
            return false;
        }
        if (!owner_.session_.retain_mutation_specimen(
                tray_.items()[0].genome, tray_.items()[0].provenance, true, error) ||
            !owner_.session_.retain_mutation_specimen(
                tray_.items()[1].genome, tray_.items()[1].provenance, false, error)) {
            return false;
        }
        std::vector<core::Genome> parents{tray_.items()[0].genome, tray_.items()[1].genome};
        if (!owner_.session_.breed_and_promote(parents, config.mutation_seed, error)) return false;
        const std::string crossover_identity = owner_.session_.genome_identity();
        if (!owner_.session_.activate_lineage_specimen(root_identity, error) ||
            !owner_.session_.activate_lineage_specimen(crossover_identity, error)) {
            return false;
        }
        return owner_.session_.ensure_preview(error);
    }

    [[nodiscard]] const app::SpecimenTrayModel& tray_model() const noexcept {
        return tray_;
    }

private:
    static LRESULT CALLBACK panel_proc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        ExplorerTrayPanel* self = reinterpret_cast<ExplorerTrayPanel*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            self = static_cast<ExplorerTrayPanel*>(create->lpCreateParams);
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
        auto* self = reinterpret_cast<ExplorerTrayPanel*>(GetPropW(window, kTrayPropertyName));
        if (self == nullptr || self->old_parent_proc_ == nullptr) {
            return DefWindowProcW(window, message, w_param, l_param);
        }
        if (message == WM_COMMAND) {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (self->handle_tray_command(command)) return 0;
        }
        if (message == WM_HOTKEY) {
            if (self->handle_hotkey(static_cast<int>(w_param))) return 0;
        }
        if (message == WM_DESTROY) {
            self->unregister_hotkeys();
            RemovePropW(window, kTrayPropertyName);
            return CallWindowProcW(self->old_parent_proc_, window, message, w_param, l_param);
        }

        const LRESULT result = CallWindowProcW(self->old_parent_proc_, window, message, w_param, l_param);
        if (message == WM_SIZE) {
            self->layout();
        } else if (message == WM_COMMAND) {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (command == command_open_image || command == command_open_project) {
                self->reset_if_source_changed();
                self->sync_lineage_controls();
            }
            self->restore_selected_comparison();
        } else if (message == WM_KEYDOWN || message == kRenderMessage) {
            self->restore_selected_comparison();
        }
        return result;
    }

    LRESULT handle_panel_message(
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        switch (message) {
            case WM_COMMAND:
                if (handle_control_command(
                        static_cast<UINT>(LOWORD(w_param)),
                        static_cast<UINT>(HIWORD(w_param)))) {
                    return 0;
                }
                break;
            case WM_LBUTTONDOWN: {
                const int x = static_cast<short>(LOWORD(l_param));
                const int y = static_cast<short>(HIWORD(l_param));
                const auto hit = item_at(x, y);
                if (!hit.has_value()) return 0;
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                    if (tray_.toggle_crossover_parent(*hit)) {
                        InvalidateRect(panel_, nullptr, FALSE);
                        const std::uint32_t rank = tray_.items()[*hit].crossover_rank;
                        update_status(rank == 0U
                            ? "Removed specimen from crossover parent set."
                            : "Selected specimen as crossover parent #" + std::to_string(rank) + ".");
                    }
                } else if (tray_.select(*hit)) {
                    restore_selected_comparison();
                    InvalidateRect(panel_, nullptr, FALSE);
                    update_status("Selected specimen " + std::to_string(*hit) + " for comparison. Ctrl+click toggles ordered crossover parents.");
                }
                return 0;
            }
            case WM_PAINT:
                paint();
                return 0;
            case WM_SIZE:
                layout_controls();
                InvalidateRect(panel_, nullptr, FALSE);
                return 0;
            case kTrayRenderMessage:
                render_one(static_cast<std::uint64_t>(w_param));
                return 0;
            case WM_ERASEBKGND:
                return 1;
            default:
                break;
        }
        return DefWindowProcW(panel_, message, w_param, l_param);
    }

    void create_controls() {
        const auto make = [&](const wchar_t* klass, const wchar_t* text, const DWORD style, const UINT id) {
            HWND control = CreateWindowExW(
                0, klass, text, WS_CHILD | WS_VISIBLE | style,
                0, 0, 80, 24, panel_,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), owner_.instance_, nullptr);
            owner_.set_default_font(control);
            return control;
        };

        seed_label_ = make(L"STATIC", L"Seed", SS_LEFT, 0U);
        seed_edit_ = make(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, control_tray_seed);
        radius_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST, control_tray_radius);
        count_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST, control_tray_count);
        generate_button_ = make(L"BUTTON", L"Generate", BS_PUSHBUTTON, control_tray_generate);
        reroll_button_ = make(L"BUTTON", L"Reroll", BS_PUSHBUTTON, control_tray_reroll);
        promote_button_ = make(L"BUTTON", L"Promote", BS_PUSHBUTTON, control_tray_promote);
        pin_button_ = make(L"BUTTON", L"Favourite", BS_PUSHBUTTON, control_tray_pin);
        breed_button_ = make(L"BUTTON", L"Breed", BS_PUSHBUTTON, control_tray_breed);
        lineage_label_ = make(L"STATIC", L"Lineage", SS_LEFT, 0U);
        lineage_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, control_lineage_combo);
        lineage_activate_button_ = make(L"BUTTON", L"Activate", BS_PUSHBUTTON, control_lineage_activate);
        provenance_button_ = make(L"BUTTON", L"Provenance", BS_PUSHBUTTON, control_lineage_provenance);

        SendMessageW(radius_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"Low radius"));
        SendMessageW(radius_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"Medium radius"));
        SendMessageW(radius_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"High radius"));
        SendMessageW(count_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"4 specimens"));
        SendMessageW(count_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"8 specimens"));
        SendMessageW(count_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(L"12 specimens"));
    }

    void create_menu_items() {
        HMENU explore = CreatePopupMenu();
        AppendMenuW(explore, MF_STRING, command_tray_generate, L"&Generate tray\tCtrl+Alt+G");
        AppendMenuW(explore, MF_STRING, command_tray_reroll, L"&Reroll descendants\tCtrl+Alt+R");
        AppendMenuW(explore, MF_STRING, command_tray_promote, L"&Promote selected\tCtrl+Alt+Enter");
        AppendMenuW(explore, MF_STRING, command_tray_pin, L"Toggle &favourite\tCtrl+Alt+P");
        AppendMenuW(explore, MF_STRING, command_tray_breed, L"&Breed selected parents\tCtrl+Alt+B");
        AppendMenuW(explore, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(explore, MF_STRING, command_lineage_activate, L"Activate selected &lineage specimen");
        AppendMenuW(explore, MF_STRING, command_lineage_provenance, L"Show &provenance");
        AppendMenuW(explore, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(explore, MF_STRING, command_tray_radius_low, L"Radius: &Low");
        AppendMenuW(explore, MF_STRING, command_tray_radius_medium, L"Radius: &Medium");
        AppendMenuW(explore, MF_STRING, command_tray_radius_high, L"Radius: &High");
        AppendMenuW(owner_.menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(explore), L"E&xplore");
        DrawMenuBar(owner_.hwnd_);
    }

    void register_hotkeys() {
        constexpr UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
        RegisterHotKey(owner_.hwnd_, hotkey_tray_generate, modifiers, 'G');
        RegisterHotKey(owner_.hwnd_, hotkey_tray_reroll, modifiers, 'R');
        RegisterHotKey(owner_.hwnd_, hotkey_tray_promote, modifiers, VK_RETURN);
        RegisterHotKey(owner_.hwnd_, hotkey_tray_pin, modifiers, 'P');
        RegisterHotKey(owner_.hwnd_, hotkey_tray_radius_down, modifiers, VK_LEFT);
        RegisterHotKey(owner_.hwnd_, hotkey_tray_radius_up, modifiers, VK_RIGHT);
        RegisterHotKey(owner_.hwnd_, hotkey_tray_breed, modifiers, 'B');
        hotkeys_registered_ = true;
    }

    void unregister_hotkeys() noexcept {
        if (!hotkeys_registered_ || owner_.hwnd_ == nullptr) return;
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_generate);
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_reroll);
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_promote);
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_pin);
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_radius_down);
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_radius_up);
        UnregisterHotKey(owner_.hwnd_, hotkey_tray_breed);
        hotkeys_registered_ = false;
    }

    void layout() {
        if (panel_ == nullptr || owner_.hwnd_ == nullptr) return;
        RECT client{};
        GetClientRect(owner_.hwnd_, &client);
        const int full_width = std::max<LONG>(1, client.right - client.left);
        const int full_height = std::max<LONG>(1, client.bottom - client.top);
        const int width = std::max(280, full_width - kEditorWidth - 16);
        const int y = std::max(0, full_height - kStatusHeight - kTrayHeight - 4);
        MoveWindow(panel_, 6, y, width, kTrayHeight, TRUE);
        layout_controls();
    }

    void layout_controls() {
        if (panel_ == nullptr) return;
        RECT client{};
        GetClientRect(panel_, &client);
        const int width = std::max<LONG>(1, client.right - client.left);
        int x = 8;
        const int y = 7;
        MoveWindow(seed_label_, x, y + 4, 34, 20, TRUE); x += 38;
        MoveWindow(seed_edit_, x, y, 124, 24, TRUE); x += 130;
        MoveWindow(radius_combo_, x, y, 110, 200, TRUE); x += 116;
        MoveWindow(count_combo_, x, y, 102, 160, TRUE); x += 108;
        MoveWindow(generate_button_, x, y, 66, 24, TRUE); x += 70;
        MoveWindow(reroll_button_, x, y, 60, 24, TRUE); x += 64;
        MoveWindow(promote_button_, x, y, 64, 24, TRUE); x += 68;
        MoveWindow(pin_button_, x, y, 68, 24, TRUE); x += 72;
        MoveWindow(breed_button_, x, y, 54, 24, TRUE);

        const int line_y = 36;
        MoveWindow(lineage_label_, 8, line_y + 4, 48, 20, TRUE);
        const int combo_width = std::max(120, width - 8 - 52 - 70 - 82 - 86);
        MoveWindow(lineage_combo_, 60, line_y, combo_width, 240, TRUE);
        int line_x = 64 + combo_width;
        MoveWindow(lineage_activate_button_, line_x, line_y, 76, 24, TRUE); line_x += 80;
        MoveWindow(provenance_button_, line_x, line_y, 82, 24, TRUE);
    }

    void sync_controls_from_config() {
        const app::SpecimenTrayConfig& config = tray_.config();
        SetWindowTextW(seed_edit_, utf8_to_wide(config.mutation_seed.to_string()).c_str());
        SendMessageW(radius_combo_, CB_SETCURSEL, static_cast<WPARAM>(config.radius), 0U);
        const int count_index = config.population_size <= 4U ? 0 : (config.population_size <= 8U ? 1 : 2);
        SendMessageW(count_combo_, CB_SETCURSEL, static_cast<WPARAM>(count_index), 0U);
    }

    void sync_lineage_controls() {
        if (lineage_combo_ == nullptr) return;
        SendMessageW(lineage_combo_, CB_RESETCONTENT, 0U, 0U);
        const auto& records = owner_.session_.lineage().records();
        int active_index = -1;
        for (std::size_t index = 0U; index < records.size(); ++index) {
            const app::SpecimenRecord& record = records[index];
            std::string label = "#" + std::to_string(record.creation_ordinal) + " ";
            if (record.favourite) label += "* ";
            label += std::string{app::derivation_kind_name(record.derivation.kind)} + " ";
            label += record.genome_identity.substr(0U, 12U);
            SendMessageW(lineage_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(utf8_to_wide(label).c_str()));
            if (record.genome_identity == owner_.session_.lineage().active_identity()) active_index = static_cast<int>(index);
        }
        if (active_index >= 0) SendMessageW(lineage_combo_, CB_SETCURSEL, static_cast<WPARAM>(active_index), 0U);
    }

    [[nodiscard]] std::optional<app::SpecimenTrayConfig> config_from_controls() {
        app::SpecimenTrayConfig config = tray_.config();
        std::array<wchar_t, 128> seed{};
        GetWindowTextW(seed_edit_, seed.data(), static_cast<int>(seed.size()));
        const auto parsed_seed = core::RootSeed::parse(wide_to_utf8(seed.data()));
        if (!parsed_seed.has_value()) {
            update_status("ERROR: seed is not a valid FAULTMINE root-seed literal.");
            return std::nullopt;
        }
        config.mutation_seed = *parsed_seed;
        const LRESULT radius = SendMessageW(radius_combo_, CB_GETCURSEL, 0U, 0U);
        if (radius >= 0 && radius <= 2) config.radius = static_cast<core::MutationRadius>(radius);
        const LRESULT count = SendMessageW(count_combo_, CB_GETCURSEL, 0U, 0U);
        config.population_size = count == 0 ? 4U : (count == 2 ? 12U : 8U);
        return config;
    }

    [[nodiscard]] bool handle_control_command(const UINT id, const UINT notification) {
        if (id == control_tray_generate && notification == BN_CLICKED) { generate(false); return true; }
        if (id == control_tray_reroll && notification == BN_CLICKED) { generate(true); return true; }
        if (id == control_tray_promote && notification == BN_CLICKED) { promote_selected(); return true; }
        if (id == control_tray_pin && notification == BN_CLICKED) { pin_selected(); return true; }
        if (id == control_tray_breed && notification == BN_CLICKED) { breed_selected(); return true; }
        if (id == control_lineage_activate && notification == BN_CLICKED) { activate_lineage_selected(); return true; }
        if (id == control_lineage_provenance && notification == BN_CLICKED) { show_provenance(); return true; }
        if (id == control_tray_radius && notification == CBN_SELCHANGE) {
            InvalidateRect(panel_, nullptr, FALSE);
            return true;
        }
        if (id == control_tray_count && notification == CBN_SELCHANGE) return true;
        if (id == control_lineage_combo && notification == CBN_SELCHANGE) return true;
        return false;
    }

    [[nodiscard]] bool handle_tray_command(const UINT command) {
        switch (command) {
            case command_tray_generate: generate(false); return true;
            case command_tray_reroll: generate(true); return true;
            case command_tray_promote: promote_selected(); return true;
            case command_tray_pin: pin_selected(); return true;
            case command_tray_breed: breed_selected(); return true;
            case command_lineage_activate: activate_lineage_selected(); return true;
            case command_lineage_provenance: show_provenance(); return true;
            case command_tray_radius_low: set_radius(core::MutationRadius::low); return true;
            case command_tray_radius_medium: set_radius(core::MutationRadius::medium); return true;
            case command_tray_radius_high: set_radius(core::MutationRadius::high); return true;
            default: return false;
        }
    }

    [[nodiscard]] bool handle_hotkey(const int id) {
        switch (id) {
            case hotkey_tray_generate: generate(false); return true;
            case hotkey_tray_reroll: generate(true); return true;
            case hotkey_tray_promote: promote_selected(); return true;
            case hotkey_tray_pin: pin_selected(); return true;
            case hotkey_tray_radius_down: shift_radius(-1); return true;
            case hotkey_tray_radius_up: shift_radius(1); return true;
            case hotkey_tray_breed: breed_selected(); return true;
            default: return false;
        }
    }

    void set_radius(const core::MutationRadius radius) {
        SendMessageW(radius_combo_, CB_SETCURSEL, static_cast<WPARAM>(radius), 0U);
        generate(false);
    }

    void shift_radius(const int direction) {
        LRESULT selected = SendMessageW(radius_combo_, CB_GETCURSEL, 0U, 0U);
        if (selected == CB_ERR) selected = 1;
        const LRESULT next = std::clamp<LRESULT>(selected + direction, 0, 2);
        SendMessageW(radius_combo_, CB_SETCURSEL, static_cast<WPARAM>(next), 0U);
        generate(false);
    }

    void generate(const bool reroll) {
        if (!owner_.session_.has_source() || owner_.session_.full_source() == nullptr) {
            update_status("ERROR: load a source image before generating specimens.");
            return;
        }
        auto config = config_from_controls();
        if (!config.has_value()) return;
        if (reroll) {
            config->mutation_seed = app::SpecimenTrayModel::reroll_seed(config->mutation_seed);
            SetWindowTextW(seed_edit_, utf8_to_wide(config->mutation_seed.to_string()).c_str());
        }
        std::string error;
        if (!tray_.generate(
                owner_.session_.genome(), owner_.session_.locks(), owner_.session_.registry(), *config, &error)) {
            update_status("ERROR: specimen generation failed: " + error);
            InvalidateRect(panel_, nullptr, FALSE);
            return;
        }
        seen_source_identity_ = owner_.session_.source_identity();
        InvalidateRect(panel_, nullptr, FALSE);
        update_status(
            "Specimen tray generating " + std::to_string(tray_.items().size()) +
            " candidates at " + std::string{core::mutation_radius_name(config->radius)} + " radius...");
        PostMessageW(panel_, kTrayRenderMessage, static_cast<WPARAM>(tray_.generation_token()), 0U);
    }

    void render_one(const std::uint64_t token) {
        if (token != tray_.generation_token() || !owner_.session_.has_source() || owner_.session_.full_source() == nullptr) return;
        std::string error;
        const bool processed = tray_.render_next(
            *owner_.session_.full_source(), owner_.session_.source_identity(),
            owner_.session_.registry(), token, &error);
        if (!processed && tray_.busy()) {
            update_status("ERROR: specimen thumbnail generation stalled.");
            return;
        }
        InvalidateRect(panel_, nullptr, FALSE);
        restore_selected_comparison();
        if (tray_.busy()) {
            PostMessageW(panel_, kTrayRenderMessage, static_cast<WPARAM>(token), 0U);
        } else if (!error.empty()) {
            update_status("Specimen tray ready with render errors: " + error);
        } else {
            update_status(
                "Specimen tray ready | seed " + tray_.config().mutation_seed.to_string() +
                " | radius " + std::string{core::mutation_radius_name(tray_.config().radius)} + ".");
        }
    }

    void promote_selected() {
        const app::SpecimenTrayItem* selected = tray_.selected_item();
        if (selected == nullptr) {
            update_status("ERROR: select a specimen before promotion.");
            return;
        }
        std::string error;
        if (!owner_.session_.promote_mutation_specimen(
                selected->genome, selected->provenance, selected->pinned, &error)) {
            update_status("ERROR: specimen promotion failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        sync_lineage_controls();
        update_status(
            "Promoted descendant " + std::to_string(selected->provenance.descendant_index) +
            " with durable mutation provenance; full canonical render validated before adoption.");
    }

    void pin_selected() {
        const auto selected_index = tray_.selected_index();
        if (!selected_index.has_value() || !tray_.toggle_pin(*selected_index)) {
            update_status("ERROR: select a specimen before toggling favourite state.");
            return;
        }
        const app::SpecimenTrayItem& selected = tray_.items()[*selected_index];
        std::string error;
        if (!owner_.session_.retain_mutation_specimen(
                selected.genome, selected.provenance, selected.pinned, &error)) {
            (void)tray_.toggle_pin(*selected_index);
            update_status("ERROR: could not retain specimen: " + error);
            return;
        }
        const std::string identity = core::genome_identity_hex(selected.genome);
        if (!owner_.session_.set_specimen_favourite(identity, selected.pinned, &error)) {
            update_status("ERROR: could not persist favourite state: " + error);
            return;
        }
        sync_lineage_controls();
        InvalidateRect(panel_, nullptr, FALSE);
        update_status(selected.pinned
            ? "Favourite retained durably; it survives rerolls and project save/reload."
            : "Removed durable favourite flag; retained lineage/provenance remains intact.");
    }

    void breed_selected() {
        const std::vector<std::size_t> indices = tray_.crossover_parent_indices();
        if (indices.size() < 2U) {
            update_status("ERROR: Ctrl+click at least two specimens to define ordered crossover parents.");
            return;
        }
        if (indices.size() > core::kMaximumCrossoverParents) {
            update_status("ERROR: crossover policy v1 supports at most eight parents.");
            return;
        }
        auto config = config_from_controls();
        if (!config.has_value()) return;
        std::vector<core::Genome> parents;
        parents.reserve(indices.size());
        std::string error;
        for (const std::size_t index : indices) {
            const app::SpecimenTrayItem& item = tray_.items()[index];
            if (!owner_.session_.retain_mutation_specimen(item.genome, item.provenance, item.pinned, &error)) {
                update_status("ERROR: could not retain crossover parent: " + error);
                return;
            }
            parents.push_back(item.genome);
        }
        if (!owner_.session_.breed_and_promote(parents, config->mutation_seed, &error)) {
            update_status("ERROR: crossover failed: " + error);
            return;
        }
        tray_.clear_crossover_selection();
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        sync_lineage_controls();
        InvalidateRect(panel_, nullptr, FALSE);
        update_status("Bred ordered retained parents with typed crossover v1 | " + owner_.session_.active_provenance_summary());
    }

    void activate_lineage_selected() {
        const LRESULT selected = SendMessageW(lineage_combo_, CB_GETCURSEL, 0U, 0U);
        const auto& records = owner_.session_.lineage().records();
        if (selected == CB_ERR || static_cast<std::size_t>(selected) >= records.size()) {
            update_status("ERROR: select a retained lineage specimen first.");
            return;
        }
        const std::string identity = records[static_cast<std::size_t>(selected)].genome_identity;
        std::string error;
        if (!owner_.session_.activate_lineage_specimen(identity, &error)) {
            update_status("ERROR: lineage navigation failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        sync_lineage_controls();
        update_status("Activated retained lineage specimen | " + owner_.session_.active_provenance_summary());
    }

    void show_provenance() {
        const LRESULT selected = SendMessageW(lineage_combo_, CB_GETCURSEL, 0U, 0U);
        const auto& records = owner_.session_.lineage().records();
        if (selected != CB_ERR && static_cast<std::size_t>(selected) < records.size()) {
            update_status(app::specimen_provenance_summary(records[static_cast<std::size_t>(selected)]));
        } else {
            update_status(owner_.session_.active_provenance_summary());
        }
    }

    void reset_if_source_changed() {
        const std::string identity = owner_.session_.source_identity();
        if (identity != seen_source_identity_) {
            tray_.reset();
            seen_source_identity_ = identity;
            InvalidateRect(panel_, nullptr, FALSE);
        }
    }

    void update_status(std::string message) {
        owner_.transient_status_ = std::move(message);
        owner_.update_status();
    }

    void restore_selected_comparison() {
        if (owner_.session_.view_state().show_before) return;
        const app::SpecimenTrayItem* selected = tray_.selected_item();
        if (selected == nullptr || !selected->preview.has_value()) return;
        if (auto error = owner_.renderer_.upload_image(*selected->preview); error.has_value()) {
            update_status("ERROR: specimen comparison upload failed: " + *error);
            return;
        }
        InvalidateRect(owner_.hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] RECT item_rect(const std::size_t index) const noexcept {
        RECT client{};
        GetClientRect(panel_, &client);
        const int width = std::max<LONG>(1, client.right - client.left);
        constexpr int top = 68;
        constexpr int cell_width = 146;
        constexpr int cell_height = 82;
        const int columns = std::max(1, (width - 12) / cell_width);
        const int column = static_cast<int>(index % static_cast<std::size_t>(columns));
        const int row = static_cast<int>(index / static_cast<std::size_t>(columns));
        const int left = 8 + column * cell_width;
        const int y = top + row * cell_height;
        return RECT{left, y, left + cell_width - 8, y + cell_height - 6};
    }

    [[nodiscard]] std::optional<std::size_t> item_at(const int x, const int y) const noexcept {
        for (std::size_t index = 0U; index < tray_.items().size(); ++index) {
            const RECT rect = item_rect(index);
            if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) return index;
        }
        return std::nullopt;
    }

    void paint_preview(HDC dc, const RECT& rect, const core::ImageBuffer& image) {
        if (image.width == 0U || image.height == 0U || image.bytes.empty()) return;
        std::vector<std::uint8_t> bgra(image.bytes.size());
        for (std::size_t offset = 0U; offset + 3U < image.bytes.size(); offset += 4U) {
            bgra[offset + 0U] = image.bytes[offset + 2U];
            bgra[offset + 1U] = image.bytes[offset + 1U];
            bgra[offset + 2U] = image.bytes[offset + 0U];
            bgra[offset + 3U] = image.bytes[offset + 3U];
        }
        BITMAPINFO info{};
        info.bmiHeader.biSize = static_cast<DWORD>(sizeof(BITMAPINFOHEADER));
        info.bmiHeader.biWidth = static_cast<LONG>(image.width);
        info.bmiHeader.biHeight = -static_cast<LONG>(image.height);
        info.bmiHeader.biPlanes = 1U;
        info.bmiHeader.biBitCount = 32U;
        info.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(
            dc,
            rect.left + 2,
            rect.top + 2,
            std::max<LONG>(1, rect.right - rect.left - 4),
            52,
            0,
            0,
            static_cast<int>(image.width),
            static_cast<int>(image.height),
            bgra.data(),
            &info,
            DIB_RGB_COLORS,
            SRCCOPY);
    }

    void paint() {
        PAINTSTRUCT paint_struct{};
        HDC dc = BeginPaint(panel_, &paint_struct);
        RECT client{};
        GetClientRect(panel_, &client);
        FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));

        for (std::size_t index = 0U; index < tray_.items().size(); ++index) {
            const app::SpecimenTrayItem& item = tray_.items()[index];
            const RECT rect = item_rect(index);
            FillRect(dc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            if (item.preview.has_value()) paint_preview(dc, rect, *item.preview);
            if (!item.render_error.empty()) {
                RECT error_rect = rect;
                error_rect.top += 16;
                DrawTextW(dc, L"RENDER ERROR", -1, &error_rect, DT_CENTER | DT_SINGLELINE);
            }

            const bool selected = tray_.selected_index().has_value() && *tray_.selected_index() == index;
            HPEN pen = CreatePen(PS_SOLID, selected ? 3 : 1, GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOWFRAME));
            HGDIOBJ old_pen = SelectObject(dc, pen);
            HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(dc, old_brush);
            SelectObject(dc, old_pen);
            DeleteObject(pen);

            std::wstring label = L"#" + std::to_wstring(item.provenance.descendant_index);
            if (item.pinned) label += L" [FAV]";
            if (item.crossover_rank != 0U) label += L" [P" + std::to_wstring(item.crossover_rank) + L"]";
            label += L" " + utf8_to_wide(item.provenance.mutation_seed.to_string());
            RECT label_rect = rect;
            label_rect.top = rect.top + 55;
            DrawTextW(dc, label.c_str(), -1, &label_rect, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        if (tray_.items().empty()) {
            RECT message = client;
            message.top = 92;
            DrawTextW(
                dc,
                L"Generate deterministic descendants. Click to compare; Ctrl+click 2-8 thumbnails to choose ordered crossover parents.",
                -1,
                &message,
                DT_CENTER | DT_TOP | DT_WORDBREAK);
        } else if (tray_.busy()) {
            RECT busy = client;
            busy.top = 66;
            DrawTextW(dc, L"rendering...", -1, &busy, DT_RIGHT | DT_TOP);
        }
        EndPaint(panel_, &paint_struct);
    }

    MainWindow& owner_;
    app::SpecimenTrayModel tray_;
    HWND panel_{};
    HWND seed_label_{};
    HWND seed_edit_{};
    HWND radius_combo_{};
    HWND count_combo_{};
    HWND generate_button_{};
    HWND reroll_button_{};
    HWND promote_button_{};
    HWND pin_button_{};
    HWND breed_button_{};
    HWND lineage_label_{};
    HWND lineage_combo_{};
    HWND lineage_activate_button_{};
    HWND provenance_button_{};
    WNDPROC old_parent_proc_{};
    std::string seen_source_identity_;
    bool hotkeys_registered_{};
};

}  // namespace

int run_application(const HINSTANCE instance, const int show_command, const bool smoke_test) {
    MainWindow window{instance};
    if (!window.create()) return EXIT_FAILURE;

    ExplorerTrayPanel tray{window};
    std::string tray_error;
    if (!tray.attach(&tray_error)) {
        show_error_box(nullptr, L"FAULTMINE exploration/lineage panel initialization failed:\n" + utf8_to_wide(tray_error));
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        if (!window.smoke_present()) {
            show_error_box(nullptr, L"FAULTMINE D3D11/session smoke presentation failed.");
            return EXIT_FAILURE;
        }
        if (!tray.smoke_explore(&tray_error)) {
            show_error_box(nullptr, L"FAULTMINE mutation/crossover/lineage smoke failed:\n" + utf8_to_wide(tray_error));
            return EXIT_FAILURE;
        }
        PostMessageW(nullptr, WM_NULL, 0U, 0U);
    } else {
        window.show(show_command);
    }
    if (smoke_test) {
        HWND target = FindWindowW(kWindowClassName, kWindowTitle);
        if (target != nullptr) PostMessageW(target, WM_CLOSE, 0U, 0U);
    }
    return window.message_loop();
}

}  // namespace faultmine::platform::win32
