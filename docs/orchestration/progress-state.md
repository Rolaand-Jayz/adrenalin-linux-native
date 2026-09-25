# Project execution state

## Objective

Complete the Final Audited Ticket Pack through the Final 1:1 Parity Closure,
using the Final Audited Engineering Spec as the controlling contract.

## Current phase

Tickets 02 and 03 — Ticket 01's hosted-CI gate is complete on both branches.
Ticket 02's remaining gates require authentic reference evidence; Ticket 03
remains the active implementation frontier, with its unimplemented event families
and ID-185 recovery sequence tracked against their owning production contracts.

## Completed evidence

- Hardware1 CPU package static-info: `GetDeviceInfo` now returns the hwloc CPUID
  vendor identifier and decimal family/model pair for CPU_PACKAGE subjects. It
  omits stepping and physical package index. A CPU identity change updates the
  opaque subject token, advancing inventory and capability graph generations.
  Provider tests cover two
  subject-to-metadata mappings and generation behavior; the private-bus service
  test verifies the fields over the existing D-Bus schema. Strict-warning builds
  succeeded and the focused provider/session tests passed 2/2. This closes only a
  static-info sub-scope; no live multi-socket qualification or Ticket 03 completion
  is claimed.

- Hardware1 snapshot/event reconciliation: production device, static-info, and
  capability reads now include the shared per-service event cursor captured with
  each snapshot. The shell client reconciles common envelopes and Hardware1 family
  changes, handling duplicates, unrelated sequential events, gaps, identity or
  generation changes, and stale in-flight reads through authoritative refreshes.
  The shared nested Display1 Hardware1 reply signature was updated consistently.
  Strict-warning builds passed for the contract/service/client/shell targets, and
  focused CTest passed 5/5 including shell smoke. Independent review of the exact
  slice found no actionable findings. Display1 production list/state and mirrored
  event roots are now implemented, while Display1 client reconciliation, remaining
  ID-093 families, other ID-107 operations, and ID-185 recovery remain open;
  Ticket 03 is not complete.

- Telemetry1 contract progress: `OpenStream` now carries the shared ID-093 cursor; an isolated test model advances through common events from other Session1 families and reopens on sequence gaps, owner/generation changes, or changed telemetry definitions. The ABI fixture's reader tests now reject malformed mapping lengths/alignment, incompatible fixed header fields, and invalid slot sequence/guard/state/encoding. Strict-warning ABI build and focused CTest pass. The contract/cursor tests passed 3/3 after the earlier slice. This is fixture validation only: minor/reserved-field policy and independent ABI review remain open, as do production source selection, producer/client, readiness/recovery, Ticket 07, and production ID-093/ID-185 work.

- Profiles1 contract correction: `ReadProfile` carries service incarnation and
  event-sequence cursor beside the profile; zero is valid before the first event,
  while invalid/unavailable replies require an empty envelope. Fresh changes alone
  produce a typed local event; replay, no-op, and error outcomes do not. The
  generated proxy/private-bus test pins field order and snapshot/event behavior.
  Independent review found no remaining issue; a root-run strict-warning target
  build and focused CTest passed 1/1. This remains an ID-104 contract artifact only;
  persistence, inheritance/effective-state behavior, presets, UI, and Ticket 12
  production acceptance remain open behind Tickets 05 and 11 plus reference evidence.

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
- The Hardware1 wire/mock/test slice has a matching Reply XML signature and
  serialized field order, typed subject fields, failure-path output clearing, and
  ordered seven-field event declarations. Production Session1 now exports initial
  provider-backed Hardware1 inventory, info, and capability reads. A separate
  testable core target injects snapshots only in private-bus tests. Independent
  review found and the lead fixed a readiness bug: failed inventory now leaves the
  service FAILED, and Hardware1 replies fail closed after any FAILED state. Focused
  CTest passed 4/4 after the final readiness correction. The shell smoke test also
  passed 1/1. The aggregate all-target build remains unavailable in this session's
  existing build tree because `tests/identity_test.cpp` cannot find the Catch2
  header `catch2/catch_test_macros.hpp`; the affected implementation targets and
  focused suites build and pass independently.
- The native QML shell now exposes accessible names and roles for visible
  component identities. A separate AT-SPI process verified eight name/role pairs
  and positive in-frame extents under Xvfb. A candidate-capture CTest also runs a
  real scale-2 render and checks PNG dimensions, geometry schema/source, unique
  component IDs, and rectangle bounds. Its geometry is explicitly
  self-reported; WM/Wayland physical-pixel mapping, independent attestation, and
  parity remain unproven. The Xvfb smoke passed 1/1 after isolating its capture
  process from D-Bus activation.
