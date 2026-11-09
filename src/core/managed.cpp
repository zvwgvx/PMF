#include "pmf/managed.hpp"
#include "pmf/workspace.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32

#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <iomanip>

namespace pmf {
namespace {

constexpr ULONG key_size = 32;
constexpr std::uint64_t iterations = 600000;

bool random_bytes(std::vector<std::uint8_t>& bytes) {
    return BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

std::string hex_encode(const std::vector<std::uint8_t>& input) {
    static constexpr char alphabet[] = "0123456789abcdef";
    std::string text;
    text.reserve(input.size() * 2);

    for (const auto byte : input) {
        text.push_back(alphabet[byte >> 4]);
        text.push_back(alphabet[byte & 15]);
    }

    return text;
}

std::vector<std::uint8_t> hex_decode(const std::string& text) {
    if (text.empty() || text.size() % 2 != 0) {
        return {};
    }

    std::vector<std::uint8_t> result;
    result.reserve(text.size() / 2);

    for (std::size_t i = 0; i < text.size(); i += 2) {
        auto value = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };

        const int a = value(text[i]);
        const int b = value(text[i + 1]);

        if (a < 0 || b < 0) {
            return {};
        }

        result.push_back(static_cast<std::uint8_t>((a << 4) | b));
    }

    return result;
}

bool derive_secret(const std::wstring& secret, const std::vector<std::uint8_t>& salt,
                   std::vector<std::uint8_t>& output) {
    if (secret.empty() || salt.empty()) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                                   nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);

    if (status != 0) {
        return false;
    }

    output.assign(key_size, 0);
    status = BCryptDeriveKeyPBKDF2(algorithm,
                                  reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(secret.data())),
                                  static_cast<ULONG>(secret.size() * sizeof(wchar_t)),
                                  const_cast<PUCHAR>(salt.data()),
                                  static_cast<ULONG>(salt.size()),
                                  iterations, output.data(), static_cast<ULONG>(output.size()), 0);

    BCryptCloseAlgorithmProvider(algorithm, 0);
    return status == 0;
}

bool check_secret(const std::wstring& secret, const std::string& salt_hex,
                  const std::string& hash_hex) {
    const auto salt = hex_decode(salt_hex);
    const auto expected = hex_decode(hash_hex);

    if (salt.size() != 16 || expected.size() != key_size) {
        return false;
    }

    std::vector<std::uint8_t> derived;

    if (!derive_secret(secret, salt, derived)) {
        return false;
    }

    std::uint8_t difference = 0;

    for (std::size_t i = 0; i < expected.size(); ++i) {
        difference |= static_cast<std::uint8_t>(expected[i] ^ derived[i]);
    }

    SecureZeroMemory(derived.data(), derived.size());
    return difference == 0;
}

std::string make_recovery_code(const std::vector<std::uint8_t>& bytes) {
    static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string text;
    text.reserve(39);

    for (std::size_t i = 0; i < bytes.size(); i += 5) {
        std::uint64_t value = 0;

        for (std::size_t j = 0; j < 5; ++j) {
            value = (value << 8) | bytes[i + j];
        }

        for (int k = 7; k >= 0; --k) {
            text.push_back(alphabet[(value >> (k * 5)) & 31]);
        }

        if (i + 5 < bytes.size()) {
            text.push_back('-');
        }
    }

    return text;
}

bool file_sha256(const std::filesystem::path& file, std::string& result) {
    std::ifstream stream(file, std::ios::binary);

    if (!stream) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                                   nullptr, 0);

    if (status != 0) {
        return false;
    }

    status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);

    if (status == 0) {
        std::array<char, 65536> buffer{};

        while (stream) {
            stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = stream.gcount();

            if (count > 0) {
                status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                        static_cast<ULONG>(count), 0);

                if (status != 0) {
                    break;
                }
            }
        }

        if (stream.bad()) {
            status = -1;
        }

        if (status == 0) {
            std::vector<std::uint8_t> digest(key_size);
            status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);

            if (status == 0) {
                result = hex_encode(digest);
            }
        }

        BCryptDestroyHash(hash);
    }

    BCryptCloseAlgorithmProvider(algorithm, 0);
    return status == 0;
}

