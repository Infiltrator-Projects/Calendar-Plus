#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith
"""Calendar Plus typography host for Cinnamon's native xlet settings UI.

Cinnamon's generic xlet-settings process is a separate GTK application, so it
cannot inherit the St/Cinnamon stylesheet used by the panel applet. This thin
host deliberately reuses Cinnamon's own MainWindow, JSON settings widgets,
persistence, D-Bus callbacks, import/export and reset behaviour. Its only
presentation policy is to install the Calendar Plus font family before that
window is constructed.
"""

from __future__ import annotations

import argparse
import importlib.util
import signal
import sys
from pathlib import Path

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, Gtk  # noqa: E402

UUID = "calendar-plus@the-infiltratr"
CINNAMON_SETTINGS_DIR = Path("/usr/share/cinnamon/cinnamon-settings")
XLET_SETTINGS = CINNAMON_SETTINGS_DIR / "xlet-settings.py"

CSS = b"""
* {
    font-family: "MB Corpo S Title WEB";
}
headerbar .title {
    font-family: "MB Corpo A Title Cond WEB";
    font-weight: 400;
}
"""


def load_cinnamon_settings():
    if not XLET_SETTINGS.is_file():
        raise SystemExit(f"Cinnamon xlet settings renderer not found: {XLET_SETTINGS}")

    path = str(CINNAMON_SETTINGS_DIR)
    if path not in sys.path:
        sys.path.insert(0, path)

    spec = importlib.util.spec_from_file_location(
        "calendar_plus_cinnamon_xlet_settings",
        XLET_SETTINGS,
    )
    if spec is None or spec.loader is None:
        raise SystemExit("Unable to load Cinnamon xlet settings renderer")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def apply_typography() -> None:
    screen = Gdk.Screen.get_default()
    if screen is None:
        return

    provider = Gtk.CssProvider()
    provider.load_from_data(CSS)
    Gtk.StyleContext.add_provider_for_screen(
        screen,
        provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Calendar Plus settings")
    parser.add_argument(
        "--instance",
        type=int,
        default=None,
        help="Calendar Plus applet instance to configure",
    )
    parser.add_argument(
        "--tab",
        type=int,
        default=None,
        help="Settings tab index",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cinnamon_settings = load_cinnamon_settings()
    apply_typography()

    native_args = argparse.Namespace(
        type="applet",
        uuid=UUID,
        id=args.instance,
        tab=args.tab,
    )
    window = cinnamon_settings.MainWindow(native_args)
    signal.signal(signal.SIGINT, window.quit)
    Gtk.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
