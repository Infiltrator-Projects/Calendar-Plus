#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Enforce the platform-core/presentation-adapter boundary transitively.

The core contract applies to the complete local include closure, not merely to
hand-picked implementation files.  A neutral source that includes a neutral
header which then includes an adapter header is therefore a build failure.
"""

from __future__ import annotations

import re
from collections import deque
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "Makefile"
LOCAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
C_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
C_LINE_COMMENT_RE = re.compile(r"//.*?$", re.MULTILINE)


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def code(relative: str) -> str:
    source = C_BLOCK_COMMENT_RE.sub("", read(relative))
    return C_LINE_COMMENT_RE.sub("", source)


def make_list(name: str) -> tuple[str, ...]:
    """Return a simple backslash-continued Make variable as path tokens."""
    lines = MAKEFILE.read_text(encoding="utf-8").splitlines()
    prefix = f"{name} :="
    collecting = False
    values: list[str] = []

    for raw in lines:
        if not collecting:
            if not raw.startswith(prefix):
                continue
            collecting = True
            raw = raw[len(prefix):]

        text = raw.strip()
        continued = text.endswith("\\")
        if continued:
            text = text[:-1].rstrip()
        if text:
            values.extend(text.split())
        if not continued:
            break

    assert collecting, f"missing Make variable {name}"
    assert values, f"empty Make variable {name}"
    return tuple(values)


def resolve_local_include(relative: str, include: str) -> str | None:
    """Resolve quoted includes that belong to this source tree."""
    owner = ROOT / relative
    candidates = (
        owner.parent / include,
        ROOT / include,
        ROOT / "src/core" / include,
        ROOT / "src/adapters" / include,
        ROOT / "src/app" / include,
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve().relative_to(ROOT.resolve()).as_posix()
    return None


def include_closure(roots: tuple[str, ...]) -> tuple[set[str], dict[str, str]]:
    """Walk local quoted includes and retain parents for diagnostics."""
    seen: set[str] = set()
    parent: dict[str, str] = {}
    queue = deque(roots)

    while queue:
        relative = queue.popleft()
        if relative in seen:
            continue
        assert (ROOT / relative).is_file(), f"missing core path {relative}"
        seen.add(relative)

        for include in LOCAL_INCLUDE_RE.findall(code(relative)):
            dependency = resolve_local_include(relative, include)
            if dependency is None or dependency in seen:
                continue
            parent.setdefault(dependency, relative)
            queue.append(dependency)

    return seen, parent


def include_chain(path: str, parent: dict[str, str]) -> str:
    chain = [path]
    while chain[-1] in parent:
        chain.append(parent[chain[-1]])
    chain.reverse()
    return " -> ".join(chain)


def main() -> None:
    core_sources = make_list("CORE_SOURCES")
    core_headers = make_list("CORE_HEADERS")
    adapter_sources = set(make_list("ADAPTER_SOURCES"))
    private_headers = set(make_list("PRIVATE_HEADERS"))
    declared_core = set(core_sources) | set(core_headers)

    closure, parent = include_closure(core_sources + core_headers)

    # Every repository-local dependency reached from the portable core must be
    # explicitly declared as part of that core.  This catches adapter leakage
    # even when a forbidden API token happens not to appear in the first file.
    undeclared = closure - declared_core
    assert not undeclared, (
        "core include closure reaches non-core files: "
        + "; ".join(
            include_chain(path, parent) for path in sorted(undeclared)
        )
    )
    leaked_adapters = closure & (adapter_sources | private_headers)
    assert not leaked_adapters, (
        "core include closure reaches adapter files: "
        + "; ".join(
            include_chain(path, parent) for path in sorted(leaked_adapters)
        )
    )

    forbidden_tokens = (
        "glib-object.h",
        "GObject",
        "GVariant",
        "g_timeout",
        "Gtk",
        "Gdk",
        "Gjs",
        "Cinnamon",
    )
    for relative in sorted(closure):
        implementation = code(relative)
        for token in forbidden_tokens:
            assert token not in implementation, (
                f"{include_chain(relative, parent)} leaks adapter token {token}"
            )

    # Adapter ownership remains explicit: these facilities must exist, but only
    # outside the neutral closure checked above.
    assert "GVariant" not in code("src/adapters/calendar-system.c")
    assert "GVariant" not in code("src/adapters/event-store.c")
    assert "g_timeout" not in code("src/adapters/system-clock.c")
    assert "GVariant" in read("src/adapters/calendar-gvariant-adapter.c")
    assert "GVariant" in read("src/adapters/event-gvariant-adapter.c")
    assert "g_timeout_add_full" in read("src/adapters/clock-glib-adapter.c")

    makefile = read("Makefile")
    assert "CORE_SOURCES :=" in makefile
    assert "ADAPTER_SOURCES :=" in makefile
    assert "libcalendar-plus-core.a" in makefile
    assert "test-portable-core.c $(CORE_SOURCES)" in makefile
    assert "$(CORE_CFLAGS)" in makefile
    assert "validate-architecture" in makefile

    public_event_adapter = read("src/adapters/event-store.h")
    assert "Cinnamon CalendarServer event tuple" in public_event_adapter
    assert "CalendarPlusEventInput" not in public_event_adapter
    assert "CalendarPlusEventSource" in read("src/core/event-source.h")


if __name__ == "__main__":
    main()
