#include "platform/win32/main_window.hpp"

#include <windows.h>

#include <string_view>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR command_line,
    int show_command) {
    const std::wstring_view arguments =
        command_line != nullptr ? std::wstring_view{command_line} : std::wstring_view{};
    const bool smoke_test = arguments == L"--smoke-test";

    return faultmine::platform::win32::run_application(instance, show_command, smoke_test);
}
