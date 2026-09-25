# Settings1 contract notes

`Settings1` exposes the product telemetry consent tracer and the schema-defined
`toast.notifications` boolean preference. Toast notifications are not initialized
to a guessed default: until a reference-backed value has been chosen and saved,
`GetToastNotifications` returns `UNAVAILABLE` with an empty service envelope.
`SetToastNotifications` can initialize that preference at revision zero.

Successful reads carry the Session1 service UUID, generation, shared event
sequence, value, and preference revision. Updates require an operation ID and
expected revision. A changed value is persisted transactionally and publishes
one `ToastNotificationsChanged` event on the same service-wide event sequence
used by the other Session1 families. No-op updates and exact replays publish no
additional event; stale revisions and operation-ID reuse with a different
payload fail explicitly.

This is a production persistence and IPC slice, not the full settings surface.
General settings enumeration, settings export/import, reference-derived default
behavior, and a user-facing preference control remain open in their owning
tickets.
