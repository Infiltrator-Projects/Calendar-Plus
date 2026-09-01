// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Calendar Plus panel controller.
 *
 * Architecture
 * ------------
 * Cinnamon owns actors, menus, settings bindings and desktop integration.
 * libcalendar-plus owns deterministic calendar arithmetic, non-standard time
 * systems and their boundary-aligned timer.  This file is intentionally the
 * narrow orchestration layer between those two worlds.
 *
 * Lifecycle invariant
 * -------------------
 * _initialiseState establishes every cleanup field before validating the
 * native version or building actors. Later construction can therefore fail
 * without making _destroy() guess which resources exist.
 */

const Applet = imports.ui.applet;
const CalendarPlus = imports.gi.CalendarPlus;
const CinnamonDesktop = imports.gi.CinnamonDesktop;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Gettext = imports.gettext;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Settings = imports.ui.settings;
const Util = imports.misc.util;

const UUID = "calendar-plus@the-infiltratr";
const CALENDAR_PLUS_GETTEXT_DOMAIN = UUID;

Gettext.bindtextdomain(CALENDAR_PLUS_GETTEXT_DOMAIN, "/usr/share/locale");
const CalendarPlusGettext = Gettext.domain(CALENDAR_PLUS_GETTEXT_DOMAIN);

function CP_(text) {
    return CalendarPlusGettext.gettext(text);
}

const FONT_UI_REGULAR =
    'font-family: "MB Corpo S Title WEB";';
const FONT_UI_BOLD =
    'font-family: "MB Corpo S Title WEB"; font-weight: 700;';
const FONT_DISPLAY =
    'font-family: "MB Corpo S Title WEB"; font-weight: 700;';

function _applyTypography(actor, style) {
    if (actor && typeof actor.set_style === "function") {
        actor.set_style(style);
    }
}

function _addStyleClass(actor, styleClass) {
    if (actor && typeof actor.add_style_class_name === "function") {
        actor.add_style_class_name(styleClass);
    }
}

/*
 * Bootstrap only the shared runtime helper here.  Once loaded, it owns the
 * Cinnamon 6.4/6.6/6.7 module-resolution seam for every feature module.
 */
function _loadRuntimeSupport() {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports.runtimeSupport) {
                return extension.imports.runtimeSupport;
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }

    return require("./runtimeSupport");
}

const RuntimeSupport = _loadRuntimeSupport();
const SignalBag = RuntimeSupport.SignalBag;
const Calendar = RuntimeSupport.loadLocalModule("calendar");
const EventManager = RuntimeSupport.loadLocalModule("eventManager");
const EventView = RuntimeSupport.loadLocalModule("eventView");

const PanelClock = RuntimeSupport.loadLocalModule("panelClock");

class CalendarPlusApplet extends Applet.Applet {
    constructor(orientation, panel_height, instance_id, expectedVersion) {
        super(orientation, panel_height, instance_id);

        try {
            this._initialiseState(orientation, expectedVersion);
            this._createPanelLabel();
            this.setAllowedLayout(Applet.AllowedLayout.BOTH);
            this._buildApplet();
        } catch (error) {
            this._destroy();
            throw error;
        }
    }

    _createPanelLabel() {
        _addStyleClass(this.actor, "calendar-plus-applet");
        const label = new St.Label({
            style_class: "applet-label calendar-plus-panel-clock",
        });
        _applyTypography(label, FONT_DISPLAY);
        label.reactive = true;
        label.track_hover = true;
        label.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        const holder = new St.Bin();
        holder.set_child(label);
        this.actor.add(holder, { y_align: St.Align.MIDDLE, y_fill: false });
        this.actor.set_label_actor(label);

        this._clockLabel = label;
        this._labelBin = holder;
    }

