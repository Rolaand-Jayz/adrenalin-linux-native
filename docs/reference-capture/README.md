# Adrenalin 26.9.1 reference capture guide

This guide records authentic reference evidence for Ticket 02 against the fixed
baseline **AMD Software: Adrenalin Edition 26.9.1 Optional, released
2026-09-03**. It describes how to collect captures, record what was observed,
bind review artifacts to exact bytes, and invoke the repository's validators.

## Evidence status

The repository does **not** currently contain an authentic reference capture
corpus, complete environment and behavior observations, independently reviewed
annotations, or a passing reference-to-candidate comparison. The existing live
candidate screenshot and application-reported geometry export are candidate
artifacts only; they are not reference evidence or independent geometry
attestation. No reference PNGs are supplied by these documents.

`tools/reference/test_accessibility.py` independently checks accessible names,
roles, and extents for a running candidate window. It does not capture the same
window image bytes or establish the screenshot crop origin and decoration
relationship, so its AT-SPI extents are not emitted as comparator geometry.
Use a separately attested geometry artifact bound to the exact candidate image
for comparison.

Do not populate the worksheets from memory, public web images, product
marketing screenshots, a different Adrenalin release, or generated/test
fixtures. Unknown behavior stays `REFERENCE_REQUIRED`; it is not a pass. A
manifest that says `VALID CAPTURE INPUTS` proves only that its metadata and PNG
bytes meet the validator's structural checks. It does not prove authenticity,
independent review, or visual parity.

## Evidence roles

- **Operator-entered observations** are transcriptions of the Windows reference
  environment and observed UI behavior. Keep them tied to a capture row and
  record unknowns explicitly.
- **Checked evidence** is limited to what the current tools actually inspect:
  JSON shape and uniqueness, fixed baseline identity, project-relative image
  paths, PNG integrity and dimensions, recorded SHA-256 equality, and required
  cross-record hash bindings.
- **Human review** is an independent person's documented decision. A non-empty
  reviewer name or record locator is not proof that review occurred; the
  validator does not fetch or authenticate review records.
- **Parity results** come only from `visual_diff.py compare` with a complete
  reference manifest, review-bound checks, a candidate PNG, and externally
  attested candidate runtime geometry. The comparator checks data bindings and
  the configured gate; it cannot establish the truth of the declared evidence
  provenance.

## Required capture matrix

For each product surface being evaluated, the manifest defines these six
contexts:

| Context ID | Required setup |
| --- | --- |
| `1920x1080-100` | Physical display resolution 1920x1080 at 100% Windows scale |
| `2560x1440-100` | Physical display resolution 2560x1440 at 100% Windows scale |
| `3840x2160-recommended` | Physical display resolution 3840x2160 at the scale Windows recommends for that display; record the observed percentage |
| `narrowest-supported` | Narrowest width the reference application supports without clipping or losing supported controls; record observed physical window size and evidence for the bound |
| `default-window` | Application's initial/default window state after a clean launch; record observed physical size |
| `maximized` | Same supported display with the application maximized; record observed physical size |

Capture `default` in every context. Capture each applicable state from the
manifest matrix as well:

| Manifest state | Reference behavior to capture |
| --- | --- |
| `hover` | Primary control hovered, where a primary control exists |
| `focus` | Keyboard focus visibly placed on the relevant control, where meaningful |
| `open` | Relevant dropdown, menu, or popover open |
| `changed` | A changed/dirty setting before applying or saving, where meaningful |
| `disabled` | Disabled or unavailable hardware/control behavior, where observable |
| `confirmation` | Confirmation modal, where the flow has one |
| `error` | Reproducible failure state, with the trigger recorded |
| `success` | Successful-completion state, where the flow has one |

The context/state matrix is intentionally not a promise that every state exists
on every screen. For each genuinely inapplicable row, record a concrete reason
and have an independent reviewer review that conclusion. The manifest accepts
`not_applicable` only when its `applicability_evidence` points to a checksum-
verified captured row in the **same context**, carries that row's exact SHA-256,
and identifies the reviewer and review record. A missing or unobserved state is
not automatically inapplicable.

## Capture procedure

