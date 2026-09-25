# Hotkeys1 contract

`org.adrenalinlinux.Session1.Hotkeys1` is the v1 session-service contract for
reading application shortcut actions and requesting a binding update. The XML
schema and typed C++ record definitions are the wire authority for this
contract. This artifact is a prerequisite only; it does not provide a portal or
X11 provider.

## Snapshot and event model

`ListHotkeys` returns a complete action snapshot and a shared service envelope:
service-instance UUID, service generation, event sequence, and revision. A
failed snapshot carries no action records or success cursor. Action IDs are
stable lowercase tokens and labels are message keys, not identities. A
configured binding and an effective binding are separate fields; effective
state must reflect provider read-back when a production provider is added.

`HotkeyChanged` is an ID-093 event with subject kind `HOTKEY_ACTION` and the
stable action ID as subject. Clients treat it as a hint, ignore duplicate
sequences, and reconcile from `ListHotkeys` after a gap or generation change.

## Updates and provider outcomes

`SetHotkey` carries the action ID, requested binding, expected revision,
operation UUID, and an opaque parent-window ID. The parent ID supports
user-visible portal interaction; it grants no authority. Binding strings are
bounded opaque provider input in this contract. The test value
`Ctrl+Shift+O` is only a wire fixture and does not define a platform grammar.

The result uses the shared operation vocabulary. Provider denial, conflict,
unsupported state, and `INTERACTION_REQUIRED` remain explicit results. No
fallback provider or successful effective state may be inferred by this
contract. Replaying a matching operation ID returns its cached result and must
not publish a second state-change event; reuse with a different request is a
conflict.

## Implementation boundary

The test mock proves schema serialization and method behavior only. Production
implementation remains subject to Ticket 03 recovery/event reconciliation and
Ticket 04 shell prerequisites. Wayland must use GlobalShortcuts v2 or newer,
portal session recreation and a real visible parent where needed. X11 must use
its certified X11 provider. These backends, their effective-state behavior,
reconnect recovery, and an end-to-end shortcut action are not implemented by
this contract artifact.
