# PMF — Persistence Management Framework

PMF is a C++20 project for transparent, authorized Windows application lifecycle management. It is organized around two command-line frontends, `pmf-loader` and `pmf-unloader`, with a shared core.

## Current status

Version 0.1.0 is a project bootstrap. The loader inspects a local Windows executable and validates its label without installing or executing it. The unloader is a placeholder. Neither tool modifies startup configuration, creates services, changes antivirus settings, nor blocks system administrators from removing software.

## Build

Windows prerequisites: CMake 3.20+ and a C++20 compiler (MSVC or Clang).

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## CLI

```powershell
pmf-loader.exe "C:\Path\Example.exe" example
pmf-unloader.exe --help
```

Invoking the loader without arguments prompts for the executable and label.

## Planned design

- Installation manifest, identity, and integrity verification
- Explicit consent and documented installation workflow
- Reversible installation, status, and removal
- Password-gated application workflow with administrator recovery
- Auditable service configuration for applications expressly authorized to run at startup
- Automated tests and signed releases

PMF is not designed for stealth, security-tool exclusion, or prevention of authorized system recovery.