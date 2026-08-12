// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Calendar Plus month/period view.
 *
 * The visible calendar is always a 6 x 7 actor grid, but the meaning of a
 * "month" is delegated to CalendarPlus.CalendarSystem.  ISO week and Mayan
 * views can therefore browse their natural periods without JavaScript
 * pretending that every calendar has Gregorian month boundaries.
 *
 * Date invariant: JavaScript Date objects are used only as local civil-date
 * carriers at the Cinnamon boundary.  Calendar arithmetic, period movement,
 * ISO week numbering, work-day classification and all 42 cell flags are
 * produced by the native library.
 */

const CalendarPlus = imports.gi.CalendarPlus;
const Cinnamon = imports.gi.Cinnamon;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Signals = imports.signals;
const Mainloop = imports.mainloop;
const Gettext = imports.gettext;
const GtkGettext = imports.gettext.domain("gtk30");

const CALENDAR_PLUS_GETTEXT_DOMAIN = "calendar-plus@the-infiltratr";
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

const DESKTOP_SCHEMA = "org.cinnamon.desktop.interface";
const FIRST_WEEKDAY_KEY = "first-day-of-week";
const WEEK_NUMBER_WIDTH_DIGITS = 3;
const DATE_PARTS = Object.freeze({
    day: CalendarPlus.DatePart.DAY,
    month: CalendarPlus.DatePart.MONTH,
    year: CalendarPlus.DatePart.YEAR,
    short: CalendarPlus.DatePart.SHORT,
    full: CalendarPlus.DatePart.FULL,
});

function _dateFields(date) {
    return [date.getFullYear(), date.getMonth() + 1, date.getDate()];
}

function _sameCivilDate(a, b) {
    return CalendarPlus.date_same(..._dateFields(a), ..._dateFields(b));
}

/*
 * Navigation comes back from C as Gregorian fields.  Noon is deliberately
 * used as the reconstruction time because local midnight can be skipped or
 * repeated by civil-time transitions in some timezones.
 */
function _localDateFromVariant(parts) {
    if (parts === null) {
        return null;
    }
    const [year, month, day] = parts.deep_unpack();
    const date = new Date();
    date.setHours(12, 0, 0, 0);
    date.setFullYear(year, month - 1, day);
    return date;
}

function _localDate(year, month, day) {
    const value = new Date();
    value.setHours(12, 0, 0, 0);
    value.setFullYear(year, month - 1, day);
    return value;
}

function _weekdayAbbreviation(dayIndex) {
    /* 7 January 2024 was a Sunday; using a fixed week avoids locale ordering. */
    const sample = new Date(2024, 0, 7 + dayIndex, 12, 0, 0, 0);
    return sample.toLocaleFormat("%a");
}

