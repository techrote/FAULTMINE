// FM-009 extends the accepted FM-008 Win32 window without copying its large
// implementation. The existing implementation is included into this translation
// unit, with private access exposed only here so the tray can share the exact
// SessionModel and D3D11 renderer. FM-010 may fold this panel into a broader
// lineage UI; keeping the FM-008 source intact avoids an unrelated rewrite now.

#include "platform/win32/main_window.hpp"

#include "faultmine/colour.hpp"
#include "faultmine/editor.hpp"
#include "faultmine/image.hpp"
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
constexpr wchar_t kTrayPropertyName[] = L"FAULTMINE.FM009.Tray";
constexpr int kTrayHeight = 210;
constexpr UINT kTrayRenderMessage = WM_APP + 31U;

enum TrayCommandId : UINT {
    command_tray_generate = 1401U,
    command_tray_reroll = 1402U,
    command_tray_promote = 1403U,
    command_tray_pin = 1404U,
    command_tray_radius_low = 1405U,
    command_tray_radius_medium = 1406U,
    command_tray_radius_high = 1407U,

    control_tray_seed = 1451U,
    control_tray_radius = 1452U,
    control_tray_count = 1453U,
    control_tray_generate = 1454U,
    control_tray_reroll = 1455U,
    control_tray_promote = 1456U,
    control_tray_pin = 1457U,
};

enum TrayHotkeyId : int {
    hotkey_tray_generate = 1501,
    hotkey_tray_reroll = 1502,
    hotkey_tray_promote = 1503,
    hotkey_tray_pin = 1504,
    hotkey_tray_radius_down = 1505,
    hotkey_tray_radius_up = 1506,
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
        layout();

