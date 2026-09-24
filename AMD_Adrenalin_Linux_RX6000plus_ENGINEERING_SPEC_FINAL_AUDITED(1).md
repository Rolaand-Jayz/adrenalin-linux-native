# AMD Adrenalin Linux — RX 6000+ Engineering Specification

**Status:** Ready for agent  
**Spec revision:** 1.0  
**Date:** 2026-09-23  
**Source of truth:** `AMD Software: Adrenalin Edition — Linux 1:1 Clone` PRD Revision 1.5, RX 6000+ scope  
**Product codename:** Adrenalin-Linux  
**Target:** Native Linux desktop application for AMD Radeon RX 6000-series and newer GPUs  
**Primary implementation:** C++20, Qt 6/QML, CMake/Ninja  
**Reference product:** AMD Software: Adrenalin Edition for Windows  
**Reference release:** AMD Software: Adrenalin Edition 26.9.1 Optional, released 2026-09-03  
**Reference UI baseline:** The 26.9.1 product generation, supplemented only by AMD 25.6.1–26.9.1 documentation where the selected hardware does not expose a conditional surface; any such supplemental surface remains hardware/reference-gated and must not override observed 26.9.1 behavior.  
**Parity boundary:** 1:1 Adrenalin product experience, not 1:1 Windows operating-system plumbing  
**Agent readiness:** This specification is intended to be sufficient for implementation agents to begin work without inventing product behavior.

---

## Problem Statement

Linux Radeon users do not have a first-party application that reproduces the integrated product experience of AMD Software: Adrenalin Edition while respecting native Linux ownership boundaries.

The missing experience is broader than a graphics-settings panel. Adrenalin combines:

- a hardware and system dashboard;
- game discovery and per-game profiles;
- global graphics controls;
- display controls;
- performance telemetry;
- GPU tuning;
- recording and streaming;
- instant replay and screenshots;
- an in-game overlay;
- Smart Technology surfaces;
- hotkeys and preferences;
- optional AI assistance;
- diagnostics and update visibility.

Existing Linux tools solve parts of that problem, but the product goal is not to assemble those tools into a loose launcher or to create an Adrenalin-inspired interface. The goal is to reproduce the applicable Adrenalin product experience with matching information architecture, interaction behavior, visual language, and user-facing capabilities.

The implementation must also avoid a category error: AMD Adrenalin on Windows owns portions of the Windows driver distribution and servicing flow. On Linux, the distribution, kernel, Mesa, firmware stack, compositor, package manager, and firmware/BIOS own those responsibilities. Recreating Windows driver-installation behavior inside this application would be incorrect.

The project therefore needs one coherent native application that:

1. reproduces applicable Adrenalin product behavior;
2. translates platform-dependent features into truthful Linux-native implementations;
3. explicitly classifies Windows-only responsibilities as not applicable rather than treating them as unfinished features;
4. never exposes a working-looking control whose backend does not actually perform the claimed operation;
5. remains safe under crashes, retries, daemon restarts, display-mode failures, and tuning failures;
6. is certifiable across RX 6000/RDNA2, RX 7000/RDNA3, and RX 9000/RDNA4 hardware.

---

## Solution

Build a native Linux Adrenalin clone composed of a Qt/QML desktop UI, an unprivileged per-user core service, a minimal privileged hardware-control daemon, specialized game/capture/display services, and native Vulkan/OpenGL runtime integration.

The Windows Adrenalin application is the canonical reference for applicable product-facing UX. Linux-native implementation details are allowed to differ underneath that interface, but the resulting user-visible behavior must remain faithful.

Every reference feature is classified into one of five implementation classes:

- **Class A — Native exact:** Linux exposes equivalent functionality directly.
- **Class B — Equivalent implementation:** Linux uses a different mechanism but can reproduce the same user-visible outcome.
- **Class C — Compatibility/runtime implementation:** parity requires a Vulkan/OpenGL layer, Gamescope, compositor integration, Proton/Wine integration, or another runtime component.
- **Class D — Backend unavailable or unresolved:** the product surface is applicable, but a truthful implementation does not yet exist. The feature remains an explicit parity blocker and may not be faked.
- **Class N — Not applicable to the Linux application boundary:** the Windows feature exists because of Windows driver packaging, Windows services, the registry, Windows Update, or another responsibility already owned by the Linux platform. Class N items do not block completion when their ownership rationale is documented.

The product certifies RX 6000-series and newer hardware only. Pre-RDNA2 GPUs are outside parity scope except where shared code must avoid regressions.

The primary behavioral test seam is the per-user session service. The GUI, game watcher, capture service, display guard, and optional AI service consume typed service contracts rather than reaching into each other's implementation state. Privileged mutation is isolated behind the system daemon. UI parity is tested independently through golden-reference visual and interaction tests.

---

## User Stories

1. As a Radeon RX 6000+ Linux user, I want the application to look and behave like AMD Adrenalin, so that the experience is immediately familiar.
2. As a Radeon user, I want the same applicable top-level navigation hierarchy as Adrenalin, so that features are where I expect them.
3. As a Linux user, I want Windows-only driver-management surfaces omitted or translated truthfully, so that the application does not pretend to own my distribution's graphics stack.
4. As a user, I want the application to start without requiring root, so that normal monitoring and configuration remain unprivileged.
5. As a user, I want unsupported hardware capabilities to be shown truthfully, so that a disabled or unavailable feature is never presented as working.
6. As a user, I want the Home page to summarize my Radeon system, current graphics stack, recent games, media, and performance state.
7. As a user, I want the application to identify my installed kernel, AMDGPU, Mesa, RADV, RadeonSI, libdrm, firmware, and optional ROCm stack.
8. As a user, I want the application to tell me when relevant distribution packages have updates available without installing or replacing them itself.
9. As a user, I want an explicit handoff to my distribution's update mechanism where supported.
10. As a user, I want Steam games detected automatically.
11. As a user, I want Proton games detected and associated with the correct launch target.
12. As a user, I want Lutris games detected.
13. As a user, I want Heroic games detected.
14. As a user, I want to add an executable or launcher manually when automatic discovery fails.
15. As a user, I want each detected game to have artwork, launch information, playtime, and profile state.
16. As a user, I want a global graphics profile that applies when no game-specific override exists.
17. As a user, I want per-game overrides that inherit global settings until I explicitly change them.
18. As a user, I want changing one value in a predefined profile to produce the same Custom-state behavior as Adrenalin.
19. As a user, I want applicable HYPR-RX-style profile behavior represented according to the reference and actual Linux capability support.
20. As a user, I want Radeon Super Resolution-equivalent behavior where the certified Linux runtime provider can reproduce it.
21. As a user, I want AFMF shown only when a verified Linux backend exists, and otherwise represented as a truthful parity blocker.
22. As a user, I want AMD FSR Upscaling and FSR Frame Generation software enablement/status treated as distinct features rather than substituted with RSR or AFMF.
23. As a user, I want Radeon Chill/frame-rate policy implemented per game where a verified runtime provider exists.
24. As a user, I want image sharpening implemented through an approved runtime/shader path when supported.
25. As a user, I want Anti-Lag-related controls to avoid claiming exact AMD behavior unless it has actually been reproduced and verified.
26. As a user, I want Enhanced Sync-equivalent behavior only where Linux present/compositor semantics can reproduce the user-visible outcome.
27. As a user, I want graphics controls to expose whether their effective value comes from global state, a game override, or an unavailable backend.
28. As a user, I want my connected displays enumerated correctly, including EDID-derived identity and capabilities.
29. As a user, I want FreeSync/Adaptive-Sync status and controls where the compositor and display stack expose them.
30. As a user, I want GPU scaling and scaling mode controls where Linux exposes a safe equivalent.
31. As a user, I want integer scaling where supported.
32. As a user, I want custom color controls that use the Linux color/compositor stack rather than destructively modifying monitor hardware by default.
33. As a user, I want color depth and pixel-format controls only where the display stack can verify the effective result.
34. As a user, I want custom resolutions where the compositor/display provider can safely apply and recover them.
35. As a laptop user with supported RX 6000+ hardware, I want Vari-Bright-style behavior represented only when a real platform interface exists.
36. As a hybrid-graphics user, I want SmartAccess Graphics-style routing represented only when a verified DRM/compositor/firmware path exists.
37. As a mobile Radeon user, I want SmartShift information or controls to reflect real firmware/SMU capability rather than fake independent power sliders.
38. As a user changing risky display settings, I want a visible confirmation countdown and automatic rollback if I lose output or fail to confirm.
39. As a user, I want risky display rollback to work even if the GUI or core session service crashes.
40. As a user, I want current FPS, frame time, GPU utilization, clocks, temperatures, VRAM use, power, fan speed, and CPU/system metrics where the hardware exposes them.
41. As a user, I want unavailable metrics to show N/A rather than misleading zero values.
42. As a user, I want metrics shown in an Adrenalin-compatible list/grid presentation.
43. As a user, I want to choose which metrics appear in the overlay and which are logged.
44. As a user, I want to log performance metrics to CSV with stable units and timestamps.
45. As a user, I want the in-game metrics overlay to resemble the reference Adrenalin overlay rather than MangoHud or another third-party UI.
46. As a native Linux gamer, I want the overlay to work with certified Vulkan/OpenGL titles.
47. As a Proton gamer, I want the overlay and per-game runtime features to work for certified Proton titles.
48. As a user, I want unsupported protected or anti-cheat processes reported truthfully rather than having the application attempt to bypass their integrity controls.
49. As a user, I want GPU tuning controls to expose only ranges actually supported by my device.
50. As a user, I want automatic tuning modes represented when a verified backend can perform them.
51. As a user, I want manual GPU frequency control where supported.
52. As a user, I want voltage or voltage-offset control only where the GPU/kernel interface safely exposes it.
53. As a user, I want VRAM tuning where supported.
54. As a user, I want fan tuning and a fan curve where supported.
55. As a user, I want Zero RPM behavior where supported.
56. As a user, I want power-limit control where supported.
57. As a user, I want tuning changes staged until I explicitly apply them when the reference uses Apply/Discard semantics.
58. As a user, I want a failed multi-value tuning transaction rolled back rather than partially applied.
59. As a user, I want the application to recover from a crash during tuning without blindly reapplying a suspect profile.
60. As a user, I want tuning profiles importable and exportable with hardware-compatibility validation.
61. As a user, I want a bounded GPU stress test that never bypasses platform thermal or power protections.
62. As a user with a compatible Ryzen system, I want CPU/System tuning surfaces represented only when the selected reference exposes them and a verified Linux backend exists.
63. As a user, I want recording to use native Linux capture and AMD hardware encoding where available.
64. As a user, I want to record the desktop, a game/window, or the applicable reference capture target.
65. As a user, I want screenshots from the same product surface and hotkey model as Adrenalin.
66. As a user, I want microphone and system audio capture with configurable tracks.
67. As a user, I want AVC/H.264, HEVC, and AV1 options only when the selected hardware/encoder backend supports them.
68. As a user, I want configurable recording resolution, frame rate, bitrate, audio bitrate, and quality presets.
69. As a user, I want Instant Replay to continuously preserve only the configured recent window without unbounded memory growth.
70. As a user, I want saving Instant Replay to preserve the active replay buffer even if the file save fails.
71. As a user, I want an Instant Replay save to contain the most recent configured interval with bounded timing error.
72. As a user, I want captured media to remain ordinary filesystem files rather than being locked inside an application database.
73. As a user, I want the Media page to index recordings and screenshots without becoming their sole owner.
74. As a user, I want live streaming through custom RTMP/RTMPS even if platform-specific account integrations are unavailable.
75. As a user, I want stream credentials stored through the desktop secret service rather than plaintext configuration.
76. As a streamer, I want multiple scenes.
77. As a streamer, I want image, video, camera, browser, indicator, and chat-style scene elements where the reference exposes them.
78. As a streamer, I want browser sources sandboxed.
79. As a streamer, I want scene elements to support reference-equivalent positioning, visibility, dimensions, opacity, and ordering.
80. As a streamer, I want camera chroma-key behavior where the reference exposes it.
81. As a user, I want recording/stream state to survive GUI navigation and remain controlled by the capture service rather than the page lifecycle.
82. As a user, I want capture output to remain valid or recoverable if the GUI crashes.
83. As a user, I want global hotkeys for overlay, metrics, recording, replay, screenshots, microphone, streaming, and scene actions where applicable.
84. As a Wayland user, I want global shortcuts integrated through the desktop portal rather than through an unsafe workaround.
85. As a user, I want hotkey conflicts detected before a binding is accepted.
86. As a user, I want notification history with read/unread state.
87. As a user, I want desktop toasts controlled by my preferences.
88. As a user, I want a global search field that can find pages, settings, games, display controls, tuning controls, capture controls, and preferences.
89. As a user, I want search results to navigate to the owning surface and reveal the relevant setting.
90. As a user, I want settings export/import to include product settings and profiles but exclude secrets.
91. As a user, I want importing settings to validate schema and hardware compatibility before applying changes.
92. As a user, I want display- or tuning-affecting imported settings protected by rollback behavior.
93. As a user, I want Factory Reset to reset application state without deleting my recordings or uninstalling Linux graphics packages.
94. As a user, I want Noise Suppression implemented through a local Linux audio-processing path when supported.
95. As a user, I want Smart Access Memory status derived from actual PCIe/platform state.
96. As a user, I want SmartAccess Video, Privacy View, and other proprietary Smart Technology features shown only when there is a truthful equivalent or an explicit parity blocker.
97. As a user, I want AMD Video Upscaling and related video-enhancement surfaces treated as real product capabilities rather than silently omitted.
98. As a user, I want encoder enhancement options exposed only when the selected Linux encoding path really implements them.
99. As a user, I want Variable Graphics Memory surfaced only on platforms that expose a real equivalent capability.
100. As a user, I want AMD Assistant-style automatic behavior only if it is present in the selected reference and its triggers can be implemented truthfully.
101. As a user, I want the optional AI/Chat feature to remain local-first and separate from core Radeon control.
102. As a user, I want the AI service to query application/hardware state through typed read-only tools rather than unrestricted system access.
103. As a user, I want any AI-triggered setting change to require the same validation and authorization as a UI-triggered change.
104. As a user, I want the AI component removable without affecting the rest of the application.
105. As a user, I want the application to remain useful if no AI component is installed.
106. As a user, I want the integrated web-browser surface reproduced only as a product feature and not used as the foundation of the application itself.
107. As a user, I want the application itself to be a native Qt/C++ program rather than Electron, a PWA, or a localhost-served web application.
108. As a user, I want startup and page transitions to remain responsive while hardware discovery happens asynchronously.
109. As a user, I want a service failure reported as unavailable/degraded rather than having the UI hang indefinitely.
110. As a user, I want state-changing requests to be idempotent so a timeout/retry cannot accidentally start duplicate recordings or apply a hardware change twice.
111. As a user, I want concurrent profile edits protected from silently overwriting newer state.
112. As a user, I want stale capability data rejected before a hardware mutation.
113. As a user, I want service restarts reconciled so the UI learns the actual current operation state rather than guessing from a process exit.
114. As a user, I want persistent tuning restored only when it is known safe and explicitly enabled.
115. As a user, I want uninstall/removal to quiesce active app-owned tuning, risky display state, recording/replay, and per-game runtime modifications before binaries disappear.
116. As a user, I want uninstall blocked rather than leaving unsafe state if quiescence cannot be verified.
117. As a user, I want the Linux distribution to remain the owner of kernel, Mesa, firmware, and package updates.
118. As a user, I want Windows Update, registry, Windows service, driver-package clean-install, and Driver Only workflows omitted as not applicable.
119. As a user, I want all omitted Windows-only surfaces documented so omission cannot be confused with forgotten work.
120. As a user, I want the project to certify RX 6000/RDNA2, RX 7000/RDNA3, and RX 9000/RDNA4 independently rather than assuming one generation's behavior applies to another.
121. As a mobile Radeon user, I want certification to include a real RX 6000+ mobile/hybrid system rather than extrapolating from desktop hardware.
122. As a multi-display user, I want certification to include VRR and multi-monitor behavior.
123. As a developer, I want one authoritative capability graph so UI code never guesses support from GPU model names.
124. As a developer, I want one authoritative user-session state owner so multiple processes cannot race to write the same database.
125. As a developer, I want every cross-process feature to have a typed, versioned interface before implementation merges.
126. As a developer, I want high-frequency telemetry separated from low-frequency RPC so the overlay does not require per-frame D-Bus calls.
127. As a developer, I want privileged hardware operations expressed semantically rather than as arbitrary filesystem writes.
128. As a developer, I want the privileged daemon to revalidate every mutation rather than trusting the GUI.
129. As a developer, I want feature-provider selection deterministic when multiple Linux mechanisms could implement the same product capability.
130. As a developer, I want the parity ledger to identify every applicable reference feature, implementation class, provider, hardware scope, test, and known deviation.
131. As a developer, I want Class D features visible as explicit unresolved work instead of hidden behind working-looking no-op controls.
132. As a developer, I want Class N features to name their real Linux responsibility owner so platform boundaries remain explicit.
133. As a developer, I want golden-reference screenshots and interaction scenarios to make 1:1 parity objectively reviewable.
134. As a developer, I want performance and capture acceptance driven by fixed certification fixtures rather than subjective "works well" judgments.
135. As a maintainer, I want schema migrations versioned and recoverable.
136. As a maintainer, I want diagnostics to identify selected providers, capabilities, permissions, compositor state, capture encoders, and recent recovery operations.
137. As a maintainer, I want logs structured by subsystem and automatically redacted for secrets.
138. As a maintainer, I want optional external integrations isolated behind providers so future backend replacement does not require a UI redesign.
139. As a release engineer, I want every production page to have loading, loaded, unavailable, permission-denied, unsupported, failure, disconnected, and stale-data behavior where applicable.
140. As a release engineer, I want every production page to pass visual, interaction, persistence, accessibility, and backend-integration gates before it is called complete.
141. As a release engineer, I want a release blocked if a claimed hardware class did not run its mandatory certification fixture.
142. As a release engineer, I want a release blocked if the UI reports success after a backend mutation failed.
143. As a release engineer, I want a release blocked if a working product surface has no parity-ledger entry or test evidence.
144. As a release engineer, I want a release blocked if a Class N exclusion lacks a concrete Linux ownership rationale.
145. As a release engineer, I want the project to call itself a 1:1 clone only when all applicable Class A/B/C features are verified, all Class D gaps are explicit, and all Class N exclusions are documented.
146. As a user, I want the integrated Web Browser surface when the selected reference exposes it, so that this reference-visible tool is not silently omitted.
147. As a user, I want System Tray behavior to match the applicable reference preference, so that background access and tray presence are predictable.
148. As a user, I want promotional/advertising content to obey the reference Advertisements preference, so that disabling it actually removes that content.
149. As a SteamVR user, I want the reference SteamVR integration surfaced only when a verified Linux SteamVR path exists, so that the control is truthful.
150. As a user, I want to choose the application language through the reference-style preference surface, so that localization is user-controlled.
151. As a user, I want Always On Top and Sidebar Position preferences to reproduce the reference behavior where applicable, so that window behavior is familiar.
152. As a user, I want Animation & Effects preferences to control application motion, including reduced-animation behavior, so that visual behavior follows my preference.
153. As a user, I want product telemetry/user-experience participation to be explicit opt-in, so that no usage data leaves the machine silently.
154. As a user, I want Game Advisor to appear after sufficient gameplay telemetry and give deterministic, explainable recommendations that only reference capabilities actually available on my system.
155. As a user, I want Issue Reporting and automatic Issue Detection surfaces where the selected reference exposes them, so that failures can produce a local diagnostic/report workflow without silently uploading data.
156. As a user, I want Image Inspector represented only when its workflow can be reproduced truthfully and privately, so that a proprietary or cloud-dependent reference feature is not faked.
157. As a user, I want Settings snapshot export/import presented as a user-facing recovery/migration workflow, so that I can move or restore application configuration without exporting secrets.
158. As a keyboard user, I want every production surface navigable without a mouse, so that the application remains operable through keyboard input.
159. As a user of assistive technology, I want screen-reader labels and semantic names for icon-only controls, so that controls are understandable without relying on visual appearance.
160. As a user, I want visible focus and critical success/failure states that do not rely on color alone, so that interaction state remains perceptible.
161. As a user, I want text scaling and reduced-animation behavior within reference-compatible bounds, so that accessibility does not require abandoning the Adrenalin layout.
162. As a user, I want the UI architecture to support localization from the start, so that language support can expand without rewriting control logic.
163. As an AI/Chat user, I want to upload local documents for local RAG, so that I can ask questions over my own files without requiring a cloud provider.
164. As an AI/Chat user, I want chat history stored locally, so that prior sessions remain available without becoming part of the core Radeon-control database.
165. As an AI/Chat user, I want the assistant to answer hardware/software, active-feature, game-library, graphics-stack version, GPU/CPU live-data, and display-information questions through typed product tools.
166. As an AI/Chat user, I want optional local image generation when installed and supported, so that the feature remains modular and never becomes a core-control dependency.
167. As a Linux gamer, I want non-Steam Steam shortcuts and Bottles games discovered alongside Steam, Lutris, and Heroic titles, so that the library reflects the launch ecosystems I actually use.
168. As a user, I want game-marked `.desktop` launchers and detectable AppImages discovered, so that native Linux games are not limited to store-managed installs.
169. As a user manually adding a game, I want native ELF, AppImage, `.desktop`, and Wine/Proton-associated Windows executables accepted, with shell launchers requiring explicit confirmation because process matching is less reliable.
170. As a user, I want global search to work fully offline and jump directly to the owning page/section, so that finding a control never depends on a network service.
171. As a user, I want recent media actions to open, play, reveal, delete with confirmation, and expose applicable reference share/stream actions, so that the Home media card is functional rather than decorative.
172. As a user, I want long-running actions to show progress, safe cancellation where possible, and explicit success/failure, so that no control appears to ignore my input.
173. As a user, I want the reference Favorites affordance whenever the selected build exposes it, so that shell-level reference functionality is not silently dropped.
174. As a user entering hardware tuning for the first time, I want a clear reference-equivalent risk acknowledgement before dangerous controls become actionable, so that elevated hardware risk is explicit rather than implied.
175. As a user whose tuning or stress test causes a GPU reset, reboot, or other instability, I want the last transaction treated as suspect and the application to restore a known-safe state or block further tuning with a persistent recovery warning until safety is verified.

