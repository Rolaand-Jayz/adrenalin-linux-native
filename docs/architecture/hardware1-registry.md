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

- GPU metrics and clock, voltage, VRAM, fan, power, tuning-mode/preset,
  Variable Graphics Memory, and stress-test capabilities map to `GPU_PCI`.
- Smart Access Memory is platform-wide and maps to `PLATFORM`.
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
so unknown IDs and non-listed pairs cannot pass record validation. Golden
SHA-256 digests over the sorted ID set and ID-to-scope mappings pin the v1
vocabulary in its contract test. The
registry does not use labels, marketing names, or array positions to infer scope.

## V1 record vocabularies

Provider IDs, evidence codes, units, and closed enum values are also explicit
v1 registries. `Capability::isValid()` rejects identifiers outside those
registries, units that do not apply to the named capability, and enum options
outside the capability's closed domain. A supported numeric value must use its
registered capability-specific unit. A supported enum may publish provider-
verified allowed options before a current configured/effective option is known.

The current provider and evidence IDs are vocabulary tokens only. They do not
assert a runtime provider exists, was selected, or observed a specific machine.
Tokens under `test.*` are reserved for test fixtures. Unit and enum mappings are
implementation-owned until reconciled with provider contracts and authentic
reference behavior; registration alone does not certify parity or support.

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
