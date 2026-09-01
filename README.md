<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calendar Plus

[![Build and test](https://github.com/Infiltrator-Projects/Calendar-Plus/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Infiltrator-Projects/Calendar-Plus/actions/workflows/ci.yml?query=branch%3Amain)

Calendar Plus is a native Cinnamon panel clock and calendar with alternative clock modes, 30 selectable calendar systems and CalendarServer integration.

**Stable release:** 1.0.1.

**Runtime:** Cinnamon 6.4, 6.6 and 6.7.

**Build-tested bases:** Debian 13, Linux Mint 22 and Ubuntu 24.04.

## Capabilities

Clock modes include normal 12/24-hour time, French Republican decimal time, Internet Time, Unix time, hexadecimal and binary time, sidereal time, apparent and mean solar time, Julian/MJD, traditional Chinese double-hours and hundred-kè time, Roman temporal time, Edo Japanese seasonal time, Italian hours from sunset, the historical European gnomonic convention known as Babylonian hours from sunrise, Indian ghaṭī time from sunrise, and Nuremberg equal hours resetting at sunrise and sunset.

Primary and optional secondary dates include Gregorian, Julian, ISO week, Hebrew, four Islamic variants, Persian, Chinese, Dangi, Indian National, Coptic, Ethiopic Amete Mihret and Amete Alem, Buddhist, Japanese, Minguo, French Republican, Roman, Mayan Long Count with Tzolk’in/Haab Calendar Round, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era), traditional Armenian, and Sweden's historical 1700–1753 civil calendar including 30 February 1712.

The applet has its own seconds preference, can coexist with Cinnamon's stock Calendar applet and installs no project-owned daemon, polling service or autostart entry.

Calendar Plus uses the three supplied MB Corpo faces throughout its owned interface: MB Corpo S Regular for normal UI text, MB Corpo S Bold for emphasis and actions, and MB Corpo A Condensed Regular for clock/title display. Every build verifies the exact font archive and each extracted font before packaging or installation.

Location-dependent clocks do not silently assume Greenwich. They show `N/A LOC` until **Geographic location** is enabled and coordinates are supplied. Existing non-zero coordinates from older releases are migrated automatically; a genuine 0°, 0° location can now be selected explicitly.

Calendar workday styling follows ICU/CLDR weekend data for the active locale, including regions whose weekend is not Saturday/Sunday. If locale weekend data is unavailable, Calendar Plus falls back to Monday-Friday workdays.

## Architecture

The source tree is grouped by responsibility:

```text
src/
├── core/       Portable calendar, clock and event logic
├── adapters/   GLib, GVariant and native integration
├── app/        Project identity, version API and About helper
├── cinnamon/   Cinnamon JavaScript runtime and settings
├── i18n/       Gettext sources and translations
├── abi/        Exported-library version map
└── vendor/     Pinned Infiltratr Common submodule
```

The portable core is kept separate from presentation and platform adapters. Cinnamon owns desktop actors, settings and CalendarServer transport; the native C library owns chronology, astronomy, alternative clocks and event semantics. Generic strings, UTF-8 validation, checked/saturating arithmetic, timing and dynamic-library mechanics are supplied by the pinned Infiltratr Common library.

### Native ABI policy

The installed `libcalendar-plus.so.0` and its versioned symbol map are a **runtime stability contract for Calendar Plus itself**, not a separately supported C SDK. The neutral core headers remain source-internal and are not installed as a development package. The exported ABI is nevertheless regression-tested so the Cinnamon typelib, About helper and packaged runtime cannot drift accidentally between releases. If a supported third-party SDK is ever introduced, it will get installed headers, pkg-config metadata and its own compatibility policy rather than silently treating these internal headers as public.

## Correctness model

| Area | Authority / model |
| --- | --- |
| Conventional panel time and Gregorian locale presentation | CinnamonDesktop.WallClock |
| Hebrew, Islamic, Persian, Chinese, Dangi, Indian, Coptic, Ethiopic, Buddhist, Japanese and Minguo calendars | ICU/CLDR calendar data |
| Julian, ISO week, French Republican, Roman, Mayan, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era) and traditional Armenian calendars | Calendar Plus deterministic native algorithms |
| French Republican decimal, Internet, Unix, hexadecimal, binary and Chinese hundred-kè clocks | Exact integer/rational partitioning |
| Sidereal, solar, Roman temporal, Edo seasonal, Italian, Babylonian-hour, Indian ghaṭī and Nuremberg clocks | Native astronomical models using configured coordinates |

Historical calendars and clocks are deterministic computational models rather than claims about every historical locality or observational practice. Modern Badíʿ years use a Tehran-referenced astronomical March equinox and sunset boundary; years before 172 B.E. retain the historical Western 21-March civil convention. Italian hours use equal hours measured strictly from computed sunset. “Babylonian hours” uses the later European gnomonic convention of equal hours from sunrise; Calendar Plus does not present that label as a reconstruction of ancient Mesopotamian civil timekeeping. The source comments document the exact continuation rules, epochs and astronomical assumptions used where more than one convention exists.

