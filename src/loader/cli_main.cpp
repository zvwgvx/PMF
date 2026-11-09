#include "pmf/console_tui.hpp"
#include "pmf/identity.hpp"
#include "pmf/service.hpp"
#include "pmf/windows_ui.hpp"

#ifdef _WIN32

#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string ascii(const std::wstring& value) {
    std::string result;
    for (const auto character : value) {
        if (character > 127 || character < 0) return {};
        result.push_back(static_cast<char>(character));
    }
    return result;
}

std::wstring suggested_label(const std::filesystem::path& file) {
    std::wstring label;
    for (const wchar_t character : file.stem().wstring()) {
        if ((character >= L'a' && character <= L'z') ||
            (character >= L'0' && character <= L'9') ||
            character == L'-' || character == L'_') {
            label.push_back(character);
        } else if (character >= L'A' && character <= L'Z') {
            label.push_back(character - L'A' + L'a');
        } else if (!label.empty() && label.back() != L'-') {
            label.push_back(L'-');
        }
    }
    if (label.empty()) label = L"service";
    return label;
}

void erase(std::wstring& value) {
    if (!value.empty()) {
        SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
        value.clear();
    }
}

void usage() {
    pmf::tui::header(L"PMF LOADER", L"Windows Service installation");
    std::wcout << L"  pmf-loader-cli.exe\n";
    std::wcout << L"  pmf-loader-cli.exe install [service.exe] [label]\n";
    std::wcout << L"  pmf-loader-cli.exe help\n\n";
    std::wcout << L"  Launch without arguments to use the interactive wizard.\n";
}

int install(int argc, wchar_t** argv) {
    pmf::tui::clear();
    pmf::tui::header(L"PMF LOADER", L"Step 1 of 4 - Select service executable");

    std::filesystem::path file;
    const bool path_argument = argc >= 3;
    for (;;) {
        const auto value = path_argument
            ? std::wstring(argv[2])
            : pmf::tui::normalize_path(pmf::tui::ask(L"Service EXE path (drag and drop supported)"));

        if (value.empty()) {
            std::wcout << L"Cancelled.\n";
            return 0;
        }

        file = std::filesystem::path(pmf::tui::normalize_path(value));
        std::string error;
        if (pmf::inspect_service_executable(file, error)) break;

        std::cerr << "Cannot use this executable: " << error << "\n";
        if (path_argument || !std::wcin.good()) return 1;
    }

    pmf::tui::header(L"PMF LOADER", L"Step 2 of 4 - Service configuration");

    std::wstring label_w;
    const bool label_argument = argc >= 4;
    for (;;) {
        label_w = label_argument
            ? std::wstring(argv[3])
            : pmf::tui::ask(L"Service label", suggested_label(file));

        if (pmf::valid_label(ascii(label_w))) break;
        std::wcerr << L"Invalid label. Use a Windows-compatible ASCII label.\n";
        if (label_argument || !std::wcin.good()) return 1;
    }

    const auto identity = pmf::inspect_target(file, ascii(label_w));
    if (!identity) {
        std::wcerr << L"Invalid service executable or label.\n";
        return 1;
    }

    std::wcout << L"\nService: PMF-" << label_w << L"\n";
    std::wcout << L"Account: NT AUTHORITY\\LocalService\n";
    std::wcout << L"Startup: Automatic\n";
    std::wcout << L"Destination: C:\\Program Files\\PMF\\Services\\"
               << label_w << L"\\service.exe\n";

    pmf::tui::header(L"PMF LOADER", L"Step 3 of 4 - Management credentials");

    auto password = pmf::read_secret(L"Management password (Enter for none): ");
    if (!password.empty()) {
        for (;;) {
            auto confirmation = pmf::read_secret(L"Confirm password: ");
            const bool matches = password == confirmation;
            erase(confirmation);
            if (matches) break;
            std::wcerr << L"Passwords do not match. Try again.\n";
            erase(password);
            password = pmf::read_secret(L"Management password: ");
            if (!std::wcin.good()) return 1;
        }
    } else {
        std::wcout << L"Warning: no management password will be configured.\n";
        if (!pmf::tui::confirm(L"Continue without a password?")) {
            erase(password);
            return 0;
        }
    }

    const auto default_recovery = std::filesystem::current_path() /
        std::filesystem::path(L"PMF-" + label_w + L"-recovery.txt");

    const auto recovery_text = pmf::tui::normalize_path(
        pmf::tui::ask(L"Recovery file", default_recovery.wstring()));
    if (recovery_text.empty()) {
        erase(password);
        std::wcerr << L"A recovery file is required.\n";
        return 1;
    }

    const std::filesystem::path recovery(recovery_text);

    pmf::tui::header(L"PMF LOADER", L"Step 4 of 4 - Review and install");
    std::wcout << L"Source: " << identity->executable.wstring() << L"\n";
    std::wcout << L"Service: PMF-" << label_w << L"\n";
    std::wcout << L"Account: LocalService\n";
    std::wcout << L"Recovery: " << recovery.wstring() << L"\n";
    std::wcout << L"Password: " << (password.empty() ? L"None" : L"Configured") << L"\n\n";

    if (!pmf::tui::confirm(L"Install and start this visible Windows Service?")) {
        erase(password);
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    std::wcout << L"\nInstalling...\n";
    std::string error;
    const bool success = pmf::install_service(*identity, password, recovery, error);
    erase(password);

    if (!success) {
        std::cerr << "Installation failed: " << error << "\n";
        return 1;
    }

    std::wcout << L"\nInstallation complete.\n";
    std::wcout << L"Service: PMF-" << label_w << L"\n";
    std::wcout << L"Recovery: " << recovery.wstring() << L"\n";
    return 0;
}

}

int wmain(int argc, wchar_t** argv) {
    if (argc == 1) {
        const int result = install(argc, argv);
        pmf::tui::pause();
        return result;
    }

    const std::wstring command(argv[1]);
    if (command == L"help" || command == L"--help" || command == L"-h") {
        usage();
        return 0;
    }

    if (command == L"install" && argc <= 4) return install(argc, argv);

    usage();
    return 2;
}

#else

#include <iostream>

int main() {
    std::cout << "PMF Windows Service installer is available on Windows only.\n";
    return 1;
}

#endif
