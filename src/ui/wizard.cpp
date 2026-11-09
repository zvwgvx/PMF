#include "pmf/identity.hpp"
#include "pmf/managed.hpp"
#include "pmf/windows_ui.hpp"
#include "pmf/workspace.hpp"

#ifdef _WIN32

#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace pmf::ui {

namespace {

constexpr int back_id = 110;
constexpr int next_id = 111;
constexpr int cancel_id = 112;
constexpr int browse_id = 113;
constexpr int save_id = 114;
constexpr int list_id = 115;
constexpr int password_id = 116;
constexpr int recovery_id = 117;

std::wstring value_of(HWND control) {
    if (!control) return {};
    int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length + 1), L'\0');
    int count = GetWindowTextW(control, result.data(), length + 1);
    result.resize(static_cast<std::size_t>(count));
    return result;
}

std::string ascii(const std::wstring& value) {
    std::string result;
    for (wchar_t c : value) {
        if (c > 127 || c < 0) return {};
        result.push_back(static_cast<char>(c));
    }
    return result;
}

void scrub(std::wstring& data) {
    if (!data.empty()) {
        SecureZeroMemory(data.data(), data.size() * sizeof(wchar_t));
        data.clear();
    }
}

}

class Window {
public:
    explicit Window(bool loader) : loader_(loader) {}

    int run(HINSTANCE instance, int show) {
        instance_ = instance;
        font_ = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FF_SWISS, L"Segoe UI");
        bold_ = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FF_SWISS, L"Segoe UI");
        large_ = CreateFontW(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FF_SWISS, L"Segoe UI");
        light_ = CreateSolidBrush(RGB(251, 252, 254));
        dark_ = CreateSolidBrush(RGB(27, 42, 65));

        const wchar_t* name = loader_ ? L"PMFLoaderUI" : L"PMFUnloaderUI";
        WNDCLASSW klass{};
        klass.lpfnWndProc = &Window::callback;
        klass.hInstance = instance;
        klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_NORMAL));
        klass.lpszClassName = name;

        if (!RegisterClassW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            cleanup();
            return 1;
        }

        int x = (GetSystemMetrics(SM_CXSCREEN) - 800) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - 550) / 2;
        window_ = CreateWindowExW(0, name,
                                  loader_ ? L"PMF - Setup Wizard" : L"PMF - Removal Wizard",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  x, y, 800, 550, nullptr, nullptr, instance, this);

        if (!window_) {
            cleanup();
            return 1;
        }

        ShowWindow(window_, show);
        UpdateWindow(window_);
        MSG message{};

        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        cleanup();
        return static_cast<int>(message.wParam);
    }

