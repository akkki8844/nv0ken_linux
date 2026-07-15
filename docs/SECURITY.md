# Security policy

## Scope

nv0ken is an educational operating-system project and is not represented as a
secure production platform. Memory safety, privilege boundaries, parsers,
filesystem code, network code, boot packaging, and third-party dependencies are
all security-relevant and should be treated accordingly.

## Reporting a vulnerability

Do not open a public issue for a vulnerability that could enable data loss,
privilege escalation, arbitrary code execution, insecure boot behavior, or
remote compromise. Instead, use GitHub's private security reporting feature for
this repository. If it is unavailable, contact a repository maintainer through
a private GitHub channel and include:

- A concise description and affected revision.
- Reproduction steps or a minimal proof of concept.
- Impact, prerequisites, and any suggested mitigation.
- Whether disclosure is time-sensitive.

Please avoid accessing data that you do not own, attacking third-party systems,
or publishing exploit details before maintainers have had a reasonable chance
to investigate.

## Response goals

Maintainers should acknowledge reports, reproduce them in an isolated emulator,
assess impact, prepare a fix and regression test where feasible, and publish a
coordinated advisory after a fix is available. Response timing is best effort;
the project does not promise a support SLA.

## Supported revisions

Security fixes, when available, target the current `main` branch. Historical
snapshots and generated release images are not guaranteed to receive patches.
