# Repository Instructions

These instructions apply throughout this repository. If a more specific instruction file exists in a subdirectory, follow it together with this file. For implementation decisions, the Final Audited Engineering Spec is the governing contract.

## Project scope and authority

- This project implements native Linux AMD Adrenalin support for Radeon RX 6000 and newer.
- FSR reverse engineering is outside this project's scope. Do not add FSR reverse engineering work under this goal.
- Treat `AMD_Adrenalin_Linux_RX6000plus_ENGINEERING_SPEC_FINAL_AUDITED(1).md` as the authoritative implementation contract. It overrides conflicting project documents.
- Follow `LUNA_SWARM_EXECUTION_PROTOCOL.md` and `LUNA_AUTHORITY_SAFETY_REASONING.md` for execution, safety, and review. Use `AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md` as the ticket acceptance and progress record.
- If documents conflict, the Final Audited Engineering Spec wins. Do not silently weaken its requirements to make a ticket appear complete.

## Production requirements

- Build production behavior from real providers, runtime state, and validated contracts. Do not represent stubs, placeholders, fixtures, prepared work, dead UI, or mocked success as implemented capability.
- If a provider, permission, hardware feature, or evidence source is unavailable, report that state clearly and fail safely. Do not invent data or claim parity without evidence.
- Preserve fail-closed safety behavior, least privilege, and the host's distro, kernel, Mesa, firmware, and package-manager state. Keep system installation or host mutation out of scope unless the governing contract explicitly requires it.
- Keep public interfaces, schemas, service identity, generation, event ordering, and error behavior consistent with the audited contract. Coordinate before changing shared seams.

## Filesystem paths

- **NEVER HARDCODE FILESYSTEM PATHS.** Do not add machine-, user-, checkout-, build-, or developer-specific paths to source code, tests, build files, scripts, or documentation. This prohibition includes paths observed in the current environment.
- Derive paths from runtime discovery, explicit configuration, XDG conventions, Qt, CMake, or GNUInstallDirs as appropriate. Tests must use generated temporary directories.
- Fixed paths required by an operating-system or D-Bus protocol may be used only when they are protocol-defined constants and their purpose is documented. Do not confuse protocol constants with local machine paths.
- Before finishing a change, inspect the diff for newly introduced absolute paths and remove any path that is not a documented protocol constant.

## Ticket workflow and progress

- Work the dependency frontier, not ticket number order. Recompute dependencies after each integrated change and pursue every unblocked ticket.
- Read the ticket's full acceptance criteria, dependencies, and relevant audited spec sections before implementation.
- Mark progress in `AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md` and update `docs/orchestration/progress-state.md` as work advances. Record concrete delivered scope, evidence, and remaining blockers.
- Mark a ticket complete only after every acceptance criterion has evidence. A partial implementation remains explicitly partial, even when its focused tests pass.
- Keep evidence precise: distinguish code implementation, local validation, independent review, external reference evidence, hosted CI, and final acceptance. Do not claim a check or result that was not observed.
- When required reference material or hardware is unavailable, document the missing evidence and its effect on acceptance; never fabricate captures, annotations, or parity results.

## Swarm execution

- Use the execution protocol to identify work that is genuinely unblocked. Delegate independent tasks with disjoint file and contract ownership; serialize work that shares schemas, safety state, or architectural seams.
- Give each worker a bounded ticket or review scope, its acceptance criteria, files it owns, and any files it must not change.
- Integrate worker results only after inspecting their diffs and evidence. Resolve conflicts centrally, run the relevant checks, record the outcome in the ticket Markdown, and then recompute the frontier.
- Do not create concurrent implementations of the same contract or treat parallel activity as proof of completion.

## Implementation and validation

- Preserve unrelated working tree changes. Keep each change reviewable and within the project boundary.
- Implement the complete authorized acceptance scope; do not stop at a plan when implementation is possible.
- Use the checks required by the ticket, spec, and execution protocol. Report exact commands and outcomes. Do not add or run unrelated tests solely for activity.
- Review the final diff for contract compliance, unsafe defaults, path literals, inaccurate documentation, and unsupported completion claims before committing.
