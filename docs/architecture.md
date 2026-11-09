# PMF Architecture

## Executables

- `pmf-loader`: interactive target inspection, validation, and future consent-first installation workflow.
- `pmf-unloader`: future authorized removal workflow.
- `pmf-core`: reusable policy and identity library.

## Boundaries

Version 0.1.0 performs no persistence, arbitrary execution, process injection, antivirus alteration, or anti-removal operations.

## Future interfaces

1. Inspect a prospective Windows executable and validate its identity.
2. Present the installation scope and request explicit approval.
3. Record a verifiable installation manifest and audit trail.
4. Support removal through authenticated application management, with an administrative recovery path.
5. Use documented Windows service controls only for software expressly installed as a service.

The installation and removal lifecycle must remain auditable and reversible by the device owner.
