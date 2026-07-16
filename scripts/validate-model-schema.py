#!/usr/bin/env python3
"""Validate the shipped model manifests against the frozen JSON Schema."""

from __future__ import annotations

import json
from pathlib import Path

from jsonschema import Draft202012Validator


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = PROJECT_ROOT / "models" / "model-manifest.schema.json"
MANIFEST_PATHS = (
    PROJECT_ROOT / "models" / "example" / "model.json",
    PROJECT_ROOT / "models" / "ppocrv6-tiny" / "model.json",
)


def load_json(path: Path) -> object:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def main() -> int:
    schema = load_json(SCHEMA_PATH)
    Draft202012Validator.check_schema(schema)
    validator = Draft202012Validator(schema)

    failures = 0
    for manifest_path in MANIFEST_PATHS:
        errors = sorted(
            validator.iter_errors(load_json(manifest_path)),
            key=lambda error: list(error.absolute_path),
        )
        if not errors:
            print(f"PASS: {manifest_path.relative_to(PROJECT_ROOT)}")
            continue

        failures += 1
        print(f"FAIL: {manifest_path.relative_to(PROJECT_ROOT)}")
        for error in errors:
            location = "/".join(str(part) for part in error.absolute_path) or "<root>"
            print(f"  {location}: {error.message}")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
