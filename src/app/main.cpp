#include "platform/win32/main_window.hpp"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return faultmine::platform::win32::run_application(instance, show_command);
}
