# PMF — Persistence Management Framework

PMF v0.7.0 provides both native Win32 setup wizards and interactive console tools for the same Windows Service management core. All four tools are independent EXE binaries; PMF does not use an MSI installer or register itself in Windows Apps & Features.

## Four executables

| Binary | Interface | Purpose |
|---|---|---|
| `pmf-loader.exe` | Classic Windows GUI wizard | Install an authorized Windows Service |
| `pmf-unloader.exe` | Classic Windows GUI wizard | Remove a PMF-managed service |
| `pmf-loader-cli.exe` | Console | Install a service interactively or with path and label arguments |
| `pmf-unloader-cli.exe` | Console | List and remove PMF-managed services |

Both interface styles call the same shared `pmf-core` library and use the same metadata, password, recovery-code, and SCM implementation. Changing between the GUI and CLI does not change the installed service.

### CLI usage

Run from an elevated Windows Terminal:

```powershell
.\pmf-loader-cli.exe install "C:\Path\my-service.exe" example
.\pmf-unloader-cli.exe list
.\pmf-unloader-cli.exe remove example
```

The CLI programs also accept interactive input without fixed arguments. Passwords are never required as command-line arguments.

## Classic wizard experience

The two graphical programs use a consistent Windows installer layout with a white header, native controls, system-color content area, a classic welcome/finish sidebar, and Back / Next / Cancel buttons. The interface includes a working state with a progress indicator and a completion or error page.

### Setup wizard: pmf-loader.exe

1. Welcome — explains what the installer will do.
2. Select Windows Service — Browse for an existing service-compatible executable and choose a label.
3. Service Configuration — review service account, startup behavior and installation path.
4. Password and Recovery — set any password, including a blank one; choose a separate text file for the recovery key.
5. Ready to Install — review and explicitly authorize registration.
6. Installing — Windows SCM registration and start operation with progress status.
7. Finish — displays the result and a meaningful error if installation fails.

The executable must implement Windows Service lifecycle APIs. Validating the PE header is necessary but does not guarantee the selected executable can run as a Windows Service.

### Removal wizard: pmf-unloader.exe

1. Welcome.
2. Select Windows Service — choose a PMF-managed service.
3. Authenticate — management password, recovery code or no-password mode.
4. Confirm Removal — explicitly authorize service stop/unregistration and managed file cleanup.
5. Removing — progress indicator.
6. Finish — displays success or the Windows error.

The original selected executable and exported recovery file are retained after removal.

## Windows Service configuration

| Property | Value |
|---|---|
| Service name | PMF-<label> |
| Program | C:\Program Files\PMF\Services\<label>\service.exe |
| Metadata | C:\ProgramData\PMF\managed\<label>\manifest.txt |
| Service account | NT AUTHORITY\LocalService |
| Startup | Automatic |
| Privileged operations | Windows UAC approval required |

The service remains fully visible to Windows Services and administrative tools. The management password governs PMF's normal removal workflow only and cannot prevent administrative recovery.

## Password and recovery

Passwords are optional and have no minimum length. Choosing an empty password displays an explicit warning. A unique recovery code is still generated and exported to a user-selected text file. The corresponding metadata stores only salted PBKDF2-HMAC-SHA256 verifiers, never the plaintext password or recovery code.

## Build

On Windows, install Visual Studio 2022/2026 C++ Build Tools and CMake 3.20 or newer.

~~~powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Double-click `build/Release/pmf-loader.exe` or `build/Release/pmf-unloader.exe` for the GUI. Run `build/Release/pmf-loader-cli.exe` or `build/Release/pmf-unloader-cli.exe` in a terminal for the CLI. The MSVC release build statically links the C runtime. GitHub Actions produces the `pmf-windows` artifact containing all four EXEs.

## Trust and security

Running as a GUI does not eliminate AV false positives. Unsigned, low-reputation binaries can be flagged by SmartScreen, and Service Control Manager registration may be assessed by antivirus behavior detection. PMF does not attempt to bypass antivirus, create exclusions, hide processes, or prevent administrators from removing the service. For distribution, use reproducible builds and Authenticode code signing.

This framework deliberately uses LocalService rather than SYSTEM for selected service executables. PMF does not convert non-service EXEs into services.
