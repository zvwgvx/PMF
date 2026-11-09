#include "pmf/workspace.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace pmf {

std::optional<std::filesystem::path> workspace_directory() {
#ifdef _WIN32
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &raw);

    if (FAILED(hr) || raw == nullptr) {
        if (raw != nullptr) {
            CoTaskMemFree(raw);
        }

        return std::nullopt;
    }

    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    return path / L"PMF";
#else
    return std::nullopt;
#endif
}

std::optional<WorkspaceResult> prepare_workspace(std::error_code& ec) {
    ec.clear();
    const auto path = workspace_directory();

    if (!path) {
        ec = std::make_error_code(std::errc::function_not_supported);
        return std::nullopt;
    }

    const bool existed = std::filesystem::exists(*path, ec);

    if (ec) {
        return std::nullopt;
    }

    if (existed && !std::filesystem::is_directory(*path, ec)) {
        if (!ec) {
            ec = std::make_error_code(std::errc::not_a_directory);
        }

        return std::nullopt;
    }

    if (ec) {
        return std::nullopt;
    }

    if (!existed) {
        std::filesystem::create_directories(*path, ec);

        if (ec) {
            return std::nullopt;
        }
    }

    return WorkspaceResult{*path, existed};
}

}
