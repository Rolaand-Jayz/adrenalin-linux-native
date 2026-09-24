"""Version-pinned, standard-library golden-reference visual comparison."""

from __future__ import annotations

import argparse
import array
import binascii
import json
import math
import struct
import sys
import zlib
from collections import deque
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True
import manifest as capture_manifest
import strict_json


IMPLEMENTATION_VERSION = "adrenalin-reference-diff/1.0.0"
SSIM_WINDOW = 11
SSIM_C1 = (0.01 * 255.0) ** 2
SSIM_C2 = (0.03 * 255.0) ** 2
SSIM_MINIMUM = 0.995
MAX_POSITION_DEVIATION = 2
MAX_DIMENSION_DEVIATION = 2
MAX_DELTA_E00 = 2.0
COLOR_TRANSFORM = "IEC 61966-2-1 sRGB D65 2-degree observer to CIELAB; CIEDE2000"


class VisualDiffError(ValueError):
    """The requested visual comparison cannot be evaluated safely."""


def _fail(message: str) -> None:
    raise VisualDiffError(message)


def _png_chunks(path: Path):
    with path.open("rb") as stream:
        if stream.read(8) != b"\x89PNG\r\n\x1a\n":
            _fail(f"not a PNG image: {path.name}")
        while True:
            length_bytes = stream.read(4)
            if not length_bytes:
                break
            if len(length_bytes) != 4:
                _fail(f"truncated PNG chunk in {path.name}")
            length = struct.unpack(">I", length_bytes)[0]
            kind = stream.read(4)
            data = stream.read(length)
            crc = stream.read(4)
            if len(kind) != 4 or len(data) != length or len(crc) != 4:
                _fail(f"truncated PNG chunk in {path.name}")
            yield kind, data
            if kind == b"IEND":
                break


def load_rgb(path: Path) -> tuple[int, int, bytearray]:
    """Decode opaque 8-bit RGB/RGBA, non-interlaced PNGs into RGB bytes."""
    capture_manifest._png_dimensions(path)
    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    saw_srgb = False
    saw_idat = False
    saw_plte = False
    for kind, data in _png_chunks(path):
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", data)
        elif kind == b"PLTE":
            saw_plte = True
        elif kind == b"IDAT":
            saw_idat = True
            compressed.extend(data)
        elif kind in (b"iCCP", b"gAMA", b"cHRM", b"cICP"):
            _fail(f"{path.name}: embedded ICC/gamma/chromaticity/cICP color metadata is not accepted by the pinned sRGB transform")
        elif kind == b"tRNS":
            _fail(f"{path.name}: PNG transparency-key chunks are unsupported for fixed-color UI comparison")
        elif kind == b"sRGB":
            if saw_srgb or saw_plte or saw_idat or len(data) != 1 or data[0] > 3:
                _fail(f"{path.name}: invalid sRGB rendering-intent chunk or chunk ordering")
            saw_srgb = True
    if bit_depth != 8 or color_type not in (2, 6) or interlace != 0:
        _fail(f"{path.name}: visual comparison requires non-interlaced 8-bit RGB or RGBA PNG")
    channels = 3 if color_type == 2 else 4
    row_bytes = width * channels
    expected = height * (row_bytes + 1)
    decompressor = zlib.decompressobj()
    try:
        raw = decompressor.decompress(bytes(compressed), expected + 1)
    except zlib.error as exc:
        raise VisualDiffError(f"{path.name}: invalid PNG image data") from exc
    if len(raw) != expected or not decompressor.eof or decompressor.unused_data or decompressor.unconsumed_tail:
        _fail(f"{path.name}: invalid PNG image-data length")

    rgb = bytearray(width * height * 3)
    previous = bytearray(row_bytes)
    source_offset = target_offset = 0
    for _ in range(height):
        filter_type = raw[source_offset]
        source_offset += 1
        if filter_type > 4:
            _fail(f"{path.name}: invalid PNG filter")
        row = bytearray(raw[source_offset : source_offset + row_bytes])
        source_offset += row_bytes
        for index in range(row_bytes):
            left = row[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 1:
                row[index] = (row[index] + left) & 0xFF
            elif filter_type == 2:
                row[index] = (row[index] + above) & 0xFF
            elif filter_type == 3:
                row[index] = (row[index] + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate - left), abs(estimate - above), abs(estimate - upper_left))
                predictor = left if distances[0] <= distances[1] and distances[0] <= distances[2] else (
                    above if distances[1] <= distances[2] else upper_left
                )
                row[index] = (row[index] + predictor) & 0xFF
        if channels == 3:
            rgb[target_offset : target_offset + width * 3] = row
            target_offset += width * 3
        else:
            for index in range(0, row_bytes, 4):
                if row[index + 3] != 255:
                    _fail(f"{path.name}: translucent pixels are unsupported for UI parity")
                rgb[target_offset : target_offset + 3] = row[index : index + 3]
                target_offset += 3
        previous = row
    return width, height, rgb


