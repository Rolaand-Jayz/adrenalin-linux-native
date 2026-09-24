"""Create and validate portable Adrenalin reference-capture manifests.

This is developer tooling only. It records capture provenance and validates
inputs; it deliberately does not decide visual parity or manufacture golden
images.
"""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import re
import struct
import sys
import zlib
from pathlib import Path, PurePosixPath
from typing import Any


REFERENCE = {
    "product": "AMD Software: Adrenalin Edition",
    "version": "26.9.1 Optional",
    "release_date": "2026-09-03",
}
MANIFEST_VERSION = 2
STATES = ("default", "hover", "focus", "open", "changed", "disabled", "confirmation", "error", "success")
CONTEXTS = (
    {"id": "1920x1080-100", "kind": "resolution", "width": 1920, "height": 1080, "scale_percent": 100},
    {"id": "2560x1440-100", "kind": "resolution", "width": 2560, "height": 1440, "scale_percent": 100},
    {"id": "3840x2160-recommended", "kind": "resolution", "width": 3840, "height": 2160, "scale_percent": None},
    {"id": "narrowest-supported", "kind": "window", "width": None, "height": None, "scale_percent": None},
    {"id": "default-window", "kind": "window", "width": None, "height": None, "scale_percent": None},
    {"id": "maximized", "kind": "window", "width": None, "height": None, "scale_percent": None},
)
PENDING_STATES = {"pending", "captured", "not_applicable"}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
MAX_PNG_FILE_BYTES = 512 * 1024 * 1024
MAX_DECODED_PNG_BYTES = 256 * 1024 * 1024
OBSERVATION_FIELDS = (
    "visible_labels",
    "control_properties",
    "enabled_disabled_conditions",
    "global_vs_game_behavior",
    "restart_persistence",
    "unavailable_error_behavior",
    "hotkey",
    "animation_transition",
    "parity_ledger_identity",
    "test_identity",
)


class ManifestError(ValueError):
    """Invalid or incomplete reference-capture manifest."""


def initial_manifest(screen_id: str) -> dict[str, Any]:
    if not re.fullmatch(r"[a-z0-9][a-z0-9._-]*", screen_id):
        raise ManifestError("screen id must use lowercase letters, digits, dot, underscore, or hyphen")
    return {
        "manifest_version": MANIFEST_VERSION,
        "reference": dict(REFERENCE),
        "screen_id": screen_id,
        "capture_source": {"host_id": None, "windows_version": None, "gpu": None, "cpu": None, "display": None},
        "contexts": [dict(context) for context in CONTEXTS],
        "captures": [
            {
                "id": f"{context['id']}:{state}",
                "context_id": context["id"],
                "state": state,
                "status": "pending",
                "applicability_reason": None,
                "applicability_evidence": None,
                "image": None,
                "sha256": None,
                "physical_width": None,
                "physical_height": None,
                "observations": None,
            }
            for context in CONTEXTS
            for state in STATES
        ],
        "parity": {
            "status": "not_run",
            "metric_implementation": None,
            "metric_parameters": None,
            "approved_masks": [],
        },
    }


def _safe_image_path(manifest_path: Path, relative: Any) -> Path:
    if not isinstance(relative, str) or not relative:
        raise ManifestError("captured entry must provide a project-relative image path")
    posix = PurePosixPath(relative)
    if (
        posix.is_absolute()
        or ".." in posix.parts
        or "\\" in relative
        or re.match(r"^[A-Za-z]:", relative) is not None
        or any(ord(character) < 32 or ord(character) == 127 for character in relative)
    ):
        raise ManifestError(f"image path must be a normalized project-relative path: {relative!r}")
    root = manifest_path.resolve().parent
    image = (root / Path(*posix.parts)).resolve(strict=True)
    try:
        image.relative_to(root)
    except ValueError as exc:
        raise ManifestError("image path resolves outside the manifest directory") from exc
    if not image.is_file():
        raise ManifestError(f"capture image is not a regular file: {relative}")
    return image


