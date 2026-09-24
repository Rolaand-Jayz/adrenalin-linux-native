# Project execution state

## Objective

Complete the Final Audited Ticket Pack through the Final 1:1 Parity Closure,
using the Final Audited Engineering Spec as the controlling contract.

## Current phase

Ticket 01 — clean-checkout CI build and smoke-test acceptance evidence.

## Completed evidence

- The full checked-in workflow was exercised locally from a fresh `git archive`:
  invalid activation-path rejection, 90/90 build steps, 4/4 CTest suites, 40/40
  manifest tests, staged install verification, systemd and desktop validation,
  live D-Bus activation, and custom desktop-path quoting all passed. Hosted CI
  evidence remains a separate open criterion.
- Ticket 01 was rechecked from a clean `git archive` of the current commit. A
  clean-source CMake/Ninja build completed, and all 4/4 CTest suites passed,
  including the shell smoke and both private-bus suites. The run used Qt 6.11.2,
  GCC 16.2.1, and the workflow's Catch2 3.4.0 package. This proves the local
  clean-source build/test path but does not establish a hosted CI run.
- The in-progress Hardware1 test target built and its focused private-bus CTest
  passed 1/1. Independent review then found a mismatch between the Reply XML
  signature and streamed field order, plus stale mock outputs and incomplete
  signal identity. The slice is not integrated and remains open; Ticket 03 is
  still blocked by Ticket 01 under the audited dependency graph.
- Ticket 01 native Qt shell and initial packaging/bootstrap commits exist.
- The shell smoke test was built in a temporary out-of-tree harness and passed
  on this CachyOS host: QML loaded, the process exited normally with status 0,
  and stderr was empty. This covers the platform smoke criterion, not a
  clean-checkout CI run.
- Ticket 02's deterministic visual-diff harness is committed. Its required
  authentic reference corpus, candidate render, geometry export, reviewed
  annotations, and screen-level passing comparison are still outstanding.
- Ticket 02's manifest is now version 2. `not_applicable` rows must cite a
  checksum-verified captured image from the same context and include reviewer
  and review-record identities. The validator cannot prove that human review
  occurred. The reference harness suite passes 40 tests; authentic captures are
  still absent.
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
  duplicate sequences are ignored. The readiness state event now uses the same
  per-service cursor and publishes the same envelope through a dedicated
  versioned `Service1.EventPublished` signal. Standard `PropertiesChanged`
  carries only values that changed. The readiness client ignores duplicates,
  advances over sequential events from other service families, and refreshes on
  gaps, owner/identity changes, or malformed envelopes.
  The focused private-bus session suite passes 17/17 QtTest totals, and the
  focused readiness-client suite passes 8/8 totals. Other v1 service event
  families still need this envelope.
- Readiness fields now live in a shared `ServiceReadinessContract`, independent
  of Settings1 operations. The session mock implements and tests all fields;
  the focused session-contract suite passes 15/15 totals. A new
  `ServiceReadinessClient` reads the generated `Service1` properties, reconciles
  property/owner changes, rejects unsupported API majors, and discards an
  in-flight stale snapshot before publishing READY. Its focused private-bus
  suite passes 6/6 totals. `Settings1Client` now gates reads, writes, and
  uncertain-operation replay on compatible READY plus a matching service UUID
  and generation. The full private-bus session-contract suite passes 17/17
  totals after this integration. Other long-lived service roots must adopt the
  same contract as they are implemented.
- README and CI distinguish the final logical install prefix from `DESTDIR`
  staging. D-Bus, systemd, and desktop Exec paths are derived from configured
  destinations; the systemd unit directory is discovered through pkg-config.
  The D-Bus service name and root object path are generated from one CMake
  identity definition instead of repeated in runtime code. A local
  RelWithDebInfo staged install verified the activation targets, systemd unit,
  desktop-entry syntax, custom desktop path quoting, and actual D-Bus activation.
- Reference harness checks pass locally (40 tests) and are included in CI.

## Current blockers

- No authentic fixed-reference capture pack or candidate screen artifact is
  present, so Ticket 02 cannot pass its visual parity acceptance gate yet.
- The Ticket 01 hosted CI criterion remains open: this checkout has no
  configured Git remote or current workflow run evidence. The clean local
  archive build and 4/4 CTest result are recorded in the ticket pack.
- The focused private-bus session-contract suite passes 17/17 QtTest totals and
  the focused readiness-client suite passes 8/8. The clean-source full CTest
  run now passes 4/4; a hosted CI run is still unverified.
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
- The project workspace is the real `adrenalin-linux-native` directory. The old
  entry and symlink were removed; repository documentation does not encode its
  machine-specific parent path.

## Next actions

1. Obtain a hosted clean-checkout CI run for Ticket 01. The local clean-source
   build and full CTest pass are recorded but do not close this criterion.
2. Once Ticket 01 is complete, recompute the frontier and resume Tickets 02 and
   03. Ticket 02 requires authentic reference captures and real candidate
   screen/geometry evidence; synthetic fixtures cannot pass its parity gate.
3. For Ticket 03, fix and independently review the in-progress Hardware1
   contract slice before integration. Keep shared schema work serialized, then
   implement the remaining ID-105/107 topology, ID-106 readiness, ID-093 events,
   and ID-185 recovery sequence.
4. Continue implementing, testing, reviewing, committing coherent slices, and
   recomputing the dependency frontier until the Final 1:1 Parity Closure passes.