def _horizontal_products(reference_row: bytes, candidate_row: bytes, width: int, channel: int, window: int):
    sums = [array.array("d") for _ in range(5)]
    totals = [0.0] * 5
    for x in range(window):
        left = reference_row[x * 3 + channel]
        right = candidate_row[x * 3 + channel]
        for index, value in enumerate((left, right, left * left, right * right, left * right)):
            totals[index] += value
    for total, output in zip(totals, sums):
        output.append(total)
    for start in range(1, width - window + 1):
        old_x = (start - 1) * 3 + channel
        new_x = (start + window - 1) * 3 + channel
        left_old, left_new = reference_row[old_x], reference_row[new_x]
        right_old, right_new = candidate_row[old_x], candidate_row[new_x]
        values = (
            left_new - left_old,
            right_new - right_old,
            left_new * left_new - left_old * left_old,
            right_new * right_new - right_old * right_old,
            left_new * right_new - left_old * right_old,
        )
        for total, delta, output in zip(totals, values, sums):
            total += delta
            output.append(total)
    return sums


def _ssim_channel(reference: bytes, candidate: bytes, mask: bytearray, width: int, height: int, channel: int) -> float:
    window = SSIM_WINDOW
    if width < window or height < window:
        _fail(f"images must be at least {window}x{window} pixels for the pinned SSIM window")
    out_width = width - window + 1
    totals = [array.array("d", [0.0]) * out_width for _ in range(5)]
    masked_totals = array.array("I", [0]) * out_width
    history: deque[tuple[list[array.array], array.array]] = deque()
    valid_windows = 0
    score_sum = 0.0
    count = window * window

    for y in range(height):
        begin = y * width * 3
        reference_row = reference[begin : begin + width * 3]
        candidate_row = candidate[begin : begin + width * 3]
        current = _horizontal_products(reference_row, candidate_row, width, channel, window)
        mask_row = mask[y * width : (y + 1) * width]
        current_mask = array.array("I")
        total = sum(mask_row[:window])
        current_mask.append(total)
        for start in range(1, out_width):
            total += mask_row[start + window - 1] - mask_row[start - 1]
            current_mask.append(total)

        history.append((current, current_mask))
        for target, values in zip(totals, current):
            for x in range(out_width):
                target[x] += values[x]
        for x in range(out_width):
            masked_totals[x] += current_mask[x]
        if len(history) > window:
            old, old_mask = history.popleft()
            for target, values in zip(totals, old):
                for x in range(out_width):
                    target[x] -= values[x]
            for x in range(out_width):
                masked_totals[x] -= old_mask[x]
        if len(history) < window:
            continue

        for x in range(out_width):
            if masked_totals[x]:
                continue
            mean_a = totals[0][x] / count
            mean_b = totals[1][x] / count
            variance_a = max(0.0, totals[2][x] / count - mean_a * mean_a)
            variance_b = max(0.0, totals[3][x] / count - mean_b * mean_b)
            covariance = totals[4][x] / count - mean_a * mean_b
            numerator = (2.0 * mean_a * mean_b + SSIM_C1) * (2.0 * covariance + SSIM_C2)
            denominator = (mean_a * mean_a + mean_b * mean_b + SSIM_C1) * (variance_a + variance_b + SSIM_C2)
            score_sum += numerator / denominator
            valid_windows += 1

    if valid_windows == 0:
        _fail("approved masks cover every SSIM window")
    return score_sum / valid_windows


def calculate_ssim(reference: bytes, candidate: bytes, mask: bytearray, width: int, height: int) -> float:
    """Pinned v1 SSIM: mean RGB-channel SSIM over valid 11x11 uniform windows."""
    expected = width * height * 3
    if len(reference) != expected or len(candidate) != expected or len(mask) != width * height:
        _fail("SSIM input buffers do not match the supplied dimensions")
    return sum(_ssim_channel(reference, candidate, mask, width, height, channel) for channel in range(3)) / 3.0


