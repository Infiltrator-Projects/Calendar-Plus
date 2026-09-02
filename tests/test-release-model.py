#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Static contracts for Calendar Plus's release model."""

from __future__ import annotations

import json
import re
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
    publisher = read(".github/workflows/release.yml")
    artifact_validator = read("tools/validate-release-artifacts.sh")
    gitmodules = read(".gitmodules")

    assert sorted(path.name for path in ROOT.glob("*.md")) == ["README.md"]
    assert sorted(path.name for path in (ROOT / ".github").glob("*.md")) == [
        "CODE_OF_CONDUCT.md", "CONTRIBUTING.md", "SECURITY.md"
    ]
    assert (ROOT / "LICENSE").is_file()
    assert not (ROOT / "COPYING").exists()
    assert "DEVELOPING.md" not in makefile
    assert "install -m644 README.md" in makefile

    assert "BUILD_MODE ?= generic" in makefile
    assert "-O2 -g" in makefile
    assert "-O3 -g -march=native -mtune=native -flto=auto" in makefile
    assert "override CFLAGS += $(NATIVE_CFLAGS) $(CALENDAR_CFLAGS)" in makefile
    assert "Calendar-Plus-$(VERSION)-local-source.tar.gz" in makefile
    assert "Calendar-Plus-$(VERSION)-local-source.zip" not in makefile
    assert "TZ=UTC zip -X -9 -q" not in makefile
    assert "export CFLAGS" not in makefile
    assert "INFILTRATR_COMMON_CFLAGS :=" in makefile
    assert "INFILTRATR_COMMON_CPPFLAGS := -Iinclude" in makefile
    assert makefile.count(
        '\n\t\tCPPFLAGS="$(INFILTRATR_COMMON_CPPFLAGS)" \\\n'
        "\t\tCFLAGS='$(INFILTRATR_COMMON_CFLAGS)' \\\n"
    ) == 2
    assert "\n\tCFLAGS= \\\n\t$(G_IR_SCANNER) \\" in makefile
    assert "\n\t.gitmodules \\\n" in makefile
    assert "--exclude='*/.git'" in makefile
    assert "--exclude-vcs" not in read("tools/reproducible-build.sh")
    assert "calendar-plus-$(VERSION)-local-folder.run" in makefile
    assert "calendar-plus-about" in makefile
    assert "FONT_ARCHIVE :=" not in makefile
    assert "prepare-fonts.py" not in makefile
    assert not (ROOT / "assets/fonts/mb-corpo-fonts.tar.xz").exists()
    assert not (ROOT / "tools/prepare-fonts.py").exists()
    assert "does not redistribute proprietary MB Corpo font binaries" in read("README.md")
    assert "Files: src/cinnamon/settings-schema.json" in read("debian/copyright")
    assert "Linux Mint Project and Cinnamon contributors" in read("debian/copyright")
    assert "MB Corpo S Title WEB" in read("src/cinnamon/applet.js")
    assert "calendar-plus-popup" in read("src/cinnamon/applet.js")
    assert "external-configuration-app" in read("src/cinnamon/metadata.json")
    stylesheet = read("src/cinnamon/stylesheet.css")
    assert "MB Corpo S Title WEB" in stylesheet
    assert '.calendar-plus-panel-clock {' in stylesheet
    assert 'font-family: "MB Corpo S Title WEB";' in stylesheet
    assert "font-weight: 400;" in stylesheet
    assert "font-size: 1.08em;" in stylesheet
    applet = read("src/cinnamon/applet.js")
    assert "FONT_PANEL_CLOCK" in applet
    assert 'font-family: "MB Corpo S Title WEB"; font-weight: 400; font-size: 1.08em;' in applet
    assert "MB Corpo S Title WEB" in read("src/cinnamon/settings.py")
    assert "xlet-settings.py" in read("src/cinnamon/settings.py")
    assert "MB Corpo S Title WEB" in read("src/app/about-dialog.c")
    assert "MB Corpo A Title Cond WEB" in read("src/app/about-dialog.c")
    assert "fontconfig," not in read("debian/control")
    assert "libinfiltratr-common.a" in makefile
    assert "src/vendor/infiltratr-common" in makefile
    assert (
        "INFILTRATR_COMMON_COMMIT := "
        "0ac7b8a7ff202b8b0360da2c68c0c145b42d1a71"
    ) in makefile
    assert "INFILTRATR_COMMON_VERSION := 1.15.5" in makefile
    assert "normal `make` automatically retrieves" in read("README.md")
    assert "common-bootstrap: common-check" in makefile
    assert "common-test: $(INFILTRATR_COMMON_ARCHIVE)" in makefile
    assert 'BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)" check' in makefile
    assert "git -C \"$(INFILTRATR_COMMON_DIR)\" fetch -q --depth 1 origin" in makefile
    assert "Infiltrator-Projects/Infiltrator-Libraries.git" in gitmodules
    assert "validate-translations" in makefile
    translation_tool = read("tools/update-translations.py")
    assert '"msgmerge", "--update", "--backup=none"' in translation_tool
    assert "missing maintained translation catalogue" in translation_tool
    assert "CALENDAR_PLUS_BUILD_MODE ?= generic" in rules
    assert "BUILD_MODE=$(CALENDAR_PLUS_BUILD_MODE)" in rules
    assert "DEB_BUILD_MAINT_OPTIONS := optimize=-lto" in rules
    assert "DEB_BUILD_MAINT_OPTIONS := optimize=+lto" in rules
    assert "CHANGELOG.md" not in rules
    assert "validate-translations" in rules
    assert "validate-architecture" in rules
    assert "validate-abi" in rules
    assert "validate-runtime-deps" in rules

    # Common is an independently versioned library. Calendar may include its
    # public headers and link its public archive, but must never encode Common's
    # private source-file membership in this repository.
    assert "INFILTRATR_COMMON_SOURCES :=" not in makefile
    assert "INFILTRATR_COMMON_OBJECTS :=" not in makefile
    assert "$(BUILD_DIR)/infiltratr-%.o" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/core.c" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/arithmetic.c" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/timing.c" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/dynlib.c" not in makefile
    assert "INFILTRATR_COMMON_BUILD_DIR :=" in makefile
    assert '$(MAKE) -C "$(INFILTRATR_COMMON_DIR)"' in makefile
    assert 'BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)"' in makefile

    assert 'NATIVE_VERSION="${VERSION}+native${NATIVE_REVISION}"' in installer
    assert "CALENDAR_PLUS_BUILD_MODE=native" in installer
    assert "dpkg-buildpackage -us -uc -b" in installer
    assert "apt-get install -y --allow-downgrades \"$OUTPUT_DEB\"" in installer
    assert "--build-only" in installer
    assert "--verify-only" in installer
    assert "@PAYLOAD_SHA256@" in installer
    assert 'cat "$SOURCE_TAR" >> "$TEMP_OUTPUT"' in builder

    assert 'ARCH" = "amd64"' in release
    assert "make reproducible-build" in release
    assert "tools/native-installer-smoke.sh" in release
    assert "tools/validate-release-artifacts.sh" in release
    assert "lintian --fail-on error" in release
    assert "local-source.zip" not in release
    assert "exactly two files" in artifact_validator
    assert 'gh release create "$tag"' in publisher
    assert 'gh release upload "$tag" "$deb" "$installer" --clobber' in publisher
    assert 'gh release edit "$tag" --draft=false --latest' in publisher
    assert "baseline_reset" not in publisher
    assert 'git push --force origin "$tag"' not in publisher
    assert 'release_was_published=false' in publisher
    assert 'release_was_published=true' in publisher
    assert 'if [[ "$release_was_published" == false ]]; then' in publisher
    assert 'VERSION="$(sed -n \'s/^VERSION := //p\' Makefile)"' in publisher
    assert '< VERSION' not in publisher
    assert "APT_REPOSITORY_DISPATCH_TOKEN" not in publisher
    assert "repository safety refresh" in publisher
    assert "seq 1 180" in publisher
    assert "within 30 minutes" in publisher
    assert "--draft" in publisher
    assert "release_state=" in publisher
    assert '--json databaseId --jq .databaseId' in publisher
    assert "local-source.zip" not in publisher
    assert "actions/checkout@v" not in workflow
    assert workflow.count("submodules: true") == 6
    assert "check-upstream-drift.sh" not in workflow
    assert "upstream-drift:" not in workflow
    assert "schedule:" not in workflow
    assert (ROOT / ".github/workflows/upstream-drift.yml").is_file()
    assert (ROOT / "tools/check-upstream-calendar-drift.py").is_file()
    assert (ROOT / "tools/upstream-calendar-baseline.json").is_file()
    assert (ROOT / "tools/live-cinnamon-ci-smoke.sh").is_file()
    assert "tools/live-cinnamon-ci-smoke.sh" in workflow
    assert "Probe exact-version live Cinnamon qualification" in workflow
    assert "steps.live-cinnamon.outputs.ready == 'true'" in workflow
    live_smoke = read("tools/live-cinnamon-ci-smoke.sh")
    assert "dpkg-buildpackage -us -uc -b" in live_smoke
    assert "sudo -n apt-get install -y --no-install-recommends" in live_smoke
    assert "Extension.reloadExtension" in live_smoke
    assert "--probe" in live_smoke
    assert "live smoke deferred" not in live_smoke

    assert not (ROOT / "calendar-plus@the-infiltratr").exists()
    for legacy_root in ("cinnamon", "po", "shared"):
        assert not (ROOT / legacy_root).exists()
    assert (ROOT / "src/cinnamon/runtime-sources.sha256").is_file()
    assert (ROOT / "src/i18n/LINGUAS").is_file()
    assert {
        line.strip()
        for line in read("src/i18n/LINGUAS").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    } == {"de", "en_AU", "en_GB", "en_US", "es", "fr", "it", "ja", "pt_BR", "ru"}
    assert (ROOT / "src/vendor/infiltratr-common").is_dir()
    source_root = ROOT / "src"
    assert not [path for path in source_root.iterdir() if path.is_file()]
    assert sorted(path.name for path in source_root.iterdir() if path.is_dir()) == [
        "abi", "adapters", "app", "cinnamon", "core", "i18n", "vendor"
    ]

    applet = read("src/cinnamon/applet.js")
    assert 'Util.spawnCommandLine("/usr/libexec/calendar-plus-about")' in applet
    assert "getCurrentExtension" in applet
    assert 'return require("./runtimeSupport")' in applet
    assert 'RuntimeSupport.loadLocalModule("calendar")' in applet
    assert 'RuntimeSupport.loadLocalModule("eventManager")' in applet
    assert 'RuntimeSupport.loadLocalModule("eventView")' in applet
    assert 'RuntimeSupport.loadLocalModule("panelClock")' in applet
    assert "class CalendarPlusApplet extends Applet.Applet" in applet
    assert "configureApplet(tab = 0)" in applet
    assert "/settings.py " in applet

    runtime = read("src/cinnamon/runtimeSupport.js")
    assert "return require(`./${name}`)" in runtime
    assert runtime.count("var SignalBag = class SignalBag") == 1
    assert "var EventList = class EventList" in read(
        "src/cinnamon/eventView.js"
    )
    assert "var EventsManager = class EventsManager" in read(
        "src/cinnamon/eventManager.js"
    )
    assert "var Calendar = class Calendar" in read(
        "src/cinnamon/calendar.js"
    )
    assert 'var CLOCK_MODE_STANDARD = "standard";' in read(
        "src/cinnamon/panelClock.js"
    )

    registry = read("src/core/calendar-registry.c")
    custom = read("src/core/calendar-custom.c")
    calendar_core = read("src/core/calendar-core.c")
    calendar_system = read("src/adapters/calendar-system.c")
    time_formats = read("src/core/time-formats.c")
    provider_header = read("src/core/calendar-registry.h")
    assert "CALENDAR_PLUS_CALENDAR_PROVIDER_ABI = 1" in provider_header
    assert "CalendarPlusCalendarProvider" in provider_header
    assert "provider->format" in calendar_core
    assert "provider->add_periods" in calendar_core
    assert "calendar_plus_calendar_engine_build_grid_for_locale" in calendar_core
    assert "locale-workday-grid" in read("tests/test-time-formats.c")
    assert "cell->is_work_day = weekday != 0 && weekday != 6;" not in calendar_core
    assert "switch (" not in custom
    assert "switch (" not in calendar_core
    assert "switch (" not in time_formats
    assert "TIME_PROVIDER(" in time_formats
    assert "ICU_PROVIDER(" in registry
    assert "CUSTOM_PROVIDER(" in registry
    assert (ROOT / "src/core/time-astronomy.c").is_file()
    assert (ROOT / "src/core/time-formats-civil.c").is_file()
    assert (ROOT / "src/core/time-formats-astronomy.c").is_file()
    assert (ROOT / "src/core/time-formats-internal.h").is_file()
    assert "src/core/time-formats-civil.c" in makefile
    assert "src/core/time-formats-astronomy.c" in makefile
    assert (ROOT / "src/adapters/calendar-gvariant-adapter.c").is_file()
    assert (ROOT / "src/adapters/event-gvariant-adapter.c").is_file()
    assert (ROOT / "src/adapters/clock-glib-adapter.c").is_file()
    assert (ROOT / "tests/test-exact-clock-boundaries.c").is_file()
    assert "g_variant_" not in calendar_system
    assert "g_timeout_" not in read("src/adapters/system-clock.c")
    assert "CORE_SOURCES :=" in makefile
    assert "ADAPTER_SOURCES :=" in makefile
    assert "libcalendar-plus-core.a" in makefile
    assert not (ROOT / "src/i18n/POTFILES.in").exists()
    assert not (ROOT / "src/i18n/settings-strings.c").exists()
    assert not (ROOT / "tests/exported-symbols.txt").exists()
    assert "update-runtime-hashes.py --check" in makefile
    assert "tests/test-abi.py --library" in makefile
    assert "validate-runtime-deps:" in makefile
    assert "Generic runtime must not embed an ICU-major DT_NEEDED entry" in makefile
    assert "forbidden compatibility library" in read("tools/validate-release-artifacts.sh")
    assert "libcalendar-base.so.0" in read("tools/validate-release-artifacts.sh")
    assert "libcpicu.so.0" in read("tools/validate-release-artifacts.sh")
    for header, minimum_docs in {
        "src/core/calendar-core.h": 16,
        "src/core/clock-engine.h": 7,
        "src/core/event-core.h": 13,
        "src/core/event-source.h": 2,
    }.items():
        assert read(header).count("/**") >= minimum_docs
    assert "### Native ABI policy" in read("README.md")

    assert "COVERAGE_MIN_LINES ?= 80" in makefile
    assert "COVERAGE_MIN_BRANCHES ?= 60" in makefile
    assert "--fail-under-line $(COVERAGE_MIN_LINES)" in makefile
    assert "--fail-under-branch $(COVERAGE_MIN_BRANCHES)" in makefile
    assert "clang-analyzer-*" in makefile
    assert (ROOT / "tests/abi-baseline.txt").is_file()

    # Validate Calendar's actual Common calls against Common's complete public
    # header surface. Do not duplicate Common's private source membership here.
    common = ROOT / "src/vendor/infiltratr-common"
    assert (common / "VERSION").read_text(encoding="utf-8").strip() == "1.15.5"
    assert (common / "LICENSE").is_file()
    common_include = common / "include/infiltratr"
    for public_header in (
        "core.h", "arithmetic.h", "timing.h", "dynlib.h", "utf8.h"
    ):
        assert (common_include / public_header).is_file()
    public_api = "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted(common_include.glob("*.h"))
    )
    call_pattern = re.compile(r"\b(infiltratr_[a-z0-9_]+)\s*\(")
    calendar_common_calls: set[str] = set()
    for source_dir in ("app", "core", "adapters"):
        for path in sorted((ROOT / "src" / source_dir).glob("*.[ch]")):
            calendar_common_calls.update(
                call_pattern.findall(path.read_text(encoding="utf-8"))
            )
    assert calendar_common_calls, "Calendar no longer exercises Common public APIs"
    missing_common_api = sorted(
        function for function in calendar_common_calls
        if not re.search(rf"\b{re.escape(function)}\s*\(", public_api)
    )
    assert not missing_common_api, (
        "Calendar calls APIs absent from pinned Common public headers: "
        f"{missing_common_api}"
    )
    assert "calendar_plus_project_info" in read("src/app/project-info.c")

    # Keep generic mechanics in Common and one Calendar-owned shim/helper layer.
    assert "src/core/calendar-helpers.c" in makefile
    assert "src/core/integer-math.h" in makefile
    assert "infiltratr_utf8_validate" in read("src/core/event-core.c")
    assert "g_utf8_validate" not in read("src/core/event-core.c")
    assert "g_utf8_validate" not in read("src/adapters/event-gvariant-adapter.c")
    assert "calendar_plus_time_floor_divide" not in read("src/core/time-formats.c")
    assert "calendar_plus_time_positive_modulo" not in read("src/core/time-formats.c")
    assert "static gint64\ncount_multiples_inclusive" not in read(
        "src/core/calendar-historical.c"
    )
    assert "static gint64\ncount_multiples_inclusive" not in read(
        "src/core/calendar-reform.c"
    )
    assert "static gchar *\nformat_named_date" not in read(
        "src/core/calendar-custom.c"
    )
    assert "static gchar *\nformat_named_date" not in read(
        "src/core/calendar-historical.c"
    )
    assert "calendar_plus_julian_month_length" in read(
        "src/core/calendar-ancient.c"
    )

    control = read("debian/control")
    assert "cinnamon (>= 6.4)" in control
    for icu_major in range(72, 81):
        assert f"libicu{icu_major}" in control
    assert "libgtk-3-0," in control
    assert "$(PKG_CONFIG) --libs gobject-2.0)" in makefile
    assert "$(PKG_CONFIG) --libs glib-2.0)" in makefile
    assert "-DU_DISABLE_RENAMING=1" in makefile
    assert "src/core/icu-compat-bridge.c" in makefile
    assert "ICU_BRIDGE_LIBS = $(DYNLIB_LIBS) -pthread" in makefile
    assert "DYNLIB_LIBS = -ldl" in makefile

    metadata = json.loads(read("src/cinnamon/metadata.json"))
    version = metadata["version"]
    extracted_version = subprocess.run(
        ["sed", "-n", "s/^VERSION := //p", "Makefile"],
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout.strip()
    assert extracted_version == version
    assert f"VERSION := {version}" in makefile
    assert metadata["author"] == "Shannon Smith"
    assert metadata["website"] == "https://github.com/Infiltrator-Projects/Calendar-Plus"
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

    assert ("GPL-" + "2.0-or-later") not in read("README.md")
    assert "Calendar Plus is GPL-3.0-or-later" in read("README.md")
    assert "License: GPL-3+" in read("debian/copyright")
    assert "/usr/share/common-licenses/GPL-3" in read("debian/copyright")
    assert "GNU GENERAL PUBLIC LICENSE" in read(
        "src/vendor/infiltratr-common/LICENSE"
    )
    assert "BSD 3-Clause License" not in read("src/vendor/infiltratr-common/LICENSE")

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
