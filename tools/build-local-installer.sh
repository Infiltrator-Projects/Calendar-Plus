#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
SOURCE_TAR=${1:-"$ROOT/build/Calendar-Plus-${VERSION}-local-source.tar.gz"}
OUTPUT=${2:-"$ROOT/dist/calendar-plus-${VERSION}-local-folder.run"}
TEMPLATE="$ROOT/tools/local-installer.sh.in"

[ -s "$SOURCE_TAR" ] || {
    echo "Local source payload does not exist: $SOURCE_TAR" >&2
    exit 1
}
[ -s "$TEMPLATE" ] || {
    echo "Local installer template does not exist: $TEMPLATE" >&2
    exit 1
}

PAYLOAD_SHA256=$(sha256sum "$SOURCE_TAR" | awk '{print $1}')
mkdir -p "$(dirname -- "$OUTPUT")"
TEMP_OUTPUT="${OUTPUT}.tmp"
trap 'rm -f "$TEMP_OUTPUT"' EXIT HUP INT TERM

sed \
    -e "s/@VERSION@/$VERSION/g" \
    -e "s/@PAYLOAD_SHA256@/$PAYLOAD_SHA256/g" \
    "$TEMPLATE" > "$TEMP_OUTPUT"
cat "$SOURCE_TAR" >> "$TEMP_OUTPUT"
chmod 0755 "$TEMP_OUTPUT"
mv -f "$TEMP_OUTPUT" "$OUTPUT"
trap - EXIT HUP INT TERM

printf 'Local hardware-native installer: %s\n' "$OUTPUT"
