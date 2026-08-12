#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
UUID=calendar-plus@the-infiltratr
EXPECTED_VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
APPLET_DIR="/usr/share/cinnamon/applets/$UUID"
ABOUT=/usr/libexec/calendar-plus-about

case "${XDG_CURRENT_DESKTOP:-}" in
    *Cinnamon*) ;;
    *)
        echo "This integration check must run inside a Cinnamon session." >&2
        exit 1
        ;;
esac

[ -d "$APPLET_DIR" ] || {
    echo "Installed applet not found: $APPLET_DIR" >&2
    exit 1
}
[ -x "$ABOUT" ] || {
    echo "Installed About helper not found: $ABOUT" >&2
    exit 1
}

python3 - "$APPLET_DIR" "$EXPECTED_VERSION" <<'PY'
import json
import sys
from pathlib import Path

applet = Path(sys.argv[1])
expected = sys.argv[2]
metadata = json.loads((applet / "metadata.json").read_text(encoding="utf-8"))
if metadata.get("version") != expected:
    raise SystemExit(
        f"Installed metadata version mismatch: {metadata.get('version')} != {expected}"
    )
PY

while read -r expected relative; do
    case "$relative" in
        "$UUID"/*)
            installed="$APPLET_DIR/${relative#"$UUID"/}"
            [ -f "$installed" ] || {
                echo "Installed runtime file missing: $installed" >&2
                exit 1
            }
            actual=$(sha256sum "$installed" | awk '{print $1}')
            [ "$actual" = "$expected" ] || {
                echo "Installed runtime hash mismatch: $installed" >&2
                exit 1
            }
            ;;
    esac
done < "$ROOT/runtime-sources.sha256"

"$ABOUT" --print-metadata | grep -qx "version=$EXPECTED_VERSION"

CALENDAR_PLUS_EXPECTED_VERSION="$EXPECTED_VERSION" \
    gjs "$ROOT/tests/smoke-typelib.js" >/dev/null

ENABLED=$(gsettings get org.cinnamon enabled-applets)
case "$ENABLED" in
    *"$UUID"*) ;;
    *)
        echo "Calendar Plus is installed but is not enabled on a panel." >&2
        exit 1
        ;;
esac

if [ -f "$HOME/.xsession-errors" ]; then
    if tail -n 1000 "$HOME/.xsession-errors" | \
        grep -E "\[$UUID\].*(Failed to load|Could not create)|$UUID.*(ReferenceError|TypeError)" >/dev/null; then
        echo "A Calendar Plus load error is present in the current session log." >&2
        exit 1
    fi
fi

printf 'Calendar Plus %s passed installed hashes, native providers and Cinnamon enablement checks.\n' \
    "$EXPECTED_VERSION"