bool write_manifest(const std::filesystem::path& path, const std::map<std::string, std::string>& fields) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);

    if (!output) {
        return false;
    }

    for (const auto& [key, value] : fields) {
        output << key << '=' << value << '\n';
    }

    output.flush();
    return output.good();
}

bool read_manifest(const std::filesystem::path& path, std::map<std::string, std::string>& fields) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        return false;
    }

    std::string line;

    while (std::getline(input, line)) {
        const auto position = line.find('=');

        if (position == std::string::npos || position == 0) {
            return false;
        }

        fields[line.substr(0, position)] = line.substr(position + 1);
    }

    return fields["version"] == "1";
}

std::optional<std::filesystem::path> managed_root() {
    const auto workspace = workspace_directory();

    if (!workspace) {
        return std::nullopt;
    }

    return *workspace / L"managed";
}

bool is_reparse_point(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

}

bool install_managed(const InstallationIdentity& identity, const std::wstring& password,
                     const std::filesystem::path& recovery_path, std::string& error) {
    if (!valid_label(identity.label) || password.size() < 12) {
        error = "Invalid label or password shorter than 12 characters.";
        return false;
    }

    const auto root = managed_root();

    if (!root) {
        error = "Windows workspace is unavailable.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(*root, ec);

    if (ec || is_reparse_point(*root)) {
        error = "Cannot create or trust the managed directory.";
        return false;
    }

    const auto destination = *root / identity.label;

    if (std::filesystem::exists(destination, ec) || ec) {
        error = "The installation label already exists.";
        return false;
    }

    if (!std::filesystem::create_directory(destination, ec) || ec) {
        error = "Cannot create the managed application directory.";
        return false;
    }

    auto rollback = [&]() {
        std::error_code ignored;
        std::filesystem::remove_all(destination, ignored);
    };

    const auto binary = destination / L"payload.exe";
    std::filesystem::copy_file(identity.executable, binary, std::filesystem::copy_options::none, ec);

    if (ec) {
        rollback();
        error = "Cannot copy the executable.";
        return false;
    }

    std::string original_hash;
    std::string copied_hash;

    if (!file_sha256(identity.executable, original_hash) || !file_sha256(binary, copied_hash) ||
        original_hash != copied_hash) {
        rollback();
        error = "Executable integrity verification failed.";
        return false;
    }

    std::vector<std::uint8_t> password_salt(16);
    std::vector<std::uint8_t> recovery_salt(16);
    std::vector<std::uint8_t> recovery_seed(20);

    if (!random_bytes(password_salt) || !random_bytes(recovery_salt) || !random_bytes(recovery_seed)) {
        rollback();
        error = "Secure random generation failed.";
        return false;
    }

    const auto recovery = make_recovery_code(recovery_seed);
    const std::wstring recovery_wide(recovery.begin(), recovery.end());
    std::vector<std::uint8_t> password_hash;
    std::vector<std::uint8_t> recovery_hash;

    if (!derive_secret(password, password_salt, password_hash) ||
        !derive_secret(recovery_wide, recovery_salt, recovery_hash)) {
        rollback();
        error = "Could not derive password verifiers.";
        return false;
    }

    const std::map<std::string, std::string> fields{
        {"version", "1"},
        {"label", identity.label},
        {"binary", "payload.exe"},
        {"sha256", copied_hash},
        {"password_salt", hex_encode(password_salt)},
        {"password_hash", hex_encode(password_hash)},
        {"recovery_salt", hex_encode(recovery_salt)},
        {"recovery_hash", hex_encode(recovery_hash)}
    };

    SecureZeroMemory(password_hash.data(), password_hash.size());
    SecureZeroMemory(recovery_hash.data(), recovery_hash.size());

    if (!write_manifest(destination / L"manifest.txt", fields)) {
        rollback();
        error = "Cannot write the installation manifest.";
        return false;
    }

    const auto workspace = workspace_directory();
    std::error_code path_ec;
    const auto relative_recovery = std::filesystem::relative(recovery_path, *workspace, path_ec);

    if (!path_ec && !relative_recovery.empty() && !relative_recovery.is_absolute() &&
        *relative_recovery.begin() != std::filesystem::path(L"..")) {
        rollback();
        error = "Recovery file must be stored outside the PMF workspace.";
        return false;
    }

    ec.clear();

    if (std::filesystem::exists(recovery_path, ec) || ec) {
        rollback();
        error = "Recovery file already exists; select a new file path.";
        return false;
    }

    std::ofstream recovery_file(recovery_path, std::ios::binary);

    if (!recovery_file) {
        rollback();
        error = "Cannot create the recovery file.";
        return false;
    }

    recovery_file << "PMF recovery information\n";
    recovery_file << "Label: " << identity.label << "\n";
    recovery_file << "Recovery code: " << recovery << "\n";
    recovery_file << "Keep this file secure and separate from the PMF workspace.\n";
    recovery_file.flush();

    if (!recovery_file.good()) {
        recovery_file.close();
        std::filesystem::remove(recovery_path, ec);
        rollback();
        error = "Could not finish writing the recovery file.";
        return false;
    }

    recovery_file.close();
    return true;
}

bool uninstall_managed(const std::string& label, const std::wstring& secret,
                       bool recovery_mode, std::string& error) {
    if (!valid_label(label)) {
        error = "Invalid installation label.";
        return false;
    }

    const auto root = managed_root();

    if (!root) {
        error = "Windows workspace is unavailable.";
        return false;
    }

    const auto destination = *root / label;

    if (is_reparse_point(*root) || is_reparse_point(destination)) {
        error = "Cannot access a reparse point or missing installation.";
        return false;
    }

    std::map<std::string, std::string> fields;

    if (!read_manifest(destination / L"manifest.txt", fields) || fields["label"] != label) {
        error = "Installation manifest is missing or invalid.";
        return false;
    }

    if (!check_secret(secret, fields[recovery_mode ? "recovery_salt" : "password_salt"],
                      fields[recovery_mode ? "recovery_hash" : "password_hash"])) {
        error = "Invalid password or recovery code.";
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(destination, ec);

    if (ec) {
        error = "Unable to remove managed files: " + ec.message();
        return false;
    }

    return true;
}

std::vector<std::string> list_managed(std::string& error) {
    std::vector<std::string> labels;
    const auto root = managed_root();

    if (!root) {
        error = "Windows workspace is unavailable.";
        return labels;
    }

    std::error_code ec;

    if (!std::filesystem::exists(*root, ec)) {
        return labels;
    }

    if (ec || is_reparse_point(*root)) {
        error = "Cannot inspect the managed directory.";
        return labels;
    }

    for (const auto& entry : std::filesystem::directory_iterator(*root, ec)) {
        if (ec) {
            break;
        }

        const auto label = entry.path().filename().string();

        if (valid_label(label) && entry.is_directory(ec) && !ec &&
            !is_reparse_point(entry.path())) {
            labels.push_back(label);
        }
    }

    if (ec) {
        error = "Error while listing managed applications.";
    }

    return labels;
}

}

#else

namespace pmf {

bool install_managed(const InstallationIdentity&, const std::wstring&,
                     const std::filesystem::path&, std::string& error) {
    error = "PMF managed installation is supported on Windows only.";
    return false;
}

bool uninstall_managed(const std::string&, const std::wstring&, bool, std::string& error) {
    error = "PMF managed removal is supported on Windows only.";
    return false;
}

std::vector<std::string> list_managed(std::string& error) {
    error = "PMF managed applications are supported on Windows only.";
    return {};
}

}

#endif
