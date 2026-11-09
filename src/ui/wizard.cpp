#include "pmf/identity.hpp"
#include "pmf/managed.hpp"
#include "pmf/service.hpp"
#include "pmf/windows_ui.hpp"

#ifdef _WIN32

#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace pmf::ui {

namespace {

constexpr int id_back = 1201;
constexpr int id_next = 1202;
constexpr int id_cancel = 1203;
constexpr int id_browse_exe = 1204;
constexpr int id_browse_recovery = 1205;
constexpr int id_service_list = 1206;
constexpr int id_password = 1207;
constexpr int id_recovery = 1208;
constexpr int id_none = 1209;
constexpr int id_consent = 1210;
constexpr UINT msg_complete = WM_APP + 21;

std::wstring text_value(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length < 0) return {};
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    const int written = GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(written));
    return text;
}

std::string narrow_label(const std::wstring& text) {
    std::string result;
    result.reserve(text.size());
    for (const wchar_t c : text) {
        if (c < 0 || c > 127) return {};
        result.push_back(static_cast<char>(c));
    }
    return result;
}

void clear_secret(std::wstring& secret) {
    if (secret.empty()) return;
    SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
    secret.clear();
}

std::wstring wide_message(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

std::wstring shorten_path(const std::wstring& text, std::size_t maximum = 87) {
    if (text.size() <= maximum) return text;
    return text.substr(0, 35) + L"..." + text.substr(text.size() - (maximum - 38));
}

}

class Wizard {
public:
    explicit Wizard(bool install) : install_(install) {}

    int run(HINSTANCE instance, int show) {
        instance_ = instance;
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&controls);

