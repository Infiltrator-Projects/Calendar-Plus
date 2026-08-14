# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
G_IR_SCANNER ?= g-ir-scanner
G_IR_COMPILER ?= g-ir-compiler
PREFIX ?= /usr
DESTDIR ?=

VERSION := 3.9.6
UUID := calendar-plus@the-infiltratr
BUILD_DIR := build
DIST_DIR := dist
INFILTRATR_COMMON_DIR := shared/infiltratr-common
INFILTRATR_COMMON_URL := https://github.com/The-First-Infiltrator/Infiltrator-Libraries.git
INFILTRATR_COMMON_TAG := v1.5.0
INFILTRATR_COMMON_COMMIT := a0e75ffbe4e038c74c8f1e3d589f2dae87b2b7bb
INFILTRATR_COMMON_VERSION := 1.5.0
LIB_BASENAME := calendar-plus
LIB_SONAME := lib$(LIB_BASENAME).so.0
LIB_REALNAME := lib$(LIB_BASENAME).so.0.0.0
ABOUT_BINARY := calendar-plus-about
GIR := CalendarPlus-1.0
CORE_SOURCES := \
	src/clock-engine.c \
	src/time-formats.c \
	src/time-astronomy.c \
	src/julian-day.c \
	src/calendar-registry.c \
	src/icu-calendar.c \
	src/icu-compat-bridge.c \
	src/calendar-ancient.c \
	src/calendar-bahai.c \
	src/calendar-perpetual.c \
	src/calendar-reform.c \
	src/calendar-custom.c \
	src/calendar-core.c \
	src/event-core.c \
	src/event-source.c
ADAPTER_SOURCES := \
	src/system-clock.c \
	src/clock-glib-adapter.c \
	src/calendar-system.c \
	src/calendar-gvariant-adapter.c \
	src/event-store.c \
	src/event-gvariant-adapter.c
SOURCES := src/project-info.c src/version.c $(CORE_SOURCES) $(ADAPTER_SOURCES)
PUBLIC_HEADERS := \
	src/version.h \
	src/system-clock.h \
	src/time-formats.h \
	src/calendar-types.h \
	src/calendar-system.h \
	src/event-types.h \
	src/event-store.h
CORE_HEADERS := \
	src/clock-engine.h \
	src/time-formats.h \
	src/time-astronomy.h \
	src/julian-day.h \
	src/calendar-types.h \
	src/calendar-core.h \
	src/calendar-internal.h \
	src/calendar-registry.h \
	src/icu-calendar.h \
	src/calendar-ancient.h \
	src/calendar-bahai.h \
	src/calendar-perpetual.h \
	src/calendar-reform.h \
	src/calendar-custom.h \
	src/event-types.h \
	src/event-core.h \
	src/event-source.h
PRIVATE_HEADERS := \
	src/project-info.h \
	src/clock-glib-adapter.h \
	src/calendar-system-private.h \
	src/event-store-private.h
HEADERS := $(sort $(PUBLIC_HEADERS) $(CORE_HEADERS) $(PRIVATE_HEADERS))
# Calendar Plus consumes the POSIX-free Common core and formatting layer.
INFILTRATR_COMMON_SOURCES := \
	$(INFILTRATR_COMMON_DIR)/src/core.c \
	$(INFILTRATR_COMMON_DIR)/src/format.c
INFILTRATR_COMMON_HEADERS := \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/compiler.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/core.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/format.h
INFILTRATR_COMMON_OBJECTS := \
	$(patsubst $(INFILTRATR_COMMON_DIR)/src/%.c,$(BUILD_DIR)/infiltratr-%.o,$(INFILTRATR_COMMON_SOURCES))
INFILTRATR_COMMON_ARCHIVE := $(BUILD_DIR)/libinfiltratr-common.a
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
CORE_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
CORE_ARCHIVE := $(BUILD_DIR)/libcalendar-plus-core.a
MULTIARCH ?= $(shell $(CC) -dumpmachine)
LIBDIR ?= $(PREFIX)/lib/$(MULTIARCH)

