# Changelog

Notable changes, summarized from the commit history. Dates are UTC. There are no releases yet.

## 2026-11-19

- **Project hygiene:** added a contributing guide, code of conduct, issue and pull request templates, and this changelog.

## 2026-09-25

- **Hardware inventory:** added CPU and display inventory sources, a Hardware1 v1 capability ID catalog that covers GPU tuning capabilities, closed record vocabularies, and composed inventory snapshots. The session service now exports provider-backed Hardware1 snapshots, reconciles hotplug changes, and reconciles snapshots by event cursor. CPU packages expose static metadata.
- **Contracts:** defined Notifications1, Profiles1, Hotkeys1, and Display1 contract artifacts, and reconciled the Telemetry1 event cursor contract.
- **Notifications:** persisted notification state and events, and added a notification history client and panel with a mark-read control. Notification errors are now actionable. The toast notification preference is persisted, reconciled by the client, and has a shell control.
- **Profiles:** persisted Profiles1 session updates and added client snapshot reconciliation.
- **Display and hotkeys:** added safe Display1 identity reads with private-bus integration tests, and client snapshot reconciliation for Display1 and Hotkeys1.
- **Reference capture:** exported externally observed candidate geometry.
- **Telemetry fixture:** hardened ABI reader validation, validated producer headers and sample identity, and documented the fixture trust boundary.
- **Fixes:** enforced the Hardware1 registry and startup envelope, aligned the Profiles1 snapshot cursor contract, and removed a fixed interpreter path from a helper.
- **Docs:** expanded the repository agent instructions, updated the public project status, and recorded inventory probe evidence, regression evidence, and ticket progress.

## 2026-09-24

- **Bootstrap:** native Qt shell, build constraints, and a fixed-reference visual parity harness.
- **Session service:** persisted session preference tracer, typed mutation results, a shared service-readiness contract with sequenced events, and sequenced consent change events. The session client reconciles across retries, and the settings client is gated on service readiness.
- **Contracts:** a bounded capture and Hardware1 contract slice, hardened event and parity evidence contracts, and a telemetry contract fixture.
- **Hardware inventory:** a bounded libdrm GPU inventory source and opaque PCI GPU subject identities.
- **Build and CI:** preserved configured install activation paths, fixed the CI Qt architecture, and guarded a GCC 16 warning flag on older compilers.
- **Docs:** a portable reference capture workflow, shell smoke and clean-checkout evidence, ticket progress records, and the closed native shell CI gate.
