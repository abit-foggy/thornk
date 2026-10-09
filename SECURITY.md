# Security Policy

## Supported Versions

Security updates are applied to the active release stream.

| Version | Supported |
| ------- | --------- |
| Stable  | Yes       |
| nightly | Yes       |
| < current | No      |

## Reporting a Vulnerability

The thornk project takes security vulnerabilities seriously. If you discover a vulnerability or security issue, please do not file a public issue on GitHub.

Please report security vulnerabilities through GitHub Private Vulnerability Reporting:
Navigate to the Security tab of the repository on GitHub, select "Report a vulnerability", and provide the details.

### What to Include in Your Report

To help us triage and resolve the issue quickly, please include:
- A clear description of the vulnerability and its potential impact.
- Affected component: the Kbuild / Kconfig scanners (`src/scan.c`, `src/config.c`), dependency resolution (`src/resolve.c`), code generation (`src/*.pi`, `src/prepare.c`, `src/post.c`, `src/clean.c`), or subprocess handling.
- Step-by-step reproduction instructions, including a sample Kbuild file or kernel tree layout that triggers the issue.
- Target kernel version and architecture (e.g. Linux 6.12 x86_64).
- Any proposed mitigations or proof-of-concept files.

## Response and Disclosure Process

1. **Acknowledgment**: We aim to acknowledge receipt of security reports within 48 hours.
2. **Investigation & Triage**: We will confirm the vulnerability, determine its severity, and provide regular progress updates.
3. **Patch Development**: Fixes are developed in private branches and tested against the kernel compatibility test matrix.
4. **Coordinated Disclosure**: Once a fix is verified and ready for release, we will coordinate public disclosure and publish a security advisory with credit to the reporter.

## Security Architecture & Invariants

thornk is designed with several defensive security principles in mind:
- **Bounded ingestion**: external Kbuild, Kconfig, and Makefile parsing uses bounded, fixed-size buffers; hostile input fails with a diagnostic, never with memory unsafety.
- **Trust model**: kernel trees and their build files are trusted project inputs, the same way `make` trusts a Makefile. thornk never fetches or evaluates files from outside the target tree.
- **Subprocess confinement**: `system()` invocations are confined to declared kernel build toolchain commands, with generated command lines written deterministically from parsed project content.
- **Deterministic generation**: generated wrappers and Ninja specifications are byte-deterministic for identical inputs; no timestamps or hidden environment probing.
- **Output containment**: generated artifacts stay inside the declared build output directory.
- **Inherited invariants**: thornk runs on thorn's embedded engine and pith's runtime, inheriting their ARC safety, bounded ingestion, and no-VM execution guarantees.

## Authorship & Review

The majority of the code in this repository was written by an AI. All
architectural design was made by a human, and every change was
reviewed by both a human and an AI for flaws before it landed.
Security-sensitive components (external file parsing, subprocess
execution, and generated-command construction) receive additional
review scrutiny.
