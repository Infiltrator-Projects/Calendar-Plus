#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
TARGET="$TMP/Calendar Plus path-space smoke"

mkdir -p "$TARGET"
(
    cd "$ROOT"
    tar --exclude='./build' \
        --exclude='./dist' \
        --exclude='./debian/.debhelper' \
        --exclude='./debian/calendar-plus' \
        --exclude='./debian/files' \
        --exclude='./debian/*.substvars' \
        --exclude='./debian/debhelper-build-stamp' \
        --exclude='__pycache__' \
        --exclude='*.pyc' \
        --exclude='*.pyo' \
        --exclude='.git' \
        --exclude='*/.git' \
        -cf - .
) | tar -C "$TARGET" -xf -

# Build the actual native library and typelib, then run the Debian install
# override. This catches whitespace regressions in compiler flags and DESTDIR.
make -C "$TARGET" clean >/dev/null
make -C "$TARGET" all >/dev/null
(
    cd "$TARGET"
    debian/rules override_dh_auto_install >/dev/null
)

# The Makefile derives its library directory from the selected compiler's
# target triplet. Clang may report x86_64-pc-linux-gnu while GCC reports
# x86_64-linux-gnu, so validate the directory the build actually selected.
MULTIARCH=$(${CC:-cc} -dumpmachine)
STAGE="$TARGET/debian/calendar-plus"
test -f "$STAGE/usr/lib/$MULTIARCH/libcalendar-plus.so.0.0.0"
test -f "$STAGE/usr/lib/$MULTIARCH/girepository-1.0/CalendarPlus-1.0.typelib"
test -f "$STAGE/usr/share/cinnamon/applets/calendar-plus@the-infiltratr/applet.js"
test -f "$STAGE/usr/share/cinnamon/applets/calendar-plus@the-infiltratr/stylesheet.css"
test -x "$STAGE/usr/share/cinnamon/applets/calendar-plus@the-infiltratr/settings.py"

printf 'Path-with-spaces build/install smoke test passed: %s\n' "$TARGET"
