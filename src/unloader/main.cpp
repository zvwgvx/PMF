#include "pmf/managed.hpp"
#include "pmf/windows_ui.hpp"
#include "pmf/workspace.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#ifdef _WIN32

#include <windows.h>

namespace {

std::string ascii_label(const std::wstring& input) {
    std::string output;

    for (const auto character : input) {
        if (character > 127 || character < 0) {
            return {};
        }

        output.push_back(static_cast<char>(character));
    }

    return output;
}

void clear_secret(std::wstring& secret) {
    if (!secret.empty()) {
        SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
        secret.clear();
    }
}

void print_list() {
    std::string error;
    const auto labels = pmf::list_managed(error);

    if (!error.empty()) {
        std::cout << "Cannot list managed applications: " << error << '\n';
        return;
    }

    std::cout << "Managed applications: " << labels.size() << '\n';

    for (const auto& label : labels) {
        std::cout << "  " << label << '\n';
    }
}

}

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring(argv[1]) == L"--help") {
        std::wcout << L"Usage: pmf-unloader [label | --list | --status]\n";
        return 0;
    }

    if (argc == 2 && std::wstring(argv[1]) == L"--list") {
        print_list();
        return 0;
    }

    if (argc == 2 && std::wstring(argv[1]) == L"--status") {
        const auto path = pmf::workspace_directory();

        if (!path) {
            std::wcerr << L"Windows workspace is unavailable.\n";
            return 1;
        }

        std::error_code ec;
        const bool present = std::filesystem::is_directory(*path, ec);

        if (ec) {
            std::wcerr << L"Cannot inspect workspace.\n";
            return 1;
        }

        std::wcout << L"Workspace: " << path->wstring() << L"\n";
        std::wcout << L"Status: " << (present ? L"present" : L"not found") << L"\n";
        return 0;
    }

    std::wstring label_w;

    if (argc == 2) {
        label_w = argv[1];
    } else if (argc == 1) {
        print_list();
        std::wcout << L"Installation label to remove: ";
        std::getline(std::wcin, label_w);
    } else {
        std::wcerr << L"Usage: pmf-unloader [label | --list | --status]\n";
        return 2;
    }

    const std::string label = ascii_label(label_w);

    if (!pmf::valid_label(label)) {
        std::wcerr << L"Invalid installation label.\n";
        return 1;
    }

    std::wcout << L"Remove managed copy: " << label_w << L"\n";
    std::wcout << L"Use [p]assword or [r]ecovery code? ";
    std::wstring mode;
    std::getline(std::wcin, mode);

    if (mode != L"p" && mode != L"P" && mode != L"r" && mode != L"R") {
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    const bool recovery_mode = mode == L"r" || mode == L"R";
    auto secret = pmf::read_secret(recovery_mode ? L"Recovery code: " : L"Password: ");
    std::wcout << L"This removes only the PMF-managed copy, not the original executable.\n";
    std::wcout << L"Confirm removal? [y/N]: ";
    std::wstring answer;
    std::getline(std::wcin, answer);

    if (answer != L"y" && answer != L"Y" && answer != L"yes") {
        clear_secret(secret);
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    std::string error;
    const bool removed = pmf::uninstall_managed(label, secret, recovery_mode, error);
    clear_secret(secret);

    if (!removed) {
        std::wcerr << L"Removal failed: " << error.c_str() << L"\n";
        return 1;
    }

    std::wcout << L"Managed copy removed: " << label_w << L"\n";
    std::wcout << L"Your recovery text file is not deleted automatically.\n";
    return 0;
}

#else

int main() {
    std::cout << "PMF managed removal is available on Windows only.\n";
    return 1;
}

#endif
