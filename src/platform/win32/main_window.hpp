#pragma once

#include <windows.h>

namespace faultmine::platform::win32 {

int run_application(HINSTANCE instance, int show_command, bool smoke_test);

}  // namespace faultmine::platform::win32
