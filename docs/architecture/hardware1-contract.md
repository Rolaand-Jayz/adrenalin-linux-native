# Hardware1 v1 contract

This document defines the session-bus Hardware1 schema choices left to the
versioned interface definition by engineering-spec IDs 017, 018, 020, 023,
035, 036, 093–096, 105–107, and 185. It does not grant support to a provider or
describe a hardware mutation API.

## Methods and reply envelope

`org.adrenalinlinux.Session1.Hardware1` is exported by the core session service
at the fixed Session1 root object.

- `ListDevices()` returns a typed result and one snapshot envelope containing
  service-instance UUID, service generation, inventory generation, and
  capability generation, followed by device records.
- `GetDeviceInfo(subject_kind, subject_id)` returns a typed result, the same
  four-part snapshot envelope, and static information for the requested
  resolved subject.
- `GetCapabilityGraph(subject_kind, subject_id)` returns a typed result, the
  same four-generation envelope, and the requested subject's capability
  records.

Every response carries service UUID, service generation, inventory generation,
and capability generation, so a caller can detect a capability-only change
between `ListDevices`, `GetDeviceInfo`, and `GetCapabilityGraph`. Each response
is built from one immutable in-process snapshot. A caller must discard a
combination of replies if any envelope field differs. The inventory generation
starts at one after the first successful reconciliation in a service
incarnation. It advances when resolved subject membership or typed
identity-bearing evidence changes; order and presentation-label changes do not
advance it. The capability generation starts at one after the first complete
graph build and advances when any published graph state, provider, evidence,
range, configured value, effective value, or failure state changes. Volatile
telemetry samples are not capability-graph facts and do not advance it. The
service generation advances only on wholesale published-state reset. All three
counters are monotonic within the service instance. The inventory and
capability counters are not persisted across a service-instance UUID change. A
counter never wraps: exhaustion fails the service closed before publishing an
ambiguous generation.

Hardware1 reads are available only after initial inventory and capability
reconciliation has produced a complete envelope. While the service is
`STARTING` or `RECOVERING`, reads return `BUSY`, `retryable=true`,
`snapshot_valid=false`, service UUID/current service generation, zero inventory
and capability generations, and no record payload. If initialization fails
before a snapshot exists, reads return `BACKEND_UNAVAILABLE` with the same
invalid-snapshot envelope and no payload. Generation zero is reserved for this
invalid envelope and is never a valid snapshot generation. A `DEGRADED` service
may return `snapshot_valid=true` with a complete envelope whose unavailable
providers are represented truthfully in the graph.

Identity changes that remove a subject make that subject unavailable to
subsequent reads. A lookup for a removed or unknown subject returns the
canonical typed `NOT_FOUND` or `DEVICE_DISCONNECTED` result and no stale
record. Read calls are side-effect free. Future mutations must carry the
service UUID, service generation, and capability generation from the graph
they used; stale capability generations are rejected before provider entry.

## Subject identities

Every addressed subject is the pair `(subject_kind, subject_id)`. Kinds are
`CPU_PACKAGE`, `GPU_PCI`, `DISPLAY`, and `PLATFORM`. The reserved `PLATFORM`
subject ID `platform` is the only non-opaque token exception; consumers must
branch on `subject_kind` and must not parse the ID string.

- `GPU_PCI` identity derives from canonical PCI domain, bus, device, and
  function evidence. BDF is identity evidence, never a display label.
- `CPU_PACKAGE` identity derives from stable physical package/topology
  evidence. Logical CPU, thread, and array positions are not identities.
- `DISPLAY` identity derives from the owning GPU identity, canonical connector
  evidence, and EDID-derived evidence. Raw EDID bytes, serial values, device
  node names, and filesystem paths do not cross this interface.
- `PLATFORM` is the single stable platform-wide subject.

Names and labels are presentation fields only. Subject IDs are opaque to IPC
consumers; they are stable tokens derived from typed identity evidence, not
values callers may parse. GPU BDF evidence is canonicalized as lower-case
hexadecimal domain, bus, device, and function components before token creation.
Display token inputs include the owning GPU token, normalized connector
identity, and a digest of EDID identity fields; the raw EDID and serial are
never retained in the token or exposed. CPU token inputs include stable
physical package evidence and the platform CPU identity needed to distinguish
packages. Duplicate or contradictory evidence is marked ambiguous and never
resolved using labels or array order.

The device list contains resolved canonical GPU, CPU-package, and display
subjects. Unresolved hardware
observations are diagnostics, not Device/Display subjects and not capability
nodes. Their appearance or disappearance is announced, if needed, with the
reserved `PLATFORM` subject as the event subject; callers then refresh the
inventory. An empty subject ID is never returned as a device identity. The
same typed Display subject and token are used by Hardware1 and Display1, and
Display1 requests/replies carry the same inventory generation and capability
generation vocabulary.

