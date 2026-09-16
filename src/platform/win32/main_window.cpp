#include "platform/win32/main_window.hpp"

#include "faultmine/image.hpp"
#include "faultmine/session.hpp"
#include "faultmine/wic_io.hpp"
#include "render/d3d11/canvas_renderer.hpp"

#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace faultmine::platform::win32 {
namespace {

constexpr wchar_t kWindowClassName[] = L"FAULTMINE.MainWindow";
constexpr wchar_t kWindowTitle[] = L"FAULTMINE";
constexpr int kStatusHeight = 24;
constexpr UINT kRenderMessage = WM_APP + 4U;

enum CommandId : UINT {
    command_open = 1001U,
    command_export = 1002U,
    command_exit = 1003U,
    command_fit = 1101U,
    command_actual = 1102U,
    command_before = 1103U,
    command_proxy = 1104U,
    command_zoom_in = 1105U,
    command_zoom_out = 1106U,
    command_toggle_effects = 1201U,
    command_row_left = 1202U,
    command_row_right = 1203U,
    command_jitter_down = 1204U,
    command_jitter_up = 1205U,
    command_reroll = 1206U,
};

std::wstring utf8_to_wide(const std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (required <= 0) {
        return L"<invalid UTF-8>";
    }
    std::wstring output(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        output.data(),
        required);
    return output;
}

void show_error_box(const HWND owner, const std::wstring& message) {
    MessageBoxW(owner, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
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
            0,
            kWindowClassName,
            kWindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1280,
            800,
            nullptr,
            nullptr,
            instance_,
            this);
        if (hwnd_ == nullptr) {
            show_error_box(nullptr, L"CreateWindowExW failed: " + std::to_wstring(GetLastError()));
            return false;
        }

        create_menu();
        status_ = CreateWindowExW(
            0,
            L"STATIC",
            L"Open an image with Ctrl+O.",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            0,
            0,
            100,
            kStatusHeight,
            hwnd_,
            nullptr,
            instance_,
            nullptr);
        if (status_ != nullptr) {
            SendMessageW(status_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (auto error = renderer_.initialize(hwnd_); error.has_value()) {
            show_error_box(hwnd_, L"D3D11 initialization failed:\n" + utf8_to_wide(*error));
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
            return false;
        }
        renderer_ready_ = true;
        resize_children();
        update_status();
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
        if (!session_.ensure_preview(&error)) {
            return false;
        }
        if (!refresh_display_texture(&error)) {
            return false;
        }
        if (auto draw_error = renderer_.draw(session_.view_state()); draw_error.has_value()) {
            return false;
        }
        return true;
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
    static LRESULT CALLBACK window_proc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            self = static_cast<MainWindow*>(create->lpCreateParams);
            if (self != nullptr) {
                self->hwnd_ = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
        }
        if (self != nullptr) {
            return self->handle_message(message, w_param, l_param);
        }
        return DefWindowProcW(window, message, w_param, l_param);
    }

    LRESULT handle_message(const UINT message, const WPARAM w_param, const LPARAM l_param) {
        switch (message) {
            case WM_COMMAND:
                handle_command(static_cast<UINT>(LOWORD(w_param)));
                return 0;
            case WM_KEYDOWN:
                handle_key(w_param);
                return 0;
            case WM_MOUSEWHEEL: {
                const int delta = GET_WHEEL_DELTA_WPARAM(w_param);
                session_.zoom_by(delta > 0 ? 1.2 : (1.0 / 1.2));
                update_status();
                InvalidateRect(hwnd_, nullptr, FALSE);
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
                    session_.pan_by(
                        static_cast<double>(x - last_mouse_x_),
                        static_cast<double>(y - last_mouse_y_));
                    last_mouse_x_ = x;
                    last_mouse_y_ = y;
                    update_status();
                    InvalidateRect(hwnd_, nullptr, FALSE);
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
        HMENU view_menu = CreatePopupMenu();
        HMENU fault_menu = CreatePopupMenu();

        AppendMenuW(file_menu, MF_STRING, command_open, L"&Open...\tCtrl+O");
        AppendMenuW(file_menu, MF_STRING, command_export, L"&Export canonical PNG...\tCtrl+E");
        AppendMenuW(file_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(file_menu, MF_STRING, command_exit, L"E&xit");

        AppendMenuW(view_menu, MF_STRING, command_fit, L"&Fit\tF");
        AppendMenuW(view_menu, MF_STRING, command_actual, L"&1:1\t1");
        AppendMenuW(view_menu, MF_STRING, command_zoom_in, L"Zoom &in");
        AppendMenuW(view_menu, MF_STRING, command_zoom_out, L"Zoom &out");
        AppendMenuW(view_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(view_menu, MF_STRING, command_before, L"&Before / after\tB");
        AppendMenuW(view_menu, MF_STRING | MF_CHECKED, command_proxy, L"Use &proxy preview\tP");

        AppendMenuW(fault_menu, MF_STRING | MF_CHECKED, command_toggle_effects, L"Starter faults enabled\tSpace");
        AppendMenuW(fault_menu, MF_STRING, command_row_left, L"Row offset -1\t[");
        AppendMenuW(fault_menu, MF_STRING, command_row_right, L"Row offset +1\t]");
        AppendMenuW(fault_menu, MF_STRING, command_jitter_down, L"Jitter -1\t-");
        AppendMenuW(fault_menu, MF_STRING, command_jitter_up, L"Jitter +1\t=");
        AppendMenuW(fault_menu, MF_STRING, command_reroll, L"Reroll seed\tR");

        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"&File");
        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), L"&View");
        AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(fault_menu), L"&Faults");
        SetMenu(hwnd_, menu_);
    }

    void resize_children() {
        if (hwnd_ == nullptr) {
            return;
        }
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int width = std::max<LONG>(1, client.right - client.left);
        const int height = std::max<LONG>(1, client.bottom - client.top);
        if (status_ != nullptr) {
            MoveWindow(status_, 4, std::max(0, height - kStatusHeight), std::max(1, width - 8), kStatusHeight, TRUE);
        }
        if (renderer_ready_) {
            if (auto error = renderer_.resize(
                    static_cast<std::uint32_t>(width),
                    static_cast<std::uint32_t>(height));
                error.has_value()) {
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

    void handle_command(const UINT command) {
        switch (command) {
            case command_open:
                open_source();
                break;
            case command_export:
                export_png();
                break;
            case command_exit:
                PostMessageW(hwnd_, WM_CLOSE, 0U, 0U);
                break;
            case command_fit:
                session_.set_fit_view();
                update_status();
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case command_actual:
                session_.set_actual_view();
                update_status();
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case command_zoom_in:
                session_.zoom_by(1.2);
                update_status();
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case command_zoom_out:
                session_.zoom_by(1.0 / 1.2);
                update_status();
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case command_before:
                session_.toggle_before();
                sync_menu_checks();
                refresh_and_repaint();
                break;
            case command_proxy:
                session_.set_proxy_enabled(!session_.proxy_enabled());
                sync_menu_checks();
                schedule_render();
                break;
            case command_toggle_effects:
                session_.toggle_effects();
                sync_menu_checks();
                schedule_render();
                break;
            case command_row_left:
                session_.adjust_row_offset(-1);
                schedule_render();
                break;
            case command_row_right:
                session_.adjust_row_offset(1);
                schedule_render();
                break;
            case command_jitter_down:
                session_.adjust_jitter(-1);
                schedule_render();
                break;
            case command_jitter_up:
                session_.adjust_jitter(1);
                schedule_render();
                break;
            case command_reroll:
                session_.reroll_seed();
                schedule_render();
                break;
            default:
                break;
        }
    }

    void handle_key(const WPARAM key) {
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (control && (key == 'O' || key == 'o')) {
            open_source();
            return;
        }
        if (control && (key == 'E' || key == 'e')) {
            export_png();
            return;
        }

        switch (key) {
            case 'F':
                handle_command(command_fit);
                break;
            case '1':
                handle_command(command_actual);
                break;
            case 'B':
                handle_command(command_before);
                break;
            case 'P':
                handle_command(command_proxy);
                break;
            case 'R':
                handle_command(command_reroll);
                break;
            case VK_SPACE:
                handle_command(command_toggle_effects);
                break;
            case VK_OEM_4:
                handle_command(command_row_left);
                break;
            case VK_OEM_6:
                handle_command(command_row_right);
                break;
            case VK_OEM_MINUS:
                handle_command(command_jitter_down);
                break;
            case VK_OEM_PLUS:
                handle_command(command_jitter_up);
                break;
            case VK_LEFT:
                session_.pan_by(-20.0, 0.0);
                refresh_and_repaint(false);
                break;
            case VK_RIGHT:
                session_.pan_by(20.0, 0.0);
                refresh_and_repaint(false);
                break;
            case VK_UP:
                session_.pan_by(0.0, -20.0);
                refresh_and_repaint(false);
                break;
            case VK_DOWN:
                session_.pan_by(0.0, 20.0);
                refresh_and_repaint(false);
                break;
            default:
                break;
        }
    }

    void open_source() {
        std::array<wchar_t, 32768> filename{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.wdp\0All files\0*.*\0\0";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog) == FALSE) {
            const DWORD extended = CommDlgExtendedError();
            if (extended != 0U) {
                show_error_box(hwnd_, L"Open dialog failed: " + std::to_wstring(extended));
            }
            return;
        }

        io::WicLoadResult loaded = io::load_wic_image(std::filesystem::path{filename.data()});
        if (!loaded.ok()) {
            show_error_box(hwnd_, L"Image load failed:\n" + utf8_to_wide(loaded.error->message));
            return;
        }

        std::string error;
        io::LoadedSource source = std::move(*loaded.source);
        const std::wstring title_path = source.path.filename().wstring();
        if (!session_.set_source(
                std::move(source.image),
                std::move(source.source_identity),
                std::move(source.path),
                &error)) {
            show_error_box(hwnd_, L"Session rejected source:\n" + utf8_to_wide(error));
            return;
        }

        SetWindowTextW(hwnd_, (std::wstring{kWindowTitle} + L" — " + title_path).c_str());
        if (const core::ImageBuffer* source_image = session_.full_source(); source_image != nullptr) {
            if (auto upload_error = renderer_.upload_image(*source_image); upload_error.has_value()) {
                set_status_error(*upload_error);
            }
        }
        sync_menu_checks();
        update_status();
        InvalidateRect(hwnd_, nullptr, FALSE);
        schedule_render();
    }

    void export_png() {
        if (!session_.has_source()) {
            show_error_box(hwnd_, L"Load an image before exporting.");
            return;
        }
        std::array<wchar_t, 32768> filename{};
        const std::wstring default_name = session_.source_path().stem().wstring() + L"-faultmine.png";
        std::copy_n(
            default_name.c_str(),
            std::min<std::size_t>(default_name.size(), filename.size() - 1U),
            filename.data());

        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = L"PNG image\0*.png\0All files\0*.*\0\0";
        dialog.lpstrDefExt = L"png";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameW(&dialog) == FALSE) {
            const DWORD extended = CommDlgExtendedError();
            if (extended != 0U) {
                show_error_box(hwnd_, L"Save dialog failed: " + std::to_wstring(extended));
            }
            return;
        }

        std::string error;
        auto full_result = session_.render_full(&error);
        if (!full_result.has_value()) {
            show_error_box(hwnd_, L"Canonical export render failed:\n" + utf8_to_wide(error));
            return;
        }
        if (auto save_error = io::save_wic_png(*full_result, std::filesystem::path{filename.data()}); save_error.has_value()) {
            show_error_box(hwnd_, L"PNG export failed:\n" + utf8_to_wide(save_error->message));
            return;
        }
        transient_status_ = "Exported full-resolution canonical PNG.";
        update_status();
    }

    void sync_menu_checks() {
        if (menu_ == nullptr) {
            return;
        }
        CheckMenuItem(
            menu_,
            command_proxy,
            MF_BYCOMMAND | (session_.proxy_enabled() ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(
            menu_,
            command_before,
            MF_BYCOMMAND | (session_.view_state().show_before ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(
            menu_,
            command_toggle_effects,
            MF_BYCOMMAND | (session_.effects_enabled() ? MF_CHECKED : MF_UNCHECKED));
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
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void set_status_error(const std::string& error) {
        transient_status_ = "ERROR: " + error;
        update_status();
    }

    void update_status() {
        if (status_ == nullptr) {
            return;
        }
        std::wostringstream status;
        if (!session_.has_source()) {
            status << L"Ctrl+O open | F fit | 1 1:1 | wheel zoom | drag/arrow pan | B before/after";
        } else {
            const core::ImageBuffer* full = session_.full_source();
            const app::PreviewState preview = session_.preview_state();
            status << session_.source_path().filename().wstring() << L" | ";
            if (full != nullptr) {
                status << full->width << L'x' << full->height;
            }
            if (preview.width != 0U) {
                status << L" -> " << preview.width << L'x' << preview.height;
            }
            status << (preview.is_proxy ? L" | PROXY" : L" | FULL");
            status << (session_.view_state().show_before ? L" | BEFORE" : L" | RESULT");
            status << (session_.effects_enabled() ? L" | FX on" : L" | FX off");
            status << L" | row " << session_.row_offset_amount();
            status << L" | jitter " << session_.jitter_max_shift();
            status << L" | seed " << utf8_to_wide(session_.genome().root_seed.to_string().substr(0U, 8U));
            status << (renderer_.using_warp() ? L" | D3D11 WARP" : L" | D3D11 HW");
        }
        if (!transient_status_.empty()) {
            status << L" | " << utf8_to_wide(transient_status_);
            transient_status_.clear();
        }
        SetWindowTextW(status_, status.str().c_str());
    }

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND status_{};
    HMENU menu_{};
    bool renderer_ready_{};
    bool render_message_pending_{};
    bool dragging_{};
    int last_mouse_x_{};
    int last_mouse_y_{};
    std::string transient_status_;

    app::SessionModel session_;
    render::d3d11::CanvasRenderer renderer_;
};

}  // namespace

int run_application(
    const HINSTANCE instance,
    const int show_command,
    const bool smoke_test) {
    MainWindow window{instance};
    if (!window.create()) {
        return EXIT_FAILURE;
    }

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
        if (target != nullptr) {
            PostMessageW(target, WM_CLOSE, 0U, 0U);
        }
    }
    return window.message_loop();
}

}  // namespace faultmine::platform::win32
