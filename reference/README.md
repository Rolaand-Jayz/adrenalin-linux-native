# Fixed Adrenalin reference capture

The only accepted golden-reference target for this project is AMD Software:
Adrenalin Edition **26.9.1 Optional, released 2026-09-03**. Captures from a
newer Adrenalin release, web pages, recreated screens, mockups, or generated
images are not golden evidence. Keep the supplied captures and their metadata
in version control only when redistribution rights allow it; otherwise retain
them in the authorized project evidence store and keep the manifest portable.

## Capture requirements

For each production screen, follow the reference-capture contract in the
engineering spec (ID-137 and ID-138): capture at 1920x1080/100%,
2560x1440/100%, 3840x2160 at Windows-recommended scale, the narrowest supported
window, the default window, and maximized state. Capture default, hover, focus,
open, changed, disabled/unavailable, confirmation, failure, and success states
where each state applies. A `not_applicable` decision requires a reason and a
review record bound to a captured image in the same manifest. Its
`applicability_evidence` contains the captured row's `capture_id` and exact
lowercase SHA-256, plus nonempty `reviewed_by` and `review_record` identifiers.
The referenced capture must use the same resolution/window context as the
`not_applicable` row; another state in that context is allowed. The validator
checks that the referenced row is `captured`, validates its PNG and checksum,
and compares the evidence digest with that verified image. A
review record is an auditable human assertion; validation cannot prove that a
review occurred or that the reviewer is independent. Unknown applicability
remains pending.

Record the Windows build, reference-machine GPU/CPU/display, observed labels and
control properties, enabled conditions, global/game scope, persistence,
unavailable/error behavior, hotkey, animation/transition, parity-ledger row,
and test identity. Preserve native physical pixel dimensions. Do not resize
goldens to make comparisons pass. Dynamic content can be masked only by a
separate reviewed and checked-in mask workflow.

## Portable manifest workflow

The development-only validator uses Python's standard library and is not a
shipping application dependency. Supply an output path when creating a plan;
there is no machine-specific default path:

```sh
: "${REFERENCE_MANIFEST:?Set REFERENCE_MANIFEST to the chosen manifest output path}"
python3 tools/reference/manifest.py init --screen-id home --output "$REFERENCE_MANIFEST"
python3 tools/reference/manifest.py validate "$REFERENCE_MANIFEST"
PYTHONDONTWRITEBYTECODE=1 python3 tools/reference/test_manifest.py
```

The initialized schema-version-2 JSON is an empty capture plan. It contains all six required
contexts and the nine state slots, each marked `pending`; it contains no
screenshots and cannot pass parity. Fill it only from actual captures. Captured
image paths are normalized paths relative to the manifest file. Absolute
paths, parent traversal, symlinks escaping the manifest directory, checksum
mismatches, dimension mismatches, malformed/truncated PNG chunks or image
streams, and unrecognized reference versions are rejected. A matrix of
`not_applicable` rows cannot validate without at least one real captured image
to which each review assertion is bound. No home directory,
mount point, or workstation-specific filesystem location is assumed.

`validate` checks provenance metadata and PNG bytes only. It does **not** render
the Linux candidate, compare pixels, validate masks, or report visual parity.
Use the comparator below only after the corpus, candidate render, geometry
evidence, and reviewed annotation inputs exist.

## Visual comparison harness

`tools/reference/visual_diff.py` is the standard-library-only parity comparator.
Every filesystem argument is explicit; the golden path is resolved relative
to the supplied manifest. The selected manifest must validate as a complete
capture matrix and the requested row must be a real `captured` entry. The tool
rejects mismatched physical dimensions and never resizes either image.

The pinned implementation is `adrenalin-reference-diff/1.0.0`:

- SSIM uses independent RGB channels, a uniform 11x11 local window, population
  variance/covariance, constants K1=0.01 and K2=0.03, and the mean of all
  unmasked valid windows; threshold is 0.995.
- Geometry compares reviewed reference rectangles with rectangles parsed from
  a separate Qt/QML runtime geometry export for the exact candidate build,
  fixture, screen, and state. Top-left displacement and width/height deviations
  are each limited to 2 physical pixels. Reference rectangles are manually
  annotated from the authentic capture; the tool does not infer UI components
  from pixels or accept candidate rectangles in the annotation file.