- A read-only libdrm PCI inventory source is now implemented as isolated
  ID-104 groundwork. It bounds enumeration, validates PCI evidence, filters
  AMD vendor IDs, canonicalizes transient BDF evidence, rejects duplicates,
  and releases libdrm records on every path. A versioned, domain-separated
  SHA-256 mapper derives opaque `GPU_PCI` subject IDs from validated PCI evidence;
  its stability boundary is the PCI address plus vendor/device IDs. The source
  and identity tests are wired into project CTest and pass 2/2 (18 QtTest cases total).
  The test-enabled
  configure used a temporary Catch2 package shim because Catch2 v3 is not
  installed; only the DRM target was built. The full production app/sessiond
  build with tests disabled passed. DRM device nodes were not exposed during that
  source-only check, so runtime enumeration was not verified there. Subsequent
  work consumes the GPU, CPU-package, and display sources in the production
  Hardware1 inventory provider; this host's current provider-backed result is
  covered by the focused integration tests.
- The production Session1 service now exports provider-backed Hardware1 reads,
  reconciles successful refreshes atomically, tracks removal tombstones, publishes
  resolved-subject events, and returns DEVICE_DISCONNECTED versus NOT_FOUND with
  no stale payload. Failed refresh preserves the last authoritative snapshot but
  fails Hardware1 reads closed until recovery. The daemon uses a debounced filtered
  libudev DRM monitor to trigger serialized full provider refreshes. Focused private-
  bus tests cover removal, failure, reappearance, event envelopes, and closed
  reads when observation is unavailable. The daemon retries udev observation with
  capped backoff and requests a full refresh on receive loss. Live physical hotplug,
  event-gap client reconciliation, ID-107's remaining operation families, and full
  ID-185 startup recovery remain open. Notifications1 has a versioned schema, typed
  record vocabulary, in-memory test mock, and private-bus list/mark-read contract
  test for ID-104/ID-188. Production
  persistence now migrates schema v1 to v2 transactionally, atomically stores
  consent changes with one localized-key notification, serves consistent persistent
  snapshots, and persists MarkRead state plus operation replay/conflict semantics.
  The daemon exports the generated Notifications1 adaptor. Consent preflights two
  shared ID-093 event values; MarkRead publishes only on a fresh transition and
  refuses mutation when its event cursor is exhausted. Startup validates notification
  revision metadata, and each increment requires exactly one changed row. V1
  migration, restart replay/read persistence, stale/conflict/not-found, D-Bus cursor,
  corruption, and sequence exhaustion have regression coverage. Strict-warning
  production and test targets build; focused CTest passes 4/4 for session,
  Notifications1, Profiles1, and Hardware1 service. Independent review findings
  were fixed and rechecked. Toast preference/delivery, critical recovery notices,
  and bounded history-query policy remain open; Ticket 08 is not complete. An
  asynchronous Qt Notifications1 list model now reconciles persistent snapshots with service readiness and NotificationsChanged,
  preserves operation UUIDs across uncertain retry, and discards snapshots when an
  event arrives in flight. The shell exposes an accessible bell, unread badge,
  persistent history panel, human-readable consent messages, and mark-read actions.
  Client tests cover model roles, retry identity, and the snapshot/event race. The
  strict-warning shell and client-test targets build, all available CTest entries pass
  15/15 including focused contracts and shell smoke, and QML lint passes. The Catch2-
  based identity target could not compile because this environment has a package config
  shim but not Catch2 v3 headers, so it was excluded from the run. Independent review found no blocker in identity, readiness,
  cursor, or retry flow. The UI test opens notification history and clicks the
  rendered Mark as read button. Toast preference/delivery, other
  notification producers, critical recovery notices, and bounded history policy
  remain open. The follow-up error-feedback slice adds an accessible status banner
  and a rendered failure-path check for fixed human text, no raw backend code, and
  preserved unread state. The focused client suite passed 1/1 before a final
  copy-only refinement; `qmllint` and `git diff --check` pass afterward. A later
  focused CTest retry could not start its private D-Bus daemon because sandbox
  socket binding was denied. The revised copy avoids promising retry or refresh
  completion prematurely. This does not close Ticket 03 or Ticket 08.
  SessionService JSON logs include service UUID and generation; this
  satisfies the logging sub-scope, while READY remains gated on complete ID-185
  recovery. These are local results, not hosted CI. MarkRead reports persisted
  revision-counter exhaustion as an internal failure; the strict-warning rebuild and
  focused 4/4 CTest rerun passed, and independent review confirmed the mapping. A
  contract-only Profiles1 schema, typed global/game record, reference-gated mock, and
  private-bus read/update test
  cover the bounded ID-104/105/107 artifact. The focused strict-warning build and
  test pass; production persistence, inheritance/effective-state behavior, presets,
  UI, and Ticket 12 acceptance remain gated by Tickets 05 and 11 plus reference
  evidence. The combined focused regression set for Profiles1, Notifications1, and
  the affected Hardware1 service passes 3/3. The separate test-only snapshot
  injection target is not used by the production daemon.