## Capability records

Capability IDs, provider IDs, evidence codes, failure codes, and enum values
are stable, untranslated tokens from versioned product registries. Their
display text belongs to the UI. Each record carries subject kind and ID,
capability ID, support state, provider ID, evidence code, failure code, unit,
configured value, effective value, minimum, maximum, step, and allowed enum
values. Unit tokens come from the versioned unit registry; numeric values
require a matching unit, while Boolean and enum values require an empty unit.

The closed support states are `SUPPORTED`, `UNSUPPORTED`, `UNKNOWN`, and
`PROVIDER_UNAVAILABLE`. A `SUPPORTED` record requires both a selected provider
ID and provider or observed evidence. It may omit configured value when the
product does not own a setting, and may omit effective value when the provider
cannot verify the current state; absent effective state is never displayed as
active. Ranges and allowed enum values are present only when the selected
provider verifies them. `UNKNOWN` and `PROVIDER_UNAVAILABLE` carry no
configured/effective values, ranges, or allowed values. `PROVIDER_UNAVAILABLE`
requires the canonical `BACKEND_UNAVAILABLE` failure code. `UNSUPPORTED` is
distinct from unknown, requires evidence, and carries no configured/effective
values, ranges, or allowed values. Configured and effective values are always
separate.

Each configured/effective/bound/step member is always present in the fixed wire
record and is its own closed discriminated union value; unavailable values use
`kind=NONE`, never omitted wire fields. One member's `NONE` does not affect
another member. Inactive scalar payload members are ignored according to the
tag and must be zero/empty in serialized records. Values use a closed
discriminated union with kinds `NONE`, `BOOLEAN`,
`SIGNED_INTEGER`, `UNSIGNED_INTEGER`, `REAL`, and `ENUM`. Only the member named
by `kind` is meaningful. `NONE` means no value is available; it is not numeric
zero. Real values must be finite. A numeric range is either wholly absent
(minimum, maximum, and step are all `NONE`) or has typed minimum and maximum of
the same kind and unit; step may be `NONE` for a continuous range or a positive
value of that kind and unit. Numeric values require a unit token. `ENUM`
values and allowed values are stable untranslated tokens. An `ENUM` value
requires a non-empty allowed-values set containing that value. Boolean and
enum controls do not carry numeric ranges. Unknown kinds, mismatched members,
partial or inverted bounds, non-positive steps, NaN, and infinities are
contract errors.

Capability records are never inferred from marketing names. No provider or
evidence means no `SUPPORTED` state.

## Events and readiness

Hardware inventory and capability changes are versioned hints. They use the
shared per-service event sequence and the ID-093 envelope (service-instance
UUID, service generation, event sequence, stable subject ID). Resolved subject
events use that subject; unresolved discovery diagnostics use the stable
`PLATFORM` subject. A removed subject may be named in its final disconnect
event, after which reads return `DEVICE_DISCONNECTED` while its removal is
known to the current service incarnation. Otherwise an unknown subject returns
`NOT_FOUND`. Clients refresh through the read methods after a gap, reconnect,
owner change, identity change, or generation mismatch. Initialization cannot
become `READY` until hardware and capability reconciliation required by ID-185
has completed.

## Error behavior

Read methods return an in-band typed result using the canonical v1 result-code
vocabulary from ID-091. A reply includes `code`, `human_message_key`,
`diagnostic_message`, `retryable`, `provider`, `subject_id`, `snapshot_valid`,
and the full snapshot envelope. A failed read has no device or capability
payload.
`NOT_FOUND` means a syntactically valid subject was not present in the current
service incarnation; `DEVICE_DISCONNECTED` is used when the current service
observed removal of that subject. `BACKEND_UNAVAILABLE` means required
discovery/provider access is unavailable; an empty successful inventory means
discovery completed and found no resolved subjects. Diagnostic text is for
logs only; clients branch only on the stable code and message key. D-Bus
transport errors are reserved for process/protocol failures, not domain
outcomes.

## Contract-test requirements

Generated bindings and a private-bus round trip must verify exact record
signatures and every field. Contract tests also cover unresolved and duplicate
identities, duplicate BDF/EDID evidence, label changes without identity or
capability changes, subject removal, Hardware1/Display1 display-ID round trips,
independent inventory/capability/service generation increments, provider or
capability changes between list and graph reads, stale reply envelopes, all
support states, every legal/illegal value-field combination, absent values,
wrong value kinds, non-finite reals, and rejection of unsupported records that
claim success. The mock is test-only and is not registered by the shipping
session service.