---

## Implementation Decisions

### Product and scope model

**ID-001 — Native desktop product.**  
The application is a native Linux desktop product implemented in C++20 with Qt 6/QML. Electron, browser-hosted UI, a PWA, or a localhost-served web application are not acceptable substitutes. Qt WebEngine is used only for reference features that inherently embed web content, such as the integrated browser or streaming Browser Source.

**ID-002 — RX 6000+ certification boundary.**  
The supported certification scope begins at Radeon RX 6000 / RDNA2 and includes RX 7000 / RDNA3, RX 9000 / RDNA4, and applicable RDNA2+ integrated/mobile Radeon configurations. Pre-RDNA2 product parity is out of scope. Shared code must still fail safely on unsupported hardware.

**ID-003 — Product parity, not Windows plumbing parity.**  
The reference Windows application defines product-facing UX. Windows-only responsibilities such as Windows driver package installation, rollback, registry/service management, Windows Update integration, Driver Only installation, and Windows clean-install flows are Class N and are not reproduced as active Linux functionality.

**ID-004 — Full application surface is the certification target.**  
The Linux project does not recreate Windows Full/Minimal/Driver Only installer modes as product requirements. Optional Linux package splits may exist, but once installed they do not reduce the parity obligations of the full application.

**ID-005 — Five-class parity model.**  
Every reference feature receives Class A, B, C, D, or N. No applicable reference feature may remain unclassified.

**ID-006 — No fake success.**  
A UI control may not report active/successful behavior unless an approved provider can verify the effective state. A no-op provider can report unavailable only.

**ID-007 — Reference-gated unknowns.**  
When exact labels, defaults, ranges, states, or conditional presentation depend on the Windows reference, implementation must wait for reference capture rather than inventing behavior.

### System ownership and processes

**ID-008 — Single user-session authority.**  
The unprivileged session service is the authoritative owner of user-session application state, schema migration, persistence, capability graph, telemetry production, search, notifications, global-hotkey portal sessions, normal display coordination, update visibility, settings snapshots, and service reconciliation.

**ID-009 — GUI is a client, not a state owner.**  
The GUI renders product state and issues typed commands. It does not directly write the production database or perform hardware mutations.

**ID-010 — Single database writer.**  
Only the session service writes the core SQLite database. Game, capture, GUI, and AI processes report finalized state through typed service contracts.

**ID-011 — Privileged mutation isolation.**  
The system daemon is the only elevated process. It performs validated tuning writes and narrowly scoped privileged display operations. It never renders UI.

**ID-012 — Semantic privileged API.**  
Privileged interfaces expose operations such as applying a tuning profile or prepared display operation. They do not expose arbitrary path/write primitives.

**ID-013 — Game watcher lifecycle.**  
Game discovery/process tracking/profile activation remains independent of the GUI lifecycle. A game profile must remain active and be safely restored even if the GUI closes.

**ID-014 — Capture service lifecycle.**  
Recording, streaming, instant replay, camera/audio capture, and media finalization remain independent of page/GUI lifecycle.

**ID-015 — Display guard ownership.**  
Any display operation that could remove visible output is handed to a transient display guard before the first risky mutation. Once accepted, the guard owns apply, countdown, commit, and rollback until a verified terminal state.

**ID-016 — Optional AI isolation.**  
AI has separate storage and lifecycle. Core Radeon control never depends on it.

### Capability and provider model

**ID-017 — Capability graph is authoritative.**  
Every hardware-facing UI reads support, valid ranges, provider identity, and effective state from one versioned capability graph. UI code does not infer capability from GPU marketing names.

**ID-018 — Capability generations protect mutation.**  
A state-changing hardware request uses the capability generation it was validated against. Stale capability generations are rejected before mutation.

**ID-019 — Deterministic provider policy.**  
If multiple Linux backends can implement a feature, provider selection is defined by an explicit policy, not by plugin registration order or discovery timing.

**ID-020 — Provider truthfulness.**  
A provider must expose capability, configured value, effective value, and failure reason as appropriate. It cannot claim a product semantic it only approximates materially.

**ID-021 — Initial RSR path.**  
RSR parity begins with the approved Gamescope/runtime scaling provider. A different provider may replace it only after passing the same parity and certification gates.

**ID-022 — Runtime feature isolation.**  
Frame limiting, sharpening, overlay, scaling, and other runtime behavior use explicit Vulkan/OpenGL/Gamescope providers rather than globally mutating unrelated process environments.

### Cross-process contracts

**ID-023 — Interface-first development.**  
Every cross-process feature requires a versioned typed interface, generated bindings, mock implementation, and contract test before feature code merges.

**ID-024 — Session/system bus split.**  
User-session services live on the session bus. Privileged tuning/low-level display operations live on the system bus.

**ID-025 — Service readiness states.**  
Long-lived services expose initialization state and do not accept state-changing work as READY until mandatory recovery/reconciliation has completed.

**ID-026 — Operation IDs.**  
State-changing operations use stable operation IDs for retry/deduplication. Idempotency keys include the method/phase identity so legitimate multi-phase operations can share one logical operation without being mistaken for duplicates.

**ID-027 — Optimistic concurrency for mutable records.**  
Concurrent profile/settings edits use expected revisions. A stale revision fails explicitly rather than silently overwriting newer state.

**ID-028 — Typed errors.**  
Cross-process failures are represented by stable semantic error codes. User-facing text is derived at the UI boundary rather than encoded as backend control flow.

**ID-029 — Event reconciliation.**  
Events are useful for live updates but are not authoritative after reconnect. Clients reconcile current state through explicit read APIs after service restarts or event gaps.

### Telemetry

**ID-030 — High-frequency telemetry uses shared memory.**  
Per-frame/high-rate metrics do not use D-Bus round trips.

**ID-031 — Single producer, multiple readers.**  
The session service is the sole telemetry-ring producer. Consumers map the stream read-only.

