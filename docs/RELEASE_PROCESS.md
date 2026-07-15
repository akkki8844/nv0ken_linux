# Release process

## Release objective

A nv0ken release is a source revision accompanied by a reproducible bootable
ISO and clear validation evidence. A release must describe what was actually
tested; it must not imply hardware compatibility or security guarantees that
were not verified.

## Preparation

1. Start from a clean working tree on the intended commit.
2. Confirm the documented host prerequisites from [building.md](building.md).
3. Rebuild the ISO from source with the documented toolchain.
4. Verify that the kernel is an x86_64 ELF executable and that the initrd
   contains only intended runtime files.
5. Inspect ISO contents for `/kernel.elf`, `/initrd.tar`, the Limine
   configuration, and the EFI boot file.

## Runtime validation

Boot the ISO in headless QEMU with serial output captured. Record the command,
host toolchain versions, commit hash, and the last known-good serial milestone.
For any changed boot, memory, scheduler, driver, filesystem, network, or ABI
path, perform a focused smoke test of the changed path in addition to booting.

## Publication checklist

- `git diff --check` is clean.
- Generated files are excluded from the source commit.
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) is current.
- The README, PRD, and roadmap distinguish validated behavior from planned work.
- Release notes list build command, validation performed, known limitations, and
  hashes for published images where images are distributed.

## Rollback

If a released image fails to boot, withdraw or mark the release as broken,
publish the last known-good revision, and diagnose the regression in a new
focused change. Do not silently replace a release artifact without documenting
the revision and hash change.
