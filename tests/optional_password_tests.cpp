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

    const auto temp = fs::temp_directory_path() /
        ("pmf-auth-" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::create_directories(temp, ec);
    if (ec) return 1;

    const auto source = temp / L"sample.exe";
    {
        std::ofstream out(source, std::ios::binary);
        out << "MZ" << "test-data";
    }

    const auto workspace = pmf::prepare_workspace(ec);
    if (!workspace || ec) return 2;

    for (int i = 0; i < 2; ++i) {
        const auto label = "pmf-auth-" + std::to_string(GetCurrentProcessId()) +
                           "-" + std::to_string(i);
        const auto identity = pmf::inspect_target(source, label);
        if (!identity) return 3;

        const std::wstring password = i == 0 ? L"" : L"x";
        const auto recovery = temp / (std::to_string(i) + "-recovery.txt");
        std::string error;

        if (!pmf::install_managed(*identity, password, recovery, error, false)) {
            std::cerr << error << '\n';
            return 4;
        }

        if (!pmf::verify_managed_secret(label, password, false, error)) return 5;

        if (pmf::verify_managed_secret(label, L"incorrect", false, error)) return 6;

        if (i == 1 && pmf::verify_managed_secret(label, L"", false, error)) return 7;

        if (!pmf::uninstall_managed(label, password, false, error)) {
            std::cerr << error << '\n';
            return 8;
        }

        if (fs::exists(workspace->path / L"managed" / label)) return 9;
        if (!fs::exists(recovery)) return 10;
    }

    fs::remove_all(temp, ec);
    return ec ? 11 : 0;
}
