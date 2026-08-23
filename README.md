<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calendar Plus

[![Build and test](https://github.com/The-First-Infiltrator/Calendar-Plus/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/Calendar-Plus/actions/workflows/ci.yml)

Calendar Plus is a native Cinnamon panel clock and calendar that extends the stock Cinnamon experience with alternative clocks, 22 selectable calendar systems and CalendarServer integration while remaining compatible with Cinnamon's panel, popup and settings model.

**Current version:** 3.9.9  
**Platforms:** Cinnamon 6.4, 6.6 and 6.7 on supported Debian/Linux Mint bases  
**Licence:** GPL-3.0-or-later

## Capabilities

Calendar Plus provides conventional local time plus forced 12/24-hour time, decimal time, Internet Time, Unix time, hexadecimal and binary time, sidereal time, apparent and mean solar time, Julian/MJD, traditional Chinese double-hours, Roman temporal time and Edo Japanese seasonal time.

Primary and optional secondary dates include Gregorian, Julian, ISO week, Hebrew, three Islamic variants, Persian, Chinese, Indian National, Coptic, Ethiopian, Buddhist, Japanese, Minguo, French Republican, Roman, Mayan Long Count, Badíʿ, International Fixed, World and Positivist calendars.

The applet has its own seconds setting, can coexist with Linux Mint's stock Calendar applet and installs no project-owned daemon, polling service or autostart entry.

## Architecture

Calendar Plus is deliberately split into three layers:

- `shared/infiltratr-common` is the pinned Infiltratr Common dependency for reusable parsing, arithmetic, timing and dynamic-library mechanics.
- The portable C core owns calendar arithmetic, alternative-clock calculations, boundary scheduling, event interval semantics and the 42-cell calendar grid.
- Native adapters provide GObject Introspection, GVariant conversion and GLib timer integration; JavaScript owns Cinnamon actors, settings, translations, CalendarServer transport and applet lifecycle.

Calendar and clock implementations are registered through internal provider tables. Those tables are the source of truth for both runtime behaviour and settings choices. The JavaScript applet verifies the loaded native-library version before creating its popup.

Application-specific calendar rules, astronomy, ICU symbol policy and Cinnamon integration remain in Calendar Plus rather than moving into Common.

## Build and test

On Debian 13, Linux Mint 22 or Ubuntu 24.04:

```bash
sudo apt install build-essential clang debhelper gettext gobject-introspection gjs \
    gir1.2-glib-2.0-dev libglib2.0-dev libicu-dev nodejs pkg-config \
    python3 ripgrep shellcheck zip git

git clone --recurse-submodules https://github.com/The-First-Infiltrator/Calendar-Plus.git
cd Calendar-Plus
make check
```

Git clones carry the pinned Common submodule. When a GitHub automatic source archive is used instead and the shared tree is absent, normal `make` automatically retrieves the exact pinned Common commit before building.

Useful verification targets include:

```bash
make check
make sanitize
make coverage
make static-analysis
make reproducible-build
make release-check
```

`make check` covers the C core, clock/calendar reference anchors, boundary behaviour, JavaScript lifecycle/syntax, GJS/typelib compatibility, exported symbols, translations, generated settings, source integrity and architecture boundaries. `make release-check` validates the complete release artifact set.

## Release assets

A numbered GitHub release publishes these project-owned artifacts:

| File | Purpose |
| --- | --- |
| `calendar-plus_<version>_amd64.deb` | Generic amd64 Debian package. |
| `calendar-plus-<version>-local-folder.run` | Verified local native build/install program. |
| `Calendar-Plus-<version>-local-source.zip` | Tested source archive containing Calendar Plus and the exact pinned Common source. |

### Install

Install the generic package with:

```bash
sudo apt install ./calendar-plus_<version>_amd64.deb
```

Or use the native builder:

```bash
chmod +x calendar-plus-<version>-local-folder.run
./calendar-plus-<version>-local-folder.run
```

After installation, add **Calendar Plus** from **System Settings → Applets**.

## Repository and release policy

This repository uses `main` as its working branch. Development changes are made directly on `main`; the normal project workflow does not depend on PR, feature or release branches.

Every push to `main` runs CI. Ordinary commits do not publish a release. A commit is release-eligible only when its subject begins with the exact source version in the form `Release <version>` (optionally followed by a colon and description) and the complete `main` CI run succeeds.

The publisher then re-checks that the tested commit is still the exact current `main`, reruns the release gates, builds the `.deb`, `.run` and deterministic source ZIP, creates the version tag and publishes the release. Existing version tags and published releases are immutable and are never moved, replaced or edited in place.

Manually runnable build/test workflows, where present, are diagnostic helpers only and are not release-approval mechanisms.

## Model limits

Historical calendars and clocks are computational models. Islamic results are not local crescent observations; solar/seasonal clocks use defined astronomical approximations and can return N/A at polar latitudes where a requested solar boundary does not occur. Japanese era output follows the installed ICU data.

## Licence

Calendar Plus is GPL-3.0-or-later. It is free software licensed under the GNU General Public License version 3 or, at your option, any later version. The pinned Infiltratr Common dependency uses the same licence. Provenance and retained third-party notices are recorded in `debian/copyright` and the repository licence files.