**ID-032 — Tear-free publication.**  
Each telemetry slot uses a single atomic sequence guard with odd=in-progress and even=committed semantics. Readers accept a sample only if the guard is equal and even before and after the payload copy.

**ID-033 — Versioned telemetry ABI.**  
The telemetry memory layout defines fixed-width types, alignment, endianness, versioning, sequence wrap behavior, and definition generations.

**ID-034 — Unknown is not zero.**  
Unavailable or stale telemetry is represented explicitly, never as a synthetic numeric zero.

### Persistence and identity

**ID-035 — XDG storage model.**  
User configuration, data, cache, and state use XDG Base Directory locations. Persistent privileged recovery state uses root-owned system state storage.

**ID-036 — Stable entity identities.**  
GPU, display, game, profile, capture, and operation identities are stable enough to reconcile across restarts without relying on display labels or translated names.

**ID-037 — Versioned schema.**  
SQLite and exported settings snapshots use explicit schema versions and migration logic.

**ID-038 — Secrets are not database settings.**  
Stream tokens/keys and equivalent credentials use the desktop secret service. There is no plaintext fallback.

### Graphics and game profiles

**ID-039 — Global/per-game inheritance.**  
Game profiles inherit global settings until overridden. Per-game mutation never changes the global value.

**ID-040 — Reference profile behavior.**  
Predefined Adrenalin profiles are data-driven. Modifying one of their constituent values produces the same Custom behavior as the reference.

**ID-041 — Process-scoped runtime changes.**  
Per-game runtime variables/layers/wrappers apply only to the selected game context and are removed/restored when the game session ends.

**ID-042 — Proton-aware process association.**  
Game tracking resolves launcher ancestry, Wine/Proton prefixes, and final render processes rather than assuming the initial Steam process is the game.

**ID-043 — Anti-cheat boundary.**  
The overlay/runtime layer never bypasses protected-process or anti-cheat integrity mechanisms. Unsupported protected titles are explicit compatibility results.

### Display

**ID-044 — Compositor-safe ownership.**  
Wayland display control uses compositor/KScreen/portal-supported paths first. The application does not seize DRM master from the compositor as normal operation.

**ID-045 — X11 provider.**  
Tier-1 X11 display management uses **libXrandr** for output enumeration, mode selection, placement, rotation, and other properties exposed through XRandR. Production code does not shell out to the `xrandr` command. Direct DRM is not used while the X server owns the display unless a specific operation is proven safe, capability-gated, and recorded in the parity ledger.

**ID-046 — Risky display transactions are two-party crash-safe commits.**  
The user-side display guard and privileged daemon maintain durable recovery information sufficient to determine whether a change is pending, kept, reverted, or recovery-required after arbitrary process crashes.

**ID-047 — Rollback requires no second prompt.**  
Authorization required for a risky display operation is obtained before the mutation. Recovery/rollback must remain possible if the screen becomes unusable and cannot depend on another interactive authorization prompt.

**ID-048 — Confirmation window.**  
Output-risking display changes use a 15-second confirmation timer unless the captured reference establishes a different value that is then adopted deliberately.

**ID-049 — Display provider verifies effective state.**  
A requested mode/color/format change is successful only when the provider can read back or otherwise verify the effective result.

### Tuning

**ID-050 — Capability-bounded tuning.**  
Tuning limits come from discovered kernel/device capability, not a hardcoded GPU table.

**ID-051 — Transactional apply.**  
A tuning apply snapshots current state, validates all requested values, performs writes in a deterministic order, verifies resulting state, and rolls back prior writes on failure.

**ID-052 — Durable privileged journal.**  
The privileged daemon writes a root-owned, atomic, integrity-checked recovery journal before the first dangerous write.

**ID-053 — Recovery before readiness.**  
After restart, tuning recovery/reconciliation completes before the daemon reports itself ready for new mutations.

**ID-054 — Safe persistent tuning.**  
Persistent tuning is opt-in. A profile associated with failure is not automatically re-applied indefinitely.

**ID-055 — Bounded stress testing.**  
Stress-test workloads may exercise the GPU but never intentionally disable thermal, current, or power protections.

### Recording, replay, streaming, media

**ID-056 — Native Linux capture.**  
PipeWire is the primary capture path. AMD hardware encoding uses the verified Linux encoder stack.

**ID-057 — Efficient data path.**  
Capture avoids unnecessary GPU→CPU→GPU transfers when a zero/low-copy provider path exists.

**ID-058 — Replay ring is bounded.**  
Instant Replay uses a bounded encoded ring buffer with configurable duration.

**ID-059 — Replay save and replay pipeline are separate states.**  
A file-save failure such as disk-full does not disable a healthy replay buffer.

**ID-060 — Media remains filesystem-owned.**  
The application indexes media but does not make SQLite the sole owner of recording/screenshot bytes.

**ID-061 — Secret-safe streaming.**  
Custom RTMP/RTMPS is required. Third-party account integrations use supported OAuth/API flows when implemented.

**ID-062 — Sandboxed browser source.**  
Web content used in streaming scenes runs inside the supported sandboxed WebEngine path.

### Hotkeys and Wayland portals

**ID-063 — Portal-owned global shortcuts on Wayland.**  
The session service owns one application shortcut session using stable action identifiers.

**ID-064 — Portal lifecycle is explicit.**  
The application handles shortcut-session recreation, stale permissions, conflict outcomes, and portal-version compatibility instead of assuming persistent OS-global registration.

**ID-065 — Wayland capture respects portal permissions.**  
Screen/window capture uses the ScreenCast/PipeWire portal contract or a certified compositor-specific path; it does not bypass Wayland security.

### Smart Technology and conditional features

**ID-066 — Smart features require real Linux semantics.**  
SmartShift, SmartAccess Graphics, SmartAccess Video, Privacy View, Vari-Bright, Variable Graphics Memory, AMD Assistant, and similar conditional features are implemented only through verified platform/runtime mechanisms or remain Class D.

**ID-067 — SAM status is truthful.**  
Resizable BAR / large-BAR state may be detected and displayed even when no safe runtime toggle exists.

**ID-068 — Noise suppression is an equivalent implementation.**  
The Linux path uses PipeWire virtual nodes and a local denoiser. The UI may claim GPU processing only when the selected backend actually uses the GPU.

**ID-069 — Video-quality features are explicit parity families.**  
AMD Video Upscaling, Geometric Downscaling, encoder enhancements, and reference-visible video profiles are tracked independently rather than hidden under generic media quality.

### AI

**ID-070 — AI is local-first.**  
After model installation, chat and system queries can operate locally.

**ID-071 — Typed tool access.**  
AI queries application and hardware state through typed interfaces, not unrestricted shell/root access.

**ID-072 — Mutating AI actions use normal control paths.**  
Any AI-requested change goes through the same validation, authorization, idempotency, and confirmation path as an equivalent GUI action.

**ID-073 — Resource suspension.**  
The AI service can yield/suspend resource-heavy work during demanding games.

### UI and parity

**ID-074 — Custom Adrenalin component system.**  
Production pages use application-styled QML components rather than native Breeze/GNOME/default Qt controls.

**ID-075 — Golden reference is authoritative for applicable UI.**  
Every production screen has controlled Windows reference captures covering default, hover/focus/open state, changed state, disabled/unavailable state, confirmation, failure, and success where reproducible.

**ID-076 — Reference geometry beats Linux redesign.**  
Linux conventions do not justify moving navigation, replacing control types, changing density, or exposing raw sysfs terminology in normal product surfaces.

**ID-077 — Linux ownership can change wording/action only when literal Windows behavior would be false.**  
System/driver surfaces may be translated into graphics-stack information and package-manager handoff while retaining the closest reference composition.

**ID-078 — Dynamic UI comes from capability state.**  
Conditional pages and controls are selected by capability/reference rules rather than hardcoded GPU-name conditionals in QML.

### Packaging and lifecycle

**ID-079 — Native packages are canonical.**  
Arch/CachyOS is Tier 1; Fedora and Ubuntu/Debian are Tier 2. Flatpak is non-canonical because the full product requires privileged/device/runtime integration.

**ID-080 — Install does not install the Linux graphics stack.**  
Installing this application must not replace the kernel, Mesa, firmware, or equivalent distribution packages.

**ID-081 — Uninstall must quiesce application-owned mutable state.**  
Before normal removal, active risky display transactions are resolved, application-owned tuning/runtime state is restored where required, active media pipelines are finalized safely, and temporary game runtime modifications are removed.

**ID-082 — Unsafe uninstall can fail closed.**  
If safe quiescence cannot be verified, normal package removal is refused rather than knowingly leaving hazardous application-owned state.

### Release and parity accounting

**ID-083 — Mandatory parity ledger.**  
Every reference feature has a row containing reference version/screen, classification, Linux owner/disposition/provider, hardware scope, test evidence, known deviation, and issue ownership.

**ID-084 — Class N completion rule.**  
A Class N item is complete only when the real Linux responsibility owner is named and the application's disposition is documented as not-applicable, status-only, or handoff-only.

**ID-085 — Class D remains visible debt.**  
A Class D feature can ship only as an explicit blocker/deviation according to the release policy; it cannot disappear from the ledger.

**ID-086 — 1:1 claim is earned.**  
The project may call itself a 1:1 Adrenalin Linux clone only when applicable Class A/B/C features are verified, Class D limitations are enumerated, Class N boundaries are documented, and the required UI/interaction/hardware certification gates pass.

### State, lifecycle, and failure semantics

**ID-087 — Controller state and operation result are distinct.**  
Reusable subsystem controllers return to their base state after a terminal operation result is durably recorded and published. A successful or recoverable terminal result must not leave an otherwise healthy subsystem permanently busy. Recovery-blocked state is reserved for failures whose safe state cannot be verified.

**ID-088 — Tuning state machine is normative.**  
The tuning controller follows `IDLE -> VALIDATING -> AWAITING_AUTHORIZATION -> SNAPSHOTTING -> APPLYING -> VERIFYING -> APPLIED -> IDLE`. Pre-apply failures produce `FAILED_NO_CHANGE -> IDLE`; post-mutation failures enter `ROLLING_BACK -> ROLLED_BACK -> IDLE`, and rollback failure enters `RECOVERY_BLOCKED`. Only one tuning transaction per tuning subject runs at a time; concurrent apply returns `BUSY`.

**ID-089 — Recording, replay, and stream controllers are reusable.**  
Recording follows `IDLE -> PREPARING -> RECORDING -> FINALIZING -> IDLE`, with `ERROR -> CLEANUP -> IDLE` for recoverable failure. Instant Replay follows `DISABLED -> STARTING -> BUFFERING`, with save operations `BUFFERING -> SAVING -> BUFFERING`, recoverable `SAVE_FAILED -> BUFFERING`, and unrecoverable `PIPELINE_ERROR -> STOPPING -> DISABLED`. Live streaming returns to `OFFLINE` after stop/error cleanup; automatic reconnect uses bounded exponential backoff and does not retry authentication failure indefinitely.

**ID-090 — Game and display state machines are normative.**  
A game session moves through discovered, launching, process-resolution, active, exiting, and complete states; loss of the tracked process is a distinct `LOST` result that still restores temporary state. Display mutation distinguishes failure-before-change from failure-after-change. Any failure after mutation begins must revert; inability to verify known-good restoration enters `RECOVERY_BLOCKED`.

**ID-091 — Canonical typed result contract.**  
Cross-process state-changing calls return a structured result containing at least `code`, `operation_id`, `human_message_key`, `diagnostic_message`, `retryable`, `provider`, and `subject_id`. The stable v1 result codes are `OK`, `INVALID_ARGUMENT`, `UNSUPPORTED`, `NOT_FOUND`, `PERMISSION_DENIED`, `AUTHORIZATION_CANCELLED`, `CANCELLED`, `INTERACTION_REQUIRED`, `BUSY`, `CONFLICT`, `STALE_REVISION`, `INCOMPATIBLE_VERSION`, `BACKEND_UNAVAILABLE`, `BACKEND_FAILURE`, `TIMEOUT`, `DEVICE_DISCONNECTED`, `STALE_CAPABILITY`, `VALIDATION_FAILED`, `APPLY_FAILED`, `VERIFY_FAILED`, `ROLLBACK_FAILED`, `IO_ERROR`, `ENCODER_UNAVAILABLE`, `PORTAL_DENIED`, `AUTH_REQUIRED`, `NETWORK_ERROR`, and `INTERNAL_ERROR`. UI behavior is driven by these typed codes, never by parsing English diagnostic strings.

**ID-092 — Threading domains are explicit.**  
Blocking I/O, hardware probes, package queries, filesystem scans, PipeWire negotiation, network operations, and privileged calls never execute on the Qt GUI thread. Session/database work runs in session-service workers; telemetry has a dedicated producer context; capture uses backend-appropriate real-time threads; overlay rendering performs no blocking IPC; privileged hardware mutations are serialized per device/subject.

**ID-093 — Events are sequenced, versioned hints, not durable truth.**  
Every cross-process event carries a service-instance identity, service generation, monotonic per-service event sequence, and stable subject identity. Event gaps, generation changes, or reconnects invalidate incremental assumptions and force an authoritative snapshot refresh. Device removal cancels queued operations against that device with a typed disconnected result.

**ID-094 — Service generation invalidates stale client state.**  
A service restart creates a new instance identity. A wholesale published-state reset increments its service generation. Any change invalidates cached handles, subscriptions, shared-memory interpretation, pending view-model assumptions, and cached capability snapshots tied to the prior generation. Reconnect uses bounded exponential backoff and each service reconciles durable/active work before advertising READY.

**ID-095 — Stable identity forms are domain contracts.**  
Stable identities are typed and opaque: CPU package, PCI-addressed GPU, GPU+connector+EDID-derived display, source-qualified game, UUID-backed profile/capture/scene/transaction. Display labels, localized names, array indexes, and transient process IDs are not persistent identity.

**ID-096 — Persistent relational invariants are explicit.**  
One global graphics profile is active; game profiles store overrides unless reference behavior requires snapshots; game sessions record the effective profile revision used; tuning transactions reference one typed subject and source profile revision; stream accounts store only secret-store references; scenes own ordered elements; hardware/capability rows are reconciled rather than trusted forever. Foreign-key delete behavior is explicit and multi-entity mutations are atomic.

