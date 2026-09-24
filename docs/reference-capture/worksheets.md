# Reference capture and parity review worksheets

These are blank operator worksheets, not evidence or example capture data. Use
one environment sheet per capture host, one row in the state log for every
manifest entry, and one independent review sheet per review decision. Copy
observed facts into the manifest and retain the completed worksheets alongside
the evidence pack. Leave unknowns as `REFERENCE_REQUIRED`; do not fill gaps from
memory.

## A. Fixed reference and capture host

| Field | Operator-entered value | Evidence source / notes |
| --- | --- | --- |
| Product | AMD Software: Adrenalin Edition | Fixed by contract |
| Version / edition | 26.9.1 Optional | Fixed by contract |
| Release date | 2026-09-03 | Fixed by contract |
| Capture host identifier |  |  |
| Windows edition, version, build |  |  |
| Adrenalin application build shown by the installed reference |  |  |
| GPU model and active driver version |  |  |
| CPU model |  |  |
| Display model(s) |  |  |
| Display resolution(s), active mode(s), orientation |  |  |
| Windows scale percentage per display |  |  |
| Display containing the application |  |  |
| Date/time and time zone of collection |  |  |
| Operator identity |  |  |
| Reference installation/source evidence locator |  |  |
| Environment review record locator |  |  |

The manifest maps the first host fields to `capture_source.host_id`,
`windows_version`, `gpu`, `cpu`, and `display`. Keep fuller build/monitor notes
in this worksheet and the retained review record. Do not put confidential
credentials, account identifiers, or unrelated personal data in the capture
pack.

## B. Context log

Complete a row for each context. For resolution contexts, confirm active
physical resolution and Windows scale in Display Settings. For the recommended
4K scale, preserve evidence showing Windows' recommended value. For window
contexts, record the physical PNG dimensions and the window state; do not
estimate dimensions from logical units.

| Context ID | Observed display / app state | Physical width × height | Windows scale % | How measured / supporting evidence | Ready? |
| --- | --- | ---: | ---: | --- | --- |
| `1920x1080-100` |  |  |  |  |  |
| `2560x1440-100` |  |  |  |  |  |
| `3840x2160-recommended` |  |  |  |  |  |
| `narrowest-supported` |  |  |  |  |  |
| `default-window` |  |  |  |  |  |
| `maximized` |  |  |  |  |  |

## C. Capture/state log

Add a row per screen, context, and state. `Capture ID` must match the generated
manifest ID exactly, for example the ID pattern is `<context-id>:<state>`.
Applicable states include `default`, `hover`, `focus`, `open`, `changed`,
`disabled`, `confirmation`, `error`, and `success`.

| Screen ID | Capture ID | Applicable? | If inapplicable, concrete reason | Trigger/action used | PNG relative path | SHA-256 of original PNG | Pixel width × height | Collection timestamp | Observation / review record locator |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |  |  |  |

Use a fresh, complete screenshot for each relevant state. A state is
`not_applicable` only after review confirms that the screen has no such behavior
in this context. The manifest requires a second captured image from the same
context for the `applicability_evidence` binding; record that image's exact ID
and SHA-256, the reviewer, and the review record locator. The validator checks
the image binding and same-context rule, not the truth of the applicability
reason or the contents of the review record.

## D. Observed behavior record

Complete one copy per captured state row. Add additional rows as needed for
multiple controls. Describe the observation and its method; avoid conclusions
that cannot be checked from the recorded source.

| Observation | Operator-entered value / evidence notes |
| --- | --- |
| `visible_labels` — exact visible labels and language; truncation/wrapping |  |
| `control_properties` — control type, min, max, default, step where applicable |  |
| `enabled_disabled_conditions` — condition and resulting availability |  |
| `global_vs_game_behavior` — scope and how it was established |  |
| `restart_persistence` — restart type, before/after state, result |  |
| `unavailable_error_behavior` — exact message/state and reproducible trigger |  |
| `hotkey` — exact shortcut or where/how absence was checked |  |
| `animation_transition` — visible transition/animation, timing, or where/how absence was checked |  |
| `parity_ledger_identity` — stable product screen/feature ledger key |  |
| `test_identity` — stable test/acceptance case key |  |

Enter corresponding values under these exact keys in the capture manifest's
`observations` object. A required field may contain `REFERENCE_REQUIRED` while
work is incomplete, but that placeholder is an explicit unresolved requirement.

## E. Independent applicability / evidence review

