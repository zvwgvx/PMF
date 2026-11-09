# PMF — Persistence Management Framework

PMF is a C++20 Windows project for transparent, consent-based management of local executable packages. It consists of two portable, standalone command-line binaries: `pmf-loader.exe` and `pmf-unloader.exe`.

## Current capabilities

- Browse for a Windows PE executable in the native Windows file selection dialog, or provide the executable path on the command line
- Choose a unique installation label
- Explicitly approve the managed-copy operation
- Set a per-label password of at least 12 characters
- Create a cryptographically random recovery code in a text file at a user-selected path
- Copy the selected EXE to `%ProgramData%\PMF\managed\<label>\payload.exe`
- Verify SHA-256 of the original and copied EXE
- Record salted PBKDF2-HMAC-SHA256 password and recovery-code verifiers
- List managed copies and remove one using its password or recovery code

PMF never launches the managed executable, adds startup persistence, changes antivirus settings, or prevents an administrator from removing its files. It does not register an installed app entry in Windows Apps & Features. The original selected executable is not altered or removed.

## Build on Windows

Requires CMake 3.20+ and Visual Studio 2022 C++ tools.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The MSVC build uses the static runtime and links against built-in Windows libraries. Each released CLI is an independent executable.

## Usage

```powershell
.\build\Release\pmf-loader.exe
.\build\Release\pmf-loader.exe "C:\Path\MyProgram.exe" my-program
.\build\Release\pmf-unloader.exe --list
.\build\Release\pmf-unloader.exe my-program
.\build\Release\pmf-unloader.exe --status
```

Invoking `pmf-loader.exe` without arguments opens a Browse dialog. After approval it prompts for a password and Save As location for the recovery text file.

Invoking `pmf-unloader.exe` without arguments lists labels and prompts for the application to remove. You may authenticate using either its password or its recovery code.

## Data format

A managed copy is stored in `%ProgramData%\PMF\managed\<label>` with `payload.exe` and `manifest.txt`. The manifest stores the label, executable digest, unique salts, and derived verifier hashes. Passwords and recovery codes are not stored in the manifest in plaintext.

**Recovery text files are secrets.** PMF asks you to place each recovery file outside the managed directory. Store it securely and do not distribute it with the managed copy. A recovery code authorizes normal PMF removal; it does not replace administrator recovery rights.

## Non-goals

PMF does not provide stealth, process injection, C2/implant deployment, security-tool bypass, auto-start persistence, or tamper-resistant removal. Antivirus exclusions are never created or required.
