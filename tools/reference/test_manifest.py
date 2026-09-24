import binascii
import contextlib
import hashlib
import importlib.util
import io
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import visual_diff


MODULE_PATH = Path(__file__).with_name("manifest.py")
SPEC = importlib.util.spec_from_file_location("reference_manifest", MODULE_PATH)
manifest = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(manifest)


def png_chunk(kind, data):
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", binascii.crc32(payload) & 0xFFFFFFFF)


def make_png(width=1, height=1, idat_data=None, pre_idat_chunks=(), post_idat_chunks=()):
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    if idat_data is None:
        compressor = zlib.compressobj()
        compressed_parts = []
        row = b"\x00" + b"\x00\x00\x00\xff" * width
        for _ in range(height):
            compressed_parts.append(compressor.compress(row))
        compressed_parts.append(compressor.flush())
        idat_data = b"".join(compressed_parts)
    chunks = b"".join(png_chunk(kind, data) for kind, data in pre_idat_chunks)
    post_chunks = b"".join(png_chunk(kind, data) for kind, data in post_idat_chunks)
    return b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header) + chunks + png_chunk(b"IDAT", idat_data) + post_chunks + png_chunk(b"IEND", b"")


def captured_manifest(directory, image_bytes, context_id="1920x1080-100", physical_width=1920, physical_height=1080):
    directory = Path(directory)
    image_path = directory / "capture.png"
    image_path.write_bytes(image_bytes)
    data = manifest.initial_manifest("home")
    row = next(row for row in data["captures"] if row["id"] == f"{context_id}:default")
    row.update(
        status="captured",
        image="capture.png",
        sha256=hashlib.sha256(image_bytes).hexdigest(),
        physical_width=physical_width,
        physical_height=physical_height,
        observations={field: "REFERENCE_REQUIRED" for field in manifest.OBSERVATION_FIELDS},
    )
    path = directory / "capture-manifest.json"
    path.write_text(json.dumps(data), encoding="utf-8")
    return path, data, row