CPPFLAGS += -Isrc -I$(INFILTRATR_COMMON_DIR)/include \
	-DCALENDAR_PLUS_VERSION=\"$(VERSION)\" \
	-DCALENDAR_PLUS_SOURCE_ID=\"calendar-plus-$(VERSION)\" \
	-DCALENDAR_PLUS_BUILD_PROFILE=\"$(BUILD_MODE)\" \
	-DGETTEXT_PACKAGE=\"$(UUID)\" \
	-DU_DISABLE_RENAMING=1
BUILD_MODE ?= generic
GENERIC_CFLAGS := -O2 -g
NATIVE_CFLAGS := -O3 -g -march=native -mtune=native -flto=auto
COMMON_CFLAGS := -std=c11 -fPIC -Wall -Wextra -Wpedantic -Werror \
	-Wformat=2 -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
	-Wconversion -Wsign-conversion -Wnull-dereference \
	-fstack-protector-strong -fno-common \
	"-ffile-prefix-map=$(CURDIR)=." "-fdebug-prefix-map=$(CURDIR)=." \
	"-fmacro-prefix-map=$(CURDIR)=."

ifeq ($(BUILD_MODE),generic)
CFLAGS ?= $(GENERIC_CFLAGS)
CFLAGS += $(COMMON_CFLAGS)
BUILD_DESCRIPTION := generic amd64-compatible (Debian/Mint ICU runtime bridge)
else ifeq ($(BUILD_MODE),native)
CFLAGS ?=
override CFLAGS += $(NATIVE_CFLAGS) $(COMMON_CFLAGS)
LDFLAGS += -flto=auto
BUILD_DESCRIPTION := local hardware-native (-O3 -march=native -mtune=native -flto=auto)
else
$(error Unsupported BUILD_MODE '$(BUILD_MODE)'; use generic or native)
endif

REPRO_SEED_PREFIX ?= calendar-plus-$(VERSION)
LDFLAGS += -Wl,-z,relro,-z,now -Wl,--as-needed
SHARED_LDFLAGS = $(LDFLAGS) -Wl,--version-script=src/calendar-plus.map
GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags gobject-2.0 icu-i18n)
GLIB_LIBS = $(shell $(PKG_CONFIG) --libs gobject-2.0)
CORE_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0 icu-i18n)
CORE_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0)
GMODULE_CFLAGS = $(shell $(PKG_CONFIG) --cflags gmodule-2.0)
GMODULE_LIBS = $(shell $(PKG_CONFIG) --libs gmodule-2.0)
ICU_BRIDGE_LIBS = -ldl -pthread
MATH_LIBS = -lm
GIR_INCLUDE_PATH ?= $(shell $(PKG_CONFIG) --variable=libdir glib-2.0)/gir-1.0
GIR_SHARE_INCLUDE_PATH ?= $(PREFIX)/share/gir-1.0

SOURCE_DATE_EPOCH ?= 1786682280
COVERAGE_MIN_LINES ?= 65
DIST_FILES := \
	.github \
	.gitmodules \
	COPYING \
	Makefile \
	README.md \
	runtime-sources.sha256 \
	calendar-plus@the-infiltratr \
	debian \
	po \
	shared \
	src \
	tests \
	tools

.PHONY: all check check-deps clean common-bootstrap common-check core-check coverage install package-source \
	package-local-installer \
	sanitize static-analysis test validate-architecture validate-js validate-package-inputs \
	validate-sources validate-exports validate-abi validate-runtime-deps validate-release-model smoke-gjs \
	path-space-smoke release-check \
	reproducible-build translations update-pot validate-translations \
	update-settings validate-settings-generated update-runtime-hashes

all: common-check check-deps \
	$(INFILTRATR_COMMON_ARCHIVE) \
	$(CORE_ARCHIVE) \
	$(BUILD_DIR)/$(LIB_REALNAME) \
	$(BUILD_DIR)/$(GIR).typelib \
	$(BUILD_DIR)/$(ABOUT_BINARY) \
	translations

