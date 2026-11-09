# PMF v0.6.0 — classic Windows wizard

## Design

Two native Windows GUI executables, pmf-loader.exe and pmf-unloader.exe, share a static Win32 wizard component. The visual design follows conventional Windows setup assistants rather than a custom webview or mixed modern/classic UI.

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
