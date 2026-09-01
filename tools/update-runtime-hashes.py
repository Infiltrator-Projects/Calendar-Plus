#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith
"""Generate or verify the small runtime integrity manifest.

The manifest deliberately covers files loaded directly by Cinnamon or by the
Calendar Plus settings host before or alongside the native library: JavaScript,
runtime JSON, the applet stylesheet and the thin settings launcher. Source,
tests and packaging are covered by the immutable Git tag and GitHub source
archives.
Discovering JavaScript files here prevents a newly split module from being
silently omitted from the integrity gate.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPLET = ROOT / "src/cinnamon"
MANIFEST = APPLET / "runtime-sources.sha256"


def runtime_files() -> list[Path]:
    files = sorted(APPLET.glob("*.js"), key=lambda p: p.name)
    files.extend(
        APPLET / name
        for name in (
            "metadata.json",
            "settings-schema.json",
            "settings.py",
            "stylesheet.css",
        )
    )
    return files


def render() -> str:
    lines: list[str] = []
    for path in runtime_files():
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        relative = path.relative_to(ROOT).as_posix()
        lines.append(f"{digest}  {relative}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = render()
    if args.check:
        actual = MANIFEST.read_text(encoding="utf-8") if MANIFEST.exists() else ""
        if actual != expected:
            raise SystemExit(
                "src/cinnamon/runtime-sources.sha256 is stale; run make update-runtime-hashes"
            )
        return 0
    MANIFEST.write_text(expected, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