**ID-097 — Profile precedence is deterministic.**  
For game-scoped graphics settings, precedence is reference/application default < global profile < game override < technically required transient provider constraint. Transient constraints may alter effective value but never silently rewrite configured values. The product exposes configured and effective values, plus the reason when they differ.

### Security, recovery, and delivery gates

**ID-098 — Authenticated peer identity is authoritative.**  
Privileged calls derive caller UID/PID/session identity from authenticated bus peer credentials. Caller-supplied identity fields are never trusted for authorization. Polkit authorization is separated at least into tuning apply, tuning persistence, and low-level display actions rather than one blanket privileged action.

**ID-099 — Trust boundaries remain explicit.**  
Unprivileged services, privileged daemon, injected game process, compositor/portals, network providers, browser content, imported archives, and local AI tooling are distinct trust zones. Launch arguments are constructed without shell concatenation, browser sources are sandboxed, imported archives are path-traversal/archive-bomb checked, and AI cannot bypass typed authorization paths. A threat model is required before the first release containing privileged hardware writes.

**ID-100 — Persistent tuning startup is deliberately conservative.**  
GPU tuning persistence is opt-in, initial releases do not apply it pre-login, and session startup waits a **10-second safety grace period** before reapplying a validated matching profile. Unclean prior shutdown, changed capability fingerprint, or evidence of reset/failure suppresses automatic reapply and creates a warning. CPU/System tuning persistence does not inherit GPU behavior automatically.

**ID-101 — Background portal interaction cannot fabricate consent.**  
A background service that needs an interactive portal but lacks a suitable user-visible parent returns `INTERACTION_REQUIRED`. Restore tokens are used only within portal semantics; invalid/revoked tokens are retired. Instant Replay cannot claim buffering until an actual capture stream exists. Global shortcut ownership remains centralized in the session service.

**ID-102 — Pull requests have mandatory quality gates.**  
Applicable changes must pass debug and RelWithDebInfo builds, unit/integration tests with mocks, Qt/QML tests, formatting, static analysis, ASan/UBSan where eligible, schema migration tests, malformed-input/fuzz coverage for persisted/imported formats, affected visual regression tests, and dependency/license inventory. New UI requires a reviewed golden/fixture update; new capability providers require capability tests; new privileged methods require validation tests and security review; new persisted fields require migration coverage.

**ID-103 — Implementation order is dependency-driven.**  
Work begins with repository/bootstrap/CI, reference capture/parity ledger, design tokens/components, shell/navigation, provider abstractions/mocks, identities/persistence, then real read-only hardware/telemetry. Privileged tuning, gamewatch/profiles, overlay, capture/replay, streaming/scenes, display control, Smart Technology, difficult graphics-provider parity, optional AI, and final parity closure follow in that dependency order. Parallel work is allowed only after prerequisite contracts exist.

**ID-104 — Missing contracts block production implementation.**  
Before a subsystem begins production implementation, its blocking artifact must exist: typed IPC + mock + contract test for cross-process work; migration/schema tests for persistence; reference capture/token mapping/fixture for production UI; capability/rollback/restart tests for hardware writes; provider-policy/activation-restoration tests for graphics features; portal fixtures for Wayland capture; certification matrices for capture; ABI/budget fixture for overlay; malicious-input corpus for imports; auth/secret-flow spec for stream integrations. Missing artifacts permit only isolated mock/research work, not invention of permanent behavior.

### Canonical service interfaces

**ID-105 — D-Bus topology is fixed for v1.**  
The session bus exposes one core service `org.adrenalinlinux.Session1` at `/org/adrenalinlinux/Session` with `Hardware1`, `Telemetry1`, `Profiles1`, `Settings1`, `Display1`, `Hotkeys1`, and `Notifications1`. Separate session services are `GameWatch1`, `Capture1`, `DisplayGuard1`, and optional `AI1`, each with its matching root object/interface. The system bus exposes `org.adrenalinlinux.System1` at `/org/adrenalinlinux/System` with `Tuning1` and `LowLevelDisplay1`. The numeric suffix is the breaking interface-major version; additive compatible changes stay within the major.

**ID-106 — Every long-lived service has a common readiness contract.**  
Each long-lived service root publishes initialization state, service-instance UUID, service generation, API major/minor, and last initialization error. Initialization state is one of STARTING, RECOVERING, READY, DEGRADED, or FAILED. State-changing work is rejected while mandatory recovery is incomplete; it is never silently queued across an unvalidated recovery boundary.

**ID-107 — Core session-service operation families are fixed.**  
The minimum v1 surface includes device listing/static info/capabilities; telemetry stream open/definitions/subjects; global and game profile read/update; settings read/update/export/import; display list/state/validate/apply; hotkey list/update; and notification list/mark-read. Exact wire signatures live in the versioned interface schema, but an implementation may not merge while omitting one of these required operation families.

**ID-108 — Privileged operation families are fixed.**  
Tuning exposes current state, validation, apply, operation query, reset, and stress test. Low-level display exposes validate, prepare, apply-prepared, revert-prepared, finalize-prepared, terminal acknowledgement, and operation query. Prepared operations are capability-bound and cannot be repurposed to a different display/value.

**ID-109 — Game/capture/guard operation families are fixed.**  
GameWatch exposes game listing, rescan, launch, and current-session query. Capture exposes start/stop recording, enable/disable replay, save replay, screenshot, start/stop stream, and current-state queries required for reconciliation. DisplayGuard exposes start transaction, transaction query, confirm keep, and request revert.

**ID-110 — Telemetry open is a negotiated contract.**  
Opening telemetry returns a read-only shared-memory handle plus metric and subject definition snapshots matching the stream generations. Definition-generation, producer-generation, ABI-major, or mapped-size changes invalidate cached interpretation and require refresh/reopen before additional samples are trusted.

### Crash-critical durability contracts

**ID-111 — Privileged recovery records use crash-durable atomic replacement.**  
Before the first hardware mutation, the privileged daemon writes a root-owned recovery record containing operation identity, subject/capability fingerprint, original values, intended values, phase, schema version, and integrity data. The record is non-world-writable, must not follow a user-controlled symlink/path, and is updated by same-filesystem temporary write -> file fsync -> atomic rename -> parent-directory fsync. The daemon must be able to reopen and validate the record before mutation begins. Phase changes that alter recovery interpretation are durably recorded before the next irreversible step.

**ID-112 — Risky display change uses explicit two-party terminal ordering.**  
The display guard durably records `PREPARED` before mutation. After verified temporary apply it records `APPLIED_AWAITING_CONFIRM` and runs the confirmation timer. On Keep, the privileged side verifies the intended state and durably records a terminal `COMMITTED` result before the guard records `COMMIT_VERIFIED`. On revert/timeout, the privileged side restores/verifies known-good state and durably records terminal `REVERTED` before the guard records `REVERT_VERIFIED`. The guard then acknowledges the matching privileged terminal result; only after matching terminal records/acknowledgement may recovery records be compacted. A crash between either side's terminal writes must never turn a verified commit into an automatic revert or an unverified apply into a commit.

**ID-113 — Display recovery is revert-first unless commitment is durable.**  
On restart, a durable privileged `COMMITTED` result is authoritative evidence to preserve the intended state. A durable `REVERTED` result is authoritative evidence of restoration. Any nonterminal applied/indeterminate post-mutation record without durable commit proof is revert-first. Corrupt or identity-mismatched recovery data blocks further app-owned mutation for the affected subject until reconciliation; it is never discarded as if nothing changed.

**ID-114 — Mutation ownership survives client disappearance.**  
Once a privileged tuning/display mutation has begun, loss of the caller does not cancel verification or rollback. The owner completes the safety state machine, persists the operation result, and a reconnecting client queries by operation ID. Capture/stream finalization likewise survives GUI disconnect; explicit user cancellation is distinct from client disappearance.

**ID-115 — Retry deduplication survives meaningful restarts.**  
Completed idempotency records are retained for at least the current service instance and at least 24 hours for durable side effects. Operations whose effects outlive their owner process persist enough deduplication metadata to reconcile the same method/operation ID after restart. Retrying the same canonical request returns the authoritative in-progress/final result; reusing the same method/operation ID with different payload returns conflict.

### Build and process lifecycle contracts

**ID-116 — Canonical build workflow and component switches are fixed.**  
A clean Tier-1 checkout configures with CMake/Ninja, builds, and runs CTest without project-specific wrapper magic. Production packaging defaults to RelWithDebInfo. The root build exposes switches for GUI, session service, display guard, privileged daemon, gamewatch, capture, overlay, optional AI, tests, and sanitizers; all core components default on except AI and sanitizers. Required dependencies fail configure with actionable diagnostics, optional providers degrade to unavailable capability, generated code stays out of the source tree, and new project warnings fail CI.

**ID-117 — Stable application identity is `org.adrenalinlinux.App`.**  
The desktop application ID and portal host identity use `org.adrenalinlinux.App`; a public display name may change independently. Changing the application ID after release is a migration because it can invalidate portal consent/state and therefore requires explicit migration/re-consent handling.

**ID-118 — Service activation is deterministic.**  
The GUI is user-launched; sessiond and gamewatch are per-user D-Bus/systemd-user activated; display guard is a per-transaction transient user service only for output-risking display work; capture activates on first capture/stream need and remains resident while replay requires it; the privileged daemon is system-D-Bus activated only for privileged operations; overlay loads only through an explicitly supported game launch path; optional AI starts only when installed and requested. Initial certification assumes systemd user/system units plus D-Bus activation.

**ID-119 — Clean shutdown follows safety ownership.**  
User-session shutdown first stops accepting new mutations, then flushes/finalizes capture state, restores temporary game-scoped runtime state, commits/rolls back active persistence transactions, releases shared-memory/session handles, preserves only explicitly enabled safe persistent tuning, and records a clean-session marker. GUI termination alone must not stop recording, streaming, replay buffering, game-profile state, hotkeys, telemetry production, or active recovery transactions.

### Canonical product surface inventory

**ID-120 — Top-level navigation follows the reference.**  
The full application exposes Home, Gaming, Record & Stream, Performance, and Smart Technology as the canonical top-level destinations. AI/Chat is conditional when the selected reference and capability rules expose it. Search, notifications, Settings, back/forward behavior where present, and reference-equivalent window controls remain shell-level affordances rather than separate Linux redesigns.

**ID-121 — Home is a reference-style dashboard, not a generic status page.**  
Home includes the applicable reference card families: application/graphics-stack status, recent or most-played game, active graphics/HYPR-RX-style profile quick control, recent media, compact performance summary, Game Advisor entry when sufficient telemetry exists, and the reference promotional/content region subject to its preference. The Windows driver card is translated into truthful Linux graphics-stack/update visibility rather than driver installation.

**ID-122 — Gaming hierarchy is fixed.**  
Gaming contains Games, Graphics, Display, and any Advisors sub-surface present in the selected reference. Games contains discovery/manual-add/library/detail/launch/performance/profile flows. Graphics contains predefined profiles plus all applicable global/per-game graphics controls. Display contains per-display Radeon controls and their per-game overrides only where the reference and Linux provider semantics support them.

**ID-123 — Record & Stream hierarchy is fixed.**  
Record & Stream contains Record, Live Stream, Scene Editor, Media, and Settings. Recording exposes desktop/target capture, screenshot, microphone/camera and advanced encoding/audio/replay settings. Live Stream exposes account/custom RTMP, scene, microphone/camera, status/chat and advanced stream settings. Scene Editor owns scenes and ordered browser/image/GIF/video/indicator/camera/chat elements. Media indexes finalized and recoverable output.

**ID-124 — Performance hierarchy is fixed.**  
Performance contains Metrics, Tuning, and Settings. Metrics owns gauges/graphs/list-grid presentation, overlay/logging selection, logging controls and sampling. Tuning owns automatic/preset/manual controls, GPU/VRAM/fan/power sections, applicable CPU/System tuning, profiles, Apply/Discard/Reset and stress testing. Performance Settings owns sampling, overlay visibility/output, logging location and overlay presentation controls.

**ID-125 — Settings hierarchy and preference families are fixed.**  
Settings contains System, Graphics, Display, Audio & Video, Hotkeys, and Preferences. System owns application/graphics-stack information, update visibility/handoff, settings snapshots, Factory Reset, diagnostics/issue-reporting entry points where applicable. Audio & Video owns Noise Suppression and reference video profile/enhancement surfaces. Hotkeys owns the action registry and bindings. Preferences tracks the applicable reference families including In-Game Overlay, SteamVR integration, integrated Web Browser, System Tray behavior, Advertisements/promotional content, Toast Notifications, game-adjustment tracking/notifications, language, Always On Top, Sidebar Position, Animation & Effects, telemetry/user-experience opt-in, and conditional Image Inspector behavior. A Windows-only preference may be Class N, but it must be dispositioned rather than silently omitted.

**ID-126 — Conditional reference tools remain explicit surfaces.**  
Game Advisor, Issue Reporting/Issue Detection, AMD Assistant, Video Upscaling/Video Enhancements, Variable Graphics Memory, Vari-Bright, SmartAccess features, SmartShift, Privacy View, AMD Chat, SteamVR integration, and Image Inspector are independently tracked conditional families. They are never considered covered merely because a generic adjacent page exists.

### Authoritative control inventories

**ID-127 — Graphics control inventory is explicit.**  
The global/per-game graphics inventory includes, when the selected reference/capability exposes them: Radeon Super Resolution; AMD FSR Upscaling software status/control; AMD FSR Frame Generation software status/control; current AFMF; Radeon Anti-Lag and reference-visible Anti-Lag 2 latency-monitor surface; Radeon Boost; Radeon Chill/frame-rate target; Radeon Image Sharpening/RIS 2; AMD Video Upscaling/Video Enhancements/Clarity; Geometric Downscaling; Enhanced Sync; Wait for Vertical Refresh; anti-aliasing mode and method; morphological AA; anisotropic filtering; texture filtering quality; surface format optimization; tessellation mode and maximum level; OpenGL triple buffering; shader cache/reset; and 10-Bit Pixel Format as a Graphics control distinct from display-link color depth. Additional controls observed in the selected golden reference are mandatory ledger entries rather than optional extras.

