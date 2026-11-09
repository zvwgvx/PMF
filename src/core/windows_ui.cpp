#include "pmf/windows_ui.hpp"

#ifdef _WIN32

#include <windows.h>
#include <shobjidl.h>
#include <iostream>
#include <string>

namespace pmf {

std::optional<std::filesystem::path> browse_executable() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(init)) {
        return std::nullopt;
    }

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));

    std::optional<std::filesystem::path> result;

    if (SUCCEEDED(hr)) {
        const COMDLG_FILTERSPEC types[] = {{L"Windows Executable", L"*.exe"}};
        dialog->SetFileTypes(1, types);
        dialog->SetFileTypeIndex(1);
        dialog->SetTitle(L"Select the executable to manage");

        hr = dialog->Show(nullptr);

        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            hr = dialog->GetResult(&item);

            if (SUCCEEDED(hr)) {
                PWSTR raw = nullptr;
                hr = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);

                if (SUCCEEDED(hr) && raw != nullptr) {
                    result = std::filesystem::path(raw);
                    CoTaskMemFree(raw);
                }

                item->Release();
            }
        }

        dialog->Release();
    }

    CoUninitialize();
    return result;
}

std::optional<std::filesystem::path> browse_recovery_destination(const std::string& label) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(init)) {
        return std::nullopt;
    }

    IFileSaveDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));

    std::optional<std::filesystem::path> result;

    if (SUCCEEDED(hr)) {
        const COMDLG_FILTERSPEC types[] = {{L"Text File", L"*.txt"}};
        dialog->SetFileTypes(1, types);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"txt");
        dialog->SetTitle(L"Save PMF recovery information");
        const std::wstring filename(label.begin(), label.end());
        const std::wstring suggestion = filename + L"-pmf-recovery.txt";
        dialog->SetFileName(suggestion.c_str());
        dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);

        hr = dialog->Show(nullptr);

        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            hr = dialog->GetResult(&item);

            if (SUCCEEDED(hr)) {
                PWSTR raw = nullptr;
                hr = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);

                if (SUCCEEDED(hr) && raw != nullptr) {
                    result = std::filesystem::path(raw);
                    CoTaskMemFree(raw);
                }

                item->Release();
            }
        }

        dialog->Release();
    }

    CoUninitialize();
    return result;
}

std::wstring read_secret(const std::wstring& prompt) {
    std::wcout << prompt << std::flush;
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_mode = 0;
    const bool have_console = handle != INVALID_HANDLE_VALUE &&
                              GetConsoleMode(handle, &old_mode) != 0;

    if (have_console) {
        SetConsoleMode(handle, old_mode & ~ENABLE_ECHO_INPUT);
    }

    std::wstring result;
    std::getline(std::wcin, result);

    if (have_console) {
        SetConsoleMode(handle, old_mode);
    }

    std::wcout << L"\n";
    return result;
}

}

#else

namespace pmf {

std::optional<std::filesystem::path> browse_executable() {
    return std::nullopt;
}

std::optional<std::filesystem::path> browse_recovery_destination(const std::string&) {
    return std::nullopt;
}

std::wstring read_secret(const std::wstring&) {
    return {};
}

}

#endif
