# Project execution state

## Objective

Complete the Final Audited Ticket Pack through the Final 1:1 Parity Closure,
using the Final Audited Engineering Spec as the controlling contract.

## Current phase

Ticket 03 — canonical cross-process contract and persisted preference tracer.

## Completed evidence

- Ticket 01 native Qt shell and initial packaging/bootstrap commits exist.
- The shell smoke test was built in a temporary out-of-tree harness and passed
  on this CachyOS host: QML loaded, the process exited normally with status 0,
  and stderr was empty. This covers the platform smoke criterion, not a
  clean-checkout CI run.
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
- A follow-up closes the client-side reconnect event race: changes
  arriving during reads are treated as hints and force another authoritative
  read. An uncertain consent write keeps its operation ID and payload, retries
  that same operation after reconnect, and uses bounded polling while the
  service is not READY. A normal successful write's own change event does not
  cause a duplicate operation; revision gaps also force a snapshot refresh.
  Retry timers skip stale work after recovery and do not mark an in-flight
  request for an unnecessary extra refresh.
- The current Ticket 03 slice now defines the audited v1 result-code vocabulary
  and returns the consent mutation as named D-Bus result fields, including
  operation identity, message key, retryability, provider and subject using the
  spec's snake_case wire names. Exact code-name round trips and result fields
  are covered by the session contract test. Terminal errors preserve the typed
  result for the UI while refreshing authoritative state so a failed write
  does not leave the preference disabled. This does not complete the remaining
  interface topology or operation families.
- A shared `Service1` D-Bus interface now owns the long-lived service readiness
  properties; `Settings1` owns only the settings operation surface. The session
  daemon exports both interfaces from the canonical root object, and integration
  coverage reads readiness through `Service1`.
- `ServiceGeneration` now emits a standard `PropertiesChanged` notification when
  initialization advances readiness. The contract test observes the D-Bus signal
  and checks the published generation; the complete focused session suite passes
  12/12 QtTest totals after this correction.
- The consent-change event now carries the service UUID, generation, monotonic
  sequence, and stable subject. Its read snapshot includes the corresponding
  event cursor, and the client requests an authoritative refresh after sequence
  or revision gaps. UUID/generation/subject mismatch also forces a refresh, and
  duplicate sequences are ignored. The updated private-bus session suite passes
  15/15 totals. Other service event families still need this envelope.
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
- The focused private-bus session suite passes 15/15 QtTest totals with socket
  permissions. The full clean-checkout CMake/CTest workflow is still unverified
  locally because Catch2 v3 is absent.
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

1. Continue the eligible ID-105/107 topology and operation-family frontier,
   keeping shared schema edits serialized and interfaces aligned to the audited
   contract.
2. Complete ID-185 recovery reconciliation and the event envelope for remaining
   service families before advertising READY after restart.
3. Implement Ticket 02 against authentic reference captures and a real candidate
   screen/geometry probe; synthetic fixtures cannot satisfy the parity gate.
4. Re-run focused and cross-component validation, obtain adversarial review,
   commit coherent slices, then recompute the dependency frontier.
