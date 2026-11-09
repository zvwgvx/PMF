#include "pmf/service.hpp"
#include "pmf/managed.hpp"
#include "pmf/workspace.hpp"

#ifdef _WIN32

#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>
#include <string>
#include <system_error>

namespace pmf {
namespace {

class ScHandle {
public:
    explicit ScHandle(SC_HANDLE handle = nullptr) : handle_(handle) {}
    ~ScHandle() { if (handle_) CloseServiceHandle(handle_); }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    SC_HANDLE get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
private:
    SC_HANDLE handle_;
};

std::wstring service_name(const std::string& label) {
    return L"PMF-" + std::wstring(label.begin(), label.end());
}

std::string win32_error(const std::string& text, DWORD code = GetLastError()) {
    return text + " (Win32 error " + std::to_string(code) + ")";
}

std::optional<std::filesystem::path> service_root() {
    PWSTR raw = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &raw);
    if (FAILED(hr) || !raw) {
        if (raw) CoTaskMemFree(raw);
        return std::nullopt;
    }
    std::filesystem::path result(raw);
    CoTaskMemFree(raw);
    return result / L"PMF" / L"Services";
}

bool trusted_directory(const std::filesystem::path& directory) {
    DWORD attributes = GetFileAttributesW(directory.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool parent_chain_trusted(const std::filesystem::path& directory) {
    std::filesystem::path current = directory;
    while (!current.empty()) {
        const DWORD attributes = GetFileAttributesW(current.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return false;
        }
        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return true;
}

bool wait_for_status(SC_HANDLE service, DWORD wanted, DWORD timeout_ms, std::string& error) {
    const ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < timeout_ms) {
        SERVICE_STATUS_PROCESS status{};
        DWORD bytes = 0;
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                  reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytes)) {
            error = win32_error("Cannot query Windows Service state");
            return false;
        }
        if (status.dwCurrentState == wanted) return true;
        if (wanted == SERVICE_RUNNING && status.dwCurrentState == SERVICE_STOPPED) {
            error = "Service stopped during startup (exit " +
                    std::to_string(status.dwWin32ExitCode) + ")";
            return false;
        }
        if (wanted == SERVICE_STOPPED && status.dwCurrentState == SERVICE_STOPPED) return true;
        Sleep(250);
    }
    error = "Timed out waiting for the service to change state.";
    return false;
}

bool stop_service(SC_HANDLE service, std::string& error) {
    SERVICE_STATUS_PROCESS status{};
    DWORD bytes = 0;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                              reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytes)) {
        error = win32_error("Cannot read service state");
        return false;
    }
    if (status.dwCurrentState == SERVICE_STOPPED) return true;
    if (status.dwCurrentState != SERVICE_STOP_PENDING) {
        SERVICE_STATUS response{};
        if (!ControlService(service, SERVICE_CONTROL_STOP, &response)) {
            error = win32_error("Cannot request the service to stop");
            return false;
        }
    }
    return wait_for_status(service, SERVICE_STOPPED, 15000, error);
}

void erase_installer_artifacts(const std::filesystem::path& folder) {
    std::error_code ec;
    std::filesystem::remove(folder / L"service.exe", ec);
    ec.clear();
    std::filesystem::remove(folder, ec);
}

}

std::vector<std::string> list_services(std::string& error) {
    std::vector<std::string> result;
    const auto root = service_root();
    if (!root || !parent_chain_trusted(*root)) {
        error = "Windows Program Files directory is unavailable.";
        return result;
    }

    const auto packages = list_managed(error);
    if (!error.empty()) return result;

    for (const auto& label : packages) {
        const auto directory = *root / label;
        std::error_code ec;
        if (trusted_directory(directory) &&
            std::filesystem::is_regular_file(directory / L"service.exe", ec) && !ec) {
            result.push_back(label);
        }
    }

    return result;
}

bool elevated_administrator() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD bytes = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation,
                                        sizeof(elevation), &bytes) != 0;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

bool inspect_service_executable(const std::filesystem::path& file, std::string& error) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        error = "Cannot open service executable.";
        return false;
    }

    IMAGE_DOS_HEADER dos{};
    input.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!input || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < sizeof(dos)) {
        error = "The selected file is not a valid Windows PE executable.";
        return false;
    }
    input.seekg(static_cast<std::streamoff>(dos.e_lfanew));
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    WORD optional_magic = 0;
    input.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    input.read(reinterpret_cast<char*>(&optional_magic), sizeof(optional_magic));

    if (!input || signature != IMAGE_NT_SIGNATURE ||
        (header.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0 ||
        (header.Characteristics & IMAGE_FILE_DLL) != 0 ||
        (optional_magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
         optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)) {
        error = "Expected a native Windows PE executable, not a DLL or unsupported file.";
        return false;
    }

    if (header.Machine != IMAGE_FILE_MACHINE_AMD64 && header.Machine != IMAGE_FILE_MACHINE_I386) {
        error = "Only Windows x86 and x64 service executables are supported.";
        return false;
    }

    return true;
}