private:
    bool loader_;
    bool finished_{};
    bool recovery_mode_{};
    int page_{};
    HINSTANCE instance_{};
    HWND window_{};
    HWND file_edit_{};
    HWND label_edit_{};
    HWND pass_edit_{};
    HWND confirm_edit_{};
    HWND recovery_edit_{};
    HWND package_list_{};
    HWND radio_recovery_{};
    HWND agreement_{};
    HFONT font_{};
    HFONT bold_{};
    HFONT large_{};
    HBRUSH light_{};
    HBRUSH dark_{};
    std::vector<HWND> items_;
    std::filesystem::path file_;
    std::filesystem::path recovery_;
    std::wstring label_;
    std::wstring password_;
    std::wstring secret_;

    static LRESULT CALLBACK callback(HWND window, UINT message, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l);
            self = static_cast<Window*>(create->lpCreateParams);
            self->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->handle(message, w, l) : DefWindowProcW(window, message, w, l);
    }

    LRESULT handle(UINT message, WPARAM w, LPARAM l) {
        switch (message) {
        case WM_CREATE:
            render();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window_, &ps);
            RECT bounds{};
            GetClientRect(window_, &bounds);
            FillRect(dc, &bounds, light_);
            RECT side{0, 0, 195, bounds.bottom};
            FillRect(dc, &side, dark_);
            HBRUSH footer = CreateSolidBrush(RGB(242, 245, 250));
            RECT bottom{195, 437, bounds.right, bounds.bottom};
            FillRect(dc, &bottom, footer);
            DeleteObject(footer);
            EndPaint(window_, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HWND child = reinterpret_cast<HWND>(l);
            RECT rect{};
            GetWindowRect(child, &rect);
            MapWindowPoints(HWND_DESKTOP, window_, reinterpret_cast<POINT*>(&rect), 2);
            bool is_side = rect.left < 195;
            HDC dc = reinterpret_cast<HDC>(w);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, is_side ? RGB(231, 239, 251) : RGB(32, 45, 66));
            return reinterpret_cast<LRESULT>(is_side ? dark_ : light_);
        }
        case WM_COMMAND:
            if (HIWORD(w) == BN_CLICKED) {
                switch (LOWORD(w)) {
                case back_id:
                    back();
                    return 0;
                case next_id:
                    next();
                    return 0;
                case cancel_id:
                    DestroyWindow(window_);
                    return 0;
                case browse_id:
                    browse();
                    return 0;
                case save_id:
                    save();
                    return 0;
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            scrub(password_);
            scrub(secret_);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window_, message, w, l);
    }

    void cleanup() {
        scrub(password_);
        scrub(secret_);
        if (font_) DeleteObject(font_);
        if (bold_) DeleteObject(bold_);
        if (large_) DeleteObject(large_);
        if (light_) DeleteObject(light_);
        if (dark_) DeleteObject(dark_);
    }

    HWND add(const wchar_t* klass, const std::wstring& text, int x, int y,
             int width, int height, DWORD style = 0, int id = 0,
             HFONT font = nullptr, DWORD extended = 0) {
        HWND child = CreateWindowExW(extended, klass, text.c_str(),
                                     WS_CHILD | WS_VISIBLE | style,
                                     x, y, width, height, window_,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     instance_, nullptr);
        if (child) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : font_), TRUE);
            items_.push_back(child);
        }
        return child;
    }

    HWND text(const std::wstring& message, int x, int y, int width,
              int height = 28, bool strong = false) {
        return add(L"STATIC", message, x, y, width, height,
                   SS_LEFT, 0, strong ? bold_ : font_);
    }

    HWND edit(const std::wstring& message, int x, int y, int width,
              bool hidden = false, bool readonly = false) {
        DWORD style = WS_TABSTOP | ES_AUTOHSCROLL;
        if (hidden) style |= ES_PASSWORD;
        if (readonly) style |= ES_READONLY;
        return add(L"EDIT", message, x, y, width, 32, style, 0, font_, WS_EX_CLIENTEDGE);
    }

    HWND button(const std::wstring& caption, int id, int x, int y, int width) {
        return add(L"BUTTON", caption, x, y, width, 33,
                   BS_PUSHBUTTON | WS_TABSTOP, id);
    }

    void title(const std::wstring& heading, const std::wstring& description) {
        add(L"STATIC", heading, 236, 40, 525, 47, SS_LEFT, 0, large_);
        text(description, 236, 99, 525, 50);
    }

    void render() {
        for (HWND child : items_) DestroyWindow(child);
        items_.clear();
        file_edit_ = label_edit_ = pass_edit_ = confirm_edit_ = nullptr;
        recovery_edit_ = package_list_ = radio_recovery_ = agreement_ = nullptr;

        add(L"STATIC", L"PMF", 25, 43, 155, 48, SS_LEFT, 0, large_);
        text(L"Persistence\nManagement\nFramework", 25, 99, 158, 85);
        text(loader_ ? L"SETUP WIZARD" : L"REMOVAL WIZARD", 25, 233, 159, 26, true);

        const int count = loader_ ? 5 : 4;
        for (int i = 0; i < count; ++i) {
            const wchar_t* install_steps[] = {L"1   Welcome", L"2   Select program",
                                              L"3   Security", L"4   Review", L"5   Finish"};
            const wchar_t* remove_steps[] = {L"1   Select program",
                                             L"2   Authentication", L"3   Review", L"4   Finish"};
            text(loader_ ? install_steps[i] : remove_steps[i],
                 25, 270 + i * 34, 163, 28, i == page_);
        }

        if (loader_) loader_page();
        else unloader_page();

        HWND cancel = button(L"Cancel", cancel_id, 657, 458, 106);
        HWND back_button = button(L"< Back", back_id, 422, 458, 105);
        button(finished_ ? L"Close" :
               (loader_ && page_ == 3 ? L"Install" :
                !loader_ && page_ == 2 ? L"Remove" : L"Next >"),
               next_id, 539, 458, 106);

        if (page_ == 0 || finished_) EnableWindow(back_button, FALSE);
        if (finished_) ShowWindow(cancel, SW_HIDE);
        InvalidateRect(window_, nullptr, TRUE);
    }

    void loader_page() {
        if (page_ == 0) {
            title(L"Welcome to PMF", L"Windows application management setup");
            text(L"This wizard will create a managed copy of a Windows executable.",
                 236, 179, 525, 52);
            text(L"Each managed application can have a separate password\n"
                 L"and a recovery text file.", 236, 242, 525, 74);
            text(L"PMF does not execute the file, configure automatic startup,\n"
                 L"modify antivirus settings, or block administrator removal.",
                 236, 343, 525, 68);
        } else if (page_ == 1) {
            title(L"Select program", L"Choose the executable and installation label");
            text(L"Windows executable (.exe)", 236, 154, 525, 28, true);
            file_edit_ = edit(file_.wstring(), 236, 184, 408);
            button(L"Browse...", browse_id, 656, 184, 107);
            text(L"Installation label", 236, 259, 525, 28, true);
            label_edit_ = edit(label_, 236, 289, 527);
            text(L"The original executable will not be modified.", 236, 360, 527, 37);
        } else if (page_ == 2) {
            title(L"Security and recovery", L"Set unique credentials for this application");
            text(L"Management password (at least 12 characters)", 236, 144, 527, 28, true);
            pass_edit_ = edit(password_, 236, 175, 527, true);
            text(L"Confirm password", 236, 224, 527, 28, true);
            confirm_edit_ = edit(password_, 236, 253, 527, true);
            text(L"Recovery text file", 236, 310, 527, 28, true);
            recovery_edit_ = edit(recovery_.wstring(), 236, 339, 403, false, true);
            button(L"Save As...", save_id, 651, 338, 112);
            text(L"Save this file outside PMF and keep it private.", 236, 389, 527, 31);
        } else if (page_ == 3) {
            title(L"Ready to install", L"Review the managed-copy operation");
            text(L"Program:", 236, 161, 527, 29, true);
            text(file_.wstring(), 236, 196, 527, 53);
            text(L"Label: " + label_, 236, 258, 527, 29, true);
            text(L"Recovery file: " + recovery_.wstring(), 236, 301, 527, 54);
            agreement_ = add(L"BUTTON",
                             L"I authorize this managed-copy installation.",
                             236, 377, 527, 32, BS_AUTOCHECKBOX | WS_TABSTOP);
        } else {
            title(L"Installation complete", L"Managed executable registered");
            text(L"Label: " + label_, 236, 187, 527, 37, true);
            text(L"The managed copy is stored in C:\\ProgramData\\PMF\\managed.",
                 236, 259, 527, 58);
            text(L"Save the recovery text file securely. No automatic startup was configured.",
                 236, 360, 527, 54);
        }
    }

    void unloader_page() {
        if (page_ == 0) {
            title(L"Select application", L"Choose a managed executable to remove");
            std::string error;
            auto names = pmf::list_managed(error);
            package_list_ = add(L"LISTBOX", L"", 236, 167, 527, 204,
                                LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
                                list_id, font_, WS_EX_CLIENTEDGE);
            for (const auto& name : names) {
                std::wstring wide(name.begin(), name.end());
                SendMessageW(package_list_, LB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(wide.c_str()));
            }
            if (!label_.empty()) {
                SendMessageW(package_list_, LB_SELECTSTRING, static_cast<WPARAM>(-1),
                             reinterpret_cast<LPARAM>(label_.c_str()));
            }
            if (!error.empty()) {
                text(std::wstring(error.begin(), error.end()), 236, 388, 527, 39);
            } else {
                text(names.empty() ? L"No managed applications found." :
                     L"Select one application to continue.", 236, 388, 527, 35);
            }
        } else if (page_ == 1) {
            title(L"Authentication", L"Verify your password or recovery code");
            text(L"Application: " + label_, 236, 165, 527, 35, true);
            HWND radio_pass = add(L"BUTTON", L"Password", 236, 234, 222, 30,
                                  BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, password_id);
            radio_recovery_ = add(L"BUTTON", L"Recovery code", 486, 234, 269, 30,
                                  BS_AUTORADIOBUTTON | WS_TABSTOP, recovery_id);
            SendMessageW(recovery_mode_ ? radio_recovery_ : radio_pass,
                         BM_SETCHECK, BST_CHECKED, 0);
            text(L"Management credential", 236, 300, 527, 27, true);
            pass_edit_ = edit(secret_, 236, 330, 527, true);
            text(L"This credential protects the normal PMF removal workflow.",
                 236, 389, 527, 37);
        } else if (page_ == 2) {
            title(L"Confirm removal", L"Review before removing the managed copy");
            text(L"Application: " + label_, 236, 182, 527, 31, true);
            text(L"The PMF-managed executable and manifest will be deleted.\n"
                 L"The original executable and recovery text file stay untouched.",
                 236, 248, 527, 77);
            agreement_ = add(L"BUTTON", L"I authorize removing this managed copy.",
                             236, 369, 527, 34, BS_AUTOCHECKBOX | WS_TABSTOP);
        } else {
            title(L"Removal complete", L"The selected managed copy has been deleted");
            text(L"Application: " + label_, 236, 194, 527, 37, true);
            text(L"The original executable and recovery text file were not changed.",
                 236, 302, 527, 59);
        }
    }

    void error(const std::wstring& message) {
        MessageBoxW(window_, message.c_str(), L"PMF", MB_OK | MB_ICONWARNING);
    }

    void browse() {
        const auto selected = pmf::browse_executable();
        if (!selected) return;
        file_ = *selected;
        SetWindowTextW(file_edit_, file_.c_str());
        if (value_of(label_edit_).empty()) {
            auto proposed = file_.stem().wstring();
            std::replace(proposed.begin(), proposed.end(), L' ', L'-');
            if (pmf::valid_label(ascii(proposed))) {
                SetWindowTextW(label_edit_, proposed.c_str());
            }
        }
    }

    void save() {
        const auto selected = pmf::browse_recovery_destination(ascii(label_));
        if (!selected) return;
        recovery_ = *selected;
        SetWindowTextW(recovery_edit_, recovery_.c_str());
    }

    void next() {
        if (finished_) {
            DestroyWindow(window_);
            return;
        }
        if (loader_) {
            if (page_ == 1) {
                file_ = value_of(file_edit_);
                label_ = value_of(label_edit_);
                if (!pmf::inspect_target(file_, ascii(label_))) {
                    error(L"Select an existing .exe file and a valid installation label.");
                    return;
                }
            } else if (page_ == 2) {
                auto first = value_of(pass_edit_);
                auto repeated = value_of(confirm_edit_);
                bool valid = first.size() >= 12 && first == repeated && !recovery_.empty();
                scrub(repeated);
                if (!valid) {
                    scrub(first);
                    error(L"Passwords must match, have at least 12 characters, and a recovery file must be selected.");
                    return;
                }
                scrub(password_);
                password_ = std::move(first);
            } else if (page_ == 3) {
                if (SendMessageW(agreement_, BM_GETCHECK, 0, 0) != BST_CHECKED) {
                    error(L"Please explicitly authorize this operation.");
                    return;
                }
                auto identity = pmf::inspect_target(file_, ascii(label_));
                if (!identity) {
                    error(L"The selected executable is not accessible.");
                    return;
                }
                std::error_code ec;
                if (!pmf::prepare_workspace(ec)) {
                    error(L"Cannot prepare the PMF folder. Confirm you have write permissions.");
                    return;
                }
                std::string failure;
                bool installed = pmf::install_managed(*identity, password_, recovery_, failure);
                scrub(password_);
                if (!installed) {
                    error(std::wstring(failure.begin(), failure.end()));
                    return;
                }
                finished_ = true;
            }
        } else {
            if (page_ == 0) {
                auto index = SendMessageW(package_list_, LB_GETCURSEL, 0, 0);
                if (index == LB_ERR) {
                    error(L"Select a managed application.");
                    return;
                }
                auto length = SendMessageW(package_list_, LB_GETTEXTLEN,
                                           static_cast<WPARAM>(index), 0);
                if (length <= 0) {
                    error(L"Invalid selection.");
                    return;
                }
                std::wstring selected(static_cast<std::size_t>(length + 1), L'\0');
                SendMessageW(package_list_, LB_GETTEXT, static_cast<WPARAM>(index),
                             reinterpret_cast<LPARAM>(selected.data()));
                selected.resize(static_cast<std::size_t>(length));
                label_ = std::move(selected);
            } else if (page_ == 1) {
                scrub(secret_);
                secret_ = value_of(pass_edit_);
                recovery_mode_ = SendMessageW(radio_recovery_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (secret_.empty()) {
                    error(L"Enter your password or recovery code.");
                    return;
                }
            } else if (page_ == 2) {
                if (SendMessageW(agreement_, BM_GETCHECK, 0, 0) != BST_CHECKED) {
                    error(L"Please explicitly authorize removal.");
                    return;
                }
                std::string failure;
                bool removed = pmf::uninstall_managed(ascii(label_), secret_,
                                                      recovery_mode_, failure);
                scrub(secret_);
                if (!removed) {
                    error(std::wstring(failure.begin(), failure.end()));
                    return;
                }
                finished_ = true;
            }
        }
        ++page_;
        render();
    }

    void back() {
        if (page_ == 0 || finished_) return;
        if (loader_ && page_ == 2) scrub(password_);
        --page_;
        render();
    }
};

int run_loader(HINSTANCE instance, int show) {
    Window window(true);
    return window.run(instance, show);
}

int run_unloader(HINSTANCE instance, int show) {
    Window window(false);
    return window.run(instance, show);
}

}

#endif
