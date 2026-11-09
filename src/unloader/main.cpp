#include "pmf/workspace.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: pmf-unloader [--status]\n";
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--status") {
        const auto path = pmf::workspace_directory();

        if (!path) {
            std::cout << "Windows workspace is unavailable on this platform.\n";
            return 0;
        }

        std::error_code ec;
        const bool exists = std::filesystem::is_directory(*path, ec);

        if (ec) {
            std::cerr << "Cannot inspect workspace: " << ec.message() << '\n';
            return 1;
        }

        std::cout << "Workspace: " << path->string() << '\n';
        std::cout << "Status: " << (exists ? "present" : "not found") << '\n';
        return 0;
    }

    if (argc != 1) {
        std::cerr << "Usage: pmf-unloader [--status]\n";
        return 2;
    }

    std::cout << "PMF Unloader 0.1.0\n";
    std::cout << "Removal is not implemented in this bootstrap version.\n";
    return 0;
}