class ManifestTests(unittest.TestCase):
    def test_initial_plan_has_fixed_reference_and_full_pending_matrix(self):
        data = manifest.initial_manifest("home")
        self.assertEqual(data["reference"], manifest.REFERENCE)
        self.assertEqual(len(data["contexts"]), 6)
        self.assertEqual(len(data["captures"]), 6 * len(manifest.STATES))
        self.assertTrue(all(row["status"] == "pending" for row in data["captures"]))
        self.assertEqual(data["parity"]["status"], "not_run")

    def test_screen_id_is_validated(self):
        with self.assertRaises(manifest.ManifestError):
            manifest.initial_manifest("Home Screen")

    def test_incomplete_plan_is_not_reported_as_parity_success(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            path.write_text(json.dumps(manifest.initial_manifest("home")), encoding="utf-8")
            count, pending = manifest.validate(path)
        self.assertEqual(count, 0)
        self.assertEqual(len(pending), 6 * len(manifest.STATES))

    def test_reference_release_cannot_be_silently_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            data = manifest.initial_manifest("home")
            data["reference"]["version"] = "newer release"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "reference identity"):
                manifest.validate(path)

    def test_validate_rejects_invalid_screen_id(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            data = manifest.initial_manifest("home")
            data["screen_id"] = "Home Screen"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "screen_id"):
                manifest.validate(path)

    def test_png_parser_rejects_truncated_data(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "truncated.png"
            path.write_bytes(make_png()[:-8])
            with self.assertRaisesRegex(manifest.ManifestError, "truncated|IEND"):
                manifest._png_dimensions(path)

    def test_png_parser_rejects_bad_chunk_crc(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad-crc.png"
            image = bytearray(make_png())
            image[20] ^= 1
            path.write_bytes(image)
            with self.assertRaisesRegex(manifest.ManifestError, "checksum"):
                manifest._png_dimensions(path)

    def test_png_parser_rejects_invalid_zlib_image_data(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad-image-data.png"
            path.write_bytes(make_png(idat_data=b"not a zlib stream"))
            with self.assertRaisesRegex(manifest.ManifestError, "image-data"):
                manifest._png_dimensions(path)

    def test_png_parser_rejects_valid_crc_trns_after_idat(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "late-transparency.png"
            header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
            image = (
                b"\x89PNG\r\n\x1a\n"
                + png_chunk(b"IHDR", header)
                + png_chunk(b"IDAT", zlib.compress(b"\x00\x00\x00\x00"))
                + png_chunk(b"tRNS", struct.pack(">HHH", 0, 0, 0))
                + png_chunk(b"IEND", b"")
            )
            path.write_bytes(image)
            with self.assertRaisesRegex(manifest.ManifestError, "tRNS"):
                manifest._png_dimensions(path)

    def test_manifest_validator_rejects_srgb_or_cicp_after_palette_or_idat(self):
        cases = (
            {"pre_idat_chunks": ((b"PLTE", b"\x00\x00\x00"), (b"sRGB", b"\x00"))},
            {"post_idat_chunks": ((b"sRGB", b"\x00"),)},
            {"pre_idat_chunks": ((b"sRGB", b"\x00"), (b"sRGB", b"\x00"))},
            {"pre_idat_chunks": ((b"PLTE", b"\x00\x00\x00"), (b"cICP", bytes((1, 13, 0, 1))))},
            {"post_idat_chunks": ((b"cICP", bytes((1, 13, 0, 1))),)},
            {"pre_idat_chunks": ((b"cICP", bytes((1, 13, 0, 1))), (b"cICP", bytes((1, 13, 0, 1))))},
        )
        for options in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                path, _, _ = captured_manifest(
                    directory,
                    make_png(12, 12, **options),
                    context_id="default-window",
                    physical_width=12,
                    physical_height=12,
                )
                with self.assertRaisesRegex(manifest.ManifestError, "(sRGB.*(ordering|chunk)|cICP.*(ordering|chunk|unsupported))"):
                    manifest.validate(path)

    def test_manifest_validator_accepts_srgb_before_palette_and_image_data(self):
        with tempfile.TemporaryDirectory() as directory:
            path, _, _ = captured_manifest(
                directory,
                make_png(12, 12, pre_idat_chunks=((b"sRGB", b"\x00"), (b"PLTE", b"\x00\x00\x00"))),
                context_id="default-window",
                physical_width=12,
                physical_height=12,
            )
            count, _ = manifest.validate(path)
            self.assertEqual(count, 1)

    def test_validator_rejects_capture_checksum_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            path, _, _ = captured_manifest(directory, make_png(1920, 1080))
            image = path.parent / "capture.png"
            image.write_bytes(image.read_bytes() + b"changed")
            with self.assertRaisesRegex(manifest.ManifestError, "checksum mismatch"):
                manifest.validate(path)

    def test_validator_rejects_declared_dimension_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            path, data, row = captured_manifest(directory, make_png(1920, 1080))
            row["physical_width"] = 1921
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "dimensions"):
                manifest.validate(path)

    def test_absolute_image_path_is_rejected(self):
        separator = chr(47)
        posix_absolute = separator + separator.join(("captures", "reference.png"))
        drive_absolute = chr(67) + chr(58) + separator + separator.join(("captures", "reference.png"))
        for image_path in (posix_absolute, drive_absolute):
            with self.subTest(image_path=image_path), tempfile.TemporaryDirectory() as directory:
                path, data, row = captured_manifest(directory, make_png(1920, 1080))
                row["image"] = image_path
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaisesRegex(manifest.ManifestError, "project-relative"):
                    manifest.validate(path)

    def test_symlink_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory, tempfile.TemporaryDirectory() as outside:
            external = Path(outside) / "external.png"
            external.write_bytes(make_png(1920, 1080))
            path, data, row = captured_manifest(directory, make_png(1920, 1080))
            link = path.parent / "linked.png"
            link.symlink_to(external)
            row["image"] = "linked.png"
            row["sha256"] = hashlib.sha256(external.read_bytes()).hexdigest()
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "outside"):
                manifest.validate(path)

    def test_boolean_is_not_accepted_as_scale_or_dimension(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            data = manifest.initial_manifest("home")
            window_context = next(context for context in data["contexts"] if context["id"] == "default-window")
            window_context["scale_percent"] = True
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "positive integer"):
                manifest.validate(path)

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            data = manifest.initial_manifest("home")
            window_context = next(context for context in data["contexts"] if context["id"] == "default-window")
            window_context["width"] = True
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "positive integer"):
                manifest.validate(path)

        with tempfile.TemporaryDirectory() as directory:
            path, data, row = captured_manifest(directory, make_png(1, 1), "default-window", 1, 1)
            row["physical_width"] = True
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "dimensions"):
                manifest.validate(path)

    def test_nul_image_path_is_clean_cli_validation_error(self):
        with tempfile.TemporaryDirectory() as directory:
            path, data, row = captured_manifest(directory, make_png(1920, 1080))
            row["image"] = "capture\x00.png"
            path.write_text(json.dumps(data), encoding="utf-8")
            error_output = io.StringIO()
            with contextlib.redirect_stderr(error_output):
                exit_code = manifest.main(["validate", str(path)])
        self.assertEqual(exit_code, 1)
        self.assertIn("ERROR:", error_output.getvalue())
        self.assertIn("project-relative", error_output.getvalue())
        self.assertNotIn("Traceback", error_output.getvalue())

    def test_parity_metadata_is_strict_and_cannot_claim_a_result(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            data = manifest.initial_manifest("home")
            data["parity"]["result"] = "pass"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "pixel parity status"):
                manifest.validate(path)

    def test_image_path_cannot_escape_manifest_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            data = manifest.initial_manifest("home")
            row = data["captures"][0]
            row.update(
                status="captured",
                image=(chr(46) * 2) + chr(47) + "outside.png",
                sha256="0" * 64,
                observations={field: "REFERENCE_REQUIRED" for field in manifest.OBSERVATION_FIELDS},
            )
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "project-relative"):
                manifest.validate(path)

    def test_initializer_refuses_to_overwrite_existing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-manifest.json"
            path.write_text("preserve", encoding="utf-8")
            with self.assertRaisesRegex(manifest.ManifestError, "refusing to overwrite"):
                manifest._write_initial(path, "home")
            self.assertEqual(path.read_text(encoding="utf-8"), "preserve")


