#pragma once

#include <filesystem>
#include <optional>
#include <system_error>

namespace pmf {

struct WorkspaceResult {
    std::filesystem::path path;
    bool already_existed;
};

std::optional<std::filesystem::path> workspace_directory();
std::optional<WorkspaceResult> prepare_workspace(std::error_code& ec);

}
