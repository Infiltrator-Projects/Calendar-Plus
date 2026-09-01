#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Protect the published C ABI from symbol removal or version reassignment.

The baseline records the first symbol-version node assigned to every API that
has shipped through Calendar Plus 3.6.2.  Normal releases may add symbols, but
must not edit historical baseline entries.  The optional library check also
verifies that the linker emitted the recorded GNU symbol versions.
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP = ROOT / "src/abi/calendar-plus.map"
BASELINE = ROOT / "tests/abi-baseline.txt"
NODE_RE = re.compile(r"^(CALENDAR_PLUS_\d+\.\d+)\s*\{")
SYMBOL_RE = re.compile(r"^(calendar_plus_[A-Za-z0-9_]+);$")
DYNAMIC_RE = re.compile(
    r"\b(calendar_plus_[A-Za-z0-9_]+)@@(CALENDAR_PLUS_\d+\.\d+)\b"
)


def parse_map() -> dict[str, str]:
    mapping: dict[str, str] = {}
    node: str | None = None
    in_global = False

    for raw in MAP.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        match = NODE_RE.match(line)
        if match:
            node = match.group(1)
            in_global = False
            continue
        if line == "global:":
            assert node is not None, "global section outside version node"
            in_global = True
            continue
        if line == "local:":
            in_global = False
            continue
        if line.startswith("}"):
            node = None
            in_global = False
            continue
        if not in_global:
            continue
        symbol = SYMBOL_RE.match(line)
        if symbol:
            name = symbol.group(1)
            assert name not in mapping, f"duplicate version assignment: {name}"
            assert node is not None
            mapping[name] = node

    return mapping


def parse_baseline() -> dict[str, str]:
    mapping: dict[str, str] = {}
    for number, raw in enumerate(
        BASELINE.read_text(encoding="utf-8").splitlines(), 1
    ):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        try:
            symbol, node = line.split("@", 1)
        except ValueError as error:
            raise AssertionError(f"invalid ABI baseline line {number}: {line}") from error
        assert symbol.startswith("calendar_plus_"), f"invalid symbol on line {number}"
        assert NODE_RE.match(f"{node} {{"), f"invalid ABI node on line {number}"
        assert symbol not in mapping, f"duplicate ABI baseline symbol: {symbol}"
        mapping[symbol] = node
    assert mapping, "ABI baseline is empty"
    return mapping


def parse_dynamic_symbols(library: Path) -> dict[str, str]:
    output = subprocess.run(
        ["readelf", "--dyn-syms", "--wide", str(library)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout
    return {symbol: node for symbol, node in DYNAMIC_RE.findall(output)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path)
    arguments = parser.parse_args()

    version_map = parse_map()
    baseline = parse_baseline()
    missing_from_map = baseline.keys() - version_map.keys()
    assert not missing_from_map, (
        "published ABI symbols removed from version map: "
        + ", ".join(sorted(missing_from_map))
    )
    moved = {
        symbol: (baseline[symbol], version_map[symbol])
        for symbol in baseline.keys() & version_map.keys()
        if baseline[symbol] != version_map[symbol]
    }
    assert not moved, "published ABI symbols changed version node: " + ", ".join(
        f"{symbol} {old}->{new}" for symbol, (old, new) in sorted(moved.items())
    )
    if arguments.library is not None:
        assert arguments.library.is_file(), f"library not found: {arguments.library}"
        dynamic = parse_dynamic_symbols(arguments.library)
        missing_dynamic = baseline.keys() - dynamic.keys()
        assert not missing_dynamic, (
            "published ABI symbols absent from built library: "
            + ", ".join(sorted(missing_dynamic))
        )
        wrong_dynamic = {
            symbol: (baseline[symbol], dynamic[symbol])
            for symbol in baseline.keys() & dynamic.keys()
            if baseline[symbol] != dynamic[symbol]
        }
        assert not wrong_dynamic, (
            "built library changed published symbol versions: "
            + ", ".join(
                f"{symbol} {old}->{new}"
                for symbol, (old, new) in sorted(wrong_dynamic.items())
            )
        )


if __name__ == "__main__":
    main()