- Fixed-color samples compare the supplied reference/candidate pixel
  coordinates using IEC 61966-2-1 sRGB, D65, the 2-degree observer, CIELAB,
  and CIEDE2000. Threshold is ΔE00 <= 2. Inputs with embedded ICC/gamma/chroma
  or cICP transforms are rejected so the transform is not silently guessed.
  cICP is rejected even when an sRGB chunk is also present. An sRGB chunk is
  accepted only before PLTE and IDAT, as required by the PNG format. Unmarked
  RGB samples are interpreted as sRGB under this declared tool contract.
- Masks are optional. A supplied mask file must record an approved review,
  reviewer, date, approval reference, and must bind its `screen_id`, selected
  `capture_id`, and exact `reference_capture_sha256`. It also records a reason
  and rectangle for each mask. The checks coverage includes
  `approved_masks_sha256`: JSON `null` when no mask file is supplied, otherwise
  the exact SHA-256 of the supplied mask JSON. The report repeats the selected
  mask binding and hash. Review status and identity remain human assertions;
  code cannot prove that the review occurred or that files are tracked.
  SSIM windows touching any masked pixel are excluded. Color samples inside a
  mask are rejected. The review record is an auditable assertion; code cannot
  prove that it was reviewed or checked into version control.

The geometry/color check file has exactly these top-level fields. Its coverage
record binds the review to the precise reference capture, candidate PNG, and
runtime-geometry evidence hashes; a nonempty review identity is required.

```json
{
  "schema_version": 1,
  "coverage": {
    "status": "reviewed_complete",
    "reviewer": "independent reviewer identity",
    "reviewer_role": "independent_reviewer",
    "review_record": "review artifact or decision identity",
    "reference_capture_id": "capture id from the supplied manifest",
    "reference_capture_sha256": "sha256 from the selected golden row",
    "candidate_png_sha256": "sha256 of the exact candidate PNG",
    "candidate_geometry_sha256": "sha256 of the runtime geometry export",
    "approved_masks_sha256": null
  },
  "components": [
    {"id": "stable-component-id", "reference": {"x": 0, "y": 0, "width": 10, "height": 10}}
  ],
  "color_samples": [
    {"id": "fixed-token-id", "reference_xy": [5, 5], "candidate_xy": [5, 5]}
  ]
}
```

Replace the illustrative values only with actual review and capture evidence.
When a reviewed mask file is supplied, replace the `null` with that file's
exact lowercase SHA-256 and bind the mask file itself to the selected screen,
capture id, and reference image hash.
The independent reviewer attests that the reference annotations and fixed
color samples cover every applicable visible component/color and match the
specified golden capture. The `candidate_geometry_sha256` must match a separate
runtime export with this shape:

```json
{
  "schema_version": 1,
  "source": "externally_attested_qt_qml_runtime_geometry_export",
  "build_identity": "lowercase sha256 of rendered application build",
  "fixture_id": "deterministic UI fixture identity",
  "screen_id": "manifest screen id",
  "capture_id": "selected manifest capture id",
  "components": [
    {"id": "stable-component-id", "rect": {"x": 0, "y": 0, "width": 10, "height": 10}}
  ]
}
```

That export must be generated externally by a Qt/QML runtime geometry probe
which queries the rendered app for the same fixture/screen/state. This repo
does not yet contain that probe, and `visual_diff.py` does not discover or
generate candidate geometry. Coordinates entered by hand are not runtime
measurements. The harness requires the external export, checks its declared
source identity, matching component IDs, capture/screen IDs, build/fixture
identities, and exact SHA-256 bound by independent review. The review record and
provenance are reported, but code cannot cryptographically prove an operator's
attestation is truthful. Both arrays must be nonempty, IDs unique, and
coordinates within the matched physical screenshot. Input and output paths are
selected by the caller:

```sh
python3 tools/reference/visual_diff.py compare \
  --manifest "$REFERENCE_MANIFEST" \
  --capture-id "$REFERENCE_CAPTURE_ID" \
  --candidate "$CANDIDATE_PNG" \
  --checks "$VISUAL_CHECKS" \
  --candidate-geometry "$EXTERNALLY_GENERATED_RUNTIME_GEOMETRY_JSON" \
  --report "$VISUAL_REPORT" \
  --diff-output "$VISUAL_DIFF_PNG"
```

