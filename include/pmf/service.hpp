#pragma once

#include "pmf/identity.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace pmf {

bool elevated_administrator();
std::vector<std::string> list_services(std::string& error);
bool inspect_service_executable(const std::filesystem::path& file, std::string& error);
bool install_service(const InstallationIdentity& identity, const std::wstring& password,
                     const std::filesystem::path& recovery_path, std::string& error);
bool uninstall_service(const std::string& label, const std::wstring& secret,
                       bool recovery_mode, std::string& error);

}
