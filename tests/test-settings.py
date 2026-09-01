#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Static contracts for settings that cross the Cinnamon/C boundary."""

import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
METADATA = json.loads((PROJECT_ROOT / "src/cinnamon/metadata.json").read_text())
VERSION = METADATA["version"]
APPLET_DIR = PROJECT_ROOT / "src/cinnamon"


def main() -> None:
    schema = json.loads(
        (APPLET_DIR / "settings-schema.json").read_text(encoding="utf-8")
    )
    applet_source = (APPLET_DIR / "applet.js").read_text(encoding="utf-8")
    calendar_source = (APPLET_DIR / "calendar.js").read_text(encoding="utf-8")
    event_source = (APPLET_DIR / "eventView.js").read_text(encoding="utf-8")
    event_manager_source = (APPLET_DIR / "eventManager.js").read_text(encoding="utf-8")
    runtime_source = (APPLET_DIR / "runtimeSupport.js").read_text(encoding="utf-8")
    panel_clock_source = (APPLET_DIR / "panelClock.js").read_text(encoding="utf-8")

    clock_modes = schema["clock-mode"]["options"]
    expected_modes = [
        "standard",
        "standard-24",
        "standard-12",
        "internet",
        "unix",
        "binary",
        "hexadecimal",
        "julian",
        "modified-julian",
        "sidereal",
        "solar",
        "mean-solar",
        "decimal",
        "chinese-time",
        "chinese-ke",
        "roman-temporal",
        "japanese-temporal",
        "italian-hours",
        "babylonian-hours",
        "indian-ghati",
        "nuremberg-hours",
    ]
    assert list(clock_modes.values()) == expected_modes
    assert clock_modes["Astronomical Julian Date (JD)"] == "julian"
    assert clock_modes["Modified Julian Date (MJD)"] == "modified-julian"

    seconds = schema["show-seconds"]
    assert seconds["type"] == "switch"
    assert seconds["default"] is False

    location_configured = schema["location-configured"]
    assert location_configured["type"] == "switch"
    assert location_configured["default"] is False

    latitude = schema["latitude"]
    assert latitude["type"] == "spinbutton"
    assert latitude["default"] == 0.0
    assert latitude["min"] == -90.0
    assert latitude["max"] == 90.0
    assert latitude["dependency"] == "location-configured"

    longitude = schema["longitude"]
    assert longitude["type"] == "spinbutton"
    assert longitude["default"] == 0.0
    assert longitude["min"] == -180.0
    assert longitude["max"] == 180.0
    assert longitude["dependency"] == "location-configured"

    assert '"location-configured"' in applet_source
    assert '"location_configured"' in applet_source
    assert "this._migrateLocationSetting();" in applet_source
    assert "locationConfigured: this.location_configured" in applet_source
    assert "time_mode_requires_longitude(" in panel_clock_source
    assert "time_mode_requires_latitude(" in panel_clock_source
    assert '"N/A LOC"' in panel_clock_source

    assert (
        'this.settings.bind("show-seconds", "show_seconds", '
        "this._onSettingsChanged);"
    ) in applet_source
    assert (
        'this.settings.bind("latitude", "latitude", '
        "this._onSettingsChanged);"
    ) in applet_source
    assert (
        'this.settings.bind("longitude", "longitude", '
        "this._onSettingsChanged);"
    ) in applet_source
    assert "systemClock.start_at_location(" in panel_clock_source
    assert "@${latitude.toFixed(4)}" not in panel_clock_source
    assert "this.clock_mode" in applet_source
    assert "this.show_seconds" in applet_source
    assert "this.latitude" in applet_source
    assert "this.longitude" in applet_source
    assert "CalendarPlus.SystemClock.new()" in applet_source

    expected_calendars = [
    "gregorian",
    "iso-week",
    "julian",
    "revised-julian",
    "hebrew",
    "islamic-umalqura",
    "islamic-civil",
    "islamic-tbla",
    "islamic",
    "persian",
    "bahai",
    "buddhist",
    "coptic",
    "ethiopian",
    "ethiopic-amete-alem",
    "chinese",
    "dangi",
    "indian",
    "japanese",
    "minguo",
    "roman",
    "byzantine",
    "egyptian-nabonassar",
    "armenian-traditional",
    "mayan",
    "french-republican",
    "swedish-historical",
    "international-fixed",
    "world",
    "positivist",
]
    assert list(schema["primary-calendar"]["options"].values()) == expected_calendars
    assert list(schema["secondary-calendar"]["options"].values()) == [
        "none",
        *expected_calendars,
    ]
    assert schema["primary-calendar"]["default"] == "gregorian"
    assert schema["secondary-calendar"]["default"] == "none"
    assert '"primary-calendar"' in applet_source
    assert '"secondary-calendar"' in applet_source
    assert "CalendarPlus.CalendarSystem.new(" in applet_source
    assert "CalendarPlus.CalendarSystem.new(" in calendar_source
    assert "this._calendar.setCalendarSystem(" in applet_source
    assert "this._calendarSystem.add_months_parts(" in calendar_source
    assert "this._calendarSystem.add_years_parts(" in calendar_source
    assert "this._calendarSystem.build_grid(" in calendar_source
    assert "CalendarPlus.DatePart.DAY" in calendar_source
    assert "CalendarPlus.DatePart.SHORT" in applet_source
    assert "CalendarPlus.DatePart.FULL" in panel_clock_source
    assert ".format_date_part(" in calendar_source
    assert ".format_date_part(" in applet_source
    assert ".format_date_part(" in panel_clock_source
    assert ".format_date(" not in calendar_source
    assert ".format_date(" not in applet_source
    assert ".format_date(" not in panel_clock_source
    assert "while (cellsPlaced < 42)" not in calendar_source

    # Date equality, work-week semantics and navigation are native contracts.
    # JavaScript passes typed date parts and never maintains a parallel date
    # arithmetic implementation.
    assert "CalendarPlus.date_same(" in calendar_source
    assert "CalendarPlus.date_is_work_day(" in calendar_source
    assert "function _isWorkDay(" not in calendar_source
    assert "_dateFromIso" not in calendar_source

    # CalendarServer tuples, interval queries, culling and sorting belong to
    # the native store. JavaScript retains only Cinnamon's D-Bus and actor APIs.
    assert "CalendarPlus.EventStore.new()" in event_manager_source
    assert "this.event_store.add_or_update(" in event_manager_source
    assert "this.event_store.get_snapshot(" in event_manager_source
    assert "this.event_store.get_colors(" in event_manager_source
    assert "this.event_store.refresh_timezone()" in event_manager_source
    assert "CalendarPlus.event_day_relation(" in event_manager_source
    assert "CalendarPlus.event_timing(" in event_manager_source
    assert "CalendarPlus.EventState." in event_source
    assert "CalendarPlus.EventDayRelation." in event_source
    for legacy in (
        "starts_on_day(date)",
        "ends_on_day(date)",
        "started_before_day(date)",
        "ended_before_day(date)",
        "ends_after_day(date)",
        "started_after_day(date)",
    ):
        assert legacy not in event_manager_source
        assert legacy not in event_source
    assert "class EventDataList" not in event_manager_source
    assert "this.events_by_date" not in event_manager_source

    # The applet owns this preference; it must not silently fall back to the
    # separate system-wide seconds switch again.
    assert 'get_boolean("clock-show-seconds")' not in applet_source

    # Standard horizontal clocks need all combinations of date, 12/24-hour
    # mode and seconds while retaining Cinnamon's locale-aware formatting.
    required_formats = {
        "withDate24Seconds",
        "withDate12Seconds",
        "withDate24",
        "withDate12",
        "withoutDate24Seconds",
        "withoutDate12Seconds",
        "withoutDate24",
        "withoutDate12",
    }
    assert all(name in panel_clock_source for name in required_formats)

    # The three conventional choices stay in Cinnamon's locale-aware path.
    assert 'var CLOCK_MODE_STANDARD = "standard";' in panel_clock_source
    assert 'var CLOCK_MODE_STANDARD_24 = "standard-24";' in panel_clock_source
    assert 'var CLOCK_MODE_STANDARD_12 = "standard-12";' in panel_clock_source
    assert "function isNativeClockMode(mode)" in panel_clock_source
    assert 'RuntimeSupport.loadLocalModule("panelClock")' in applet_source
    assert "PanelClock.panelText(" in applet_source
    assert "PanelClock.todayDisplay(" in applet_source

    # Construction must be atomic. Essential native state is established
    # before actors are built; a failed build cleans up and rethrows instead
    # of leaving Cinnamon with a partly initialised applet.
    assert "this._initialiseState(orientation, expectedVersion);" in applet_source
    assert "this._buildApplet();" in applet_source
    assert "this._destroy();\n            throw error;" in applet_source
    assert applet_source.index("this._initialiseState(orientation, expectedVersion);") < \
        applet_source.index("this._buildApplet();")
    assert "on_applet_removed_from_panel()" in applet_source
    assert "this.events_manager.destroy();" in applet_source
    assert "this.settings.finalize();" in applet_source

    # D-Bus watches, cancellables, GLib sources and proxy signals have a
    # deterministic lifecycle when the applet is removed or construction
    # aborts.
    assert "this._bus_watch_id = 0;" in event_manager_source
    assert "this._serverSignals = new SignalBag();" in event_manager_source
    assert "this._cancellable = new Gio.Cancellable();" in event_manager_source
    assert "Gio.bus_unwatch_name(this._bus_watch_id);" in event_manager_source
    assert "() => this._calendarServerVanished()" in event_manager_source
    assert "this._scheduleReconnect();" in event_manager_source
    assert "this._cancelReconnect();" in event_manager_source
    assert "this._cancellable.cancel();" in event_manager_source
    assert 'Gio.File.new_for_path("/etc/localtime")' in event_manager_source
    assert "this._timezone_monitor.cancel();" in event_manager_source
    assert "destroy()" in event_manager_source
    assert "this._bus_watch_id\n" not in event_manager_source

    # The month view is also a long-lived signal/source owner. SignalBag keeps
    # those ownership groups explicit, and pending GLib sources are cancelled
    # before the actor graph is destroyed.
    assert "this._eventSignals = new SignalBag();" in calendar_source
    assert "this._desktopSignals = new SignalBag();" in calendar_source
    assert "this._actorSignals = new SignalBag();" in calendar_source
    assert "_cancel_set_date_idle()" in calendar_source
    assert "this._calendar.destroy();" in applet_source
    assert calendar_source.count('"style-changed"') == 1
    assert "disconnectAll()" in calendar_source
    assert "destroy() {" in calendar_source

    # Event fetching follows the native 42-cell grid rather than assuming that
    # every primary calendar shares Gregorian month boundaries.
    assert "set_visible_range(firstDate, lastDate, force)" in event_manager_source
    assert "this.events_manager.set_visible_range(" in calendar_source
    assert "fetch_month_events" not in event_manager_source
    assert "current_month_year" not in event_manager_source

    # A stale native library must be rejected explicitly rather than allowed
    # to fail later through a missing or incompatible symbol.
    assert "const APP_VERSION" not in applet_source
    assert "metadata.version" in applet_source
    assert "CalendarPlus.get_version()" in applet_source
    assert applet_source.index("this._signals = new SignalBag();") < \
        applet_source.index("CalendarPlus.get_version()")
    assert "native library ${nativeVersion} does not match " in applet_source
    assert "`applet ${expectedVersion}`" in applet_source

    # Shared runtime mechanics live in one module; feature modules own only
    # their domain state. Transport and agenda presentation are separate.
    assert runtime_source.count("var SignalBag = class SignalBag") == 1
    for source in (applet_source, calendar_source, event_source, event_manager_source, panel_clock_source):
        assert "class SignalBag" not in source
    assert 'RuntimeSupport.loadLocalModule("eventManager")' in applet_source
    assert "class EventList" not in event_manager_source
    assert "class EventRow" not in event_manager_source
    assert "var EventList = class EventList" in event_source
    assert "class EventRow" in event_source
    assert "new EventManager.EventsManager(" in applet_source

    # Calendar Plus-owned interface text uses its own installed gettext domain;
    # Cinnamon-provided desktop strings remain in Cinnamon's catalogue.
    for source in (applet_source, calendar_source, event_source):
        assert '"calendar-plus@the-infiltratr"' in source
        assert "Gettext.bindtextdomain(" in source
    for label in (
        "Previous month",
        "Next month",
        "Previous year",
        "Next year",
        "Week",
    ):
        assert f'CP_("{label}")' in calendar_source
    assert 'CP_("Show today")' in applet_source
    assert 'CP_("About Calendar Plus")' in applet_source
    # Cinnamon's automatic right-click About item must use the same native
    # Calendar Plus dialog as the popup item, never xlet-about-dialog.
    assert "openAbout()" in applet_source
    assert "this._onAbout();" in applet_source
    assert 'Util.spawnCommandLine("/usr/libexec/calendar-plus-about")' in applet_source
    assert "xlet-about-dialog" not in applet_source
    assert 'CP_("Open selected date in Calendar")' in event_source
    assert 'CP_("Open Calendar")' in event_source

    # New interactive surfaces remain reachable without a pointer.
    assert "class CalendarPlusApplet extends Applet.Applet" in applet_source
    assert "can_focus: true" in calendar_source
    assert "can_focus: canLaunch" in event_source
    assert event_source.count('find_program_in_path("gnome-calendar")') == 1
    assert "clickable: this._canLaunchCalendar" in event_source
    assert "reactive: this._canLaunchCalendar" in event_source
    assert "can_focus: this._canLaunchCalendar" in event_source
    assert "accessible_role: Atk.Role.LIST_ITEM" in event_source
    assert "accessible_name:" in calendar_source

    # Calendar cells provide spatial keyboard navigation instead of forcing
    # keyboard users to Tab linearly through all 42 cells.
    for key_name in (
        "KEY_Left", "KEY_Right", "KEY_Up", "KEY_Down",
        "KEY_Page_Up", "KEY_Page_Down", "KEY_Home", "KEY_End",
    ):
        assert f"Clutter.{key_name}" in calendar_source
    assert '"key-press-event"' in calendar_source
    assert "grab_key_focus()" in calendar_source

    metadata = json.loads(
        (APPLET_DIR / "metadata.json").read_text(encoding="utf-8")
    )
    assert metadata["cinnamon-version"] == ["6.4", "6.6", "6.7"]


if __name__ == "__main__":
    main()