- Hotkeys1 contract prerequisite now covers ListHotkeys/SetHotkey, stable
  action IDs, configured/effective binding separation, parent-window context,
  operation revisions/replay, and the ID-093 signal envelope. Its private-bus test
  passes 1/1 after an independent review found and corrected duplicate event
  publication on idempotent replay. The integrated session daemon and shell targets
  build; the broader available CTest selection passes 17/17, excluding the
  environment's unbuildable Catch2 v3 identity target. No portal/X11 provider,
  startup rebind, or production action handler is implemented; Ticket 03, ID-093,
  ID-185 and Ticket 09 remain open.
- The Hotkeys1 read-only production client gates ListHotkeys on service readiness
  and reconciles common and family-specific event streams. It validates identity,
  generation, subject envelope, and cursor progression; refreshes on action changes
  and gaps; fences stale replies across event and owner changes; and retries when a
  held ListHotkeys call fails after an event. The revision-zero event path is also
  cursor-fenced after independent review. Strict-warning client and daemon targets
  build; focused private-D-Bus CTest passes 1/1 (nine QtTest cases). No SetHotkey
  action, portal/X11 provider, or provider restart rebind is implemented; Ticket 03,
  ID-093, ID-185, and Ticket 09 remain open.
- Display1 now has a contract-only list/state/validate/apply schema, typed
  Hardware1 identity/generation records, isolated mock, and private-bus test for
  stale generations, malformed requests, readiness envelopes, verified apply
  replies, idempotent replay, and DisplayChanged followed by authoritative state
  refresh. Its strict-warning target builds; focused CTest passed 1/1 and the
  broader available suite passed 17/17. The safety route field is synthetic intent
  metadata only; there is no production guard/provider integration, event-gap or
  reconnect reconciliation, or completed ID-185 recovery. Ticket 03 remains open.
- Display1 production identity/read and safe-refusal sub-scope (2026-09-25): the
  daemon exports display list/state from the Hardware1 snapshot and returns typed
  UNKNOWN capabilities without inventing state. Validate/apply return UNSUPPORTED
  for valid changes until real control and safety providers exist; stale inputs are
  rejected and apply has no route, verification, revision, mutation, or event.
  Display1 relays display inventory/capability events using the corresponding
  Hardware1 event cursor. Strict-warning daemon and session-test builds succeeded;
  focused contract/session CTest passed 2/2. The private-D-Bus session test covers
  generated list/state/validate/apply calls, UNKNOWN capability serialization,
  readiness and malformed-ID envelopes, stale inventory/capability refusals,
  unchanged post-apply capability record and generations, and exact
  inventory/capability event pairing with
  Hardware1. Modes,
  active display state, successful control, ID-185 display recovery, Ticket 19
  and full ID-093 acceptance remain open.
- Display1 production client reconciliation (2026-09-25): the client assembles
  ListDisplays and GetDisplayState results only when their service UUID, generation,
  shared event cursor, inventory generation, and capability generation match. It
  handles common events and mirrored Hardware1/Display1 hints once per cursor,
  refreshes on gaps/owner or generation changes, and fences stale in-flight reads.
  Public snapshot generations clear while unavailable, and persistent state-read
  failures stop without a tight retry loop. Focused private-bus scenarios cover all
  three mirrored signal orders, unrelated sequential and duplicate events, gaps,
  stale replies, owner loss, same-owner generation rollover, and repeated NOT_FOUND.
  Strict-warning targets build; the focused client, contract, and service CTests
  pass 3/3. The available local regression suite passes 20/20 with the
  Catch2-dependent identity test excluded because its v3 headers are unavailable in
  this environment. Independent review found no actionable findings. This closes
  only this production client slice, not Ticket 03, full ID-093, ID-185, or display
  controls.
- Successful startup now completes database recovery and initial Hardware1 inventory
  composition before READY. This still does not perform the full ID-185 sequence:
  display recovery, capability rebuild after runtime changes, telemetry recreation,
  hotkey rebind, gamewatch/capture republish, and client snapshot reconciliation
  remain open.
- Current checkout validation (2026-09-25, `9d4a6c1`): the strict-warning full
  build completed for all available targets except `adrenalin-identity-test`, whose
  Catch2 v3 header is unavailable in this environment. The 19-test CTest set excluding
  that target passed 19/19 under private D-Bus/Xvfb-capable execution. Focused
  Display1 contract and production-session coverage passed 2/2. A live remote-ref
  query could not resolve GitHub in this session; the latest local refs show both
  remote-tracking branches at `9d4a6c1`.
