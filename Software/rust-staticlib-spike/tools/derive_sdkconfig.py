#!/usr/bin/env python3
"""Derive esp-idf-sys sdkconfig defaults from the prebuilt Arduino core.

The Arduino libraries ship one Kconfig-format `sdkconfig` per chip, built for
the `qio_qspi` memory variant, plus a generated `sdkconfig.h` for each memory
variant. RADR uses `qio_opi`, so this applies the header differences between
`qio_qspi` and the requested variant to the base file. The result lets the
Rust bindings see the same configuration as the libraries they link against.
"""

import argparse
import re
from pathlib import Path

DEFINE = re.compile(r"^#define (CONFIG_\w+) (.+)$")


def header_values(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text().splitlines():
        match = DEFINE.match(line)
        if not match:
            continue
        key, value = match.groups()
        # Compatibility aliases point at another symbol and are not options.
        if value.startswith("CONFIG_"):
            continue
        values[key] = "y" if value == "1" else value
    return values


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("libs", type=Path, help="framework-arduinoespressif32-libs/<chip>")
    parser.add_argument("variant", help="memory variant, e.g. qio_opi")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    base = header_values(args.libs / "qio_qspi" / "include" / "sdkconfig.h")
    target = header_values(args.libs / args.variant / "include" / "sdkconfig.h")
    overrides = {key: value for key, value in target.items() if base.get(key) != value}
    removed = sorted(set(base) - set(target))

    lines = []
    for line in (args.libs / "sdkconfig").read_text().splitlines():
        key = None
        if line.startswith("CONFIG_"):
            key = line.split("=", 1)[0]
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            key = line[2:].split(" ", 1)[0]
        if key in overrides:
            line = f"{key}={overrides.pop(key)}"
        elif key in removed:
            line = f"# {key} is not set"
        lines.append(line)

    if overrides:
        lines.append(f"# {args.variant} options absent from the base sdkconfig")
        lines.extend(f"{key}={value}" for key, value in sorted(overrides.items()))

    args.output.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