    _initialiseState(orientation, expectedVersion) {
        this.orientation = orientation;
        this._destroyed = false;
        this._added_to_panel = false;
        this._keybinding_set = false;
        this._is_entered = false;

        this.clock_mode = PanelClock.CLOCK_MODE_STANDARD;
        this.show_seconds = false;
        this.location_configured = false;
        this.latitude = 0.0;
        this.longitude = 0.0;
        this.primary_calendar = "gregorian";
        this.secondary_calendar = "none";
        this.show_events = true;
        this.use_custom_format = false;
        this.custom_format = "";
        this.custom_tooltip_format = "";
        this.keyOpen = "";

        this.menuManager = null;
        this.menu = null;
        this.settings = null;
        this.desktop_settings = null;
        this.clock = null;
        this.system_clock = null;
        this.events_manager = null;
        this.event_list = null;
        this._calendar = null;
        this._popupBody = null;
        this._calendarColumn = null;
        this._resume_source = null;

        this._signals = new SignalBag();
        this._eventSignals = new SignalBag();
        this._resumeSignals = new SignalBag();

        /*
         * Version identity is a hard boundary, not a feature probe.  Mixing an
         * applet with a stale typelib can otherwise fail much later through a
         * missing symbol and leave Cinnamon with a partially built menu.
         */
        const nativeVersion = CalendarPlus.get_version();
        if (typeof expectedVersion !== "string" || expectedVersion.length === 0) {
            throw new Error(`${UUID}: Cinnamon did not provide an applet version.`);
        }
        if (nativeVersion !== expectedVersion) {
            throw new Error(
                `${UUID}: native library ${nativeVersion} does not match ` +
                `applet ${expectedVersion}`
            );
        }

        this._primary_calendar_system =
            CalendarPlus.CalendarSystem.new("gregorian");
        if (this._primary_calendar_system === null) {
            throw new Error(`${UUID}: native Gregorian calendar unavailable`);
        }
        this._secondary_calendar_system = null;
    }

    _buildApplet() {
        this.menuManager = new PopupMenu.PopupMenuManager(this);
        this.menu = new Applet.AppletPopupMenu(this, this.orientation);
        _addStyleClass(this.menu.actor, "calendar-plus-popup");
        _applyTypography(this.actor, FONT_UI_REGULAR);
        _applyTypography(this.menu.actor, FONT_UI_REGULAR);
        this.menuManager.addMenu(this.menu);
        this.menu.setCustomStyleClass("calendar-background");

        this.settings = new Settings.AppletSettings(this, UUID, this.instance_id);
        this._migrateLocationSetting();
        this.desktop_settings = new Gio.Settings({
            schema_id: "org.cinnamon.desktop.interface",
        });

        this.clock = new CinnamonDesktop.WallClock();
        this.system_clock = CalendarPlus.SystemClock.new();
        this._signals.connect(
            this.system_clock,
            "tick",
            () => this._updatePanelClock()
        );

        this.event_list = new EventView.EventList(
            this.settings,
            this.desktop_settings
        );
        _applyTypography(this.event_list.actor, FONT_UI_REGULAR);
        _applyTypography(this.event_list.selected_date_label, FONT_UI_BOLD);
        this.events_manager = new EventManager.EventsManager(
            this.settings,
            this.desktop_settings,
            this.event_list
        );
        this._eventSignals.connect(
            this.events_manager,
            "events-manager-ready",
            () => this._syncEventVisibility(true)
        );
        this._eventSignals.connect(
            this.events_manager,
            "has-calendars-changed",
            () => this._syncEventVisibility(false)
        );

        this._buildPopupContents();
        this._bindSettings();
        this._watchDesktopPreferences();
        this._watchPointerAndMenu();
        this._startResumeMonitor();
    }

