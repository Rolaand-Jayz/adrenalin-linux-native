"""Strict JSON decoding for reference evidence files."""

from __future__ import annotations

import json
from typing import Any


class DuplicateJSONKeyError(ValueError):
    """A JSON object repeats a key and would otherwise be ambiguous."""


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateJSONKeyError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def loads(source: str) -> Any:
    """Decode JSON while rejecting duplicate keys at every object depth."""
    return json.loads(source, object_pairs_hook=_unique_object)
