#include "platform/win32/main_window.hpp"

#include "faultmine/colour.hpp"
#include "faultmine/editor.hpp"
#include "faultmine/image.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
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

namespace faultmine::platform::win32 {
namespace {

constexpr wchar_t kWindowClassName[] = L"FAULTMINE.MainWindow";
constexpr wchar_t kWindowTitle[] = L"FAULTMINE";
constexpr int kStatusHeight = 24;
constexpr int kEditorWidth = 370;
constexpr UINT kRenderMessage = WM_APP + 4U;

enum CommandId : UINT {
    command_open_image = 1001U,
    command_export = 1002U,
    command_exit = 1003U,
    command_open_project = 1004U,
    command_save_project = 1005U,
    command_save_project_as = 1006U,

    command_undo = 1051U,
    command_redo = 1052U,

    command_fit = 1101U,
    command_actual = 1102U,
    command_before = 1103U,
    command_proxy = 1104U,
    command_zoom_in = 1105U,
    command_zoom_out = 1106U,

    command_toggle_effects = 1201U,
    command_reroll = 1202U,
    command_rerender = 1203U,

    command_add = 1301U,
    command_remove = 1302U,
    command_duplicate = 1303U,
    command_move_up = 1304U,
    command_move_down = 1305U,
    command_bypass = 1306U,
    command_operator_lock = 1307U,
    command_apply_parameter = 1308U,
    command_nudge_down = 1309U,
    command_nudge_up = 1310U,
    command_parameter_lock = 1311U,
    command_asset_load = 1312U,
    command_stack_list = 1320U,
    command_add_combo = 1321U,
    command_parameter_combo = 1322U,
    command_parameter_edit = 1323U,
    command_parameter_choice = 1324U,
};

std::wstring utf8_to_wide(const std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) {
        return L"<invalid UTF-8>";
    }
    std::wstring output(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), output.data(), required);
    return output;
}

std::string wide_to_utf8(const std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string output(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), output.data(), required, nullptr, nullptr);
    return output;
}

