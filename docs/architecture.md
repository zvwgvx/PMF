# PMF 0.5.0 — CLI architecture

## Executables

`pmf-loader.exe` and `pmf-unloader.exe` are independently linkable Windows console binaries. The former handles interactive service installation; the latter handles listing and authenticated service removal.

They use `pmf-core` for PE validation, authenticated manifest storage, and Service Control Manager interaction. The prior Win32 wizard source is retained as historical source, but is not built or distributed in v0.5.0.

## Registration lifecycle

The loader prompts for a Windows Service EXE, a unique service label, an optional password, and a location to save recovery text. It prints the configured service name, account and destination and requires explicit confirmation before registration.

Services are installed into `%ProgramFiles%\PMF\Services\<label>\service.exe`, with authentication metadata in `%ProgramData%\PMF\managed\<label>\manifest.txt`.

PMF registers `PMF-<label>` as a standard, auto-start `SERVICE_WIN32_OWN_PROCESS` service under `NT AUTHORITY\LocalService`, never LocalSystem. Windows UAC is used for privileged installation and removal. The service remains visible and removable through Windows administration.

## Credential model

Password mode is either `set` or `none`. In `set` mode, any nonempty password length is accepted and verified using salted PBKDF2-HMAC-SHA256. In `none` mode, no management password is required for PMF removal. Both modes generate an independent recovery code that is exported outside managed storage.

Earlier manifests lacking the explicit `password_mode` value continue to use password validation.

## Limitations

PMF does not create or convert Windows Service executables, supply a SYSTEM privilege option, configure antivirus exclusions, or implement stealth or anti-removal persistence. Administrator authority to manage services is not constrained by PMF authentication.