def _srgb_to_lab(rgb: tuple[int, int, int]) -> tuple[float, float, float]:
    linear = []
    for value in rgb:
        encoded = value / 255.0
        linear.append(encoded / 12.92 if encoded <= 0.04045 else ((encoded + 0.055) / 1.055) ** 2.4)
    red, green, blue = linear
    x = (0.4124564 * red + 0.3575761 * green + 0.1804375 * blue) / 0.95047
    y = 0.2126729 * red + 0.7151522 * green + 0.0721750 * blue
    z = (0.0193339 * red + 0.1191920 * green + 0.9503041 * blue) / 1.08883
    delta = 6.0 / 29.0

    def f(component: float) -> float:
        return component ** (1.0 / 3.0) if component > delta ** 3 else component / (3.0 * delta * delta) + 4.0 / 29.0

    fx, fy, fz = f(x), f(y), f(z)
    return 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)


def _delta_e_2000_lab(lab_a: tuple[float, float, float], lab_b: tuple[float, float, float]) -> float:
    l1, a1, b1 = lab_a
    l2, a2, b2 = lab_b
    c1, c2 = math.hypot(a1, b1), math.hypot(a2, b2)
    c_bar = (c1 + c2) / 2.0
    c_bar7 = c_bar ** 7
    g = 0.5 * (1.0 - math.sqrt(c_bar7 / (c_bar7 + 25.0 ** 7)))
    a1p, a2p = (1.0 + g) * a1, (1.0 + g) * a2
    c1p, c2p = math.hypot(a1p, b1), math.hypot(a2p, b2)

    def hue(a: float, b: float) -> float:
        angle = math.degrees(math.atan2(b, a))
        return angle + 360.0 if angle < 0.0 else angle

    h1p, h2p = hue(a1p, b1), hue(a2p, b2)
    delta_l = l2 - l1
    delta_cp = c2p - c1p
    if c1p * c2p == 0.0:
        delta_hp = 0.0
    else:
        difference = h2p - h1p
        if difference > 180.0:
            difference -= 360.0
        elif difference < -180.0:
            difference += 360.0
        delta_hp = 2.0 * math.sqrt(c1p * c2p) * math.sin(math.radians(difference / 2.0))
    l_bar = (l1 + l2) / 2.0
    c_bar_p = (c1p + c2p) / 2.0
    if c1p * c2p == 0.0:
        h_bar = h1p + h2p
    elif abs(h1p - h2p) <= 180.0:
        h_bar = (h1p + h2p) / 2.0
    elif h1p + h2p < 360.0:
        h_bar = (h1p + h2p + 360.0) / 2.0
    else:
        h_bar = (h1p + h2p - 360.0) / 2.0
    t = (
        1.0
        - 0.17 * math.cos(math.radians(h_bar - 30.0))
        + 0.24 * math.cos(math.radians(2.0 * h_bar))
        + 0.32 * math.cos(math.radians(3.0 * h_bar + 6.0))
        - 0.20 * math.cos(math.radians(4.0 * h_bar - 63.0))
    )
    delta_theta = 30.0 * math.exp(-(((h_bar - 275.0) / 25.0) ** 2))
    c_bar_p7 = c_bar_p ** 7
    r_c = 2.0 * math.sqrt(c_bar_p7 / (c_bar_p7 + 25.0 ** 7))
    l_term = l_bar - 50.0
    s_l = 1.0 + 0.015 * (l_term * l_term) / math.sqrt(20.0 + l_term * l_term)
    s_c = 1.0 + 0.045 * c_bar_p
    s_h = 1.0 + 0.015 * c_bar_p * t
    r_t = -math.sin(math.radians(2.0 * delta_theta)) * r_c
    l_component, c_component, h_component = delta_l / s_l, delta_cp / s_c, delta_hp / s_h
    return math.sqrt(max(0.0, l_component * l_component + c_component * c_component + h_component * h_component + r_t * c_component * h_component))


def delta_e_2000(rgb_a: tuple[int, int, int], rgb_b: tuple[int, int, int]) -> float:
    """CIEDE2000 distance after the pinned sRGB-to-Lab D65 transformation."""
    return _delta_e_2000_lab(_srgb_to_lab(rgb_a), _srgb_to_lab(rgb_b))


