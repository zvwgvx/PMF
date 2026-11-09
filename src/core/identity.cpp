#include "pmf/identity.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace pmf {

bool valid_label(std::string_view label) {
    if (label.empty() || label.size() > 64) {
        return false;
    }

    for (unsigned char c : label) {
        bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        bool digit = c >= '0' && c <= '9';

        if (!alpha && !digit && c != '-' && c != '_') {
            return false;
        }
    }

    return true;
}

std::optional<InstallationIdentity> inspect_target(const std::filesystem::path& executable, std::string label) {
    if (!valid_label(label)) {
        return std::nullopt;
    }

    std::error_code ec;
    auto resolved = std::filesystem::absolute(executable, ec);

    if (ec || !std::filesystem::is_regular_file(resolved, ec) || ec) {
        return std::nullopt;
    }

    std::string extension = resolved.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (extension != ".exe") {
        return std::nullopt;
    }

    auto canonical = std::filesystem::weakly_canonical(resolved, ec);

    if (!ec) {
        resolved = std::move(canonical);
    }

    return InstallationIdentity{std::move(label), std::move(resolved)};
}

}
