#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: pmf-unloader\n";
        return 0;
    }

    if (argc != 1) {
        std::cerr << "Usage: pmf-unloader\n";
        return 2;
    }

    std::cout << "PMF Unloader 0.1.0\n";
    std::cout << "Removal is not implemented in this bootstrap version.\n";
    return 0;
}
