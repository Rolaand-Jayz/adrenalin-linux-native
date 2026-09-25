# Hardware1 v1 capability vocabulary

`interfaces/hardware1_registry.{h,cpp}` owns the implementation's stable
capability IDs and subject-kind applicability for the Hardware1 contract. The
registry is a vocabulary catalog; registration does not indicate a supported
feature, a discovered value, or an available provider. Providers must still
supply evidence, and the graph must report `UNKNOWN` or
`PROVIDER_UNAVAILABLE` when support has not been established.

## Catalog scope

The v1 catalog currently includes selected metrics and controls named by the
Final Audited Engineering Spec. Some subject assignments are implementation
choices because the spec defines subject kinds but does not map every feature to
a subject:

- GPU metrics and clock/fan/voltage controls map to `GPU_PCI`.
- CPU utilization, frequency, and temperature IDs describe per-package values
  and map to `CPU_PACKAGE`; system-wide CPU measures require a `PLATFORM` ID.
- System RAM maps to the single `PLATFORM` subject.
- Display controls map to `DISPLAY`. Static display specifications belong in
  `GetDeviceInfo`, not in the capability graph.

`gpu.metric.*` records can describe whether a source can publish a metric with
its declared validity semantics. They never contain or version volatile sample
values; live samples and metric definitions belong to Telemetry1.

IDs are lowercase stable tokens and are returned by `capabilityRegistryV1()`.
`Capability::isValid()` checks registry membership and subject-kind applicability,
so unknown IDs and non-listed pairs cannot pass record validation. A golden
SHA-256 over the sorted ID set pins the v1 vocabulary in its contract test. The
registry does not use labels, marketing names, or array positions to infer scope.

## Deliberately not assigned here

The catalog does not assign units or enum domains. Those depend on provider
semantics and verified source representation; a provider must use a token from
the eventual versioned unit/enum vocabularies and may not invent a unit ad hoc.
It also does not define provider IDs or evidence codes. Those vocabularies need
the provider arbitration and evidence provenance contracts before providers can
be selected or report support.

Game/runtime metrics such as FPS, frame time, percentile FPS, and stutter rate
are omitted because this Hardware1 contract's subject kinds have no GAME
subject and the audited spec does not assign those metrics to a GPU or platform
subject. Conditional tuning, runtime, and platform features are omitted when
their exact subject scope or capability semantics are not explicit in the
contract. An omitted or unknown capability ID cannot be published through a
catalog lookup and cannot be marked `SUPPORTED` as a consequence of this
registry.

The registry is not a complete capability graph and does not close Ticket 03,
Ticket 05, provider certification, or ID-185 readiness. Expand it only with
stable IDs and scopes supported by the audited feature contract and a reviewed
source/provenance definition.
