# AMD Adrenalin Linux RX 6000+ — Agent-Ready Ticket Pack

**Status:** in-progress
**Source:** Final audited RX 6000+ engineering spec
**Ticket model:** Matt Pocock tracer-bullet vertical slices

## Dependency frontier

Tickets 02 and 03 are now the current frontier because Ticket 01's hosted
clean-checkout workflow passed on both branches at `f6037f0`. Ticket 02 still needs
authentic reference captures, independently attested candidate geometry, reviewed
annotations, and a passing comparison. Ticket 03's tracer criteria pass, but its
audited-spec interface topology and recovery obligations remain open.

**Owner continuation authorization (2026-09-24):** The owner authorized local work
on Tickets 02 and 03 while Ticket 01 hosted CI was pending. Ticket 01 has since
passed its hosted gate; 02 and 03 are eligible now, but remain incomplete until their
own acceptance criteria and dependency gates pass.

## Progress ledger

- Ticket 01: **complete** — both pushed branches passed the clean-checkout hosted
  workflow at `f6037f0`, including the GUI shell smoke, all 7 CTest suites, reference
  manifest checks, staged install validation, D-Bus activation, and custom-prefix
  quoting. See Actions runs [main #5](https://github.com/Rolaand-Jayz/adrenalin-linux-native/actions/runs/36069938259) and [work #6](https://github.com/Rolaand-Jayz/adrenalin-linux-native/actions/runs/36069940024).
- Ticket 02: **partial / blocked** — fixed baseline, live candidate capture, and
  self-reported runtime geometry export exist; authentic reference captures,
  independent geometry attestation, reviewed annotations, and a passing
  comparison remain open.
- Ticket 03: **tracer acceptance met; audited-spec work remains** — the four tracer
  criteria below are covered. The v1 interface topology/operation families and
  ID-185 recovery sequence are not complete, so this ticket is not closed.
- ID-093 event contract: the consent tracer now uses the required event envelope;
  applying it to the remaining v1 services is still open.
- Filesystem-path policy: no machine-specific filesystem paths may be added. Fixed
  D-Bus names and object paths are protocol identities, not filesystem paths.
- [x] Project workspace folder is a real `adrenalin-linux-native` directory; the
  old workspace entry and symlink have been removed.

## Tickets

### 01 — Native Linux bootstrap and installable shell

**Blocked by:** None — can start immediately

**What to build:** Create the native C++20/Qt 6/QML application bootstrap and packaging skeleton so a user can install and launch an Adrenalin-styled Linux desktop shell.

**Acceptance criteria

- [x] Project config enforces C++20, Qt 6.8+, CMake, Ninja, Catch2 v3 and Qt Test; the shipping app has no Python runtime dependency.
- [x] The installed desktop entry launches a native Qt/QML process; Electron, PWA, localhost-served UI and browser-hosted application shells are absent.
- [x] The application opens to a deterministic shell on Arch/CachyOS and exits cleanly.
- [x] CI builds the shell and runs at least one smoke test from a clean checkout.
  - [x] Commit `9561725` built from a fresh `git archive`: all 101/101 Ninja
    steps succeeded and the full CTest suite passed 5/5, including shell CLI,
    Settings1/Service1 private-bus, and Hardware1 private-bus coverage (Qt 6.11.2,
    GCC 16.2.1, Catch2 3.4.0). Hosted CI remains a separate open requirement.
  - [x] On baseline commit `b8dc833`, all checked-in workflow steps were exercised against a fresh `git archive`: invalid activation-path rejection, 90/90 build steps, 4/4 CTest suites, 40/40 manifest tests, staged install checks, systemd and desktop validation, live D-Bus activation, and custom desktop-path quoting.
  - [x] Hosted clean-checkout runs at commit `f6037f0` passed on both `main` and
    `work/adrenalin-linux-native`: GUI build, CTest 7/7 including shell smoke,
    reference-manifest checks, staged install validation, D-Bus activation, and
    custom-prefix quoting. See [main run #5](https://github.com/Rolaand-Jayz/adrenalin-linux-native/actions/runs/36069938259) and [work run #6](https://github.com/Rolaand-Jayz/adrenalin-linux-native/actions/runs/36069940024).

### 02 — Reference corpus and parity harness

**Blocked by:** 01

**What to build:** Establish the fixed Adrenalin 26.9.1 reference workflow and make one shell surface objectively comparable to the Windows reference.

**Acceptance criteria

- [x] The reference baseline is AMD Software: Adrenalin Edition 26.9.1 Optional (2026-09-03).
- [ ] Reference capture metadata covers required resolutions/window states and the default/hover/focus/open/changed/disabled/error/success states where applicable.
- [x] The visual-diff harness uses checked-in deterministic fixtures and masks only approved dynamic regions.
- [ ] At least one shell screen passes the configured geometry/SSIM/color parity gate and emits a diff artifact on failure.

**Additional audited-spec tracking (ID-138):**

- [x] `not_applicable` capture rows bind a reviewer record to a checksum-verified
  captured image from the same display/window context.
- [ ] Authentic capture metadata records the fixed reference environment and
  observed behavior for every applicable context/state.
- [x] Candidate capture CLI renders the live Qt Quick window and measures named
  visible QML items; requested and display-scaled capture dimensions are bounded
  by the ID-137 physical-pixel envelope.
- [x] Runtime-geometry rectangle records use the comparator's `{x, y, width,
  height}` object shape. The source remains explicitly self-reported and cannot
  satisfy the independent geometry-attestation gate.
- [x] Capture-only arguments fail closed when incomplete; `--capture-size` is
  validated even when combined with `--smoke`. Focused CLI cases cover
  incomplete, undersized, oversized, and integer-overflow requests.
- [x] Manifest, comparison-check, mask, and runtime-geometry readers reject
  duplicate JSON keys instead of silently selecting one ambiguous evidence value;
  focused reference-tooling tests pass 43/43.
- [x] Portable operator guide and blank worksheets document the fixed capture
  matrix, observation fields, evidence hashing/review bindings, and validator
  workflow in `docs/reference-capture/`. The CLI commands, flags, and manifest
  observation keys were checked against the repository tooling. This enables
  authentic collection but does not supply captures, reviews, attestation, or a
  passing comparison; the acceptance gates above remain open.

### 03 — Session service and persisted preference tracer

**Blocked by:** 01

**What to build:** Introduce the authoritative per-user core service and prove the architecture with one preference that round-trips from UI through typed IPC to durable storage.

**Acceptance criteria

- [x] `adrenalin-sessiond` owns the core SQLite database and schema migration; the GUI does not open the production DB directly.
- [x] Canonical D-Bus bindings expose service readiness and a typed Settings operation for the tracer preference.
- [x] The preference survives service and GUI restart and stale expected revisions fail explicitly.
- [x] Structured logs identify the service instance/generation and recovery completes before the service reports READY.

**Additional audited-spec tracking (ID-093):**

- [x] Consent change events carry service-instance UUID, service generation,
  monotonic service event sequence, and typed stable subject kind and ID.
- [x] Consent snapshots return the event cursor; the client refreshes authoritative
  state when it observes an event-sequence or revision gap.
- [x] The client refreshes on event UUID, generation, or subject mismatch and
  ignores duplicate sequences without an unnecessary read.
- [x] Consent and Service1 readiness events share one monotonically increasing
  per-service cursor. Versioned Service1 `EventPublished` carries UUID,
  generation, sequence, typed subject kind, and stable subject ID; the readiness
  client ignores duplicates, advances over sequential events from other families,
  and reconciles gaps. Standard `PropertiesChanged` includes only changed values.
- [x] Event-sequence exhaustion fails closed: the final available sequence can
  be emitted once; later mutations are rejected before storage changes, no
  wrapped/duplicate event is published, initialization cannot report success,
  and the daemon exits so clients observe owner loss and reconcile.
- [ ] Apply the same event envelope and gap-reconciliation contract to every
  remaining cross-process event family in the v1 topology.

**Additional audited-spec tracking (ID-106):**

- [x] Service readiness is a shared C++ contract separate from Settings1
  operations; the session-service mock exercises every readiness field.
- [x] `ServiceReadinessClient` reconciles property/owner changes, rejects an
  incompatible API major, and discards stale snapshots before restoring READY.
- [x] `Settings1Client` gates reads, writes, and uncertain-operation replay on
  compatible READY plus a snapshot matching the current service UUID/generation.
- [ ] Every additional long-lived session/system service implements the same
  readiness surface and gates READY on its required recovery.

**Additional audited-spec tracking (ID-105/107):**

- [ ] Add the typed Hardware1 device-list/static-info/capability contract,
  generated bindings, test-only mock, and private-bus record round-trip before
  wiring any production hardware provider.
  - [x] The private-bus proxy/mock round-trip suite passes in the focused
    harness. Independent review and lead re-review confirm the bounded
    wire/mock/test slice below; production integration remains open.
  - [x] The generated Reply signature matches the complete serialized field
    order, including typed subject identity; failed reads expose no stale payload.
    Both seven-field event declarations are checked in order by introspection.
    Independent lead review confirms this bounded wire/mock/test slice; production
    event emission is not implemented.
  - [ ] The production Session1 root exports Hardware1 through a truthful
    provider-backed implementation; the test-only mock is never used as runtime data.
  - [ ] ID-107's telemetry, profile, settings import/export, display, hotkey,
    notification, and Hardware1 operation families exist in the fixed v1 topology.
  - [x] A bounded libdrm PCI inventory source provides read-only AMD device
    identity evidence, rejects malformed or duplicate PCI evidence, distinguishes
    enumeration errors from a valid empty inventory, and frees libdrm records on
    all paths. Its focused project CTest passes. A versioned SHA-256 mapper turns
    validated PCI address/vendor/device evidence into deterministic opaque
    `GPU_PCI` subject IDs; the ID changes if that topology evidence changes. This
    remains partial groundwork: CPU/display reconciliation, a complete capability
    graph, production Hardware1 export, and ID-185 recovery remain open.

**Additional audited-spec tracking (ID-185):**

- [ ] Session readiness waits for database migration, hardware/display inventory
  and display recovery, capability rebuild, telemetry recreation, hotkey portal
  rebind, gamewatch/capture republish, and client snapshot reconciliation.

### 04 — Adrenalin design system, accessibility, and localization shell

**Blocked by:** 02, 03

**What to build:** Build the reusable QML design system and navigation shell so subsequent screens can reach parity without page-local styling forks.

**Acceptance criteria

- [ ] Central tokens and reusable Adrenalin controls define typography, spacing, color, radii, controls, cards, navigation, modal and toast behavior.
- [ ] Top-level navigation matches the applicable reference hierarchy and uses the reference visual/interaction states.
- [ ] Keyboard navigation, visible focus, semantic accessibility labels and scalable text work without materially changing normal layout.
- [ ] Qt translation infrastructure is wired and layout tests include expansion-prone fixture strings.
- [ ] The shell includes the reference search field, notification bell, settings gear and Favorites affordance when exposed by the fixed reference.
- [ ] Top-level navigation explicitly provides Home, Gaming, Record & Stream, Performance and Smart Technology where applicable; Settings exposes System, Graphics, Display, Audio & Video, Hotkeys and Preferences.

### 05 — Capability graph and Linux System page

**Blocked by:** 03, 04

**What to build:** Discover RX 6000+ hardware/platform capabilities through one authoritative graph and expose truthful Linux graphics-stack/system information.

**Acceptance criteria

- [ ] Stable identities exist for GPU, CPU, display and relevant system subjects; QML never infers support from a GPU marketing name.
- [ ] Capability generations change when underlying hardware/provider capability changes and stale generations are rejectable by later mutations.
- [ ] System UI reports kernel, AMDGPU, Mesa, RADV, RadeonSI, libdrm, firmware and optional ROCm/application versions when available.
- [ ] Unsupported/unknown states render as unavailable/N/A rather than synthetic success or zero values.

### 06 — Distribution update visibility and handoff

**Blocked by:** 05

**What to build:** Show relevant Linux graphics-stack update availability while leaving installation ownership with the distribution.

**Acceptance criteria

- [ ] Arch/CachyOS uses a defined pacman/libalpm-safe query path; Fedora uses dnf/rpm; Debian/Ubuntu uses apt/dpkg.
- [ ] The app can report installed/available versions without changing kernel, Mesa, firmware or related distro packages.
- [ ] Where supported, a user action hands off to the distro updater/package manager rather than performing package installation itself.
- [ ] Windows driver download/install/rollback/clean-install surfaces remain Class N with explicit Linux ownership rationale.
- [ ] System update visibility includes reference-equivalent release/update details and release-notes access without treating AMD Windows driver packages as installable Linux updates.

### 07 — Shared-memory telemetry and Performance Metrics page

**Blocked by:** 03, 05

**What to build:** Provide live metrics through the versioned shared-memory telemetry ABI and render/log them through the Adrenalin Performance Metrics surface.

**Acceptance criteria

- [ ] `adrenalin-sessiond` is the sole telemetry producer and consumers map the stream read-only.
- [ ] The ring-buffer sequence protocol rejects torn samples under continuous wrap stress.
- [ ] Metrics include all available required GPU/CPU/system values with stale/unavailable validity states and stable units.
- [ ] The Performance page supports graphs/gauges, metric visibility/log selection, sampling interval and CSV logging.
- [ ] Performance settings include sample interval, performance logging location, hide-overlay-during-logging behavior and the reference logging controls.
- [ ] Baseline metric definitions explicitly cover FPS, frame time, 99th-percentile FPS, stutter rate, GPU utilization/clock, VRAM clock/utilization, board power, edge/current and junction/hotspot temperature, fan speed, GPU voltage, CPU utilization/frequency/temperature and system RAM utilization where available.

Progress (fixture slice; 2026-09-24): This patch adds a versioned Telemetry1 D-Bus schema, typed open/definition records, a test-only ABI v1 shared-memory fixture, ABI/concurrent-reader tests, and a private-bus contract test. A clean GUI-disabled Release build completed all 88 service/test build steps, and CTest passed 6/6 in the disposable integration checkout; this run does not include the GUI shell-smoke test. This evidence does not mark any full Ticket 07 acceptance criterion complete: no sessiond telemetry producer, hardware source selection, production readiness/recovery wiring, or Performance Metrics UI is included.

### 08 — Global search and notification center

**Blocked by:** 03, 04

**What to build:** Make the application searchable offline and provide persistent Adrenalin-style notification history.

**Acceptance criteria

- [ ] Search indexes pages, subsections, settings, features, detected games, tuning, display, capture, hotkeys and preferences.
- [ ] Selecting a result opens the owning surface and reveals the relevant section.
- [ ] Search works without network access and returns warm-index results within the spec threshold.
- [ ] Notifications persist read/unread state and support the defined taxonomy, toast preference and critical recovery notices.
- [ ] The reference notification bell opens the persistent notification surface and unread state is reflected in the shell.

### 09 — Global hotkey registry and desktop providers

**Blocked by:** 03, 04

**What to build:** Implement one application action registry with reliable global shortcuts on Wayland and X11.

**Acceptance criteria

- [ ] Wayland uses GlobalShortcuts v2+ with a real GUI parent for consent UI and recreates the portal session when rebinding requires it.
- [ ] X11 uses the certified X11 hotkey provider and does not depend on Wayland portal APIs.
- [ ] Conflicts, denial and interaction-required outcomes are typed and visible; there is no silent fallback.
- [ ] At least one action toggles successfully end to end and survives service reconnect/reconciliation.

### 10 — Linux game-library discovery

**Blocked by:** 03, 04

**What to build:** Build the game library across the supported Linux launch ecosystems with stable identities and manual-add support.

**Acceptance criteria

- [ ] Discovery covers Steam native, Steam Proton, non-Steam Steam shortcuts, Lutris, Heroic, Bottles, game `.desktop` entries and detectable AppImages.
- [ ] Manual add accepts native ELF, AppImage, `.desktop`, Wine/Proton `.exe`, and explicitly confirmed shell launchers.
- [ ] Game identity stores source/source-app ID, launch command, working directory, executable matchers and Wine/Proton context where applicable.
- [ ] Rescan is idempotent and does not duplicate an existing logical game.

### 11 — Game launch and session tracking

**Blocked by:** 10

**What to build:** Launch games and track the actual native/Proton render process so sessions, playtime and cleanup are correct.

**Acceptance criteria

- [ ] Steam/Proton/Wine launcher ancestry is resolved to the final game process rather than assuming the initial launcher is the render process.
- [ ] Session start/stop, playtime and active game state survive GUI closure and reconcile after gamewatch restart.
- [ ] Temporary game-session state is removed/restored on normal exit and orphaned sessions are reconciled after restart.
- [ ] Session summaries are published to the session service rather than written directly to the core DB.

### 12 — Global and per-game graphics profiles

**Blocked by:** 05, 11

**What to build:** Implement reference-equivalent global/per-game profile inheritance and Custom-state behavior.

**Acceptance criteria

- [ ] Per-game profiles inherit global values until explicitly overridden.
- [ ] Changing a constituent setting of a predefined profile creates the reference-equivalent Custom state without mutating global values.
- [ ] Mutable records use expected revisions so stale edits do not overwrite newer profile state.
- [ ] Profile state persists and the UI distinguishes configured, inherited and effective values.

### 13 — Runtime provider arbitration and per-game activation

**Blocked by:** 12

**What to build:** Provide deterministic runtime-provider selection and reversible per-game activation for graphics/runtime features.

**Acceptance criteria

- [ ] Provider selection follows an explicit policy and is not affected by registration or discovery order.
- [ ] Per-game runtime layers/environment/wrappers apply only to the selected game context.
- [ ] Game exit and watcher restart restore global/non-game state without leaving orphaned runtime mutations.
- [ ] No provider reports success unless effective state can be verified; unavailable providers remain explicit.

### 14 — Radeon Chill / frame-limit vertical slice

**Blocked by:** 13

**What to build:** Deliver one complete frame-rate policy feature from Adrenalin UI through runtime activation and verified effective behavior.

**Acceptance criteria

- [ ] Global and per-game scope follow profile inheritance rules.
- [ ] The selected runtime provider applies and removes the configured frame-rate policy with the game lifecycle.
- [ ] Configured/effective state is observable and a provider failure cannot produce a successful UI state.
- [ ] Interaction and runtime tests verify enable, change, disable and game-exit restoration.

### 15 — Radeon Image Sharpening vertical slice

**Blocked by:** 13

**What to build:** Deliver Radeon Image Sharpening-equivalent behavior through a certified Linux runtime provider.

**Acceptance criteria

- [ ] Reference control and per-game/global behavior are represented in the correct Gaming surface.
- [ ] A Vulkan/OpenGL/Gamescope-compatible shader/runtime path applies sharpening only to the intended target.
- [ ] Provider capability/effective state is explicit and unsupported targets remain unavailable rather than no-op.
- [ ] Visual/runtime regression tests demonstrate enable/disable and game-exit cleanup.

### 16 — Radeon Super Resolution vertical slice

**Blocked by:** 13

**What to build:** Deliver RSR workflow through the approved Gamescope/runtime spatial-upscaling provider.

**Acceptance criteria

- [ ] RSR is distinct from FSR and AFMF in UI, capability and parity ledger.
- [ ] Activation prerequisites and effective scaling state are detectable and truthful.
- [ ] Per-game launch integration is scoped and reversible.
- [ ] Certified tests cover supported activation, unsupported state and cleanup after game exit.

### 17 — FSR software-control parity

**Blocked by:** 12, 13

**What to build:** Represent and control the Adrenalin-facing AMD FSR Upscaling and FSR Frame Generation software surfaces for supported RX 9000/title combinations.

**Acceptance criteria

- [ ] FSR Upscaling and FSR Frame Generation are separate capabilities and are not substituted with RSR or AFMF.
- [ ] Controls appear only for hardware/title/reference combinations that expose them.
- [ ] Configured and effective state are separately observable.
- [ ] Unsupported titles/hardware produce a truthful unavailable state and parity-ledger evidence.

### 18 — Native metrics overlay

**Blocked by:** 07, 09, 11, 13

**What to build:** Render the Adrenalin-compatible in-game metrics overlay for certified native and Proton games.

**Acceptance criteria

- [ ] Vulkan and OpenGL runtime paths consume shared-memory telemetry without per-frame D-Bus calls.
- [ ] Overlay appearance passes the reference visual gate and supports configured metric visibility/hotkey toggle.
- [ ] Dormant and visible overlay overhead remain within the spec performance budgets on certification fixtures.
- [ ] Protected/anti-cheat processes are reported unsupported without bypass attempts.
- [ ] Overlay settings include reference-visible size, column count, transparency and text color/custom-color behavior.
- [ ] Overlay layout settings explicitly cover overlay size, column count, transparency and text color/custom color.

### 19 — Display discovery and read-only Display page

**Blocked by:** 05

**What to build:** Enumerate displays through Tier-1 Wayland/X11 providers and show truthful display capability/state.

**Acceptance criteria

- [ ] KDE Wayland uses KWin/KScreen-first provider precedence; X11 uses libXrandr.
- [ ] Stable display identity derives from connector/EDID/platform evidence rather than the display label alone.
- [ ] UI exposes resolution, refresh, VRR capability/state/range where detectable, HDR, scaling and relevant color/link information.
- [ ] Generic Adaptive-Sync is not mislabeled as an AMD FreeSync certification tier.

### 20 — Safe display controls

**Blocked by:** 19

**What to build:** Apply non-output-risking display controls through compositor-safe providers and verify the effective result.

**Acceptance criteria

- [ ] VRR/FreeSync controls, scaling mode, integer scaling and safe Custom Color paths are capability gated.
- [ ] KDE Wayland prefers compositor/KScreen APIs; X11 uses libXrandr and never shells out to `xrandr`.
- [ ] Custom Color uses compositor/color-management paths by default rather than destructive monitor DDC writes.
- [ ] A successful UI state requires provider read-back/effective-state verification.
- [ ] Custom Color covers reference-visible color temperature control/value, brightness, hue, contrast and saturation when the selected provider can apply and verify them.
- [ ] Display controls explicitly include GPU Scaling, Scaling Mode and Integer Scaling when supported.
- [ ] Custom Color includes Display Color Enhancement and Color Deficiency Correction when those surfaces exist in the fixed reference and a verified provider exists.
- [ ] Virtual Super Resolution is independently capability-gated and implemented only through a verified Linux-equivalent provider; ordinary desktop scaling is not mislabeled as VSR.

### 21 — Crash-safe risky display transactions

**Blocked by:** 03, 19

**What to build:** Make output-risking display changes recoverable across GUI/session/guard/daemon failure.

**Acceptance criteria

- [ ] A risky transaction is handed to the display guard before the first output-risking mutation and gets the reference 15-second Keep/Revert confirmation flow.
- [ ] Prepared privileged display authority uses a random transaction-bound capability token and no second polkit prompt is needed for recovery.
- [ ] Guard and privileged recovery records are durable before mutation and reconcile after restart.
- [ ] Crash tests cover every two-party commit phase and prove verified commit/revert without silent ambiguous state.
- [ ] Prepared privilege capability tokens have at least 256 bits of cryptographic randomness and are bound to operation ID, authenticated UID and target identity.
- [ ] The privileged daemon persists only a verifier/hash; the raw token remains in the user-private guard recovery record and never appears in logs, diagnostics, exports, notifications or the core DB.
- [ ] Wrong, reused, mismatched-UID, mismatched-target and expired-before-apply tokens are rejected; after mutation, expiry cannot remove the exact known-good recovery revert capability.

### 22 — Advanced display controls

**Blocked by:** 20, 21

**What to build:** Add advanced reference display controls while preserving crash-safe verification.

**Acceptance criteria

- [ ] Color depth, pixel format, custom resolutions and HDMI Scaling are exposed only when the selected provider can support and verify them.
- [ ] Applicable per-game display/color overrides follow the reference scope and inheritance behavior.
- [ ] Output-risking changes use the display-guard transaction flow.
- [ ] Unsupported compositor/link combinations remain explicit unavailable/Class D rather than guessed.
- [ ] Graphics 10-Bit Pixel Format is treated as a distinct capability from display-link color depth and is exposed only when the selected graphics/application path can reproduce it.
- [ ] Color Depth and Pixel Format are distinct controls and custom resolutions use the crash-safe risky-display path when output loss is possible.

### 23 — Mobile display and Vari-Bright path

**Blocked by:** 19, 20

**What to build:** Support in-scope RX 6000+ mobile built-in-display behavior and a real Vari-Bright equivalent where platform support exists.

**Acceptance criteria

- [ ] Certification uses an RX 6000+ mobile/hybrid built-in-display fixture.
- [ ] Vari-Bright maps to a documented adaptive platform interface and is not implemented as a static brightness slider or DDC loop.
- [ ] Battery/AC behavior, levels, manual-brightness interaction and restoration are testable.
- [ ] If no equivalent platform capability exists, the feature remains explicit Class D.

### 24 — Privileged tuning daemon and power-limit tracer

**Blocked by:** 03, 05

**What to build:** Introduce the privileged tuning boundary and prove it with one complete, safe GPU power-limit transaction.

**Acceptance criteria

- [ ] `adrenalin-linuxd` is the only elevated service and accepts semantic operations rather than arbitrary filesystem writes.
- [ ] Polkit authorization and caller identity derive from authenticated bus credentials.
- [ ] Apply validates capability generation/range, journals known-good state before the first write, verifies read-back and rolls back on failure.
- [ ] UI implements dirty Apply/Discard behavior and cannot report success after backend failure.
- [ ] Automatic tuning exposes Default and applicable Undervolt GPU, Overclock GPU and Overclock VRAM modes only when a verified provider implements them.
- [ ] Reference Tuning Presets such as Quiet, Balanced and Rage appear only on supported hardware/reference combinations and are distinct from manual tuning.

### 25 — GPU frequency and voltage tuning

**Blocked by:** 24

**What to build:** Add capability-bounded GPU frequency and supported voltage/offset tuning through the transactional daemon.

**Acceptance criteria

- [ ] Min/max frequency and voltage controls appear only when kernel/device capabilities expose them.
- [ ] All values are validated against the current capability generation before mutation.
- [ ] Multi-value transactions apply deterministically and roll back earlier writes if a later write fails.
- [ ] Read-back/effective state is shown after successful apply.

### 26 — VRAM, fan, Zero RPM, and power tuning

**Blocked by:** 24

**What to build:** Complete the main RDNA2+ GPU tuning controls exposed by AMDGPU/hwmon where supported.

**Acceptance criteria

- [ ] VRAM frequency/timing, fan/manual curve, Zero RPM, max fan and remaining power controls are individually capability gated.
- [ ] Unsupported nodes never produce enabled controls or fake success.
- [ ] Fan/power state is restored on rollback and service recovery.
- [ ] Hardware tests cover at least one supported behavior on each mandatory generation fixture where available.
- [ ] Advanced VRAM behavior includes Memory Timing when the kernel/device exposes an equivalent.
- [ ] Fan Tuning explicitly covers Max Fan and the reference advanced fan curve where supported.
- [ ] Power Tuning explicitly exposes the supported Power Limit control and verifies effective state.

### 27 — Tuning profiles and persistent startup behavior

**Blocked by:** 25, 26

**What to build:** Persist/import/export tuning profiles safely and implement explicit opt-in startup application.

**Acceptance criteria

- [ ] Profiles carry schema version, device identity and capability fingerprint; incompatible imports are rejected or explicitly remapped with warning.
- [ ] Persistent tuning is opt-in and uses the 10-second safety grace before automatic application.
- [ ] Last-known-safe, pending and suspect states are durable across restart.
- [ ] A profile associated with instability is not automatically re-applied until explicitly reviewed/revalidated.

### 28 — GPU stress test and instability recovery

**Blocked by:** 27

**What to build:** Provide a bounded Vulkan stress test and fail-safe recovery after GPU reset, reboot or instability.

**Acceptance criteria

- [ ] Stress workload is a dedicated bounded Vulkan workload and never disables thermal/current/power protections.
- [ ] UI exposes device selection, duration, start/stop, live readings and terminal result.
- [ ] GPU reset/watchdog/device-loss/unclean-shutdown correlation marks the active transaction/profile suspect.
- [ ] Recovery restores/verifies safe state before new tuning; unverifiable recovery blocks tuning with a persistent critical warning.

### 29 — CPU/System tuning and Variable Graphics Memory

**Blocked by:** 24

**What to build:** Implement or truthfully block reference-visible CPU/System tuning and VGM on applicable Ryzen/Radeon systems.

**Acceptance criteria

- [ ] Exact presence/defaults/controls are reference-gated against the fixed build and applicable hardware.
- [ ] Generic governor/EPP controls are not presented as Ryzen overclock semantics.
- [ ] VGM uses a documented platform/firmware interface and verifies effective state.
- [ ] If Linux cannot reproduce the semantics safely, the feature remains explicit Class D.

### 30 — Native recording core tracer

**Blocked by:** 03, 04, 05

**What to build:** Prove the native Linux capture/media seam with a complete start/stop recording path using PipeWire, FFmpeg/libav and AMD hardware encoding.

**Acceptance criteria

- [ ] PipeWire captures the selected reference-relevant video target and system audio through the approved Wayland/X11 path.
- [ ] The pipeline encodes at least one certification-baseline AVC/H.264 hardware profile through AMD VAAPI and finalizes a playable media file.
- [ ] Start/stop state is owned by the capture service and survives GUI page navigation/closure.
- [ ] The implementation avoids unnecessary GPU↔CPU copies when the selected path supports low/zero-copy behavior.
- [ ] Crash/partial-file recovery produces a recoverable/finalized outcome or an explicit failed artifact state rather than silently losing ownership.

### 31 — Advanced recording profiles, codecs and audio controls

**Blocked by:** 30

**What to build:** Expand the proven recording seam to the full reference recording settings, codec and multi-track audio surface.

**Acceptance criteria

- [ ] Recording settings cover Record Desktop, Show Indicator, applicable Borderless Region/Application Capture, Recording Profile, Recording Resolution, Recording FPS, Video Bit Rate, Audio Bit Rate and Enhanced Filtering.
- [ ] HEVC and AV1 appear only when the selected AMD encoder backend/device supports them; AVC remains available on the baseline fixture.
- [ ] Audio controls cover channel configuration, Record Microphone, Microphone Level, Push to Talk, Audio Boost, Separate Microphone Track and Audio Capture Device.
- [ ] Media Save Location is configurable and finalized container metadata/track layout matches the configured profile.
- [ ] Baseline certification meets resolution/codec/FPS/bitrate/frame-loss/A-V drift requirements for the required 1080p/1440p profiles and 4K60 where encoder preflight passes.

### 32 — Screenshots and Instant Replay

**Blocked by:** 30

**What to build:** Add screenshot targets and bounded Instant Replay without coupling either to page lifetime.

**Acceptance criteria

- [ ] Screenshots support active game, selected monitor and desktop where providers allow them; fallback to another target is never silent.
- [ ] Successful screenshots default to lossless PNG unless the fixed reference proves otherwise.
- [ ] Replay uses a bounded encoded ring with configured duration and survives GUI navigation.
- [ ] A replay save failure does not disable a healthy replay buffer and saved-window timing meets the spec tolerance.
- [ ] Instant GIF and In-Game Replay are independently represented when present in the fixed reference and use the same bounded capture/replay ownership rather than ad-hoc page-local buffers.

### 33 — Media library

**Blocked by:** 30, 32

**What to build:** Index ordinary recording/screenshot files in the Adrenalin Media surface without making the database their sole owner.

**Acceptance criteria

- [ ] Media metadata includes path, type, game, created time, duration, resolution, codec, size and thumbnail where applicable.
- [ ] Open, play, reveal-in-folder and confirmed delete actions work from the reference-equivalent UI.
- [ ] Media index can be rebuilt/reconciled from filesystem state.
- [ ] Deleting metadata alone does not implicitly delete the underlying file.

### 34 — Custom RTMP/RTMPS streaming

**Blocked by:** 31, 30

**What to build:** Deliver secure custom live streaming with the reference recording/stream control model.

**Acceptance criteria

- [ ] Custom RTMP/RTMPS supports Go Live/Stop, camera, microphone, bitrate, resolution, FPS and audio settings.
- [ ] Stream secrets are stored in Secret Service/KWallet with no plaintext fallback.
- [ ] Reconnect/failure states are observable and do not corrupt recording/replay state.
- [ ] Archive-stream behavior is implemented when supported by the selected destination/path or represented truthfully.
- [ ] Archive Stream behavior is explicitly represented and verified or reported unavailable; it is not silently omitted from the Live Stream settings surface.
- [ ] Live Stream explicitly supports the reference Custom Stream path in addition to any later first-party account integrations.

### 35 — Scene Editor

**Blocked by:** 34

**What to build:** Build the reference Scene Editor with persistent scenes and sandboxed elements.

**Acceptance criteria

- [ ] Scenes support create/rename/select/delete, width/height, ordering, visibility and direct manipulation.
- [ ] Element families include Browser Source, Image, GIF, Video, Indicator, Camera and Chat Overlay.
- [ ] Camera properties include position, dimensions, opacity, on-screen/in-video visibility and reference chroma-key properties.
- [ ] Browser Source retains Chromium sandboxing, isolates local-file access, and fails closed on TLS/permission errors.
- [ ] Reference scene hotkey behavior is represented when present.
- [ ] Camera chroma-key properties explicitly include chroma color and key strength.

### 36 — Overlay capture and streaming controls

**Blocked by:** 31, 18, 30, 32, 34

**What to build:** Expose recording, screenshot, replay, microphone and streaming controls from the native in-game overlay.

**Acceptance criteria

- [ ] Overlay actions use the same capture/hotkey/state contracts as the desktop UI.
- [ ] UI state reconciles after capture-service restart rather than inferring from process exit.
- [ ] Protected-process limitations remain explicit and no integrity bypass is attempted.
- [ ] Visual parity and performance budgets remain satisfied while these controls are visible.

### 37 — Noise Suppression

**Blocked by:** 03, 04

**What to build:** Implement the Smart Technology Noise Suppression equivalent through PipeWire virtual nodes and a local denoiser.

**Acceptance criteria

- [ ] User can select input, output or both directions and the created virtual nodes are discoverable/selectable by other apps.
- [ ] Processing label truthfully reflects CPU/GPU execution and never claims GPU use when not active.
- [ ] Routing and cleanup survive service restart and device disappearance.
- [ ] Measured latency stays within the certification matrix threshold.

### 38 — Smart Access Memory surface

**Blocked by:** 05

**What to build:** Expose evidence-backed Smart Access Memory/Resizable BAR status and only a real runtime control when one exists.

**Acceptance criteria

- [ ] State derives from PCIe/platform evidence rather than a guessed hardware table.
- [ ] UI follows the reference state/status treatment for applicable systems.
- [ ] No BIOS/firmware change is claimed unless a documented provider performs and verifies it.
- [ ] Unsupported runtime toggling remains status-only rather than fake interactive control.

### 39 — SmartAccess Graphics and SmartShift

**Blocked by:** 19, 23

**What to build:** Implement or explicitly block the mobile/hybrid routing and power-sharing semantics of SmartAccess Graphics and SmartShift.

**Acceptance criteria

- [ ] Reference capture determines applicable modes/status/restart requirements on the certified mobile fixture.
- [ ] SmartAccess Graphics verifies render GPU, display-owning GPU and routing/copy/offload outcome.
- [ ] SmartShift maps only to a real firmware/SMU/platform power-sharing interface and is never simulated with unrelated sliders.
- [ ] Missing platform semantics remain Class D with evidence.

### 40 — SmartAccess Video

**Blocked by:** 05, 30

**What to build:** Implement or explicitly block verified multi-adapter media participation for SmartAccess Video.

**Acceptance criteria

- [ ] Active state requires measured/verifiable encode/decode participation across eligible adapters.
- [ ] Ordinary selection of another encoder is not labeled SmartAccess Video.
- [ ] Capability/status is generation/platform gated and represented in the Smart Technology surface.
- [ ] Certification records performance/participation evidence or a Class D blocker.

### 41 — Privacy View

**Blocked by:** 03, 04, 30

**What to build:** Implement local permission-bound Privacy View behavior or retain an explicit Class D blocker.

**Acceptance criteria

- [ ] Camera access uses the certified PipeWire/portal or desktop permission path.
- [ ] Processing is local by default, displays an active indicator and stops on permission revocation/device disappearance.
- [ ] No camera frames are uploaded by the product.
- [ ] If captured reference semantics cannot be reproduced, no generic webcam filter is mislabeled as Privacy View.

### 42 — AMD Assistant rules engine

**Blocked by:** 03, 12

**What to build:** Implement reference-gated AMD Assistant automation as an auditable local rules engine.

**Acceptance criteria

- [ ] Phase-0 reference capture establishes whether the fixed build exposes AMD Assistant and captures user-visible controls/defaults/notifications.
- [ ] Rules may change only settings the application owns or can truthfully request from an approved provider.
- [ ] Every automatic change records trigger, prior configured value, resulting effective value and restoration semantics.
- [ ] Unknown proprietary trigger logic remains Class D and Class N distro/kernel/package responsibilities are never mutated.

### 43 — Video profiles, Video Upscaling and Geometric Downscaling

**Blocked by:** 31, 30, 05

**What to build:** Deliver the reference video-profile family plus truthful AMD Video Upscaling and Geometric Downscaling behavior.

**Acceptance criteria

- [ ] Video profiles include Default, Cinema Classic, Enhanced, Home Video, Outdoor, Sports, Vivid and Custom when present in the fixed reference.
- [ ] AMD Video Upscaling is tracked as a distinct product capability and is not substituted with ordinary display scaling.
- [ ] Geometric Downscaling is tracked separately from ordinary resolution/scaling controls and uses only a verified media/video path.
- [ ] Unsupported application/hardware scopes remain explicit B/C/D outcomes rather than hidden or mislabeled.

### 44 — Encoder enhancement controls

**Blocked by:** 31, 30, 05

**What to build:** Implement or explicitly block reference-visible encoder Pre-Analysis, Pre-Filtering, CAML and Enhanced AVC-style controls on the selected Linux media backend.

**Acceptance criteria

- [ ] Each enhancement is individually capability gated by codec/device/backend support.
- [ ] A control reports enabled only when the selected encoder path actually applies the documented behavior.
- [ ] No generic encoder-quality knob is relabeled as a specific AMD enhancement.
- [ ] Unsupported combinations remain explicit B/C/D outcomes with certification evidence.

### 45 — Game Advisor

**Blocked by:** 07, 11, 12

**What to build:** Provide the evidence-based Game Advisor after sufficient gameplay telemetry.

**Acceptance criteria

- [ ] Eligibility requires at least 180 seconds of valid gameplay telemetry unless fixed-reference capture proves a different threshold.
- [ ] Report uses average/percentile FPS, frame-time behavior, stutter, GPU/CPU utilization, VRAM pressure and detectable thermal/power constraints.
- [ ] Recommendations account for profile, resolution, VRR, frame cap, GPU-vs-CPU bound evidence, thermals and VRAM.
- [ ] Advisor never recommends unavailable/Class D features as usable.

### 46 — Core Preferences and desktop-shell behavior

**Blocked by:** 03, 04, 09

**What to build:** Complete the ordinary Preferences surface and desktop-shell behaviors that do not require a dedicated integration backend.

**Acceptance criteria

- [ ] Preferences include System Tray behavior, promotional-content/advertisement control, Toast Notifications, language, Always On Top, Sidebar Position, Animation & Effects, telemetry/user-experience opt-in and game-adjustment tracking/notifications where present.
- [ ] Changes persist through the session service and respect localization/accessibility behavior.
- [ ] Promotional content can be disabled without breaking the Home layout contract.
- [ ] No preference claims ownership of Class N distro/kernel/package responsibilities.

### 47 — Integrated browser surface

**Blocked by:** 04, 46

**What to build:** Deliver the reference integrated Web Browser as a sandboxed Qt WebEngine application feature.

**Acceptance criteria

- [ ] Browser surface matches captured navigation/layout behavior for the fixed reference.
- [ ] Chromium sandboxing remains enabled and TLS/permission failures fail closed.
- [ ] Browser history/state follows the product privacy policy and does not become a dependency of the main application shell.
- [ ] The browser can be disabled/closed without affecting core Radeon functionality.

### 48 — SteamVR integration surface

**Blocked by:** 09, 11, 46

**What to build:** Implement or explicitly block the reference SteamVR integration behavior on Linux.

**Acceptance criteria

- [ ] Reference capture defines the exact preference/status behavior before implementation.
- [ ] The provider integrates only through supported Linux SteamVR/runtime mechanisms.
- [ ] Unsupported or unavailable SteamVR state is visible and does not produce a successful toggle.
- [ ] The parity ledger records the verified B/D disposition and test evidence.

### 49 — Image Inspector parity resolution

**Blocked by:** 04, 46

**What to build:** Resolve the independently tracked AMD Image Inspector workflow without substituting an unrelated image tool.

**Acceptance criteria

- [ ] Reference capture records the visible workflow, inputs, outputs, privacy prompts and network/cloud dependence.
- [ ] If the workflow can be reproduced truthfully and privacy-safely, the implementation passes its UI/behavior tests.
- [ ] No user image is uploaded without explicit product behavior/consent matching the approved design.
- [ ] If proprietary/cloud semantics cannot be reproduced, Image Inspector remains explicit Class D.

### 50 — Issue Reporting and Issue Detection

**Blocked by:** 03, 05, 46

**What to build:** Provide the reference-visible bug-report/issue-detection workflow with safe local diagnostics handling.

**Acceptance criteria

- [ ] Reference bug/report entry points exist, including System/issue-reporting and icon affordance when present.
- [ ] Issue Detection can invoke the reporting flow for defined detected failures without silently transmitting data.
- [ ] Generated diagnostics redact secrets and remain local until the user explicitly submits/shares them.
- [ ] Remote endpoint behavior is explicit; absence of an AMD submission endpoint does not fake successful remote reporting.

### 51 — Snap Settings export/import

**Blocked by:** 12, 21, 27, 46

**What to build:** Implement schema-versioned settings snapshots with deterministic diff, validation, backup and risky-state handling.

**Acceptance criteria

- [ ] Snapshot includes application preferences, graphics/tuning/game profiles, overlay/capture config, scene metadata without secrets, hotkeys and safe display settings with schema/hardware metadata.
- [ ] Import validates schema and hardware compatibility, previews the diff and backs up current state before mutation.
- [ ] Risky display/tuning changes route through their normal transactional rollback systems rather than a snapshot-specific shortcut.
- [ ] Secrets, media bytes and Class N distro/kernel/package state are excluded from the archive.
- [ ] Risky imported state uses an explicit Keep Settings / Revert Settings terminal confirmation flow and records the import result.

### 52 — Factory Reset and Quick Setup onboarding

**Blocked by:** 46, 51

**What to build:** Deliver non-destructive Factory Reset and the captured first-run/after-reset Quick Setup experience.

**Acceptance criteria

- [ ] Factory Reset returns application settings/profiles to defaults without deleting media or distro graphics packages.
- [ ] Reset clears the appropriate persisted acknowledgement/onboarding state and preserves secrets only according to explicit reset scope.
- [ ] First launch/after-reset invokes the captured Quick Setup flow.
- [ ] Onboarding does not become a Linux sysadmin wizard; platform problems surface as focused actionable compatibility states.

### 53 — Local AI/Chat core

**Blocked by:** 03, 04, 05

**What to build:** Deliver the optional, separately installable local AI service with safe read-only product/system tools.

**Acceptance criteria

- [ ] Core application operates fully without AI installed.
- [ ] AI supports local chat/history and typed queries for hardware, graphics stack, game library, profiles, displays and live telemetry.
- [ ] AI has no unrestricted shell/root access and stores its own history/indexes outside the core application DB.
- [ ] Resource-heavy AI work suspends/yields during demanding games according to the product rules.

### 54 — AI document RAG, image generation and confirmed actions

**Blocked by:** 53

**What to build:** Complete the optional AI feature surface with document RAG, optional local image generation and safe confirmed actions.

**Acceptance criteria

- [ ] Document upload/index/RAG remains local by default and respects file-access boundaries.
- [ ] Optional local image generation is independently installable/capability gated.
- [ ] Any setting mutation uses the same typed validation/authorization/idempotency path as the GUI.
- [ ] Mutating actions require explicit confirmation and cannot alter Class N platform responsibilities.

### 55 — AFMF parity resolution

**Blocked by:** 13, 18

**What to build:** Resolve AFMF for Linux through focused research/prototyping and leave a truthful product outcome.

**Acceptance criteria

- [ ] Prototype/evidence measures whether a Linux backend can reproduce the reference user-visible frame-generation behavior on in-scope hardware.
- [ ] RX 6000/7000/9000 mode differences are recorded independently rather than inherited.
- [ ] If verified, provider integration gets capability/effective-state tests; if not, the feature remains an explicit Class D blocker.
- [ ] No unrelated interpolation/runtime feature is labeled AFMF.

### 56 — Anti-Lag / Anti-Lag 2 Latency Monitor parity resolution

**Blocked by:** 13, 18

**What to build:** Resolve Anti-Lag semantics and the reference latency-monitor surface without substituting a generic limiter.

**Acceptance criteria

- [ ] Research/prototype distinguishes scheduling/latency behavior from ordinary FPS limiting.
- [ ] Anti-Lag 2 Latency Monitor is implemented only with verified title/instrumentation support.
- [ ] Supported behavior has measurable activation/effective-state evidence.
- [ ] Unreproducible proprietary behavior remains explicit Class D.

### 57 — Radeon Boost and Enhanced Sync parity resolution

**Blocked by:** 13, 19

**What to build:** Resolve the remaining difficult Boost and Enhanced Sync runtime/display semantics.

**Acceptance criteria

- [ ] Boost research establishes whether dynamic-resolution/runtime integration can reproduce the captured user outcome.
- [ ] Enhanced Sync research establishes whether present/compositor behavior can reproduce the captured semantics.
- [ ] Verified providers expose measurable effective state and cleanup.
- [ ] Otherwise each feature remains explicit Class D and is not substituted with neighboring controls.

### 58 — Arch/CachyOS production packaging and service activation

**Blocked by:** 03, 11, 18, 21, 24, 30

**What to build:** Produce the canonical Tier-1 production package with every runtime artifact needed by the non-AI application.

**Acceptance criteria

- [ ] Arch/CachyOS package installs GUI, user/system services, D-Bus activation, polkit policy, required udev rules, Vulkan layer manifests, desktop entry and project-owned icons.
- [ ] Qt WebEngine sandbox prerequisites are declared for browser/scene features without weakening sandboxing.
- [ ] Systemd/D-Bus activation starts the correct long-lived services without requiring the GUI to stay open.
- [ ] Installing/upgrading the package never installs or replaces the distro graphics stack.
- [ ] Flatpak is explicitly non-canonical for full functionality; any later Flatpak build is treated as a restricted/UI-only variant and cannot replace the native package certification path.

### 59 — Optional AI package and activation

**Blocked by:** 53, 54, 58

**What to build:** Package the optional AI service/models boundary so it can be installed or removed independently of core Radeon control.

**Acceptance criteria

- [ ] Core application installs/runs with no AI package present.
- [ ] AI service activation, storage permissions and model assets are isolated from the core application package.
- [ ] Removing AI leaves core settings/database and Radeon functionality intact.
- [ ] Package metadata clearly distinguishes optional large model assets from required binaries.

### 60 — Tier-2 Fedora and Debian/Ubuntu packaging

**Blocked by:** 58

**What to build:** Produce installable Tier-2 package definitions that preserve the same service/privilege/runtime contract as Tier 1.

**Acceptance criteria

- [ ] Fedora package declares correct system/user service, polkit, D-Bus, Vulkan-layer and desktop integration.
- [ ] Debian/Ubuntu package declares the same runtime contract.
- [ ] Neither package installs/replaces kernel, Mesa, firmware or other distro graphics-stack components.
- [ ] Install/uninstall smoke tests run in representative Tier-2 environments.

### 61 — Upgrade and migration hardening

**Blocked by:** 51, 58

**What to build:** Make application/database upgrades deterministic, backed up and recoverable.

**Acceptance criteria

- [ ] Versioned DB/config migrations back up state before destructive steps.
- [ ] Migration failure leaves the previous usable schema/state recoverable and does not silently partially migrate.
- [ ] Upgrade reconciles service/recovery records before accepting new risky mutations.
- [ ] User media and exported profiles survive ordinary upgrades.

### 62 — Diagnostics, logging and crash-reporting hardening

**Blocked by:** 50, 58

**What to build:** Make production failures diagnosable without leaking secrets or requiring implementation-specific tribal knowledge.

**Acceptance criteria

- [ ] Diagnostics expose capability/provider/compositor/PipeWire/Vulkan-layer/encoder/permission/recovery state.
- [ ] Structured logs rotate, use subsystem tags and automatically redact secrets/capability tokens.
- [ ] Crash dumps remain local unless the user explicitly opts into upload/sharing.
- [ ] A generated diagnostic report is reproducible and safe to share after redaction.

### 63 — Safe uninstall and state quiescence

**Blocked by:** 11, 21, 27, 30, 34, 58

**What to build:** Prevent package removal from abandoning application-owned mutable or unsafe runtime state.

**Acceptance criteria

- [ ] Removal preflight resolves/reverts risky display transactions, restores app-owned tuning/runtime state and removes temporary game mutations.
- [ ] Active recording/replay/stream pipelines finalize or stop safely before binaries disappear.
- [ ] User media and exported profiles are retained unless explicit data deletion is requested.
- [ ] Normal removal fails closed if safe quiescence cannot be verified.

### 64 — RDNA2 desktop certification

**Blocked by:** 22, 28, 36, 40, 43, 55, 56, 57, 61, 62

**What to build:** Run the complete applicable release-certification suite on the mandatory RX 6000 / RDNA2 discrete desktop fixture.

**Acceptance criteria

- [ ] Fixture identity, kernel/Mesa/compositor/display and test-matrix revisions are recorded.
- [ ] Applicable graphics, overlay, tuning, display, media/capture and Smart Technology tests run and publish artifacts.
- [ ] Unsupported generation-specific capabilities are recorded explicitly rather than inherited from newer GPUs.
- [ ] Failures link back to the parity ledger and block RDNA2 certification.

### 65 — RDNA3 desktop certification

**Blocked by:** 22, 28, 36, 40, 43, 44, 55, 56, 57, 61, 62

**What to build:** Run the complete applicable release-certification suite on the mandatory RX 7000 / RDNA3 discrete desktop fixture.

**Acceptance criteria

- [ ] Fixture/software/test-matrix identities are recorded.
- [ ] Applicable graphics, overlay, tuning, display, media/capture and Smart Technology tests run with artifacts.
- [ ] RDNA3-specific behavior is recorded independently rather than inferred from RDNA2/RDNA4.
- [ ] Failures block RDNA3 certification and link to parity-ledger rows.

### 66 — RDNA4 desktop certification

**Blocked by:** 17, 22, 28, 36, 40, 43, 44, 55, 56, 57, 61, 62

**What to build:** Run the complete applicable release-certification suite on the mandatory RX 9000 / RDNA4 discrete desktop fixture.

**Acceptance criteria

- [ ] Fixture/software/test-matrix identities are recorded.
- [ ] FSR software-control surfaces and other RDNA4-specific capabilities are explicitly exercised.
- [ ] Applicable graphics, overlay, tuning, display, media/capture and Smart Technology tests publish artifacts.
- [ ] Failures block RDNA4 certification and link to parity-ledger rows.

### 67 — Mobile/hybrid and multi-adapter certification

**Blocked by:** 23, 29, 37, 39, 40, 41, 42, 43, 61, 62

**What to build:** Certify the mandatory RX 6000+ mobile/hybrid built-in-display and APU+dGPU routing/power/media paths.

**Acceptance criteria

- [ ] Fixture exercises built-in display, hybrid graphics and applicable Vari-Bright/SmartAccess Graphics/SmartShift paths.
- [ ] Multi-adapter enumeration/routing and SmartAccess Video evidence are captured where applicable.
- [ ] Mobile/APU-specific CPU tuning/VGM behavior is tested or dispositioned Class D.
- [ ] Form-factor-specific failures block the corresponding certification claim.

### 68 — Multi-monitor VRR certification

**Blocked by:** 20, 21, 22, 61, 62

**What to build:** Certify multi-monitor, VRR and risky-display behavior on the mandatory RX 6000+ display fixture.

**Acceptance criteria

- [ ] VRR capability/active/range behavior is verified across the certification display configuration.
- [ ] Advanced display operations and 15-second risky rollback are exercised on real outputs.
- [ ] Crash/restart cases from the display recovery matrix pass on hardware.
- [ ] Results publish reproducible fixture and parity-ledger artifacts.

### 69 — RX 6000+ certification synthesis

**Blocked by:** 64, 65, 66, 67, 68

**What to build:** Merge per-fixture certification evidence into the authoritative RX 6000+ capability/certification matrix.

**Acceptance criteria

- [ ] Matrix records generation/form-factor differences rather than flattening capabilities.
- [ ] Every claimed hardware class has a completed mandatory fixture result.
- [ ] Known deviations/blockers link to parity-ledger entries and release decisions.
- [ ] No untested class is described as certified.

### 70 — Advanced graphics controls

**Blocked by:** 12, 13

**What to build:** Implement the remaining reference-visible global/per-game advanced graphics controls as explicit capabilities rather than allowing them to disappear behind generic profile behavior.

**Acceptance criteria

- [ ] Reference controls cover anti-aliasing mode/method, morphological anti-aliasing, anisotropic filtering, texture filtering quality and surface format optimization where present.
- [ ] Tessellation mode and maximum tessellation level are exposed according to the fixed reference and provider capability.
- [ ] Wait for Vertical Refresh and OpenGL triple buffering are independently represented where applicable; no Vulkan setting is mislabeled as an OpenGL-only control.
- [ ] Shader-cache/reset behavior is represented only through a real provider or explicit unavailable/Class D outcome.
- [ ] All controls obey global/per-game inheritance and configured/effective-state truthfulness.

### 71 — Home dashboard parity integration

**Blocked by:** 05, 06, 08, 11, 12, 33, 45, 46

**What to build:** Compose the reference Home dashboard from the already working system, game, profile, media, performance/advisor and preference surfaces.

**Acceptance criteria

- [ ] Home includes the graphics-stack/software status card, recent or most-played game card, launch action and relevant profile/performance state.
- [ ] Home includes the quick graphics-profile/HYPR-RX-style control, compact performance summary and Game Advisor entry when eligible.
- [ ] Recent media appears with reference-compatible tile behavior and actions supplied by the Media library.
- [ ] Promotional/content region follows the captured reference layout and the Advertisements/promotional-content preference can suppress it without collapsing unrelated Home content.
- [ ] Favorites/notifications/search shell affordances remain consistent with the fixed reference while on Home.
- [ ] Home passes golden-reference visual and interaction parity with deterministic fixture data.
- [ ] The Home software/update card reflects distro-backed update availability and exposes the approved check/handoff/release-details action without taking package-install ownership.

### 72 — Application-wide performance certification

**Blocked by:** 07, 18, 31, 57, 71

**What to build:** Run the fixed performance certification matrix against the packaged application and close the global responsiveness, memory and runtime-overhead gates.

**Acceptance criteria

- [ ] Cold launch to interactive shell is <= 2.0 s and warm launch is <= 1.0 s on the checked-in certification baseline.
- [ ] Page-navigation response is <= 100 ms excluding intentionally asynchronous backend completion; warm-index first search results are <= 50 ms.
- [ ] UI render target is 60 FPS at the certification display mode and hardware discovery performs no blocking work on the GUI thread.
- [ ] Base GUI RSS is <= 250 MB excluding AI; gamewatch <= 50 MB RSS; sessiond/telemetry <= 50 MB RSS under the specified steady-state measurement procedure.
- [ ] Visible telemetry supports a 0.25 s minimum interval and overlay transport operates at the defined 10–60 Hz range without per-frame heap allocation.
- [ ] With the overlay disabled, dormant hooks add < 0.1% measurable average-FPS impact; a result >= 0.1% requires the spec-defined recorded exception/investigation.
- [ ] With the overlay visible at 1440p, CPU frame overhead is < 0.15 ms and GPU overlay render overhead is < 0.20 ms; the overlay path performs no synchronous disk I/O and no per-frame D-Bus round trip.
- [ ] 1440p60 hardware recording is the baseline capture target; 4K60 is required where encoder preflight confirms capability; median average-FPS capture overhead is <= 5% on each mandatory GPU-bound certification workload with 1% low/frame-time regression recorded/gated by the matrix.

### 73 — Final 1:1 parity closure

**Blocked by:** 06, 08, 14, 15, 16, 17, 22, 28, 29, 33, 35, 36, 37, 38, 41, 42, 43, 44, 45, 47, 48, 49, 50, 51, 52, 54, 55, 56, 57, 59, 60, 62, 63, 69, 70, 71, 72

**What to build:** Close the parity ledger and prove the RX-6000+ Linux product meets the defined 1:1 Adrenalin product-experience bar.

**Acceptance criteria

- [ ] Every applicable reference feature is verified Class A/B/C or explicitly documented Class D; every Windows-only exclusion is verified Class N with a Linux owner/disposition.
- [ ] Every production page satisfies page-level visual/interaction/error/accessibility/persistence/backend Definition of Done.
- [ ] All hardware, performance, capture, security, migration, lifecycle and uninstall release gates pass.
- [ ] No UI reports success for a failed/unverified backend, and the project can legitimately make the defined 1:1 claim.