1. Use an authorized Windows installation running the exact fixed release,
   **26.9.1 Optional (2026-09-03)**. Record the full Windows build/version and
   any relevant driver/app build identifiers. If this installation or release
   cannot be accessed, stop at collection planning; do not substitute another
   source.
2. Record the capture host identifier, Windows version/build, GPU model and
   driver, CPU model, and display configuration. Include each monitor's model
   when available, native resolution, active resolution, scale percentage,
   orientation, and which display contains the application. Avoid recording
   secrets or unrelated personal information.
3. Launch the reference application and select one screen/surface. Use the
   worksheet to walk all six contexts and each meaningful state. Keep dynamic
   values stable with a documented product fixture when available; otherwise
   identify the exact dynamic region and reason in observations. Do not mask a
   region merely because it is difficult to reproduce.
4. Capture the actual application display at native physical pixels. Save the
   original screenshot as PNG without resizing, recompressing, cropping,
   annotating, or converting it. A screenshot from a web page is discovery
   material only. If a state requires a separate window, menu, or modal, capture
   the whole application surface needed to compare it and record the trigger.
5. Immediately compute SHA-256 over each original PNG, record its physical pixel
   width and height, and store it under the capture pack using a normalized
   path relative to the manifest directory. Retain the original bytes and
   collection notes. Never put a workstation-specific absolute path in the
   manifest.
6. Complete all ten observation fields for every captured row. Copy actual
   labels and measured control properties; do not infer min/max/default/step,
   persistence, global-versus-game scope, hotkeys, or transitions. Enter the
   literal `REFERENCE_REQUIRED` for any value that has not been directly
   established. Resolve these unknowns before claiming complete metadata.
7. Have an independent reviewer examine the exact capture bytes and source
   context. Store a durable review record with reviewer, timestamp, decision,
   scope, and the capture ID plus SHA-256 reviewed. Use its stable identifier
   in the manifest or comparison checks. Preserve review records with the
   evidence pack; the validator treats their identifiers as text and does not
   verify the record itself.
8. If a row is truly inapplicable, record its reason and create the required
   same-context reviewed-image binding in the manifest. Do not use this status
   to hide a missing capture or unresolved behavior.
9. Validate the completed manifest. Only then prepare reference annotations,
   color samples, and any reviewed dynamic-region masks for a candidate
   comparison. See [review worksheets](worksheets.md).

## Manifest setup and mapping

Run commands from a repository checkout. The paths below are repository-relative
examples; choose a writable evidence-pack location as needed.

```sh
python3 tools/reference/manifest.py init \
  --output reference-captures/home/capture-manifest.json \
  --screen-id home
```

`init` writes a version 2 manifest with the fixed reference identity, the six
required contexts, and nine state rows per context, all `pending`. It refuses
to overwrite an existing output. It creates no images and records no evidence.

Copy each worksheet observation into the existing manifest row with the
matching `context_id` and `state`. The validator's ten required observation
keys are:

| Manifest observation key | Record |
| --- | --- |
| `visible_labels` | Exact visible text and language; note truncation or wrapping |
| `control_properties` | Control type and observed minimum, maximum, default, and step where applicable |
| `enabled_disabled_conditions` | Conditions under which controls are enabled, disabled, or unavailable |
| `global_vs_game_behavior` | Whether the setting applies globally, per game, or both; show the observed scope |
| `restart_persistence` | Whether changes survive application/service/system restart and how verified |
| `unavailable_error_behavior` | Exact unavailable/error presentation and observed trigger |
| `hotkey` | Exact hotkey/gesture or an explicit observation that none was found |
| `animation_transition` | Observed motion, transition, timing, or explicit absence after checking |
| `parity_ledger_identity` | Stable screen/feature identity in the project's parity ledger |
| `test_identity` | Stable test/acceptance identity associated with the observation |

Each value must be a non-empty JSON value. If unresolved, use the literal string
`REFERENCE_REQUIRED`; that allows the incomplete evidence to remain visible,
but it does not satisfy the requirement to establish the reference behavior.

The corresponding manifest fields are:

- `capture_source`: `host_id`, `windows_version`, `gpu`, `cpu`, and `display`;
- `contexts`: fill `width`, `height`, and `scale_percent` from the reference
  system. Resolution-context dimensions and scales fixed in the manifest must
  stay unchanged. For window contexts, record actual physical screenshot
  dimensions and the observed scale;