**ID-128 — Display control inventory is explicit.**  
Per-display controls include, when supported: FreeSync/Adaptive-Sync status/control and range; Virtual Super Resolution equivalent; GPU Scaling; Scaling Mode; HDMI Scaling/overscan-underscan; Vari-Bright on supported built-in displays; Integer Scaling; Custom Color; Color Temperature Control and temperature; Brightness; Hue; Contrast; Saturation; Display Color Enhancement; Color Deficiency Correction; Color Depth; Pixel Format; Custom Resolutions; display specifications; and reference multi-display/Eyefinity-equivalent grouping. Pixel-format options include the reference-supported YCbCr 4:4:4/4:2:2/4:2:0 and RGB 4:4:4 Limited/Full set when link/hardware capability permits. Reference-supported game overrides preserve Use Global Settings/override semantics and bind display-specific overrides to stable display identity.

**ID-129 — Record/Stream control inventory is explicit.**  
Recording settings include Record Desktop, indicator, region/application capture equivalent, profile, resolution, FPS, video/audio bitrate, encoding type, Enhanced Filtering, codec/hardware-conditional Pre-Analysis, Pre-Filtering, CAML and Enhanced AVC quality, audio channels, separate microphone track, microphone enable/level, Push to Talk, Audio Boost, media location, Instant Replay/duration, Instant GIF/length/quality, In-Game Replay, and audio capture device. Reference profile baselines are Low 720p60/5 Mb/s, Medium in-game 60 FPS/10 Mb/s, High in-game 60 FPS/30 Mb/s, and Custom, subject to Phase-0 reference revalidation. Live Stream includes account/custom stream, Go Live/Stop, mic/camera, Push to Talk, indicator, desktop capture, scene/chat/setup, Low/Medium/High/Ultra/Custom/Adaptive profiles, 360p through 2160p reference resolutions, 30/60 FPS, video/audio bitrate, Enhanced Filtering and Archive Stream.

**ID-130 — Metrics and performance-settings inventory is explicit.**  
Metric subjects include FPS, frame time, 99th-percentile FPS, stutter rate, GPU utilization/clock/VRAM clock/board power/edge temperature/junction temperature/fan/VRAM use/voltage/VRAM temperature when exposed, CPU utilization/frequency/temperature, and system RAM. Metrics UI includes grid/list views, Additional Metrics, expand/collapse, gauge/graph, per-metric UI/overlay/logging selection, Start/Stop Logging, Reset, and visible sampling interval of 0.25–5 seconds. Performance Settings includes sample interval, overlay visibility, inclusion in recorded/stream output, logging location, hide-overlay-during-logging, overlay size, columns, transparency and text color. CSV logs include UTC time, monotonic/elapsed time, app/host version, GPU identity, selected metrics and units, and game/profile context where applicable.

**ID-131 — Tuning control inventory is explicit.**  
Tuning includes automatic Default/Undervolt GPU/Overclock GPU/Overclock VRAM where supported; reference presets such as Quiet/Balanced/Rage where applicable; and Manual/Custom. Manual GPU tuning includes GPU Tuning/Advanced Control, min/max frequency, generation-appropriate voltage/offset, VRAM tuning/frequency, Memory Timing when exposed, Fan Tuning, Zero RPM, maximum fan, advanced fan curve, Power Tuning/power limit, Smart Access Memory state, Variable Graphics Memory where the reference exposes it, Apply Changes, Discard Changes, Reset, Import Profile, and Export Profile. Stress Test includes GPU selector, duration, start/stop, live readings, completion result, Run Another Test, and Finish. CPU/System tuning remains capability/reference gated and generic governor/EPP changes are not mislabeled as overclocking.

### Canonical domain model

**ID-132 — Core persistent entities use one shared vocabulary.**  
The core domain contains Device, Display, Game, GameLaunchTarget, GameSession, GraphicsProfile, GraphicsSetting, DisplayProfile, TuningProfile, TuningTransaction, MetricDefinition, MetricSampleIndex, PerformanceLog, Capture, StreamAccount, Scene, SceneElement, HotkeyBinding, Preference, Notification, SettingsSnapshot, Capability, and ParityException. Agents may introduce subordinate implementation records, but they must map back to these canonical concepts rather than creating competing product-domain names.

**ID-133 — High-rate samples are not ordinary relational rows.**  
SQLite stores metric definitions, log metadata, indexes/summaries, and product state. Live high-rate telemetry lives in the shared-memory ring; long-duration performance samples live in log files/columnar output. The core database must not become a per-sample telemetry sink.

**ID-134 — Game identity and launch targets are separate.**  
A logical Game may own multiple GameLaunchTargets so one title can be associated with Steam/Proton/manual or other launch routes without duplicating product-profile identity. Sessions reference the logical game and snapshot the effective profile revision used for that run.

**ID-135 — Media database state is metadata, not ownership.**  
Capture records describe filesystem media and may reference a GameSession, but the media remains valid if that session record is absent or imported. Deleting or rebuilding metadata must not implicitly destroy media bytes.

**ID-136 — StreamAccount stores metadata plus secret references only.**  
Credentials never become domain-record fields. Scene owns ordered SceneElements; user-mutable entities use UUIDs and revisions where applicable; detected hardware is reconciled from current discovery rather than preserved as unquestioned historical truth.

### Reference-capture contract

**ID-137 — Golden-reference capture matrix is mandatory.**  
Before a production screen can pass parity review, the project captures the selected Windows reference at minimum at 1920x1080/100% scale, 2560x1440/100% scale, 3840x2160 at Windows-recommended scale, the narrowest supported application window, the default application window, and maximized state. Every applicable screen captures default state plus, where meaningful, hovered primary control, keyboard/focus state, opened dropdown/popover, changed/dirty state, disabled/unavailable hardware state, confirmation modal, reproducible failure state, and successful-completion state. Dynamic data is controlled by fixture or explicitly masked; web screenshots are discovery aids, not golden visual evidence.

**ID-138 — Reference capture records environment and behavior, not screenshots alone.**  
Each captured feature records exact reference build, GPU/CPU/display configuration, labels, control min/max/default/step, enabled/disabled conditions, global-versus-game behavior, restart persistence, unavailable/error behavior, hotkey, animation/transition behavior, and the matching parity-ledger/test identity. Unknown reference behavior remains `REFERENCE_REQUIRED` until captured.

### Packaging artifact contract

**ID-139 — Native packages install the complete runtime integration set.**  
The canonical package contains the GUI, session service, display guard, gamewatch service, capture service, privileged daemon, overlay runtime library, session/system D-Bus activation definitions, matching systemd user/system units, polkit actions, required udev rules only when necessary, Vulkan layer manifest(s), desktop entry, and project-owned icons. The AI executable/service/model dependencies are a separate optional package. Service activation definitions delegate to the managed units rather than spawning duplicate unmanaged service instances.

**ID-140 — Package removal uses a constrained maintenance path.**  
Packaging provides a non-interactive privileged maintenance entry point used only by distribution removal hooks to drive the specified safe quiesce/recovery sequence. It is not a general hardware-control CLI. Removal proceeds only after capture finalization, replay shutdown, transient game-state restoration, risky display resolution, tuning reconciliation/persistence disablement, and service shutdown are verified; otherwise ordinary removal fails closed with an actionable diagnostic. User media, exported profiles, and user-created configuration are retained unless the user explicitly requests data deletion.

### Accessibility and localization

**ID-141 — Accessibility is part of parity acceptance.**  
Every production surface supports keyboard navigation, a visible design-consistent focus state, screen-reader labels, semantic names for icon-only controls, scalable text within reference-compatible layout bounds, and reduced-animation behavior tied to Animation & Effects. Critical success/failure state is never communicated by color alone. Accessibility adjustments must preserve the normal reference layout as closely as possible rather than creating a separate unrelated UI.

**ID-142 — Localization is architectural, not a later retrofit.**  
User-facing strings are separated from control logic and use the Qt translation system from the start. English is the initial required language. Layout/visual tests include expansion-prone fixture strings before additional translations ship so later localization does not invalidate component geometry unexpectedly.

### Operability and upgrade contracts

**ID-143 — Production logging is structured and redacted.**  
Logs use error/warn/info/debug/trace levels and stable subsystem tags covering UI, hardware, DRM, telemetry, tuning, profiles, gamewatch, overlay, capture, streaming, display, update, and AI. Production defaults to info. Secrets and sensitive credential material are redacted before emission, and log rotation is mandatory.

**ID-144 — Crash reporting is local-first and safety-prioritized.**  
Local crash dumps may be generated, but automatic upload is disabled unless the user explicitly opts in. Crash recovery prioritizes hardware safety, restoration of transient per-game state, tuning/display recovery, then preservation of user profiles/database. A diagnostic bundle remains local until the user explicitly shares it.

**ID-145 — Diagnostics expose provider evidence without secrets.**  
The diagnostics surface can report capability/provider selection, compositor/session state, PipeWire state, Vulkan-layer state, game-detection trace, available capture encoders, relevant permissions, recent tuning/display recovery information, logs, and a copyable diagnostic report. Secret material is redacted automatically.

**ID-146 — Dependency selection favors auditable libraries and explicit providers.**  
Prefer mature auditable open-source libraries. Do not hide an entire external application behind a subprocess when a stable library/API is the proper boundary. External applications such as Gamescope are acceptable when they are the correct Linux primitive, but they remain explicit capability-detected providers. Third-party dependency licenses are tracked and are a release gate.

**ID-147 — Upgrade migrations fail safely.**  
Database/config schemas and profile formats are versioned. Destructive migrations create and validate a backup before commit and roll back on failure. A failed user database is preserved rather than replaced silently. Tuning-profile portability is checked using schema version, device identity and capability fingerprint; applying a profile created for different hardware requires validation and an explicit warning.

### Environment and presentation-state contracts

**ID-148 — Certification tiers are explicit.**  
Tier-1 desktop/session targets are KDE Plasma 6 on Wayland and X11. Tier-2 targets are GNOME on Wayland and X11. Tier-1 distribution targets are Arch Linux and CachyOS; Tier-2 distributions are Fedora and Ubuntu/Debian. Other compositors/distributions may run the product but are not parity-certified until their overlay, capture, hotkey, display-control, packaging, and lifecycle requirements pass.

**ID-149 — Responsive behavior follows the reference, not a mobile redesign.**  
For each golden reference width the project records minimum window size, card reflow, column count, scrolling/clipping, title truncation, graph resizing, and top-navigation behavior. Unsupported tiny sizes enforce a reference-compatible minimum-size constraint rather than collapsing into a separate mobile UI.

**ID-150 — Every data-driven surface has explicit non-happy states.**  
Components define loading, loaded, unavailable, permission denied, unsupported, transient failure, disconnected hardware, stale telemetry, not-running game, and backend-missing states where applicable. Unknown numeric telemetry is N/A, never synthetic zero. Service-backed pages may render their shell before the owner is READY, but mutable content remains loading until the first authoritative snapshot arrives.

**ID-151 — Multi-GPU behavior is first-class.**  
The product supports iGPU+dGPU and multiple-discrete-GPU enumeration. Device-sensitive surfaces carry an explicit selected-device context where the reference requires it; per-game profiles record target GPU when meaningful; capture records render GPU and encoder GPU separately; cross-GPU transfer is explicit, measured, and included in performance evidence rather than hidden.

**ID-152 — First-run and Factory Reset use reference Quick Setup.**  
On first launch and after Factory Reset, the application reproduces the selected reference onboarding flow, including graphics-profile selection, capture/stream setup where invoked, and privacy/telemetry choice where applicable. Linux-specific compatibility issues such as missing polkit/udev integration appear as one actionable compatibility dialog rather than turning onboarding into a sysadmin wizard.

### Runtime determinism contracts

**ID-153 — GPU telemetry source precedence is fixed per metric.**  
For fields exposed validly by the current structure version, AMDGPU `gpu_metrics` is preferred. HWMON is next for temperatures, fan, voltage/current, power and power-cap/readback fields it exposes. libdrm/amdgpu query interfaces follow for supported static/dynamic values, with other documented AMDGPU sysfs/DRM statistics as field-specific fallbacks. A higher-priority source is rejected only when unavailable, invalid, stale, or demonstrably less authoritative for that metric; Diagnostics records the chosen source.

**ID-154 — Instant Replay uses one bounded disk-backed segment design.**  
The production replay buffer is a bounded encoded circular segment store in the user's cache domain. RAM contains indexes and active codec/mux state rather than the full replay duration. The store is capped by configured duration plus one in-progress segment and is cleaned on normal disable/expiration. A tmpfs cache naturally makes the same implementation memory-backed; the normal UI does not expose a separate storage-mode choice unless the reference does.

**ID-155 — Wayland capture portal versions have fixed semantics.**  
Generic Wayland capture uses the ScreenCast portal plus PipeWire. Tier-1 certification requires persistent restore-token support equivalent to ScreenCast v4 or newer. When v6 or newer is available, stream targeting prefers `pipewire-serial`/the stable target-object mechanism rather than reusable node IDs. Restore tokens are treated as single-use and replaced by the token returned from each successful restored start, stored in the desktop secret service, and invalidated on revocation/failure. Background code never loops permission prompts.

**ID-156 — Wayland global shortcuts require GlobalShortcuts v2 semantics.**  
Tier-1 Wayland certification requires the GlobalShortcuts portal interface v2 or newer. The session service owns one logical application shortcut session using stable action IDs; peer services register handlers with sessiond instead of opening competing portal sessions. Effective binding state reflects the actual portal result.

**ID-157 — Service reconnect backoff is fixed.**  
GUI and peer services reconnect automatically with bounded exponential backoff beginning at 100 ms and capped at 5 seconds; the backoff resets after 30 seconds of stable connectivity. Reconnect always performs authoritative state reconciliation before incremental event handling resumes.

**ID-158 — Feature provider policy is reviewable data.**  
Each production feature has exactly one provider-policy record containing feature identity, scope, ordered provider candidates, required capabilities, conflict group, and restart/relaunch policy. CI rejects missing policy or ambiguous priority. Runtime chooses the first candidate whose capability predicate is satisfied; plugin load order, filesystem order, hash iteration, or registration timing may never select a provider.

