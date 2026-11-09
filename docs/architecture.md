# PMF Architecture

## Components

- `pmf-loader.exe`: a standalone C++ CLI for target inspection and consent-based workspace preparation.
- `pmf-unloader.exe`: a standalone C++ CLI for workspace inspection and future authorized removal.
- `pmf-core`: a static library linked into both executables.

## Workspace

On Windows, the workspace is `%ProgramData%\PMF`, usually `C:\ProgramData\PMF`. The path is resolved from the Windows known-folder API. The loader creates or checks it only after an affirmative response. The core does not change Windows ACLs, antivirus policy, or startup registration.

The project does not install itself into Apps & Features or Windows Programs & Features.

## Boundaries

Version 0.1.0 performs no persistence, arbitrary execution, process injection, antivirus alteration, or anti-removal operations. Removing the workspace remains an administrator-controlled operation.

## Future interfaces

1. Inspect an authorized Windows executable.
2. Present a clear installation scope and obtain consent.
3. Create a verifiable manifest and audit trail.
4. Support authorized removal, including administrator recovery.
5. Apply documented Windows service configuration only for applications explicitly approved to operate as services.
