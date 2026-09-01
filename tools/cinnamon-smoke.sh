#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
UUID=calendar-plus@the-infiltratr
SOURCE_DIR=src/cinnamon
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
        "$SOURCE_DIR"/*)
            installed="$APPLET_DIR/${relative#"$SOURCE_DIR"/}"
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
done < "$ROOT/$SOURCE_DIR/runtime-sources.sha256"

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

command -v gdbus >/dev/null 2>&1 || {
    echo "gdbus is required for live Cinnamon qualification." >&2
    exit 1
}

EVAL_SCRIPT=$(cat <<EOF
(() => {
    const applets = AppletManager.getRunningInstancesForUuid("$UUID");
    if (!applets || applets.length === 0)
        throw new Error("Calendar Plus has no running Cinnamon instance");

    for (const applet of applets) {
        if (!applet || applet._destroyed)
            throw new Error("Calendar Plus instance is not healthy");

        applet._updateClockAndDate();
        applet._resetCalendar();

        applet._is_entered = true;
        applet._configureWallClock();
        applet._updatePanelClock();
        applet._is_entered = false;
        applet._configureWallClock();

        applet._onResume();
        applet._rebalancePopupWidth();

        if (applet.menu) {
            applet.menu.open();
            applet.menu.close();
        }
    }

    return { count: applets.length, healthy: true };
})()
EOF
)

EVAL_RESULT=$(gdbus call --session \
    --dest org.Cinnamon \
    --object-path /org/Cinnamon \
    --method org.Cinnamon.Eval \
    "$EVAL_SCRIPT") || {
        echo "Calendar Plus live Cinnamon evaluation failed." >&2
        exit 1
    }

printf '%s\n' "$EVAL_RESULT" | grep -Eq '^\(true,' || {
    echo "Calendar Plus live Cinnamon evaluation returned failure: $EVAL_RESULT" >&2
    exit 1
}
printf '%s\n' "$EVAL_RESULT" | grep -Fq '"healthy":true' || {
    echo "Calendar Plus live Cinnamon evaluation did not report a healthy instance." >&2
    exit 1
}

if [ -f "$HOME/.xsession-errors" ]; then
    if tail -n 1000 "$HOME/.xsession-errors" | \
        grep -E "\[$UUID\].*(Failed to load|Could not create)|$UUID.*(ReferenceError|TypeError)" >/dev/null; then
        echo "A Calendar Plus load error is present in the current session log." >&2
        exit 1
    fi
fi

printf 'Calendar Plus %s passed installed hashes, native providers, live applet interaction and Cinnamon enablement checks.\n' \
    "$EXPECTED_VERSION"
