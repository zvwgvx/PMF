#include "pmf/identity.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

int main() {
    if (!pmf::valid_label("sample-app_01")) {
        return 1;
    }

    if (pmf::valid_label("") || pmf::valid_label("../sample") || pmf::valid_label("bad label")) {
        return 2;
    }

    const auto dir = std::filesystem::temp_directory_path();
    const auto file = dir / "pmf-identity-test.exe";

    {
        std::ofstream output(file, std::ios::binary);
        output << "sample";
    }

    const auto identity = pmf::inspect_target(file, "sample");
    std::error_code ec;
    std::filesystem::remove(file, ec);

    if (!identity || identity->label != "sample" || identity->executable.filename() != file.filename()) {
        return 3;
    }

    std::cout << "PMF identity tests passed\n";
    return 0;
}