class VisualMetricUnitTests(unittest.TestCase):
    def test_identical_synthetic_rgb_buffers_have_unit_ssim(self):
        width = height = 16
        pixels = bytes((90, 110, 130)) * (width * height)
        score = visual_diff.calculate_ssim(pixels, pixels, bytearray(width * height), width, height)
        self.assertAlmostEqual(score, 1.0, places=12)

    def test_local_synthetic_change_reduces_ssim(self):
        width = height = 16
        reference = bytearray(bytes((90, 110, 130)) * (width * height))
        candidate = bytearray(reference)
        offset = (height // 2 * width + width // 2) * 3
        candidate[offset] += 20
        score = visual_diff.calculate_ssim(reference, candidate, bytearray(width * height), width, height)
        self.assertLess(score, 1.0)

    def test_ciede2000_matches_published_lab_reference_pair(self):
        lab_a = (50.0, 2.6772, -79.7751)
        lab_b = (50.0, 0.0, -82.7485)
        self.assertAlmostEqual(visual_diff._delta_e_2000_lab(lab_a, lab_b), 2.0425, places=4)
        self.assertEqual(visual_diff.delta_e_2000((12, 34, 56), (12, 34, 56)), 0.0)

    def test_decoder_accepts_deterministic_synthetic_opaque_png(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "unit-only.png"
            path.write_bytes(make_png(12, 12))
            width, height, pixels = visual_diff.load_rgb(path)
        self.assertEqual((width, height), (12, 12))
        self.assertEqual(pixels, bytearray((0, 0, 0)) * (width * height))

    def test_decoder_accepts_srgb_before_palette_and_image_data(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "srgb-before-palette.png"
            path.write_bytes(make_png(12, 12, pre_idat_chunks=((b"sRGB", b"\x00"), (b"PLTE", b"\x00\x00\x00"))))
            width, height, _ = visual_diff.load_rgb(path)
        self.assertEqual((width, height), (12, 12))

    def test_decoder_rejects_srgb_after_palette_or_image_data(self):
        cases = (
            {"pre_idat_chunks": ((b"PLTE", b"\x00\x00\x00"), (b"sRGB", b"\x00"))},
            {"post_idat_chunks": ((b"sRGB", b"\x00"),)},
        )
        for options in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "late-srgb.png"
                path.write_bytes(make_png(12, 12, **options))
                with self.assertRaisesRegex(ValueError, "sRGB.*ordering"):
                    visual_diff.load_rgb(path)

    def test_output_alias_guard_rejects_hard_links_to_inputs_and_each_other(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "input.json"
            alias = root / "report.json"
            second = root / "diff.png"
            source.write_text("source", encoding="utf-8")
            alias.hardlink_to(source)
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "every input"):
                visual_diff._ensure_output_paths_distinct([alias, second], [source])
            alias.unlink()
            second.write_text("output", encoding="utf-8")
            alias.hardlink_to(second)
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "distinct file identities"):
                visual_diff._ensure_output_paths_distinct([alias, second], [source])
            alias.unlink()
            self.assertIsNone(visual_diff._ensure_output_paths_distinct([alias, second], [source]))

    def test_decoder_rejects_cicp_even_when_srgb_chunk_is_also_present(self):
        cicp = bytes((1, 13, 0, 1))
        for chunks in (
            ((b"cICP", cicp),),
            ((b"sRGB", b"\x00"), (b"cICP", cicp)),
            ((b"cICP", cicp), (b"sRGB", b"\x00")),
        ):
            with self.subTest(chunks=[kind.decode("ascii") for kind, _ in chunks]), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "cicp-color-space.png"
                path.write_bytes(make_png(12, 12, pre_idat_chunks=chunks))
                with self.assertRaisesRegex(ValueError, "cICP"):
                    visual_diff.load_rgb(path)

    def test_failure_diff_encoder_emits_png_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "output" / "unit-diff.png"
            visual_diff._encode_png(path, 12, 12, bytes((3, 7, 11)) * (12 * 12))
            self.assertEqual(manifest._png_dimensions(path), (12, 12))

    def test_dimension_failure_artifact_keeps_both_physical_sizes_unscaled(self):
        reference = bytes((10, 20, 30)) * (2 * 2)
        candidate = bytes((40, 50, 60)) * (3 * 1)
        width, height, side_by_side = visual_diff._dimension_failure_diff(reference, 2, 2, candidate, 3, 1)
        self.assertEqual((width, height), (5, 2))
        self.assertEqual(side_by_side[:6], reference[:6])
        self.assertEqual(side_by_side[6:15], candidate[:9])

    def test_geometry_checks_require_explicit_component_rectangles(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "checks.json"
            ref_hash, candidate_hash, geometry_hash = "a" * 64, "b" * 64, "c" * 64
            checks = {
                "schema_version": 1,
                "coverage": {
                    "status": "reviewed_complete",
                    "reviewer": "synthetic unit-test reviewer",
                    "reviewer_role": "independent_reviewer",
                    "review_record": "synthetic unit-test review record",
                    "reference_capture_id": "synthetic-unit-capture",
                    "reference_capture_sha256": ref_hash,
                    "candidate_png_sha256": candidate_hash,
                    "candidate_geometry_sha256": geometry_hash,
                    "approved_masks_sha256": None,
                },
                "components": [{
                    "id": "unit-fixture-only",
                    "reference": {"x": 1, "y": 2, "width": 8, "height": 9},
                }],
                "color_samples": [{"id": "unit-color-only", "reference_xy": [0, 0], "candidate_xy": [0, 0]}],
            }
            path.write_text(json.dumps(checks), encoding="utf-8")
            components, colors, _ = visual_diff._load_checks(path, 16, 16, "synthetic-unit-capture", ref_hash, candidate_hash, geometry_hash, None)
            checks["coverage"]["candidate_geometry_sha256"] = "d" * 64
            path.write_text(json.dumps(checks), encoding="utf-8")
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "do not bind"):
                visual_diff._load_checks(path, 16, 16, "synthetic-unit-capture", ref_hash, candidate_hash, geometry_hash, None)
        self.assertEqual(components[0]["reference"], (1, 2, 8, 9))
        self.assertEqual(colors[0]["id"], "unit-color-only")

    def test_approved_masks_bind_capture_reference_and_reviewed_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "masks.json"
            reference_hash = "a" * 64
            record = {
                "schema_version": 1,
                "review_status": "approved",
                "reviewed_by": "synthetic independent reviewer",
                "reviewed_at": "synthetic timestamp",
                "approval_reference": "synthetic review record",
                "screen_id": "synthetic-screen",
                "capture_id": "synthetic-capture",
                "reference_capture_sha256": reference_hash,
                "masks": [{"id": "unit-mask", "reason": "synthetic fixture", "rect": {"x": 1, "y": 1, "width": 2, "height": 2}}],
            }
            path.write_text(json.dumps(record), encoding="utf-8")
            bitmap, masks, binding = visual_diff._load_masks(path, 16, 16, "synthetic-screen", "synthetic-capture", reference_hash)
            self.assertEqual(len(masks), 1)
            self.assertEqual(binding["approved_masks_sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(binding["reference_capture_sha256"], reference_hash)
            self.assertEqual(bitmap[1 * 16 + 1], 1)
            record["capture_id"] = "different-capture"
            path.write_text(json.dumps(record), encoding="utf-8")
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "not bound"):
                visual_diff._load_masks(path, 16, 16, "synthetic-screen", "synthetic-capture", reference_hash)

    def test_checks_coverage_binds_exact_approved_mask_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "checks.json"
            ref_hash, candidate_hash, geometry_hash, mask_hash = "a" * 64, "b" * 64, "c" * 64, "e" * 64
            coverage = {
                "status": "reviewed_complete", "reviewer": "synthetic reviewer",
                "reviewer_role": "independent_reviewer", "review_record": "synthetic review",
                "reference_capture_id": "synthetic-capture", "reference_capture_sha256": ref_hash,
                "candidate_png_sha256": candidate_hash, "candidate_geometry_sha256": geometry_hash,
                "approved_masks_sha256": mask_hash,
            }
            checks = {"schema_version": 1, "coverage": coverage,
                "components": [{"id": "component", "reference": {"x": 1, "y": 1, "width": 2, "height": 2}}],
                "color_samples": [{"id": "color", "reference_xy": [0, 0], "candidate_xy": [0, 0]}]}
            path.write_text(json.dumps(checks), encoding="utf-8")
            visual_diff._load_checks(path, 16, 16, "synthetic-capture", ref_hash, candidate_hash, geometry_hash, mask_hash)
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "approved_masks_sha256"):
                visual_diff._load_checks(path, 16, 16, "synthetic-capture", ref_hash, candidate_hash, geometry_hash, None)

    def test_runtime_geometry_requires_qt_qml_source_and_exact_state_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "runtime-geometry.json"
            evidence = {
                "schema_version": 1,
                "source": "externally_attested_qt_qml_runtime_geometry_export",
                "build_identity": "d" * 64,
                "fixture_id": "synthetic fixture only",
                "screen_id": "synthetic-screen",
                "capture_id": "synthetic-capture",
                "components": [{"id": "fixture-component", "rect": {"x": 1, "y": 2, "width": 8, "height": 9}}],
            }
            path.write_text(json.dumps(evidence), encoding="utf-8")
            components, provenance = visual_diff._load_runtime_geometry(path, 16, 16, "synthetic-screen", "synthetic-capture")
            evidence["source"] = "caller_entered_coordinates"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "externally attested"):
                visual_diff._load_runtime_geometry(path, 16, 16, "synthetic-screen", "synthetic-capture")
        self.assertEqual(components["fixture-component"], (1, 2, 8, 9))
        self.assertEqual(provenance["source"], "externally_attested_qt_qml_runtime_geometry_export")

    def test_runtime_geometry_build_identity_must_be_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "runtime-geometry.json"
            evidence = {
                "schema_version": 1,
                "source": "externally_attested_qt_qml_runtime_geometry_export",
                "build_identity": "not-a-build-hash",
                "fixture_id": "synthetic fixture only",
                "screen_id": "synthetic-screen",
                "capture_id": "synthetic-capture",
                "components": [{"id": "fixture-component", "rect": {"x": 1, "y": 2, "width": 8, "height": 9}}],
            }
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(visual_diff.VisualDiffError, "build_identity"):
                visual_diff._load_runtime_geometry(path, 16, 16, "synthetic-screen", "synthetic-capture")


if __name__ == "__main__":
    unittest.main()