**ID-159 — Stress Test uses a project-owned bounded Vulkan workload.**  
The production GPU stress-test backend is a deterministic project-owned Vulkan compute/graphics workload with bounded duration and normal platform thermal/power protections intact. External benchmark applications may be comparison tools but are not the product stress-test implementation.

**ID-160 — Game Advisor baseline and evidence model are fixed.**  
The initial eligibility threshold is at least 180 seconds of valid gameplay telemetry, matching the current documented reference baseline unless controlled 26.9.1 capture proves a different threshold. Its report uses average FPS, percentile performance, frame-time series/percentile behavior, stutter indicators, GPU utilization, CPU utilization, VRAM pressure, and detectable power/thermal constraints. Recommendation rules are deterministic and explainable and account for the active game profile, current resolution, VRR state, frame cap, GPU-bound-versus-CPU-bound evidence, thermal-throttling evidence, and VRAM pressure. The advisor never recommends a Class D/unavailable feature as though it were usable and never turns an unverified inference into a hardware diagnosis.

### Settings snapshot and reset contracts

**ID-161 — Settings snapshots have a fixed product-level payload.**  
A snapshot contains application preferences, global graphics profile, per-game profile overrides, tuning profiles, overlay configuration, capture configuration, stream scene metadata without secrets, hotkey bindings, display application settings that are safe to restore, schema version, and hardware-compatibility metadata. Stream credentials, secret restore tokens, and other credential material are never exported.

**ID-162 — Snapshot import is a previewed transactional workflow.**  
Import first inspects and validates schema/content, computes a user-visible diff, warns about hardware-specific settings, and backs up current state. Application begins only after validation. Any imported tuning/display-affecting changes use the existing safety transaction model and a visible rollback confirmation path equivalent to Keep Settings/Revert Settings. The import produces a detailed result log and either reaches verified committed state or restores the pre-import state.

**ID-163 — Factory Reset resets product state, not the machine.**  
Factory Reset returns application settings/profiles to defaults and then invokes reference Quick Setup where applicable. It does not delete recordings/screenshots, uninstall or downgrade Linux graphics-stack packages, erase unrelated user files, or remove stream credentials unless the confirmation explicitly includes credential deletion.

### Remaining settings/tool contracts

**ID-164 — Audio & Video profile inventory is explicit and capability-separated.**  
The reference video profile set includes Default, Cinema Classic, Enhanced, Home Video, Outdoor, Sports, Vivid, and Custom plus any current-generation additions found in capture. AMD Video Upscaling, Geometric Downscaling, RIS 2 desktop/video sharpening, Noise Suppression, and profile-based video processing remain separate capabilities even when one Linux provider implements several of them. Effective scope is explicit as desktop, video, supported application(s), or per-game rather than collapsed into one generic enhancement switch.

**ID-165 — Hotkey action inventory and known defaults are explicit.**  
The centrally managed action registry covers overlay open/toggle, metrics overlay, performance logging, recording start/stop, screenshot, Instant Replay save, Instant GIF, microphone, Push to Talk, stream start/stop, reference-visible Anti-Lag 2 Latency Monitor actions, and scene actions where exposed. Known reference baselines include Ctrl+Shift+L for performance logging and Ctrl+Shift+O for performance overlay; all defaults are revalidated against the golden reference before release rather than treated as timeless constants.

**ID-166 — Issue Reporting and Issue Detection are local-first Class B workflows.**  
When the selected reference exposes the bug icon, System Issue Reporting, or Issue Detection, the Linux product collects a deterministic redacted diagnostic manifest, shows the categories before export/submission, captures the reference-equivalent affected app/game, category/symptom, description, reproduction steps, history, optional contact field and user-selected attachments, and always offers local export. Nothing is transmitted until the user explicitly performs the final send/share action. Remote submission targets only an explicitly configured project support endpoint unless an authorized AMD interface exists. Issue Detection may offer/open the report flow from trustworthy evidence such as AMDGPU reset/fault, product-service crash, package/API mismatch, or device disappearance; it never auto-transmits and it rate-limits repeated faults.

### Branding and privacy boundaries

**ID-167 — Engineering parity and public branding are separable.**  
Internal development reproduces the reference geometry/behavior using developer-controlled reference captures. A public release must not imply AMD production, endorsement, or support without authorization and must not redistribute proprietary AMD image assets unless their license permits it. Trademarked/public brand assets are isolated behind a replaceable asset package so a legally distinct product name/skin can ship without rewriting functional UI code. This legal/distribution separation does not weaken the internal 1:1 parity target.

**ID-168 — Core functionality requires no cloud account and product telemetry is opt-in.**  
Detected hardware, game library, settings, metrics, playtime and media metadata are local product data. No cloud account is required for core Radeon control. No product telemetry leaves the machine without explicit opt-in. User-selected streaming traffic is distinct from product telemetry, and diagnostic bundles remain local until the user explicitly shares them.

### Completion and parity-accounting contracts

**ID-169 — Parity ledger schema and statuses are fixed.**  
Each parity row records identity, area, reference version/screen, feature, UI status, backend status, parity class, Linux responsibility owner/disposition/provider, hardware scope, test identity, golden-reference identity, known deviation, issue and owner. Allowed lifecycle statuses are not_started, reference_captured, ui_partial, ui_complete, backend_partial, backend_complete, verified, and blocked. Class N rows require a concrete Linux owner and disposition of not_applicable, status_only, or handoff_only. Nothing is complete without `verified`.

**ID-170 — Page Definition of Done is uniform.**  
A page is complete only when its golden reference exists; every visible component is represented; layout passes visual regression; interactive controls have behavior; loading/unavailable/error states exist; keyboard navigation and accessibility labels pass; persistence behavior is tested where relevant; backend operations are integration-tested; and all page parity-ledger entries are verified.

**ID-171 — Feature Definition of Done is uniform.**  
A feature is complete only when it is discoverable from the reference-equivalent path, label/visual state matches, capability detection is correct, enable and disable paths work where applicable, effective state is verifiable, global/game scope behaves correctly, restart persistence matches the contract, failure leaves no hidden partial state, diagnostics/logging are useful, and UI success is impossible when backend verification failed.

### Toolchain baseline

**ID-172 — Shipping and test stack is fixed.**  
The shipping language baseline is C++20 with standard extensions disabled. The UI baseline is Qt 6.8+ with Qt Quick/QML and custom product-styled controls; Qt WebEngine 6.8+ is included only for reference features that require embedded web content. CMake + Ninja is the canonical build path. Catch2 v3 is the native unit/integration test framework and Qt Test covers QML/UI behavior. SQLite is the structured local store, D-Bus is the low-rate RPC transport, and versioned shared memory is the high-rate telemetry transport. The shipping application has no Python runtime dependency. Raising a minimum dependency or language level requires an explicit reviewed architecture/build change.

### Initial parity-classification baseline

**ID-173 — Difficult features start from a shared classification baseline.**  
Engineering research may change these classifications only with evidence recorded in the parity ledger. The starting dispositions are:

- Windows driver install/download/rollback/clean-install, Windows Update/registry/driver-service management, and Driver Only installer behavior: **N**;
- Windows Full/Minimal installer workflow: **N by default**; Linux full application surface remains the certification target;
- Linux graphics-stack version/update visibility and package-manager handoff: **B**;
- GPU metrics and supported clocks/power/fan control: **A**;
- recording, streaming, Instant Replay: **B**;
- game discovery: **B**;
- metrics overlay: **C**;
- Chill/frame-rate limiting: **C**;
- Image Sharpening: **C**;
- AMD Video Upscaling/Video Enhancements: **B/C/D by scope**;
- Geometric Downscaling: **B/C/D by scope**;
- encoder Pre-Analysis/Pre-Filtering/CAML/Enhanced AVC quality: **B/C/D by codec/device/provider**;
- RSR: **C** with Gamescope as the initial provider;
- AMD FSR Upscaling/FSR Frame Generation Adrenalin software controls: **A/B/C/D by supported GPU/title/provider evidence**;
- FreeSync/VRR: **A/B**;
- Custom Color: **B**;
- Pixel Format: **A/B/D**;
- AFMF: **D initially**;
- Anti-Lag: **D/C**; Anti-Lag 2 Latency Monitor: **C/D**;
- Radeon Boost: **D/C**;
- Enhanced Sync: **D/C**;
- System/CPU Auto Overclock: **D initially**;
- Variable Graphics Memory: **A/B/D by platform**;
- Issue Reporting/Issue Detection: **B**;
- 10-Bit Pixel Format (Graphics): **D/B** and remains distinct from display output depth;
- HDMI Scaling: **A/B/D by compositor/link**;
- Vari-Bright: **A/B/D by mobile platform**;
- integrated Web Browser: **B**;
- Image Inspector: **D initially**;
- SteamVR integration: **D/B**;
- Smart Access Memory: **A/D** depending on status/toggle capability;
- AMD Assistant: **B/D if present**;
- Noise Suppression: **B**;
- SmartAccess Video: **D initially**;
- SmartAccess Graphics: **B/D by platform**;
- SmartShift: **A/B/D by platform**;
- Privacy View: **B/D**;
- AMD Chat: **B**.

A classification change does not permit silent scope reduction: the evidence, provider, affected hardware scope, test identity and user-visible disposition all change together in the ledger.

### Native media and overlay backend contracts

**ID-174 — Capture/media stack is fixed for v1.**  
The primary capture pipeline uses PipeWire video and audio, libavformat/libavcodec for container/codec handling, and VAAPI on AMD VCN as the baseline hardware-encoding path. AVC/H.264 is supported, HEVC when the device/provider supports it, and AV1 when hardware supports it. Software encode fallback occurs only when explicitly selected or hardware encoding is unavailable. Where PipeWire and the selected encoder can share compatible DMA-BUF formats/modifiers, the pipeline negotiates zero/low-copy transport end to end; CPU-copy fallback is allowed only after negotiation failure and increments an observable diagnostic counter used in certification.

**ID-175 — Overlay render integration is fixed by graphics API.**  
Vulkan overlay support uses an explicit/implicit Vulkan layer that renders after the application frame and before present, consumes shared-memory telemetry, and has no Qt dependency inside the injected game process. OpenGL support uses a GLX/EGL interposer and renders at swap/present. A compositor overlay window is a fallback only when it can reliably appear above the target and does not evade protected-process or anti-cheat controls.

**ID-176 — Optional AI/Chat has a complete enabled contract.**  
When the selected reference exposes AI/Chat and the optional module is installed, it provides local/offline text chat after model installation; typed queries for hardware/software, current feature state, game library, graphics-stack/application versions, GPU/CPU live data and display information; local document upload/RAG; local chat history; optional local image generation; and automatic resource suspension while a demanding game is active. ROCm may be used where supported. AI history/embeddings/model indexes remain in AI-owned storage, not the core application database. Read questions use typed read-only tools; mutating actions require explicit user confirmation and the same validated service path as GUI actions.

### Linux system/update integration

**ID-177 — Package update integration is read-mostly and distro-specific.**  
The application uses a PackageUpdateProvider abstraction for inspection/handoff only. Arch/CachyOS uses libalpm or a safe read-only pacman metadata path; Ubuntu/Debian uses apt/dpkg metadata; Fedora uses dnf/rpm metadata. Providers can refresh/check metadata, list relevant packages, return installed/available versions, and open the canonical system updater/package detail surface. They never independently install, downgrade, replace, pin, or remove kernel, Mesa, firmware, ROCm, Vulkan-loader, compositor or other distro-owned graphics-stack packages.

**ID-178 — System page reports the Linux stack rather than inventing a monolithic driver package.**  
System information includes application version, Linux distribution, kernel, GPU(s), CPU, AMDGPU/kernel-driver context, Mesa, RADV/RadeonSI, libdrm, LLVM where relevant, firmware information, optional ROCm, release/update status, release notes, hardware identifiers where the reference exposes analogous data, settings export/import, and Factory Reset. The compact Home software card may summarize this stack, but detailed component versions live on System.

### Game discovery and identity

**ID-179 — Automatic discovery inventory is explicit.**  
Gamewatch scans Steam native titles, Steam Proton titles, non-Steam Steam shortcuts, Lutris, Heroic, Bottles, game-marked desktop launchers, detectable AppImage launchers, and previously registered manual targets. Discovery providers normalize store-specific records into one logical Game plus one or more GameLaunchTargets rather than duplicating the same title per launcher.

**ID-180 — Manual-add and launch-target data are explicit.**  
Manual add accepts native ELF executables, AppImages, desktop launchers, and Windows executables associated with a Wine/Proton prefix. Shell launchers require explicit confirmation because process resolution may be ambiguous. A launch target retains source/source-app identity, command and working directory, executable/process matchers, Wine prefix and Proton version where relevant; the logical Game retains display name, artwork references, last-played time, accumulated playtime and profile identity.

### Smart Technology implementation semantics

**ID-181 — Noise Suppression reproduces routing semantics, not only denoising.**  
The Class B Linux implementation uses PipeWire virtual input/output nodes and supports reference-equivalent selectable input, selectable output, and input/output/both processing modes. The local denoiser may be RNNoise-, DeepFilterNet-, or equivalent class, but must meet the latency/quality certification fixture. CPU/GPU processing selection is exposed only when both are real implemented backends; the UI may never label a CPU path as GPU processing merely because an AMD GPU is present.

### Service-specific recovery and embedded-web security

**ID-182 — Gamewatch restart reconciles live processes before accepting new launches.**  
After restart, gamewatch rescans known launch sources and process ancestry, reconstructs any still-running game session it can prove, and reconciles transient provider/profile state. If a previously active game profile has no matching live game, gamewatch restores global/transient state before accepting a new launch. A vanished process is recorded as a lost session rather than silently abandoned.

**ID-183 — Capture restart preserves recoverable output and never invents success.**  
After capture-service restart, in-progress/partial media is inspected. If its container can be safely finalized, it becomes a recovered capture carrying a warning/recovered flag; otherwise it remains quarantined for explicit user recovery/deletion and is not entered as an ordinary successful capture. Instant Replay returns to DISABLED after capture-service failure until capture permission/source is successfully re-established.

**ID-184 — Browser Source is isolated beyond generic sandboxing.**  
Browser Source uses Qt WebEngine with the Chromium sandbox enabled and isolated storage partitions. It receives no stream credentials, product secret-store material, or privileged D-Bus access. Direct local-file access is disabled by default; a user-selected local source may access only the explicitly approved resource tree. Network access uses normal system TLS validation and certificate errors are never silently ignored.

