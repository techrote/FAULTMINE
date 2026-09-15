#include "platform/win32/main_window.hpp"

#include <cstdlib>
#include <string>

namespace faultmine::platform::win32 {
namespace {

constexpr wchar_t kWindowClassName[] = L"FAULTMINE.MainWindow";
constexpr wchar_t kWindowTitle[] = L"FAULTMINE";

void show_win32_error(const wchar_t* operation, const DWORD error_code) {
    std::wstring message{operation};
    message += L" failed. Win32 error code: ";
    message += std::to_wstring(error_code);
    MessageBoxW(nullptr, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK window_proc(
    const HWND window,
    const UINT message,
    const WPARAM w_param,
    const LPARAM l_param) {
    switch (message) {
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(EXIT_SUCCESS);
            return 0;
        default:
            return DefWindowProcW(window, message, w_param, l_param);
    }
}

}  // namespace

int run_application(
    const HINSTANCE instance,
    const int show_command,
    const bool smoke_test) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    window_class.lpszClassName = kWindowClassName;

    const ATOM class_atom = RegisterClassExW(&window_class);
    if (class_atom == 0) {
        show_win32_error(L"RegisterClassExW", GetLastError());
        return EXIT_FAILURE;
    }

    const HWND window = CreateWindowExW(
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
        instance,
        nullptr);

    if (window == nullptr) {
        const DWORD error_code = GetLastError();
        UnregisterClassW(kWindowClassName, instance);
        show_win32_error(L"CreateWindowExW", error_code);
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        if (PostMessageW(window, WM_CLOSE, 0, 0) == 0) {
            const DWORD error_code = GetLastError();
            DestroyWindow(window);
            UnregisterClassW(kWindowClassName, instance);
            show_win32_error(L"PostMessageW", error_code);
            return EXIT_FAILURE;
        }
    } else {
        ShowWindow(window, show_command);
        UpdateWindow(window);
    }

    MSG message{};
    while (true) {
        const BOOL get_message_result = GetMessageW(&message, nullptr, 0, 0);
        if (get_message_result == 0) {
            break;
        }
        if (get_message_result == -1) {
            const DWORD error_code = GetLastError();
            DestroyWindow(window);
            UnregisterClassW(kWindowClassName, instance);
            show_win32_error(L"GetMessageW", error_code);
            return EXIT_FAILURE;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnregisterClassW(kWindowClassName, instance);
    return static_cast<int>(message.wParam);
}

}  // namespace faultmine::platform::win32
