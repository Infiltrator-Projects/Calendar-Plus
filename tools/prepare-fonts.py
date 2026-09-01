#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith
"""Verify and materialise Calendar Plus's supplied MB Corpo fonts."""

from __future__ import annotations

import argparse
import hashlib
import os
import tarfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ARCHIVE = ROOT / "assets/fonts/mb-corpo-fonts.tar.xz"
DEFAULT_OUTPUT = ROOT / "build/fonts"
ARCHIVE_SHA256 = "bdb6063f838a7fab22b4d6b412170640c69511df53aa3dfa9a4ea8431c9d8274"
FONT_SHA256 = {
    "mb_corpo_a_cond_regular.ttf": "c8bcd7e1a7d71169b38491d9b7c1ffe7ba7b46e888f0c1219931343a47bc0e05",
    "mb_corpo_s_bold.ttf": "d37ea986e2344d83390f94f170e6272b56efd00bfec808afe8314c4ca45d43b4",
    "mb_corpo_s_regular.ttf": "94ede6629443c03d4362dcef425fb3ff520be5d654370021a34e81286804465c",
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_archive(archive: Path) -> dict[str, bytes]:
    if not archive.is_file():
        raise SystemExit(f"required font archive is missing: {archive}")
    if sha256_file(archive) != ARCHIVE_SHA256:
        raise SystemExit("MB Corpo font archive hash mismatch")

    payloads: dict[str, bytes] = {}
    with tarfile.open(archive, mode="r:xz") as bundle:
        members = bundle.getmembers()
        names = [member.name for member in members]
        if set(names) != set(FONT_SHA256) or len(names) != len(FONT_SHA256):
            raise SystemExit(
                "MB Corpo archive must contain exactly the three approved font files"
            )
        for member in members:
            if not member.isfile() or Path(member.name).name != member.name:
                raise SystemExit(f"unsafe font archive member: {member.name}")
            extracted = bundle.extractfile(member)
            if extracted is None:
                raise SystemExit(f"unable to read font archive member: {member.name}")
            data = extracted.read()
            if sha256_bytes(data) != FONT_SHA256[member.name]:
                raise SystemExit(f"MB Corpo font hash mismatch: {member.name}")
            payloads[member.name] = data
    return payloads


def materialise(output: Path, payloads: dict[str, bytes]) -> None:
    output.mkdir(parents=True, exist_ok=True)
    for stale in output.glob("*.ttf"):
        stale.unlink()
    for name, data in sorted(payloads.items()):
        target = output / name
        target.write_bytes(data)
        os.chmod(target, 0o644)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()

    payloads = verify_archive(args.archive)
    if not args.verify_only:
        materialise(args.output, payloads)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