| Field | Reviewer-entered value |
| --- | --- |
| Review record stable identifier / relative locator |  |
| Reviewer identity |  |
| Reviewer role / independence basis |  |
| Review timestamp and time zone |  |
| Screen ID and capture ID reviewed |  |
| Reference version/build reviewed |  |
| Image file relative path |  |
| Exact image SHA-256 reviewed |  |
| Context ID reviewed |  |
| Decision and rationale |  |
| Applicability conclusion, if applicable |  |
| Evidence sources examined |  |
| Reviewer sign-off / record location |  |

For an inapplicable manifest row, set `applicability_evidence` to the exact
object below, substituting the ID and digest of a `captured` image in the same
context. `reviewed_by` and `review_record` are plain strings. The manifest
validator does not hash or dereference the review record.

```json
{
  "capture_id": "<captured-row-id-in-the-same-context>",
  "sha256": "<64-lowercase-hex-digest-of-that-captured-image>",
  "reviewed_by": "<independent-reviewer-identity>",
  "review_record": "<stable-review-record-id-or-relative-locator>"
}
```

## F. Geometry, fixed-color, and mask review worksheets

The comparator consumes an exact schema. The following are schema templates
only; replace every angle-bracket value and complete the measurements from the
real reference and candidate artifacts. Never run a review using these
placeholders.

### `checks.json` schema template

Bind this file to the exact bytes reviewed. Hash fields are lowercase SHA-256
digests. `approved_masks_sha256` must be JSON `null` if no mask file is used;
otherwise it must equal the SHA-256 of the exact approved-mask JSON file.

```json
{
  "schema_version": 1,
  "coverage": {
    "status": "reviewed_complete",
    "reviewer": "<independent-reviewer-identity>",
    "reviewer_role": "independent_reviewer",
    "review_record": "<stable-review-record-id-or-relative-locator>",
    "reference_capture_id": "<manifest-capture-id>",
    "reference_capture_sha256": "<lowercase-sha256-of-reference-png>",
    "candidate_png_sha256": "<lowercase-sha256-of-candidate-png>",
    "candidate_geometry_sha256": "<lowercase-sha256-of-candidate-geometry-json>",
    "approved_masks_sha256": null
  },
  "components": [
    {
      "id": "<stable-component-id>",
      "reference": {"x": 0, "y": 0, "width": 1, "height": 1}
    }
  ],
  "color_samples": [
    {
      "id": "<stable-color-sample-id>",
      "reference_xy": [0, 0],
      "candidate_xy": [0, 0]
    }
  ]
}
```

The numeric rectangle and pixel coordinates above are schema placeholders, not
reference measurements. Replace them with independently reviewed values that
fall inside the corresponding images. Component IDs must exactly match the
candidate geometry component IDs. Every component rectangle uses physical PNG
pixels and the exact `{x, y, width, height}` keys. Include the complete reviewed
component and fixed-color coverage required for the selected screen.

### `approved-masks.json` schema template

Use only for reviewed dynamic regions with a specific, reproducible rationale.
The file hash is computed after the final review fields and mask rectangles are
written. Copy that digest to `checks.json` afterward, then review both files as
one bound evidence set.

```json
{
  "schema_version": 1,
  "review_status": "approved",
  "reviewed_by": "<independent-reviewer-identity>",
  "reviewed_at": "<RFC3339-timestamp>",
  "approval_reference": "<stable-review-record-id-or-relative-locator>",
  "screen_id": "<manifest-screen-id>",
  "capture_id": "<manifest-capture-id>",
  "reference_capture_sha256": "<lowercase-sha256-of-reference-png>",
  "masks": [
    {
      "id": "<stable-mask-id>",
      "reason": "<why-this-specific-region-is-dynamic>",
      "rect": {"x": 0, "y": 0, "width": 1, "height": 1}
    }
  ]
}
```

Rectangles use physical pixels, must lie within the reference image, and must
not cover every SSIM window or any chosen fixed-color sample. Empty masks are
permitted by the parser but should be omitted as an input instead; a mask should
exist only when there is a real reviewed dynamic region to exclude.

## G. Final evidence handoff

| Item | Final value / location | Digest or review evidence checked? |
| --- | --- | --- |
| Capture manifest |  |  |
| Original capture PNGs |  |  |
| Completed host/context/state worksheets |  |  |
| Applicability review records |  |  |
| Reference annotations and color checks |  |  |
| Candidate PNG and candidate build identity |  |  |
| Independently attested candidate geometry |  |  |
| Approved masks, if any |  |  |
| Comparison report |  |  |
| Failure diff, when gate fails |  |  |
| Independent reviewer and review record |  |  |
| Remaining `REFERENCE_REQUIRED` items |  |  |

Preserve this handoff with the exact version of the evidence pack the reviewer
examined. Any changed PNG, manifest, checks, geometry, or masks invalidates the
corresponding prior byte bindings; recompute digests and repeat review.
