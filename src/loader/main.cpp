#include "pmf/identity.hpp"
#include "pmf/service.hpp"
#include "pmf/windows_ui.hpp"

#ifdef _WIN32

#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace {

std::wstring ask(const std::wstring& prompt) {
    std::wcout << prompt << std::flush;
    std::wstring value;
    std::getline(std::wcin, value);
    return value;
}

std::string label_ascii(const std::wstring& label) {
    std::string value;
    for (wchar_t c : label) {
        if (c < 0 || c > 127) return {};
        value.push_back(static_cast<char>(c));
    }
    return value;
}

void erase(std::wstring& value) {
    if (!value.empty()) {
        SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
        value.clear();
    }
}

bool confirm(const std::wstring& prompt) {
    return ask(prompt + L" [y/N]: ") == L"y";
}

int install(int argc, wchar_t** argv) {
    const std::filesystem::path file = argc >= 3
        ? std::filesystem::path(argv[2])
        : std::filesystem::path(ask(L"Service executable (.exe): "));
    const std::wstring label_w = argc >= 4
        ? std::wstring(argv[3])
        : ask(L"Service label: ");
    const std::string label = label_ascii(label_w);
    const auto identity = pmf::inspect_target(file, label);

    if (!identity) {
        std::wcerr << L"Invalid executable path or service label.\n";
        return 1;
    }

    std::string message;
    if (!pmf::inspect_service_executable(identity->executable, message)) {
        std::wcerr << L"Service validation: " << message.c_str() << L"\n";
        return 1;
    }

    std::wcout << L"\nSelected: " << identity->executable.wstring() << L"\n";
    std::wcout << L"Service name: PMF-" << label_w << L"\n";
    std::wcout << L"Account: NT AUTHORITY\\LocalService\n";
    std::wcout << L"Startup: Automatic\n";
    std::wcout << L"Destination: Program Files\\PMF\\Services\\" << label_w << L"\n";
    std::wcout << L"The service will be visible and removable through Windows Services.\n\n";

    auto password = pmf::read_secret(L"Management password (optional, Enter for none): ");

    if (!password.empty()) {
        auto confirmation = pmf::read_secret(L"Confirm password: ");
        const bool matches = password == confirmation;
        erase(confirmation);

        if (!matches) {
            erase(password);
            std::wcerr << L"Passwords do not match.\n";
            return 1;
        }
    } else {
        std::wcout << L"Warning: no password will protect PMF's normal removal workflow.\n";
        if (!confirm(L"Continue without a password?")) {
            erase(password);
            return 0;
        }
    }

    const auto recovery_input = ask(L"Full path for new recovery .txt file: ");
    if (recovery_input.empty()) {
        erase(password);
        std::wcerr << L"A recovery file destination is required.\n";
        return 1;
    }

    const std::filesystem::path recovery(recovery_input);
    if (!confirm(L"Register and start this LocalService Windows Service?")) {
        erase(password);
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    std::string error;
    const bool success = pmf::install_service(*identity, password, recovery, error);
    erase(password);

    if (!success) {
        std::cerr << "Installation failed: " << error << "\n";
        return 1;
    }

    std::wcout << L"Service installed and started: PMF-" << label_w << L"\n";
    std::wcout << L"Recovery file: " << recovery.wstring() << L"\n";
    return 0;
}

void usage() {
    std::wcout << L"PMF 0.5.0 - Windows Service installer\n\n";
    std::wcout << L"Usage:\n";
    std::wcout << L"  pmf-loader.exe install [service.exe] [label]\n";
    std::wcout << L"  pmf-loader.exe help\n";
    std::wcout << L"Without arguments, the installer prompts for a service EXE and label.\n";
}

}

int wmain(int argc, wchar_t** argv) {
    if (argc == 1) {
        usage();
        return install(argc, argv);
    }

    const std::wstring command(argv[1]);
    if (command == L"help" || command == L"--help" || command == L"-h") {
        usage();
        return 0;
    }

    if (command != L"install" || argc > 4) {
        usage();
        return 2;
    }

    return install(argc, argv);
}

#else

#include <iostream>

int main() {
    std::cout << "PMF Windows Service installer is available on Windows only.\n";
    return 1;
}

#endif
