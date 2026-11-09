#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace pmf {

std::optional<std::filesystem::path> browse_executable();
std::optional<std::filesystem::path> browse_recovery_destination(const std::string& label);
std::wstring read_secret(const std::wstring& prompt);

}