common-bootstrap: common-check
	@:

common-check:
	@if test ! -f "$(INFILTRATR_COMMON_DIR)/VERSION"; then \
		command -v git >/dev/null 2>&1 || { \
			echo "git is required to retrieve the pinned shared source." >&2; \
			exit 1; \
		}; \
		if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
			git submodule update --init --depth 1 -- "$(INFILTRATR_COMMON_DIR)"; \
		else \
			mkdir -p "$(dir $(INFILTRATR_COMMON_DIR))"; \
			git clone --depth 1 --branch "$(INFILTRATR_COMMON_TAG)" \
				"$(INFILTRATR_COMMON_URL)" "$(INFILTRATR_COMMON_DIR)"; \
		fi; \
	fi
	@test -f "$(INFILTRATR_COMMON_DIR)/VERSION" || { \
		echo "Unable to retrieve Infiltratr Common $(INFILTRATR_COMMON_VERSION)." >&2; \
		exit 1; \
	}
	@test "$$(tr -d '[:space:]' < "$(INFILTRATR_COMMON_DIR)/VERSION")" = \
		"$(INFILTRATR_COMMON_VERSION)" || { \
		echo "Infiltratr Common $(INFILTRATR_COMMON_VERSION) is required." >&2; \
		exit 1; \
	}
	@actual_commit=$$(git -C "$(INFILTRATR_COMMON_DIR)" rev-parse HEAD 2>/dev/null || true); \
		if test -n "$$actual_commit" && test "$$actual_commit" != "$(INFILTRATR_COMMON_COMMIT)"; then \
			echo "Infiltratr Common must be pinned to $(INFILTRATR_COMMON_COMMIT)." >&2; \
			exit 1; \
		fi
check-deps: common-check
	@command -v "$(PKG_CONFIG)" >/dev/null || { \
		echo "Missing build dependency: pkg-config" >&2; exit 1; }
	@$(G_IR_SCANNER) --version >/dev/null 2>&1 || { \
		echo "Missing build dependency: g-ir-scanner" >&2; exit 1; }
	@$(G_IR_COMPILER) --version >/dev/null 2>&1 || { \
		echo "Missing build dependency: g-ir-compiler" >&2; exit 1; }
	@$(PKG_CONFIG) --exists gobject-2.0 || { \
		echo "Missing build dependency: libglib2.0-dev" >&2; exit 1; }
	@$(PKG_CONFIG) --exists glib-2.0 || { \
		echo "Missing build dependency: libglib2.0-dev" >&2; exit 1; }
	@$(PKG_CONFIG) --exists icu-i18n || { \
		echo "Missing build dependency: libicu-dev" >&2; exit 1; }
	@command -v msgfmt >/dev/null || { \
		echo "Missing build dependency: gettext" >&2; exit 1; }
	@command -v python3 >/dev/null || { \
		echo "Missing test dependency: python3" >&2; exit 1; }
	@command -v node >/dev/null || { \
		echo "Missing test dependency: nodejs" >&2; exit 1; }
	@command -v rg >/dev/null || { \
		echo "Missing test dependency: ripgrep" >&2; exit 1; }
	@command -v gjs >/dev/null || { \
		echo "Missing test dependency: gjs" >&2; exit 1; }

$(INFILTRATR_COMMON_SOURCES) $(INFILTRATR_COMMON_HEADERS): | common-check
	@test -f "$@" || { echo "Unable to materialize pinned Infiltratr Common source file: $@" >&2; exit 1; }

