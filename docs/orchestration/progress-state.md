# Project execution state

## Objective

Complete the Final Audited Ticket Pack through the Final 1:1 Parity Closure,
using the Final Audited Engineering Spec as the controlling contract.

## Current phase

Ticket 03 — canonical cross-process contract and persisted preference tracer.

## Completed evidence

- Ticket 01 native Qt shell and initial packaging/bootstrap commits exist.
- Ticket 02's deterministic visual-diff harness is committed. Its required
  authentic reference corpus, candidate render, geometry export, reviewed
  annotations, and screen-level passing comparison are still outstanding.
- A follow-up harness commit adds the required confirmation capture state and a
  manifest-backed failure-only comparison test; synthetic data still cannot
  pass a parity gate.
- Ticket 03 has a persisted consent tracer, generated Settings1 bindings,
  recovery-gated readiness, and reconnect/reconciliation tests in the working
  tree. It is not complete: the implementation currently exposes only one of
  the required core interfaces and omits the other operation families fixed by
  spec IDs 105–110.
- README and CI distinguish the final logical install prefix from `DESTDIR`
  staging. D-Bus, systemd, and desktop Exec paths are derived from configured
  destinations; the systemd unit directory is discovered through pkg-config.
  The D-Bus service name and root object path are generated from one CMake
  identity definition instead of repeated in runtime code. A local
  RelWithDebInfo staged install verified the activation targets, systemd unit,
  desktop-entry syntax, custom desktop path quoting, and actual D-Bus activation.
- Reference harness checks pass locally (36 tests) and are included in CI.

## Current blockers

- No authentic fixed-reference capture pack or candidate screen artifact is
  present, so Ticket 02 cannot pass its visual parity acceptance gate yet.
- Local full CMake test configuration currently lacks Catch2 v3. CI installs
  Catch2, but the full CTest run has not been observed in this environment.
- In this sandbox, an offscreen shell run cannot finish because session D-Bus
  access blocks during client startup; `dbus-run-session` cannot bind its
  socket here. CI provides the required session-bus test environment.
- Ticket 03 requires central completion of the v1 interface topology and
  operation families before it can be considered complete.

## Decisions and constraints

- Product scope is the AMD Adrenalin Linux RX 6000+ application. This project
  does not perform FSR reverse engineering.
- Do not encode machine-specific filesystem paths. Resolve storage via XDG/Qt
  locations, build/install destinations via CMake/GNUInstallDirs, and temporary
  work locations from the environment or runtime APIs.
- Fixed D-Bus service names and object paths remain protocol identities required
  by the engineering spec; they are not filesystem locations.
- The user's path-portability requirement applies from bootstrap onward.

## Next actions

1. Integrate the reviewed ASCII-whitespace path guard, configure-time prefix
   documentation, and path-derived D-Bus service discovery into the Ticket 03
   slice; retain the successful production build/staged-install evidence.
2. Centralize and review the complete v1 service schema before adding more
   independent D-Bus contract variants.
3. Re-run focused Ticket 03 tests after schema integration, then obtain
   independent adversarial review and commit only a coherent verified slice.
4. Resume the dependency frontier and record acceptance evidence per ticket.
