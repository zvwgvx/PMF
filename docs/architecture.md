# PMF Architecture

## Components

- `pmf-loader.exe`: standalone Windows CLI using the native Windows file picker, password prompt, and recovery-file picker.
- `pmf-unloader.exe`: standalone Windows CLI for listing and removing managed binary copies with password or recovery authentication.
- `pmf-core`: a static library linked into both executables.

## Workspace

PMF uses the Windows Known Folder API to find `%ProgramData%`, normally `C:\ProgramData`, and maintains binary copies under `PMF\managed\<label>`. It refuses to follow a reparse point at a managed copy destination when removing a package.

Each managed directory holds exactly a copied executable and a text manifest. There is no installation registry entry, Windows service, autorun entry, or antivirus policy change.

## Authentication

A password with at least 12 characters and an independently generated recovery code are required during registration. PMF derives separate PBKDF2-HMAC-SHA256 verifiers using random 128-bit salts and 600,000 iterations. Recovery codes contain 160 bits of secure random data and are stored as a separate text file chosen by the user. The manifest stores only derived hashes and salts.

Password-based removal is a normal workflow control, not a mechanism for preventing the device administrator from recovering or removing files.

## Reliability

- Refuses duplicate installation labels
- Validates executable extension and path
- Verifies original and copied SHA-256 hashes
- Attempts to roll back incomplete managed-copy installation
- Requires explicit confirmation before removing managed copies
- Never modifies the original executable

## Scope

PMF v0.2.0 is a package-management prototype, not a persistence mechanism. It never launches the copied executable, arranges auto-start, creates a service, changes AV settings, or implements anti-removal features.
