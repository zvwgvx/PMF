#include "pmf/wizard.hpp"

#ifdef _WIN32

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return pmf::ui::run_loader(instance, show);
}

#else

#include <iostream>

int main() {
    std::cout << "PMF Setup Wizard is available on Windows only.\n";
    return 1;
}

#endif
