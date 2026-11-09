#pragma once

#include "pmf/identity.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace pmf {

bool install_managed(const InstallationIdentity& identity, const std::wstring& password,
                     const std::filesystem::path& recovery_path, std::string& error);
bool uninstall_managed(const std::string& label, const std::wstring& secret,
                       bool recovery_mode, std::string& error);
std::vector<std::string> list_managed(std::string& error);

}
