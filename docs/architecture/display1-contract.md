# Display1 v1 contract

Display1 is a versioned interface on the core `org.adrenalinlinux.Session1`
session service at the Session1 root object. It defines typed display inventory,
state, validation, and apply operations. It is a contract artifact; this
interface and its fixture mock do not select or register a production display
provider.

## Identity and snapshots

Display1 uses the Hardware1 `DISPLAY` subject and opaque subject ID. The ID is
derived from owning GPU, canonical connector identity, and EDID identity
evidence. Labels and array order are presentation only. Raw EDID bytes, serials,
device paths, and connector filesystem paths are not fields in this interface.
Display reads return the Hardware1 result envelope, including the service
instance UUID, service generation, inventory generation, and capability
generation. A caller must discard combined state if any envelope member differs
and refresh after restart, event gap, owner change, identity change, or stale
generation. Capability rows use the Hardware1 closed value union and support
states; configured and effective values remain distinct, and unknown or
unavailable values are never represented as zero.

Before an initial complete snapshot exists, `BUSY`/recovering and
`BACKEND_UNAVAILABLE` reads use the Hardware1 invalid-snapshot envelope:
service identity and nonzero service generation remain present, inventory and
capability generations are zero, `snapshot_valid` is false, and no display or
capability records are returned.

## Methods

- `ListDisplays()` returns one platform snapshot envelope and the resolved
  `DISPLAY` subjects in that snapshot.
- `GetDisplayState(subject_id)` returns the requested display and its
  capability records from one immutable snapshot. Unknown or removed IDs return
  a typed failure and no stale payload.
- `ValidateDisplay(operation_id, subject_id, expected_inventory_generation,
  expected_capability_generation, changes)` validates a typed set of display
  capability updates without mutation. It returns a stable result code,
  validation message keys, the current generation envelope, and a safety
  classification (`SAFE`, `OUTPUT_RISKING`, or `UNKNOWN`). A stale inventory
  fails with `CONFLICT`; stale capability generation fails with
  `STALE_CAPABILITY` before provider entry. An `OK` validation cannot carry an
  `UNKNOWN` safety classification.
- `ApplyDisplay(...)` uses the same stable operation UUID, target ID, expected
  generations, and typed changes. Repeated identical operation IDs and payloads
  return the original result; reuse with a different payload returns
  `CONFLICT`. Success requires provider read-back/effective-state verification,
  a new revision, a nonzero event cursor, and a concrete safety-route intent.
  Malformed UUID or empty-subject requests return a representable typed
  `INVALID_ARGUMENT` result. An unverified apply cannot claim `OK`.

Control identifiers and values are capability-registry tokens and the
Hardware1 discriminated value type. This schema does not define reference
labels, ranges, defaults, or provider support. Those remain evidence-gated.

## Safety and authorization boundary

Display1 is not a privileged API. The session service owns normal display
coordination, while the system service owns narrow privileged low-level display
operations. A production `ApplyDisplay` coordinator must classify output risk
before provider mutation and route every output-risking operation through
DisplayGuard1 first. It may not directly mutate a risky output or accept a
caller-supplied assertion that a change is safe. The guard owns the Keep/Revert
window and recovery lifecycle. The privileged side uses only the
transaction-bound LowLevelDisplay1 prepare/apply-prepared/revert-prepared/
finalize/acknowledge protocol; it is not a generic display-write API.

Authorization must complete before the first output-risking mutation. Prepared
authority is bound to the operation UUID, authenticated UID, display target,
and captured known-good/intended states. The privileged daemon persists only a
verifier/hash; the raw capability token belongs only in the guard's private
recovery record and is never an argument or result of Display1. Recovery must
not require a second authorization prompt. After mutation, expiry cannot remove
the exact known-good revert capability. Two-party terminal records are durable
before acknowledgement/compaction; uncertain post-mutation recovery is
revert-first and blocks further writes when identity or integrity cannot be
verified. A successful apply is published only after effective state has been
verified.

## Events and contract tests

`DisplayChanged` is an ID-093 sequenced hint with service instance, service
generation, event sequence, stable subject ID, inventory generation, and
capability generation. It is not durable truth; clients refresh through the
read methods after a gap or restart. The fixture emits this full event envelope
for a successful synthetic change and the test reads the authoritative state
snapshot afterward. This validates only the contract/wire behavior. Production
ID-093 sequencing, gap detection, reconnect handling, and service snapshot
reconciliation remain open acceptance work.

The private-bus contract test checks exact wire signatures, typed identity and
generation round trips, typed values, stale-generation rejection, operation
idempotency/conflict behavior, pre-snapshot error envelopes, invalid-argument
replies, the full fixture event cursor, and snapshot refresh. Its
`safetyRouteIntent` field is synthetic classification metadata only; the test
does not establish that DisplayGuard1 or LowLevelDisplay1 was invoked. The mock
is test-only, uses explicitly synthetic evidence, and is not registered by the
shipping session service. Crash-phase, authorization, persistence, event-gap,
production recovery, and real provider routing tests remain required before
production display mutation.
