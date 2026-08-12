// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Agenda presentation.
 *
 * This module owns the selected-day Cinnamon actors and presentation-only
 * formatting. Event acquisition, cache lifetime and native snapshot creation
 * live in eventManager.js, keeping D-Bus state out of the view.
 */

const Atk = imports.gi.Atk;
const CalendarPlus = imports.gi.CalendarPlus;
const CinnamonDesktop = imports.gi.CinnamonDesktop;
const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Signals = imports.signals;
const Mainloop = imports.mainloop;
const Separator = imports.ui.separator;
const Util = imports.misc.util;
const Gettext = imports.gettext;

const APPLET_UUID = "calendar-plus@the-infiltratr";
const CALENDAR_PLUS_GETTEXT_DOMAIN = APPLET_UUID;
Gettext.bindtextdomain(CALENDAR_PLUS_GETTEXT_DOMAIN, "/usr/share/locale");
const CalendarPlusGettext = Gettext.domain(CALENDAR_PLUS_GETTEXT_DOMAIN);

function CP_(text) {
    return CalendarPlusGettext.gettext(text);
}

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
const ARROW_SEPARATOR = "  ►  ";
const DATE_FORMAT_FULL = CinnamonDesktop.WallClock.lctime_format(
    "cinnamon", _("%A, %B %-e, %Y")
);
const DAY_FORMAT = CinnamonDesktop.WallClock.lctime_format("cinnamon", "%A");

function _capitaliseLocale(text) {
    return text.length === 0
        ? text
        : text.charAt(0).toLocaleUpperCase() + text.slice(1);
}

function _midnight(dateTime) {
    return GLib.DateTime.new_local(
        dateTime.get_year(),
        dateTime.get_month(),
        dateTime.get_day_of_month(),
        0, 0, 0
    );
}

function _sameInstant(a, b) {
    return a !== null && b !== null && a.to_unix() === b.to_unix();
}

function _relationHas(mask, flag) {
    return (mask & flag) !== 0;
}

function _separatorActor() {
    const separator = new Separator.Separator();
    return separator.actor || separator;
}

/* Countdown text is deliberately approximate; the exact row time remains visible. */
function _countdown(secondsUntilStart) {
    const roundedMinutes = Math.max(1, Math.ceil(secondsUntilStart / 60));
    if (roundedMinutes <= 5) {
        return ["imminent", _("Starting in a few minutes")];
    }
    if (roundedMinutes < 60) {
        return ["soon", _("Starting in %d minutes").format(roundedMinutes)];
    }

    const roundedHours = Math.ceil(roundedMinutes / 60);
    if (roundedHours <= 6) {
        return [
            "",
            ngettext("In %d hour", "In %d hours", roundedHours)
                .format(roundedHours),
        ];
    }
    return ["", ""];
}