def _rect(value: Any, label: str, width: int, height: int) -> tuple[int, int, int, int]:
    if not isinstance(value, dict) or set(value) != {"x", "y", "width", "height"}:
        _fail(f"{label} rectangle must provide exactly x, y, width, and height")
    coords = tuple(value[key] for key in ("x", "y", "width", "height"))
    if any(not isinstance(number, int) or isinstance(number, bool) for number in coords):
        _fail(f"{label} rectangle coordinates must be integers")
    x, y, rect_width, rect_height = coords
    if x < 0 or y < 0 or rect_width <= 0 or rect_height <= 0 or x + rect_width > width or y + rect_height > height:
        _fail(f"{label} rectangle lies outside the image")
    return coords


def _load_checks(path: Path, width: int, height: int, capture_id: str,
                 reference_sha256: str, candidate_sha256: str, geometry_sha256: str,
                 approved_masks_sha256: str | None):
    try:
        data = strict_json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError, strict_json.DuplicateJSONKeyError) as exc:
        raise VisualDiffError(f"cannot read geometry/color checks: {exc}") from exc
    expected_keys = {"schema_version", "coverage", "components", "color_samples"}
    if not isinstance(data, dict) or set(data) != expected_keys or data["schema_version"] != 1:
        _fail("checks file must use the exact visual-check schema version 1")
    coverage = data["coverage"]
    coverage_keys = {
        "status", "reviewer", "reviewer_role", "review_record", "reference_capture_id", "reference_capture_sha256",
        "candidate_png_sha256", "candidate_geometry_sha256", "approved_masks_sha256",
    }
    if not isinstance(coverage, dict) or set(coverage) != coverage_keys or coverage["status"] != "reviewed_complete":
        _fail("checks file requires independently reviewed, complete geometry/color coverage")
    for field in coverage_keys - {"status", "approved_masks_sha256"}:
        if not isinstance(coverage[field], str) or not coverage[field].strip():
            _fail(f"checks coverage requires {field}")
    if approved_masks_sha256 is None:
        if coverage["approved_masks_sha256"] is not None:
            _fail("checks coverage must set approved_masks_sha256 to JSON null when no masks are supplied")
    elif coverage["approved_masks_sha256"] != approved_masks_sha256:
        _fail("checks coverage does not bind to the exact approved-mask file hash")
    if coverage["reviewer_role"] != "independent_reviewer":
        _fail("checks coverage requires an independent_reviewer attestation")
    if coverage["reference_capture_id"] != capture_id or coverage["reference_capture_sha256"] != reference_sha256:
        _fail("reviewed checks do not bind to the selected authentic reference capture")
    if coverage["candidate_png_sha256"] != candidate_sha256 or coverage["candidate_geometry_sha256"] != geometry_sha256:
        _fail("reviewed checks do not bind to these exact candidate image and runtime-geometry evidence bytes")
    if coverage["approved_masks_sha256"] != approved_masks_sha256:
        _fail("reviewed checks do not bind to the selected approved-mask file")
    components, colors = data["components"], data["color_samples"]
    if not isinstance(components, list) or not components or not isinstance(colors, list) or not colors:
        _fail("checks file must include component geometry and fixed-color samples")
    seen: set[str] = set()
    checked_components = []
    for component in components:
        if not isinstance(component, dict) or set(component) != {"id", "reference"}:
            _fail("each reference component requires exactly an id and annotated reference rectangle")
        identity = component["id"]
        if not isinstance(identity, str) or not identity or identity in seen:
            _fail("component ids must be nonempty and unique")
        seen.add(identity)
        checked_components.append({
            "id": identity,
            "reference": _rect(component["reference"], f"component {identity} reference", width, height),
        })
    seen.clear()
    checked_colors = []
    for sample in colors:
        if not isinstance(sample, dict) or set(sample) != {"id", "reference_xy", "candidate_xy"}:
            _fail("each fixed-color sample requires id, reference_xy, and candidate_xy")
        identity = sample["id"]
        if not isinstance(identity, str) or not identity or identity in seen:
            _fail("fixed-color sample ids must be nonempty and unique")
        seen.add(identity)
        coordinates = []
        for key in ("reference_xy", "candidate_xy"):
            pair = sample[key]
            if not isinstance(pair, list) or len(pair) != 2 or any(not isinstance(n, int) or isinstance(n, bool) for n in pair):
                _fail(f"{identity} {key} must be a two-integer pixel coordinate")
            x, y = pair
            if not 0 <= x < width or not 0 <= y < height:
                _fail(f"{identity} {key} lies outside the image")
            coordinates.append((x, y))
        checked_colors.append({"id": identity, "reference_xy": coordinates[0], "candidate_xy": coordinates[1]})
    return checked_components, checked_colors, coverage


