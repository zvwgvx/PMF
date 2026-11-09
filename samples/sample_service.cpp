#include <windows.h>

namespace {

SERVICE_STATUS_HANDLE handle = nullptr;
SERVICE_STATUS state{};
HANDLE stop_event = nullptr;

void update(DWORD current, DWORD accepted, DWORD wait_hint = 0) {
    state.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    state.dwCurrentState = current;
    state.dwControlsAccepted = accepted;
    state.dwWin32ExitCode = NO_ERROR;
    state.dwWaitHint = wait_hint;
    SetServiceStatus(handle, &state);
}

DWORD WINAPI control(DWORD command, DWORD, LPVOID, LPVOID) {
    if (command == SERVICE_CONTROL_STOP) {
        update(SERVICE_STOP_PENDING, 0, 2000);
        if (stop_event) SetEvent(stop_event);
        return NO_ERROR;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI service_main(DWORD, LPWSTR*) {
    handle = RegisterServiceCtrlHandlerExW(L"PMF-Sample", &control, nullptr);
    if (!handle) return;

    update(SERVICE_START_PENDING, 0, 2000);
    stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (!stop_event) {
        update(SERVICE_STOPPED, 0);
        return;
    }

    update(SERVICE_RUNNING, SERVICE_ACCEPT_STOP);
    WaitForSingleObject(stop_event, INFINITE);
    update(SERVICE_STOPPED, 0);
    CloseHandle(stop_event);
    stop_event = nullptr;
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SERVICE_TABLE_ENTRYW entries[] = {
        {const_cast<LPWSTR>(L"PMF-Sample"), &service_main},
        {nullptr, nullptr}
    };
    return StartServiceCtrlDispatcherW(entries) ? 0 : 1;
}