def _png_dimensions(image: Path) -> tuple[int, int]:
    """Validate PNG framing, chunk CRCs, zlib stream, scanline layout and filters."""
    try:
        size = image.stat().st_size
        if size > MAX_PNG_FILE_BYTES:
            raise ManifestError(f"capture PNG exceeds the {MAX_PNG_FILE_BYTES}-byte validation limit")
        with image.open("rb") as stream:
            if stream.read(8) != b"\x89PNG\r\n\x1a\n":
                raise ManifestError(f"capture must have a PNG signature: {image.name}")
            position = 8
            width = height = bit_depth = color_type = interlace = None
            saw_ihdr = saw_plte = saw_trns = saw_idat = idat_ended = saw_iend = False
            saw_srgb = False
            palette_entries = 0
            compressed = bytearray()
            while position < size:
                raw_length = stream.read(4)
                if len(raw_length) != 4:
                    raise ManifestError(f"truncated PNG chunk length: {image.name}")
                chunk_length = struct.unpack(">I", raw_length)[0]
                chunk_type = stream.read(4)
                if (
                    len(chunk_type) != 4
                    or not all(65 <= byte <= 90 or 97 <= byte <= 122 for byte in chunk_type)
                    or (len(chunk_type) == 4 and 97 <= chunk_type[2] <= 122)
                ):
                    raise ManifestError(f"invalid PNG chunk type: {image.name}")
                position += 8
                if chunk_length > size - position - 4:
                    raise ManifestError(f"truncated PNG chunk data: {image.name}")
                chunk_data = stream.read(chunk_length)
                raw_crc = stream.read(4)
                if len(chunk_data) != chunk_length or len(raw_crc) != 4:
                    raise ManifestError(f"truncated PNG chunk: {image.name}")
                expected_crc = struct.unpack(">I", raw_crc)[0]
                actual_crc = binascii.crc32(chunk_type + chunk_data) & 0xFFFFFFFF
                if expected_crc != actual_crc:
                    raise ManifestError(f"PNG chunk checksum mismatch: {image.name}")
                position += chunk_length + 4

                if not saw_ihdr:
                    if chunk_type != b"IHDR" or chunk_length != 13:
                        raise ManifestError(f"PNG must begin with a 13-byte IHDR: {image.name}")
                    width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk_data)
                    valid_depths = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}
                    if width == 0 or height == 0 or width > 32768 or height > 32768:
                        raise ManifestError(f"invalid PNG dimensions: {image.name}")
                    if width * height > 268_435_456:
                        raise ManifestError(f"PNG pixel count exceeds the validation limit: {image.name}")
                    if color_type not in valid_depths or bit_depth not in valid_depths[color_type]:
                        raise ManifestError(f"invalid PNG color type/bit depth: {image.name}")
                    if compression != 0 or filtering != 0 or interlace not in (0, 1):
                        raise ManifestError(f"unsupported or invalid PNG method: {image.name}")
                    saw_ihdr = True
                    continue

                if chunk_type == b"IHDR":
                    raise ManifestError(f"duplicate PNG IHDR: {image.name}")
                if chunk_type == b"PLTE":
                    if saw_plte or saw_idat or chunk_length == 0 or chunk_length % 3 or chunk_length > 768:
                        raise ManifestError(f"invalid PNG palette: {image.name}")
                    palette_entries = chunk_length // 3
                    if color_type in (0, 4):
                        raise ManifestError(f"PNG grayscale image cannot contain a palette: {image.name}")
                    if color_type == 3 and palette_entries > (1 << bit_depth):
                        raise ManifestError(f"PNG palette exceeds indexed bit depth: {image.name}")
                    saw_plte = True
                elif chunk_type == b"sRGB":
                    if saw_srgb or saw_plte or saw_idat or chunk_length != 1 or chunk_data[0] > 3:
                        raise ManifestError(f"invalid PNG sRGB chunk or ordering: {image.name}")
                    saw_srgb = True
                elif chunk_type == b"cICP":
                    raise ManifestError(f"unsupported PNG cICP chunk for the pinned sRGB comparison contract: {image.name}")
                elif chunk_type == b"tRNS":
                    if saw_trns or saw_idat:
                        raise ManifestError(f"PNG tRNS must occur once before image data: {image.name}")
                    if color_type == 0:
                        if chunk_length != 2 or struct.unpack(">H", chunk_data)[0] >= (1 << bit_depth):
                            raise ManifestError(f"invalid grayscale PNG tRNS chunk: {image.name}")
                    elif color_type == 2:
                        if chunk_length != 6 or any(
                            sample >= (1 << bit_depth)
                            for sample in struct.unpack(">HHH", chunk_data)
                        ):
                            raise ManifestError(f"invalid truecolor PNG tRNS chunk: {image.name}")
                    elif color_type == 3:
                        if not saw_plte or chunk_length == 0 or chunk_length > palette_entries:
                            raise ManifestError(f"invalid indexed PNG tRNS chunk: {image.name}")
                    else:
                        raise ManifestError(f"PNG tRNS is forbidden for alpha color types: {image.name}")
                    saw_trns = True
                elif chunk_type == b"IDAT":
                    if idat_ended:
                        raise ManifestError(f"PNG IDAT chunks must be consecutive: {image.name}")
                    if color_type == 3 and not saw_plte:
                        raise ManifestError(f"indexed PNG is missing its palette: {image.name}")
                    saw_idat = True
                    compressed.extend(chunk_data)
                elif chunk_type == b"IEND":
                    if chunk_length != 0 or not saw_idat:
                        raise ManifestError(f"invalid PNG IEND or missing image data: {image.name}")
                    saw_iend = True
                    if position != size:
                        raise ManifestError(f"trailing bytes after PNG IEND: {image.name}")
                    break
                else:
                    if saw_idat:
                        idat_ended = True
                    # Unknown critical chunks are not safely interpretable here.
                    if 65 <= chunk_type[0] <= 90 and chunk_type not in (b"PLTE",):
                        raise ManifestError(f"unknown critical PNG chunk: {image.name}")
                    if chunk_type == b"PLTE" and not saw_plte:
                        raise ManifestError(f"invalid PNG palette: {image.name}")

            if not saw_iend or width is None or height is None:
                raise ManifestError(f"truncated PNG without a complete IEND: {image.name}")
            if color_type == 3 and not saw_plte:
                raise ManifestError(f"indexed PNG is missing its palette: {image.name}")

            channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
            bits_per_pixel = channels * bit_depth
            passes = (
                ((0, 0, 1, 1),)
                if interlace == 0
                else ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))
            )
            scanlines: list[tuple[int, int]] = []
            decoded_length = 0
            for start_x, start_y, step_x, step_y in passes:
                pass_width = max(0, (width - start_x + step_x - 1) // step_x)
                pass_height = max(0, (height - start_y + step_y - 1) // step_y)
                if pass_width and pass_height:
                    row_bytes = (pass_width * bits_per_pixel + 7) // 8
                    decoded_length += pass_height * (row_bytes + 1)
                    scanlines.append((pass_height, row_bytes))
            if decoded_length > MAX_DECODED_PNG_BYTES:
                raise ManifestError(f"PNG decoded data exceeds the {MAX_DECODED_PNG_BYTES}-byte validation limit")
            decompressor = zlib.decompressobj()
            try:
                decoded = decompressor.decompress(bytes(compressed), decoded_length + 1)
            except zlib.error as exc:
                raise ManifestError(f"invalid PNG image-data stream: {image.name}") from exc
            if (
                len(decoded) != decoded_length
                or not decompressor.eof
                or decompressor.unused_data
                or decompressor.unconsumed_tail
            ):
                raise ManifestError(f"invalid or truncated PNG image-data stream: {image.name}")
            offset = 0
            for rows, row_bytes in scanlines:
                for _ in range(rows):
                    filter_type = decoded[offset]
                    if filter_type > 4:
                        raise ManifestError(f"invalid PNG scanline filter: {image.name}")
                    offset += row_bytes + 1
            if offset != decoded_length:
                raise ManifestError(f"invalid PNG scanline layout: {image.name}")
            return width, height
    except (OSError, struct.error, zlib.error, OverflowError) as exc:
        if isinstance(exc, ManifestError):
            raise
        raise ManifestError(f"cannot validate PNG {image.name}: {exc}") from exc


def _sha256_file(image: Path) -> str:
    digest = hashlib.sha256()
    with image.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate(manifest_path: Path) -> tuple[int, list[str]]:
    """Validate capture metadata and bytes; return count and pending diagnostics."""
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ManifestError(f"cannot read manifest: {exc}") from exc
    if not isinstance(manifest, dict):
        raise ManifestError("manifest root must be an object")
    if isinstance(manifest.get("manifest_version"), bool) or manifest.get("manifest_version") != MANIFEST_VERSION:
        raise ManifestError(f"manifest_version must be {MANIFEST_VERSION}")
    if manifest.get("reference") != REFERENCE:
        raise ManifestError("reference identity must exactly match AMD Software: Adrenalin Edition 26.9.1 Optional (2026-09-03)")
    screen_id = manifest.get("screen_id")
    if not isinstance(screen_id, str) or re.fullmatch(r"[a-z0-9][a-z0-9._-]*", screen_id) is None:
        raise ManifestError("screen_id must use lowercase letters, digits, dot, underscore, or hyphen")

    contexts = manifest.get("contexts")
    if not isinstance(contexts, list) or len(contexts) != len(CONTEXTS):
        raise ManifestError("contexts must define the required six resolution/window contexts")
    context_by_id = {}
    for context in contexts:
        if not isinstance(context, dict) or context.get("id") not in {item["id"] for item in CONTEXTS}:
            raise ManifestError("contexts contains an unknown or malformed context")
        if context["id"] in context_by_id:
            raise ManifestError(f"duplicate context: {context['id']}")
        expected = next(item for item in CONTEXTS if item["id"] == context["id"])
        if context.get("kind") != expected["kind"]:
            raise ManifestError(f"{context['id']}: context kind cannot be changed")
        for field in ("width", "height", "scale_percent"):
            value = context.get(field)
            if value is not None and (not isinstance(value, int) or isinstance(value, bool) or value <= 0):
                raise ManifestError(f"{context['id']}: {field} must be a positive integer when recorded")
        if expected["width"] is not None and (context.get("width"), context.get("height")) != (expected["width"], expected["height"]):
            raise ManifestError(f"{context['id']}: required physical resolution cannot be changed")
        if expected["scale_percent"] is not None and context.get("scale_percent") != expected["scale_percent"]:
            raise ManifestError(f"{context['id']}: required display scale cannot be changed")
        context_by_id[context["id"]] = context
    rows = manifest.get("captures")
    if not isinstance(rows, list):
        raise ManifestError("captures must be an array")
    expected_ids = {f"{context['id']}:{state}" for context in CONTEXTS for state in STATES}
    by_id: dict[str, dict[str, Any]] = {}
    for row in rows:
        if not isinstance(row, dict) or not isinstance(row.get("id"), str):
            raise ManifestError("each capture row needs an id")
        if row["id"] in by_id:
            raise ManifestError(f"duplicate capture id: {row['id']}")
        by_id[row["id"]] = row
    if set(by_id) != expected_ids:
        missing = sorted(expected_ids - set(by_id))
        extra = sorted(set(by_id) - expected_ids)
        raise ManifestError(f"capture matrix mismatch; missing={missing}, extra={extra}")

    errors: list[str] = []
    captured_count = 0
    captured_digests: dict[str, str] = {}
    applicability_reviews: list[tuple[str, dict[str, Any]]] = []
    for capture_id in sorted(expected_ids):
        row = by_id[capture_id]
        context_id, state = capture_id.split(":", 1)
        if row.get("context_id") != context_id or row.get("state") != state:
            raise ManifestError(f"{capture_id}: context_id/state do not match the matrix identity")
        status = row.get("status")
        if status not in PENDING_STATES:
            raise ManifestError(f"{capture_id}: status must be pending, captured, or not_applicable")
        if status == "pending":
            if row.get("applicability_evidence") is not None:
                raise ManifestError(f"{capture_id}: pending entries cannot contain applicability evidence")
            errors.append(f"{capture_id}: capture and reference applicability are unresolved")
            continue
        if status == "not_applicable":
            reason = row.get("applicability_reason")
            if not isinstance(reason, str) or not reason.strip():
                raise ManifestError(f"{capture_id}: not_applicable requires a reason")
            if any(row.get(key) is not None for key in ("image", "sha256", "physical_width", "physical_height")):
                raise ManifestError(f"{capture_id}: not_applicable rows cannot carry image metadata")
            evidence = row.get("applicability_evidence")
            evidence_keys = {"capture_id", "sha256", "reviewed_by", "review_record"}
            if not isinstance(evidence, dict) or set(evidence) != evidence_keys:
                raise ManifestError(
                    f"{capture_id}: not_applicable requires applicability_evidence with capture_id, sha256, reviewed_by, and review_record"
                )
            for field in ("capture_id", "reviewed_by", "review_record"):
                if not isinstance(evidence[field], str) or not evidence[field].strip():
                    raise ManifestError(f"{capture_id}: applicability_evidence.{field} must be recorded")
            if not isinstance(evidence["sha256"], str) or not SHA256_RE.fullmatch(evidence["sha256"]):
                raise ManifestError(f"{capture_id}: applicability_evidence.sha256 must be a lowercase SHA-256 digest")
            applicability_reviews.append((capture_id, evidence))
            continue

        if row.get("applicability_evidence") is not None:
            raise ManifestError(f"{capture_id}: captured entries cannot contain applicability evidence")

        observations = row.get("observations")
        if not isinstance(observations, dict):
            raise ManifestError(f"{capture_id}: captured entry requires observed environment/behavior metadata")
        for field in OBSERVATION_FIELDS:
            value = observations.get(field)
            if value is None or value == "" or value == [] or value == {}:
                raise ManifestError(f"{capture_id}: observations.{field} must be recorded or set to REFERENCE_REQUIRED")
        digest = row.get("sha256")
        if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
            raise ManifestError(f"{capture_id}: sha256 must be a lowercase SHA-256 digest")
        image = _safe_image_path(manifest_path, row.get("image"))
        actual_digest = _sha256_file(image)
        if actual_digest != digest:
            raise ManifestError(f"{capture_id}: image checksum mismatch")
        captured_digests[capture_id] = actual_digest
        width, height = _png_dimensions(image)
        declared_width, declared_height = row.get("physical_width"), row.get("physical_height")
        if (
            not isinstance(declared_width, int)
            or isinstance(declared_width, bool)
            or not isinstance(declared_height, int)
            or isinstance(declared_height, bool)
            or declared_width != width
            or declared_height != height
        ):
            raise ManifestError(f"{capture_id}: declared physical dimensions do not match PNG pixels")
        context = context_by_id[row["context_id"]]
        if context.get("width") is not None and width != context["width"]:
            raise ManifestError(f"{capture_id}: screenshot width does not match recorded physical context")
        if context.get("height") is not None and height != context["height"]:
            raise ManifestError(f"{capture_id}: screenshot height does not match recorded physical context")
        if context["kind"] == "resolution" and (width != context["width"] or height != context["height"]):
            raise ManifestError(f"{capture_id}: screenshot dimensions must equal physical reference resolution")
        captured_count += 1

    for capture_id, evidence in applicability_reviews:
        evidence_capture_id = evidence["capture_id"]
        if evidence_capture_id not in by_id or by_id[evidence_capture_id].get("status") != "captured":
            raise ManifestError(
                f"{capture_id}: applicability evidence must reference a captured image in this manifest"
            )
        if by_id[evidence_capture_id].get("context_id") != by_id[capture_id].get("context_id"):
            raise ManifestError(
                f"{capture_id}: applicability evidence must reference a captured image from the same context"
            )
        if captured_digests.get(evidence_capture_id) != evidence["sha256"]:
            raise ManifestError(
                f"{capture_id}: applicability evidence SHA-256 does not match the referenced captured image"
            )

    if not errors:
        for context in contexts:
            for field in ("width", "height", "scale_percent"):
                if context.get(field) is None:
                    raise ManifestError(f"{context['id']}: captured matrix requires recorded {field}")
        _validate_source(manifest.get("capture_source"))
    parity = manifest.get("parity")
    expected_parity_keys = {"status", "metric_implementation", "metric_parameters", "approved_masks"}
    if not isinstance(parity, dict) or set(parity) != expected_parity_keys or parity.get("status") != "not_run":
        raise ManifestError("pixel parity status cannot be set by this capture-manifest validator")
    if parity.get("metric_implementation") is not None or parity.get("metric_parameters") is not None:
        raise ManifestError("a capture manifest cannot claim a visual-diff implementation or result")
    if parity.get("approved_masks") != []:
        raise ManifestError("mask approval requires the independent reviewed visual-regression workflow")
    return captured_count, errors


def _validate_source(source: Any) -> None:
    if not isinstance(source, dict):
        raise ManifestError("capture_source metadata is required")
    for field in ("host_id", "windows_version", "gpu", "cpu", "display"):
        if not isinstance(source.get(field), str) or not source[field].strip():
            raise ManifestError(f"capture_source.{field} is required before capture matrix can validate")


def _write_initial(output: Path, screen_id: str) -> None:
    if output.exists():
        raise ManifestError(f"refusing to overwrite existing manifest: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(initial_manifest(screen_id), indent=2) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    create = subparsers.add_parser("init", help="write an explicit pending capture matrix")
    create.add_argument("--output", type=Path, required=True, help="manifest output path (no implicit default)")
    create.add_argument("--screen-id", required=True, help="stable identity for the captured product screen")
    check = subparsers.add_parser("validate", help="validate metadata and referenced capture bytes")
    check.add_argument("manifest", type=Path, help="manifest path; image paths resolve relative to it")
    args = parser.parse_args(argv)
    try:
        if args.command == "init":
            _write_initial(args.output, args.screen_id)
            print(f"Created pending 26.9.1 capture matrix: {args.output}")
            print("No reference captures or parity result were created.")
            return 0
        count, pending = validate(args.manifest)
        if pending:
            print(f"INCOMPLETE: {count} captures validated; {len(pending)} matrix entries unresolved.")
            print("Reference capture is required before this manifest can establish a parity baseline.")
            return 2
        print(f"VALID CAPTURE INPUTS: {count} images verified against portable manifest metadata.")
        print("Visual parity remains untested; this command does not run the pixel-diff gate.")
        return 0
    except (ManifestError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
