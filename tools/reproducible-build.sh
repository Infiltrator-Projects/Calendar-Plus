#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")
SOURCE_DATE_EPOCH=$(sed -n 's/^SOURCE_DATE_EPOCH ?= //p' "$ROOT/Makefile")
ARCH=$(dpkg-architecture -qDEB_HOST_ARCH)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

export SOURCE_DATE_EPOCH
export LC_ALL=C
export TZ=UTC
export PYTHONHASHSEED=0
export ZERO_AR_DATE=1
export CALENDAR_PLUS_BUILD_MODE=generic

copy_source() {
    destination=$1
    mkdir -p "$destination"
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
            --exclude='./.git' \
            -cf - .
    ) | tar -C "$destination" -xf -
    find "$destination" -exec touch --date="@$SOURCE_DATE_EPOCH" {} +
}

build_once() {
    run=$1
    source_dir="$TMP/run-$run/source"
    copy_source "$source_dir"
    (
        cd "$source_dir"
        dpkg-buildpackage -us -uc -b >/dev/null
    )
}

write_manifest() {
    root=$1
    output=$2
    (
        cd "$root"
        find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum
    ) > "$output"
}

preserve_failure() {
    deb1=$1
    deb2=$2
    out="$ROOT/dist/reproducibility-failure"
    rm -rf "$out"
    mkdir -p "$out/run-1" "$out/run-2"
    cp "$deb1" "$out/run-1/"
    cp "$deb2" "$out/run-2/"

    dpkg-deb --raw-extract "$deb1" "$out/run-1/unpacked"
    dpkg-deb --raw-extract "$deb2" "$out/run-2/unpacked"
    write_manifest "$out/run-1/unpacked" "$out/run-1/manifest.sha256"
    write_manifest "$out/run-2/unpacked" "$out/run-2/manifest.sha256"
    diff -u "$out/run-1/manifest.sha256" "$out/run-2/manifest.sha256" \
        > "$out/payload.diff" || true

    for run in 1 2; do
        deb="$out/run-$run/calendar-plus_${VERSION}_${ARCH}.deb"
        mkdir -p "$out/run-$run/ar-members"
        (
            cd "$out/run-$run/ar-members"
            ar x "$deb"
            sha256sum -- ./* | LC_ALL=C sort > ../ar-members.sha256
        )
        dpkg-deb --fsys-tarfile "$deb" | \
            tar --full-time --numeric-owner -tvf - \
            > "$out/run-$run/data-tar-listing.txt"
        dpkg-deb --ctrl-tarfile "$deb" | \
            tar --full-time --numeric-owner -tvf - \
            > "$out/run-$run/control-tar-listing.txt"
    done
    diff -u "$out/run-1/ar-members.sha256" "$out/run-2/ar-members.sha256" \
        > "$out/ar-members.diff" || true
    diff -u "$out/run-1/data-tar-listing.txt" "$out/run-2/data-tar-listing.txt" \
        > "$out/data-tar-listing.diff" || true
    diff -u "$out/run-1/control-tar-listing.txt" "$out/run-2/control-tar-listing.txt" \
        > "$out/control-tar-listing.diff" || true

    if command -v diffoscope >/dev/null 2>&1; then
        diffoscope "$deb1" "$deb2" > "$out/diffoscope.txt" || true
    fi

    printf 'Reproducibility diagnostics preserved in: %s\n' "$out" >&2
}

build_once 1
build_once 2

DEB1="$TMP/run-1/calendar-plus_${VERSION}_${ARCH}.deb"
DEB2="$TMP/run-2/calendar-plus_${VERSION}_${ARCH}.deb"

if ! cmp -s "$DEB1" "$DEB2"; then
    echo "Reproducibility check failed: binary packages differ" >&2
    sha256sum "$DEB1" "$DEB2" >&2
    preserve_failure "$DEB1" "$DEB2"
    exit 1
fi

printf 'Reproducible package: '
sha256sum "$DEB1"