Add `--masks "$APPROVED_MASK_FILE"` only when the referenced mask file has
passed review and is checked in. A computed threshold failure exits 2, writes a
JSON report and a reviewable PNG difference artifact (masked pixels are shown
blue). Invalid/incomplete inputs exit 1 and cannot produce a passing result.
The comparator supports opaque, non-interlaced 8-bit RGB/RGBA PNGs; it rejects
translucent pixels and PNG color profiles that conflict with its pinned sRGB
transform. Broader image formats require a deliberate implementation revision.

The deterministic synthetic fixtures in `tools/reference/test_manifest.py`
test metric arithmetic, input provenance validation, PNG handling, and one
manifest-backed **failure-only** comparison that asserts the failure report and
diff artifact. They are not golden images, no test accepts a synthetic parity
pass, and they cannot establish product parity. No authentic Windows
26.9.1 capture or candidate UI screenshot/runtime geometry export is currently
available, so this harness has not been run against product evidence and cannot
produce a real parity pass until authentic captures, rendered-candidate
screenshots, externally generated runtime geometry, and independent review
artifacts are supplied.

## Current capture status

No authentic 26.9.1 Optional capture or capture metadata was present in the
project at the time this tooling was added. No golden pixels or passing parity
fixtures are checked in. Phase-0 capture on the specified Windows reference is
required before exact shell labels, geometry, behavior, fixtures, or parity
results can be established.

## Candidate runtime capture

The native shell can capture its current rendered QML window and report measured
rectangles for named visible components. The caller supplies all output paths
and capture identities; the command writes a PNG and schema-version-1 geometry
JSON. The PNG contains the actual `QQuickWindow` render, and the rectangles are
measured from the live Qt Quick item tree and converted to the PNG's physical
pixel coordinates.

```sh
: "${SHELL_EXECUTABLE:?Set SHELL_EXECUTABLE to the built native shell}"
: "${CANDIDATE_PNG:?Set CANDIDATE_PNG to the chosen PNG output path}"
: "${CANDIDATE_GEOMETRY:?Set CANDIDATE_GEOMETRY to the chosen geometry JSON output path}"
"$SHELL_EXECUTABLE" \
  --capture-candidate "$CANDIDATE_PNG" \
  --candidate-geometry "$CANDIDATE_GEOMETRY" \
  --screen-id home \
  --capture-id default-window:default \
  --fixture-id live-runtime-uncontrolled
```

`--capture-size WIDTHxHEIGHT` is capture-only and accepts logical sizes from
960x640 through 3840x2160, with the request and resulting display-scaled render
bounded by the 8,294,400-pixel ID-137 envelope. Requests that would exceed the
physical envelope at the current display scale fail before resizing the window.
The output reports the actual physical PNG size. Do not use a caller-supplied
fixture identity unless the displayed data/state is actually controlled by that
fixture. The current shell export identifies its geometry as
application-self-reported. The parity comparator requires independent
runtime-geometry attestation and intentionally rejects this self-report, so this
capture is useful candidate evidence but cannot pass the geometry gate. The
reference manifest and comparator schemas remain unchanged.

## Live accessibility-tree check

`tools/reference/test_accessibility.py` launches the built shell and inspects its
live Qt Quick accessibility tree from a separate AT-SPI client. It checks
accessible names, roles, and that named component extents remain inside the
window frame. It requires PyGObject's AT-SPI typelib, the AT-SPI registry,
D-Bus, and Xvfb; these are development-test dependencies, not application
runtime dependencies. Select the executable paths from the local build and
accessibility installation:

```sh
: "${SHELL_EXECUTABLE:?Set SHELL_EXECUTABLE to the built native shell}"
: "${AT_SPI_REGISTRY:?Set AT_SPI_REGISTRY to the installed AT-SPI registry daemon}"

dbus-run-session -- xvfb-run -a python3 tools/reference/test_accessibility.py \
  --shell "$SHELL_EXECUTABLE" \
  --registry "$AT_SPI_REGISTRY"
```

The check verifies the live screen-reader semantics and externally observed
logical-screen extents. It does not emit candidate geometry evidence or attest
physical-pixel mapping for window-manager decorations or Wayland compositors.
It therefore does not satisfy the independent candidate-geometry or parity
acceptance gate.