- captured row: set `status` to `captured`, `image` to a normalized
  manifest-relative PNG path, `sha256` to lowercase SHA-256 of the exact file,
  `physical_width`/`physical_height` to PNG pixel dimensions, and
  `observations` to the ten-key object above;
- inapplicable row: set `status` to `not_applicable`, enter
  `applicability_reason`, and fill `applicability_evidence` as described above.

Leave `parity` at its initialized `not_run` state. The manifest validator
deliberately rejects a parity claim.

Compute hashes without embedding machine-specific paths:

```sh
sha256sum reference-captures/home/images/<capture-file>.png
```

On Windows PowerShell, the equivalent is:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath '<capture-file.png>'
```

The placeholder is an operator-supplied path. Put only the normalized path
relative to the manifest directory into `image`; do not copy the local absolute
path from the shell output.

Validate the metadata and all captured PNGs:

```sh
python3 tools/reference/manifest.py validate reference-captures/home/capture-manifest.json
```

Exit status `2` means matrix entries are unresolved and prints the remaining
rows. Exit status `1` means invalid evidence or metadata. Exit status `0` means
all required matrix rows have valid structural inputs and bytes; it is **not** a
parity pass or independent authenticity verdict. Fixing a manifest row changes
the manifest bytes. Reviewers must review the final manifest and referenced
images after edits.

## Annotation and comparison

For an actual reference/candidate pair, prepare the schema version 1 geometry
and color checks shown in the [review worksheets](worksheets.md). Bind the
reviewed checks to the exact values:

- reference capture row ID and its PNG SHA-256;
- candidate PNG SHA-256;
- candidate runtime geometry JSON SHA-256;
- approved mask file SHA-256, or JSON `null` when no mask file is supplied;
- reviewer identity, role `independent_reviewer`, and review record locator.

The candidate runtime geometry must identify the exact screen and capture row,
carry the SHA-256 of the rendered application build, list the measured
components, and be externally attested. The current comparator checks the
schema, source label, identities, rectangles, and SHA-256-shaped build identity;
it does not authenticate the attestor or independently observe the rendering.
Application self-reported geometry is not independent attestation.

If dynamic masks are required, use the mask worksheet schema. The mask file
binds itself to screen ID, capture ID, and reference capture SHA-256. Its exact
file SHA-256 is then copied into the `approved_masks_sha256` field of the
reviewed checks file. The same reviewer must document why each masked region is
dynamic and limited to that region. The comparator rejects a checked color
sample that falls inside a mask.

Run the comparison after manifest validation and independent review:

```sh
python3 tools/reference/visual_diff.py compare \
  --manifest reference-captures/home/capture-manifest.json \
  --capture-id 1920x1080-100:default \
  --candidate <candidate.png> \
  --checks <reviewed-checks.json> \
  --candidate-geometry <independently-attested-candidate-geometry.json> \
  --report <comparison-report.json> \
  --diff-output <failure-diff.png>
```

Add `--masks <approved-masks.json>` only when an independently reviewed mask
file is included; omit the flag when there are no masks. The required report and
diff paths must be distinct from inputs and from each other. No automatic image
resize occurs. A dimension mismatch and any normal gate failure return status
`2` and produce a failure diff; status `0` means the configured geometry, SSIM,
and color checks passed for this one selected capture. Status `1` means evidence
or inputs could not be evaluated. A single passing row does not complete
Ticket 02 or establish whole-product 1:1 parity.

## Portable evidence-pack review

Before handing off a pack, verify that it contains the original PNG bytes, the
manifest, observation worksheets, any applicability review records, comparison
annotation/checks, any approved masks, reviewer records, and the comparison
report/diff when generated. Keep all image references manifest-relative and
normalize separators to `/`. Recompute all hashes after transfer. A reviewer
must confirm hashes against the delivered files, review the exact evidence
version, and record any changed bytes as a new review.

Ticket 02 remains open until authentic reference captures and metadata cover the
required matrix, independent candidate geometry attestation and reviewed
annotations exist, and at least one production shell screen passes the
configured comparison gate with a failure diff available on failure.
