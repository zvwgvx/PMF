#include "pmf/identity.hpp"
#include "pmf/managed.hpp"
#include "pmf/workspace.hpp"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

int main() {
    namespace fs = std::filesystem;

    const auto label = "pmf-test-" + std::to_string(GetCurrentProcessId());
    const auto test_directory = fs::temp_directory_path() / label;
    std::error_code ec;
    fs::create_directories(test_directory, ec);

    if (ec) {
        std::cerr << "PMF lifecycle failure 1\\n";
        return 1;
    }

    const auto source = test_directory / L"sample.exe";
    const auto recovery_file = test_directory / L"recovery.txt";

    {
        std::ofstream output(source, std::ios::binary);
        output << "MZ" << "integration-test";
    }

    const auto identity = pmf::inspect_target(source, label);
    const auto workspace = pmf::prepare_workspace(ec);

    if (!identity || !workspace || ec) {
        std::cerr << "PMF lifecycle failure 2\\n";
        return 2;
    }

    std::string error;

    if (!pmf::install_managed(*identity, L"test-password-at-least-12", recovery_file, error)) {
        std::cerr << error << '\n';
        std::cerr << "PMF lifecycle failure 3\\n";
        return 3;
    }

    const auto managed_binary = workspace->path / L"managed" / label / L"payload.exe";

    if (!fs::is_regular_file(managed_binary)) {
        std::cerr << "PMF lifecycle failure 4\\n";
        return 4;
    }

    error.clear();

    if (pmf::uninstall_managed(label, L"wrong-password", false, error)) {
        std::cerr << "PMF lifecycle failure 5\\n";
        return 5;
    }

    std::ifstream recovery_input(recovery_file);
    std::string line;
    std::string recovery;

    while (std::getline(recovery_input, line)) {
        const std::string prefix = "Recovery code: ";

        if (line.rfind(prefix, 0) == 0) {
            recovery = line.substr(prefix.size());
        }
    }

    if (recovery.empty()) {
        std::cerr << "PMF lifecycle failure 6\\n";
        return 6;
    }

    const std::wstring recovery_wide(recovery.begin(), recovery.end());
    error.clear();

    if (!pmf::uninstall_managed(label, recovery_wide, true, error)) {
        std::cerr << error << '\n';
        std::cerr << "PMF lifecycle failure 7\\n";
        return 7;
    }

    if (fs::exists(managed_binary) || !fs::exists(source)) {
        std::cerr << "PMF lifecycle failure 8\\n";
        return 8;
    }

    fs::remove_all(test_directory, ec);

    if (ec) {
        std::cerr << "PMF lifecycle failure 9\\n";
        return 9;
    }

    std::cout << "PMF managed lifecycle passed\n";
    return 0;
}
