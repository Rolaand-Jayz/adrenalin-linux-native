# Hardware1 inventory snapshots and event cursor

Hardware1 read methods return a typed reply envelope with each inventory,
static-information, and capability snapshot. The envelope includes the service
instance UUID, service generation, inventory generation, capability generation,
and the shared per-service event sequence captured with the snapshot. A sequence
cursor of zero is valid before the service publishes its first event; event
signals themselves always use a nonzero sequence.

`ListDevices`, `GetDeviceInfo`, and `GetCapabilityGraph` read the current
service-owned snapshot and its event cursor on the serialized session-service
thread. The cursor lets a client decide whether the snapshot covers common or
Hardware1 events observed while the request was in flight. `NOT_FOUND` and
`DEVICE_DISCONNECTED` can still carry an authoritative snapshot and cursor;
unavailable replies carry no device or capability payload.

Hardware1 signals are hints. Consumers reconcile against `ListDevices` after
inventory/capability changes, sequence gaps, owner changes, or service-generation
changes. They advance through sequential events from other Session1 families
without an unnecessary Hardware1 read and ignore duplicate sequences. The
service-wide `EventPublished` signal and the family-specific Hardware1 signal
describe the same event sequence, so clients must not count that pair as two
events.

This contract does not claim that every other v1 event family has a production
client or that the complete ID-185 startup recovery sequence is implemented.
