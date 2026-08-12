// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * CalendarServer transport and native event-store controller.
 *
 * This module owns no Cinnamon actors. CalendarServer tuples cross D-Bus once
 * and are immediately normalised by CalendarPlus.EventStore; an optional
 * agenda view receives detached snapshot records. Separating transport from
 * presentation makes event fetching testable without building the popup UI.
 */

const CalendarPlus = imports.gi.CalendarPlus;
const Cinnamon = imports.gi.Cinnamon;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Signals = imports.signals;
const Mainloop = imports.mainloop;

const APPLET_UUID = "calendar-plus@the-infiltratr";
const STATUS_UNKNOWN = 0;
const STATUS_NO_CALENDARS = 1;
const EDS_BUS_NAME = "org.gnome.evolution.dataserver.Calendar8";

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

function _jsDateToLocalDateTime(date) {
    return GLib.DateTime.new_from_unix_local(Math.floor(date.getTime() / 1000));
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

class EventRecord {
    constructor(row) {
        const [
            id,
            color,
            summary,
            allDay,
            multiDay,
            startUnix,
            endUnix,
            startDayUnix,
            endDayUnix,
            modified,
        ] = row;

        this.id = id;
        this.color = color;
        this.summary = summary;
        this.all_day = allDay;
        this.multi_day = multiDay;
        this.start_unix = startUnix;
        this.end_unix = endUnix;
        this.start_day_unix = startDayUnix;
        this.end_day_unix = endDayUnix;
        this.modified = modified;

        this.start = GLib.DateTime.new_from_unix_local(startUnix);
        this.end = GLib.DateTime.new_from_unix_local(endUnix);
        this.start_date = GLib.DateTime.new_from_unix_local(startDayUnix);
        this.end_date = GLib.DateTime.new_from_unix_local(endDayUnix);
    }

    relation_to_day(day) {
        return CalendarPlus.event_day_relation(
            this.start_day_unix,
            this.end_day_unix,
            day.to_unix()
        );
    }

    timing(now) {
        return CalendarPlus.event_timing(
            this.start_unix,
            this.end_unix,
            now.to_unix()
        ).deep_unpack();
    }
}

class EventSnapshot {
    constructor(variant) {
        const [revision, rows] = variant.deep_unpack();
        this.timestamp = revision;
        this.events = rows.map((row) => new EventRecord(row));
    }

    get_event_list() {
        return this.events;
    }
}

var EventsManager = class EventsManager {
    constructor(settings, desktop_settings, event_list) {
        this.settings = settings;
        this.desktop_settings = desktop_settings;
        this._destroyed = false;
        this._inited = false;
        this._bus_watch_id = 0;
        this._calendar_server = null;
        this._serverSignals = new SignalBag();
        this._cancellable = new Gio.Cancellable();
        this._cached_state = STATUS_UNKNOWN;
        this._gc_timer_id = 0;
        this._reload_today_id = 0;
        this._force_reload_pending = false;
        this._event_list = event_list || null;
        this.current_range_start = null;
        this.current_range_end = null;
        this.current_selected_date = null;
        this.last_update_timestamp = 0;
        this.event_store = CalendarPlus.EventStore.new();

        this._timezone_monitor = null;
        this._timezone_monitor_signal_id = 0;
        this._startTimezoneMonitor();
    }

    start_events() {
        if (this._destroyed || this._inited || this._bus_watch_id > 0) {
            return;
        }

        this._bus_watch_id = Gio.bus_watch_name(
            Gio.BusType.SESSION,
            EDS_BUS_NAME,
            Gio.BusNameWatcherFlags.NONE,
            () => this._connectCalendarServer(),
            null
        );
    }

    _connectCalendarServer() {
        if (this._bus_watch_id > 0) {
            Gio.bus_unwatch_name(this._bus_watch_id);
            this._bus_watch_id = 0;
        }
        if (this._destroyed || this._calendar_server !== null) {
            return;
        }

        Cinnamon.CalendarServerProxy.new_for_bus(
            Gio.BusType.SESSION,
            Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION,
            "org.cinnamon.CalendarServer",
            "/org/cinnamon/CalendarServer",
            this._cancellable,
            (object, result) => this._calendarServerReady(result)
        );
    }

    _calendarServerReady(result) {
        try {
            const server = Cinnamon.CalendarServerProxy.new_for_bus_finish(result);
            if (this._destroyed) {
                return;
            }

            this._calendar_server = server;
            this._serverSignals.connect(
                server,
                "events-added-or-updated",
                (proxy, payload) => this._ingestEvents(payload)
            );
            this._serverSignals.connect(
                server,
                "events-removed",
                (proxy, ids) => this._removeEvents(ids)
            );
            this._serverSignals.connect(
                server,
                "client-disappeared",
                () => this._calendarSetChanged()
            );
            this._serverSignals.connect(
                server,
                "notify::status",
                () => this._statusChanged()
            );

            this._cached_state = server.status;
            this._inited = true;
            this.emit("events-manager-ready");
        } catch (error) {
            if (!this._destroyed) {
                global.logError(
                    `${APPLET_UUID}: could not connect to calendar server: ${error}`
                );
            }
        }
    }

    _startTimezoneMonitor() {
        try {
            const timezone = Gio.File.new_for_path("/etc/localtime");
            this._timezone_monitor = timezone.monitor_file(
                Gio.FileMonitorFlags.NONE,
                this._cancellable
            );
            this._timezone_monitor_signal_id = this._timezone_monitor.connect(
                "changed",
                () => this._timezoneChanged()
            );
        } catch (error) {
            global.logError(
                `${APPLET_UUID}: could not monitor system timezone: ${error}`
            );
        }
    }

    _timezoneChanged() {
        if (this._destroyed || this.event_store === null) {
            return;
        }

        this.event_store.refresh_timezone();
        this._refreshSelectedAgenda(false);
        this.queue_reload_today(true);
        this.emit("events-updated");
    }

    _ingestEvents(payload) {
        if (this._destroyed || this.event_store === null) {
            return;
        }

        let changed = false;
        for (const eventVariant of payload.unpack()) {
            changed = this.event_store.add_or_update(
                eventVariant,
                this.last_update_timestamp
            ) || changed;
        }

        if (changed) {
            this._refreshSelectedAgenda(false);
        }
        this._scheduleCull();
        this.emit("events-updated");
    }

    _removeEvents(serializedIds) {
        if (this._destroyed || this.event_store === null) {
            return;
        }
        if (this.event_store.remove(serializedIds)) {
            this._refreshSelectedAgenda(false);
        }
        this.queue_reload_today(false);
        this.emit("events-updated");
    }

    _calendarSetChanged() {
        if (this._destroyed || this.event_store === null) {
            return;
        }

        /*
         * Client disappearance changes the membership of the event universe.
         * A clean reload is safer than trying to infer which cached rows came
         * from the removed EDS client.
         */
        this.event_store.clear();
        this.current_range_start = null;
        this.current_range_end = null;
        this.queue_reload_today(true);
        this.emit("events-updated");
    }

    _statusChanged() {
        if (this._destroyed || this._calendar_server === null) {
            return;
        }
        const next = this._calendar_server.status;
        if (next === this._cached_state || next === STATUS_UNKNOWN) {
            return;
        }

        this._cached_state = next;
        this.queue_reload_today(true);
        this.emit("has-calendars-changed");
    }

    _scheduleCull() {
        this._cancelCull();
        if (this._destroyed || !this.is_active()) {
            return;
        }

        /*
         * CalendarServer delivers a refresh as a burst.  Waiting three seconds
         * lets all add/update signals arrive before C removes records not seen
         * in the current refresh timestamp.
         */
        this._gc_timer_id = Mainloop.timeout_add_seconds(3, () => {
            this._gc_timer_id = 0;
            if (this._destroyed || this.event_store === null) {
                return GLib.SOURCE_REMOVE;
            }
            if (this.event_store.cull(this.last_update_timestamp)) {
                this._refreshSelectedAgenda(false);
                this.emit("events-updated");
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    _cancelCull() {
        if (this._gc_timer_id > 0) {
            Mainloop.source_remove(this._gc_timer_id);
            this._gc_timer_id = 0;
        }
    }

    set_visible_range(firstDate, lastDate, force) {
        if (this._destroyed || this._calendar_server === null ||
            this.event_store === null) {
            return;
        }

        const first = _midnight(_jsDateToLocalDateTime(firstDate));
        const last = _midnight(_jsDateToLocalDateTime(lastDate));
        if (last.to_unix() < first.to_unix()) {
            global.logError(`${APPLET_UUID}: invalid visible event range.`);
            return;
        }

        const changed = !_sameInstant(first, this.current_range_start) ||
            !_sameInstant(last, this.current_range_end);
        if (!changed && !force) {
            return;
        }

        /*
         * The calendar model is authoritative for what is visible.  Querying
         * exactly its first and last Gregorian cells keeps event dots correct
         * for calendars whose natural period does not share Gregorian month
         * boundaries.  CalendarServer expects an inclusive Unix interval.
         */
        this.current_range_start = first;
        this.current_range_end = last;
        if (changed) {
            this.event_store.clear();
        }

        const exclusiveEnd = last.add_days(1);
        this.last_update_timestamp = GLib.get_monotonic_time();
        this._calendar_server.call_set_time_range(
            first.to_unix(),
            exclusiveEnd.to_unix() - 1,
            Boolean(force),
            null,
            (server, result) => {
                try {
                    server.call_set_time_range_finish(result);
                } catch (error) {
                    if (!this._destroyed) {
                        global.logError(`${APPLET_UUID}: event range request failed: ${error}`);
                    }
                }
            }
        );
    }

    queue_reload_today(force) {
        if (this._reload_today_id > 0) {
            Mainloop.source_remove(this._reload_today_id);
            this._reload_today_id = 0;
        }
        if (this._destroyed) {
            return;
        }
        this._force_reload_pending = this._force_reload_pending || Boolean(force);
        this._reload_today_id = Mainloop.idle_add(() => {
            this._reload_today_id = 0;
            if (!this._destroyed) {
                this.select_date(new Date(), this._force_reload_pending);
            }
            this._force_reload_pending = false;
            return GLib.SOURCE_REMOVE;
        });
    }

    select_date(date, force) {
        if (this._destroyed || !this.is_active()) {
            return;
        }

        const day = _midnight(_jsDateToLocalDateTime(date));
        const previous = this.current_selected_date;
        const changedMonth = previous !== null &&
            (previous.get_year() !== day.get_year() ||
             previous.get_month() !== day.get_month());

        /* Selection changes agenda focus; the calendar view owns fetch range. */
        this.current_selected_date = day;

        if (this._event_list !== null) {
            this._event_list.set_date(day);
            this._event_list.set_events(
                this._snapshot_for_date(day),
                previous === null || changedMonth || Boolean(force)
            );
        }
    }

    _refreshSelectedAgenda(delayEmpty) {
        if (this._event_list === null || this.current_selected_date === null) {
            return;
        }
        this._event_list.set_events(
            this._snapshot_for_date(this.current_selected_date),
            Boolean(delayEmpty)
        );
    }

    _snapshot_for_date(day) {
        if (this._destroyed || this.event_store === null || day === null) {
            return null;
        }
        const snapshot = new EventSnapshot(
            this.event_store.get_snapshot(
                day.to_unix(),
                GLib.DateTime.new_now_local().to_unix()
            )
        );
        return snapshot.events.length === 0 ? null : snapshot;
    }

    get_colors_for_date(js_date) {
        if (this._destroyed || this.event_store === null) {
            return [];
        }
        const day = _midnight(_jsDateToLocalDateTime(js_date));
        return this.event_store.get_colors(
            day.to_unix(),
            GLib.DateTime.new_now_local().to_unix()
        );
    }

    is_active() {
        return !this._destroyed &&
            this._inited &&
            this.settings.getValue("show-events") &&
            this._calendar_server !== null &&
            this._calendar_server.status !== STATUS_NO_CALENDARS;
    }

    destroy() {
        if (this._destroyed) {
            return;
        }
        this._destroyed = true;
        this._inited = false;

        if (this._bus_watch_id > 0) {
            Gio.bus_unwatch_name(this._bus_watch_id);
            this._bus_watch_id = 0;
        }
        if (this._cancellable !== null) {
            try {
                this._cancellable.cancel();
            } catch (error) {
                global.logError(error);
            }
        }

        this._cancelCull();
        if (this._reload_today_id > 0) {
            Mainloop.source_remove(this._reload_today_id);
            this._reload_today_id = 0;
        }

        if (this._timezone_monitor !== null) {
            if (this._timezone_monitor_signal_id > 0) {
                try {
                    this._timezone_monitor.disconnect(this._timezone_monitor_signal_id);
                } catch (error) {
                    global.logError(error);
                }
            }
            try {
                this._timezone_monitor.cancel();
            } catch (error) {
                global.logError(error);
            }
        }
        this._timezone_monitor_signal_id = 0;
        this._timezone_monitor = null;

        this._serverSignals.disconnectAll();
        this._calendar_server = null;

        this._event_list = null;
        if (this.event_store !== null) {
            this.event_store.clear();
            this.event_store = null;
        }

        this._cancellable = null;
        this.settings = null;
        this.desktop_settings = null;
        this.current_selected_date = null;
        this.current_range_start = null;
        this.current_range_end = null;
    }
};
Signals.addSignalMethods(EventsManager.prototype);
