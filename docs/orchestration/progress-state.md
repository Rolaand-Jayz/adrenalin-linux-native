# Project execution state

## Objective

Complete the Final Audited Ticket Pack through the Final 1:1 Parity Closure,
using the Final Audited Engineering Spec as the controlling contract.

## Current phase

Tickets 02 and 03 — Ticket 01's hosted-CI gate is complete on both branches.
Ticket 02's remaining gates require authentic reference evidence; Ticket 03
remains the active implementation frontier.

## Completed evidence

- The full checked-in workflow was exercised locally from a fresh `git archive` of baseline commit `b8dc833`:
  invalid activation-path rejection, 90/90 build steps, 4/4 CTest suites, 40/40
  manifest tests, staged install verification, systemd and desktop validation,
  live D-Bus activation, and custom desktop-path quoting all passed. The hosted-CI
  criterion was later completed on both branches at `f6037f0` (see ticket evidence).
- Commit `9561725` was rebuilt from a fresh `git archive`: all 101/101 Ninja
  steps succeeded and all 5/5 CTest suites passed, including the shell CLI,
  Settings1/Service1 private-bus tests, and the new Hardware1 private-bus test.
  This clean-source run used Qt 6.11.2, GCC 16.2.1, and Catch2 3.4.0. It does not
  establish a hosted CI run.
- The Hardware1 wire/mock/test slice now has a matching Reply XML signature and
  serialized field order, typed subject fields, failure-path output clearing, and
  ordered seven-field event declarations for both inventory and capability
  signals. Independent lead review confirms these bounded repairs. The latest
  focused build passed, and the private-bus CTest passed 1/1 after the final
  event assertion update.
- A read-only libdrm PCI inventory source is now implemented as isolated
  ID-104 groundwork. It bounds enumeration, validates PCI evidence, filters
  AMD vendor IDs, canonicalizes transient BDF evidence, rejects duplicates,
  and releases libdrm records on every path. The source validation test is
  wired into project CTest and passes 1/1 (10 test cases). The test-enabled
  configure used a temporary Catch2 package shim because Catch2 v3 is not
  installed; only the DRM target was built. The full production app/sessiond
  build with tests disabled passed. `/dev/dri` is not exposed in this host,
  so runtime hardware enumeration was not verified. The source remains
  unconsumed by SessionService; no production Hardware1 provider is claimed.
- Independent review confirms Hardware1 is still not exported by the production
  Session1 service; production event emission, ID-107's remaining operation
  families, event gap reconciliation, and ID-185 startup recovery remain open.
  The corrected test-only mock is not used as runtime hardware data.
- Ticket 01 native Qt shell and initial packaging/bootstrap commits exist.
- The shell smoke test was built in a temporary out-of-tree harness and passed
  on this CachyOS host: QML loaded, the process exited normally with status 0,
  and stderr was empty. This covers the platform smoke criterion, not a
  clean-checkout CI run.
- Ticket 02's deterministic visual-diff harness is committed. Candidate capture
  tooling now records the live Qt Quick render and named component rectangles,
  with a comparator-compatible rectangle shape and fail-closed bounded CLI.
  A live 1280x800 candidate PNG and 11-component geometry export were emitted and
  structurally checked in-session; those temporary artifacts are not golden
  evidence. The export remains application-self-reported and cannot satisfy
  independent geometry attestation. The authentic reference corpus, reviewed
  annotations, and passing screen comparison remain open.
- Ticket 02's manifest is now version 2. `not_applicable` rows must cite a
  checksum-verified captured image from the same context and include reviewer
  and review-record identities. The validator cannot prove that human review
  occurred. The reference harness suite passes 40 tests; authentic captures are
  still absent.
- Ticket 02's JSON evidence readers now reject duplicate keys at every object
  depth across manifests, comparison checks, masks, and candidate geometry, so
  ambiguous provenance fields fail closed. The focused reference suite passes
  43/43. This improves validation integrity but does not supply authentic
  reference captures or independently attested candidate geometry.
- A follow-up harness commit adds the required confirmation capture state and a
  manifest-backed failure-only comparison test; synthetic data still cannot
  pass a parity gate.
- `docs/reference-capture/README.md` and `worksheets.md` now provide the operator
  workflow for collecting authentic 26.9.1 evidence, including all six contexts,
  applicable states, observations, checksum bindings, independent review, and
  comparison commands. The capture operator still needs access to the fixed Windows
  reference and an independent reviewer; no evidence corpus or parity pass exists.
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

- No authentic fixed-reference capture pack, independently attested candidate
  geometry export, reviewed annotations, or passing screen comparison is present,
  so Ticket 02 cannot pass its visual parity acceptance gate yet. The verified
  candidate PNG and self-reported geometry were written to the temporary output
  area for this session and were not added as reference evidence.
- Ticket 01 is complete: hosted clean-checkout runs on both pushed branches at
  `f6037f0` passed the GUI build, CTest 7/7 including shell smoke, manifest checks,
  staged install validation, D-Bus activation, and custom-prefix quoting. See [main
  run #5](https://github.com/Rolaand-Jayz/adrenalin-linux-native/actions/runs/36069938259)
  and [work run #6](https://github.com/Rolaand-Jayz/adrenalin-linux-native/actions/runs/36069940024).
- The hosted full CTest run at `f6037f0` passes 7/7 on both branches, including
  the shell smoke, session/readiness/Hardware1 contracts, and Telemetry1 fixtures.
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
- On 2026-09-24, the owner authorized continued implementation/review beyond
  Ticket 01 and directed that both `main` and the working checkout branch be
  pushed. Hosted CI passed on both branches at `f6037f0`; the subsequent docs
  commit `ba6aaa7` is also pushed to both. Ticket 01 is complete; Tickets 02 and
  03 remain the active frontier with their evidence and architecture criteria open.

## Next actions

1. Ticket 02 local harness and operator workflow are ready; keep its authentic
   reference capture, independent geometry attestation, reviewed annotation, and
   passing parity gates open until external evidence arrives.
2. Advance Ticket 03 from the isolated libdrm source to production Hardware1 with
   audited stable identities, truthful provider-backed fields, and production
   private-bus coverage. Keep Telemetry1 fixtures separate from production.
3. Resolve the central Notifications1 wire contract before implementation; then
   complete the remaining ID-105/107 topology, ID-106 service readiness, common
   ID-093 event reconciliation, and ID-185 recovery requirements.
4. Recompute the dependency frontier after each integration and continue through
   the Final 1:1 Parity Closure ticket.
