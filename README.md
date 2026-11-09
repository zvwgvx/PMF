# PMF — Persistence Management Framework

PMF v0.5.0 provides two standalone C++20 Windows command-line utilities for consent-based management of existing Windows Service executables:

- `pmf-loader.exe` — register and start a Windows Service using Windows Service Control Manager
- `pmf-unloader.exe` — list, stop, unregister and remove PMF-managed services

Both binaries are Windows console applications. They are not added to Apps & Features and do not require a separate MSI installer.

## Usage

From an elevated Windows Terminal or PowerShell session:

```powershell
.\pmf-loader.exe install "C:\Path\service.exe" example
.\pmf-unloader.exe list
.\pmf-unloader.exe remove example
```

Running `pmf-loader.exe` or `pmf-unloader.exe` without arguments enters interactive mode. For help use `--help`.

During installation, the loader prompts for an optional password. Any length is permitted, including an empty password. When no password is chosen, PMF explicitly warns that its normal removal workflow will not require a password. A recovery code is always generated and written to a user-selected text file.

During removal, authentication may use the original password, the recovery code, or no password if the service was registered without one. A Windows administrator remains able to stop and manage the service using standard Windows administration tools independently of PMF.

## Service installation

The selected `.exe` must be a real Windows Service that implements the Service Control Manager lifecycle API. A valid PE header does not prove service compatibility; PMF asks SCM to start the service and reports startup failure.

| Setting | Value |
|---|---|
| Service name | `PMF-<label>` |
| Installed EXE | `C:\Program Files\PMF\Services\<label>\service.exe` |
| Metadata | `C:\ProgramData\PMF\managed\<label>\manifest.txt` |
| Account | `NT AUTHORITY\LocalService` |
| Startup | Automatic |
| Elevation | UAC administrator approval |

A SYSTEM/LocalSystem account option is not provided. This installer does not create antivirus exclusions, conceal its services, or prevent administrative removal.

## Passwords and recovery codes

Passwords are optional, and there is no minimum length. Where used, passwords are checked against salted PBKDF2-HMAC-SHA256 verifiers. A randomly generated recovery code with 160 bits of entropy is saved separately to a text file chosen by the user. Recovery files should be protected and stored outside PMF's managed folders.

Existing manifests from earlier versions remain readable. New manifests also indicate when a password was intentionally omitted.

## Build

Requires CMake 3.20+ and a C++20 toolchain. On Windows, use MSVC:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The built `pmf-loader.exe` and `pmf-unloader.exe` are console-subsystem binaries that use the static MSVC runtime. Both request administrative approval via an embedded UAC manifest. GitHub Actions builds the two binaries and publishes them as the `pmf-windows` artifact.

## Antivirus and system security

Being a CLI does not inherently make an application malicious or safe. Microsoft Defender and Windows SmartScreen may still warn about unsigned, unfamiliar executables or service-registration behavior. PMF does not attempt to bypass AV, require broad folder exclusions, or disable Windows security protections.
