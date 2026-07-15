# Contributing to nv0ken

## Before changing code

1. Read [architecture.md](architecture.md) and the interface document for the
   subsystem you intend to change.
2. Check for an `AGENTS.md` file in the relevant source tree; its instructions
   apply to that subtree.
3. Keep one logical change per commit. Do not mix generated images or unrelated
   cleanup with functional changes.

## Development workflow

1. Start from current `main` and inspect the working tree before editing.
2. Make focused source changes with clear ownership boundaries.
3. Build the narrowest affected target first, then the ISO when boot or initrd
   contents are affected.
4. Run `git diff --check` before sharing a change.
5. Update documentation when changing a public ABI, build command, format, or
   user-visible behavior.

## Code expectations

- Kernel code is freestanding: do not introduce host libc or host syscall
  dependencies.
- Validate untrusted lengths, addresses, and user pointers at subsystem
  boundaries.
- Make error paths explicit and preserve the project's established errno and
  return-value conventions.
- Avoid hidden allocations and implicit global state in interrupt, scheduler,
  and early-boot code.
- Use descriptive names and keep headers as the source of truth for shared
  structures and prototypes.
- Do not add large features by copying code with an unknown license.

## Testing expectations

| Change type | Minimum validation |
| --- | --- |
| Isolated library or application | Build that component and exercise its affected path where practical. |
| Kernel or ABI change | Build the kernel and any dependent user-space targets. |
| Initrd or package change | Rebuild the ISO and inspect the archive/image contents. |
| Boot-path change | Headless QEMU boot with captured serial output. |
| Documentation-only change | Check Markdown links and `git diff --check`. |

## Commit and review checklist

- The commit message states the outcome, not just the files edited.
- No build artifacts, editor state, keys, or machine-local paths are staged.
- The diff contains no unrelated formatting churn.
- Known limitations and skipped validation are stated honestly in the change
  description.
- A reviewer can reproduce the listed validation commands from a clean checkout.
