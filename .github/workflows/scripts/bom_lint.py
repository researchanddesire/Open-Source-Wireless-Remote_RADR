#!/usr/bin/env python3
"""Validate hardware/**/bom.csv against the vendored RAD BOM schema.

Schema source of truth: researchanddesire/dev-docs schemas/bom.schema.json
(vendored read-only to hardware/bom.schema.json). See
https://dev.researchanddesire.com/meta/bom-standard/

Checks (intentionally minimal — structure, not taste):
  - exact header row (display titles, in order)
  - required fields present (not blank, not the empty marker)
  - integer / number typed fields parse
  - category is one of the closed enum
  - no blank cells: empty values must be written as the en-dash "–"
"""

from __future__ import annotations

import csv
import glob
import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
SCHEMA_PATH = os.path.join(SCRIPT_DIR, "bom.schema.json")


def load_schema(path: str) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def check_value(field: dict, raw: str, missing: list[str]) -> str | None:
    name = field["name"]
    required = field.get("constraints", {}).get("required", False)
    enum = field.get("constraints", {}).get("enum")
    ftype = field.get("type", "string")

    if raw == "":
        return f"{name}: blank cell — use '–' for an empty value"

    is_missing = raw in missing
    if is_missing:
        if required:
            return f"{name}: required, but is empty ('{raw}')"
        return None  # legitimately empty optional field

    if ftype == "integer":
        try:
            int(raw)
        except ValueError:
            return f"{name}: '{raw}' is not an integer"
    elif ftype == "number":
        try:
            float(raw)
        except ValueError:
            return f"{name}: '{raw}' is not a number"

    if enum is not None and raw not in enum:
        return f"{name}: '{raw}' is not a valid code (closed set)"

    return None


def lint_file(path: str, fields: list[dict], expected_header: list[str], missing: list[str]) -> list[str]:
    errors: list[str] = []
    rel = os.path.relpath(path, REPO_ROOT)
    with open(path, newline="", encoding="utf-8") as fh:
        rows = list(csv.reader(fh))

    if not rows:
        return [f"{rel}: file is empty (missing header row)"]

    header = rows[0]
    if header != expected_header:
        errors.append(
            f"{rel}: header mismatch\n  expected: {','.join(expected_header)}\n  found:    {','.join(header)}"
        )
        return errors  # header wrong → row checks would be noise

    ncols = len(expected_header)
    for i, row in enumerate(rows[1:], start=2):
        if not any(cell.strip() for cell in row):
            continue  # tolerate stray blank line
        if len(row) != ncols:
            errors.append(f"{rel}:{i}: expected {ncols} columns, found {len(row)}")
            continue
        for field, raw in zip(fields, row):
            msg = check_value(field, raw, missing)
            if msg:
                errors.append(f"{rel}:{i}: {msg}")
    return errors


def main() -> int:
    if not os.path.exists(SCHEMA_PATH):
        print(f"ERROR: schema not found at {SCHEMA_PATH}", file=sys.stderr)
        return 2

    schema = load_schema(SCHEMA_PATH)
    fields = schema["fields"]
    expected_header = [f["title"] for f in fields]
    missing = schema.get("missingValues", [""])

    bom_files = sorted(glob.glob(os.path.join(REPO_ROOT, "hardware", "**", "bom.csv"), recursive=True))
    if not bom_files:
        print("No hardware/**/bom.csv found — nothing to lint.")
        return 0

    all_errors: list[str] = []
    for path in bom_files:
        errs = lint_file(path, fields, expected_header, missing)
        rel = os.path.relpath(path, REPO_ROOT)
        if errs:
            all_errors.extend(errs)
        else:
            print(f"OK  {rel}")

    if all_errors:
        print("\nBOM lint failed:\n", file=sys.stderr)
        for e in all_errors:
            print(f"  - {e}", file=sys.stderr)
        print(
            "\nSee the BOM standard: https://dev.researchanddesire.com/meta/bom-standard/",
            file=sys.stderr,
        )
        return 1

    print("\nAll BOMs conform to the standard.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