**ID-185 — Session-service restart has a fixed reconciliation sequence.**  
Before READY, sessiond completes database recovery/migration, reconciles hardware/display inventory and pending display recovery, rebuilds the capability generation, recreates telemetry publication, restores/rebinds the global-hotkey portal session, and asks gamewatch/capture to republish authoritative active state. Clients invalidate stale handles/state and request fresh snapshots before processing new incremental events.

### Shell, search, notification, and media workflows

**ID-186 — Global search is local, comprehensive, and navigational.**  
The offline search index includes pages/subsections, settings/features, detected games, tuning/display/recording controls, hotkeys, preferences, and system/update entries. Selecting a result opens its owning screen and brings the relevant section into view. Warm-index first results remain subject to the <=50 ms performance gate.

**ID-187 — Shell affordances follow the selected reference.**  
Back/forward navigation where present, search, Favorites when present, notification bell, Settings gear, window controls, selected-tab treatment, and hover/pressed/focus behavior are product surfaces. They are not replaced by a Linux-style hamburger/sidebar redesign merely because the desktop environment offers one.

**ID-188 — Notification taxonomy and long-operation feedback are explicit.**  
Notification classes cover feature recommendation, setting applied, application/graphics-stack update availability, capture status, stream status, tuning warning/reset, hardware-capability change, game detected, and error. Read/unread state persists and desktop toast delivery obeys the Toast Notifications preference. No control silently ignores activation; long actions expose progress, safe cancellation where technically valid, completion state, and accessible diagnostic detail.

**ID-189 — Recent media tiles are actionable.**  
The Home recent-media card supports reference-equivalent open, play, reveal-in-folder, and delete-with-confirmation behavior plus every reference-visible share/stream/media action. If a destination lacks a supported Linux/API backend, the action remains truthful and its backend gap is classified rather than silently removed.

### Design-system seam

**ID-190 — Reference styling is centralized in tokens and reusable QML primitives.**  
Fixed UI colors, typography, radii, spacing, control heights and navigation dimensions come from one reference-derived token system; production pages do not introduce per-page magic values for those properties. The reusable component layer covers the product window/shell/navigation/search, cards/tiles, toggles/sliders/dropdowns, primary/secondary/icon buttons, graphs/gauges/metric cards, tooltips/modals/toasts/tabs, game/media tiles, section headers, warnings and progress states. Production pages compose these primitives rather than using platform-styled Qt controls directly.

### Portal lifecycle completion

**ID-191 — Global shortcut rebinding recreates the portal session when required.**  
Shortcut bind/configuration that can show portal UI originates from a visible GUI action and carries an appropriate parent-window identity; otherwise sessiond returns `INTERACTION_REQUIRED`. Because the portal binding operation is one-shot for a shortcut session, a binding-set change that cannot be represented by the active session closes it cleanly and creates/rebinds a new session. Sessiond consumes activation, deactivation, and shortcut-change signals and reconciles effective bindings. Portal rejection maps to typed portal/interaction errors, never silent fallback. KDE-specific shortcut integration is an explicit provider only when portal capability is absent or demonstrably cannot meet parity and the exception is ledgered; X11 uses the X11 hotkey provider rather than the Wayland portal.

**ID-192 — Prepared low-level display privilege is a restart-recoverable, transaction-bound capability.**  
For a risky display operation requiring privileged low-level mutation, interactive authorization completes before the first output-risking change. The privileged daemon's prepare phase creates a cryptographically random capability token of at least 256 bits bound to the operation ID, authenticated caller UID, target device/display identity, canonical known-good and intended states, and an unused-apply expiry. The public operation ID is not itself authority. The daemon durably stores only a cryptographic verifier/hash of the token with the prepared-operation recovery record; the raw token is returned only to the authorized user-session transaction owner. The display guard stores that raw token only inside its user-private durable recovery record and never exposes it through logs, diagnostics, notifications, exported settings, or the core application database.

Apply, revert, terminal finalization, and terminal acknowledgement require the exact matching token, operation identity, authenticated UID, and target identity. An unused prepared operation may expire before mutation begins. Once the operation has mutated display state, expiry may not remove the ability to restore the exact captured known-good state: the revert capability remains valid and restart-recoverable until verified commit or verified revert reaches the two-party terminal protocol. The privileged daemon reloads prepared-operation records before accepting new low-level display mutations, and the guard reloads its private token and reconciles with the daemon after restart. If the guard/token is unavailable after mutation, the daemon may perform an internal recovery-only revert from its root-owned known-good record; that path is not a general externally callable privilege mechanism and may restore only the exact captured state.

**ID-193 — The reference release is fixed for this specification.**  
Parity decisions, golden captures, terminology, navigation, defaults, and conditional-surface research target AMD Software: Adrenalin Edition **26.9.1 Optional (2026-09-03)**. AMD documentation from the 25.6.1–26.9.1 product generation may supplement discovery of hardware-conditional surfaces that the primary reference machine cannot expose, but it may not silently replace observed 26.9.1 behavior. Moving the canonical reference to a newer Adrenalin release is a deliberate spec revision with parity-ledger migration, not an incidental implementation-time update.

### Smart Technology semantic closure

**ID-194 — AMD Assistant, when present, is an explicit local rules engine rather than unexplained automation.**  
Phase-0 reference capture determines whether 26.9.1 exposes AMD Assistant and, if so, captures its controls, defaults/opt-in state, notifications, triggers that can be observed, and restoration behavior. The Linux implementation is owned by the session service and may automate only settings the application itself owns or can truthfully request from an approved provider. Every automatic change records the triggering rule/event, prior configured value, resulting effective value, and restoration semantics. The user can disable the automation according to the reference behavior. Unknown proprietary trigger logic remains Class D rather than being guessed, and AMD Assistant may never mutate Class N distribution/kernel/package responsibilities.

**ID-195 — Smart Access Memory status is evidence-based and a runtime toggle is optional, not assumed.**  
SAM/Resizable BAR state is derived from PCIe/device/platform evidence. If the platform exposes no documented safe runtime control, the product reports verified status and reference-equivalent guidance/state without pretending a BIOS/firmware change occurred. A runtime toggle may be enabled only when a provider can verify the resulting platform state.

**ID-196 — SmartAccess Video becomes active only after verified multi-adapter media behavior.**  
A Linux equivalent may distribute eligible encode/decode work across supported iGPU/dGPU hardware, but the feature reports active only when the provider verifies actual routing/participation and passes its certification benchmark. Ordinary selection of a different encoder is not sufficient to claim SmartAccess Video parity.

**ID-197 — SmartAccess Graphics is defined by the captured routing outcome, not by the phrase “GPU switching.”**  
For an applicable mobile/hybrid reference system, reference capture records modes, automatic/manual behavior, status, restart/logoff requirements, and supported hardware. A Linux Class B provider may use documented DRM/KMS, compositor, switcheroo-control/DRI PRIME, firmware-mux, or vendor platform interfaces only when they reproduce the same user-visible routing outcome. The provider records render GPU, display-owning GPU, copy/offload path, and effective mode so the result is verifiable. Firmware/BIOS-only mux changes stay outside application ownership unless a documented safe runtime interface exists. Ordinary PRIME offload is not labeled SmartAccess Graphics unless it matches the captured semantics.

**ID-198 — SmartShift represents platform firmware/SMU power-sharing policy, not independent CPU/GPU power sliders.**  
Reference-visible SmartShift status/control is exposed only through a documented Linux kernel/firmware interface that represents the same dynamic platform policy. Independent manual CPU/GPU power-limit changes must never be presented as SmartShift. Any supported control preserves platform thermal/current protections and uses the normal privileged transaction and recovery model.

**ID-199 — Privacy View is local, permission-bound vision behavior or remains Class D.**  
Privacy View/current equivalent is Class B only when local camera/vision processing can reproduce the captured user outcome and permission behavior. Camera access uses PipeWire/portal or the certified desktop permission path, runs locally by default, shows a visible active indicator, does not upload camera frames, and stops cleanly when permission is revoked or the camera disappears. If those semantics cannot be reproduced, the feature remains Class D rather than degrading into a generic webcam filter.

### Display-provider semantic closure

**ID-200 — KDE Wayland display-provider precedence is fixed and compositor-safe.**  
The initial Tier-1 Wayland display implementation targets KWin/KScreen. Provider selection for a display operation follows this precedence: (1) stable compositor/display APIs; (2) KScreen-supported interfaces; (3) DRM/KMS properties only when ownership and compositor semantics make the operation safe; and (4) direct DRM only when no compositor-safe path exists and the operation is valid. The product never seizes DRM master from the running compositor as a normal configuration strategy. Custom Color applies immediately through the compositor/color-management path; normal product behavior must not silently implement it by writing destructive/persistent monitor DDC state. A monitor-hardware color path is allowed only as an explicit advanced/diagnostic user choice and is not the default Adrenalin-parity provider.

**ID-201 — Vari-Bright is adaptive platform behavior, not a static brightness imitation.**  
Vari-Bright appears only on hardware/reference configurations that expose it. The provider must discover a documented equivalent through AMDGPU/DC, ACPI/firmware, backlight/power interfaces, or another verified platform API and must capture/test battery-vs-AC behavior, level semantics, restoration, and interaction with manual brightness. Static desktop-brightness writes, monitor DDC polling/writes, or a generic brightness slider do not satisfy Vari-Bright parity. If no equivalent platform capability exists, the reference-visible feature remains Class D.

**ID-202 — FreeSync/VRR identity is evidence-based.**  
The display surface may report VRR/Adaptive-Sync capability, active state, and range when detectable. An AMD FreeSync certification/tier label is shown only when that certification can actually be established; generic Adaptive-Sync capability must not be relabeled as an AMD certification tier. A per-game override is exposed only when the selected Linux provider can enforce and verify it.

### Record, scene, screenshot, and media semantic closure

**ID-203 — Scene Editor operations and camera properties are explicit.**  
The Scene Editor supports multiple scenes; create, rename, select, and delete; reference-visible scene hotkeys; scene width/height; ordered elements; visibility toggles; and direct manipulation in the preview. Supported element families are Browser Source, Image, GIF, Video, Indicator, Camera, and Chat Overlay. Camera elements expose position, dimensions, opacity, on-screen-versus-in-video visibility, chroma-key enablement, chroma color, and key strength when present in the reference. Scene persistence must preserve ordering and element identity across restart.

**ID-204 — Screenshot targets and output semantics are explicit.**  
Screenshot capture supports the active game, a selected monitor, and the desktop when the certified capture provider permits those targets. Overlay inclusion/exclusion follows the captured reference behavior. Successful screenshots are lossless PNG by default unless the fixed 26.9.1 reference establishes a different default. A screenshot action reports the actual captured target and output path; fallback to a different target is never silent.

**ID-205 — Media indexing uses a fixed minimum metadata contract.**  
Finalized/recovered media metadata includes filesystem path, media type, associated game when known, creation time, duration where applicable, resolution, codec, file size, and thumbnail reference. The media index may be rebuilt from filesystem objects and is never the sole owner of media bytes. Delete-from-product behavior that removes the underlying file requires the same reference-equivalent confirmation path rather than treating metadata deletion as file deletion implicitly.

### Tuning user-safety closure

**ID-206 — Dangerous tuning requires a first-use risk acknowledgement.**  
Before the user can perform the first potentially dangerous GPU tuning mutation, the tuning surface presents the same class of risk acknowledgement as the fixed Adrenalin reference. Acceptance is persisted as product preference state and may be reset by Factory Reset. Authorization or kernel acceptance is not described as proof that a value is safe; the acknowledgement is informational and does not weaken range validation, recovery, or hardware-protection requirements.

**ID-207 — Instability marks the active tuning transaction/profile suspect and fails safe.**  
A GPU reset, watchdog event, service-detected device disappearance, reboot/unclean shutdown correlated with an active tuning transaction, or stress-test failure marks that transaction and its source profile revision suspect. On recovery, the product attempts verified restoration to captured known-good/default/last-safe state before enabling new tuning. If restoration cannot be verified, tuning mutations for that GPU remain blocked in a persistent recovery state and the user receives a persistent critical recovery notification with diagnostics. A suspect profile is never automatically re-applied until explicitly reviewed/revalidated.

---

## Testing Decisions

The testing strategy follows three primary seams and deliberately avoids multiplying implementation-specific seams.

### Primary seam — user-session service contracts

Most product behavior is tested through the highest stable boundary: typed session-service APIs plus observable persisted/effective state.

Tests at this seam verify:

- capability discovery and generation changes;
- global/per-game profile inheritance;
- settings mutation and optimistic concurrency;
- notification behavior;
- search indexing;
- game-session reconciliation;
- display validation and handoff;
- settings export/import;
- service restart reconciliation;
- update visibility/handoff;
- hotkey registration state.

Tests assert externally observable state and typed results. They do not assert private class layout, helper-call counts, SQL query shapes, or provider implementation details.

### Privileged seam — system daemon contracts

Tuning and low-level display mutation are tested against the privileged semantic interface.

The main test environment uses mock/simulated sysfs/DRM fixtures. Hardware write certification runs only on dedicated opt-in hardware.

Contract tests verify:

- range validation;
- capability-generation rejection;
- operation idempotency;
- journal-before-write ordering;
- deterministic write order;
- read-back verification;
- rollback after partial failure;
- restart recovery;
- persistent-profile safety;
- prepared display-operation lifecycle;
- authorization identity derived from authenticated bus credentials rather than caller-supplied UID/session fields.

### UI seam — golden-reference render and interaction

Every production page is rendered with deterministic fixture data and compared against the approved Windows reference corpus. Golden and candidate images are compared at the same physical pixel dimensions; automatic resizing before acceptance comparison is forbidden. Dynamic regions may be masked only by reviewed checked-in masks. The canonical visual-diff implementation and its metric parameters are version-pinned.

Baseline visual acceptance is normative:

- SSIM >= 0.995 after approved dynamic masks;
- no unapproved element displacement > 2 px at baseline reference resolution;
- no unapproved component dimension deviation > 2 px;
- CIEDE2000 ΔE00 <= 2 for fixed UI colors after the controlled color transform;
- reference token mapping defines font family/weight/size/line-height, with documented platform rasterization differences allowed only where geometry remains correct.

