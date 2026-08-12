#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Static contracts for Calendar Plus's release model."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> None:
    makefile = read("Makefile")
    rules = read("debian/rules")
    installer = read("tools/local-installer.sh.in")
    builder = read("tools/build-local-installer.sh")
    release = read("tools/release-check.sh")
    workflow = read(".github/workflows/ci.yml")
    gitmodules = read(".gitmodules")

    assert sorted(path.name for path in ROOT.glob("*.md")) == ["README.md"]
    assert "DEVELOPING.md" not in makefile
    assert "install -m644 README.md" in makefile

    assert "BUILD_MODE ?= generic" in makefile
    assert "-O2 -g" in makefile
    assert "-O3 -g -march=native -mtune=native -flto=auto" in makefile
    assert "override CFLAGS += $(NATIVE_CFLAGS) $(COMMON_CFLAGS)" in makefile
    assert "Calendar-Plus-$(VERSION)-local-source.zip" in makefile
    assert "TZ=UTC zip -X -9 -q" in makefile
    assert "\n\t.gitmodules \\\n" in makefile
    assert "--exclude='*/.git'" in makefile
    assert "--exclude-vcs" not in read("tools/reproducible-build.sh")
    assert "calendar-plus-$(VERSION)-local-folder.run" in makefile
    assert "calendar-plus-about" in makefile
    assert "libinfiltratr-common.a" in makefile
    assert "shared/infiltratr-common" in makefile
    assert (
        "INFILTRATR_COMMON_COMMIT := "
        "b90cf49521bb8ecf85e46a39f67f1c0d0a0509b2"
    ) in makefile
    assert "normal `make` automatically retrieves" in read("README.md")
    assert "common-bootstrap: common-check" in makefile
    assert "git clone --depth 1 --branch" in makefile
    assert "The-First-Infiltrator/Infiltrator-Libraries.git" in gitmodules
    assert "validate-translations" in makefile
    assert "CALENDAR_PLUS_BUILD_MODE ?= generic" in rules
    assert "BUILD_MODE=$(CALENDAR_PLUS_BUILD_MODE)" in rules
    assert "DEB_BUILD_MAINT_OPTIONS := optimize=-lto" in rules
    assert "DEB_BUILD_MAINT_OPTIONS := optimize=+lto" in rules
    assert "CHANGELOG.md" not in rules
    assert "validate-translations" in rules
    assert "validate-architecture" in rules
    assert "validate-abi" in rules
    assert "validate-runtime-deps" in rules

    assert 'NATIVE_VERSION="${VERSION}+native${NATIVE_REVISION}"' in installer
    assert "CALENDAR_PLUS_BUILD_MODE=native" in installer
    assert "dpkg-buildpackage -us -uc -b" in installer
    assert "apt-get install -y \"$OUTPUT_DEB\"" in installer
    assert "--build-only" in installer
    assert "--verify-only" in installer
    assert "@PAYLOAD_SHA256@" in installer
    assert 'cat "$SOURCE_TAR" >> "$TEMP_OUTPUT"' in builder

    assert 'ARCH" = "amd64"' in release
    assert "make reproducible-build" in release
    assert "tools/native-installer-smoke.sh" in release
    assert "tools/validate-release-artifacts.sh" in release
    assert "lintian --fail-on error" in release
    assert "actions/checkout@v" not in workflow
    assert workflow.count("submodules: true") == 5
    assert "check-upstream-drift.sh" not in workflow
    assert "upstream-drift:" not in workflow
    assert "schedule:" not in workflow

    applet = read("calendar-plus@the-infiltratr/applet.js")
    assert 'Util.spawnCommandLine("/usr/libexec/calendar-plus-about")' in applet
    assert "getCurrentExtension" in applet
    assert 'return require("./runtimeSupport")' in applet
    assert 'RuntimeSupport.loadLocalModule("calendar")' in applet
    assert 'RuntimeSupport.loadLocalModule("eventManager")' in applet
    assert 'RuntimeSupport.loadLocalModule("eventView")' in applet
    assert 'RuntimeSupport.loadLocalModule("panelClock")' in applet
    assert "class CalendarPlusApplet extends Applet.Applet" in applet

    runtime = read("calendar-plus@the-infiltratr/runtimeSupport.js")
    assert "return require(`./${name}`)" in runtime
    assert runtime.count("var SignalBag = class SignalBag") == 1
    assert "var EventList = class EventList" in read(
        "calendar-plus@the-infiltratr/eventView.js"
    )
    assert "var EventsManager = class EventsManager" in read(
        "calendar-plus@the-infiltratr/eventManager.js"
    )
    assert "var Calendar = class Calendar" in read(
        "calendar-plus@the-infiltratr/calendar.js"
    )
    assert 'var CLOCK_MODE_STANDARD = "standard";' in read(
        "calendar-plus@the-infiltratr/panelClock.js"
    )

    # Built-in calendars and clocks use versioned provider operation tables.
    # Core dispatch must not grow a second mode-switch architecture.
    registry = read("src/calendar-registry.c")
    custom = read("src/calendar-custom.c")
    calendar_core = read("src/calendar-core.c")
    calendar_system = read("src/calendar-system.c")
    time_formats = read("src/time-formats.c")
    provider_header = read("src/calendar-registry.h")
    assert "CALENDAR_PLUS_CALENDAR_PROVIDER_ABI = 1" in provider_header
    assert "CalendarPlusCalendarProvider" in provider_header
    assert "provider->format" in calendar_core
    assert "provider->add_periods" in calendar_core
    assert "switch (" not in custom
    assert "switch (" not in calendar_core
    assert "switch (" not in time_formats
    assert "TIME_PROVIDER(" in time_formats
    assert "ICU_PROVIDER(" in registry
    assert "CUSTOM_PROVIDER(" in registry
    assert (ROOT / "src/time-astronomy.c").is_file()
    assert (ROOT / "src/calendar-gvariant-adapter.c").is_file()
    assert (ROOT / "src/event-gvariant-adapter.c").is_file()
    assert (ROOT / "src/clock-glib-adapter.c").is_file()
    assert "g_variant_" not in calendar_system
    assert "g_timeout_" not in read("src/system-clock.c")
    assert "CORE_SOURCES :=" in makefile
    assert "ADAPTER_SOURCES :=" in makefile
    assert "libcalendar-plus-core.a" in makefile
    assert not (ROOT / "po/POTFILES.in").exists()
    assert "update-runtime-hashes.py --check" in makefile
    assert "tests/test-abi.py --library" in makefile
    assert "validate-runtime-deps:" in makefile
    assert "Generic runtime must not embed an ICU-major DT_NEEDED entry" in makefile
    assert "forbidden compatibility library" in read("tools/validate-release-artifacts.sh")
    assert "libcalendar-base.so.0" in read("tools/validate-release-artifacts.sh")
    assert "libcpicu.so.0" in read("tools/validate-release-artifacts.sh")
    assert "strtod(text" not in read("shared/infiltratr-common/src/core.c")
    assert "independent of LC_NUMERIC" in read(
        "shared/infiltratr-common/include/infiltratr/core.h"
    )
    assert "--fail-under-line $(COVERAGE_MIN_LINES)" in makefile
    assert "clang-analyzer-*" in makefile
    assert (ROOT / "tests/abi-baseline.txt").is_file()
    shared = ROOT / "shared/infiltratr-common"
    assert (shared / "VERSION").read_text(encoding="utf-8").strip() == "1.2.0"
    assert (shared / "LICENSE").is_file()
    assert (shared / "include/infiltratr/core.h").is_file()
    assert (shared / "include/infiltratr/format.h").is_file()
    assert (shared / "include/infiltratr/posix.h").is_file()
    assert (shared / "src/core.c").is_file()
    assert (shared / "src/format.c").is_file()
    assert (shared / "src/posix.c").is_file()
    assert "InfiltratrProjectInfo" in read(
        "shared/infiltratr-common/include/infiltratr/core.h"
    )
    assert "calendar_plus_project_info" in read("src/project-info.c")

    control = read("debian/control")
    assert "cinnamon (>= 6.4)" in control
    assert "libicu76 | libicu74 | libicu78" in control
    assert "libgtk-3-0," not in control
    # ICU must be a runtime-selected provider, not a build-host SONAME baked
    # into libcalendar-plus. The headers remain a build dependency.
    assert "$(PKG_CONFIG) --libs gobject-2.0)" in makefile
    assert "$(PKG_CONFIG) --libs glib-2.0)" in makefile
    assert "-DU_DISABLE_RENAMING=1" in makefile
    assert "src/icu-compat-bridge.c" in makefile
    assert "ICU_BRIDGE_LIBS = -ldl -pthread" in makefile

    metadata = json.loads(read("calendar-plus@the-infiltratr/metadata.json"))
    version = metadata["version"]
    assert f"VERSION := {version}" in makefile
    assert metadata["author"] == "Shannon Smith"
    assert "const APP_VERSION" not in applet
    assert "metadata.version" in applet

    subprocess.run(
        ["dpkg", "--compare-versions", f"{version}+native1", "gt", version],
        check=True,
    )
    subprocess.run(
        [
            "dpkg", "--compare-versions",
            ".".join([*version.split(".")[:-1], str(int(version.split(".")[-1]) + 1)]),
            "gt", f"{version}+native1"
        ],
        check=True,
    )

    # Calendar Plus and its canonical shared dependency are GPL-3.0-or-later.
    assert ("GPL-" + "2.0-or-later") not in read("README.md")
    assert "Calendar Plus is GPL-3.0-or-later" in read("README.md")
    assert "License: GPL-3+" in read("debian/copyright")
    assert "/usr/share/common-licenses/GPL-3" in read("debian/copyright")
    assert "GNU GENERAL PUBLIC LICENSE" in read(
        "shared/infiltratr-common/LICENSE"
    )
    assert "BSD 3-Clause License" not in read("shared/infiltratr-common/LICENSE")

    project_owned_suffixes = {".c", ".h", ".js", ".py", ".sh", ".in", ".yml", ".yaml"}
    for path in ROOT.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        if path.suffix not in project_owned_suffixes:
            continue
        content = path.read_text(encoding="utf-8")
        assert ("GPL-" + "2.0-or-later") not in content, path.relative_to(ROOT)
        assert "SPDX-License-Identifier: GPL-3.0-or-later" in content, (
            f"missing GPL-3 SPDX marker: {path.relative_to(ROOT)}"
        )

    forbidden = ("Chat" + "GPT", "Open" + "AI", "H" + "AL 9000")
    for path in ROOT.rglob("*"):
        if (not path.is_file() or ".git" in path.parts or
                path.parts[-2:-1] in (("build",), ("dist",))):
            continue
        if path.suffix not in {
            ".c", ".h", ".js", ".json", ".md", ".py", ".sh", ".in",
            ".rules", ".yml", ".yaml",
        } and path.name not in {"Makefile", "changelog", "control", "copyright"}:
            continue
        content = path.read_text(encoding="utf-8")
        assert not any(word in content for word in forbidden), (
            f"assistant attribution in release input: {path.relative_to(ROOT)}"
        )


if __name__ == "__main__":
    main()
