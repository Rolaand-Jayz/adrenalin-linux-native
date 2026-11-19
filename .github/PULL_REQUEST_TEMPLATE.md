## What this changes

<!-- One or two sentences. -->

## Ticket and spec

<!-- Ticket number(s) and the relevant engineering spec sections or IDs. -->

## Checklist

- [ ] I read the ticket's full acceptance criteria and the relevant spec sections.
- [ ] No stubs, placeholders, fixtures, mocked success, or dead UI are presented as working features.
- [ ] Unavailable providers, permissions, or hardware report a truthful state and fail safely.
- [ ] Success is reported only after the effective state is verified.
- [ ] No kernel, Mesa, firmware, package, or other host state is changed.
- [ ] The diff adds no hardcoded filesystem paths. Any protocol constant is documented.
- [ ] Shared contracts (D-Bus interfaces, schemas, telemetry ABI, capability representation, recovery protocols) are unchanged, or the change was agreed in an issue.
- [ ] Tests are added or updated at the highest stable seam.
- [ ] The ticket pack and `docs/orchestration/progress-state.md` are updated. Partial work is marked partial.
- [ ] No proprietary AMD images or assets are added.

## Hardware-facing changes

<!-- Delete this section if the change does not touch GPU or display state. -->

- [ ] Authorization is obtained before the change.
- [ ] Rollback works without a second prompt and without the GUI staying alive.
- [ ] Limits come from discovered device capability, not a GPU-name check or table.
- [ ] Reviewed by someone other than the builder.

## Validation

<!-- Exact commands run and their results. Say which were run locally and which in CI. -->

## Tested on

<!-- Distribution, kernel, session type, GPU, Mesa, Qt. Write "not run on hardware" if it wasn't. -->
