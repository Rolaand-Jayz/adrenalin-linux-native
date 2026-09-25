# Profiles1 contract artifact

Profiles1 is a versioned, contract-only interface. It describes profile
identity, bounded scalar settings, optimistic revision checks, canonical
mutation results, and event envelope shape. It does not claim persistence,
profile inheritance, preset behavior, a profile UI, or production event
reconciliation.

## Read snapshots and cursor

`ReadProfile` returns, in order, the result code, service-instance UUID,
service generation, event sequence, and profile record. The service envelope
is outside the `Profile` domain record. `OK` carries a valid profile and a
syntactically valid service identity and per-service event cursor.
`NOT_FOUND` carries an empty profile and the same valid cursor; this is an
authoritative answer at that event position. The cursor may be zero before the
first event. `ProfileChangedEvent` itself requires a nonzero event sequence.
Invalid or unavailable reads carry an empty profile and an empty/zero envelope,
so callers cannot mistake them for an authoritative snapshot. Reply validation
rejects malformed service identity, zero service generations, malformed
successful profiles, and payloads or service cursors on invalid or unavailable
answers.

## Mutation results and events

`UpdateProfile` returns only the canonical `MutationResult` fields declared by
the wire schema. The contract mock has a local `UpdateOutcome` wrapper that
contains that mutation result and an optional typed `ProfileChangedEvent`.
The event exists only when the current call changes the profile and publishes
a fresh event. An idempotent replay returns the original mutation result and no
second event; no-op and error results also have no event. Event sequence and
service identity therefore do not appear as ambiguous snapshot cursor fields on
the mutation result.

The event identifies the service incarnation, sequence, stable subject, and
resulting profile revision. Consumers still require the production shared
allocator and authoritative snapshot reconciliation before this contract can
be treated as production event delivery.
