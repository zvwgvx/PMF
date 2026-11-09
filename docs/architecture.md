# PMF Architecture

## Graphical user interfaces

\`src/ui/wizard.cpp\` implements a shared native Win32 wizard UI. Two Windows GUI subsystem frontends call the shared interface:

- \`pmf-loader.exe\` — five pages: Welcome, Select Program, Security and Recovery, Review, Finish.
- \`pmf-unloader.exe\` — four pages: Select Program, Authentication, Review, Finish.

The interface uses standard Win32 controls and file picker dialogs, with no CLR, .NET, Qt, Electron, or MSI runtime dependency. The binaries do not add an Apps & Features entry.

## Managed storage

A managed executable is copied to \`%ProgramData%\PMF\managed\<label>\payload.exe\` with its SHA-256 integrity metadata and separate salted password/recovery code verifiers. The original executable stays unchanged. Recovery code material is saved to a user-selected text file outside the PMF workspace.

## Core library

\`pmf-core\` exposes identity validation, workspace preparation, managed package installation/removal and credential verification. The shared wizard invokes this core without changing its authorization model.

## Limitations

PMF does not execute PE files, configure startup, create services, change antivirus policies, conceal binaries, or prevent administrative removal. The package manager remains a reversible and auditable local file-management tool.
