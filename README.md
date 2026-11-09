# PMF — Persistence Management Framework

PMF is a native C++20 Windows Service installation and removal framework. It consists of two independent graphical executables, `pmf-loader.exe` and `pmf-unloader.exe`, and does not register itself in Windows Apps & Features.

## Windows Service workflow

The loader opens a five-step Win32 setup wizard: review permissions, browse for an existing Windows Service EXE, choose a unique label, set an independent password, save a recovery text file, and confirm registration.

The UAC manifest requests administrator approval. PMF validates the PE file structure, copies it to a managed location in Program Files, registers it using Windows Service Control Manager, and starts it. Services are named `PMF-<label>`, use automatic startup and `SERVICE_WIN32_OWN_PROCESS`, and run under `NT AUTHORITY\LocalService` — not SYSTEM.

The executable must already implement the Windows Service API. PMF does not turn ordinary console or GUI EXEs into Windows Services. SCM startup validation fails and installation is rolled back when the selected executable cannot start as a service.

The unloader displays PMF-managed services, accepts a per-service password or recovery code, asks for confirmation, stops and unregisters the service, then removes PMF-managed files. It does not modify the original executable or its recovery text file.

## File locations

Service executable: `C:\Program Files\PMF\Services\<label>\service.exe`

Authentication metadata: `C:\ProgramData\PMF\managed\<label>\manifest.txt`

Passwords and recovery codes are stored only as separate salted PBKDF2-HMAC-SHA256 verifiers. Recovery codes contain 160 bits of cryptographic randomness and are saved in a user-selected text file outside PMF storage.

## Build

Windows prerequisites are CMake 3.20+ and Visual Studio C++ Build Tools.

~~~powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Double-click `build\Release\pmf-loader.exe` or `build\Release\pmf-unloader.exe` to launch the graphical wizard. Both management programs are standalone Windows GUI binaries with statically linked MSVC runtime. `build\Release\pmf-sample-service.exe` is a separate, no-op development fixture.

## Security boundaries

- Service creation and removal are subject to Windows administrator consent.
- The installed service runs as LocalService with limited permissions.
- Services remain visible and removable through Windows SCM and administrative tooling.
- PMF passwords protect only the normal application management workflow. Administrators can override PMF.
- PMF does not modify AV settings, create exclusions, hide processes, install drivers, or provide anti-removal.
- Selected service EXEs must provide a working service entry point and required dependencies; a PE header check alone is not proof of service compatibility.
- Legacy v0.2/v0.3 managed copies are not automatically migrated.
