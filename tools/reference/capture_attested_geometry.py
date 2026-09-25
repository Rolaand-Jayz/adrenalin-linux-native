"""Capture a live X11 candidate and export geometry observed through AT-SPI.

This is an external candidate-evidence exporter. It does not establish reference
authenticity, human review, or visual parity. It supports X11 only and fails
closed when the screenshot and AT-SPI coordinate spaces cannot be matched.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image


REQUIRED_COMPONENTS = {
    ("panel", "Application header"): "header",
    ("label", "AMD Software"): "brandMark",
    ("label", "Linux"): "platformLabel",
    ("heading", "AMD Software for Linux"): "screenTitle",
    (
        "label",
        "The native desktop shell is running. Hardware and feature status will appear as their Linux providers become available.",
    ): "screenDescription",
    ("panel", "Product telemetry settings"): "telemetryPreferenceRow",
    ("label", "Product telemetry participation"): "telemetryPreferenceLabel",
    ("switch", "Product telemetry participation"): "productTelemetryConsentSwitch",
}
MAX_CAPTURE_WIDTH = 3840
MAX_CAPTURE_HEIGHT = 2160
MAX_CAPTURE_PIXELS = MAX_CAPTURE_WIDTH * MAX_CAPTURE_HEIGHT


class ProbeError(RuntimeError):
    pass


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _atspi_components(frame, atspi):
    observed: dict[tuple[str, str], list[tuple[int, int, int, int]]] = {}

    def walk(node, depth=0):
        if depth > 32:
            raise ProbeError("candidate accessibility tree exceeded the traversal depth limit")
        bounds = node.get_extents(atspi.CoordType.SCREEN)
        if bounds.width < 0 or bounds.height < 0:
            raise ProbeError("AT-SPI returned invalid screen extents")
        key = (node.get_role_name() or "unknown", node.get_name() or "")
        observed.setdefault(key, []).append((bounds.x, bounds.y, bounds.width, bounds.height))
        for index in range(node.get_child_count()):
            child = node.get_child_at_index(index)
            if child is not None:
                walk(child, depth + 1)

    walk(frame)
    components = {}
    for accessible_key, component_id in REQUIRED_COMPONENTS.items():
        matches = observed.get(accessible_key, [])
        if len(matches) != 1:
            raise ProbeError(f"expected one externally observed component {accessible_key}, found {len(matches)}")
        x, y, width, height = matches[0]
        if width <= 0 or height <= 0:
            raise ProbeError(f"component has empty AT-SPI extents: {component_id}")
        components[component_id] = (x, y, width, height)
    return components


def _atomic_write(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(handle, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _load_atspi():
    try:
        import gi

        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi
    except (ImportError, ValueError) as error:
        raise ProbeError("PyGObject with the Atspi 2.0 typelib is required") from error
    return Atspi


def _window_for_process(pid: int, expected_rect: tuple[int, int, int, int]) -> int:
    try:
        from Xlib import X, display
    except ImportError as error:
        raise ProbeError("python-xlib is required to bind the screenshot to the candidate X11 window") from error

    connection = display.Display()
    root = connection.screen().root
    pid_atom = connection.intern_atom("_NET_WM_PID")
    matches = []

    def visit(parent, root_child, depth=0):
        if depth > 32:
            raise ProbeError("X11 window tree exceeded the traversal depth limit")
        for window in parent.query_tree().children:
            top_level = window if parent.id == root.id else root_child
            attributes = window.get_attributes()
            if attributes.map_state == X.IsViewable:
                prop = window.get_full_property(pid_atom, X.AnyPropertyType)
                if prop is not None and len(prop.value) == 1 and int(prop.value[0]) == pid:
                    origin = root.translate_coords(window, 0, 0)
                    geometry = window.get_geometry()
                    rect = (origin.x, origin.y, geometry.width, geometry.height)
                    if rect == expected_rect:
                        matches.append((window, top_level, rect))
            visit(window, top_level, depth + 1)

    visit(root, root)
    if len(matches) != 1:
        connection.close()
        raise ProbeError(f"expected one mapped candidate X11 window matching AT-SPI extents, found {len(matches)}")

    candidate_window, top_level, rect = matches[0]
    root_children = root.query_tree().children
    root_geometry = root.get_geometry()
    if (
        rect[0] < 0
        or rect[1] < 0
        or rect[0] + rect[2] > root_geometry.width
        or rect[1] + rect[3] > root_geometry.height
    ):
        connection.close()
        raise ProbeError("candidate X11 window is not fully visible within the display bounds")
    try:
        stack_index = next(index for index, window in enumerate(root_children) if window.id == top_level.id)
    except StopIteration as error:
        connection.close()
        raise ProbeError("candidate X11 top-level window is absent from the display stack") from error

    x, y, width, height = rect
    for sibling in root_children[stack_index + 1 :]:
        if sibling.id == top_level.id or sibling.get_attributes().map_state != X.IsViewable:
            continue
        origin = root.translate_coords(sibling, 0, 0)
        geometry = sibling.get_geometry()
        border = geometry.border_width
        other = (origin.x - border, origin.y - border,
                 geometry.width + 2 * border, geometry.height + 2 * border)
        if x < other[0] + other[2] and other[0] < x + width and y < other[1] + other[3] and other[1] < y + height:
            connection.close()
            raise ProbeError("another mapped X11 window overlaps the candidate frame")
    xid = candidate_window.id
    connection.close()
    return xid


def capture(args) -> tuple[str, str]:
    if not os.environ.get("DISPLAY"):
        raise ProbeError("an X11 DISPLAY is required; Wayland-native capture is not supported")
    screenshot_tool = shutil.which(args.screenshot_tool)
    if screenshot_tool is None:
        raise ProbeError(f"screenshot tool is unavailable: {args.screenshot_tool}")
    shell = Path(args.shell).resolve(strict=True)
    if not shell.is_file() or not os.access(shell, os.X_OK):
        raise ProbeError("candidate shell must be an executable file")
    image_path = Path(args.candidate).resolve()
    geometry_path = Path(args.geometry).resolve()
    if image_path == geometry_path:
        raise ProbeError("candidate PNG and geometry output paths must differ")
    if image_path == shell or geometry_path == shell:
        raise ProbeError("output paths must not replace the candidate executable")
    if not args.overwrite and (image_path.exists() or geometry_path.exists()):
        raise ProbeError("output files already exist; choose new paths or pass --overwrite")
    for output_path in (image_path, geometry_path):
        if output_path.exists() and os.path.samefile(output_path, shell):
            raise ProbeError("output files must not alias the candidate executable")

    env = os.environ.copy()
    env.update({
        "QT_QPA_PLATFORM": "xcb",
        "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
        "GDK_BACKEND": "x11",
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
    })
    registry = subprocess.Popen(
        [args.registry, "--use-gnome-session"], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    candidate = None
    try:
        atspi = _load_atspi()
        if atspi.init() != 0:
            raise ProbeError("AT-SPI initialization failed; run within a D-Bus session")
        candidate = subprocess.Popen([str(shell)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.monotonic() + args.timeout
        app = None
        frame = None
        while time.monotonic() < deadline:
            if candidate.poll() is not None:
                raise ProbeError(f"candidate exited before observation with status {candidate.returncode}")
            desktop = atspi.get_desktop(0)
            for index in range(max(0, desktop.get_child_count())):
                app_candidate = desktop.get_child_at_index(index)
                if app_candidate is not None and app_candidate.get_process_id() == candidate.pid:
                    windows = [
                        app_candidate.get_child_at_index(i)
                        for i in range(app_candidate.get_child_count())
                    ]
                    frames = [w for w in windows if w is not None and w.get_role_name() == "frame"]
                    if len(frames) == 1:
                        app, frame = app_candidate, frames[0]
                        break
            if frame is not None:
                break
            time.sleep(0.1)
        if frame is None:
            raise ProbeError("candidate did not expose exactly one AT-SPI frame before timeout")

        components_before = _atspi_components(frame, atspi)
        bounds_before = frame.get_extents(atspi.CoordType.SCREEN)
        frame_rect = (bounds_before.x, bounds_before.y, bounds_before.width, bounds_before.height)
        if bounds_before.width <= 0 or bounds_before.height <= 0:
            raise ProbeError("candidate frame has empty AT-SPI screen extents")
        window_id = _window_for_process(candidate.pid, frame_rect)

        with tempfile.TemporaryDirectory(prefix="adrenalin-geometry-") as temporary_directory:
            window_capture = Path(temporary_directory) / "candidate-window.png"
            result = subprocess.run(
                [screenshot_tool, "-window", f"0x{window_id:x}", "-silent", str(window_capture)],
                env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=False,
                timeout=args.timeout,
            )
            if result.returncode != 0 or not window_capture.is_file():
                raise ProbeError("external X11 candidate-window capture failed")
            with Image.open(window_capture) as desktop_image:
                desktop_image.load()
                if desktop_image.size != (frame_rect[2], frame_rect[3]):
                    raise ProbeError("captured X11 window dimensions do not match AT-SPI frame extents")
                if (
                    frame_rect[2] > MAX_CAPTURE_WIDTH
                    or frame_rect[3] > MAX_CAPTURE_HEIGHT
                    or frame_rect[2] * frame_rect[3] > MAX_CAPTURE_PIXELS
                ):
                    raise ProbeError("candidate frame exceeds the ID-137 physical-pixel capture envelope")
                crop = desktop_image.convert("RGBA")
                png_stream = io.BytesIO()
                crop.save(png_stream, format="PNG")
                png_bytes = png_stream.getvalue()

        if _window_for_process(candidate.pid, frame_rect) != window_id:
            raise ProbeError("candidate X11 window identity or visibility changed during capture")
        bounds_after = frame.get_extents(atspi.CoordType.SCREEN)
        components_after = _atspi_components(frame, atspi)
        if candidate.poll() is not None:
            raise ProbeError("candidate exited during external capture")
        if (bounds_after.x, bounds_after.y, bounds_after.width, bounds_after.height) != frame_rect:
            raise ProbeError("candidate frame moved or resized during external capture")
        if components_after != components_before:
            raise ProbeError("candidate component geometry changed during external capture")

        # Linux procfs exposes the running executable for this PID; hashing that
        # descriptor avoids trusting a separately supplied binary path.
        build_identity = _sha256_file(Path(f"/proc/{candidate.pid}/exe"))
        relative = {
            component_id: {
                "x": x - frame_rect[0],
                "y": y - frame_rect[1],
                "width": width,
                "height": height,
            }
            for component_id, (x, y, width, height) in components_before.items()
        }
        for identity, rect in relative.items():
            if rect["x"] < 0 or rect["y"] < 0 or rect["x"] + rect["width"] > frame_rect[2] or rect["y"] + rect["height"] > frame_rect[3]:
                raise ProbeError(f"component escapes the externally captured frame: {identity}")

        artifact = {
            "schema_version": 1,
            "source": "externally_attested_qt_qml_runtime_geometry_export",
            "build_identity": build_identity,
            "fixture_id": args.fixture_id,
            "screen_id": args.screen_id,
            "capture_id": args.capture_id,
            "components": [{"id": key, "rect": value} for key, value in sorted(relative.items())],
        }
        _atomic_write(image_path, png_bytes)
        _atomic_write(geometry_path, (json.dumps(artifact, indent=2, sort_keys=True) + "\n").encode())
        return hashlib.sha256(png_bytes).hexdigest(), hashlib.sha256(
            (json.dumps(artifact, indent=2, sort_keys=True) + "\n").encode()
        ).hexdigest()
    finally:
        if candidate is not None:
            candidate.terminate()
            try:
                candidate.wait(timeout=3)
            except subprocess.TimeoutExpired:
                candidate.kill()
                candidate.wait()
        registry.terminate()
        try:
            registry.wait(timeout=3)
        except subprocess.TimeoutExpired:
            registry.kill()
            registry.wait()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shell", required=True, help="built native candidate executable")
    parser.add_argument("--registry", required=True, help="AT-SPI registry executable")
    parser.add_argument("--screenshot-tool", default="import", help="ImageMagick import executable selected from PATH")
    parser.add_argument("--candidate", required=True, help="output candidate PNG path")
    parser.add_argument("--geometry", required=True, help="output comparator-schema geometry JSON path")
    parser.add_argument("--screen-id", required=True, help="operator-supplied screen identity")
    parser.add_argument("--capture-id", required=True, help="operator-supplied capture/state identity")
    parser.add_argument("--fixture-id", required=True, help="operator-supplied candidate fixture identity")
    parser.add_argument("--overwrite", action="store_true", help="replace existing output files")
    parser.add_argument("--timeout", type=float, default=10.0, help="seconds to wait for candidate accessibility")
    args = parser.parse_args()
    if args.timeout <= 0 or not all((args.screen_id.strip(), args.capture_id.strip(), args.fixture_id.strip())):
        parser.error("timeout and all capture identities must be nonempty")
    try:
        candidate_sha, geometry_sha = capture(args)
    except Exception as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS: externally observed candidate geometry bound to one live X11 capture")
    print(f"candidate_png_sha256={candidate_sha}")
    print(f"candidate_geometry_sha256={geometry_sha}")
    print("This does not establish authentic reference evidence, independent human review, or visual parity.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
