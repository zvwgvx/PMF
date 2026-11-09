# PMF v0.4.0 — Windows Service architecture

## Components

- `pmf-loader.exe`: graphical setup wizard for a user-selected Windows Service executable
- `pmf-unloader.exe`: graphical removal wizard with password and recovery-code authentication
- `pmf-wizard`: shared Win32 GUI library statically linked into both EXEs
- `pmf-core`: PE validation, authenticated manifests, file management, and SCM integration

PMF management EXEs use the Windows GUI subsystem with an administrator UAC manifest. No MSI installation or Apps & Features registration is performed for PMF itself.

## Install

1. Select an existing Windows Service PE and a valid installation label.
2. Provide explicit consent, a management password, and a recovery file destination.
3. Create `%ProgramFiles%\PMF\Services\<label>\service.exe`.
4. Store credential verifiers and manifest under `%ProgramData%\PMF\managed\<label>\`.
5. Register `PMF-<label>` with SCM under `NT AUTHORITY\LocalService` as an automatically started own-process service.
6. Ask SCM to start the service and wait for the running state.
7. Roll back newly created resources on failed installation where possible.

## Remove

1. Select an existing PMF-managed Windows Service and enter its password or recovery code.
2. Verify credentials, then confirm removal.
3. Request SCM stop, wait for the stopped state, and unregister with `DeleteService`.
4. Remove the managed service executable and authentication manifest.

Administrative service-management rights remain effective regardless of the PMF password.

## Scope

PMF does not install into SYSTEM, create AV exclusions, use process injection, hide services, or implement self-repair or anti-removal functionality. It cannot convert an arbitrary non-service EXE into a Windows Service.