var EventList = class EventList {
    constructor(settings, desktop_settings) {
        this.settings = settings;
        this.desktop_settings = desktop_settings;
        this._destroyed = false;
        this.selected_date = GLib.DateTime.new_now_local();
        this._no_events_timeout_id = 0;
        this._scroll_to_idle_id = 0;
        this._rows = [];
        this._current_event_data_list_timestamp = 0;
        this._signals = new SignalBag();

        this.actor = new St.BoxLayout({
            style_class: "calendar-events-main-box",
            vertical: true,
            visible: false,
        });

        this.selected_date_label = new St.Button({
            style_class: "calendar-events-date-label",
            reactive: true,
            can_focus: true,
            accessible_name: CP_("Open selected date in Calendar"),
        });
        this._signals.connect(this.selected_date_label, "clicked", () => {
            this.launch_calendar(this.selected_date);
        });
        this.actor.add_actor(this.selected_date_label);

        this._buildEmptyState();
        this._buildEventScroller();
    }

    _buildEmptyState() {
        this.no_events_box = new St.BoxLayout({
            style_class: "calendar-events-no-events-box",
            vertical: true,
            visible: false,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            y_expand: true,
        });

        const canLaunch = GLib.find_program_in_path("gnome-calendar") !== null;
        this.no_events_button = new St.Button({
            style_class: "calendar-events-no-events-button",
            reactive: canLaunch,
            can_focus: canLaunch,
            accessible_name: CP_("Open Calendar"),
        });
        this._signals.connect(this.no_events_button, "clicked", () => {
            this.launch_calendar(this.selected_date);
        });

        const content = new St.BoxLayout({ vertical: true });
        content.add_actor(new St.Icon({
            style_class: "calendar-events-no-events-icon",
            icon_name: "xsi-x-office-calendar",
            icon_type: St.IconType.SYMBOLIC,
            icon_size: 48,
        }));
        content.add_actor(new St.Label({
            style_class: "calendar-events-no-events-label",
            text: _("No Events"),
            y_align: Clutter.ActorAlign.CENTER,
        }));

        this.no_events_button.add_actor(content);
        this.no_events_box.add_actor(this.no_events_button);
        this.actor.add_actor(this.no_events_box);
    }

    _buildEventScroller() {
        this.events_box = new St.BoxLayout({
            style_class: "calendar-events-event-container",
            vertical: true,
            accessible_role: Atk.Role.LIST,
        });
        this.events_scroll_box = new St.ScrollView({
            style_class: "calendar-events-scrollbox vfade",
            hscrollbar_policy: St.PolicyType.NEVER,
            vscrollbar_policy: St.PolicyType.AUTOMATIC,
            enable_auto_scrolling: true,
        });

        const bar = this.events_scroll_box.get_vscroll_bar();
        this._signals.connect(bar, "scroll-start", () => {
            this.emit("start-pass-events");
        });
        this._signals.connect(bar, "scroll-stop", () => {
            this.emit("stop-pass-events");
        });

        this.events_scroll_box.add_actor(this.events_box);
        this.actor.add_actor(this.events_scroll_box);
    }

    launch_calendar(gdate) {
        if (this._destroyed || gdate === null) {
            return;
        }
        Util.trySpawn(["gnome-calendar", "--date", gdate.format("%x")], false);
        this.emit("launched-calendar");
    }

    set_date(gdate) {
        if (this._destroyed) {
            return;
        }
        this.selected_date = gdate;
        const text = _capitaliseLocale(gdate.format(DATE_FORMAT_FULL));
        this.selected_date_label.set_label(text);
        this.selected_date_label.set_accessible_name(
            `${CP_("Open selected date in Calendar")}: ${text}`
        );
    }

    set_events(snapshot, delayEmpty) {
        if (this._destroyed) {
            return;
        }
        this._cancelScroll();

        if (snapshot !== null &&
            snapshot.timestamp === this._current_event_data_list_timestamp) {
            for (const row of this._rows) {
                row.update_variations();
            }
            return;
        }

        this._clearRows();
        this._cancelEmptyDelay();

        if (snapshot === null) {
            this._current_event_data_list_timestamp = 0;
            this._showEmptyState(Boolean(delayEmpty));
            return;
        }

        this.no_events_box.hide();
        this._current_event_data_list_timestamp = snapshot.timestamp;

        let scrollTarget = null;
        const use24h = this.desktop_settings.get_boolean("clock-use-24h");
        for (const event of snapshot.get_event_list()) {
            if (this._rows.length > 0) {
                this.events_box.add_actor(_separatorActor());
            }

            const row = new EventRow(event, this.selected_date, { use_24h: use24h });
            row.connect("view-event", (actor, uuid) => {
                this.emit("launched-calendar");
                Util.trySpawn(["gnome-calendar", "--uuid", uuid], false);
            });
            this.events_box.add_actor(row.actor);
            this._rows.push(row);
            if (scrollTarget === null && row.is_current_or_next) {
                scrollTarget = row;
            }
        }

        if (scrollTarget !== null) {
            this._queueScrollTo(scrollTarget);
        }
    }

    _showEmptyState(delayed) {
        if (!delayed) {
            this.no_events_box.show();
            return;
        }

        /*
         * A fresh CalendarServer range arrives asynchronously.  Briefly hiding
         * the empty state prevents a distracting "No Events" flash between the
         * date selection and the first server response.
         */
        this._no_events_timeout_id = Mainloop.timeout_add(600, () => {
            this._no_events_timeout_id = 0;
            if (!this._destroyed) {
                this.no_events_box.show();
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    _queueScrollTo(row) {
        this._scroll_to_idle_id = Mainloop.idle_add(() => {
            this._scroll_to_idle_id = 0;
            if (this._destroyed || !row || !row.actor) {
                return GLib.SOURCE_REMOVE;
            }

            const adjustment = this.events_scroll_box
                .get_vscroll_bar()
                .get_adjustment();
            const centre = row.actor.y + row.actor.height / 2 -
                this.events_box.height / 2;
            adjustment.set_value(Math.max(0, centre));
            return GLib.SOURCE_REMOVE;
        });
    }

    _clearRows() {
        for (const actor of this.events_box.get_children()) {
            actor.destroy();
        }
        this._rows = [];
    }

    _cancelEmptyDelay() {
        if (this._no_events_timeout_id > 0) {
            Mainloop.source_remove(this._no_events_timeout_id);
            this._no_events_timeout_id = 0;
        }
    }

    _cancelScroll() {
        if (this._scroll_to_idle_id > 0) {
            Mainloop.source_remove(this._scroll_to_idle_id);
            this._scroll_to_idle_id = 0;
        }
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._cancelEmptyDelay();
        this._cancelScroll();
        this._signals.disconnectAll();
        this._rows = [];
        if (this.actor) {
            this.actor.destroy();
            this.actor = null;
        }
        this.settings = null;
        this.desktop_settings = null;
        this.selected_date = null;
    }
};
Signals.addSignalMethods(EventList.prototype);

class EventRow {
    constructor(event, selectedDate, params) {
        this.event = event;
        this.selected_date = selectedDate;
        this.use_24h = params.use_24h;
        this.is_current_or_next = false;

        const canLaunch = GLib.find_program_in_path("gnome-calendar") !== null;
        this.actor = new St.Button({
            style_class: "calendar-event-button",
            reactive: canLaunch,
            can_focus: canLaunch,
            accessible_role: Atk.Role.LIST_ITEM,
            accessible_name: event.summary,
        });
        if (canLaunch) {
            this.actor.connect("clicked", () => this.emit("view-event", event.id));
        }

        const shell = new St.BoxLayout({ x_expand: true });
        shell.add_actor(new St.Bin({
            style_class: "calendar-event-color-strip",
            style: `background-color: ${event.color};`,
        }));

        /*
         * A two-row table makes the information hierarchy explicit: timing and
         * status occupy the first row, while the summary owns the full second
         * row.  This avoids coupling the row to any inherited box-layout shape.
         */
        const details = new St.Table({
            style_class: "calendar-event-row-content",
            homogeneous: false,
            x_expand: true,
        });
        this.event_time = new St.Label({
            x_align: Clutter.ActorAlign.START,
            text: "",
            style_class: "calendar-event-time-present",
        });
        this.countdown_label = new St.Label({
            x_align: Clutter.ActorAlign.END,
            style_class: "calendar-event-countdown",
        });
        const summary = new St.Label({
            text: event.summary,
            y_expand: true,
            style_class: "calendar-event-summary",
        });
        summary.get_clutter_text().line_wrap = true;
        summary.get_clutter_text().ellipsize = Pango.EllipsizeMode.NEVER;

        details.add(this.event_time, {
            row: 0, col: 0, x_expand: true, x_fill: true,
        });
        details.add(this.countdown_label, {
            row: 0, col: 1, x_expand: true, x_fill: false,
        });
        details.add(summary, {
            row: 1, col: 0, col_span: 2, x_expand: true, x_fill: true,
        });
        shell.add(details, { expand: true, x_fill: true });
        this.actor.set_child(shell);

        this.update_variations();
    }

    update_variations() {
        const now = GLib.DateTime.new_now_local();
        const today = _midnight(now);
        const selected = this.selected_date;
        const [state, secondsUntilStart] = this.event.timing(now);
        const todayRelation = this.event.relation_to_day(today);

        this._resetStateStyles();
        const startsToday = _relationHas(
            todayRelation,
            CalendarPlus.EventDayRelation.STARTS_ON_DAY
        );

        if (state === CalendarPlus.EventState.PAST) {
            this.event_time.set_style_class_name("calendar-event-time-past");
        } else if (state === CalendarPlus.EventState.FUTURE) {
            this.event_time.set_style_class_name("calendar-event-time-future");
            if (startsToday) {
                const [pseudoClass, text] = _countdown(secondsUntilStart);
                this.countdown_label.set_text(text);
                if (pseudoClass) {
                    this.countdown_label.add_style_pseudo_class(pseudoClass);
                }
                this.is_current_or_next = !this.event.all_day;
            }
        } else {
            this.event_time.set_style_class_name("calendar-event-time-present");
            if (this.event.all_day || this.event.multi_day) {
                this.event_time.add_style_pseudo_class("all-day");
            } else {
                this.countdown_label.set_text(_("In progress"));
                this.countdown_label.add_style_pseudo_class("current");
            }
            this.is_current_or_next = startsToday && !this.event.all_day;
        }

        this.event_time.set_text(this._rangeText(today, selected));
    }

    _resetStateStyles() {
        for (const pseudoClass of ["imminent", "soon", "current"]) {
            this.countdown_label.remove_style_pseudo_class(pseudoClass);
        }
        this.event_time.remove_style_pseudo_class("all-day");
        this.countdown_label.set_text("");
        this.is_current_or_next = false;
    }

    _rangeText(today, selected) {
        const selectedRelation = this.event.relation_to_day(selected);
        const timeFormat = this.use_24h ? "%H:%M" : "%-l:%M %p";

        if (!this.event.multi_day && _relationHas(
            selectedRelation,
            CalendarPlus.EventDayRelation.STARTS_ON_DAY
        )) {
            if (this.event.all_day) {
                return _("All day");
            }
            return `${this.event.start.format(timeFormat)}${ARROW_SEPARATOR}` +
                `${this.event.end.format(timeFormat)}`;
        }

        return `${this._formatBoundary(true, today, selected, timeFormat)}` +
            `${ARROW_SEPARATOR}` +
            `${this._formatBoundary(false, today, selected, timeFormat)}`;
    }

    _formatBoundary(isStart, today, selected, timeFormat) {
        const endpoint = isStart ? this.event.start : this.event.end;
        const endpointDay = isStart ? this.event.start_date : this.event.end_date;
        const selectedRelation = this.event.relation_to_day(selected);
        const boundaryFlag = isStart
            ? CalendarPlus.EventDayRelation.STARTS_ON_DAY
            : CalendarPlus.EventDayRelation.ENDS_ON_DAY;

        if (_relationHas(selectedRelation, boundaryFlag)) {
            if (!this.event.all_day) {
                return endpoint.format(timeFormat);
            }
            return _sameInstant(endpointDay, today)
                ? _("Today")
                : _capitaliseLocale(endpointDay.format(DAY_FORMAT));
        }

        if (_sameInstant(endpointDay, today)) {
            return this.event.all_day
                ? _("Today")
                : `${endpoint.format(timeFormat)} ${_("Today")}`;
        }

        /*
         * Use weekday names only for boundaries genuinely close to today.
         * add_days() keeps this comparison correct across DST transitions; a
         * raw seconds-per-day division would not.
         */
        for (let offset = -4; offset <= 4; offset++) {
            if (_sameInstant(endpointDay, today.add_days(offset))) {
                return _capitaliseLocale(endpointDay.format(DAY_FORMAT));
            }
        }
        return endpointDay.format("%x");
    }

}
Signals.addSignalMethods(EventRow.prototype);
