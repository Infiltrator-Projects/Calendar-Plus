<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calendar Plus

Calendar Plus is a Cinnamon panel clock and calendar for Cinnamon 6.4, 6.6
and 6.7. The 6.4 compatibility path targets Debian 13; the newer paths cover
current Linux Mint Cinnamon releases.
It keeps Cinnamon's panel, popup, settings and CalendarServer integration while
adding alternative clocks and 22 selectable calendar systems. It has its own
seconds setting and can coexist with Linux Mint's stock Calendar applet.

Clock choices include conventional local time, forced 12- or 24-hour time,
decimal time, Internet Time, Unix time, hexadecimal and binary time, local
sidereal time, apparent and mean solar time, Julian and Modified Julian Date,
traditional Chinese double-hours, Roman temporal time and Edo Japanese
seasonal time. Primary and optional secondary dates include Gregorian, Julian,
ISO week, Hebrew, three Islamic
variants, Persian, Chinese, Indian National, Coptic, Ethiopian, Buddhist,
Japanese, Minguo, French Republican, Roman, Mayan Long Count, Badíʿ,
International Fixed, World and Positivist calendars.

## Architecture

Calendar Plus has three deliberate layers:

- `shared/infiltratr-common` is a Git submodule pinned to the GPL-3.0-or-later
  C11 foundation in the [Infiltrator Libraries](https://github.com/The-First-Infiltrator/Infiltrator-Libraries)
  repository. It owns project identity, bounded strings, overflow-safe
  arithmetic, binary quantity formatting and optional POSIX file/path/clock
  adapters. Calendar Plus links it statically, so the source has one canonical
  owner without creating a cross-package runtime dependency.
- The C core owns calendar arithmetic, alternative-clock calculations,
  boundary scheduling, the 42-cell grid and event interval semantics. Its
  records and callback interfaces do not depend on GObject, GVariant, GJS or a
  GUI main loop. GLib base facilities remain a portable dependency. ICU is used through a
small runtime-loaded C adapter so the generic package is not tied to one ICU
SONAME; current release packaging accepts ICU 74 (Ubuntu 24.04/Mint 22), ICU 76
(Debian 13) or ICU 78.
- Native adapters provide GObject Introspection, GVariant tuple conversion and
  the GLib timer used by Cinnamon.
- JavaScript owns Cinnamon actors, settings, translations, CalendarServer
  transport and applet lifecycle.

Calendar and clock implementations are registered through versioned internal
provider tables. Provider metadata generates the matching settings choices, so
the C registry is the single source of truth. The provider ABI is internal;
third-party binary module loading is not part of this release.

The JavaScript applet checks the loaded native library version before creating
the popup. The only helper executable is the on-demand GTK 3 About dialog.
Calendar Plus installs no daemon, polling service or autostart entry.

## Release downloads

Each release is intended to be simple for end users:

| File | Purpose |
| --- | --- |
| `calendar-plus_<version>_amd64.deb` | Generic amd64 package built with conservative `-O2` optimisation. |
| `calendar-plus-<version>-local-folder.run` | Verifies its embedded source, builds with `-O3 -march=native -mtune=native -flto=auto`, creates a `+native1` Debian package and installs it through APT. |

GitHub automatically provides `Source code (zip)` and `Source code (tar.gz)`
for every tagged release. When one of those source archives is extracted, a
normal `make` automatically retrieves the exact pinned Infiltratr Common source
into `shared/infiltratr-common` if it is not already present. Calendar Plus and
the shared library therefore remain separate source trees, but the person
building Calendar Plus does not have to manage that relationship manually.

The local `.run` builder already carries the required Calendar Plus and shared
source trees in its verified payload. The generic package never uses
host-specific CPU flags. Both installation paths produce normal Debian packages
managed by APT. The generic amd64 package intentionally avoids a direct ELF
dependency on a single ICU major so one package can span the supported Debian
and Mint/Ubuntu bases.

## Install

Generic package:

```bash
sudo apt install ./calendar-plus_<version>_amd64.deb
```

Hardware-native build:

```bash
chmod +x calendar-plus-<version>-local-folder.run
./calendar-plus-<version>-local-folder.run
```

The native builder installs missing build dependencies and leaves its generated
`.deb` in the launch directory. `--build-only` builds without installing;
`--verify-only` checks the embedded source payload. After either installation,
add **Calendar Plus** in **System Settings -> Applets**.

## Build and verify

On Debian 13, Linux Mint 22 or Ubuntu 24.04:

```bash
sudo apt install build-essential clang-tidy debhelper gettext gcovr lintian \
    gir1.2-glib-2.0-dev \
    gobject-introspection gjs libglib2.0-dev libicu-dev nodejs pkg-config \
    python3 ripgrep shellcheck zip git
git clone --recurse-submodules https://github.com/The-First-Infiltrator/Calendar-Plus.git
cd Calendar-Plus
make check
sudo make install
```

Git clones and GitHub's automatic source archives use the same pinned shared
source version. If the shared tree is absent, the normal build retrieves the
exact required release automatically; no manual submodule step is required.

`make check` covers C reference/property tests, the adapter-free core,
JavaScript lifecycle and syntax, GJS/typelib compatibility, exported symbols,
immutable ABI history, translations, generated settings, source integrity,
architecture boundaries and builds from paths containing spaces. Calendar
reference anchors cover every registered provider. Additional gates are
`make sanitize`, `make coverage`, `make static-analysis`,
`make reproducible-build` and `make release-check`; the release gate also runs
Lintian against the generic Debian package.
The last command builds and validates the three public files in `dist/`.

On an installed Cinnamon session, `tools/cinnamon-smoke.sh` checks installed
runtime hashes, metadata, About helper, native library/typelib compatibility
and every registered clock/calendar provider. Panel reload, popup use, every clock/calendar mode,
suspend/resume, removal/re-add and reboot still require an interactive smoke
test.

## Maintenance rules

- Keep GObject, GVariant, GJS and toolkit assumptions out of `calendar-core`,
  `clock-engine`, `event-core` and `event-source`.
- Update the `shared/infiltratr-common` submodule only to a reviewed, tested
  Infiltratr Libraries release commit. Never edit a private application copy.
- Infiltratr Common 1.2 owns strict scalar parsing, null-safe string predicates,
  bounded numeric helpers and the shared memory, disk, network, percentage,
  frequency, temperature and power formatters.
- Add built-in calendars and clocks through their provider tables, then run
  `make update-settings`.
- After runtime JavaScript or JSON changes, run
  `make update-runtime-hashes`. After translatable text changes, run
  `make update-pot` and update the language catalogue.
- Comments record ownership, units, invariants, algorithm choices, platform
  boundaries and non-obvious failure behaviour. They must not narrate syntax,
  preserve obsolete implementation history or promise behaviour the code does
  not enforce.
- Existing entries in `tests/abi-baseline.txt` are historical ABI records and
  are not changed by routine releases; new API may add entries and version nodes.
- A release updates `VERSION`, `metadata.json`, `SOURCE_DATE_EPOCH` and the top
  Debian changelog entry, then publishes the `.deb` and `.run`; GitHub provides
  the tagged source archives automatically.

## Model limits

Historical calendars and clocks are computational models. Islamic results are
not local crescent observations; the Badíʿ implementation uses a civil-midnight
Persian year boundary; the French Republican continuation uses a defined
arithmetic rule. Apparent/seasonal solar calculations use the compact NOAA
fractional-year approximation. Roman temporal time divides sunrise-to-sunset
into 12 unequal seasonal hours and the night into four watches. Edo Japanese
time divides daylight and night into six toki each and uses the Kansei-calendar
dawn/dusk altitude. Traditional Chinese time exposes the 12 Earthly-Branch
double-hours rather than inventing a modern finer subdivision. At polar
latitudes where the requested solar boundary does not occur, seasonal clocks
display N/A. Japanese era calendar output follows the installed ICU data.

## Provenance and licence

The current runtime is independently implemented against Cinnamon's public
interfaces. Releases through 3.3.0 retained the notices required by their
Cinnamon-derived files; current provenance details are in `debian/copyright`.

Calendar Plus is GPL-3.0-or-later. The pinned Infiltratr Common dependency uses
the same licence. See `COPYING` and the dependency's `LICENSE`.
