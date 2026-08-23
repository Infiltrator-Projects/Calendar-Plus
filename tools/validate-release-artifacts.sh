#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
DIST=${1:-"$ROOT/dist"}
ARCH=amd64
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
DEB="$DIST/calendar-plus_${VERSION}_${ARCH}.deb"
RUN="$DIST/calendar-plus-${VERSION}-local-folder.run"
SOURCE="$DIST/Calendar-Plus-${VERSION}-local-source.zip"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

for artifact in "$DEB" "$RUN" "$SOURCE"; do
    [ -s "$artifact" ] || {
        echo "Missing release artifact: $artifact" >&2
        exit 1
    }
done

[ "$(find "$DIST" -maxdepth 1 -type f | wc -l)" -eq 3 ] || {
    echo "The uploaded release directory must contain exactly three files" >&2
    exit 1
}
[ -x "$RUN" ] || {
    echo "Local builder is not executable: $RUN" >&2
    exit 1
}

unzip -tq "$SOURCE" >/dev/null
[ "$(unzip -p "$SOURCE" "Calendar-Plus-${VERSION}/shared/infiltratr-common/VERSION" | tr -d '[:space:]')" = "1.9.0" ] || {
    echo "Source ZIP does not vendor the pinned Infiltratr Common 1.9.0 tree" >&2
    exit 1
}
[ "$(unzip -p "$SOURCE" "Calendar-Plus-${VERSION}/Makefile" | sed -n 's/^VERSION := //p')" = "$VERSION" ] || {
    echo "Source ZIP version does not match Calendar Plus $VERSION" >&2
    exit 1
}

[ "$(dpkg-deb -f "$DEB" Package)" = "calendar-plus" ]
[ "$(dpkg-deb -f "$DEB" Version)" = "$VERSION" ]
[ "$(dpkg-deb -f "$DEB" Architecture)" = "$ARCH" ]
dpkg-deb -x "$DEB" "$TMP/generic"
test -f "$TMP/generic/usr/lib/$MULTIARCH/libcalendar-plus.so.0.0.0"
test -f \
    "$TMP/generic/usr/lib/$MULTIARCH/girepository-1.0/CalendarPlus-1.0.typelib"
test -f \
    "$TMP/generic/usr/share/cinnamon/applets/calendar-plus@the-infiltratr/applet.js"
test -x "$TMP/generic/usr/libexec/calendar-plus-about"
test -f \
    "$TMP/generic/usr/share/locale/en_AU/LC_MESSAGES/calendar-plus@the-infiltratr.mo"
"$TMP/generic/usr/libexec/calendar-plus-about" --print-metadata |
    grep -qx "version=$VERSION"
grep -q '^Build mode: generic amd64-compatible (Debian/Mint ICU runtime bridge)$' \
    "$TMP/generic/usr/share/doc/calendar-plus/BUILD-INFO"
if grep -q -- '-march=native' \
    "$TMP/generic/usr/share/doc/calendar-plus/BUILD-INFO"; then
    echo "Generic package contains native CPU tuning" >&2
    exit 1
fi
RUNTIME_LIB="$TMP/generic/usr/lib/$MULTIARCH/libcalendar-plus.so.0.0.0"
# Public releases must come from the ordinary monolithic source build. A
# compatibility-assembled split library can work at runtime while silently
# weakening the published link-time ABI, so reject that topology outright.
for private_library in libcalendar-base.so.0 libcpicu.so.0; do
    if [ -e "$TMP/generic/usr/lib/$MULTIARCH/$private_library" ]; then
        echo "Generic package contains forbidden compatibility library: $private_library" >&2
        exit 1
    fi
done
if readelf -d "$RUNTIME_LIB" 2>/dev/null | \
        grep -Eq 'Shared library: \[libicu[^]]*\.so\.[0-9]+'; then
    echo "Generic package has a direct build-host ICU SONAME dependency" >&2
    exit 1
fi
python3 "$ROOT/tests/test-abi.py" --library "$RUNTIME_LIB"
nm -D --defined-only --format=posix "$RUNTIME_LIB" | \
    awk '$1 ~ /^calendar_plus_/ { sub(/@.*/, "", $1); print $1 }' | \
    LC_ALL=C sort -u > "$TMP/generic-exports.actual"
LC_ALL=C sort -u "$ROOT/tests/exported-symbols.txt" > \
    "$TMP/generic-exports.expected"
diff -u "$TMP/generic-exports.expected" "$TMP/generic-exports.actual"

sed '/^__ARCHIVE_BELOW__$/q' "$RUN" > "$TMP/installer-header.sh"
sh -n "$TMP/installer-header.sh"
"$RUN" --verify-only >/dev/null

printf 'Uploaded release artifacts validated for Calendar Plus %s.\n' \
    "$VERSION"