## Build and test

Install the development dependencies on Debian 13, Linux Mint 22 or Ubuntu 24.04:

```bash
sudo apt install build-essential clang debhelper gettext gobject-introspection gjs \
    gir1.2-glib-2.0-dev libglib2.0-dev libicu-dev nodejs pkg-config \
    python3 ripgrep shellcheck git
```

Clone recursively because Calendar Plus pins Infiltratr Common as a submodule:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calendar-Plus.git
cd Calendar-Plus
make check
```

Git clones carry the pinned Common submodule. If a GitHub automatic source archive does not contain the vendor checkout, normal `make` automatically retrieves the exact pinned Common commit.

Additional quality gates are available through `make sanitize`, `make coverage`, `make static-analysis`, `make reproducible-build` and `make release-check`. CI requires at least 80% C line coverage and 60% branch coverage in addition to sanitizer, static-analysis, ABI, packaging and reproducibility gates. A clean Debian 13 container also builds, installs, executes and purges the generic package before a release is eligible for publication; the central Infiltrator repository separately performs a full Linux Mint 22.3 package lifecycle test after publication.

For an installed Cinnamon session, `tools/cinnamon-smoke.sh` verifies installed runtime hashes and typelib identity, then exercises the live applet through Cinnamon's D-Bus evaluation interface without changing persistent settings. CI first probes whether a trusted runner can qualify the exact source revision. A matching installed version is tested directly; a Cinnamon runner with passwordless package-install permission builds, installs and reloads the exact revision before testing. When neither is possible the actual live-smoke step is explicitly shown as skipped rather than reporting an older applet as a pass.

A scheduled `upstream-calendar-drift` workflow watches Cinnamon's stock calendar applet, CalendarServer and resume integration. It does not import upstream code: it fails only when the reviewed upstream surface changes so Calendar Plus can assess relevant compatibility fixes deliberately.

Release publication asks the central Infiltrator APT repository to refresh immediately and verifies the published catalogue. If cross-repository dispatch is unavailable, Calendar Plus waits for the repository safety schedule; the central repository detects when that scheduled refresh advances Calendar Plus and still runs its Linux Mint 22.3 lifecycle qualification for the newly published version.

## Install and releases

Numbered releases publish two project-owned artifacts:

| File | Purpose |
| --- | --- |
| `calendar-plus_<version>_amd64.deb` | Generic amd64 Debian package |
| `calendar-plus-<version>-local-folder.run` | Verified local hardware-native builder |

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

GitHub supplies the standard source ZIP and tarball for each immutable release tag, so the project does not upload a redundant source archive. Development is performed on `main`; every push is tested, and only a successful commit whose subject begins with `Release <version>` is eligible for automated tagging and publication.

## Project policies

Contribution guidance is in [`.github/CONTRIBUTING.md`](.github/CONTRIBUTING.md), security reporting is in [`.github/SECURITY.md`](.github/SECURITY.md), and participation standards are in [`.github/CODE_OF_CONDUCT.md`](.github/CODE_OF_CONDUCT.md). These files contain policy only; user and developer guidance remains consolidated here.

## Troubleshooting

- **No events:** confirm Cinnamon CalendarServer/Evolution Data Server is available; Calendar Plus reconnects automatically after service restarts.
- **Alternative clock shows `N/A LOC`:** enable **Geographic location** in the applet settings and enter latitude/longitude.
- **Applet refuses to load after an upgrade:** a stale native library is rejected deliberately when its version differs from the installed applet; reinstall the matching package.
- **Custom format is invalid:** Calendar Plus falls back safely instead of passing a null format result into the native library.
- **Installed-state verification:** run `tools/cinnamon-smoke.sh` from a matching source checkout inside the Cinnamon session.

## Model limits

Historical calendars and clocks are computational models. Islamic results are not local crescent observations; solar and seasonal clocks use defined astronomical approximations and may return N/A at polar latitudes. Japanese era output follows the installed ICU data.

## Translations

Project-owned catalogues are shipped for German, Spanish, French, Italian, Brazilian Portuguese, Russian, Japanese, Australian English, British English and United States English. Every maintained catalogue is deterministically merged against the generated POT during validation, so a new UI string cannot silently leave non-English catalogues structurally stale. Historical proper names and calendar terms whose conventional spelling is intentionally unchanged remain untranslated where appropriate. Other locales fall back to the source English strings; ICU and Cinnamon continue to localise the calendar/date material they own.

## Licence

Calendar Plus is GPL-3.0-or-later. The pinned Infiltratr Common dependency uses the same licence. The complete project licence is in `LICENSE`; Debian packaging provenance is recorded in `debian/copyright`.
