# Luna Authority, Safety, Reasoning, and Review Rules

## Authority order

Use this precedence whenever sources disagree:

1. **Final Audited Engineering Spec**
2. Final Audited Ticket Pack and ticket acceptance criteria
3. Final audited PRD and approved reference material
4. Existing implementation, where it does not conflict with the above
5. Agent judgment

Tickets are execution slices. They do not narrow, override, or replace requirements in the Final Audited Engineering Spec.

If a ticket, code path, documentation statement, worker conclusion, or assumption conflicts with the Final Audited Engineering Spec, the **Final Audited Engineering Spec wins**.

## Reference authority

For Adrenalin behavior that is reference-gated, use the fixed product reference defined by the Final Audited Engineering Spec:

**AMD Software: Adrenalin Edition 26.9.1 Optional — 2026-09-03**

Do not silently substitute behavior from a newer release.

Do not guess exact Adrenalin behavior when the spec requires reference capture.

## Truthfulness

Never report success where effective backend state cannot be verified.

Never substitute a neighboring Linux feature simply because it produces vaguely similar results.

Configured state and effective state must remain distinguishable where the spec requires it.

Unsupported or unavailable capabilities must remain truthful.

## Safety invariants

Never weaken these requirements to make implementation easier:

- privileged daemon isolation
- authenticated caller identity
- polkit boundaries
- capability-generation validation
- idempotent cross-process mutation
- display rollback
- two-party display commit/recovery
- tuning recovery journals
- tuning rollback
- known-safe startup behavior
- secret-store requirements
- browser sandboxing
- Wayland security boundaries
- anti-cheat/protected-process boundaries
- import validation
- safe uninstall/quiescence
- truthful effective-state verification

A safety failure blocks ticket completion.

## Reasoning policy

The lead Luna orchestrator should operate with **High reasoning** for the project.

Classify each ticket/subtask by required reasoning depth:

### Low

Use for:
- mechanical edits
- formatting
- generated bindings
- repetitive packaging changes
- straightforward test expansion

### Medium

Use for:
- ordinary feature implementation
- established provider integrations
- normal UI/backend work
- clear acceptance-criteria work

### High

Use for:
- architecture
- concurrency
- IPC contracts
- capability semantics
- provider selection
- cross-service state
- complex runtime integration
- difficult debugging
- migration/recovery logic

### xHigh / exceptional reasoning

Reserve for:
- privilege boundaries
- tuning safety
- risky display transactions
- crash consistency
- security-critical state machines
- difficult Class D research
- problems that resisted correct resolution at lower reasoning depth

If the orchestration environment exposes worker reasoning controls, use these classifications directly.

If it does not expose worker reasoning controls, treat the classification as an execution requirement:

- give High/xHigh work narrower scope;
- provide more complete context;
- use the strongest available worker;
- require stronger independent review;
- escalate unresolved work to a fresh investigation rather than repeatedly patching symptoms.

Do not waste deep reasoning on mechanical tasks.

## Builder + adversarial reviewer

For safety-critical or architecture-critical work, use separate builder and reviewer workers whenever capacity allows.

Use this pattern especially for:

- privileged hardware mutation
- tuning transactions/recovery
- risky display transactions/recovery
- D-Bus contract changes
- telemetry ABI changes
- schema changes
- capture-state recovery
- package removal/quiescence
- security/privacy boundaries
- Class D research conclusions

The reviewer should specifically look for:

- violated invariants
- crash/restart edge cases
- race conditions
- privilege escalation paths
- stale-state bugs
- unsupported assumptions
- false-success states
- conflicts with the Final Audited Engineering Spec

The lead orchestrator resolves review findings before integration.

## Completion standard

A ticket is complete only when:

- every applicable acceptance criterion is satisfied;
- required tests pass;
- safety and architecture invariants remain intact;
- effective state is verified where required;
- review findings are resolved;
- evidence is recorded;
- the implementation is integrated coherently.

The project is complete only when the Final 1:1 Parity Closure ticket passes.
