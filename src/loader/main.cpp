#include "pmf/identity.hpp"
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

}

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring(argv[1]) == L"--help") {
        std::wcout << L"Usage: pmf-loader [path-to-executable.exe label]\n";
        return 0;
    }

    std::filesystem::path executable;
    std::wstring label_w;

    if (argc == 3) {
        executable = argv[1];
        label_w = argv[2];
    } else if (argc == 1) {
        const auto selection = pmf::browse_executable();

        if (!selection) {
            std::wcout << L"No executable selected. Cancelled.\n";
            return 0;
        }

        executable = *selection;
        std::wcout << L"Installation label: ";
        std::getline(std::wcin, label_w);
    } else {
        std::wcerr << L"Usage: pmf-loader [path-to-executable.exe label]\n";
        return 2;
    }

    const std::string label = ascii_label(label_w);
    const auto identity = pmf::inspect_target(executable, label);

    if (!identity) {
        std::wcerr << L"Invalid label or executable path.\n";
        return 1;
    }

    const auto workspace = pmf::workspace_directory();

    if (!workspace) {
        std::wcerr << L"Windows data directory is unavailable.\n";
        return 1;
    }

    std::wcout << L"PMF Loader\n";
    std::wcout << L"Executable: " << identity->executable.wstring() << L"\n";
    std::wcout << L"Label: " << label_w << L"\n";
    std::wcout << L"Managed workspace: " << workspace->wstring() << L"\n";
    std::wcout << L"PMF will copy this executable into its managed workspace.\n";
    std::wcout << L"It will not execute the program or configure automatic startup.\n";
    std::wcout << L"Continue? [y/N]: ";

    std::wstring consent;
    std::getline(std::wcin, consent);

    if (consent != L"y" && consent != L"Y" && consent != L"yes") {
        std::wcout << L"Cancelled. No changes were made.\n";
        return 0;
    }

    auto password = pmf::read_secret(L"Management password (12+ characters): ");

    if (password.size() < 12) {
        clear_secret(password);
        std::wcerr << L"Password must contain at least 12 characters.\n";
        return 1;
    }

    auto repeat = pmf::read_secret(L"Confirm management password: ");
    const bool matches = password == repeat;
    clear_secret(repeat);

    if (!matches) {
        clear_secret(password);
        std::wcerr << L"Passwords do not match.\n";
        return 1;
    }

    std::wcout << L"Choose a secure location OUTSIDE the PMF workspace to save your recovery text file.\n";
    const auto recovery = pmf::browse_recovery_destination(label);

    if (!recovery) {
        clear_secret(password);
        std::wcout << L"Recovery file not selected. Cancelled.\n";
        return 0;
    }

    std::error_code ec;
    const auto prepared = pmf::prepare_workspace(ec);

    if (!prepared) {
        clear_secret(password);
        std::wcerr << L"Could not prepare workspace: " << ec.message().c_str() << L"\n";
        return 1;
    }

    std::string error;
    const bool installed = pmf::install_managed(*identity, password, *recovery, error);
    clear_secret(password);

    if (!installed) {
        std::wcerr << L"Installation failed: " << error.c_str() << L"\n";
        return 1;
    }

    std::wcout << L"Managed copy created for label: " << label_w << L"\n";
    std::wcout << L"Recovery text saved at: " << recovery->wstring() << L"\n";
    std::wcout << L"No executable was launched or registered for startup.\n";
    return 0;
}

#else

int main() {
    std::cout << "PMF managed loading is available on Windows only.\n";
    return 1;
}

#endif
