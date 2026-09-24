# Luna Swarm Execution Protocol

## Purpose

This document defines how the lead Luna orchestrator executes the Final Audited Ticket Pack.

The lead orchestrator owns the dependency graph, worker assignment, integration order, cross-ticket consistency, and completion state.

## Work the dependency frontier

A ticket is eligible only when all of its blockers are complete.

Do not work tickets merely because they have a lower number.

Continuously maintain the current frontier:

> **frontier = all incomplete tickets whose blockers are complete**

When several frontier tickets are independent, spawn workers concurrently.

When only one ticket is available, concentrate on that ticket rather than inventing parallel work.

## Swarm behavior

Use as much safe parallelism as the environment supports.

Good parallel domains include:

- UI/reference work
- capability discovery
- telemetry
- game/runtime providers
- display
- tuning
- capture/media
- Smart Technology
- optional AI
- packaging
- testing/certification

Do **not** parallelize workers that would independently modify the same:

- D-Bus contract
- database schema
- telemetry ABI
- domain model
- capability representation
- provider-selection policy
- display recovery protocol
- tuning recovery protocol
- capture ownership/state machine
- privilege boundary
- package ownership contract

Those changes require centralized coordination by the lead orchestrator.

## Worker assignment

Before assigning a worker:

1. Identify the exact ticket.
2. Confirm every blocker is complete.
3. Give the worker the ticket and relevant Final Audited Engineering Spec sections.
4. Identify the existing architectural/test seam it must use.
5. Tell it not to expand scope except for necessary prefactoring.
6. State any neighboring work that must not be disturbed.

Workers must not reinterpret settled architecture independently.

If a worker discovers a shared architectural problem, return the decision to the lead orchestrator instead of creating a local competing convention.

## Per-ticket workflow

For each ticket:

1. Read the complete ticket.
2. Read the relevant Final Audited Engineering Spec sections.
3. Inspect the current implementation before changing it.
4. Implement the smallest complete end-to-end vertical slice satisfying the ticket.
5. Preserve established architecture, ownership, terminology, safety invariants, provider rules, and testing seams.
6. Add/update tests at the highest stable seam.
7. Run focused tests during implementation.
8. Run relevant integration tests before completion.
9. Review the implementation against both the ticket and Final Audited Engineering Spec.
10. Resolve review findings.
11. Record evidence for every applicable acceptance criterion.
12. Commit the completed work coherently.
13. Mark the ticket complete only after all applicable criteria pass.

Writing code is not sufficient evidence of completion.

## Integration protocol

When a worker finishes:

1. Inspect the diff.
2. Compare it to the Final Audited Engineering Spec.
3. Confirm no settled architecture or safety rule changed.
4. Run the ticket's focused tests.
5. Integrate the change.
6. Run affected cross-ticket tests.
7. Update ticket state.
8. Recompute the dependency frontier.
9. Immediately use freed worker capacity on newly unblocked work.

Do not allow workers to silently fork shared contracts.

## Conflict handling

When workers disagree:

1. Consult the Final Audited Engineering Spec.
2. Apply the decision already defined there.
3. Reject implementations that contradict it.
4. If the spec says behavior is reference-gated, run the required reference-capture workflow.
5. If the spec says behavior is research-gated, run the required research/prototype workflow.
6. If the spec is genuinely silent and a new architectural decision is unavoidable, pause the affected branch and resolve it centrally before dependent work continues.

Do not resolve ambiguity by allowing different subsystems to invent different conventions.

## Class D tickets

A Class D resolution does not require pretending an implementation exists.

If research shows a truthful Linux backend is unavailable:

- document the evidence;
- record provider research;
- update the parity ledger;
- retain the Class D disposition;
- implement the truthful unavailable state;
- test that the UI never reports false success.

That is a valid completion outcome when the ticket/spec permits it.

## Class N responsibilities

Class N responsibilities stay outside application ownership.

Do not take ownership of Windows-only or distro-owned responsibilities merely to create superficial parity.

In particular, do not independently install/remove/replace:

- kernel
- Mesa
- firmware
- graphics-stack packages
- compositor
- package-manager state

The application may report status and hand off to the proper Linux owner as specified.

## Continuous execution

Do not stop after one ticket.

Repeat:

**frontier → swarm → implement → test → review → integrate → commit → recompute frontier**

until the Final 1:1 Parity Closure ticket passes.
