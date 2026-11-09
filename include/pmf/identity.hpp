#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace pmf {

struct InstallationIdentity {
    std::string label;
    std::filesystem::path executable;
};

bool valid_label(std::string_view label);
std::optional<InstallationIdentity> inspect_target(const std::filesystem::path& executable, std::string label);

}
