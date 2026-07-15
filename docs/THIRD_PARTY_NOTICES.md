# Third-party notices

## Purpose

This file records dependencies and externally sourced materials that require
separate attribution or license review. It is a living inventory: a dependency
must not be treated as cleared merely because it is listed here.

## Build and runtime dependencies

| Component | Role | Distribution status in this repository |
| --- | --- | --- |
| Limine | Boot protocol support and ISO boot files | Retrieved or prepared by build tooling; review the versioned Limine source and license used by the build. |
| QEMU | Emulator used for development and validation | External development tool; not distributed in this repository. |
| LLVM/Clang, lld, NASM, GNU binutils | Host build toolchain | External development tools; not distributed in this repository. |

## Assets and imported code

Before adding a font, icon, image, bootloader artifact, code snippet, or binary
asset, record its source URL or package/version, copyright holder, exact license,
and any required attribution here or beside the asset. Do not import material
whose license conflicts with the repository's MIT distribution model.

## Verification before release

The release owner must review the build output and confirm that it does not
bundle an unreviewed third-party binary or asset. If tooling fetches a
third-party component, record the version or revision used in the release notes.
