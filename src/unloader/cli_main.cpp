#include "pmf/console_tui.hpp"
#include "pmf/identity.hpp"
#include "pmf/service.hpp"
#include "pmf/windows_ui.hpp"

#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string ascii(const std::wstring& value) {
    std::string result;
    for (const auto character : value) {
        if (character > 127 || character < 0) return {};
        result.push_back(static_cast<char>(character));
    }
    return result;
}

void erase(std::wstring& value) {
    if (!value.empty()) {
        SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
        value.clear();
    }
}

void usage() {
    pmf::tui::header(L"PMF UNLOADER", L"Windows Service management");
    std::wcout << L"  pmf-unloader-cli.exe\n";
    std::wcout << L"  pmf-unloader-cli.exe list\n";
    std::wcout << L"  pmf-unloader-cli.exe remove [label]\n";
    std::wcout << L"  pmf-unloader-cli.exe help\n\n";
    std::wcout << L"  Launch without arguments to choose an installed service.\n";
}

int list() {
    std::string error;
    const auto services = pmf::list_services(error);
    if (!error.empty()) {
        std::cerr << "Cannot list services: " << error << "\n";
        return 1;
    }

    pmf::tui::header(L"PMF UNLOADER", L"Installed PMF-managed Windows Services");
    if (services.empty()) {
        std::wcout << L"No PMF-managed services were found.\n";
        return 0;
    }

    for (size_t i = 0; i < services.size(); ++i) {
        std::wcout << L"  " << i + 1 << L". PMF-"
                   << std::wstring(services[i].begin(), services[i].end()) << L"\n";
    }

    return 0;
}

int remove_selected(const std::string& label) {
    bool recovery = false;

    for (;;) {
        pmf::tui::clear();
        pmf::tui::header(L"PMF UNLOADER", L"Authenticate and remove selected service");
        std::wcout << L"Selected: PMF-" << std::wstring(label.begin(), label.end()) << L"\n";
        std::wcout << L"Only PMF-managed files will be removed.\n";
        std::wcout << L"The original executable and recovery file will be retained.\n\n";

        auto secret = pmf::read_secret(
            recovery ? L"Recovery code: " : L"Management password (Enter if none): ");

        std::wcout << L"\nThe selected service will be stopped and unregistered.\n";
        const bool confirmed = pmf::tui::ask(L"Type REMOVE to confirm") == L"REMOVE";
        if (!confirmed) {
            erase(secret);
            std::wcout << L"Cancelled.\n";
            return 0;
        }

        std::wcout << L"\nRemoving...\n";
        std::string error;
        const bool success = pmf::uninstall_service(label, secret, recovery, error);
        erase(secret);

        if (success) {
            std::wcout << L"\nRemoved: PMF-"
                       << std::wstring(label.begin(), label.end()) << L"\n";
            return 0;
        }

        std::cerr << "Removal failed: " << error << "\n";

        const int choice = pmf::tui::choose(
            L"PMF UNLOADER", L"Removal was not completed",
            {L"Try management password again", L"Use recovery code", L"Cancel"});

        if (choice == 0) recovery = false;
        else if (choice == 1) recovery = true;
        else return 1;
    }
}

int interactive_remove() {
    std::string error;
    const auto services = pmf::list_services(error);
    if (!error.empty()) {
        std::cerr << "Cannot list services: " << error << "\n";
        return 1;
    }

    if (services.empty()) {
        pmf::tui::clear();
        pmf::tui::header(L"PMF UNLOADER", L"Installed PMF-managed Windows Services");
        std::wcout << L"No PMF-managed services were found.\n";
        return 0;
    }

    std::vector<std::wstring> choices;
    choices.reserve(services.size());
    for (const auto& service : services) {
        choices.push_back(L"PMF-" + std::wstring(service.begin(), service.end()));
    }

    const int selection = pmf::tui::choose(
        L"PMF UNLOADER", L"Choose a service to uninstall", choices);
    if (selection < 0) {
        std::wcout << L"Cancelled.\n";
        return 0;
    }

    return remove_selected(services[static_cast<size_t>(selection)]);
}

int remove_command(int argc, wchar_t** argv) {
    if (argc == 2) return interactive_remove();

    const auto label = ascii(std::wstring(argv[2]));
    if (!pmf::valid_label(label)) {
        std::wcerr << L"Invalid service label.\n";
        return 1;
    }

    return remove_selected(label);
}

}

int wmain(int argc, wchar_t** argv) {
    if (argc == 1) {
        const int result = interactive_remove();
        pmf::tui::pause();
        return result;
    }

    const std::wstring command(argv[1]);
    if (command == L"help" || command == L"--help" || command == L"-h") {
        usage();
        return 0;
    }

    if (command == L"list" && argc == 2) return list();
    if (command == L"remove" && argc <= 3) return remove_command(argc, argv);

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
