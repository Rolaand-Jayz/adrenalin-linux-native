# Telemetry1 v1 contract proposal (review artifact)

This patch is a proposed typed IPC + ABI test fixture for audited-spec IDs
030–034, 093, 104–110, and 153. It is not a production telemetry provider.
The wire schema is versioned by the interface-major suffix and by the ABI
major in the mapped header. Guard and sample sequence counters never wrap;
publishing fails closed when either would exhaust its remaining range.

## Negotiation and records

`OpenStream(requested_abi_major)` returns a typed result, a Unix descriptor
handle array, metric records, and subject records. The result tuple is
`(code, diagnostic, service-instance UUID, service generation, producer
generation, metric-definition generation, subject-definition generation, ABI
major, mapped byte length)`, signature `(sssttttqt)`. The handle is intended
for read-only mapping; no path or caller-selected shared-memory name crosses
D-Bus. Each metric record is `(stable metric ID, value kind, unit, state
semantics)` and each subject record is `(subject kind, opaque stable subject
ID, label, identity scheme)`. Subject IDs are typed by subject kind; clients
must not use localized labels, array positions, or process IDs as subject IDs.

The snapshots and ABI header must match exactly on service generation,
producer generation, metric-definition generation, subject-definition
generation, ABI major, and mapped size before a reader accepts a sample.
The descriptor array contains exactly one read-only handle only when `code` is
`OK`; typed error results carry an empty descriptor array and empty definition
arrays. Clients reject any other cardinality and never map a handle from an error
reply.

Definition-generation, producer-generation, ABI-major, and mapped-size changes invalidate the negotiated view and require a complete stream reopen. `DefinitionsChanged` carries the event subject (`PLATFORM` / `platform` for a whole-stream change) and the common ID-093 event prefix. The telemetry-specific generation payload follows that prefix. Its event sequence comes from the shared service allocator and is separate from both slot guard and sample sequence; the isolated fixture uses a fixed sequence only in its test signal.

No high-rate sample travels through D-Bus. ABI v1 fixture values use explicit
`VALID`, `UNAVAILABLE`, or `STALE` state; an unavailable/stale value is ignored
rather than represented as zero. This fixture has one `UNSIGNED_MICRO_UNITS`
metric only to exercise the encoding; the actual metric catalog and source
selection implementation remain production work. The production source order
for each field must implement ID-153 and report the chosen source in diagnostics.

## Fixture safety boundary

`telemetry1_abi_v1.h` is a deliberately minimal ABI fixture: little-endian
x86_64 Linux with GCC/Clang direct lock-free atomics, fixed sizes/offsets, two
slots, and one illustrative typed value. ISO C++ recommends address-free
lock-free atomics but does not require them, so the fixture makes the compiler
and architecture constraint explicit and tests actual forked processes. The test-only mock
must create a UUID-named POSIX shared-memory object, hold its producer descriptor,
reopen a separate `O_RDONLY` descriptor before unlinking the name, and return
only the read-only descriptor. POSIX requires `shm_open` names to begin with a
slash; here the name is an ephemeral identifier formed from a generated UUID,
not a fixed or absolute filesystem path, and it never crosses D-Bus. It writes fixture samples only and makes no
hardware/provider claims. This ABI fixture is a review proposal, not a release
ABI. Its tests cover forked-producer/concurrent-reader publication, slot wrap,
guard exhaustion, malformed state rejection, read-only permission, and
negotiated-generation invalidation. The reader loads every shared payload field
atomically to avoid a C++ data race; an even, unchanged guard alone is not used
to justify copying non-atomic payload bytes. Production adoption still requires
independent review and broader malformed-mapping/fuzz coverage.

## Unresolved dependencies

- A single production service event allocator shared with Hardware1 and other
  Session1 interfaces (ID-093); this artifact does not allocate event numbers.
- Real sessiond-owned producer, per-field AMDGPU/HWMON/libdrm/sysfs source
  selection and diagnostics (ID-031, ID-153).
- `Session1` production root export and fixed v1 topology registration (ID-105,
  ID-107).
- Common Service1 readiness and DEGRADED/RECOVERING gating (ID-106), plus
  restart reconciliation and stream recreation (ID-185 / ID-094).
- Production-size, multi-metric ABI review, broader malformed mapping
  validation/fuzzing and tier hardware evidence.

Therefore this artifact can close only a reviewed contract-fixture slice. It
cannot close Ticket 07, claim live telemetry, claim production readiness, or
claim end-to-end hardware parity.
