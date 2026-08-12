#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Regenerate provider-backed Cinnamon settings choices deterministically."""

from __future__ import annotations

import json
import re
import sys
from collections import OrderedDict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPLET = ROOT / "calendar-plus@the-infiltratr"
SCHEMA = APPLET / "settings-schema.json"


def calendar_options() -> OrderedDict[str, str]:
    source = (ROOT / "src/calendar-registry.c").read_text(encoding="utf-8")
    pattern = re.compile(
        r"(?:ICU|CUSTOM)_PROVIDER\(\s*[A-Z0-9_]+\s*,\s*"
        r'"([^"]+)"\s*,\s*"([^"]+)"'
    )
    values = [(name, ident) for ident, name in pattern.findall(source)]
    if len(values) != 22:
        raise RuntimeError(f"expected 22 calendar providers, found {len(values)}")
    return OrderedDict(values)


def native_time_options() -> OrderedDict[str, str]:
    source = (ROOT / "src/time-formats.c").read_text(encoding="utf-8")
    pattern = re.compile(
        r"TIME_PROVIDER\(\s*[A-Z0-9_]+\s*,\s*[a-z0-9_]+\s*,\s*"
        r'"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(?:TRUE|FALSE)\s*,\s*'
        r"(?:TRUE|FALSE)\s*,\s*(?:TRUE|FALSE)\s*\)",
        re.MULTILINE,
    )
    values = [(name, ident) for ident, name in pattern.findall(source)]
    if len(values) != 13:
        raise RuntimeError(f"expected 13 native time providers, found {len(values)}")
    return OrderedDict(values)


def generated_schema() -> str:
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"), object_pairs_hook=OrderedDict)
    calendars = calendar_options()
    schema["primary-calendar"]["options"] = calendars
    schema["secondary-calendar"]["options"] = OrderedDict(
        [("None", "none"), *calendars.items()]
    )

    conventional = OrderedDict([
        ("Standard time (follow Mint setting)", "standard"),
        ("Standard time (24-hour)", "standard-24"),
        ("Standard time (12-hour)", "standard-12"),
    ])
    schema["clock-mode"]["options"] = OrderedDict(
        [*conventional.items(), *native_time_options().items()]
    )
    return json.dumps(schema, ensure_ascii=False, indent=4) + "\n"


def main() -> None:
    check = sys.argv[1:] == ["--check"]
    if sys.argv[1:] not in ([], ["--check"]):
        raise SystemExit("usage: update-settings.py [--check]")

    output = generated_schema()
    current = SCHEMA.read_text(encoding="utf-8")
    if check:
        if current != output:
            raise SystemExit(
                "settings-schema.json does not match native provider registries; "
                "run tools/update-settings.py"
            )
        return

    SCHEMA.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
