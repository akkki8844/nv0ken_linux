# nv0ken documentation

This directory is the documentation home for nv0ken. It intentionally keeps
technical references, product direction, contributor guidance, and legal
notices together so a checkout contains the information needed to build,
evaluate, and extend the project.

## Start here

| Document | Purpose |
| --- | --- |
| [Product requirements](PRD.md) | Product scope, supported targets, quality goals, and acceptance criteria. |
| [Building](building.md) | Toolchain prerequisites and build workflow. |
| [Architecture](architecture.md) | Kernel and user-space subsystem overview. |
| [Debugging](debugging.md) | QEMU, serial, and GDB debugging workflow. |
| [Roadmap](roadmap.md) | Development milestones and outstanding work. |
| [Release process](RELEASE_PROCESS.md) | Rules for producing a reproducible release image. |

## Technical references

- [Memory layout](memory_layout.md)
- [System call table](syscall_table.md)
- [Driver model](driver_model.md)
- [VFS interface](vfs_interface.md)
- [Compositor IPC protocol](ipc_protocol.md)

## Project and legal documents

- [Contributing](CONTRIBUTING.md)
- [Code of conduct](CODE_OF_CONDUCT.md)
- [Security policy](SECURITY.md)
- [Copyright notice](COPYRIGHT.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)
- [MIT license](../LICENSE)

## Documentation rules

Keep documentation aligned with code that has been built and tested. State an
interface as implemented only when the corresponding source is present and the
relevant build target succeeds. Planned or partially integrated features belong
in the roadmap or PRD, not in implementation claims.
