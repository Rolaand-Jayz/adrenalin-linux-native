# Contributing to Adrenalin Linux

Adrenalin Linux is an independent, early-stage project. It explores a native Qt/QML Linux desktop for workflows inspired by AMD Software: Adrenalin Edition, for Radeon RX 6000 and newer GPUs. It is not affiliated with or endorsed by AMD.

The project is built by AI coding agents working under the maintainer's direction. The rules those agents follow are written down in this repository. They apply to every contributor, human or AI agent.

## Project status

The current build is not a GPU control utility. It has a native application shell, a per-user session service, persisted settings and profile data, and read-only hardware inventory. GPU control providers are not implemented. No screen has passed the visual-parity gate. There are no release packages and no certified GPU or distribution configurations.

See the [README](README.md) for the current state.

## Before you start

Read these first. Contributions are reviewed against them:

- [`AGENTS.md`](AGENTS.md): repository rules for any contributor.
- [Final audited engineering spec](AMD_Adrenalin_Linux_RX6000plus_ENGINEERING_SPEC_FINAL_AUDITED%281%29.md): the governing implementation contract. If documents conflict, the spec wins.
- [Final audited ticket pack](AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md): ticket acceptance criteria and the progress record.
- [`LUNA_AUTHORITY_SAFETY_REASONING.md`](LUNA_AUTHORITY_SAFETY_REASONING.md): authority order, truthfulness, safety invariants, and the completion standard.
- [`LUNA_SWARM_EXECUTION_PROTOCOL.md`](LUNA_SWARM_EXECUTION_PROTOCOL.md): how work is picked, integrated, and recorded.
- [`docs/orchestration/progress-state.md`](docs/orchestration/progress-state.md): the current phase and open work.

## Ways to contribute

- **Report a bug.** Use the *Bug report* form. Include your full environment; nothing is certified yet, so every report needs it.
- **Suggest a feature.** Use the *Feature request* form. Scope is set by the engineering spec. Say which Adrenalin screen or behavior the request relates to.
- **Work on a ticket.** Pick from the dependency frontier in the ticket pack: tickets whose blockers are complete. Do not pick by ticket number.
- **Correct a record.** If a document overstates progress or evidence, open an issue or a pull request with the correction and its source.

## Safety rules

The project's goal includes changing GPU and display settings. These rules apply to every change, including ones that touch no hardware today:

1. **Fail closed.** If a provider, permission, hardware feature, or evidence source is unavailable, report that state clearly. Do not invent data.
2. **Verify before reporting success.** A change succeeds only when its effective state can be read back or otherwise verified. UI success follows verified backend success.
3. **Leave the host alone.** Kernel, Mesa, firmware, graphics-stack packages, the compositor, and package-manager state belong to the distribution. The app may report their status. It does not install, remove, or replace them.
4. **Keep least privilege.** There is one elevated service. Keep polkit boundaries and authenticated caller identity. Privileged mutations are semantic and revalidated.
5. **Make risky display changes recoverable.** Get authorization before the change. Use a confirmation timer (15 seconds unless the reference capture sets another value). Rollback must work without a second prompt and without the GUI staying alive.
6. **Make tuning transactional and bounded.** Limits come from discovered device capability, not a hardcoded GPU table. Snapshot, validate, write in order, verify, and roll back on failure. Write the recovery journal before the first dangerous write.
7. **Never apply risky tuning silently.** Persistent tuning is opt-in. The first dangerous tuning change needs a risk acknowledgement. A profile linked to instability is marked suspect and is never re-applied automatically.
8. **Keep hardware protections on.** Stress tests never disable thermal, current, or power protections.
9. **Do not claim support from a name.** No feature becomes supported from a GPU-name check alone.
10. **Keep data local.** Core features need no cloud account. No product telemetry leaves the machine without explicit opt-in.

The full list of safety invariants is in [`LUNA_AUTHORITY_SAFETY_REASONING.md`](LUNA_AUTHORITY_SAFETY_REASONING.md). A safety failure blocks a ticket from being marked complete.

## Pull request rules

1. **Read the ticket and the spec.** Read the ticket's full acceptance criteria, its dependencies, and the relevant spec sections before you start.
2. **No stubs or fake success.** Do not present stubs, placeholders, fixtures, mocked success, or dead UI as working features.
3. **Never hardcode filesystem paths.** Derive paths from runtime discovery, explicit configuration, XDG conventions, Qt, CMake, or GNUInstallDirs. Tests use generated temporary directories. A fixed path is allowed only as a documented operating-system or D-Bus protocol constant. Check your diff for new absolute paths before you open the pull request.
4. **Coordinate shared contracts.** Changes to D-Bus interfaces, database schemas, the telemetry ABI, capability representation, or recovery protocols need agreement first. Open an issue before changing them.
5. **Do not guess Adrenalin behavior.** The fixed reference is AMD Software: Adrenalin Edition 26.9.1 Optional. Where the spec requires a reference capture, use the [reference-capture workflow](docs/reference-capture/README.md). Do not substitute behavior from a newer release or from a similar Linux feature.
6. **Record progress honestly.** Update the ticket pack and `docs/orchestration/progress-state.md`. Partial work stays marked partial. Keep local validation, independent review, and hosted CI separate. Do not claim a check you did not observe. Never fabricate captures, annotations, or parity results.
7. **Keep brand assets out.** Do not add proprietary AMD images or assets.
8. **Stay in scope.** Keep each change small and reviewable, and leave unrelated work alone. FSR reverse engineering is outside this project.

## Build and test

Requirements, from the [README](README.md):

- CMake 3.24 or newer
- Ninja
- A C++20 compiler
- Qt 6.8 or newer: Core, DBus, Gui, Qml, Quick, Quick Controls 2, and Sql
- Catch2 v3 and Qt Test
- pkg-config and systemd development metadata for session-service installation
- D-Bus and `dbus-run-session` for the private-bus integration tests

Set a final logical installation prefix for your environment, then build, test, and stage:

```sh
: "${INSTALL_PREFIX:?Set the final logical installation prefix}"
build_dir="$(mktemp -d)"
stage_root="$(mktemp -d)"

cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
QT_QPA_PLATFORM=offscreen "$build_dir/adrenalin-shell" --smoke
DESTDIR="$stage_root" cmake --install "$build_dir"
```

The CI workflow also checks the reference manifest and comparator contracts:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/reference/test_manifest.py
```

CI runs on every push and pull request (Ubuntu 24.04, Qt 6.8). In your pull request, list the exact commands you ran and their results.

## Commit messages

Use a short prefix that says what changed: `feat:`, `fix:`, `test:`, `docs:`, `build:`, `refactor:`, `chore:`, `tools:`. Explain *why* in the body when it isn't obvious.

## License

The repository does not have a license file yet.

## Conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md).
