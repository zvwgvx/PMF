#pragma once

#ifdef _WIN32

#include <windows.h>
#include <conio.h>
#include <io.h>
#include <cstdio>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>

namespace pmf::tui {

inline bool interactive() {
    return _isatty(_fileno(stdin)) != 0;
}

inline void clear() {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (output == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(output, &info)) {
        std::wcout << L"\n\n";
        return;
    }

    const DWORD length = static_cast<DWORD>(info.dwSize.X) * info.dwSize.Y;
    const COORD origin{0, 0};
    DWORD written = 0;
    FillConsoleOutputCharacterW(output, L' ', length, origin, &written);
    FillConsoleOutputAttribute(output, info.wAttributes, length, origin, &written);
    SetConsoleCursorPosition(output, origin);
}

inline void header(const std::wstring& title, const std::wstring& subtitle) {
    std::wcout << L"============================================================\n";
    std::wcout << L"  " << title << L"\n";
    std::wcout << L"============================================================\n";
    if (!subtitle.empty()) std::wcout << L"  " << subtitle << L"\n";
    std::wcout << L"\n";
}

inline std::wstring ask(const std::wstring& prompt, const std::wstring& fallback = L"") {
    std::wcout << prompt;
    if (!fallback.empty()) std::wcout << L" [" << fallback << L"]";
    std::wcout << L": " << std::flush;
    std::wstring answer;
    if (!std::getline(std::wcin, answer)) return {};
    return answer.empty() ? fallback : answer;
}

inline bool confirm(const std::wstring& question) {
    const auto answer = ask(question + L" [y/N]");
    return answer == L"y" || answer == L"Y" || answer == L"yes" || answer == L"YES";
}

inline std::wstring normalize_path(std::wstring value) {
    const auto begin = value.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) return {};
    const auto end = value.find_last_not_of(L" \t\r\n");
    value = value.substr(begin, end - begin + 1);
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

inline int choose(const std::wstring& title, const std::wstring& subtitle,
                  const std::vector<std::wstring>& items) {
    if (items.empty()) return -1;

    if (!interactive()) {
        header(title, subtitle);
        for (size_t i = 0; i < items.size(); ++i) {
            std::wcout << L"  " << i + 1 << L". " << items[i] << L"\n";
        }
        for (;;) {
            const auto answer = ask(L"Select number (0 = cancel)");
            if (answer.empty() || answer == L"0") return -1;
            try {
                const size_t selection = std::stoull(answer);
                if (selection >= 1 && selection <= items.size()) return static_cast<int>(selection - 1);
            } catch (...) {}
            std::wcout << L"Invalid selection.\n";
        }
    }

    size_t selected = 0;
    for (;;) {
        clear();
        header(title, subtitle);
        const size_t first = (selected / 10) * 10;
        const size_t last = (first + 10 < items.size()) ? first + 10 : items.size();

        for (size_t i = first; i < last; ++i) {
            std::wcout << (selected == i ? L" > " : L"   ")
                       << i + 1 << L". " << items[i] << L"\n";
        }

        if (items.size() > 10) {
            std::wcout << L"\n  Showing " << first + 1 << L"-" << last
                       << L" of " << items.size() << L"\n";
        }
        std::wcout << L"\n  Up/Down: navigate   Enter: select   Esc: cancel\n" << std::flush;

        const int key = _getwch();
        if (key == 27) return -1;
        if (key == 13) return static_cast<int>(selected);

        if (key == 0 || key == 224) {
            const int next = _getwch();
            if (next == 72 && selected > 0) --selected;
            if (next == 80 && selected + 1 < items.size()) ++selected;
            if (next == 73) selected = selected > 10 ? selected - 10 : 0;
            if (next == 81) selected = (selected + 10 < items.size()) ? selected + 10 : items.size() - 1;
            if (next == 71) selected = 0;
            if (next == 79) selected = items.size() - 1;
        }

        if (key >= '1' && key <= '9') {
            const size_t index = static_cast<size_t>(key - '1');
            if (index < items.size()) return static_cast<int>(index);
        }
    }
}

inline void pause() {
    if (!interactive()) return;
    std::wcout << L"\nPress any key to close..." << std::flush;
    _getwch();
    std::wcout << L"\n";
}

}

#endif
