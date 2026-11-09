# PMF v0.7.0 — dual GUI and CLI interfaces

## Design

PMF builds four independently runnable tools: `pmf-loader.exe` and `pmf-unloader.exe` use the native Win32 wizard, while `pmf-loader-cli.exe` and `pmf-unloader-cli.exe` provide terminal interactions. All four link against the same `pmf-core` service and credential implementation. The two graphical programs share a static Win32 wizard component. The visual design follows conventional Windows setup assistants rather than a custom webview or mixed modern/classic UI.

The wizard uses native EDIT, BUTTON, LISTBOX and progress-bar controls, the Windows file picker and Save As picker, classic Windows dialog keyboard navigation, and modeless page state. Lengthy Windows service operations run on a worker thread so the UI remains responsive while controls are disabled.

## Install sequence

Welcome → Select Windows Service → Service Configuration → Password and Recovery → Review → Installing → Finish.

The installer checks the executable PE format, collects an independent label, allows a blank or non-empty management password, confirms the recovery file path, and requires explicit consent. SCM registers an auto-start own-process Windows Service named PMF-<label> under NT AUTHORITY\LocalService.

## Removal sequence

Welcome → Select Service → Authenticate → Review → Removing → Finish.

The removal wizard supports password, recovery-code, and no-password modes. Removal uses Windows SCM stop and unregister operations before deleting PMF-managed binary and metadata files. The source EXE and recovery export remain untouched.

## Implementation and scope

- C++20 and Win32, no .NET, Electron or Qt.
- Static MSVC runtime.
- Embedded administrator UAC manifest for SCM configuration.
- Native Windows file dialogs.
- Transparent installation under Program Files and metadata under ProgramData.
- No SYSTEM option, antivirus exclusions, anti-removal, process injection, or service hiding.

The build and test workflows compile Windows and Linux targets and upload the Windows GUI executables.

## Build targets

The CMake targets `pmf-loader` and `pmf-unloader` use the Windows GUI subsystem. Targets `pmf-loader-cli` and `pmf-unloader-cli` use the console subsystem. GitHub Actions builds and bundles all four in the `pmf-windows` artifact.