var Calendar = class Calendar {
    constructor(settings, events_manager) {
        this.settings = settings;
        this.events_manager = events_manager;
        this._destroyed = false;
        this._update_id = 0;
        this._set_date_idle_id = 0;
        this._weekStart = Cinnamon.util_get_week_start();
        this._digitWidth = NaN;
        this._calendarSystemId = "gregorian";
        this._calendarSystem = CalendarPlus.CalendarSystem.new("gregorian");
        this._selectedDate = new Date();
        this.events_enabled = false;

        this._actorSignals = new SignalBag();
        this._eventSignals = new SignalBag();
        this._desktopSignals = new SignalBag();

        this.actor = new St.Table({
            homogeneous: false,
            style_class: "calendar",
            reactive: true,
        });

        this.settings.bindWithObject(
            this,
            "show-week-numbers",
            "show_week_numbers",
            this._onSettingsChange
        );

        this.desktop_settings = new Gio.Settings({ schema_id: DESKTOP_SCHEMA });
        this._desktopSignals.connect(
            this.desktop_settings,
            `changed::${FIRST_WEEKDAY_KEY}`,
            () => this._onSettingsChange(null, FIRST_WEEKDAY_KEY)
        );

        for (const signal of [
            "events-updated",
            "events-manager-ready",
            "has-calendars-changed",
        ]) {
            this._eventSignals.connect(this.events_manager, signal, () => {
                if (signal === "events-updated") {
                    this._queue_update();
                } else {
                    this._refreshEventAvailability();
                }
            });
        }

        this._actorSignals.connect(this.actor, "scroll-event", (actor, event) => {
            this._onScroll(event);
        });
        this._actorSignals.connect(this.actor, "style-changed", () => {
            this._measureWeekNumberColumn();
        });

        /* GTK's translation token tells us whether the locale writes M/Y or Y/M. */
        const headingOrder = GtkGettext.gettext("calendar:MY");
        this._monthBeforeYear = headingOrder !== "calendar:YM";
        if (headingOrder !== "calendar:MY" && headingOrder !== "calendar:YM") {
            global.logError(
                "Calendar Plus: GTK calendar heading-order translation is invalid."
            );
        }

        this._buildHeader();
    }

    setCalendarSystem(calendarId) {
        if (calendarId === this._calendarSystemId) {
            return;
        }
        const replacement = CalendarPlus.CalendarSystem.new(calendarId);
        if (replacement === null) {
            global.logError(`Calendar Plus: unknown calendar '${calendarId}'.`);
            return;
        }
        this._calendarSystem = replacement;
        this._calendarSystemId = calendarId;
        this._update(true);
    }

    formatDate(date, part) {
        const typedPart = DATE_PARTS[part];
        if (typedPart === undefined) {
            global.logError(`Calendar Plus: invalid date part '${part}'.`);
            return "";
        }
        return this._calendarSystem.format_date_part(
            ..._dateFields(date),
            typedPart
        );
    }

    getCalendarName() {
        return this._calendarSystem.get_name();
    }

    setDate(date, forceReload) {
        if (this._destroyed) {
            return;
        }
        const changed = !_sameCivilDate(date, this._selectedDate);
        if (changed) {
            this._selectedDate = new Date(date.getTime());
            this.emit("selected-date-changed", this._selectedDate);
        }
        if (changed || forceReload) {
            this._update(Boolean(forceReload));
        }
    }

    getSelectedDate() {
        return this._selectedDate;
    }

    todaySelected() {
        return _sameCivilDate(this._selectedDate, new Date());
    }

    queue_set_date(date) {
        if (this._destroyed || this._set_date_idle_id > 0) {
            return;
        }

        /*
         * A short delay coalesces rapid wheel events and navigation clicks,
         * keeping expensive actor rebuilds out of a burst of input events.
         */
        this._set_date_idle_id = Mainloop.timeout_add(25, () => {
            this._set_date_idle_id = 0;
            if (!this._destroyed) {
                this.setDate(date, false);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    _onSettingsChange(object, key) {
        if (this._destroyed) {
            return;
        }
        if (key === FIRST_WEEKDAY_KEY) {
            this._weekStart = Cinnamon.util_get_week_start();
        }
        this._buildHeader();
        this._update(false);
    }

    _refreshEventAvailability() {
        if (this._destroyed) {
            return;
        }
        this.events_enabled = this.events_manager.is_active();
        this._queue_update();
    }

    _queue_update() {
        if (this._destroyed) {
            return;
        }
        this._cancel_update();
        this._update_id = Mainloop.idle_add(() => {
            this._update_id = 0;
            if (!this._destroyed) {
                this._update(false);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    _cancel_update() {
        if (this._update_id > 0) {
            Mainloop.source_remove(this._update_id);
            this._update_id = 0;
        }
    }

    _cancel_set_date_idle() {
        if (this._set_date_idle_id > 0) {
            Mainloop.source_remove(this._set_date_idle_id);
            this._set_date_idle_id = 0;
        }
    }

    _buildHeader() {
        if (this._destroyed) {
            return;
        }

        const weekOffset = this.show_week_numbers ? 1 : 0;
        this.actor.destroy_all_children();

        const monthBox = new St.BoxLayout();
        const yearBox = new St.BoxLayout();
        if (this._monthBeforeYear) {
            this.actor.add(monthBox, { row: 0, col: 0, col_span: weekOffset + 4 });
            this.actor.add(yearBox, { row: 0, col: weekOffset + 4, col_span: 3 });
        } else {
            this.actor.add(yearBox, { row: 0, col: 0, col_span: weekOffset + 3 });
            this.actor.add(monthBox, { row: 0, col: weekOffset + 3, col_span: 4 });
        }

        this._monthLabel = this._buildNavigator(
            monthBox,
            CP_("Previous month"),
            CP_("Next month"),
            () => this._browse(0, -1),
            () => this._browse(0, 1)
        );
        this._yearLabel = this._buildNavigator(
            yearBox,
            CP_("Previous year"),
            CP_("Next year"),
            () => this._browse(-1, 0),
            () => this._browse(1, 0)
        );

        if (this.show_week_numbers) {
            this._weekNumberHeader = new St.Label({
                text: "",
                style_class: "calendar-day-base calendar-week-number",
            });
            this.actor.add(this._weekNumberHeader, {
                row: 1,
                col: 0,
                x_fill: false,
                x_align: St.Align.MIDDLE,
            });
        } else {
            this._weekNumberHeader = null;
        }

        for (let logical = 0; logical < 7; logical++) {
            const weekday = (this._weekStart + logical) % 7;
            /* Native work-day policy avoids a second JS definition of weekends. */
            const sample = _localDate(2024, 1, 7 + weekday);
            const workday = CalendarPlus.date_is_work_day(..._dateFields(sample));
            const label = new St.Label({
                text: _weekdayAbbreviation(weekday),
                style_class:
                    "calendar-day-base calendar-day-heading " +
                    (workday ? "calendar-work-day" : "calendar-nonwork-day"),
            });
            this.actor.add(label, {
                row: 1,
                col: weekOffset + logical,
                x_fill: false,
                x_align: St.Align.MIDDLE,
            });
        }

        /* Cells are the only actors removed during a normal view refresh. */
        this._firstCellIndex = this.actor.get_n_children();
        this._measureWeekNumberColumn();
    }

    _buildNavigator(box, backName, forwardName, onBack, onForward) {
        const back = new St.Button({
            style_class: "calendar-change-month-back",
            can_focus: true,
            accessible_name: backName,
        });
        const label = new St.Label({ style_class: "calendar-month-label" });
        const forward = new St.Button({
            style_class: "calendar-change-month-forward",
            can_focus: true,
            accessible_name: forwardName,
        });

        back.connect("clicked", onBack);
        forward.connect("clicked", onForward);
        box.add(back);
        box.add(label, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });
        box.add(forward);
        return label;
    }

    _measureWeekNumberColumn() {
        if (!this.actor || !this.show_week_numbers || !this._weekNumberHeader) {
            return;
        }
        const context = this.actor.get_pango_context();
        const node = this.actor.get_theme_node();
        const metrics = context.get_metrics(node.get_font(), context.get_language());
        this._digitWidth = metrics.get_approximate_digit_width() / Pango.SCALE;
        if (Number.isFinite(this._digitWidth)) {
            this._weekNumberHeader.set_width(
                this._digitWidth * WEEK_NUMBER_WIDTH_DIGITS
            );
        }
    }

    _onScroll(event) {
        const direction = event.get_scroll_direction();
        if (direction === Clutter.ScrollDirection.UP ||
            direction === Clutter.ScrollDirection.LEFT) {
            this._browse(0, -1);
        } else if (direction === Clutter.ScrollDirection.DOWN ||
                   direction === Clutter.ScrollDirection.RIGHT) {
            this._browse(0, 1);
        }
    }

    _browse(yearDelta, periodDelta) {
        const args = _dateFields(this._selectedDate);
        const variant = yearDelta !== 0
            ? this._calendarSystem.add_years_parts(...args, yearDelta)
            : this._calendarSystem.add_months_parts(...args, periodDelta);
        const destination = _localDateFromVariant(variant);
        if (destination !== null) {
            this.queue_set_date(destination);
        }
    }

    _removeCells() {
        const children = this.actor.get_children();
        for (let index = children.length - 1; index >= this._firstCellIndex; index--) {
            this.actor.remove_actor(children[index]);
        }
    }

    _update(forceReload) {
        if (this._destroyed || !this._calendarSystem) {
            return;
        }

        this._monthLabel.text = this.formatDate(this._selectedDate, "month").capitalize();
        this._yearLabel.text = this.formatDate(this._selectedDate, "year");
        this._removeCells();

        const model = this._calendarSystem.build_grid(
            ..._dateFields(this._selectedDate),
            ..._dateFields(new Date()),
            this._weekStart
        );
        if (model === null) {
            global.logError("Calendar Plus: native grid generation failed.");
            return;
        }

        const records = model.deep_unpack();
        for (const record of records) {
            this._addGridCell(record);
        }

        /*
         * The native grid supplies Gregorian coordinates for every displayed
         * cell even when the primary calendar uses different period boundaries.
         * Those exact endpoints define the event-cache query; selection only
         * controls which day's agenda is shown.
         */
        if (records.length > 0) {
            const first = records[0];
            const last = records[records.length - 1];
            this.events_manager.set_visible_range(
                _localDate(first[1], first[2], first[3]),
                _localDate(last[1], last[2], last[3]),
                forceReload
            );
        }
        this.events_manager.select_date(this._selectedDate, forceReload);
    }

    _addGridCell(record) {
        const [
            dayLabel,
            year,
            month,
            day,
            row,
            column,
            weekNumber,
            isWorkDay,
            isToday,
            isSelected,
            isCurrentPeriod,
            isTopRow,
            isLeftEdge,
        ] = record;

        const date = _localDate(year, month, day);
        const group = new Cinnamon.Stack();
        const button = new St.Button({
            label: dayLabel,
            can_focus: true,
            accessible_name: `${this.getCalendarName()}: ${this.formatDate(date, "full")}`,
        });
        button.connect("clicked", () => {
            if (this.events_enabled) {
                this.setDate(date, false);
            }
        });
        if (isSelected) {
            button.add_style_pseudo_class("selected");
        }
        button.style_class = this._dayStyle(
            isWorkDay,
            isToday,
            isCurrentPeriod,
            isTopRow,
            isLeftEdge
        );
        group.add_actor(button);

        const dotBox = new Cinnamon.GenericContainer({
            style_class: "calendar-day-event-dot-box",
        });
        dotBox.connect("allocate", (actor, box, flags) => {
            this._allocateDots(actor, box, flags);
        });
        group.add_actor(dotBox);

        const offset = this.show_week_numbers ? 1 : 0;
        this.actor.add(group, { row, col: offset + column });

        if (this.show_week_numbers && weekNumber > 0) {
            this.actor.add(new St.Label({
                text: String(weekNumber),
                style_class: "calendar-day-base calendar-week-number",
                accessible_name: `${CP_("Week")} ${weekNumber}`,
            }), {
                row,
                col: 0,
                y_align: St.Align.MIDDLE,
            });
        }

        if (!this.events_enabled) {
            return;
        }
        for (const color of this.events_manager.get_colors_for_date(date)) {
            dotBox.add_actor(new St.Bin({
                style_class: "calendar-day-event-dot",
                style: `background-color: ${color};`,
                x_align: Clutter.ActorAlign.CENTER,
            }));
        }
    }

    _dayStyle(workday, today, currentPeriod, topRow, leftEdge) {
        const names = [
            "calendar-day-base",
            "calendar-day",
            workday ? "calendar-work-day" : "calendar-nonwork-day",
        ];
        if (topRow) {
            names.push("calendar-day-top");
        }
        if (leftEdge) {
            names.push("calendar-day-left");
        }
        if (today) {
            names.push("calendar-today");
        } else if (currentPeriod) {
            names.push("calendar-not-today");
        } else {
            names.push("calendar-other-month-day");
        }
        return names.join(" ");
    }

    _allocateDots(actor, box, flags) {
        const dots = actor.get_children();
        if (dots.length === 0) {
            return;
        }

        const [, dotWidth] = dots[0].get_preferred_width(-1);
        const [, dotHeight] = dots[0].get_preferred_height(-1);
        const width = Math.max(0, box.x2 - box.x1);
        if (dotWidth <= 0 || dotHeight <= 0 || width <= 0) {
            return;
        }

        const [hasThemeLimit, themeLimit] = actor.get_theme_node().lookup_double(
            "max-rows",
            false
        );
        const rowLimit = hasThemeLimit ? Math.max(1, Math.trunc(themeLimit)) : 2;

        /*
         * Balance the dots into the smallest useful number of rows, then give
         * each row equal-width cells.  The last row is centred independently.
         * Row balance is the invariant; the final row is centred separately.
         */
        const naturalColumns = Math.max(1, Math.floor(width / dotWidth));
        const rowsNeeded = Math.ceil(dots.length / naturalColumns);
        const rows = Math.min(rowLimit, Math.max(1, rowsNeeded));
        const columns = Math.ceil(dots.length / rows);
        let index = 0;

        for (let row = 0; row < rows && index < dots.length; row++) {
            const remaining = dots.length - index;
            const inThisRow = Math.min(columns, remaining);
            const cellWidth = width / columns;
            const rowWidth = inThisRow * cellWidth;
            const rowStart = (width - rowWidth) / 2;

            for (let column = 0; column < inThisRow; column++, index++) {
                const x = rowStart + column * cellWidth +
                    (cellWidth - dotWidth) / 2;
                const allocation = new Clutter.ActorBox();
                allocation.x1 = Math.floor(x);
                allocation.x2 = allocation.x1 + dotWidth;
                allocation.y1 = row * dotHeight;
                allocation.y2 = allocation.y1 + dotHeight;
                dots[index].allocate(allocation, flags);
            }
        }
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._cancel_update();
        this._cancel_set_date_idle();
        this._desktopSignals.disconnectAll();
        this._eventSignals.disconnectAll();
        this._actorSignals.disconnectAll();

        if (this.actor) {
            this.actor.destroy();
            this.actor = null;
        }
        this._calendarSystem = null;
        this.desktop_settings = null;
        this.events_manager = null;
        this.settings = null;
        this._selectedDate = null;
    }
};

Signals.addSignalMethods(Calendar.prototype);