- The service now exposes a nonzero provisional generation during STARTING,
  allowing valid Hardware1 BUSY invalid-snapshot envelopes before SQLite recovery
  has loaded the persisted service generation. Recovery keeps that value or
  advances it before publishing readiness; PropertiesChanged still reports only
  values that actually changed. The focused session contract test covers this.
- A bounded implementation-owned Hardware1 v1 capability-ID catalog now names
  GPU/CPU/platform metrics, per-display controls, and Ticket 24–26 GPU tuning
  families. Subject assignments are documented implementation
  choices where the spec does not assign scope. Record validation enforces
  registry membership/scope, and the focused contract test checks token syntax,
  a golden v1 ID-set digest, every scope mapping, and unknown/wrong-scope
  rejection. It is vocabulary only: closed provider/evidence vocabularies and
  capability-specific unit/enum mappings are enforced, while graphics/runtime
  capabilities, remaining display/system feature mappings, and wider provider
  qualification remain open. The service now waits for a successful initial
  Hardware1 inventory before READY and fails closed when discovery fails. Full
  ID-185 recovery remains incomplete.
  An independent ID-107 frontier audit found the remaining families depend on
  shared schemas, providers, safety semantics, or prerequisite tickets, so no
  disjoint implementation was started. Ticket 03 remains open at this shared
  contract seam.
- Ticket 01 native Qt shell and initial packaging/bootstrap commits exist.
- The shell smoke test was built in a temporary out-of-tree harness and passed
  on this CachyOS host: QML loaded, the process exited normally with status 0,
  and stderr was empty. This covers the platform smoke criterion, not a
  clean-checkout CI run.
- Ticket 02 candidate geometry now has an external same-run X11 capture probe.
  It observes the launched shell by PID through AT-SPI, captures the
  PID-owned window with an external screenshot process, verifies it is mapped
  and not overlapped by a higher stacked root window, and checks exact screenshot
  dimensions against AT-SPI frame extents. It also verifies frame/component
  geometry stayed fixed. An isolated Xvfb/D-Bus run
  emitted a 1280x800 PNG and eight-component schema-v1 export; the comparator's
  geometry loader accepted the export against those image bounds. This proves
  only candidate runtime geometry for that run. Ticket 02 still needs authentic
  reference captures, independently reviewed annotations, and a passing
  comparison.
- Profiles1 production persistence and Session1 integration are implemented as
  a bounded Ticket 03 slice. The transactional schema v3 migration stores
  global and source-qualified game profiles, typed scalar patches, revisions,
  and replayable operation results; changed writes publish once on the shared
  event sequence. Production/session test targets build, focused CTest passes
  4/4, and independent review reports no remaining actionable issue. Ticket 03
  and Ticket 12 remain open: the ID-185 recovery sequence, complete ID-093
  reconciliation, inherited/effective profile behavior, reference-backed
  Custom/preset semantics, and profile UI are still outstanding.
- The Profiles1 production client now reads authoritative snapshots and reconciles
  profile changes, event gaps, service-generation changes, and owner rebinding. A
  review found and fixed a lost-refresh race when an event arrived during a failed
  snapshot request; a private-bus regression test covers that path. The focused
  client CTest passes 1/1 after the fix. This is a bounded client slice: Ticket 03's
  remaining event families and ID-185 recovery gates, and Ticket 12's
  inheritance/effective values, reference-backed presets, and UI remain open.
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
- Telemetry1 fixture producer validation now rejects incorrect magic and metric
  count, matching reader validation. Regression tests prove invalid producer
  construction and publication fail closed. The strict-warning ABI target builds,
  focused CTest passes 1/1, and `git diff --check` passes. A second independent
  reasoning-only independent review found no actionable issue in the described
  fix but could not inspect the checkout diff. Production telemetry, ID-153 source selection, readiness and
  recovery integration, and Performance UI remain unimplemented. This does not
  complete Ticket 07 or Ticket 03.
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
2. Ticket 03 now has persistent Notifications1 list/read operations, a consent-
   change producer, and a tested client/history UI with unread state and read actions.
   Toast behavior, critical recovery notices, and the other ID-107 families remain.
   History remains an
   unbounded snapshot because the spec defines no pagination or retention policy;
   silent pruning is not implemented. Keep the session database and shared event
   allocator centralized for future producers; resolve the ID-185 readiness
   dependency cycle before integrating its remaining recovery families. Keep
   Telemetry1 fixtures separate from production until ID-153 source precedence and
   recovery are met.
3. Profiles1 has production persistence and snapshot/event reconciliation slices;
   full profile semantics remain open. ID-097 configured/effective precedence and
   reason fields need a reference-backed production contract. Continue the other ID-105/107 families only
   after their shared schema, provider, portal, rollback, and migration prerequisites
   are met.
4. Recompute the dependency frontier after each integration and continue through
   the Final 1:1 Parity Closure ticket.