bool install_service(const InstallationIdentity& identity, const std::wstring& password,
                     const std::filesystem::path& recovery_path, std::string& error) {
    if (!elevated_administrator()) {
        error = "Administrator elevation is required to register a Windows Service.";
        return false;
    }
    if (!valid_label(identity.label) || !inspect_service_executable(identity.executable, error)) {
        if (error.empty()) error = "Invalid installation label.";
        return false;
    }

    const auto root = service_root();
    if (!root || !parent_chain_trusted(*root)) {
        error = "Windows Program Files directory is unavailable or contains a reparse point.";
        return false;
    }

    const auto directory = *root / identity.label;
    std::error_code ec;
    if (std::filesystem::exists(directory, ec) || ec) {
        error = "The service directory already exists.";
        return false;
    }

    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!manager) {
        error = win32_error("Cannot open Windows Service Control Manager");
        return false;
    }

    const auto name = service_name(identity.label);
    ScHandle existing(OpenServiceW(manager.get(), name.c_str(), SERVICE_QUERY_STATUS));
    if (existing) {
        error = "A service with this PMF label already exists.";
        return false;
    }
    if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) {
        error = win32_error("Cannot confirm the service name is available");
        return false;
    }

    std::filesystem::create_directories(directory, ec);
    if (ec || !trusted_directory(directory)) {
        error = "Cannot create an ordinary Program Files service directory.";
        return false;
    }

    const auto installed_binary = directory / L"service.exe";
    std::filesystem::copy_file(identity.executable, installed_binary,
                               std::filesystem::copy_options::none, ec);
    if (ec) {
        erase_installer_artifacts(directory);
        error = "Cannot copy the Windows service executable: " + ec.message();
        return false;
    }

    const auto workspace = prepare_workspace(ec);
    if (!workspace) {
        erase_installer_artifacts(directory);
        error = "Cannot prepare PMF metadata directory.";
        return false;
    }

    if (!install_managed(identity, password, recovery_path, error, false)) {
        erase_installer_artifacts(directory);
        return false;
    }

    const auto path_argument = L"\"" + installed_binary.wstring() + L"\"";
    const auto display = L"PMF - " + std::wstring(identity.label.begin(), identity.label.end());

    ScHandle service(CreateServiceW(manager.get(), name.c_str(), display.c_str(),
                                    DELETE | SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS |
                                    SERVICE_CHANGE_CONFIG,
                                    SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                                    SERVICE_ERROR_NORMAL, path_argument.c_str(),
                                    nullptr, nullptr, nullptr,
                                    L"NT AUTHORITY\\LocalService", nullptr));
    if (!service) {
        error = win32_error("Cannot register service under LocalService");
        std::string ignored;
        uninstall_managed(identity.label, password, false, ignored);
        erase_installer_artifacts(directory);
        ec.clear();
        std::filesystem::remove(recovery_path, ec);
        return false;
    }

    SERVICE_DESCRIPTIONW description{};
    description.lpDescription = const_cast<LPWSTR>(
        L"User-authorized PMF-managed Windows service, running as LocalService.");
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &description);

    bool started = StartServiceW(service.get(), 0, nullptr) != 0;
    if (!started) {
        error = win32_error("Windows rejected service startup");
    } else {
        started = wait_for_status(service.get(), SERVICE_RUNNING, 15000, error);
    }

    if (!started) {
        std::string ignored;
        stop_service(service.get(), ignored);
        DeleteService(service.get());
        uninstall_managed(identity.label, password, false, ignored);
        erase_installer_artifacts(directory);
        ec.clear();
        std::filesystem::remove(recovery_path, ec);
        return false;
    }

    return true;
}

bool uninstall_service(const std::string& label, const std::wstring& secret,
                       bool recovery_mode, std::string& error) {
    if (!elevated_administrator()) {
        error = "Administrator elevation is required to remove a Windows Service.";
        return false;
    }

    if (!verify_managed_secret(label, secret, recovery_mode, error)) {
        return false;
    }

    const auto root = service_root();
    if (!root || !parent_chain_trusted(*root)) {
        error = "Program Files path is invalid.";
        return false;
    }

    const auto directory = *root / label;
    if (!trusted_directory(directory)) {
        error = "Service directory is absent or unsafe to remove.";
        return false;
    }

    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!manager) {
        error = win32_error("Cannot open Windows Service Control Manager");
        return false;
    }

    const auto name = service_name(label);
    ScHandle service(OpenServiceW(manager.get(), name.c_str(),
                                 SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE));
    if (service) {
        if (!stop_service(service.get(), error)) return false;
        if (!DeleteService(service.get()) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE) {
            error = win32_error("Cannot unregister the Windows service");
            return false;
        }
    } else if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST &&
               GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE) {
        error = win32_error("Cannot access the managed Windows service");
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(directory / L"service.exe", ec);
    if (ec) {
        error = "Service binary is not yet removable: " + ec.message();
        return false;
    }
    std::filesystem::remove(directory, ec);
    if (ec) {
        error = "Could not remove the service directory: " + ec.message();
        return false;
    }

    return uninstall_managed(label, secret, recovery_mode, error);
}

}

#else

namespace pmf {

bool elevated_administrator() { return false; }

std::vector<std::string> list_services(std::string& error) {
    error = "Windows Services are supported on Windows only.";
    return {};
}

bool inspect_service_executable(const std::filesystem::path&, std::string& error) {
    error = "Windows Service PE validation is supported on Windows only.";
    return false;
}

bool install_service(const InstallationIdentity&, const std::wstring&,
                     const std::filesystem::path&, std::string& error) {
    error = "Windows Services are supported on Windows only.";
    return false;
}

bool uninstall_service(const std::string&, const std::wstring&, bool, std::string& error) {
    error = "Windows Services are supported on Windows only.";
    return false;
}

}

#endif
