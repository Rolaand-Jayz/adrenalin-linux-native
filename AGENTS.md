# Repository Instructions

## Authority and scope

- Use `AMD_Adrenalin_Linux_RX6000plus_ENGINEERING_SPEC_FINAL_AUDITED(1).md` as the authoritative implementation contract. It overrides conflicting project documents.
- Follow `LUNA_SWARM_EXECUTION_PROTOCOL.md`, `LUNA_AUTHORITY_SAFETY_REASONING.md`, and `AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md` for dependency ordering, safety, acceptance, and evidence.
- This repository implements native Linux AMD Adrenalin support for RX 6000 and newer. FSR reverse engineering is out of scope.

## Production and path requirements

- **NEVER HARDCODE FILESYSTEM PATHS.** Do not add machine-, user-, checkout-, build-, or developer-specific paths to source, tests, build files, scripts, or documentation. This includes paths known from the current environment.
- Derive runtime paths through platform discovery, configuration, XDG conventions, Qt, CMake, or GNUInstallDirs. Tests must use generated temporary directories. Fixed paths required by an operating-system or D-Bus protocol are permitted only when documented as protocol constants.
- Do not report unavailable, unimplemented, prepared, or placeholder behavior as complete. Missing evidence or backends must fail clearly and safely.
- Preserve host distro, kernel, Mesa, firmware, and package-manager state. Keep changes within the project boundary and follow least privilege and fail-closed safety rules.

## Ticket execution

- Work the dependency frontier, not numeric order. Delegate independent unblocked tickets with disjoint ownership; serialize work sharing contracts, schemas, safety state, or architecture seams.
- Read relevant acceptance criteria and audited spec sections before implementation. Complete the acceptance scope, run the strongest required checks, and obtain independent review where the protocol requires it.
- Record concrete progress and evidence in the ticket Markdown and `docs/orchestration/progress-state.md`. Mark a ticket complete only when every acceptance criterion has evidence; otherwise record the delivered sub-scope and remaining blockers.
- Keep changes reviewable, preserve unrelated work, and do not add tests unless requested or required by ticket/spec acceptance.
- Record exact checks and outcomes, distinguishing implementation, local checks, external reference evidence, independent review, and full acceptance.
