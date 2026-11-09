#include "pmf/identity.hpp"
#include "pmf/service.hpp"
#include "pmf/windows_ui.hpp"

#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <string>

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

int list() {
    std::string error;
    const auto services = pmf::list_services(error);

    if (!error.empty()) {
        std::cerr << "Cannot list services: " << error << "\n";
        return 1;
    }

    std::wcout << L"PMF-managed Windows Services (" << services.size() << L"):\n";
    for (const auto& service : services) {
        std::cout << "  PMF-" << service << "\n";
    }

    return 0;
}

int remove(int argc, wchar_t** argv) {
    if (argc == 2) {
        if (list() != 0) return 1;
    }

    const std::wstring label_w = argc >= 3
        ? std::wstring(argv[2])
        : ask(L"Service label to remove: ");
    const auto label = label_ascii(label_w);

    if (!pmf::valid_label(label)) {
        std::wcerr << L"Invalid service label.\n";
        return 1;
    }

    const auto method = ask(L"Authentication [p=password, r=recovery, n=no password]: ");
    const bool recovery = method == L"r";
    const bool no_password = method == L"n";

    if (!recovery && !no_password && method != L"p") {
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    auto secret = no_password ? std::wstring() :
        pmf::read_secret(recovery ? L"Recovery code: " : L"Management password: ");

    std::wcout << L"Target: PMF-" << label_w << L"\n";
    std::wcout << L"Operation: stop and unregister Windows Service, then delete managed files.\n";
    const bool confirmed = ask(L"Type REMOVE to confirm: ") == L"REMOVE";

    if (!confirmed) {
        erase(secret);
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    std::string error;
    const bool success = pmf::uninstall_service(label, secret, recovery, error);
    erase(secret);

    if (!success) {
        std::cerr << "Removal failed: " << error << "\n";
        return 1;
    }

    std::wcout << L"Service removed: PMF-" << label_w << L"\n";
    std::wcout << L"The original EXE and recovery text file remain unchanged.\n";
    return 0;
}

void usage() {
    std::wcout << L"PMF 0.5.0 - Windows Service manager\n\n";
    std::wcout << L"Usage:\n";
    std::wcout << L"  pmf-unloader.exe list\n";
    std::wcout << L"  pmf-unloader.exe remove [label]\n";
    std::wcout << L"  pmf-unloader.exe help\n";
}

}

int wmain(int argc, wchar_t** argv) {
    if (argc == 1) {
        usage();
        return remove(argc, argv);
    }

    const std::wstring command(argv[1]);

    if (command == L"help" || command == L"--help" || command == L"-h") {
        usage();
        return 0;
    }

    if (command == L"list" && argc == 2) return list();
    if (command == L"remove" && argc <= 3) return remove(argc, argv);

    usage();
    return 2;
}

#else

#include <iostream>

int main() {
    std::cout << "PMF Windows Service manager is available on Windows only.\n";
    return 1;
}

#endif
