#include "pmf/identity.hpp"
#include "pmf/workspace.hpp"

#include <iostream>
#include <string>
#include <system_error>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: pmf-loader [executable.exe label]\n";
        return 0;
    }

    std::string executable;
    std::string label;

    if (argc == 3) {
        executable = argv[1];
        label = argv[2];
    } else if (argc == 1) {
        std::cout << "Windows executable: ";
        std::getline(std::cin, executable);
        std::cout << "Installation label: ";
        std::getline(std::cin, label);
    } else {
        std::cerr << "Usage: pmf-loader [executable.exe label]\n";
        return 2;
    }

    const auto identity = pmf::inspect_target(executable, label);

    if (!identity) {
        std::cerr << "Invalid label or Windows executable path.\n";
        return 1;
    }

    std::cout << "PMF Loader 0.1.0\n";
    std::cout << "Label: " << identity->label << '\n';
    std::cout << "Executable: " << identity->executable.string() << '\n';

    const auto workspace = pmf::workspace_directory();

    if (!workspace) {
        std::cout << "Workspace setup is supported on Windows only.\n";
        std::cout << "No application installation or startup changes were made.\n";
        return 0;
    }

    std::cout << "Workspace: " << workspace->string() << '\n';
    std::cout << "Create or verify this directory? [y/N]: ";

    std::string answer;
    std::getline(std::cin, answer);

    if (answer != "y" && answer != "Y" && answer != "yes" && answer != "YES") {
        std::cout << "Cancelled. No changes were made.\n";
        return 0;
    }

    std::error_code ec;
    const auto result = pmf::prepare_workspace(ec);

    if (!result) {
        std::cerr << "Could not prepare workspace: " << ec.message() << '\n';
        return 1;
    }

    std::cout << (result->already_existed ? "Workspace verified: " : "Workspace created: ")
              << result->path.string() << '\n';
    std::cout << "Preparation complete. No application installation or startup changes were made.\n";
    return 0;
}
