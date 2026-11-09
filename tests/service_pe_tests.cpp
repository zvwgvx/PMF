#include "pmf/service.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 1;

    std::string error;
    if (!pmf::inspect_service_executable(std::filesystem::path(argv[1]), error)) {
        std::cerr << error << '\n';
        return 2;
    }

    auto invalid = std::filesystem::temp_directory_path() / "pmf-invalid-service.exe";
    {
        std::ofstream out(invalid, std::ios::binary);
        out << "not-a-valid-exe";
    }

    error.clear();
    const bool accepted = pmf::inspect_service_executable(invalid, error);
    std::error_code ec;
    std::filesystem::remove(invalid, ec);
    return accepted ? 3 : 0;
}
