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
APPLET = ROOT / "src/cinnamon"
SCHEMA = APPLET / "settings-schema.json"

# Human-facing clock choices are deliberately grouped by use rather than
# leaking the internal provider/enum registration order into Cinnamon.
TIME_OPTION_ORDER = (
    "internet",
    "unix",
    "binary",
    "hexadecimal",
    "julian",
    "modified-julian",
    "sidereal",
    "solar",
    "mean-solar",
    "decimal",
    "chinese-time",
    "chinese-ke",
    "roman-temporal",
    "japanese-temporal",
    "italian-hours",
    "babylonian-hours",
    "indian-ghati",
    "nuremberg-hours",
)

# Primary and secondary calendar selectors use the same deliberate
# human-facing order. Keep provider IDs/enums stable and group the UI
# from common civil systems through living regional/religious systems,
# then historical/reform calendars.
CALENDAR_OPTION_ORDER = (
    "gregorian",
    "iso-week",
    "julian",
    "revised-julian",
    "hebrew",
    "islamic-umalqura",
    "islamic-civil",
    "islamic-tbla",
    "islamic",
    "persian",
    "bahai",
    "buddhist",
    "coptic",
    "ethiopian",
    "ethiopic-amete-alem",
    "chinese",
    "dangi",
    "indian",
    "japanese",
    "minguo",
    "roman",
    "byzantine",
    "egyptian-nabonassar",
    "armenian-traditional",
    "mayan",
    "french-republican",
    "swedish-historical",
    "international-fixed",
    "world",
    "positivist",
)


def calendar_options() -> OrderedDict[str, str]:
    source = (ROOT / "src/core/calendar-registry.c").read_text(encoding="utf-8")
    pattern = re.compile(
        r"(?:ICU|CUSTOM|SWEDISH)_PROVIDER\(\s*[A-Z0-9_]+\s*,\s*"
        r'"([^"]+)"\s*,\s*"([^"]+)"'
    )
    discovered = {ident: name for ident, name in pattern.findall(source)}
    if len(discovered) != 30:
        raise RuntimeError(
            f"expected 30 calendar providers, found {len(discovered)}"
        )
    if set(discovered) != set(CALENDAR_OPTION_ORDER):
        missing = sorted(set(discovered) - set(CALENDAR_OPTION_ORDER))
        stale = sorted(set(CALENDAR_OPTION_ORDER) - set(discovered))
        raise RuntimeError(
            f"calendar option order is out of sync; "
            f"missing={missing}, stale={stale}"
        )
    return OrderedDict(
        (discovered[ident], ident) for ident in CALENDAR_OPTION_ORDER
    )


def native_time_options() -> OrderedDict[str, str]:
    source = (ROOT / "src/core/time-formats.c").read_text(encoding="utf-8")
    pattern = re.compile(
        r"TIME_PROVIDER\(\s*[A-Z0-9_]+\s*,\s*[a-z0-9_]+\s*,\s*"
        r'"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(?:TRUE|FALSE)\s*,\s*'
        r"(?:TRUE|FALSE)\s*,\s*(?:TRUE|FALSE)\s*\)",
        re.MULTILINE,
    )
    discovered = {ident: name for ident, name in pattern.findall(source)}
    if len(discovered) != 18:
        raise RuntimeError(
            f"expected 18 native time providers, found {len(discovered)}"
        )
    if set(discovered) != set(TIME_OPTION_ORDER):
        missing = sorted(set(discovered) - set(TIME_OPTION_ORDER))
        stale = sorted(set(TIME_OPTION_ORDER) - set(discovered))
        raise RuntimeError(
            f"time option order is out of sync; missing={missing}, stale={stale}"
        )
    return OrderedDict(
        (discovered[ident], ident) for ident in TIME_OPTION_ORDER
    )


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
