#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
RUN=${1:-"$ROOT/dist/calendar-plus-${VERSION}-local-folder.run"}
ARCH=$(dpkg-architecture -qDEB_HOST_ARCH)
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
NATIVE_VERSION="${VERSION}+native1"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/output"

if ! "$RUN" --build-only --output-dir "$TMP/output" \
    > "$TMP/native-build.log" 2>&1; then
    cat "$TMP/native-build.log" >&2
    exit 1
fi
cat "$TMP/native-build.log"
for native_flag in -O3 -march=native -mtune=native -flto=auto; do
    grep -Eq "^cc .* ${native_flag}( |$)" "$TMP/native-build.log"
done

DEB="$TMP/output/calendar-plus_${NATIVE_VERSION}_${ARCH}.deb"
[ -s "$DEB" ]
[ "$(dpkg-deb -f "$DEB" Package)" = "calendar-plus" ]
[ "$(dpkg-deb -f "$DEB" Version)" = "$NATIVE_VERSION" ]
[ "$(dpkg-deb -f "$DEB" Architecture)" = "$ARCH" ]
dpkg --compare-versions "$NATIVE_VERSION" gt "$VERSION"

dpkg-deb -x "$DEB" "$TMP/native"
grep -q '^Build mode: local hardware-native ' \
    "$TMP/native/usr/share/doc/calendar-plus/BUILD-INFO"
test -f "$TMP/native/usr/lib/$MULTIARCH/libcalendar-plus.so.0.0.0"
test -f \
    "$TMP/native/usr/lib/$MULTIARCH/girepository-1.0/CalendarPlus-1.0.typelib"
test -x "$TMP/native/usr/libexec/calendar-plus-about"
test -f "$TMP/native/usr/share/cinnamon/applets/calendar-plus@the-infiltratr/stylesheet.css"
test -x "$TMP/native/usr/share/cinnamon/applets/calendar-plus@the-infiltratr/settings.py"
for font in \
    mb_corpo_a_cond_regular.ttf \
    mb_corpo_s_bold.ttf \
    mb_corpo_s_regular.ttf; do
    test -f "$TMP/native/usr/share/fonts/truetype/calendar-plus/$font"
done
test -f \
    "$TMP/native/usr/share/locale/en_AU/LC_MESSAGES/calendar-plus@the-infiltratr.mo"
"$TMP/native/usr/libexec/calendar-plus-about" --print-metadata |
    grep -qx "version=$VERSION"

CALENDAR_PLUS_EXPECTED_VERSION="$VERSION" \
LD_LIBRARY_PATH="$TMP/native/usr/lib/$MULTIARCH" \
GI_TYPELIB_PATH="$TMP/native/usr/lib/$MULTIARCH/girepository-1.0" \
    gjs "$ROOT/tests/smoke-typelib.js" >/dev/null

printf 'Local native installer smoke test passed: %s\n' "$DEB"
