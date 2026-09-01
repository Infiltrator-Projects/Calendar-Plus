#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Regenerate Calendar Plus's gettext template deterministically."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PO_DIR = ROOT / "src/i18n"
APPLET_DIR = ROOT / "src/cinnamon"



def make_value(name: str) -> str:
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    match = re.search(rf"^{re.escape(name)}\s*(?::=|\?=)\s*(.+)$", makefile, re.MULTILINE)
    if match is None:
        raise RuntimeError(f"Makefile does not define {name}")
    return match.group(1).strip()


def release_identity() -> tuple[str, str, str]:
    version = make_value("VERSION")
    epoch = make_value("SOURCE_DATE_EPOCH")
    stamp = datetime.fromtimestamp(int(epoch), timezone.utc).strftime(
        "%Y-%m-%d %H:%M+0000"
    )
    return version, epoch, stamp


def c_quote(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def settings_strings() -> list[tuple[str, str]]:
    values: list[tuple[str, str]] = []
    schema = json.loads((APPLET_DIR / "settings-schema.json").read_text())
    metadata = json.loads((APPLET_DIR / "metadata.json").read_text())

    for key in ("name", "description"):
        values.append(("metadata.json", metadata[key]))

    for setting_name, setting in schema.items():
        for key in ("description", "tooltip"):
            value = setting.get(key)
            if isinstance(value, str):
                values.append((f"settings-schema.json:{setting_name}", value))
        options = setting.get("options", {})
        for label in options:
            values.append((f"settings-schema.json:{setting_name}", label))

    return sorted(set(values))


def run(*args: str) -> None:
    _, epoch, _ = release_identity()
    environment = os.environ.copy()
    environment.update({"SOURCE_DATE_EPOCH": epoch, "TZ": "UTC"})
    subprocess.run(args, cwd=ROOT, env=environment, check=True)


def main() -> None:
    check_only = sys.argv[1:] == ["--check"]
    if sys.argv[1:] not in ([], ["--check"]):
        raise SystemExit("usage: update-translations.py [--check]")

    tracked = [PO_DIR / "calendar-plus.pot"]
    languages = [
        line.strip()
        for line in (PO_DIR / "LINGUAS").read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]
    tracked.extend(PO_DIR / f"{language}.po" for language in languages)
    original = {
        path: path.read_bytes() if path.exists() else None
        for path in tracked
    }

    PO_DIR.mkdir(exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".c",
        encoding="utf-8",
        delete=False,
    ) as temporary:
        temporary.write(
            "#define N_(text) text\n\n"
            + "\n".join(
                f"/* {source} */\nN_({c_quote(text)});"
                for source, text in settings_strings()
            )
            + "\n"
        )
        settings_source = Path(temporary.name)

    c_pot = PO_DIR / ".calendar-plus-c.pot"
    js_pot = PO_DIR / ".calendar-plus-js.pot"
    output = PO_DIR / "calendar-plus.pot"
    c_files = [
        str(path.relative_to(ROOT))
        for source_dir in (
            ROOT / "src/app",
            ROOT / "src/core",
            ROOT / "src/adapters",
        )
        for path in sorted(source_dir.glob("*.c"))
    ] + [str(settings_source)]
    js_files = [
        str(path.relative_to(ROOT))
        for path in sorted(APPLET_DIR.glob("*.js"))
    ]

    version, _, release_stamp = release_identity()
    common = (
        "--from-code=UTF-8",
        "--package-name=Calendar Plus",
        f"--package-version={version}",
        "--no-location",
        "--copyright-holder=Shannon Smith",
        "--no-wrap",
        "--sort-output",
    )
    try:
        run("xgettext", *common, "--language=C", "--keyword=_", "--keyword=N_",
            "--output", str(c_pot.relative_to(ROOT)), *c_files)
    finally:
        settings_source.unlink(missing_ok=True)
    run("xgettext", *common, "--language=JavaScript", "--keyword",
        "--keyword=CP_",
        "--output", str(js_pot.relative_to(ROOT)), *js_files)
    run("msgcat", "--use-first", "--sort-output", "--no-wrap",
        "--output", str(output.relative_to(ROOT)),
        str(c_pot.relative_to(ROOT)), str(js_pot.relative_to(ROOT)))
    c_pot.unlink()
    js_pot.unlink()
    template = output.read_text(encoding="utf-8")
    if not template.startswith("# SPDX-License-Identifier: GPL-3.0-or-later\n"):
        template = "# SPDX-License-Identifier: GPL-3.0-or-later\n" + template
    template = re.sub(
        r'"POT-Creation-Date: [^\\]+\\n"',
        lambda _: f'"POT-Creation-Date: {release_stamp}\\n"',
        template,
        count=1,
    )
    output.write_text(template, encoding="utf-8")

    for language in languages:
        language_path = PO_DIR / f"{language}.po"
        if language.startswith("en"):
            run("msgen", "--lang", language, "--no-wrap", "--sort-output",
                "--output", str(language_path.relative_to(ROOT)),
                str(output.relative_to(ROOT)))
            english = language_path.read_text(encoding="utf-8")
            if not english.startswith("# SPDX-License-Identifier: GPL-3.0-or-later\n"):
                english = "# SPDX-License-Identifier: GPL-3.0-or-later\n" + english
            english = english.replace("#, fuzzy\nmsgid \"\"", "msgid \"\"", 1)
            english = english.replace(
                '"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n"',
                f'"PO-Revision-Date: {release_stamp}\\n"',
            )
            english = english.replace(
                '"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n"',
                '"Last-Translator: Shannon Smith\\n"',
            )
            english = english.replace(
                '"Language-Team: LANGUAGE <LL@li.org>\\n"',
                f'"Language-Team: {language}\\n"',
            )
            language_path.write_text(english, encoding="utf-8")
            continue

        # Keep every maintained non-English catalogue structurally aligned
        # with the generated POT. msgmerge preserves translator text while
        # adding new entries and marking obsolete ones deterministically.
        if not language_path.exists():
            raise RuntimeError(
                f"missing maintained translation catalogue: {language_path}"
            )
        cleaned = PO_DIR / f".{language}.no-fuzzy.po"
        run("msgattrib", "--no-fuzzy", "--no-wrap", "--sort-output",
            "--output", str(cleaned.relative_to(ROOT)),
            str(language_path.relative_to(ROOT)))
        cleaned.replace(language_path)
        run("msgmerge", "--update", "--backup=none",
            "--no-fuzzy-matching", "--no-wrap", "--sort-output",
            str(language_path.relative_to(ROOT)),
            str(output.relative_to(ROOT)))

        # A maintained catalogue must be genuinely translated, not merely
        # structurally merged. msgfmt's statistics understand multiline PO
        # entries and therefore avoid a fragile home-grown PO parser.
        statistics = subprocess.run(
            ("msgfmt", "--statistics", "--check",
             "--output-file=/dev/null",
             str(language_path.relative_to(ROOT))),
            cwd=ROOT,
            env={**os.environ, "SOURCE_DATE_EPOCH": release_identity()[1],
                 "TZ": "UTC"},
            check=True,
            capture_output=True,
            text=True,
        )
        report = statistics.stderr + statistics.stdout
        match = re.search(r"(\d+) untranslated messages?", report)
        if match is not None and int(match.group(1)) != 0:
            raise RuntimeError(
                f"{language_path.relative_to(ROOT)} has "
                f"{match.group(1)} untranslated messages"
            )

    if check_only:
        changed = [
            path.relative_to(ROOT)
            for path in tracked
            if original[path] != (path.read_bytes() if path.exists() else None)
        ]
        for path, contents in original.items():
            if contents is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(contents)
        if changed:
            print("Translation files require regeneration:", file=sys.stderr)
            for path in changed:
                print(f"  {path}", file=sys.stderr)
            raise SystemExit(1)


if __name__ == "__main__":
    main()