    _buildPopupContents() {
        const body = new St.BoxLayout({
            style_class: "calendar-main-box",
            vertical: false,
        });
        _applyTypography(body, FONT_UI_REGULAR);
        this._popupBody = body;
        this.menu.addActor(body);

        this._eventSignals.connect(this.event_list, "launched-calendar", () => {
            if (this.menu) {
                this.menu.toggle();
            }
        });
        for (const [signal, passEvents] of [
            ["start-pass-events", true],
            ["stop-pass-events", false],
        ]) {
            this._eventSignals.connect(this.event_list, signal, () => {
                if (this.menu) {
                    this.menu.passEvents = passEvents;
                }
            });
        }
        body.add_actor(this.event_list.actor);

        const calendarColumn = new St.BoxLayout({ vertical: true });
        this._calendarColumn = calendarColumn;
        this.go_home_button = new St.Button({
            style_class: "calendar-today-home-button",
            x_align: Clutter.ActorAlign.CENTER,
            reactive: true,
            can_focus: true,
            accessible_name: CP_("Show today"),
        });
        this._today_box = new St.BoxLayout({ vertical: true });
        this.go_home_button.set_child(this._today_box);
        this.go_home_button.connect("clicked", () => this._resetCalendar());

        this._day = new St.Label({ style_class: "calendar-today-day-label" });
        this._date = new St.Label({ style_class: "calendar-today-date-label" });
        this._secondary_date = new St.Label({
            style_class: "calendar-today-date-label",
        });
        _applyTypography(this._day, FONT_UI_BOLD);
        _applyTypography(this._date, FONT_UI_REGULAR);
        _applyTypography(this._secondary_date, FONT_UI_REGULAR);
        this._today_box.add_actor(this._day);
        this._today_box.add_actor(this._date);
        this._today_box.add_actor(this._secondary_date);
        calendarColumn.add_actor(this.go_home_button);

        this._calendar = new Calendar.Calendar(this.settings, this.events_manager);
        this._eventSignals.connect(
            this._calendar,
            "selected-date-changed",
            () => this._updateClockAndDate()
        );
        calendarColumn.add_actor(this._calendar.actor);
        body.add_actor(calendarColumn);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const dateTimeSettings = new PopupMenu.PopupMenuItem(
            _("Date and Time Settings")
        );
        dateTimeSettings.connect("activate", () => this._onLaunchSettings());
        this.menu.addMenuItem(dateTimeSettings);

        const aboutItem = new PopupMenu.PopupMenuItem(CP_("About Calendar Plus"));
        aboutItem.connect("activate", () => this._onAbout());
        this.menu.addMenuItem(aboutItem);
    }

    _bindSettings() {
        this.settings.bind("show-events", "show_events", this._onSettingsChanged);
        this.settings.bind("clock-mode", "clock_mode", this._onSettingsChanged);
        this.settings.bind("show-seconds", "show_seconds", this._onSettingsChanged);
        this.settings.bind(
            "location-configured",
            "location_configured",
            this._onSettingsChanged
        );
        this.settings.bind("latitude", "latitude", this._onSettingsChanged);
        this.settings.bind("longitude", "longitude", this._onSettingsChanged);
        this.settings.bind(
            "primary-calendar",
            "primary_calendar",
            this._onSettingsChanged
        );
        this.settings.bind(
            "secondary-calendar",
            "secondary_calendar",
            this._onSettingsChanged
        );
        this.settings.bind(
            "use-custom-format",
            "use_custom_format",
            this._onSettingsChanged
        );
        this.settings.bind(
            "custom-format",
            "custom_format",
            this._onSettingsChanged
        );
        this.settings.bind(
            "custom-tooltip-format",
            "custom_tooltip_format",
            this._onSettingsChanged
        );
        this.settings.bind("keyOpen", "keyOpen", this._setKeybinding);
        this._setKeybinding();
    }

    _migrateLocationSetting() {
        /*
         * Releases before this setting existed used 0,0 as an unavoidable
         * sentinel. Preserve deliberately entered non-zero coordinates while
         * allowing a genuine 0,0 location to be selected explicitly now.
         */
        if (this.settings.getValue("location-configured")) {
            return;
        }

        const latitude = Number(this.settings.getValue("latitude"));
        const longitude = Number(this.settings.getValue("longitude"));
        if ((Number.isFinite(latitude) && latitude !== 0) ||
            (Number.isFinite(longitude) && longitude !== 0)) {
            this.settings.setValue("location-configured", true);
        }
    }

    _watchDesktopPreferences() {
        for (const key of ["clock-use-24h", "clock-show-date"]) {
            this._signals.connect(
                this.desktop_settings,
                `changed::${key}`,
                () => this._onSettingsChanged()
            );
        }
    }

    _watchPointerAndMenu() {
        /*
         * WallClock chooses its wake-up cadence from the format string.  While
         * the pointer is over the panel label the tooltip format may require a
         * finer cadence than the visible label, so both formats participate.
         */
        this._signals.connect(this.actor, "enter-event", () => {
            this._is_entered = true;
            this._configureWallClock();
        });
        this._signals.connect(this.actor, "leave-event", () => {
            this._is_entered = false;
            this._configureWallClock();
        });

        this._signals.connect(this.menu, "open-state-changed", (menu, open) => {
            if (!open || this._destroyed) {
                return;
            }
            this._resetCalendar();
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
            this._rebalancePopupWidth();
        });
    }