$(BUILD_DIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-$(notdir $<) $(GLIB_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/infiltratr-%.o: $(INFILTRATR_COMMON_DIR)/src/%.c \
		$(INFILTRATR_COMMON_HEADERS)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-frandom-seed=$(REPRO_SEED_PREFIX)-infiltratr-$* -MMD -MP -c $< -o $@

$(INFILTRATR_COMMON_ARCHIVE): $(INFILTRATR_COMMON_OBJECTS) | common-check
	@rm -f "$@"
	$(AR) rcsD "$@" $(INFILTRATR_COMMON_OBJECTS)

$(CORE_ARCHIVE): $(CORE_OBJECTS)
	@rm -f "$@"
	$(AR) rcsD "$@" $(CORE_OBJECTS)

$(BUILD_DIR)/$(LIB_REALNAME): $(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE) \
		src/calendar-plus.map
	$(CC) -shared $(SHARED_LDFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-link -Wl,-soname,$(LIB_SONAME) -o $@ \
		$(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE) $(GLIB_LIBS) \
		$(ICU_BRIDGE_LIBS) $(MATH_LIBS)
	ln -sfn "$(LIB_REALNAME)" "$(BUILD_DIR)/$(LIB_SONAME)"
	ln -sfn "$(LIB_SONAME)" "$(BUILD_DIR)/lib$(LIB_BASENAME).so"

$(BUILD_DIR)/$(ABOUT_BINARY): src/about-dialog.c src/project-info.c \
		src/project-info.h $(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-frandom-seed=$(REPRO_SEED_PREFIX)-about-dialog \
		$(GLIB_CFLAGS) $(GMODULE_CFLAGS) src/about-dialog.c src/project-info.c \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(GMODULE_LIBS) $(MATH_LIBS)
	chmod 0755 $@

translations:
	@mkdir -p "$(BUILD_DIR)/locale"
	@while IFS= read -r language; do \
		case "$$language" in ''|'#'*) continue ;; esac; \
		mkdir -p "$(BUILD_DIR)/locale/$$language/LC_MESSAGES"; \
		msgfmt --check --statistics \
			-o "$(BUILD_DIR)/locale/$$language/LC_MESSAGES/$(UUID).mo" \
			"po/$$language.po"; \
	done < po/LINGUAS

update-pot:
	python3 tools/update-translations.py

validate-translations:
	python3 tools/update-translations.py --check

update-settings:
	python3 tools/update-settings.py

validate-settings-generated:
	python3 tools/update-settings.py --check

$(BUILD_DIR)/$(GIR).gir: \
		$(PUBLIC_HEADERS) \
		$(SOURCES) \
		$(BUILD_DIR)/$(LIB_REALNAME)
	@mkdir -p "$(BUILD_DIR)/tmp"
	LD_LIBRARY_PATH="$(abspath $(BUILD_DIR))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" \
	TMPDIR="$(abspath $(BUILD_DIR)/tmp)" \
	$(G_IR_SCANNER) \
		--quiet \
		--warn-all \
		--no-libtool \
		--namespace=CalendarPlus \
		--nsversion=1.0 \
		--identifier-prefix=CalendarPlus \
		--symbol-prefix=calendar_plus \
		--add-include-path="$(GIR_INCLUDE_PATH)" \
		--add-include-path="$(GIR_SHARE_INCLUDE_PATH)" \
		--include=GLib-2.0 \
		--include=GObject-2.0 \
		--library=$(LIB_BASENAME) \
		--library-path="$(abspath $(BUILD_DIR))" \
		--cflags-begin $(CPPFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-gir $(GLIB_CFLAGS) --cflags-end \
		--output=$@ \
		$(PUBLIC_HEADERS) $(SOURCES)

$(BUILD_DIR)/$(GIR).typelib: $(BUILD_DIR)/$(GIR).gir
	$(G_IR_COMPILER) \
		--includedir="$(GIR_INCLUDE_PATH)" \
		--includedir="$(GIR_SHARE_INCLUDE_PATH)" \
		$< \
		--output=$@

$(BUILD_DIR)/test-time-formats: \
		tests/test-time-formats.c \
		$(SOURCES) \
		$(HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-time-formats $(GLIB_CFLAGS) \
		tests/test-time-formats.c $(SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(GLIB_LIBS) \
		$(ICU_BRIDGE_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-properties: \
		tests/test-properties.c \
		$(SOURCES) \
		$(HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-properties $(GLIB_CFLAGS) \
		tests/test-properties.c $(SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(GLIB_LIBS) \
		$(ICU_BRIDGE_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-portable-core: \
		tests/test-portable-core.c \
		$(CORE_SOURCES) \
		$(CORE_HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-portable-core $(CORE_CFLAGS) \
		tests/test-portable-core.c $(CORE_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(CORE_LIBS) \
		$(ICU_BRIDGE_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-infiltratr-common: \
		$(INFILTRATR_COMMON_DIR)/tests/portable_smoke.c \
		$(INFILTRATR_COMMON_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-frandom-seed=$(REPRO_SEED_PREFIX)-infiltratr-common-test $< \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(MATH_LIBS)

test: $(BUILD_DIR)/test-time-formats $(BUILD_DIR)/test-properties \
	$(BUILD_DIR)/test-portable-core \
	$(BUILD_DIR)/test-infiltratr-common \
	$(BUILD_DIR)/$(ABOUT_BINARY)
	./$(BUILD_DIR)/test-time-formats
	./$(BUILD_DIR)/test-properties
	./$(BUILD_DIR)/test-portable-core
	./$(BUILD_DIR)/test-infiltratr-common
	@./$(BUILD_DIR)/$(ABOUT_BINARY) --print-metadata | \
		grep -qx 'version=$(VERSION)'
	@./$(BUILD_DIR)/$(ABOUT_BINARY) --print-metadata | \
		grep -qx 'common-library=infiltratr-common-$(INFILTRATR_COMMON_VERSION)'

validate-js: validate-settings-generated
	@python3 -c 'import json; \
	from pathlib import Path; \
	p=Path("$(UUID)"); \
	[json.load(open(p / n, encoding="utf-8")) for n in ("metadata.json", "settings-schema.json")]; \
	assert json.load(open(p / "metadata.json", encoding="utf-8"))["uuid"] == "$(UUID)"'
	python3 tests/test-settings.py
	node tests/test-js-runtime.js
	@for source in "$(UUID)"/*.js; do node --check "$$source"; done
	@! rg -n 'const UUID = "calendar@cinnamon\.org"' "$(UUID)" || { \
		echo "Stock applet UUID is configured as the runtime identity." >&2; exit 1; }


validate-exports: $(BUILD_DIR)/$(LIB_REALNAME)
	@command -v nm >/dev/null || { \
		echo "Missing validation dependency: nm" >&2; exit 1; }
	@nm -D --defined-only --format=posix "$(BUILD_DIR)/$(LIB_REALNAME)" | \
		awk '$$1 ~ /^calendar_plus_/ { sub(/@.*/, "", $$1); print $$1 }' | LC_ALL=C sort -u > \
		"$(BUILD_DIR)/exported-symbols.actual"
	@LC_ALL=C sort -u tests/exported-symbols.txt > \
		"$(BUILD_DIR)/exported-symbols.expected"
	diff -u "$(BUILD_DIR)/exported-symbols.expected" \
		"$(BUILD_DIR)/exported-symbols.actual"

validate-abi: $(BUILD_DIR)/$(LIB_REALNAME)
	@command -v readelf >/dev/null || { \
		echo "Missing validation dependency: readelf" >&2; exit 1; }
	python3 tests/test-abi.py --library "$(BUILD_DIR)/$(LIB_REALNAME)"

validate-runtime-deps: $(BUILD_DIR)/$(LIB_REALNAME)
	@command -v readelf >/dev/null || { \
		echo "Missing validation dependency: readelf" >&2; exit 1; }
	@needed=$$(readelf -d "$(BUILD_DIR)/$(LIB_REALNAME)" | \
		sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p'); \
	if printf '%s\n' "$$needed" | grep -Eq '^libicu(i18n|uc|data)\.so'; then \
		echo "Generic runtime must not embed an ICU-major DT_NEEDED entry:" >&2; \
		printf '%s\n' "$$needed" >&2; \
		exit 1; \
	fi; \

smoke-gjs: $(BUILD_DIR)/$(GIR).typelib
	LD_LIBRARY_PATH="$(abspath $(BUILD_DIR))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" \
	GI_TYPELIB_PATH="$(abspath $(BUILD_DIR))$${GI_TYPELIB_PATH:+:$$GI_TYPELIB_PATH}" \
	CALENDAR_PLUS_EXPECTED_VERSION="$(VERSION)" \
		gjs tests/smoke-typelib.js

update-runtime-hashes:
	python3 tools/update-runtime-hashes.py

validate-sources:
	python3 tools/update-runtime-hashes.py --check
	sha256sum --check runtime-sources.sha256

path-space-smoke:
	tools/path-space-build.sh


validate-package-inputs:
	python3 tools/validate-package-inputs.py

validate-release-model:
	python3 tests/test-release-model.py

validate-architecture:
	python3 tests/test-architecture.py

core-check: check-deps $(CORE_ARCHIVE) $(BUILD_DIR)/test-portable-core \
	validate-architecture
	./$(BUILD_DIR)/test-portable-core

check: all test validate-js validate-translations validate-sources validate-package-inputs \
	validate-release-model validate-architecture validate-exports validate-abi validate-runtime-deps smoke-gjs path-space-smoke

sanitize:
	$(MAKE) clean
	ASAN_OPTIONS="detect_leaks=0:halt_on_error=1" \
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
	$(MAKE) CFLAGS="-O1 -g -std=c11 -fPIC -Wall -Wextra -Wpedantic \
		-Werror -Wformat=2 -Wshadow -Wstrict-prototypes \
		-Wmissing-prototypes -fsanitize=address,undefined \
		-fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address,undefined" test
	@for test_path in \
		/portable/clock-interfaces \
		/portable/clock-destroy-during-tick \
		/portable/event-source \
		/portable/time-catalogue; do \
		ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
		UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
			./$(BUILD_DIR)/test-portable-core -p "$$test_path"; \
	done

coverage:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O0 -g -std=c11 -fPIC -Wall -Wextra -Wpedantic \
		-Werror -Wformat=2 -Wshadow -Wstrict-prototypes \
		-Wmissing-prototypes --coverage" \
		LDFLAGS="--coverage" test
	@command -v gcovr >/dev/null || { \
		echo "gcovr is required for the coverage gate" >&2; exit 1; }
	gcovr --root . --filter src --exclude 'src/about-dialog.c' \
		--print-summary --fail-under-line $(COVERAGE_MIN_LINES) \
		--html-details -o "$(BUILD_DIR)/coverage.html"

static-analysis:
	@command -v clang-tidy >/dev/null || { \
		echo "clang-tidy is not installed" >&2; exit 1; }
	clang-tidy \
		-checks='-*,clang-analyzer-*,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-reserved-identifier,performance-*,portability-*' \
		-warnings-as-errors='clang-analyzer-*,bugprone-*,performance-*,portability-*' \
		$(SOURCES) -- $(CPPFLAGS) $(GLIB_CFLAGS) \
		-std=c11 -Wall -Wextra -Wpedantic

install: all
	install -Dm755 "$(BUILD_DIR)/$(LIB_REALNAME)" \
		"$(DESTDIR)$(LIBDIR)/$(LIB_REALNAME)"
	ln -sfn "$(LIB_REALNAME)" \
		"$(DESTDIR)$(LIBDIR)/$(LIB_SONAME)"
	install -Dm644 "$(BUILD_DIR)/$(GIR).typelib" \
		"$(DESTDIR)$(LIBDIR)/girepository-1.0/$(GIR).typelib"
	install -Dm755 "$(BUILD_DIR)/$(ABOUT_BINARY)" \
		"$(DESTDIR)$(PREFIX)/libexec/$(ABOUT_BINARY)"
	install -d \
		"$(DESTDIR)$(PREFIX)/share/cinnamon/applets/$(UUID)"
	install -m644 "$(UUID)"/*.js "$(UUID)"/*.json \
		"$(DESTDIR)$(PREFIX)/share/cinnamon/applets/$(UUID)/"
	@while IFS= read -r language; do \
		case "$$language" in ''|'#'*) continue ;; esac; \
		install -Dm644 \
			"$(BUILD_DIR)/locale/$$language/LC_MESSAGES/$(UUID).mo" \
			"$(DESTDIR)$(PREFIX)/share/locale/$$language/LC_MESSAGES/$(UUID).mo"; \
	done < po/LINGUAS
	install -Dm644 COPYING \
		"$(DESTDIR)$(PREFIX)/share/licenses/calendar-plus/COPYING"
	install -d "$(DESTDIR)$(PREFIX)/share/doc/calendar-plus"
	install -m644 README.md \
		"$(DESTDIR)$(PREFIX)/share/doc/calendar-plus/"
	printf 'Calendar Plus source version: %s\nBuild mode: %s\nShared C library: Infiltratr Common %s\n' \
		"$(VERSION)" "$(BUILD_DESCRIPTION)" "$(INFILTRATR_COMMON_VERSION)" \
		> "$(BUILD_DIR)/BUILD-INFO"
	install -m644 "$(BUILD_DIR)/BUILD-INFO" \
		"$(DESTDIR)$(PREFIX)/share/doc/calendar-plus/BUILD-INFO"

package-source: common-check validate-settings-generated
	@mkdir -p "$(DIST_DIR)"
	@rm -rf "$(BUILD_DIR)/source-stage"
	@mkdir -p "$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)"
	@tar --exclude='.git' --exclude='*/.git' -cf - $(DIST_FILES) | \
		tar -C "$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)" -xf -
	@rm -rf \
		"$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)/debian/.debhelper" \
		"$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)/debian/calendar-plus" \
		"$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)/debian/calendar-plus-dbgsym"
	@rm -f \
		"$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)/debian/files" \
		"$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)/debian/"*.substvars \
		"$(BUILD_DIR)/source-stage/Calendar-Plus-$(VERSION)/debian/debhelper-build-stamp"
	@find "$(BUILD_DIR)/source-stage" -depth -type d -name __pycache__ -exec rm -rf {} +
	@find "$(BUILD_DIR)/source-stage" -type f \
		\( -name '*.pyc' -o -name '*.pyo' \) -delete
	@find "$(BUILD_DIR)/source-stage" -exec \
		touch --date="@$(SOURCE_DATE_EPOCH)" {} +
	tar --sort=name \
		--mtime="@$(SOURCE_DATE_EPOCH)" \
		--owner=0 --group=0 --numeric-owner \
		-C "$(BUILD_DIR)/source-stage" \
		-cf - "Calendar-Plus-$(VERSION)" | \
		gzip -n -9 > \
		"$(BUILD_DIR)/Calendar-Plus-$(VERSION)-local-source.tar.gz"
	@rm -f "$(DIST_DIR)/Calendar-Plus-$(VERSION)-local-source.zip"
	cd "$(BUILD_DIR)/source-stage" && \
		find "Calendar-Plus-$(VERSION)" -print | LC_ALL=C sort | \
		TZ=UTC zip -X -9 -q \
			"$(abspath $(DIST_DIR))/Calendar-Plus-$(VERSION)-local-source.zip" -@
	@rm -rf "$(BUILD_DIR)/source-stage"

package-local-installer: package-source
	tools/build-local-installer.sh \
		"$(BUILD_DIR)/Calendar-Plus-$(VERSION)-local-source.tar.gz" \
		"$(DIST_DIR)/calendar-plus-$(VERSION)-local-folder.run"

release-check:
	tools/release-check.sh

reproducible-build:
	tools/reproducible-build.sh

clean:
	rm -rf "$(BUILD_DIR)"

-include $(OBJECTS:.o=.d)