        SetPropW(owner_.hwnd_, kTrayPropertyName, reinterpret_cast<HANDLE>(this));
        SetLastError(ERROR_SUCCESS);
        old_parent_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            owner_.hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ExplorerTrayPanel::parent_proc)));
        if (old_parent_proc_ == nullptr && GetLastError() != ERROR_SUCCESS) {
            RemovePropW(owner_.hwnd_, kTrayPropertyName);
            if (error != nullptr) *error = "could not subclass the FAULTMINE main window for tray commands";
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
        if (tray_.items().empty() || !tray_.items().front().preview.has_value()) {
            if (error != nullptr) *error = "smoke tray did not produce a preview";
            return false;
        }
        if (!tray_.select(0U)) return false;
        if (!owner_.session_.promote_exploration_genome(tray_.items().front().genome, error)) return false;
        return owner_.session_.ensure_preview(error);
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
                if (hit.has_value() && tray_.select(*hit)) {
                    restore_selected_comparison();
                    InvalidateRect(panel_, nullptr, FALSE);
                    update_status("Selected specimen " + std::to_string(*hit) + " for comparison.");
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

        seed_label_ = make(L"STATIC", L"Mutation seed", SS_LEFT, 0U);
        seed_edit_ = make(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, control_tray_seed);
        radius_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST, control_tray_radius);
        count_combo_ = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST, control_tray_count);
        generate_button_ = make(L"BUTTON", L"Generate", BS_PUSHBUTTON, control_tray_generate);
        reroll_button_ = make(L"BUTTON", L"Reroll", BS_PUSHBUTTON, control_tray_reroll);
        promote_button_ = make(L"BUTTON", L"Promote", BS_PUSHBUTTON, control_tray_promote);
        pin_button_ = make(L"BUTTON", L"Pin", BS_PUSHBUTTON, control_tray_pin);

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
        AppendMenuW(explore, MF_STRING, command_tray_pin, L"&Pin selected\tCtrl+Alt+P");
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
        MoveWindow(seed_label_, x, y + 4, 78, 20, TRUE); x += 82;
        MoveWindow(seed_edit_, x, y, 128, 24, TRUE); x += 134;
        MoveWindow(radius_combo_, x, y, 112, 200, TRUE); x += 118;
        MoveWindow(count_combo_, x, y, 104, 160, TRUE); x += 110;
        MoveWindow(generate_button_, x, y, 68, 24, TRUE); x += 72;
        MoveWindow(reroll_button_, x, y, 62, 24, TRUE); x += 66;
        MoveWindow(promote_button_, x, y, 66, 24, TRUE); x += 70;
        MoveWindow(pin_button_, x, y, 48, 24, TRUE);
        if (x + 48 > width) {
            MoveWindow(promote_button_, std::max(8, width - 122), y, 66, 24, TRUE);
            MoveWindow(pin_button_, std::max(78, width - 52), y, 44, 24, TRUE);
        }
    }

    void sync_controls_from_config() {
        const app::SpecimenTrayConfig& config = tray_.config();
        SetWindowTextW(seed_edit_, utf8_to_wide(config.mutation_seed.to_string()).c_str());
        SendMessageW(radius_combo_, CB_SETCURSEL, static_cast<WPARAM>(config.radius), 0U);
        int count_index = config.population_size <= 4U ? 0 : (config.population_size <= 8U ? 1 : 2);
        SendMessageW(count_combo_, CB_SETCURSEL, static_cast<WPARAM>(count_index), 0U);
    }

    [[nodiscard]] std::optional<app::SpecimenTrayConfig> config_from_controls() {
        app::SpecimenTrayConfig config = tray_.config();
        std::array<wchar_t, 128> seed{};
        GetWindowTextW(seed_edit_, seed.data(), static_cast<int>(seed.size()));
        const auto parsed_seed = core::RootSeed::parse(wide_to_utf8(seed.data()));
        if (!parsed_seed.has_value()) {
            update_status("ERROR: mutation seed is not a valid FAULTMINE root-seed literal.");
            return std::nullopt;
        }
        config.mutation_seed = *parsed_seed;
        const LRESULT radius = SendMessageW(radius_combo_, CB_GETCURSEL, 0U, 0U);
        if (radius >= 0 && radius <= 2) {
            config.radius = static_cast<core::MutationRadius>(radius);
        }
        const LRESULT count = SendMessageW(count_combo_, CB_GETCURSEL, 0U, 0U);
        config.population_size = count == 0 ? 4U : (count == 2 ? 12U : 8U);
        return config;
    }

    [[nodiscard]] bool handle_control_command(const UINT id, const UINT notification) {
        if (id == control_tray_generate && notification == BN_CLICKED) { generate(false); return true; }
        if (id == control_tray_reroll && notification == BN_CLICKED) { generate(true); return true; }
        if (id == control_tray_promote && notification == BN_CLICKED) { promote_selected(); return true; }
        if (id == control_tray_pin && notification == BN_CLICKED) { pin_selected(); return true; }
        if (id == control_tray_radius && notification == CBN_SELCHANGE) {
            InvalidateRect(panel_, nullptr, FALSE);
            return true;
        }
        if (id == control_tray_count && notification == CBN_SELCHANGE) return true;
        return false;
    }

    [[nodiscard]] bool handle_tray_command(const UINT command) {
        switch (command) {
            case command_tray_generate: generate(false); return true;
            case command_tray_reroll: generate(true); return true;
            case command_tray_promote: promote_selected(); return true;
            case command_tray_pin: pin_selected(); return true;
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
        if (token != tray_.generation_token() || !owner_.session_.has_source() ||
            owner_.session_.full_source() == nullptr) {
            return;
        }
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
        } else {
            if (!error.empty()) {
                update_status("Specimen tray ready with render errors: " + error);
            } else {
                update_status(
                    "Specimen tray ready | seed " + tray_.config().mutation_seed.to_string() +
                    " | radius " + std::string{core::mutation_radius_name(tray_.config().radius)} + ".");
            }
        }
    }

    void promote_selected() {
        const app::SpecimenTrayItem* selected = tray_.selected_item();
        if (selected == nullptr) {
            update_status("ERROR: select a specimen before promotion.");
            return;
        }
        std::string error;
        if (!owner_.session_.promote_exploration_genome(selected->genome, &error)) {
            update_status("ERROR: specimen promotion failed: " + error);
            return;
        }
        owner_.refresh_editor_controls();
        owner_.schedule_render();
        update_status(
            "Promoted descendant " + std::to_string(selected->provenance.descendant_index) +
            " from seed " + selected->provenance.mutation_seed.to_string() +
            "; full canonical render validated before adoption.");
    }

    void pin_selected() {
        const auto selected = tray_.selected_index();
        if (!selected.has_value() || !tray_.toggle_pin(*selected)) {
            update_status("ERROR: select a specimen before pinning.");
            return;
        }
        const bool pinned = tray_.items()[*selected].pinned;
        InvalidateRect(panel_, nullptr, FALSE);
        update_status(pinned ? "Pinned selected specimen; it will survive tray rerolls."
                             : "Unpinned selected specimen.");
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
        constexpr int top = 38;
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
            std::max(1, rect.right - rect.left - 4),
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
            if (item.preview.has_value()) {
                paint_preview(dc, rect, *item.preview);
            }
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
            label += item.pinned ? L" [PIN] " : L" ";
            label += utf8_to_wide(item.provenance.mutation_seed.to_string());
            RECT label_rect = rect;
            label_rect.top = rect.top + 55;
            DrawTextW(dc, label.c_str(), -1, &label_rect, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        if (tray_.items().empty()) {
            RECT message = client;
            message.top = 62;
            DrawTextW(
                dc,
                L"Generate deterministic descendants. Click a thumbnail to compare it on the main canvas.",
                -1,
                &message,
                DT_CENTER | DT_TOP | DT_WORDBREAK);
        } else if (tray_.busy()) {
            RECT busy = client;
            busy.top = 36;
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
        show_error_box(nullptr, L"FAULTMINE specimen tray initialization failed:\n" + utf8_to_wide(tray_error));
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        if (!window.smoke_present()) {
            show_error_box(nullptr, L"FAULTMINE D3D11/session smoke presentation failed.");
            return EXIT_FAILURE;
        }
        if (!tray.smoke_explore(&tray_error)) {
            show_error_box(nullptr, L"FAULTMINE specimen mutation/tray smoke failed:\n" + utf8_to_wide(tray_error));
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
