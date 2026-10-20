#!/usr/bin/env python3
"""Attribute linked bytes in a GNU ld map file to Rust crates.

Counts the input sections that came from libradr_rust.a members, grouped by
crate and by ESP32-S3 memory region (flash, IRAM, DRAM data/bss).
"""

import argparse
import re
from collections import defaultdict
from pathlib import Path

MEMBER = re.compile(r"libradr_rust\.a\(([A-Za-z0-9_]+)-[0-9a-f]+\.")
# " .text.foo  0x420be01c  0xf  path" or a wrapped name on the previous line.
SECTION = re.compile(r"^ (\.\S+)?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")


def region(name: str, address: int) -> str:
    if 0x42000000 <= address < 0x44000000:
        return "flash_text"
    if 0x3C000000 <= address < 0x3E000000:
        return "flash_rodata"
    if 0x40370000 <= address < 0x403E0000:
        return "iram"
    if 0x3FC88000 <= address < 0x3FD00000:
        return "dram_bss" if ".bss" in name else "dram_data"
    return "other"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("map", type=Path)
    args = parser.parse_args()

    totals: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    in_memory_map = False
    pending_name = ""
    for line in args.map.read_text(errors="replace").splitlines():
        if line.startswith("Linker script and memory map"):
            in_memory_map = True
            continue
        if not in_memory_map:
            continue
        if line.startswith(" .") and len(line.split()) == 1:
            pending_name = line.strip()
            continue
        match = SECTION.match(line)
        if not match:
            pending_name = ""
            continue
        name = match.group(1) or pending_name
        pending_name = ""
        address, size, source = int(match.group(2), 16), int(match.group(3), 16), match.group(4)
        member = MEMBER.search(source)
        if not member or size == 0 or address == 0:
            continue
        totals[member.group(1)][region(name, address)] += size

    regions = ["flash_text", "flash_rodata", "iram", "dram_data", "dram_bss", "other"]
    print(f"{'crate':28}" + "".join(f"{r:>14}" for r in regions) + f"{'total':>12}")
    grand = defaultdict(int)
    for crate, sizes in sorted(totals.items(), key=lambda item: -sum(item[1].values())):
        print(f"{crate:28}" + "".join(f"{sizes[r]:>14}" for r in regions) + f"{sum(sizes.values()):>12}")
        for r in regions:
            grand[r] += sizes[r]
    print(f"{'TOTAL':28}" + "".join(f"{grand[r]:>14}" for r in regions) + f"{sum(grand.values()):>12}")


if __name__ == "__main__":
    main()
