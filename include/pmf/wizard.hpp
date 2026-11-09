#pragma once

#ifdef _WIN32

#include <windows.h>

namespace pmf::ui {

int run_loader(HINSTANCE instance, int show);
int run_unloader(HINSTANCE instance, int show);

}

#endif