    _startResumeMonitor() {
        try {
            const LoginManager = imports.misc.loginManager;
            if (LoginManager && typeof LoginManager.getLoginManager === "function") {
                this._resume_source = LoginManager.getLoginManager();
                this._resumeSignals.connect(
                    this._resume_source,
                    "prepare-for-sleep",
                    (suspending) => {
                        if (!suspending) {
                            this._onResume();
                        }
                    }
                );
                return;
            }
        } catch (error) {
            /* Older Cinnamon releases use the UPower compatibility signal below. */
        }

        const UPowerGlib = imports.gi.UPowerGlib;
        this._resume_source = new UPowerGlib.Client();
        try {
            this._resumeSignals.connect(
                this._resume_source,
                "notify-resume",
                () => this._onResume()
            );
        } catch (error) {
            this._resumeSignals.connect(
                this._resume_source,
                "notify::resume",
                () => this._onResume()
            );
        }
    }

    _onResume() {
        if (this._destroyed) {
            return;
        }

        /*
         * Native timers use monotonic scheduling.  Suspend pauses that clock,
         * so restarting recalculates the next visible boundary from current
         * civil time rather than firing with a stale pre-suspend remainder.
         */
        if (PanelClock.isNativeClockMode(this.clock_mode) && this.system_clock) {
            this.system_clock.stop();
            this._syncSystemClock();
        }
        this._updateClockAndDate();
    }

    _onSettingsChanged() {
        if (this._destroyed || !this._calendar || !this.events_manager) {
            return;
        }

        this._resetLabelWidth();
        this._syncCalendarSystems();
        this._configureWallClock();
        this._syncSystemClock();
        this._updateClockAndDate();
        this._syncEventVisibility(true);
    }

    _syncCalendarSystems() {
        if (!this._primary_calendar_system ||
            this._primary_calendar_system.get_id() !== this.primary_calendar) {
            const candidate = CalendarPlus.CalendarSystem.new(this.primary_calendar);
            if (candidate) {
                this._primary_calendar_system = candidate;
                this._calendar.setCalendarSystem(this.primary_calendar);
            }
        }

        if (this.secondary_calendar === "none") {
            this._secondary_calendar_system = null;
            return;
        }

        if (!this._secondary_calendar_system ||
            this._secondary_calendar_system.get_id() !== this.secondary_calendar) {
            this._secondary_calendar_system =
                CalendarPlus.CalendarSystem.new(this.secondary_calendar);
        }
    }

    _clockConfig() {
        return {
            mode: this.clock_mode,
            showSeconds: this.show_seconds,
            locationConfigured: this.location_configured,
            latitude: this.latitude,
            longitude: this.longitude,
            useCustomFormat: this.use_custom_format,
            customFormat: this.custom_format,
            customTooltipFormat: this.custom_tooltip_format,
            pointerInside: this._is_entered,
            vertical: this._isVerticalPanel(),
            desktopSettings: this.desktop_settings,
            primaryCalendar: this.primary_calendar,
        };
    }

    _syncSystemClock() {
        PanelClock.syncNativeClock(this.system_clock, this._clockConfig());
    }

    _configureWallClock() {
        PanelClock.configureWallClock(this.clock, this._clockConfig());
    }

    _updatePanelClock() {
        if (this._destroyed || !this.clock) {
            return;
        }

        const text = PanelClock.panelText(
            this.clock,
            this.system_clock,
            this._clockConfig()
        );
        if (text) {
            this._clockLabel.set_text(text);
            this._updateLabelWidth();
        }
    }

    _updateClockAndDate() {
        if (this._destroyed || !this.clock || !this._calendar ||
            !this.events_manager || !this.go_home_button) {
            return;
        }

        this._updatePanelClock();

        const display = PanelClock.todayDisplay(
            this.clock,
            this._primary_calendar_system,
            this._clockConfig()
        );
        const dayName = PanelClock.dayName(this.clock);

        const selectedToday = this._calendar.todaySelected();
        this.go_home_button.reactive = !selectedToday;
        this.go_home_button.set_style_class_name(
            selectedToday
                ? "calendar-today-home-button"
                : "calendar-today-home-button-enabled"
        );

        this._day.set_text(dayName);
        this._date.set_text(display.shortDate);
        this.go_home_button.set_accessible_name(
            `${CP_("Show today")}: ${dayName}, ${display.shortDate}`
        );

        let tooltip = display.tooltip;
        if (this._secondary_calendar_system) {
            const secondDate = this._secondary_calendar_system.format_date_part(
                ...display.args,
                CalendarPlus.DatePart.SHORT
            );
            const secondLine =
                `${this._secondary_calendar_system.get_name()}: ${secondDate}`;
            this._secondary_date.set_text(secondLine);
            this._secondary_date.visible = true;
            tooltip += `\n${secondLine}`;
        } else {
            this._secondary_date.set_text("");
            this._secondary_date.visible = false;
        }

        this.set_applet_tooltip(tooltip);
        this.events_manager.select_date(this._calendar.getSelectedDate());
    }