Visual tests verify:

- geometry;
- hierarchy;
- component order;
- typography metrics;
- fixed colors;
- states;
- responsive/reflow behavior;
- modal layout;
- graph/gauge styling.

Dynamic regions use explicit checked-in masks only.

Interaction tests replay reference workflows at the UI boundary, such as:

- editing a predefined graphics profile and observing Custom state;
- adding a game;
- applying and discarding tuning changes;
- starting/stopping recording;
- saving instant replay;
- confirming/reverting display changes;
- importing a settings snapshot;
- binding a hotkey;
- selecting overlay metrics.

### Telemetry ABI tests

Telemetry tests use a real shared-memory producer/consumer fixture.

They verify:

- ABI version handling;
- definition-generation refresh;
- multi-reader operation;
- ring wrap;
- atomic sequence protocol;
- no accepted torn records during continuous overwrite;
- stale/malformed mapping rejection;
- service-generation changes;
- subject/metric definition synchronization.

### Game/runtime tests

Tests use certified native Linux and Proton titles or controlled harnesses representing them.

They verify:

- correct process association;
- runtime provider activation;
- environment/layer scoping;
- overlay attachment;
- profile activation;
- restoration on game exit;
- restoration after watcher restart;
- protected-process failure behavior.

The certification matrix must include representative supported native and Proton paths and must record exceptions explicitly.

### Display tests

Display tests use both mocked providers and certified physical fixtures.

They verify:

- enumeration;
- VRR state;
- scaling;
- color controls;
- pixel format/depth where supported;
- custom modes;
- multi-monitor behavior;
- risky-transaction countdown;
- automatic rollback;
- explicit keep/revert;
- GUI crash;
- session-service crash;
- display-guard crash/restart;
- privileged-daemon crash/restart;
- crash at each two-party commit phase;
- wrong, reused, mismatched-UID, mismatched-target, and expired-before-apply capability tokens are rejected;
- the prepared capability survives permitted guard/daemon restart without a second authorization prompt;
- once mutation begins, an unused-apply expiry cannot invalidate the exact recovery revert;
- the raw capability token never appears in logs, diagnostics, exports, notifications, or the core database;
- daemon-only emergency recovery can restore only the captured known-good state and cannot be repurposed as a general mutation API.

A display test is not complete merely because the requested call succeeded; effective output state must be verified.

### Tuning tests

Mock-device tests cover the full tuning state machine before hardware tests are allowed.

Dedicated hardware tests then verify generation-specific behavior for:

- RX 6000/RDNA2;
- RX 7000/RDNA3;
- RX 9000/RDNA4.

Mobile/hybrid fixtures exercise applicable SmartShift/SmartAccess Graphics/Vari-Bright paths.

Automatic overclock tests never run on arbitrary contributor/CI machines.

### Capture and streaming tests

Capture acceptance is fixture-driven.

Baseline requirements include:

- output resolution, pixel aspect, codec, nominal FPS, audio-channel count, and requested track layout exactly match the configured profile;
- constant/average-bitrate output measures within ±10% of the requested bitrate over the certification steady-state interval; quality/constant-QP/CRF modes use their fixture-specific quality/rate-control assertion instead;
- capture-attributable encoded-frame loss <= 0.1% over a 10-minute 60-FPS baseline run, with no burst > 2 consecutive missing frames;
- no monotonic timestamp regression;
- A/V end-to-end drift <= 20 ms after 20 minutes;
- ordinary recording duration differs from source/requested duration by no more than one video frame plus one audio packet, excluding fixture-defined explicit stop latency;
- a 5-minute Instant Replay saved after sufficient runtime has duration `300 s ± max(2 s, one configured GOP interval)` and its newest video frame is no more than 1 s before the save trigger unless source delivery stopped;
- replay save captures the rolling interval ending at the trigger rather than the beginning of the session;
- a requested separate microphone track remains independently addressable and is not silently mixed;
- valid crash/partial-file recovery is tested separately from successful-output quality;
- 1080p and 1440p are exercised at 30 and 60 FPS;
- 4K60 is mandatory where certification hardware advertises and passes encoder preflight.

Streaming tests additionally verify RTMP/RTMPS session lifecycle, credential isolation, scene composition, browser-source sandboxing, camera/microphone behavior, and clean stop/reconnect.

### Performance tests

Performance gates use a checked-in certification matrix that fixes the machine, software stack, workload, display mode, warm-up, sample duration, number of runs, aggregation method, and measurement tooling. Engineers may not substitute an ad-hoc "similar machine" for release certification.

The baseline release thresholds are normative:

- cold launch to interactive shell <= 2.0 s;
- warm launch <= 1.0 s;
- page-navigation response <= 100 ms, excluding intentionally asynchronous backend completion;
- first visible search results <= 50 ms for a warm index;
- UI render target 60 FPS at the certification display mode;
- base GUI <= 250 MB RSS excluding AI;
- gamewatch <= 50 MB RSS;
- sessiond/telemetry process <= 50 MB RSS;
- normal visible telemetry interval supports 0.25 s minimum;
- overlay telemetry transport may update 10–60 Hz by metric;
- no per-frame heap allocation in the overlay hot path;
- overlay disabled targets < 0.1% measurable average-FPS impact from dormant hooks; a result >= 0.1% requires a recorded performance exception and investigation;
- overlay visible targets < 0.15 ms CPU frame overhead and < 0.20 ms GPU render overhead at 1440p;
- overlay render path performs no synchronous disk I/O and no per-frame D-Bus round trip;
- 1440p60 hardware recording is the baseline capture target;
- 4K60 is required where the certification encoder advertises the required capability and passes preflight;
- median average-FPS capture overhead <= 5% for each mandatory GPU-bound certification workload, with the matrix separately gating 1% low/frame-time regression;
- no blocking hardware discovery on the UI thread.

### Accessibility and localization tests

Production-screen acceptance includes keyboard traversal, focus visibility, accessibility-role/name exposure for interactive controls, icon-only semantic labels, non-color critical-state assertions, scalable-text bounds, and reduced-animation behavior. Localization fixtures exercise expansion-prone strings through the same layout/visual-regression harness; tests assert that user-visible control logic does not depend on English text.

### Hardware certification

A release claiming full RX 6000+ coverage requires at least:

- one RX 6000 / RDNA2 discrete desktop fixture;
- one RX 7000 / RDNA3 discrete desktop fixture;
- one RX 9000 / RDNA4 discrete desktop fixture;
- one RX 6000+ mobile/hybrid A+A fixture with built-in display;
- one multi-adapter APU + RX 6000+ dGPU configuration, which may overlap the mobile fixture;
- one multi-monitor VRR fixture.

Generation-specific behavior is recorded independently. Passing on RDNA4 does not imply passing on RDNA2.

### Migration, lifecycle, and uninstall tests

Tests cover:

- fresh install;
- schema upgrade;
- failed migration rollback;
- settings snapshot compatibility;
- service activation after login;
- GUI closure while services remain active;
- logout/reboot during recoverable state;
- package upgrade with active user state;
- package removal while idle;
- package removal during tuning;
- package removal during risky display transaction;
- package removal during recording/replay;
- package removal while a per-game runtime profile is active;
- refusal to uninstall when safe quiescence cannot be verified.

### What constitutes a good test

A good test asserts user-visible behavior, protocol behavior, effective hardware/provider state, durable state, or explicitly documented safety invariants.

A weak test asserts internal implementation details that could change while behavior remains correct.

Prefer, in order:

1. product/UI behavior at the highest stable seam;
2. typed service contract behavior;
3. provider contract behavior;
4. pure unit tests for parsing, serialization, validation, and state reduction.

Direct tests of private helpers are justified only when the logic cannot be exercised deterministically through a higher seam.

---

## Out of Scope

Pre-RDNA2 product parity is out of scope. Vega, Polaris, and older hardware do not require feature inventory, reference capture, certification, or parity closure.

Windows driver-package installation is out of scope.

Windows driver-package rollback/clean-install workflows are out of scope.

Windows Update integration is out of scope.

Windows Registry manipulation is out of scope.

Windows driver-service management is out of scope.

Recreating AMD's Windows Full/Minimal/Driver Only installer is out of scope.

Installing or replacing the Linux kernel, Mesa, RADV/RadeonSI, firmware, libdrm, or distribution graphics packages is out of scope.

Seizing DRM master from an active compositor as a normal display-management mechanism is out of scope.

Bypassing Wayland security to capture framebuffer contents is out of scope.

Bypassing anti-cheat or protected-process integrity mechanisms to force overlay injection is out of scope.

Pretending that generic frame limiting is exact Anti-Lag behavior is out of scope.

Pretending that Gamescope spatial upscaling is AFMF is out of scope.

Faking proprietary Smart Technology semantics through unrelated Linux controls is out of scope.

A generic Linux control-panel redesign is out of scope.

A generic CoreCtrl/LACT/MangoHud/OBS wrapper with Adrenalin styling is out of scope.

Electron, PWA, browser-hosted, or localhost-served implementation of the main application is out of scope.

Cloud dependency for core Radeon control is out of scope.

AI as a required dependency is out of scope.

Automatic telemetry upload without explicit opt-in is out of scope.

Automatic installation of distribution package updates is outside the v1 product boundary; status and handoff are in scope.

A stable third-party public API is post-v1 and is not a v1 release blocker. Internal typed service contracts are required for v1.

Public branding/trademark decisions and final source license selection are administrative release decisions, not blockers for implementation of the internal product.

---

## Further Notes

### Ubiquitous language

Use these terms consistently:

**Reference** — the selected Windows AMD Software: Adrenalin Edition build/configuration used to define applicable product behavior.

**Parity feature** — a user-facing capability or workflow represented in the reference product.

**Capability** — discovered fact describing whether a subject can support a feature and, where relevant, its valid range/modes.

**Configured value** — the value requested/stored by the product.

**Effective value** — the value verified as active in the provider/platform.

**Provider** — the Linux implementation mechanism responsible for a feature.

**Subject** — the stable entity to which a capability or metric applies, such as a GPU, display, game, CPU, encoder, or system.

**Operation** — one state-changing action with stable identity and observable lifecycle.

**Profile** — a named collection of configuration that may be global, per-game, display-related, or tuning-related.

**Parity blocker** — an applicable Class D feature that cannot yet be implemented truthfully.

**Class N exclusion** — a reference feature whose responsibility does not belong to the Linux application.

**Reference-required** — a decision that must be resolved through controlled reference capture, not engineering taste.

**Backend-research-required** — an applicable feature whose Linux provider semantics are not yet proven.

### Module boundaries

The implementation should preserve deep module boundaries rather than distributing hardware details through UI code.

The main conceptual modules are:

- application shell and design system;
- session/application state;
- hardware discovery/capabilities;
- telemetry;
- profiles/game state;
- graphics runtime features;
- display;
- tuning;
- capture/media;
- streaming/scenes;
- hotkeys/notifications/search;
- update visibility/system information;
- Smart Technology equivalents;
- optional AI;
- IPC/contracts;
- provider policy;
- diagnostics/recovery.

Each module owns its domain invariants and exposes the smallest useful interface upward.

### Preferred dependency direction

UI depends on typed product/domain interfaces.

Product/domain logic depends on provider abstractions.

Provider implementations depend on Linux platform APIs.

Privileged implementations remain below the authenticated daemon boundary.

No QML page depends directly on sysfs, DRM, PipeWire, Gamescope, package-manager commands, SQLite, or raw D-Bus.

### Architectural invariants

1. There is exactly one core SQLite writer.
2. There is exactly one high-frequency telemetry producer.
3. There is exactly one elevated service.
4. Privileged mutations are semantic and revalidated.
5. Risky display recovery does not depend on the GUI remaining alive.
6. Tuning recovery does not depend on volatile in-memory state.
7. No feature becomes "supported" from a GPU-name check alone.
8. No Class D feature silently becomes a no-op.
9. No Class N feature is treated as unfinished parity.
10. Linux package/kernel/firmware ownership remains outside the application.
11. Per-game runtime state is scoped and reversible.
12. Secrets never use plaintext fallback.
13. UI success follows verified backend success.
14. Service restart requires reconciliation before new mutations.
15. Release certification is generation-specific.

### Delivery strategy

The engineering sequence remains intentionally vertical:

**Foundation:** reference corpus, design tokens, shell, typed IPC, capability graph, persistence, parity ledger.

**Read-only vertical slice:** system information, hardware discovery, games, displays, metrics, preferences, notifications.

**Privileged vertical slice:** tuning daemon, validated transactions, recovery journal, hardware certification.

**Game/runtime vertical slice:** per-game profiles, runtime providers, overlay, hotkeys, native/Proton certification.

**Capture vertical slice:** recording, screenshots, replay, media, then streaming/scenes.

**Display vertical slice:** compositor-safe controls, risky rollback, multi-monitor/VRR certification.

**Conditional feature slice:** Smart Technology, mobile/hybrid paths, video quality features.

**Optional AI slice:** local assistant and typed tools.

**Parity closure:** every ledger row verified, blocked, or Class N with evidence.

### Agent execution rule

An implementation agent must not fill gaps with "reasonable" Linux behavior when the specification marks behavior as reference-dependent or backend-research-dependent.

When a missing fact is encountered:

- reference-dependent behavior is resolved through the reference-capture workflow;
- Linux backend semantics are resolved through provider research/prototyping;
- product scope conflicts are resolved against this spec and the source PRD;
- Windows-only responsibilities are classified through the Linux ownership test;
- no assumption may silently become product behavior.

### Ready-for-agent condition

This spec is considered ready for implementation work because the following have already been decided:

- product boundary;
- hardware scope;
- native technology stack;
- process ownership;
- primary test seams;
- privilege boundary;
- parity classification model;
- capability/provider model;
- cross-process contract rules;
- persistence ownership;
- recovery invariants;
- display/tuning safety model;
- runtime/game integration model;
- capture/streaming ownership;
- Linux package-management boundary;
- hardware certification families;
- parity Definition of Done.

The remaining work is implementation, reference capture, provider research for explicit Class D/research-gated features, and creation of implementation tickets.

