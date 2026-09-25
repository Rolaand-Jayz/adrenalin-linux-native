#!/usr/bin/env python3
"""Verify the native shell's live Qt Quick accessibility tree through AT-SPI."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from dataclasses import dataclass


@dataclass(frozen=True)
class AccessibleNode:
    name: str
    role: str
    rect: tuple[int, int, int, int]


def _walk(node, depth: int = 0):
    if depth > 32:
        raise RuntimeError("candidate accessibility tree exceeded the traversal depth limit")
    name = node.get_name() or ""
    role = node.get_role_name() or "unknown"
    bounds = node.get_extents(_atspi.CoordType.SCREEN)
    if bounds.width < 0 or bounds.height < 0:
        raise RuntimeError(f"AT-SPI returned invalid extents for {name!r}")
    yield AccessibleNode(name, role, (bounds.x, bounds.y, bounds.width, bounds.height))
    for index in range(node.get_child_count()):
        child = node.get_child_at_index(index)
        if child is not None:
            yield from _walk(child, depth + 1)


def _candidate_app(pid: int, timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        desktop = _atspi.get_desktop(0)
        for index in range(max(0, desktop.get_child_count())):
            app = desktop.get_child_at_index(index)
            if app is not None and app.get_process_id() == pid:
                return app
        time.sleep(0.1)
    raise RuntimeError("candidate process did not appear in the AT-SPI desktop tree")


def verify(shell: str, registry: str, timeout: float) -> None:
    global _atspi
    try:
        import gi

        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi
    except (ImportError, ValueError) as error:
        raise RuntimeError("PyGObject with the Atspi 2.0 typelib is required") from error
    _atspi = Atspi

    env = os.environ.copy()
    env.update(
        {
            "QT_QPA_PLATFORM": "xcb",
            "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
            "LANG": "C.UTF-8",
            "LC_ALL": "C.UTF-8",
        }
    )
    registry_process = subprocess.Popen(
        [registry, "--use-gnome-session"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    shell_process = None
    try:
        if Atspi.init() != 0:
            raise RuntimeError("AT-SPI initialization failed; run inside a D-Bus session")
        shell_process = subprocess.Popen(
            [shell], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        app = _candidate_app(shell_process.pid, timeout)
        windows = [app.get_child_at_index(index) for index in range(app.get_child_count())]
        windows = [window for window in windows if window is not None and window.get_role_name() == "frame"]
        if len(windows) != 1:
            raise RuntimeError(f"expected one candidate window in AT-SPI, found {len(windows)}")
        frame = windows[0]
        frame_rect = frame.get_extents(Atspi.CoordType.SCREEN)
        if frame_rect.width <= 0 or frame_rect.height <= 0:
            raise RuntimeError("candidate frame has empty AT-SPI extents")

        observed = list(_walk(frame))
        by_role_name: dict[tuple[str, str], list[AccessibleNode]] = {}
        for node in observed:
            by_role_name.setdefault((node.role, node.name), []).append(node)

        required = {
            ("panel", "Application header"),
            ("label", "AMD Software"),
            ("label", "Linux"),
            ("heading", "AMD Software for Linux"),
            (
                "label",
                "The native desktop shell is running. Hardware and feature status will appear as their Linux providers become available.",
            ),
            ("panel", "Product telemetry settings"),
            ("label", "Product telemetry participation"),
            ("switch", "Product telemetry participation"),
        }
        missing = sorted(identity for identity in required if not by_role_name.get(identity))
        if missing:
            raise RuntimeError(f"candidate is missing accessible name/role pairs: {missing}")
        ambiguous = sorted(identity for identity in required if len(by_role_name[identity]) != 1)
        if ambiguous:
            raise RuntimeError(f"candidate has ambiguous accessible name/role pairs: {ambiguous}")

        frame_bounds = (frame_rect.x, frame_rect.y, frame_rect.width, frame_rect.height)
        for identity in required:
            node = by_role_name[identity][0]
            x, y, width, height = node.rect
            if width <= 0 or height <= 0:
                raise RuntimeError(f"candidate component has empty extents: {identity}")
            if x < frame_rect.x or y < frame_rect.y or x + width > frame_rect.x + frame_rect.width or y + height > frame_rect.y + frame_rect.height:
                raise RuntimeError(f"candidate component extents escape its frame: {identity} {node.rect} {frame_bounds}")
        print(f"PASS: {len(required)} accessible component identities observed through external AT-SPI")
        print(f"AT-SPI screen-coordinate frame extents: {frame_bounds}")
        for identity in sorted(required):
            print(f"{identity[0]} {identity[1]}: {by_role_name[identity][0].rect}")
    finally:
        if shell_process is not None:
            shell_process.terminate()
            try:
                shell_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                shell_process.kill()
                shell_process.wait()
        registry_process.terminate()
        try:
            registry_process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            registry_process.kill()
            registry_process.wait()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shell", required=True, help="built native shell executable")
    parser.add_argument("--registry", required=True, help="AT-SPI registry daemon executable")
    parser.add_argument("--timeout", type=float, default=10.0, help="seconds to wait for the live accessibility tree")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    try:
        verify(args.shell, args.registry, args.timeout)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
