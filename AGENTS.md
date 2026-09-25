# Repository instructions

## Authority and scope

- Treat `AMD_Adrenalin_Linux_RX6000plus_ENGINEERING_SPEC_FINAL_AUDITED(1).md` as the authoritative implementation contract. If another document conflicts with it, the audited spec wins.
- Follow `AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md`, `LUNA_SWARM_EXECUTION_PROTOCOL.md`, and `LUNA_AUTHORITY_SAFETY_REASONING.md` for ticket acceptance, execution order, delegation, and safety boundaries.
- This repository implements native Linux AMD Adrenalin support for RX 6000 and newer. FSR reverse engineering is outside this project scope.

## Production requirements

- **NEVER HARDCODE FILESYSTEM PATHS.** Do not hardcode machine-specific, user-specific, checkout-specific, build-host, workspace, or developer-home paths anywhere in source, tests, build files, scripts, or documentation. This includes paths that happen to be known in the current environment.
- Derive paths from runtime discovery, configuration, XDG conventions, Qt, CMake, or GNUInstallDirs as appropriate. Tests must use generated temporary directories. Fixed paths required by an operating-system or D-Bus protocol are allowed only as documented protocol constants, not as project-location shortcuts.
- Before marking path-related work complete, search changed files for absolute paths and explain any protocol-required constants. Never solve a path problem with a hardcoded checkout path or symlink workaround.
- Do not report a capability as complete when it is unavailable, unimplemented, only prepared, or only represented by a placeholder. Production behavior must fail clearly and safely when required evidence or a backend is unavailable.
- Preserve distro, kernel, Mesa, firmware, and package-manager state. Implement user-space behavior within the approved project boundary.
- Keep privacy, least privilege, read-only discovery, and fail-closed behavior consistent with the audited authority and safety documents.

## Ticket execution

- Work the dependency frontier described by the execution protocol. Parallelize independent tickets with clear ownership; serialize changes that share contracts, schemas, safety state, or architectural seams.
- Before implementation, read the relevant ticket acceptance criteria and the governing spec sections. Implement the complete acceptance scope, then run the strongest applicable checks and obtain the required review.
- Record concrete progress and evidence in the ticket Markdown. Mark a ticket complete only when every acceptance criterion has evidence; otherwise record the completed sub-scope and remaining blockers without overstating status.
- Keep changes reviewable, avoid unrelated edits, and do not add tests unless requested by the user or required by the ticket/spec acceptance contract.
- Keep the ticket pack and `docs/orchestration/progress-state.md` current as work proceeds; record exact checks and outcomes, and distinguish completed sub-scopes from accepted tickets.