    _syncEventVisibility(forceRefresh) {
        if (!this.event_list || !this.events_manager || !this._calendar) {
            return;
        }
        this.event_list.actor.visible = this.events_manager.is_active();
        this._rebalancePopupWidth();
        if (forceRefresh && this.events_manager.is_active()) {
            this.events_manager.select_date(
                this._calendar.getSelectedDate(),
                true
            );
        }
    }

    _rebalancePopupWidth() {
        if (this._destroyed || !this._calendarColumn || !this.event_list) {
            return;
        }

        /*
         * Cinnamon's calendar theme gives the agenda a generous natural
         * width. With the full month grid beside it, that can leave the month
         * side cramped even though the popup still has room to grow. Measure
         * the active theme rather than baking pixel dimensions into the applet:
         * the month may grow toward the agenda's natural width, but never by
         * more than 35 percent over its own natural request.
         *
         * Reset min_width before measurement so a previous font/theme result
         * does not become part of the next natural-width request. That keeps
         * the popup able to shrink again after a theme or scaling change.
         */
        this._calendarColumn.min_width = 0;
        if (!this.event_list.actor.visible) {
            return;
        }

        const [, agendaNatural] =
            this.event_list.actor.get_preferred_width(-1);
        const [, calendarNatural] =
            this._calendarColumn.get_preferred_width(-1);
        if (agendaNatural <= 0 || calendarNatural <= 0 ||
            agendaNatural <= calendarNatural) {
            return;
        }

        const target = Math.ceil(Math.min(
            agendaNatural,
            calendarNatural * 1.35
        ));
        this._calendarColumn.min_width = target;
    }

    _setKeybinding() {
        if (this._destroyed) {
            return;
        }
        this._removeKeybinding();
        if (!this.keyOpen) {
            return;
        }

        if (Main.keybindingManager.addXletHotKey) {
            Main.keybindingManager.addXletHotKey(
                this,
                "calendar-open",
                this.keyOpen,
                () => this._openMenu()
            );
        } else {
            Main.keybindingManager.addHotKey(
                `${UUID}-open-${this.instance_id}`,
                this.keyOpen,
                () => this._openMenu()
            );
        }
        this._keybinding_set = true;
    }

    _removeKeybinding() {
        if (!this._keybinding_set) {
            return;
        }
        try {
            if (Main.keybindingManager.removeXletHotKey) {
                Main.keybindingManager.removeXletHotKey(this, "calendar-open");
            } else {
                Main.keybindingManager.removeHotKey(
                    `${UUID}-open-${this.instance_id}`
                );
            }
        } catch (error) {
            global.logError(error);
        }
        this._keybinding_set = false;
    }

    _openMenu() {
        if (!this._destroyed && this.menu) {
            this.menu.toggle();
        }
    }

    _resetCalendar() {
        if (!this._destroyed && this._calendar) {
            this._calendar.setDate(new Date(), true);
        }
    }

    _isVerticalPanel() {
        return this.orientation === St.Side.LEFT ||
            this.orientation === St.Side.RIGHT;
    }

    _resetLabelWidth() {
        if (!this._labelBin) {
            return;
        }
        this._labelBin.min_width = 0;
        this._updateLabelWidth();
    }

    _updateLabelWidth() {
        if (!this._clockLabel || !this._labelBin) {
            return;
        }

        const [, naturalWidth] = this._clockLabel.get_preferred_width(-1);
        const characters = Math.max(1, this._clockLabel.get_text().length);
        if (naturalWidth <= 0) {
            return;
        }

        /*
         * Small width oscillations make the surrounding panel visibly jitter.
         * Grow immediately, but shrink only after roughly two glyph widths.
         */
        const hysteresis = 2 * naturalWidth / characters;
        if (naturalWidth > this._labelBin.min_width ||
            naturalWidth < this._labelBin.min_width - hysteresis) {
            this._labelBin.min_width = naturalWidth;
        }
    }

