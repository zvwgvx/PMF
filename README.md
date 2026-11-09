# PMF — Persistence Management Framework

PMF is a C++20 framework for transparent, authorized Windows application lifecycle management. It consists of two independent command-line executables, `pmf-loader.exe` and `pmf-unloader.exe`, which share statically linked core code.

## Current behavior

Version 0.1.0 inspects a user-selected Windows executable and validates its label. On Windows, the loader requests explicit confirmation before creating or verifying its data directory at `C:\ProgramData\PMF`. The unloader can report whether this directory exists with `--status`.

No target application is installed, launched, registered for startup, or modified. The project does not create AV exclusions, install services, register itself in Apps & Features, or prevent administrative removal.

`C:\ProgramData\PMF` is used for application-managed data rather than `C:\Windows\PMF`, because the Windows system directory is not an appropriate installation destination for third-party tools.

## Build

On Windows, install CMake 3.20+ and an MSVC or Clang C++20 toolchain.

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The MSVC configuration requests the static CRT so that release builds can be distributed as standalone executables without shipping an MSVC runtime DLL. No MSI/EXE installer is generated, and nothing is added to the Windows installed-apps list.

## CLI

```powershell
.\build\Release\pmf-loader.exe "C:\Path\Example.exe" example
.\build\Release\pmf-unloader.exe --status
```

The loader prompts for the executable path and label if invoked without arguments.

## Design goals

- Explicit consent for any installation
- Identity and integrity verification
- Reversible, auditable application lifecycle management
- Password-protected normal management workflows with an administrator recovery path
- Compatibility with Windows security products without requesting broad folder exclusions

PMF does not provide stealth, antivirus bypass, anti-removal mechanisms, or automatic C2/implant deployment.