def _load_runtime_geometry(path: Path, width: int, height: int, screen_id: str,
                           capture_id: str) -> tuple[dict[str, tuple[int, int, int, int]], dict[str, str]]:
    try:
        data = strict_json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError, strict_json.DuplicateJSONKeyError) as exc:
        raise VisualDiffError(f"cannot read candidate runtime-geometry evidence: {exc}") from exc
    keys = {"schema_version", "source", "build_identity", "fixture_id", "screen_id", "capture_id", "components"}
    if not isinstance(data, dict) or set(data) != keys or data["schema_version"] != 1:
        _fail("runtime geometry must use the exact schema version 1")
    if data["source"] != "externally_attested_qt_qml_runtime_geometry_export":
        _fail("candidate rectangles require an externally attested rendered Qt/QML runtime geometry export")
    if data["screen_id"] != screen_id or data["capture_id"] != capture_id:
        _fail("runtime geometry evidence does not match the selected screen and capture state")
    if not isinstance(data["build_identity"], str) or len(data["build_identity"]) != 64 or any(
        character not in "0123456789abcdef" for character in data["build_identity"]
    ):
        _fail("runtime geometry build_identity must be the lowercase SHA-256 of the rendered application build")
    for field in ("fixture_id",):
        if not isinstance(data[field], str) or not data[field].strip():
            _fail(f"runtime geometry evidence requires {field}")
    if not isinstance(data["components"], list) or not data["components"]:
        _fail("runtime geometry evidence must contain measured component rectangles")
    components: dict[str, tuple[int, int, int, int]] = {}
    for component in data["components"]:
        if not isinstance(component, dict) or set(component) != {"id", "rect"}:
            _fail("runtime component evidence requires exactly id and measured rect")
        identity = component["id"]
        if not isinstance(identity, str) or not identity or identity in components:
            _fail("runtime component ids must be nonempty and unique")
        components[identity] = _rect(component["rect"], f"runtime component {identity}", width, height)
    provenance = {"build_identity": data["build_identity"], "fixture_id": data["fixture_id"], "source": data["source"]}
    return components, provenance


def _load_masks(path: Path | None, width: int, height: int, screen_id: str,
                capture_id: str, reference_sha256: str):
    bitmap = bytearray(width * height)
    if path is None:
        return bitmap, [], None
    try:
        data = strict_json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError, strict_json.DuplicateJSONKeyError) as exc:
        raise VisualDiffError(f"cannot read approved-mask file: {exc}") from exc
    required = {
        "schema_version", "review_status", "reviewed_by", "reviewed_at", "approval_reference",
        "screen_id", "capture_id", "reference_capture_sha256", "masks",
    }
    if not isinstance(data, dict) or set(data) != required or data["schema_version"] != 1 or data["review_status"] != "approved":
        _fail("mask file must use schema version 1 and have an approved review record")
    for field in ("reviewed_by", "reviewed_at", "approval_reference"):
        if not isinstance(data[field], str) or not data[field].strip():
            _fail(f"mask file requires {field}")
    if data["screen_id"] != screen_id or data["capture_id"] != capture_id or data["reference_capture_sha256"] != reference_sha256:
        _fail("approved masks are not bound to the selected screen, capture, and reference image hash")
    if not isinstance(data["masks"], list):
        _fail("mask list must be an array")
    masks = []
    seen: set[str] = set()
    for item in data["masks"]:
        if not isinstance(item, dict) or set(item) != {"id", "reason", "rect"}:
            _fail("each approved mask requires exactly id, reason, and rect")
        identity, reason = item["id"], item["reason"]
        if not isinstance(identity, str) or not identity or identity in seen:
            _fail("mask ids must be nonempty and unique")
        if not isinstance(reason, str) or not reason.strip():
            _fail(f"mask {identity} requires a reason")
        seen.add(identity)
        rect = _rect(item["rect"], f"mask {identity}", width, height)
        x, y, rect_width, rect_height = rect
        for row in range(y, y + rect_height):
            start = row * width + x
            bitmap[start : start + rect_width] = b"\x01" * rect_width
        masks.append({"id": identity, "reason": reason, "rect": rect})
    binding = {
        "screen_id": screen_id,
        "capture_id": capture_id,
        "reference_capture_sha256": reference_sha256,
        "approved_masks_sha256": capture_manifest._sha256_file(path),
        "reviewed_by": data["reviewed_by"],
        "reviewed_at": data["reviewed_at"],
        "approval_reference": data["approval_reference"],
    }
    return bitmap, masks, binding


