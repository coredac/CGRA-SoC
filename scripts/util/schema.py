"""Small helpers for YAML schema and path validation in repository scripts."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path
from typing import Any

import yaml


_MISSING = object()


def _field_label(key: str, context: object | None) -> str:
    if context is None:
        return key
    if isinstance(context, Path):
        return f"{context}: '{key}'"
    return f"{context}.{key}"


def load_yaml_mapping(path: Path) -> Mapping[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        data = yaml.safe_load(stream)
    if not isinstance(data, Mapping):
        raise ValueError(f"YAML must contain a top-level mapping: {path}")
    return data


def require_mapping(
    data: Mapping[str, Any],
    key: str,
    context: object | None = None,
    *,
    default: Any = _MISSING,
) -> Mapping[str, Any]:
    value = data.get(key, default)
    label = _field_label(key, context)
    if value is _MISSING:
        raise ValueError(f"{label} is required")
    if not isinstance(value, Mapping):
        raise ValueError(f"{label} must be a mapping")
    return value


def require_int(
    data: Mapping[str, Any],
    key: str,
    context: object | None = None,
    *,
    default: Any = _MISSING,
) -> int:
    value = data.get(key, default)
    label = _field_label(key, context)
    if value is _MISSING:
        raise ValueError(f"{label} is required")
    if type(value) is not int:
        raise TypeError(f"{label} must be an integer, got {value!r}")
    return value


def require_string(
    data: Mapping[str, Any],
    key: str,
    context: object | None = None,
    *,
    default: Any = _MISSING,
) -> str:
    value = data.get(key, default)
    label = _field_label(key, context)
    if value is _MISSING:
        raise ValueError(f"{label} is required")
    if not isinstance(value, str) or not value:
        raise ValueError(f"{label} must be a non-empty string")
    return value


def ensure_file(path: Path, label: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"{label} not found: {path}")
    return path


def ensure_dir(path: Path, label: str) -> Path:
    if not path.is_dir():
        raise FileNotFoundError(f"{label} not found: {path}")
    return path