        normal_ = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        heading_ = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        brand_ = CreateFontW(-33, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        white_ = CreateSolidBrush(RGB(255, 255, 255));
        panel_ = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
        sidebar_ = CreateSolidBrush(RGB(44, 65, 91));

        WNDCLASSW klass{};
        klass.lpfnWndProc = &Wizard::dispatch;
        klass.hInstance = instance;
        klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        klass.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        klass.lpszClassName = install_ ? L"PMFSetupClassic" : L"PMFRemoveClassic";
        if (!RegisterClassW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            release();
            return 1;
        }

        RECT bounds{0, 0, 720, 506};
        AdjustWindowRectEx(&bounds, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           FALSE, 0);
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

        window_ = CreateWindowExW(0, klass.lpszClassName,
                                  install_ ? L"PMF Setup" : L"PMF Removal",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                                  WS_CLIPCHILDREN,
                                  x, y, width, height,
                                  nullptr, nullptr, instance, this);

        if (!window_) {
            release();
            return 1;
        }

        ShowWindow(window_, show);
        UpdateWindow(window_);

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(window_, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }

        if (worker_.joinable()) worker_.join();
        release();
        return static_cast<int>(message.wParam);
    }

private:
    bool install_{};
    bool busy_{};
    bool finished_{};
    bool succeeded_{};
    bool use_recovery_{};
    bool no_password_{};
    int page_{};
    HINSTANCE instance_{};
    HWND window_{};
    HWND exe_input_{};
    HWND label_input_{};
    HWND password_input_{};
    HWND confirm_input_{};
    HWND recovery_input_{};
    HWND service_list_{};
    HWND radio_password_{};
    HWND radio_recovery_{};
    HWND radio_none_{};
    HWND consent_{};
    HWND progress_{};
    HWND status_text_{};
    HWND back_button_{};
    HWND next_button_{};
    HWND cancel_button_{};
    HFONT normal_{};
    HFONT heading_{};
    HFONT brand_{};
    HBRUSH white_{};
    HBRUSH panel_{};
    HBRUSH sidebar_{};
    std::vector<HWND> children_;
    std::filesystem::path exe_;
    std::filesystem::path recovery_;
    std::wstring label_;
    std::wstring password_;
    std::wstring secret_;
    std::wstring failure_;
    std::thread worker_;

    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM w, LPARAM l) {
        Wizard* wizard = reinterpret_cast<Wizard*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* creation = reinterpret_cast<CREATESTRUCTW*>(l);
            wizard = static_cast<Wizard*>(creation->lpCreateParams);
            wizard->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(wizard));
        }
        return wizard ? wizard->process(message, w, l) :
                        DefWindowProcW(window, message, w, l);
    }

    LRESULT process(UINT message, WPARAM w, LPARAM l) {
        switch (message) {
        case WM_CREATE:
            render();
            return 0;
        case WM_PAINT:
            paint();
            return 0;
        case WM_CTLCOLORSTATIC: {
            const auto child = reinterpret_cast<HWND>(l);
            RECT bounds{};
            GetWindowRect(child, &bounds);
            MapWindowPoints(HWND_DESKTOP, window_, reinterpret_cast<POINT*>(&bounds), 2);
            const HDC dc = reinterpret_cast<HDC>(w);
            SetBkMode(dc, TRANSPARENT);
            if (bounds.left < 183 && (page_ == 0 || page_ == final_page())) {
                SetTextColor(dc, RGB(255, 255, 255));
                return reinterpret_cast<LRESULT>(sidebar_);
            }
            SetTextColor(dc, RGB(24, 24, 24));
            return reinterpret_cast<LRESULT>(bounds.top < 86 ? white_ : panel_);
        }
        case WM_COMMAND:
            if (HIWORD(w) == BN_CLICKED) {
                switch (LOWORD(w)) {
                case id_back:
                    back();
                    return 0;
                case id_next:
                    next();
                    return 0;
                case id_cancel:
                    if (!busy_) DestroyWindow(window_);
                    return 0;
                case id_browse_exe:
                    browse_exe();
                    return 0;
                case id_browse_recovery:
                    browse_recovery();
                    return 0;
                }
            }
            return 0;
        case msg_complete:
            busy_ = false;
            if (worker_.joinable()) worker_.join();
            succeeded_ = w == 1;
            finished_ = succeeded_;
            clear_secret(password_);
            clear_secret(secret_);
            ++page_;
            render();
            return 0;
        case WM_CLOSE:
            if (!busy_) DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            clear_secret(password_);
            clear_secret(secret_);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window_, message, w, l);
    }

    void release() {
        clear_secret(password_);
        clear_secret(secret_);
        if (normal_) DeleteObject(normal_);
        if (heading_) DeleteObject(heading_);
        if (brand_) DeleteObject(brand_);
        if (white_) DeleteObject(white_);
        if (panel_) DeleteObject(panel_);
        if (sidebar_) DeleteObject(sidebar_);
    }

    void paint() {
        PAINTSTRUCT ps{};
        const HDC dc = BeginPaint(window_, &ps);
        RECT bounds{};
        GetClientRect(window_, &bounds);

        FillRect(dc, &bounds, panel_);
        RECT header{0, 0, bounds.right, 87};
        FillRect(dc, &header, white_);

        if (page_ == 0 || finished_) {
            RECT panel{0, 87, 181, 410};
            FillRect(dc, &panel, sidebar_);
        }

        HPEN divider = CreatePen(PS_SOLID, 1, RGB(174, 174, 174));
        HGDIOBJ old = SelectObject(dc, divider);
        MoveToEx(dc, 0, 87, nullptr);
        LineTo(dc, bounds.right, 87);
        MoveToEx(dc, 0, 410, nullptr);
        LineTo(dc, bounds.right, 410);
        SelectObject(dc, old);
        DeleteObject(divider);
        EndPaint(window_, &ps);
    }

    HWND add(const wchar_t* klass, const std::wstring& caption,
             int x, int y, int width, int height, DWORD style = 0,
             int id = 0, HFONT font = nullptr, DWORD extended = 0) {
        HWND control = CreateWindowExW(extended, klass, caption.c_str(),
                                       WS_CHILD | WS_VISIBLE | style,
                                       x, y, width, height, window_,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       instance_, nullptr);
        if (control) {
            SendMessageW(control, WM_SETFONT,
                         reinterpret_cast<WPARAM>(font ? font : normal_), TRUE);
            children_.push_back(control);
        }
        return control;
    }

    HWND label(const std::wstring& caption, int x, int y, int width, int height = 25,
               bool heading = false) {
        return add(L"STATIC", caption, x, y, width, height, SS_LEFT,
                   0, heading ? heading_ : normal_);
    }

    HWND input(const std::wstring& value, int x, int y, int width,
               bool masked = false, bool readonly = false) {
        DWORD style = ES_AUTOHSCROLL | WS_TABSTOP;
        if (masked) style |= ES_PASSWORD;
        if (readonly) style |= ES_READONLY;
        return add(L"EDIT", value, x, y, width, 27, style, 0,
                   normal_, WS_EX_CLIENTEDGE);
    }

    HWND button(const std::wstring& caption, int id, int x, int width,
                bool primary = false) {
        return add(L"BUTTON", caption, x, 435, width, 30,
                   (primary ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON) | WS_TABSTOP, id);
    }

    void heading(const std::wstring& title, const std::wstring& description) {
        label(title, 32, 17, 640, 30, true);
        label(description, 33, 53, 648, 25);
    }

    void body_label(const std::wstring& text, int y, int height = 27) {
        label(text, 38, y, 637, height);
    }

    void show_welcome(bool ending) {
        label(L"PMF", 31, 135, 137, 52, true);
        label(L"Windows\nService\nManager", 31, 198, 145, 92);
        if (!ending) {
            label(install_ ? L"Welcome to the PMF Setup Wizard" :
                             L"Welcome to the PMF Removal Wizard",
                  208, 149, 470, 67, true);
            label(install_ ?
                  L"This wizard will register an existing Windows Service executable.\n\n"
                  L"The service will run under NT AUTHORITY\\LocalService and remain\n"
                  L"visible in the Windows Services management console." :
                  L"This wizard will safely stop, unregister, and remove a\n"
                  L"PMF-managed Windows Service.\n\n"
                  L"The original executable and exported recovery file are retained.",
                  208, 234, 477, 161);
        } else {
            label(succeeded_ ? (install_ ? L"Setup Complete" : L"Removal Complete") :
                               (install_ ? L"Installation Failed" : L"Removal Failed"),
                  208, 146, 469, 55, true);
            if (succeeded_) {
                label(install_ ?
                      L"Windows Service PMF-" + label_ + L" is running.\n\n"
                      L"Your recovery text file has been saved. Keep it secure." :
                      L"Service PMF-" + label_ + L" has been unregistered.\n\n"
                      L"The original executable and recovery text file are unchanged.",
                      208, 223, 482, 155);
            } else {
                label(L"Windows reported an error:\n\n" + failure_,
                      208, 219, 481, 171);
            }
        }
    }

    void render() {
        for (HWND control : children_) DestroyWindow(control);
        children_.clear();
        exe_input_ = label_input_ = password_input_ = confirm_input_ = nullptr;
        recovery_input_ = service_list_ = radio_password_ = radio_recovery_ = nullptr;
        radio_none_ = consent_ = progress_ = status_text_ = nullptr;

        if (page_ == 0) {
            heading(install_ ? L"PMF Setup" : L"PMF Removal",
                    install_ ? L"Windows Service Installer" : L"Windows Service Manager");
            show_welcome(false);
        } else if (finished_ || page_ == final_page()) {
            heading(install_ ? L"PMF Setup" : L"PMF Removal",
                    L"Setup Wizard");
            show_welcome(true);
        } else if (install_) {
            installer_page();
        } else {
            uninstaller_page();
        }

        back_button_ = button(L"< &Back", id_back, 352, 106);
        next_button_ = button(finished_ || page_ == final_page() ? L"&Finish" :
                              page_ == working_page() ? L"Working..." :
                              page_ == review_page() ?
                                  (install_ ? L"&Install" : L"&Remove") : L"&Next >",
                              id_next, 468, 106, true);
        cancel_button_ = button(L"Cancel", id_cancel, 587, 106);

        EnableWindow(back_button_, (page_ > 0 && page_ < working_page()) ||
                                    (page_ == final_page() && !succeeded_));
        EnableWindow(next_button_, page_ != working_page());
        EnableWindow(cancel_button_, !busy_ && page_ != final_page());

        SetFocus(page_ == working_page() ? window_ : next_button_);
        InvalidateRect(window_, nullptr, TRUE);
    }

    int review_page() const { return install_ ? 4 : 3; }
    int working_page() const { return install_ ? 5 : 4; }
    int final_page() const { return install_ ? 6 : 5; }

    void installer_page() {
        if (page_ == 1) {
            heading(L"Select Windows Service",
                    L"Select a service-compatible executable and choose a unique label.");
            body_label(L"Windows Service executable (.exe)", 124);
            exe_input_ = input(exe_.wstring(), 38, 154, 520);
            add(L"BUTTON", L"&Browse...", 571, 153, 112, 29,
                BS_PUSHBUTTON | WS_TABSTOP, id_browse_exe);
            body_label(L"Service label", 230);
            label_input_ = input(label_, 38, 258, 645);
            body_label(L"Only letters, digits, '-' and '_' are permitted (maximum 64).", 311);
            body_label(L"The executable must implement the Windows Service Control Handler.", 348);
        } else if (page_ == 2) {
            heading(L"Service Configuration",
                    L"Review where Windows will install and run the service.");
            body_label(L"Service account", 128);
            input(L"NT AUTHORITY\\LocalService", 38, 156, 645, false, true);
            body_label(L"Startup type", 218);
            input(L"Automatic (managed by Windows Service Control Manager)", 38, 246,
                  645, false, true);
            body_label(L"Install location:", 308);
            body_label(L"C:\\Program Files\\PMF\\Services\\" + label_ + L"\\service.exe", 337, 54);
        } else if (page_ == 3) {
            heading(L"Password and Recovery",
                    L"Choose optional credentials and where to save your recovery key.");
            body_label(L"Management password (optional; any length)", 113);
            password_input_ = input(password_, 38, 142, 645, true);
            body_label(L"Confirm password", 188);
            confirm_input_ = input(L"", 38, 216, 645, true);
            body_label(L"Recovery text file (required)", 261);
            recovery_input_ = input(recovery_.wstring(), 38, 290, 520, false, true);
            add(L"BUTTON", L"&Save as...", 571, 289, 112, 29,
                BS_PUSHBUTTON | WS_TABSTOP, id_browse_recovery);
            body_label(L"A blank password allows PMF removal without a password.", 340);
            body_label(L"Store the recovery text file outside the PMF directory.", 369);
        } else if (page_ == 4) {
            heading(L"Ready to Install",
                    L"Please confirm the configuration before making system changes.");
            body_label(L"Service: PMF-" + label_, 119, 35);
            body_label(L"Source: " + shorten_path(exe_.wstring()), 158, 39);
            body_label(L"Account: LocalService                 Startup: Automatic", 210, 35);
            body_label(L"Password: " + std::wstring(password_.empty() ?
                                            L"None" : L"Configured"), 252, 31);
            body_label(L"Recovery: " + shorten_path(recovery_.wstring()), 291, 38);
            consent_ = add(L"BUTTON",
                           L"I authorize registration of this Windows Service.",
                           38, 361, 645, 30, BS_AUTOCHECKBOX | WS_TABSTOP, id_consent);
        } else if (page_ == 5) {
            heading(L"Installing Windows Service",
                    L"Preparing managed files and registering the service with Windows.");
            body_label(L"Windows Service Control Manager is processing the request.", 165, 45);
            progress_ = add(PROGRESS_CLASSW, L"", 38, 237, 645, 24, PBS_MARQUEE);
            SendMessageW(progress_, PBM_SETMARQUEE, TRUE, 38);
            status_text_ = label(L"Installing. Do not close this window.", 38, 286, 645, 43);
        }
    }

    void uninstaller_page() {
        if (page_ == 1) {
            heading(L"Select Windows Service",
                    L"Select the PMF-managed service you want to remove.");
            std::string error;
            const auto services = pmf::list_services(error);
            service_list_ = add(L"LISTBOX", L"", 38, 124, 645, 223,
                                WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY, id_service_list,
                                normal_, WS_EX_CLIENTEDGE);
            for (const auto& name : services) {
                const auto wide = std::wstring(name.begin(), name.end());
                SendMessageW(service_list_, LB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(wide.c_str()));
            }
            if (!label_.empty()) {
                SendMessageW(service_list_, LB_SELECTSTRING, static_cast<WPARAM>(-1),
                             reinterpret_cast<LPARAM>(label_.c_str()));
            }
            body_label(error.empty() ?
                       (services.empty() ? L"No PMF-managed services were found." :
                                           L"Only services registered by PMF are listed.") :
                       wide_message(error), 361, 36);
        } else if (page_ == 2) {
            show_auth_page();
        } else if (page_ == 3) {
            heading(L"Confirm Removal",
                    L"Review the service removal operation before continuing.");
            body_label(L"Service: PMF-" + label_, 133, 36);
            body_label(L"Windows will stop the service and unregister it from SCM.", 197, 37);
            body_label(L"The installed service EXE and PMF manifest will be removed.", 235, 43);
            body_label(L"The original source EXE and exported recovery file are preserved.", 276, 43);
            consent_ = add(L"BUTTON",
                           L"I authorize stopping, unregistering and removing this service.",
                           38, 360, 645, 30, BS_AUTOCHECKBOX | WS_TABSTOP, id_consent);
        } else if (page_ == 4) {
            heading(L"Removing Windows Service",
                    L"Please keep this window open while Windows processes the removal.");
            body_label(L"Stopping and unregistering PMF-" + label_, 171, 40);
            progress_ = add(PROGRESS_CLASSW, L"", 38, 237, 645, 24, PBS_MARQUEE);
            SendMessageW(progress_, PBM_SETMARQUEE, TRUE, 38);
            status_text_ = label(L"Working...", 38, 286, 645, 43);
        }
    }

    void show_auth_page() {
        heading(L"Authenticate", L"Choose how to authorize the PMF removal operation.");
        body_label(L"Service: PMF-" + label_, 116, 32);
        radio_password_ = add(L"BUTTON", L"Management password", 38, 167, 233, 31,
                              BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP, id_password);
        radio_recovery_ = add(L"BUTTON", L"Recovery code", 277, 167, 174, 31,
                              BS_AUTORADIOBUTTON | WS_TABSTOP, id_recovery);
        radio_none_ = add(L"BUTTON", L"No password", 466, 167, 211, 31,
                          BS_AUTORADIOBUTTON | WS_TABSTOP, id_none);
        SendMessageW(no_password_ ? radio_none_ :
                     use_recovery_ ? radio_recovery_ : radio_password_,
                     BM_SETCHECK, BST_CHECKED, 0);
        body_label(L"Credential", 225);
        password_input_ = input(secret_, 38, 257, 645, true);
        body_label(L"Use 'No password' only if the service was installed without one.", 323, 55);
    }

    void show_error(const std::wstring& message) {
        MessageBoxW(window_, message.c_str(), L"PMF Setup Wizard",
                    MB_ICONWARNING | MB_OK);
    }

    void browse_exe() {
        const auto selected = pmf::browse_executable();
        if (!selected) return;
        exe_ = *selected;
        SetWindowTextW(exe_input_, exe_.c_str());
        if (text_value(label_input_).empty()) {
            auto suggested = exe_.stem().wstring();
            std::replace(suggested.begin(), suggested.end(), L' ', L'-');
            if (pmf::valid_label(narrow_label(suggested))) {
                SetWindowTextW(label_input_, suggested.c_str());
            }
        }
    }

    void browse_recovery() {
        const auto selected = pmf::browse_recovery_destination(narrow_label(label_));
        if (!selected) return;
        recovery_ = *selected;
        SetWindowTextW(recovery_input_, recovery_.c_str());
    }

    bool read_install_details() {
        exe_ = text_value(exe_input_);
        label_ = text_value(label_input_);
        const auto identity = pmf::inspect_target(exe_, narrow_label(label_));
        std::string error;
        if (!identity || !pmf::inspect_service_executable(exe_, error)) {
            show_error(error.empty() ? L"Choose a valid Windows Service EXE and label." :
                       wide_message(error));
            return false;
        }
        return true;
    }

    bool read_credentials() {
        auto first = text_value(password_input_);
        auto second = text_value(confirm_input_);
        const bool match = first == second;
        clear_secret(second);
        if (!match) {
            clear_secret(first);
            show_error(L"The two passwords do not match.");
            return false;
        }
        if (recovery_.empty()) {
            clear_secret(first);
            show_error(L"Choose a location for the recovery text file.");
            return false;
        }
        if (first.empty()) {
            if (MessageBoxW(window_,
                            L"No management password was entered.\n\n"
                            L"PMF removal will not require a password. Continue?",
                            L"Password Not Set",
                            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
                clear_secret(first);
                return false;
            }
        }
        clear_secret(password_);
        password_ = std::move(first);
        return true;
    }

    void next() {
        if (busy_) return;
        if (page_ == final_page()) {
            DestroyWindow(window_);
            return;
        }
        if (install_) {
            if (page_ == 1 && !read_install_details()) return;
            if (page_ == 3 && !read_credentials()) return;
        } else {
            if (page_ == 1) {
                const LRESULT selected = SendMessageW(service_list_, LB_GETCURSEL, 0, 0);
                if (selected == LB_ERR) {
                    show_error(L"Please select a service.");
                    return;
                }
                const LRESULT length = SendMessageW(service_list_, LB_GETTEXTLEN, selected, 0);
                if (length <= 0) {
                    show_error(L"Unable to read the selected service name.");
                    return;
                }
                std::wstring name(static_cast<std::size_t>(length + 1), L'\0');
                SendMessageW(service_list_, LB_GETTEXT, selected,
                             reinterpret_cast<LPARAM>(name.data()));
                name.resize(static_cast<std::size_t>(length));
                label_ = std::move(name);
            } else if (page_ == 2) {
                const bool no_password = SendMessageW(radio_none_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                const bool recovery = SendMessageW(radio_recovery_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (!no_password && !recovery &&
                    SendMessageW(radio_password_, BM_GETCHECK, 0, 0) != BST_CHECKED) {
                    show_error(L"Choose an authentication method.");
                    return;
                }

                auto entered = text_value(password_input_);
                if (!no_password && entered.empty()) {
                    show_error(L"Enter the password or recovery code, or select 'No password'.");
                    return;
                }

                clear_secret(secret_);
                secret_ = no_password ? std::wstring() : std::move(entered);
                if (no_password) clear_secret(entered);
                no_password_ = no_password;
                use_recovery_ = recovery;
            }
        }

        if (page_ == review_page()) {
            if (SendMessageW(consent_, BM_GETCHECK, 0, 0) != BST_CHECKED) {
                show_error(L"Please confirm the operation before continuing.");
                return;
            }
            ++page_;
            busy_ = true;
            render();
            start_operation();
            return;
        }

        ++page_;
        render();
    }

    void back() {
        if (busy_ || page_ == 0) return;
        if (page_ == final_page() && !succeeded_) {
            failure_.clear();
            page_ = install_ ? 3 : 2;
            clear_secret(password_);
            clear_secret(secret_);
            render();
            return;
        }
        if (page_ >= working_page()) return;
        if (install_ && page_ == 3) {
            clear_secret(password_);
            password_ = text_value(password_input_);
        }
        --page_;
        render();
    }

    void start_operation() {
        const auto label = narrow_label(label_);
        const auto exe = exe_;
        const auto recovery = recovery_;
        auto password = install_ ? password_ : secret_;
        const bool recovered = use_recovery_;

        worker_ = std::thread([this, label, exe, recovery, password, recovered]() mutable {
            std::string error;
            bool success = false;

            if (install_) {
                const auto identity = pmf::inspect_target(exe, label);
                if (identity) {
                    success = pmf::install_service(*identity, password, recovery, error);
                } else {
                    error = "The selected service executable is no longer available.";
                }
            } else {
                success = pmf::uninstall_service(label, password, recovered, error);
            }

            failure_ = wide_message(error);
            clear_secret(password);
            PostMessageW(window_, msg_complete, success ? 1 : 0, 0);
        });
    }
};

int run_loader(HINSTANCE instance, int show) {
    Wizard wizard(true);
    return wizard.run(instance, show);
}

int run_unloader(HINSTANCE instance, int show) {
    Wizard wizard(false);
    return wizard.run(instance, show);
}

}

#endif