def _rgb_at(pixels: bytes, width: int, xy: tuple[int, int]) -> tuple[int, int, int]:
    offset = (xy[1] * width + xy[0]) * 3
    return pixels[offset], pixels[offset + 1], pixels[offset + 2]


def _encode_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        payload = kind + data
        return struct.pack(">I", len(data)) + payload + struct.pack(">I", binascii.crc32(payload) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + pixels[y * width * 3 : (y + 1) * width * 3] for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    output = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, level=9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(output)


def _dimension_failure_diff(reference: bytes, ref_width: int, ref_height: int,
                            candidate: bytes, cand_width: int, cand_height: int) -> tuple[int, int, bytes]:
    """Create a side-by-side artifact when the inputs cannot be pixel-compared."""
    width, height = ref_width + cand_width, max(ref_height, cand_height)
    canvas = bytearray(width * height * 3)
    for y in range(ref_height):
        source = y * ref_width * 3
        target = y * width * 3
        canvas[target : target + ref_width * 3] = reference[source : source + ref_width * 3]
    for y in range(cand_height):
        source = y * cand_width * 3
        target = (y * width + ref_width) * 3
        canvas[target : target + cand_width * 3] = candidate[source : source + cand_width * 3]
    return width, height, canvas


def _diff_pixels(reference: bytes, candidate: bytes, mask: bytearray) -> bytearray:
    diff = bytearray(len(reference))
    for pixel, (ref_start, cand_start) in enumerate(zip(range(0, len(reference), 3), range(0, len(candidate), 3))):
        target = pixel * 3
        if mask[pixel]:
            diff[target : target + 3] = b"\x00\x70\xff"
        else:
            for channel in range(3):
                diff[target + channel] = min(255, abs(reference[ref_start + channel] - candidate[cand_start + channel]) * 4)
    return diff


def compare(reference: bytes, candidate: bytes, width: int, height: int, mask: bytearray,
            components: list[dict[str, Any]], colors: list[dict[str, Any]]) -> dict[str, Any]:
    score = calculate_ssim(reference, candidate, mask, width, height)
    position_findings = []
    dimension_findings = []
    component_measurements = []
    for component in components:
        ref_x, ref_y, ref_width, ref_height = component["reference"]
        cand_x, cand_y, cand_width, cand_height = component["candidate"]
        dx, dy = cand_x - ref_x, cand_y - ref_y
        dw, dh = cand_width - ref_width, cand_height - ref_height
        component_measurements.append({
            "id": component["id"],
            "reference": {"x": ref_x, "y": ref_y, "width": ref_width, "height": ref_height},
            "candidate": {"x": cand_x, "y": cand_y, "width": cand_width, "height": cand_height},
            "deviation": {"dx": dx, "dy": dy, "dw": dw, "dh": dh},
        })
        if abs(dx) > MAX_POSITION_DEVIATION or abs(dy) > MAX_POSITION_DEVIATION:
            position_findings.append({"id": component["id"], "dx": dx, "dy": dy})
        if abs(dw) > MAX_DIMENSION_DEVIATION or abs(dh) > MAX_DIMENSION_DEVIATION:
            dimension_findings.append({"id": component["id"], "dw": dw, "dh": dh})
    color_results = []
    for sample in colors:
        rgb_ref = _rgb_at(reference, width, sample["reference_xy"])
        rgb_candidate = _rgb_at(candidate, width, sample["candidate_xy"])
        delta = delta_e_2000(rgb_ref, rgb_candidate)
        color_results.append({
            "id": sample["id"],
            "reference_rgb": rgb_ref,
            "candidate_rgb": rgb_candidate,
            "delta_e_2000": delta,
            "pass": delta <= MAX_DELTA_E00,
        })
    gate = (
        score >= SSIM_MINIMUM
        and not position_findings
        and not dimension_findings
        and all(result["pass"] for result in color_results)
    )
    return {
        "implementation": IMPLEMENTATION_VERSION,
        "color_transform": COLOR_TRANSFORM,
        "image_dimensions": {"width": width, "height": height},
        "ssim": {"value": score, "minimum": SSIM_MINIMUM, "window": SSIM_WINDOW, "weighting": "uniform", "channels": "independent RGB mean"},
        "geometry": {
            "maximum_position_deviation_px": MAX_POSITION_DEVIATION,
            "maximum_component_dimension_deviation_px": MAX_DIMENSION_DEVIATION,
            "position_findings": position_findings,
            "dimension_findings": dimension_findings,
            "component_count": len(components),
            "measurements": component_measurements,
        },
        "fixed_color_samples": {"maximum_delta_e_2000": MAX_DELTA_E00, "samples": color_results},
        "gate": "pass" if gate else "fail",
    }


def _manifest_capture(manifest_path: Path, capture_id: str) -> tuple[dict[str, Any], Path, str]:
    count, pending = capture_manifest.validate(manifest_path)
    if pending:
        _fail(f"capture manifest is incomplete ({count} verified images; {len(pending)} unresolved entries)")
    try:
        data = strict_json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError, strict_json.DuplicateJSONKeyError) as exc:
        raise VisualDiffError(f"cannot read capture manifest: {exc}") from exc
    row = next((item for item in data["captures"] if item["id"] == capture_id), None)
    if row is None or row["status"] != "captured":
        _fail(f"capture id is missing or not captured: {capture_id}")
    reference_path = capture_manifest._safe_image_path(manifest_path, row["image"])
    return row, reference_path, data["screen_id"]