void show_error_box(const HWND owner, const std::wstring& message) {
    MessageBoxW(owner, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

bool write_text_file(const std::filesystem::path& path, const std::string_view text, std::string* error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        if (error != nullptr) {
            *error = "could not open project file for writing";
        }
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        if (error != nullptr) {
            *error = "project write failed before all canonical bytes were stored";
        }
        return false;
    }
    return true;
}

class MainWindow {
public:
    explicit MainWindow(const HINSTANCE instance) : instance_(instance) {}

    [[nodiscard]] bool create() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = static_cast<UINT>(sizeof(window_class));
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &MainWindow::window_proc;
        window_class.hInstance = instance_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = kWindowClassName;

        const ATOM class_atom = RegisterClassExW(&window_class);
        if (class_atom == 0) {
            const DWORD error_code = GetLastError();
            if (error_code != ERROR_CLASS_ALREADY_EXISTS) {
                show_error_box(nullptr, L"RegisterClassExW failed: " + std::to_wstring(error_code));
                return false;
            }
        }

        hwnd_ = CreateWindowExW(
            0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900,
            nullptr, nullptr, instance_, this);
        if (hwnd_ == nullptr) {
            show_error_box(nullptr, L"CreateWindowExW failed: " + std::to_wstring(GetLastError()));
            return false;
        }

        create_menu();
        create_editor_controls();
        status_ = CreateWindowExW(
            0, L"STATIC", L"Open an image with Ctrl+O or a project with Ctrl+Shift+O.",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            0, 0, 100, kStatusHeight, hwnd_, nullptr, instance_, nullptr);
        set_default_font(status_);

        if (auto error = renderer_.initialize(hwnd_); error.has_value()) {
            show_error_box(hwnd_, L"D3D11 initialization failed:\n" + utf8_to_wide(*error));
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
            return false;
        }
        renderer_ready_ = true;
        populate_operator_types();
        refresh_editor_controls();
        resize_children();
        update_status();
        update_title();
        return true;
    }

    void show(const int show_command) {
        ShowWindow(hwnd_, show_command);
        UpdateWindow(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool smoke_present() {
        auto created = core::make_rgba8_image(4U, 3U);
        if (!created.ok()) {
            return false;
        }
        core::ImageBuffer image = std::move(*created.image);
        for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
            image.bytes[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xffU);
        }
        std::string error;
        const std::string identity = core::source_identity_hex(image);
        if (!session_.set_source(std::move(image), identity, L"<smoke>", &error)) {
            return false;
        }
        session_.set_proxy_enabled(false);
        if (!session_.ensure_preview(&error) || !refresh_display_texture(&error)) {
            return false;
        }
        return !renderer_.draw(session_.view_state()).has_value();
    }

    int message_loop() {
        MSG message{};
        while (true) {
            const BOOL result = GetMessageW(&message, nullptr, 0U, 0U);
            if (result == 0) {
                return static_cast<int>(message.wParam);
            }
            if (result == -1) {
                show_error_box(hwnd_, L"GetMessageW failed: " + std::to_wstring(GetLastError()));
                return EXIT_FAILURE;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

private:
    static LRESULT CALLBACK window_proc(const HWND window, const UINT message, const WPARAM w_param, const LPARAM l_param) {
        MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            self = static_cast<MainWindow*>(create->lpCreateParams);
            if (self != nullptr) {
                self->hwnd_ = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
        }
        return self != nullptr ? self->handle_message(message, w_param, l_param) : DefWindowProcW(window, message, w_param, l_param);
    }

    LRESULT handle_message(const UINT message, const WPARAM w_param, const LPARAM l_param) {
        switch (message) {
            case WM_COMMAND:
                handle_command(static_cast<UINT>(LOWORD(w_param)), static_cast<UINT>(HIWORD(w_param)));
                return 0;
            case WM_KEYDOWN:
                handle_key(w_param);
                return 0;
            case WM_KEYUP:
                session_.end_coalesced_edit();
                return 0;
            case WM_MOUSEWHEEL: {
                const int delta = GET_WHEEL_DELTA_WPARAM(w_param);
                session_.zoom_by(delta > 0 ? 1.2 : (1.0 / 1.2));
                refresh_and_repaint(false);
                return 0;
            }
            case WM_LBUTTONDOWN:
                dragging_ = true;
                last_mouse_x_ = static_cast<short>(LOWORD(l_param));
                last_mouse_y_ = static_cast<short>(HIWORD(l_param));
                SetCapture(hwnd_);
                return 0;
            case WM_MOUSEMOVE:
                if (dragging_ && (w_param & MK_LBUTTON) != 0U) {
                    const int x = static_cast<short>(LOWORD(l_param));
                    const int y = static_cast<short>(HIWORD(l_param));
                    session_.pan_by(static_cast<double>(x - last_mouse_x_), static_cast<double>(y - last_mouse_y_));
                    last_mouse_x_ = x;
                    last_mouse_y_ = y;
                    refresh_and_repaint(false);
                }
                return 0;
            case WM_LBUTTONUP:
                dragging_ = false;
                ReleaseCapture();
                return 0;
            case WM_SIZE:
                resize_children();
                return 0;
            case WM_PAINT:
                paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case kRenderMessage:
                render_message_pending_ = false;
                render_preview();
                return 0;
            case WM_CLOSE:
                DestroyWindow(hwnd_);
                return 0;
            case WM_DESTROY:
                PostQuitMessage(EXIT_SUCCESS);
                return 0;
            case WM_NCDESTROY:
                SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
                return DefWindowProcW(hwnd_, message, w_param, l_param);
            default:
                return DefWindowProcW(hwnd_, message, w_param, l_param);
        }
    }

    void create_menu() {
        menu_ = CreateMenu();
        HMENU file_menu = CreatePopupMenu();
        HMENU edit_menu = CreatePopupMenu();
        HMENU stack_menu = CreatePopupMenu();
        HMENU view_menu = CreatePopupMenu();
        HMENU fault_menu = CreatePopupMenu();

        AppendMenuW(file_menu, MF_STRING, command_open_image, L"Open &image...\tCtrl+O");
        AppendMenuW(file_menu, MF_STRING, command_open_project, L"Open &project...\tCtrl+Shift+O");
        AppendMenuW(file_menu, MF_STRING, command_save_project, L"&Save project\tCtrl+S");
        AppendMenuW(file_menu, MF_STRING, command_save_project_as, L"Save project &as...\tCtrl+Shift+S");
        AppendMenuW(file_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(file_menu, MF_STRING, command_export, L"&Export canonical PNG...\tCtrl+E");
        AppendMenuW(file_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(file_menu, MF_STRING, command_exit, L"E&xit");

        AppendMenuW(edit_menu, MF_STRING, command_undo, L"&Undo\tCtrl+Z");
        AppendMenuW(edit_menu, MF_STRING, command_redo, L"&Redo\tCtrl+Y");

        AppendMenuW(stack_menu, MF_STRING, command_add, L"&Add selected type\tCtrl+Insert");
        AppendMenuW(stack_menu, MF_STRING, command_duplicate, L"&Duplicate\tCtrl+D");
        AppendMenuW(stack_menu, MF_STRING, command_remove, L"&Remove\tDelete");
        AppendMenuW(stack_menu, MF_STRING, command_move_up, L"Move &up\tAlt+Up");
        AppendMenuW(stack_menu, MF_STRING, command_move_down, L"Move &down\tAlt+Down");
        AppendMenuW(stack_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(stack_menu, MF_STRING, command_bypass, L"&Bypass selected\tX");
        AppendMenuW(stack_menu, MF_STRING, command_operator_lock, L"Mutation &lock selected\tL");
        AppendMenuW(stack_menu, MF_STRING, command_parameter_lock, L"Mutation lock &parameter\tCtrl+L");

        AppendMenuW(view_menu, MF_STRING, command_fit, L"&Fit\tF");
        AppendMenuW(view_menu, MF_STRING, command_actual, L"&1:1\t1");
        AppendMenuW(view_menu, MF_STRING, command_zoom_in, L"Zoom &in");
        AppendMenuW(view_menu, MF_STRING, command_zoom_out, L"Zoom &out");
        AppendMenuW(view_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(view_menu, MF_STRING, command_before, L"&Before / after\tB");
        AppendMenuW(view_menu, MF_STRING | MF_CHECKED, command_proxy, L"Use &proxy preview\tP");

        AppendMenuW(fault_menu, MF_STRING | MF_CHECKED, command_toggle_effects, L"All faults enabled\tSpace");
        AppendMenuW(fault_menu, MF_STRING, command_reroll, L"Reroll root seed\tR");
        AppendMenuW(fault_menu, MF_STRING, command_rerender, L"Rerender\tF5");

        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"&File");
        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(edit_menu), L"&Edit");
        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(stack_menu), L"&Stack");
        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), L"&View");
        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(fault_menu), L"&Faults");
        SetMenu(hwnd_, menu_);
    }

    HWND make_control(const wchar_t* class_name, const wchar_t* text, const DWORD style, const UINT id) {
        HWND control = CreateWindowExW(
            0, class_name, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 100, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), instance_, nullptr);
        set_default_font(control);
        return control;
    }

    void set_default_font(const HWND control) {
        if (control != nullptr) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
    }

    void create_editor_controls() {
        editor_panel_ = make_control(L"STATIC", L"", SS_WHITERECT, 0U);
        add_combo_ = make_control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, command_add_combo);
        add_button_ = make_control(L"BUTTON", L"Add", BS_PUSHBUTTON, command_add);
        stack_list_ = make_control(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_BORDER, command_stack_list);
        remove_button_ = make_control(L"BUTTON", L"Remove", BS_PUSHBUTTON, command_remove);
        duplicate_button_ = make_control(L"BUTTON", L"Duplicate", BS_PUSHBUTTON, command_duplicate);
        up_button_ = make_control(L"BUTTON", L"Up", BS_PUSHBUTTON, command_move_up);
        down_button_ = make_control(L"BUTTON", L"Down", BS_PUSHBUTTON, command_move_down);
        bypass_button_ = make_control(L"BUTTON", L"Bypass", BS_PUSHBUTTON, command_bypass);
        operator_lock_button_ = make_control(L"BUTTON", L"Op lock", BS_PUSHBUTTON, command_operator_lock);
        parameter_combo_ = make_control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, command_parameter_combo);
        parameter_edit_ = make_control(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, command_parameter_edit);
        parameter_choice_ = make_control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, command_parameter_choice);
        apply_button_ = make_control(L"BUTTON", L"Apply", BS_PUSHBUTTON, command_apply_parameter);
        nudge_down_button_ = make_control(L"BUTTON", L"-", BS_PUSHBUTTON, command_nudge_down);
        nudge_up_button_ = make_control(L"BUTTON", L"+", BS_PUSHBUTTON, command_nudge_up);
        parameter_lock_button_ = make_control(L"BUTTON", L"Gene lock", BS_PUSHBUTTON, command_parameter_lock);
        asset_button_ = make_control(L"BUTTON", L"Load asset...", BS_PUSHBUTTON, command_asset_load);
    }

    void populate_operator_types() {
        SendMessageW(add_combo_, CB_RESETCONTENT, 0U, 0U);
        for (const auto& descriptor : session_.operator_descriptors()) {
            const std::wstring label = utf8_to_wide(descriptor.type_id);
            SendMessageW(add_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(label.c_str()));
        }
        if (!session_.operator_descriptors().empty()) {
            SendMessageW(add_combo_, CB_SETCURSEL, 0U, 0U);
        }
    }

    void resize_children() {
        if (hwnd_ == nullptr) {
            return;
        }
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int width = std::max<LONG>(1, client.right - client.left);
        const int height = std::max<LONG>(1, client.bottom - client.top);
        const int editor_x = std::max(0, width - kEditorWidth);
        const int editor_height = std::max(1, height - kStatusHeight);
        MoveWindow(editor_panel_, editor_x, 0, kEditorWidth, editor_height, TRUE);

        int y = 8;
        const int x = editor_x + 8;
        const int inner = kEditorWidth - 16;
        MoveWindow(add_combo_, x, y, inner - 58, 300, TRUE);
        MoveWindow(add_button_, x + inner - 54, y, 54, 24, TRUE);
        y += 32;
        const int list_height = std::max(130, editor_height / 2 - 80);
        MoveWindow(stack_list_, x, y, inner, list_height, TRUE);
        y += list_height + 6;
        MoveWindow(remove_button_, x, y, 58, 24, TRUE);
        MoveWindow(duplicate_button_, x + 62, y, 68, 24, TRUE);
        MoveWindow(up_button_, x + 134, y, 44, 24, TRUE);
        MoveWindow(down_button_, x + 182, y, 48, 24, TRUE);
        MoveWindow(bypass_button_, x + 234, y, 54, 24, TRUE);
        MoveWindow(operator_lock_button_, x + 292, y, 62, 24, TRUE);
        y += 34;
        MoveWindow(parameter_combo_, x, y, inner, 260, TRUE);
        y += 30;
        MoveWindow(parameter_edit_, x, y, inner, 24, TRUE);
        MoveWindow(parameter_choice_, x, y, inner, 240, TRUE);
        y += 30;
        MoveWindow(apply_button_, x, y, 58, 24, TRUE);
        MoveWindow(nudge_down_button_, x + 62, y, 36, 24, TRUE);
        MoveWindow(nudge_up_button_, x + 102, y, 36, 24, TRUE);
        MoveWindow(parameter_lock_button_, x + 142, y, 78, 24, TRUE);
        MoveWindow(asset_button_, x + 224, y, 130, 24, TRUE);

        if (status_ != nullptr) {
            MoveWindow(status_, 4, std::max(0, height - kStatusHeight), std::max(1, width - 8), kStatusHeight, TRUE);
        }
        if (renderer_ready_) {
            if (auto error = renderer_.resize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)); error.has_value()) {
                set_status_error(*error);
            }
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void paint() {
        PAINTSTRUCT paint_struct{};
        BeginPaint(hwnd_, &paint_struct);
        if (renderer_ready_) {
            if (auto error = renderer_.draw(session_.view_state()); error.has_value()) {
                set_status_error(*error);
            }
        }
        EndPaint(hwnd_, &paint_struct);
    }

    void schedule_render() {
        if (!render_message_pending_) {
            render_message_pending_ = true;
            PostMessageW(hwnd_, kRenderMessage, 0U, 0U);
        }
    }

    void render_preview() {
        if (!session_.has_source()) {
            update_status();
            return;
        }
        std::string error;
        if (!session_.ensure_preview(&error)) {
            set_status_error(error);
            return;
        }
        if (!refresh_display_texture(&error)) {
            set_status_error(error);
            return;
        }
        update_status();
        update_title();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    bool refresh_display_texture(std::string* error) {
        const core::ImageBuffer* image = session_.display_image();
        if (image == nullptr) {
            image = session_.full_source();
        }
        if (image == nullptr) {
            return true;
        }
        if (auto upload_error = renderer_.upload_image(*image); upload_error.has_value()) {
            if (error != nullptr) {
                *error = *upload_error;
            }
            return false;
        }
        return true;
    }

    std::optional<std::size_t> selected_operator_index() const {
        const LRESULT selected = SendMessageW(stack_list_, LB_GETCURSEL, 0U, 0U);
        if (selected == LB_ERR || static_cast<std::size_t>(selected) >= session_.genome().operators.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(selected);
    }

    std::string selected_parameter_name() const {
        const LRESULT selected = SendMessageW(parameter_combo_, CB_GETCURSEL, 0U, 0U);
        if (selected == CB_ERR) {
            return {};
        }
        std::array<wchar_t, 512> text{};
        SendMessageW(parameter_combo_, CB_GETLBTEXT, static_cast<WPARAM>(selected), reinterpret_cast<LPARAM>(text.data()));
        return wide_to_utf8(text.data());
    }

    void refresh_editor_controls() {
        const auto wanted = session_.selected_operator();
        SendMessageW(stack_list_, LB_RESETCONTENT, 0U, 0U);
        for (std::size_t index = 0U; index < session_.genome().operators.size(); ++index) {
            const auto& instance = session_.genome().operators[index];
            std::wstring label = instance.enabled ? L"[x] " : L"[ ] ";
            label += session_.operator_locked(index) ? L"[L] " : L"[ ] ";
            label += utf8_to_wide(instance.type_id);
            SendMessageW(stack_list_, LB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(label.c_str()));
        }

        std::optional<std::size_t> selected = wanted;
        if ((!selected.has_value() || *selected >= session_.genome().operators.size()) && !session_.genome().operators.empty()) {
            selected = 0U;
        }
        if (selected.has_value()) {
            SendMessageW(stack_list_, LB_SETCURSEL, static_cast<WPARAM>(*selected), 0U);
            session_.set_selected_operator(selected);
        } else {
            session_.set_selected_operator(std::nullopt);
        }
        selected_parameter_.clear();
        refresh_parameter_list();
        sync_menu_checks();
        update_title();
        update_status();
    }

    void refresh_parameter_list() {
        SendMessageW(parameter_combo_, CB_RESETCONTENT, 0U, 0U);
        const auto selected = selected_operator_index();
        if (!selected.has_value()) {
            refresh_parameter_editor();
            return;
        }
        const core::OperatorDescriptor* descriptor = session_.descriptor_for_operator(*selected);
        if (descriptor == nullptr) {
            refresh_parameter_editor();
            return;
        }
        for (const auto& parameter : descriptor->parameters) {
            const std::wstring name = utf8_to_wide(parameter.name);
            SendMessageW(parameter_combo_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(name.c_str()));
        }
        if (!descriptor->parameters.empty()) {
            SendMessageW(parameter_combo_, CB_SETCURSEL, 0U, 0U);
            selected_parameter_ = descriptor->parameters.front().name;
        }
        refresh_parameter_editor();
    }

    void refresh_parameter_editor() {
        ShowWindow(parameter_edit_, SW_SHOW);
        ShowWindow(parameter_choice_, SW_HIDE);
        ShowWindow(asset_button_, SW_HIDE);
        SetWindowTextW(parameter_edit_, L"");
        const auto selected = selected_operator_index();
        if (!selected.has_value()) {
            SetWindowTextW(operator_lock_button_, L"Op lock");
            SetWindowTextW(parameter_lock_button_, L"Gene lock");
            return;
        }
        SetWindowTextW(operator_lock_button_, session_.operator_locked(*selected) ? L"Op lock ON" : L"Op lock");
        const std::string parameter_name = selected_parameter_name();
        if (parameter_name.empty()) {
            return;
        }
        selected_parameter_ = parameter_name;
        const core::ParameterDescriptor* descriptor = session_.descriptor_for_parameter(*selected, parameter_name);
        const auto value = session_.genome().operators[*selected].parameters.find(parameter_name);
        if (descriptor == nullptr || value == session_.genome().operators[*selected].parameters.end()) {
            return;
        }
        SetWindowTextW(parameter_lock_button_, session_.parameter_locked(*selected, parameter_name) ? L"Gene lock ON" : L"Gene lock");
        const std::string current = app::parameter_value_to_text(value->second);

        if (descriptor->kind == core::ParameterKind::boolean || descriptor->mutation.domain == core::MutationDomain::choice) {
            ShowWindow(parameter_edit_, SW_HIDE);
            ShowWindow(parameter_choice_, SW_SHOW);
            SendMessageW(parameter_choice_, CB_RESETCONTENT, 0U, 0U);
            std::vector<std::string> choices;
            if (descriptor->kind == core::ParameterKind::boolean) {
                choices = {"false", "true"};
            } else {
                choices = descriptor->mutation.choices;
            }
            int selected_choice = -1;
            for (std::size_t index = 0U; index < choices.size(); ++index) {
                const std::wstring choice = utf8_to_wide(choices[index]);
                SendMessageW(parameter_choice_, CB_ADDSTRING, 0U, reinterpret_cast<LPARAM>(choice.c_str()));
                if (choices[index] == current) {
                    selected_choice = static_cast<int>(index);
                }
            }
            if (selected_choice >= 0) {
                SendMessageW(parameter_choice_, CB_SETCURSEL, static_cast<WPARAM>(selected_choice), 0U);
            }
        } else {
            SetWindowTextW(parameter_edit_, utf8_to_wide(current).c_str());
        }
        if (descriptor->mutation.domain == core::MutationDomain::palette || descriptor->mutation.domain == core::MutationDomain::lut) {
            ShowWindow(asset_button_, SW_SHOW);
        }
    }

    void handle_command(const UINT command, const UINT notification) {
        if (command == command_stack_list && notification == LBN_SELCHANGE) {
            const auto selected = selected_operator_index();
            session_.set_selected_operator(selected);
            selected_parameter_.clear();
            refresh_parameter_list();
            return;
        }
        if (command == command_parameter_combo && notification == CBN_SELCHANGE) {
            refresh_parameter_editor();
            return;
        }

        switch (command) {
            case command_open_image: open_source(); break;
            case command_open_project: open_project(); break;
            case command_save_project: save_project(false); break;
            case command_save_project_as: save_project(true); break;
            case command_export: export_png(); break;
            case command_exit: PostMessageW(hwnd_, WM_CLOSE, 0U, 0U); break;
            case command_undo: perform_undo(); break;
            case command_redo: perform_redo(); break;
            case command_fit: session_.set_fit_view(); refresh_and_repaint(false); break;
            case command_actual: session_.set_actual_view(); refresh_and_repaint(false); break;
            case command_zoom_in: session_.zoom_by(1.2); refresh_and_repaint(false); break;
            case command_zoom_out: session_.zoom_by(1.0 / 1.2); refresh_and_repaint(false); break;
            case command_before: session_.toggle_before(); sync_menu_checks(); refresh_and_repaint(); break;
            case command_proxy: session_.set_proxy_enabled(!session_.proxy_enabled()); sync_menu_checks(); schedule_render(); break;
            case command_toggle_effects: session_.toggle_effects(); refresh_editor_controls(); schedule_render(); break;
            case command_reroll: session_.reroll_seed(); refresh_editor_controls(); schedule_render(); break;
            case command_rerender: schedule_render(); break;
            case command_add: add_selected_operator(); break;
            case command_remove: remove_selected_operator(); break;
            case command_duplicate: duplicate_selected_operator(); break;
            case command_move_up: move_selected_operator(-1); break;
            case command_move_down: move_selected_operator(1); break;
            case command_bypass: bypass_selected_operator(); break;
            case command_operator_lock: toggle_selected_operator_lock(); break;
            case command_apply_parameter: apply_selected_parameter(); break;
            case command_nudge_down: nudge_selected_parameter(-1, false); break;
            case command_nudge_up: nudge_selected_parameter(1, false); break;
            case command_parameter_lock: toggle_selected_parameter_lock(); break;
            case command_asset_load: load_selected_asset(); break;
            default: break;
        }
    }

    void handle_key(const WPARAM key) {
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

        if (control && shift && (key == 'O' || key == 'o')) { open_project(); return; }
        if (control && (key == 'O' || key == 'o')) { open_source(); return; }
        if (control && (key == 'S' || key == 's')) { save_project(shift); return; }
        if (control && (key == 'E' || key == 'e')) { export_png(); return; }
        if (control && (key == 'Z' || key == 'z')) { perform_undo(); return; }
        if (control && (key == 'Y' || key == 'y')) { perform_redo(); return; }
        if (control && (key == 'D' || key == 'd')) { duplicate_selected_operator(); return; }
        if (control && (key == 'L' || key == 'l')) { toggle_selected_parameter_lock(); return; }
        if (control && key == VK_INSERT) { add_selected_operator(); return; }
        if (alt && key == VK_UP) { move_selected_operator(-1); return; }
        if (alt && key == VK_DOWN) { move_selected_operator(1); return; }
        if (control && key == VK_UP) { select_relative_operator(-1); return; }
        if (control && key == VK_DOWN) { select_relative_operator(1); return; }

        switch (key) {
            case VK_DELETE: remove_selected_operator(); break;
            case 'X': bypass_selected_operator(); break;
            case 'L': toggle_selected_operator_lock(); break;
            case VK_OEM_4: nudge_selected_parameter(-1, shift); break;
            case VK_OEM_6: nudge_selected_parameter(1, shift); break;
            case VK_F5: schedule_render(); break;
            case 'F': session_.set_fit_view(); refresh_and_repaint(false); break;
            case '1': session_.set_actual_view(); refresh_and_repaint(false); break;
            case 'B': session_.toggle_before(); sync_menu_checks(); refresh_and_repaint(); break;
            case 'P': session_.set_proxy_enabled(!session_.proxy_enabled()); sync_menu_checks(); schedule_render(); break;
            case 'R': session_.reroll_seed(); refresh_editor_controls(); schedule_render(); break;
            case VK_SPACE: session_.toggle_effects(); refresh_editor_controls(); schedule_render(); break;
            case VK_LEFT: session_.pan_by(-20.0, 0.0); refresh_and_repaint(false); break;
            case VK_RIGHT: session_.pan_by(20.0, 0.0); refresh_and_repaint(false); break;
            case VK_UP: session_.pan_by(0.0, -20.0); refresh_and_repaint(false); break;
            case VK_DOWN: session_.pan_by(0.0, 20.0); refresh_and_repaint(false); break;
            default: break;
        }
    }

    void perform_edit(const app::EditResult& result, const bool render) {
        if (!result.ok()) {
            set_status_error(result.error->message);
            return;
        }
        refresh_editor_controls();
        if (render) {
            schedule_render();
        } else {
            update_status();
            update_title();
        }
    }

    void add_selected_operator() {
        const LRESULT selected_type = SendMessageW(add_combo_, CB_GETCURSEL, 0U, 0U);
        if (selected_type == CB_ERR || static_cast<std::size_t>(selected_type) >= session_.operator_descriptors().size()) {
            return;
        }
        const auto current = selected_operator_index();
        const std::size_t insert = current.has_value() ? *current + 1U : session_.genome().operators.size();
        perform_edit(session_.add_operator(session_.operator_descriptors()[static_cast<std::size_t>(selected_type)].type_id, insert), true);
        if (insert < session_.genome().operators.size()) {
            session_.set_selected_operator(insert);
            refresh_editor_controls();
        }
    }

    void remove_selected_operator() {
        const auto selected = selected_operator_index();
        if (!selected.has_value()) return;
        perform_edit(session_.remove_operator(*selected), true);
        if (!session_.genome().operators.empty()) {
            session_.set_selected_operator(std::min(*selected, session_.genome().operators.size() - 1U));
            refresh_editor_controls();
        }
    }

    void duplicate_selected_operator() {
        const auto selected = selected_operator_index();
        if (!selected.has_value()) return;
        perform_edit(session_.duplicate_operator(*selected), true);
        if (*selected + 1U < session_.genome().operators.size()) {
            session_.set_selected_operator(*selected + 1U);
            refresh_editor_controls();
        }
    }

    void move_selected_operator(const int direction) {
        const auto selected = selected_operator_index();
        if (!selected.has_value()) return;
        const std::ptrdiff_t destination = static_cast<std::ptrdiff_t>(*selected) + direction;
        if (destination < 0 || destination >= static_cast<std::ptrdiff_t>(session_.genome().operators.size())) return;
        const std::size_t to = static_cast<std::size_t>(destination);
        perform_edit(session_.move_operator(*selected, to), true);
        session_.set_selected_operator(to);
        refresh_editor_controls();
    }

    void select_relative_operator(const int direction) {
        if (session_.genome().operators.empty()) return;
        const auto selected = selected_operator_index();
        const std::ptrdiff_t current = selected.has_value() ? static_cast<std::ptrdiff_t>(*selected) : 0;
        const std::ptrdiff_t target = std::clamp<std::ptrdiff_t>(current + direction, 0, static_cast<std::ptrdiff_t>(session_.genome().operators.size() - 1U));
        session_.set_selected_operator(static_cast<std::size_t>(target));
        refresh_editor_controls();
        SetFocus(stack_list_);
    }

    void bypass_selected_operator() {
        const auto selected = selected_operator_index();
        if (selected.has_value()) perform_edit(session_.toggle_operator_enabled(*selected), true);
    }

    void toggle_selected_operator_lock() {
        const auto selected = selected_operator_index();
        if (selected.has_value()) perform_edit(session_.toggle_operator_lock(*selected), false);
    }

    void toggle_selected_parameter_lock() {
        const auto selected = selected_operator_index();
        const std::string parameter = selected_parameter_name();
        if (selected.has_value() && !parameter.empty()) perform_edit(session_.toggle_parameter_lock(*selected, parameter), false);
    }

    void apply_selected_parameter() {
        const auto selected = selected_operator_index();
        const std::string parameter = selected_parameter_name();
        if (!selected.has_value() || parameter.empty()) return;
        const core::ParameterDescriptor* descriptor = session_.descriptor_for_parameter(*selected, parameter);
        if (descriptor == nullptr) return;
        std::wstring wide;
        if (descriptor->kind == core::ParameterKind::boolean || descriptor->mutation.domain == core::MutationDomain::choice) {
            const LRESULT choice = SendMessageW(parameter_choice_, CB_GETCURSEL, 0U, 0U);
            if (choice == CB_ERR) return;
            std::array<wchar_t, 32768> buffer{};
            SendMessageW(parameter_choice_, CB_GETLBTEXT, static_cast<WPARAM>(choice), reinterpret_cast<LPARAM>(buffer.data()));
            wide = buffer.data();
        } else {
            const int length = GetWindowTextLengthW(parameter_edit_);
            wide.resize(static_cast<std::size_t>(std::max(0, length)));
            if (length > 0) {
                GetWindowTextW(parameter_edit_, wide.data(), length + 1);
            }
        }
        perform_edit(session_.set_parameter_from_text(*selected, parameter, wide_to_utf8(wide)), true);
    }

    void nudge_selected_parameter(const int direction, const bool large) {
        const auto selected = selected_operator_index();
        const std::string parameter = selected_parameter_name();
        if (!selected.has_value() || parameter.empty()) return;
        const app::EditResult result = session_.nudge_parameter(*selected, parameter, direction, large, true);
        if (!result.ok()) {
            set_status_error(result.error->message);
            return;
        }
        refresh_parameter_editor();
        update_title();
        schedule_render();
    }

    void load_selected_asset() {
        const auto selected = selected_operator_index();
        const std::string parameter = selected_parameter_name();
        if (!selected.has_value() || parameter.empty()) return;
        const core::ParameterDescriptor* descriptor = session_.descriptor_for_parameter(*selected, parameter);
        if (descriptor == nullptr || (descriptor->mutation.domain != core::MutationDomain::palette && descriptor->mutation.domain != core::MutationDomain::lut)) return;

        std::array<wchar_t, 32768> filename{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = descriptor->mutation.domain == core::MutationDomain::palette
            ? L"FAULTMINE palette\0*.fmpal;*.json\0All files\0*.*\0\0"
            : L"FAULTMINE LUT\0*.fmlut;*.json\0All files\0*.*\0\0";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog) == FALSE) return;
        const std::string text = read_text_file(filename.data());
        if (text.empty()) {
            show_error_box(hwnd_, L"Asset file is empty or could not be read.");
            return;
        }
        perform_edit(session_.set_parameter_from_text(*selected, parameter, text), true);
    }

    void perform_undo() {
        if (session_.undo()) {
            refresh_editor_controls();
            schedule_render();
        }
    }

    void perform_redo() {
        if (session_.redo()) {
            refresh_editor_controls();
            schedule_render();
        }
    }

    bool choose_image_path(std::filesystem::path& path) {
        std::array<wchar_t, 32768> filename{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.wdp\0All files\0*.*\0\0";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog) == FALSE) return false;
        path = filename.data();
        return true;
    }

    void open_source() {
        std::filesystem::path path;
        if (!choose_image_path(path)) return;
        io::WicLoadResult loaded = io::load_wic_image(path);
        if (!loaded.ok()) {
            show_error_box(hwnd_, L"Image load failed:\n" + utf8_to_wide(loaded.error->message));
            return;
        }
        std::string error;
        io::LoadedSource source = std::move(*loaded.source);
        if (!session_.set_source(std::move(source.image), std::move(source.source_identity), std::move(source.path), &error)) {
            show_error_box(hwnd_, L"Session rejected source:\n" + utf8_to_wide(error));
            return;
        }
        project_path_.clear();
        refresh_editor_controls();
        if (const core::ImageBuffer* image = session_.full_source(); image != nullptr) {
            if (auto upload_error = renderer_.upload_image(*image); upload_error.has_value()) set_status_error(*upload_error);
        }
        schedule_render();
    }

    bool resolve_project_source(const app::ProjectDocument& project, io::LoadedSource& resolved) {
        const std::filesystem::path recorded{utf8_to_wide(project.source.path_utf8)};
        if (!recorded.empty()) {
            io::WicLoadResult loaded = io::load_wic_image(recorded);
            if (loaded.ok() && loaded.source->source_identity == project.source.source_identity) {
                resolved = std::move(*loaded.source);
                return true;
            }
        }

        const int answer = MessageBoxW(
            hwnd_,
            L"The recorded source is missing, unreadable, or has changed content.\nRelink to an identical-content source?",
            kWindowTitle,
            MB_YESNO | MB_ICONWARNING);
        if (answer != IDYES) return false;

        std::filesystem::path replacement;
        if (!choose_image_path(replacement)) return false;
        io::WicLoadResult loaded = io::load_wic_image(replacement);
        if (!loaded.ok()) {
            show_error_box(hwnd_, L"Relink image load failed:\n" + utf8_to_wide(loaded.error->message));
            return false;
        }
        if (loaded.source->source_identity != project.source.source_identity) {
            show_error_box(hwnd_, L"Relink rejected: normalized source identity does not match the project. The changed content was not accepted silently.");
            return false;
        }
        resolved = std::move(*loaded.source);
        return true;
    }

    void open_project() {
        std::array<wchar_t, 32768> filename{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = L"FAULTMINE project\0*.fmproj\0All files\0*.*\0\0";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog) == FALSE) return;
        const std::filesystem::path path{filename.data()};
        const std::string text = read_text_file(path);
        if (text.empty()) {
            show_error_box(hwnd_, L"Project file is empty or could not be read.");
            return;
        }
        const auto parsed = app::parse_project(text, session_.registry().schema_registry());
        if (!parsed.ok()) {
            const std::wstring detail = parsed.error.has_value()
                ? utf8_to_wide(parsed.error->path + ": " + parsed.error->message)
                : L"unknown project parse error";
            show_error_box(hwnd_, L"Project load failed:\n" + detail);
            return;
        }
        io::LoadedSource source;
        if (!resolve_project_source(*parsed.project, source)) return;
        std::string error;
        if (!session_.load_project_state(*parsed.project, std::move(source.image), std::move(source.path), &error)) {
            show_error_box(hwnd_, L"Project session load failed:\n" + utf8_to_wide(error));
            return;
        }
        project_path_ = path;
        session_.mark_project_saved();
        refresh_editor_controls();
        if (const core::ImageBuffer* image = session_.full_source(); image != nullptr) {
            if (auto upload_error = renderer_.upload_image(*image); upload_error.has_value()) set_status_error(*upload_error);
        }
        schedule_render();
    }

    void save_project(const bool force_save_as) {
        if (!session_.has_source()) {
            show_error_box(hwnd_, L"Load an image before saving a project.");
            return;
        }
        std::filesystem::path destination = project_path_;
        if (force_save_as || destination.empty()) {
            std::array<wchar_t, 32768> filename{};
            const std::wstring default_name = session_.source_path().stem().wstring() + L".fmproj";
            std::copy_n(default_name.c_str(), std::min<std::size_t>(default_name.size(), filename.size() - 1U), filename.data());
            OPENFILENAMEW dialog{};
            dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
            dialog.hwndOwner = hwnd_;
            dialog.lpstrFilter = L"FAULTMINE project\0*.fmproj\0All files\0*.*\0\0";
            dialog.lpstrDefExt = L"fmproj";
            dialog.lpstrFile = filename.data();
            dialog.nMaxFile = static_cast<DWORD>(filename.size());
            dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetSaveFileNameW(&dialog) == FALSE) return;
            destination = filename.data();
        }
        std::string error;
        const auto project = session_.make_project_document(&error);
        if (!project.has_value()) {
            show_error_box(hwnd_, L"Project state is not saveable:\n" + utf8_to_wide(error));
            return;
        }
        const std::string canonical = app::serialize_project_canonical(*project);
        if (!write_text_file(destination, canonical, &error)) {
            show_error_box(hwnd_, L"Project save failed:\n" + utf8_to_wide(error));
            return;
        }
        project_path_ = destination;
        session_.mark_project_saved();
        transient_status_ = "Project saved.";
        update_title();
        update_status();
    }

    void export_png() {
        if (!session_.has_source()) {
            show_error_box(hwnd_, L"Load an image before exporting.");
            return;
        }
        std::array<wchar_t, 32768> filename{};
        const std::wstring default_name = session_.source_path().stem().wstring() + L"-faultmine.png";
        std::copy_n(default_name.c_str(), std::min<std::size_t>(default_name.size(), filename.size() - 1U), filename.data());
        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = L"PNG image\0*.png\0All files\0*.*\0\0";
        dialog.lpstrDefExt = L"png";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameW(&dialog) == FALSE) return;

        std::string error;
        auto full_result = session_.render_full(&error);
        if (!full_result.has_value()) {
            show_error_box(hwnd_, L"Canonical export render failed:\n" + utf8_to_wide(error));
            return;
        }
        if (auto save_error = io::save_wic_png(*full_result, filename.data()); save_error.has_value()) {
            show_error_box(hwnd_, L"PNG export failed:\n" + utf8_to_wide(save_error->message));
            return;
        }
        transient_status_ = "Exported full-resolution canonical PNG.";
        update_status();
    }

    void sync_menu_checks() {
        if (menu_ == nullptr) return;
        CheckMenuItem(menu_, command_proxy, MF_BYCOMMAND | (session_.proxy_enabled() ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu_, command_before, MF_BYCOMMAND | (session_.view_state().show_before ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu_, command_toggle_effects, MF_BYCOMMAND | (session_.effects_enabled() ? MF_CHECKED : MF_UNCHECKED));
        EnableMenuItem(menu_, command_undo, MF_BYCOMMAND | (session_.can_undo() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(menu_, command_redo, MF_BYCOMMAND | (session_.can_redo() ? MF_ENABLED : MF_GRAYED));
        DrawMenuBar(hwnd_);
    }

    void refresh_and_repaint(const bool upload = true) {
        if (upload) {
            std::string error;
            if (!refresh_display_texture(&error)) {
                set_status_error(error);
                return;
            }
        }
        update_status();
        update_title();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void set_status_error(const std::string& error) {
        transient_status_ = "ERROR: " + error;
        update_status();
    }

    void update_title() {
        if (hwnd_ == nullptr) return;
        std::wstring title{kWindowTitle};
        if (!project_path_.empty()) {
            title += L" — " + project_path_.filename().wstring();
        } else if (session_.has_source()) {
            title += L" — " + session_.source_path().filename().wstring();
        }
        if (session_.project_dirty()) title += L" *";
        SetWindowTextW(hwnd_, title.c_str());
    }

    void update_status() {
        if (status_ == nullptr) return;
        std::wostringstream status;
        if (!session_.has_source()) {
            status << L"Ctrl+O image | Ctrl+Shift+O project | Ctrl+Insert add | Ctrl+Z/Y undo/redo";
        } else {
            const core::ImageBuffer* full = session_.full_source();
            const app::PreviewState preview = session_.preview_state();
            status << session_.source_path().filename().wstring() << L" | ";
            if (full != nullptr) status << full->width << L'x' << full->height;
            if (preview.width != 0U) status << L" -> " << preview.width << L'x' << preview.height;
            status << (preview.is_proxy ? L" | PROXY" : L" | FULL");
            status << (session_.view_state().show_before ? L" | BEFORE" : L" | RESULT");
            status << L" | ops " << session_.genome().operators.size();
            const auto selected = selected_operator_index();
            if (selected.has_value()) status << L" | sel " << utf8_to_wide(session_.genome().operators[*selected].type_id);
            const std::string parameter = selected_parameter_name();
            if (!parameter.empty()) status << L"." << utf8_to_wide(parameter);
            status << L" | genome " << utf8_to_wide(session_.genome_identity().substr(0U, 8U));
            status << (renderer_.using_warp() ? L" | D3D11 WARP" : L" | D3D11 HW");
        }
        if (!transient_status_.empty()) {
            status << L" | " << utf8_to_wide(transient_status_);
            transient_status_.clear();
        }
        SetWindowTextW(status_, status.str().c_str());
        sync_menu_checks();
    }

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND status_{};
    HMENU menu_{};
    HWND editor_panel_{};
    HWND add_combo_{};
    HWND add_button_{};
    HWND stack_list_{};
    HWND remove_button_{};
    HWND duplicate_button_{};
    HWND up_button_{};
    HWND down_button_{};
    HWND bypass_button_{};
    HWND operator_lock_button_{};
    HWND parameter_combo_{};
    HWND parameter_edit_{};
    HWND parameter_choice_{};
    HWND apply_button_{};
    HWND nudge_down_button_{};
    HWND nudge_up_button_{};
    HWND parameter_lock_button_{};
    HWND asset_button_{};
    bool renderer_ready_{};
    bool render_message_pending_{};
    bool dragging_{};
    int last_mouse_x_{};
    int last_mouse_y_{};
    std::string selected_parameter_;
    std::string transient_status_;
    std::filesystem::path project_path_;

    app::SessionModel session_;
    render::d3d11::CanvasRenderer renderer_;
};

}  // namespace

int run_application(const HINSTANCE instance, const int show_command, const bool smoke_test) {
    MainWindow window{instance};
    if (!window.create()) return EXIT_FAILURE;
    if (smoke_test) {
        if (!window.smoke_present()) {
            show_error_box(nullptr, L"FAULTMINE D3D11/session smoke presentation failed.");
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
