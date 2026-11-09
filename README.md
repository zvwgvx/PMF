# PMF — Persistence Management Framework

PMF is a C++20 project for explicit, user-authorized Windows executable management. The Windows frontends are native graphical setup and removal wizards rather than terminal programs.

## Windows applications

- \`pmf-loader.exe\` opens a multi-step setup wizard to select a local Windows executable through Browse, specify a management label, enter and confirm a password, choose where to save a recovery text file, review the operation, and create a managed copy.
- \`pmf-unloader.exe\` opens a removal wizard to select an installed PMF-managed copy, authenticate using its password or recovery code, review the operation, and delete that managed copy.
- Both programs are compiled as Windows GUI subsystem executables. They do not open a command prompt or register themselves in Windows Apps & Features.
- PMF requires no .NET runtime. The MSVC release build uses a statically linked C runtime, and the executables use native Win32 APIs.

## Current operation

PMF stores each managed copy under \`%ProgramData%\PMF\managed\<label>\payload.exe\`, with \`manifest.txt\` beside it. The original executable is never modified.

Authentication uses separate salts and PBKDF2-HMAC-SHA256 password verifiers. Every package receives a unique randomly generated recovery code saved as a text file at a path chosen by the user. Executable integrity is checked with SHA-256.

PMF does not execute the managed executable, register an autorun task or Windows service, alter antivirus exclusions, or prevent removal by a system administrator. A management password restricts the normal PMF unloader workflow only.

## Build

On Windows, install CMake 3.20+ and MSVC C++ Build Tools.

~~~powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Run \`build\Release\pmf-loader.exe\` or \`build\Release\pmf-unloader.exe\` by double-clicking. A console is not required.

## User interface

The loader has Welcome, Select Program, Security, Review, and Finish pages. The unloader has Select Program, Authentication, Review, and Finish pages. Each wizard uses Back, Next, and Cancel controls with explicit confirmation before changing files.

## Storage and recovery

The recovery text contains a credential that authorizes removal of its corresponding managed package. Save it outside the PMF directory and protect it like a password. If PMF cannot write to \`%ProgramData%\PMF\`, run the wizard with the necessary Windows folder permissions.

## Safety boundaries

This is a managed-copy prototype, not a stealth or anti-removal persistence system. No antivirus exclusion is created or required, and the administrator remains in control of the computer.