def _ensure_output_paths_distinct(output_paths: list[Path], input_paths: list[Path]) -> None:
    """Reject lexical, symlink, and hard-link aliases before writing artifacts."""
    resolved_outputs = [path.resolve(strict=False) for path in output_paths]
    resolved_inputs = [path.resolve(strict=True) for path in input_paths]
    if len(set(resolved_outputs)) != len(resolved_outputs):
        _fail("report and diff output paths must be distinct")
    for output in output_paths:
        if not output.exists():
            continue
        for source in input_paths:
            try:
                if output.samefile(source):
                    _fail("report and diff outputs must be distinct from every input file")
            except OSError as exc:
                raise VisualDiffError(f"cannot safely compare output and input file identities: {exc}") from exc
    for index, output in enumerate(output_paths):
        if not output.exists():
            continue
        for other in output_paths[index + 1:]:
            if not other.exists():
                continue
            try:
                if output.samefile(other):
                    _fail("report and diff output files must have distinct file identities")
            except OSError as exc:
                raise VisualDiffError(f"cannot safely compare output file identities: {exc}") from exc
    if any(output in resolved_inputs for output in resolved_outputs):
        _fail("report and diff outputs must be distinct from every input file")


def run(args: argparse.Namespace) -> int:
    input_paths = [
        args.manifest.resolve(strict=True), args.candidate.resolve(strict=True),
        args.checks.resolve(strict=True), args.candidate_geometry.resolve(strict=True),
    ]
    if args.masks is not None:
        input_paths.append(args.masks.resolve(strict=True))
    output_paths = [args.report.resolve(strict=False), args.diff_output.resolve(strict=False)]
    row, reference_path, screen_id = _manifest_capture(args.manifest, args.capture_id)
    input_paths.append(reference_path.resolve(strict=True))
    _ensure_output_paths_distinct(output_paths, input_paths)
    ref_width, ref_height, reference = load_rgb(reference_path)
    mask, mask_records, mask_binding = _load_masks(
        args.masks, ref_width, ref_height, screen_id, row["id"], row["sha256"]
    )
    candidate_path = args.candidate.resolve(strict=True)
    cand_width, cand_height, candidate = load_rgb(candidate_path)
    if (ref_width, ref_height) != (cand_width, cand_height):
        try:
            checks_data = strict_json.loads(args.checks.read_text(encoding="utf-8"))
            coverage = checks_data.get("coverage", {}) if isinstance(checks_data, dict) else {}
        except (OSError, UnicodeError, json.JSONDecodeError, strict_json.DuplicateJSONKeyError):
            coverage = {}
        report = {
            "implementation": IMPLEMENTATION_VERSION,
            "reference": {
                "version": capture_manifest.REFERENCE["version"],
                "release_date": capture_manifest.REFERENCE["release_date"],
                "screen_id": screen_id,
                "capture_id": row["id"],
                "sha256": row["sha256"],
            },
            "candidate_sha256": capture_manifest._sha256_file(candidate_path),
            "checks_sha256": capture_manifest._sha256_file(args.checks),
            "candidate_geometry_sha256": capture_manifest._sha256_file(args.candidate_geometry),
            "approved_masks_sha256": mask_binding["approved_masks_sha256"] if mask_binding else None,
            "mask_binding": mask_binding,
            "review_identity": {
                "reviewer": coverage.get("reviewer"),
                "reviewer_role": coverage.get("reviewer_role"),
                "record": coverage.get("review_record"),
                "status": coverage.get("status"),
            },
            "reference_dimensions": {"width": ref_width, "height": ref_height},
            "candidate_dimensions": {"width": cand_width, "height": cand_height},
            "gate": "fail",
            "failure": "physical image dimensions differ; automatic resizing is forbidden",
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        diff_width, diff_height, diff = _dimension_failure_diff(reference, ref_width, ref_height, candidate, cand_width, cand_height)
        _encode_png(args.diff_output, diff_width, diff_height, diff)
        print(f"PARITY FAIL: report={args.report}; dimension mismatch diff={args.diff_output}")
        return 2
    if row["physical_width"] != ref_width or row["physical_height"] != ref_height:
        _fail("reference image dimensions do not match capture metadata")
    candidate_sha256 = capture_manifest._sha256_file(candidate_path)
    geometry_sha256 = capture_manifest._sha256_file(args.candidate_geometry)
    reference_components, colors, coverage = _load_checks(
        args.checks, ref_width, ref_height, row["id"], row["sha256"], candidate_sha256,
        geometry_sha256, mask_binding["approved_masks_sha256"] if mask_binding else None,
    )
    runtime_components, runtime_provenance = _load_runtime_geometry(
        args.candidate_geometry, ref_width, ref_height, screen_id, row["id"]
    )
    reference_ids = {component["id"] for component in reference_components}
    if reference_ids != set(runtime_components):
        _fail("reference annotations and runtime geometry must contain exactly the same component identities")
    components = [
        {"id": component["id"], "reference": component["reference"], "candidate": runtime_components[component["id"]]}
        for component in reference_components
    ]
    for sample in colors:
        if mask[sample["reference_xy"][1] * ref_width + sample["reference_xy"][0]] or mask[sample["candidate_xy"][1] * ref_width + sample["candidate_xy"][0]]:
            _fail(f"fixed-color sample {sample['id']} overlaps an approved dynamic mask")
    report = compare(reference, candidate, ref_width, ref_height, mask, components, colors)
    report["reference"] = {
        "version": capture_manifest.REFERENCE["version"],
        "release_date": capture_manifest.REFERENCE["release_date"],
        "screen_id": strict_json.loads(args.manifest.read_text(encoding="utf-8"))["screen_id"],
        "capture_id": row["id"],
        "sha256": row["sha256"],
    }
    report["mask_count"] = len(mask_records)
    report["candidate_sha256"] = candidate_sha256
    report["checks_sha256"] = capture_manifest._sha256_file(args.checks)
    report["review_identity"] = {
        "reviewer": coverage["reviewer"],
        "reviewer_role": coverage["reviewer_role"],
        "record": coverage["review_record"],
        "status": coverage["status"],
    }
    report["candidate_runtime_geometry"] = {**runtime_provenance, "sha256": geometry_sha256}
    report["approved_masks_sha256"] = mask_binding["approved_masks_sha256"] if mask_binding else None
    report["mask_binding"] = mask_binding
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if report["gate"] == "fail":
        _encode_png(args.diff_output, ref_width, ref_height, _diff_pixels(reference, candidate, mask))
        print(f"PARITY FAIL: report={args.report}; diff={args.diff_output}")
        return 2
    print(f"PARITY PASS: {args.report}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    command = parser.add_subparsers(dest="command", required=True).add_parser("compare")
    command.add_argument("--manifest", type=Path, required=True, help="complete fixed-reference capture manifest")
    command.add_argument("--capture-id", required=True, help="captured matrix row to compare")
    command.add_argument("--candidate", type=Path, required=True, help="candidate PNG; no resizing is performed")
    command.add_argument("--checks", type=Path, required=True, help="component geometry and fixed-color sample JSON")
    command.add_argument("--candidate-geometry", type=Path, required=True, help="externally attested Qt/QML runtime geometry export for this candidate")
    command.add_argument("--masks", type=Path, help="review-approved dynamic-mask JSON")
    command.add_argument("--report", type=Path, required=True, help="comparison report output path")
    command.add_argument("--diff-output", type=Path, required=True, help="failure diff PNG output path")
    args = parser.parse_args(argv)
    try:
        return run(args)
    except (VisualDiffError, capture_manifest.ManifestError, OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