    _onLaunchSettings() {
        if (this.menu) {
            this.menu.close();
        }
        Util.spawnCommandLine("cinnamon-settings calendar");
    }

    /*
     * Cinnamon's Applet base class normally opens its generic metadata dialog
     * from the right-click context menu.  That dialog exposes the internal UUID
     * and only a small subset of Calendar Plus metadata.  Keep every About
     * entry point on the same native dialog so the application presents one
     * consistent identity regardless of how the user opens it.
     */
    openAbout() {
        this._onAbout();
    }

    /*
     * Cinnamon normally launches its generic xlet-settings process here.
     * That process is outside this applet's St theme tree, so it cannot
     * inherit Calendar Plus typography. Route Configure through the bundled
     * thin GTK host instead; it reuses Cinnamon's own settings renderer and
     * persistence while applying the same MB Corpo family before widgets are
     * constructed.
     */
    configureApplet(tab = 0) {
        if (typeof tab !== "number" || !Number.isFinite(tab)) {
            tab = 0;
        }
        const command = `/usr/share/cinnamon/applets/${UUID}/settings.py ` +
            `--instance ${this.instance_id} --tab ${Math.trunc(tab)}`;
        Util.spawnCommandLine(command);
    }

    _onAbout() {
        if (this.menu) {
            this.menu.close();
        }
        Util.spawnCommandLine("/usr/libexec/calendar-plus-about");
    }

    on_custom_format_button_pressed() {
        Util.spawnCommandLine(
            "xdg-open https://cinnamon-spices.linuxmint.com/strftime.php"
        );
    }

    on_applet_clicked() {
        this._openMenu();
    }

    on_applet_added_to_panel() {
        if (this._destroyed || this._added_to_panel) {
            return;
        }
        this._added_to_panel = true;

        this._signals.connect(this.clock, "notify::clock", () => {
            this._updateClockAndDate();
        });
        this._signals.connect(Main.themeManager, "theme-set", () => {
            this._resetLabelWidth();
            this._rebalancePopupWidth();
        });
        this._signals.connect(global.settings, "changed::panel-edit-mode", () => {
            this._resetLabelWidth();
        });

        this._onSettingsChanged();
        this.events_manager.start_events();
        this._resetCalendar();
    }

    on_panel_height_changed() {
        this._resetLabelWidth();
    }

    on_orientation_changed(orientation) {
        if (this._destroyed) {
            return;
        }
        this.orientation = orientation;
        if (this.menu) {
            this.menu.setOrientation(orientation);
        }
        this._onSettingsChanged();
    }

    on_applet_removed_from_panel() {
        this._destroy();
    }

    _destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._added_to_panel = false;

        this._removeKeybinding();
        this._resumeSignals.disconnectAll();
        this._eventSignals.disconnectAll();
        this._signals.disconnectAll();

        if (this.system_clock) {
            try {
                this.system_clock.stop();
            } catch (error) {
                global.logError(error);
            }
            this.system_clock = null;
        }

        if (this._calendar) {
            try {
                this._calendar.destroy();
            } catch (error) {
                global.logError(error);
            }
            this._calendar = null;
        }

        if (this.events_manager) {
            try {
                this.events_manager.destroy();
            } catch (error) {
                global.logError(error);
            }
            this.events_manager = null;
        }

        if (this.event_list) {
            try {
                this.event_list.destroy();
            } catch (error) {
                global.logError(error);
            }
            this.event_list = null;
        }

        if (this.settings) {
            try {
                this.settings.finalize();
            } catch (error) {
                global.logError(error);
            }
            this.settings = null;
        }

        this._primary_calendar_system = null;
        this._secondary_calendar_system = null;
        this.desktop_settings = null;
        this.clock = null;
        this.menu = null;
        this.menuManager = null;
        this._popupBody = null;
        this._calendarColumn = null;
        this._resume_source = null;
        this.go_home_button = null;
        this._day = null;
        this._date = null;
        this._secondary_date = null;
        this._today_box = null;
    }
}

function main(metadata, orientation, panel_height, instance_id) {
    return new CalendarPlusApplet(
        orientation,
        panel_height,
        instance_id,
        metadata.version
    );
}
