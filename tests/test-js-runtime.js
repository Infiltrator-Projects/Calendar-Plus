#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const root = path.resolve(__dirname, "..");
const metadata = JSON.parse(fs.readFileSync(
    path.join(root, "src", "cinnamon", "metadata.json"),
    "utf8"
));
const version = metadata.version;

function evaluateApplet(nativeVersion = version, loaderMode = "legacy") {
    const loaderObservations = { legacyCalls: 0, extensionCalls: 0 };

    class Actor {
        add() {}
        set_label_actor() {}
        disconnect() {}
    }

    class AppletBase {
        constructor(orientation, panelHeight, instanceId) {
            this.orientation = orientation;
            this.panel_height = panelHeight;
            this.instance_id = instanceId;
            this.actor = new Actor();
        }
        setAllowedLayout() {}
    }

    class Label {
        constructor() {
            this.clutter_text = {};
            this.text = "";
        }
        get_preferred_width() { return [0, this.text.length * 8]; }
        get_text() { return this.text; }
        set_text(value) { this.text = value; }
    }

    class Bin {
        constructor() { this.min_width = 0; }
        set_child() {}
    }

    class SignalBag {
        constructor() { this.entries = []; }
        connect(object, signal, callback) {
            if (!object || typeof object.connect !== "function") return 0;
            const id = object.connect(signal, callback);
            this.entries.push([object, id]);
            return id;
        }
        disconnectAll() { this.entries = []; }
    }
    const extensionModules = {
        calendar: {}, eventManager: {}, eventView: {},
        panelClock: { CLOCK_MODE_STANDARD: "standard" },
    };
    const runtimeSupport = {
        SignalBag,
        loadLocalModule(name) {
            loaderObservations.featureCalls += 1;
            return extensionModules[name];
        },
    };
    loaderObservations.featureCalls = 0;
    extensionModules.runtimeSupport = runtimeSupport;
    const extension = loaderMode === "extension" ? {
        getCurrentExtension() {
            loaderObservations.extensionCalls += 1;
            return { imports: extensionModules };
        },
    } : undefined;

    const context = {
        console,
        require(name) {
            loaderObservations.legacyCalls += 1;
            if (loaderMode === "extension") {
                throw new Error("legacy loader used by Cinnamon 6.7 path");
            }
            if (name === "./runtimeSupport") return runtimeSupport;
            return extensionModules[name.replace(/^\.\//, "")];
        },
        _: (value) => value,
        ngettext: (one, many, count) => count === 1 ? one : many,
        global: {
            logError() {},
            settings: { disconnect() {}, connect() { return 1; } },
        },
        imports: {
            ui: {
                extension,
                applet: {
                    Applet: AppletBase,
                    AllowedLayout: { BOTH: 0 },
                    AppletPopupMenu: class {},
                },
                popupMenu: {},
                settings: {},
                main: {
                    keybindingManager: {},
                    themeManager: { disconnect() {}, connect() { return 1; } },
                },
            },
            gi: {
                Gio: {},
                CalendarPlus: {
                    get_version() { return nativeVersion; },
                    CalendarSystem: {
                        new(id) {
                            return id === "gregorian" ? {
                                get_id() { return id; },
                            } : null;
                        },
                    },
                },
                Clutter: {},
                Pango: { EllipsizeMode: { NONE: 0 } },
                St: {
                    Label,
                    Bin,
                    Align: { MIDDLE: 0 },
                    Side: { LEFT: 0, RIGHT: 1 },
                },
                UPowerGlib: {},
                CinnamonDesktop: {
                    WallClock: {
                        lctime_format(domain, value) { return value; },
                    },
                },
            },
            misc: { util: {} },
            gettext: {
                bindtextdomain() {},
                domain() { return { gettext(value) { return value; } }; },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "applet.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__CalendarPlusApplet = CalendarPlusApplet;`,
        context,
        { filename: "applet.js" }
    );
    context.__CalendarPlusApplet.loaderObservations = loaderObservations;
    return context.__CalendarPlusApplet;
}

function evaluateEventsManager() {
    let nextWatchId = 1;
    let nextSourceId = 100;
    const timeouts = new Map();
    const observations = {
        watches: 0,
        unwatches: 0,
        proxyRequests: 0,
        serverDisconnects: 0,
        cancels: 0,
        clears: 0,
        removedSources: 0,
        timezoneMonitorCancels: 0,
        timezoneMonitorDisconnects: 0,
        rangeCalls: [],
        busAppeared: null,
        busVanished: null,
        proxyCallback: null,
        runNextTimeout() {
            const next = timeouts.entries().next();
            assert.equal(next.done, false, "a reconnect timeout must be pending");
            const [id, callback] = next.value;
            timeouts.delete(id);
            callback();
        },
    };

    class Cancellable {
        cancel() { observations.cancels += 1; }
    }

    class TimezoneMonitor {
        connect() { return 77; }
        disconnect() { observations.timezoneMonitorDisconnects += 1; }
        cancel() { observations.timezoneMonitorCancels += 1; }
    }

    const signals = {
        addSignalMethods(prototype) {
            prototype.connect = function() { return 1; };
            prototype.emit = function() {};
        },
    };
    class FakeDateTime {
        constructor(unix) { this.unix = unix; }
        to_unix() { return this.unix; }
        get_year() { return new Date(this.unix * 1000).getUTCFullYear(); }
        get_month() { return new Date(this.unix * 1000).getUTCMonth() + 1; }
        get_day_of_month() { return new Date(this.unix * 1000).getUTCDate(); }
        add_days(days) { return new FakeDateTime(this.unix + days * 86400); }
    }
    const dateTime = {
        new_from_unix_local(value) { return new FakeDateTime(value); },
        new_local(year, month, day, hour, minute, second) {
            return new FakeDateTime(
                Date.UTC(year, month - 1, day, hour, minute, second) / 1000
            );
        },
        new_now_local() { return new FakeDateTime(0); },
    };

    const context = {
        console,
        _: (value) => value,
        ngettext: (one, many, count) => count === 1 ? one : many,
        log() {},
        global: { logError() {} },
        require(name) {
            if (name === "./runtimeSupport") {
                return { SignalBag: class SignalBag {
                    constructor() { this.entries = []; }
                    connect(object, signal, callback) {
                        const id = object.connect(signal, callback);
                        this.entries.push([object, id]);
                        return id;
                    }
                    disconnectAll() {
                        for (const [object, id] of this.entries.splice(0)) {
                            object.disconnect(id);
                        }
                    }
                }};
            }
            throw new Error(`unexpected module ${name}`);
        },
        imports: {
            gi: {
                Clutter: {},
                Gio: {
                    Cancellable,
                    File: {
                        new_for_path() {
                            return {
                                monitor_file() { return new TimezoneMonitor(); },
                            };
                        },
                    },
                    FileMonitorFlags: { NONE: 0 },
                    BusType: { SESSION: 0 },
                    BusNameWatcherFlags: { NONE: 0 },
                    DBusProxyFlags: { DO_NOT_AUTO_START_AT_CONSTRUCTION: 0 },
                    bus_watch_name(busType, name, flags, appeared, vanished) {
                        observations.watches += 1;
                        observations.busAppeared = appeared;
                        observations.busVanished = vanished;
                        return nextWatchId++;
                    },
                    bus_unwatch_name() { observations.unwatches += 1; },
                },
                GLib: {
                    DateTime: dateTime,
                    SOURCE_REMOVE: false,
                    TIME_SPAN_MINUTE: 60,
                    get_monotonic_time() { return 123456; },
                },
                St: {},
                Pango: {},
                Cinnamon: {
                    CalendarServerProxy: {
                        new_for_bus(busType, flags, name, path, cancellable, callback) {
                            observations.proxyRequests += 1;
                            observations.proxyCallback = callback;
                        },
                        new_for_bus_finish(result) {
                            if (result.error) {
                                throw result.error;
                            }
                            return result.server;
                        },
                    },
                },
                CalendarPlus: {
                    EventStore: {
                        new() {
                            return {
                                clear() { observations.clears += 1; },
                                refresh_timezone() { return false; },
                            };
                        },
                    },
                },
                Atk: {},
                Gtk: {},
                CinnamonDesktop: {
                    WallClock: {
                        lctime_format(domain, value) { return value; },
                    },
                },
            },
            lang: { bind(owner, fn) { return fn.bind(owner); } },
            signals,
            ui: { settings: {}, separator: {}, main: {} },
            misc: { util: {}, interfaces: {} },
            mainloop: {
                timeout_add_seconds(delay, callback) {
                    const id = nextSourceId++;
                    timeouts.set(id, callback);
                    return id;
                },
                source_remove(id) {
                    timeouts.delete(id);
                    observations.removedSources += 1;
                },
            },
            gettext: {
                bindtextdomain() {},
                domain() { return { gettext(value) { return value; } }; },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "eventManager.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__EventsManager = EventsManager;`,
        context,
        { filename: "eventManager.js" }
    );
    return { EventsManager: context.__EventsManager, observations };
}


function evaluateCalendar() {
    let nextSignalId = 1;
    const observations = {
        actorDisconnects: 0,
        actorDestroys: 0,
        desktopDisconnects: 0,
        eventDisconnects: 0,
        removedSources: 0,
    };

    class Table {
        constructor() {}
        connect() { return nextSignalId++; }
        disconnect() { observations.actorDisconnects += 1; }
        destroy() { observations.actorDestroys += 1; }
    }

    class DesktopSettings {
        connect() { return nextSignalId++; }
        disconnect() { observations.desktopDisconnects += 1; }
    }

    const signals = {
        addSignalMethods(prototype) {
            prototype.connect = prototype.connect || function() {
                return nextSignalId++;
            };
            prototype.emit = function() {};
        },
    };

    const context = {
        console,
        global: { logError() {} },
        log() {},
        require(name) {
            if (name === "./runtimeSupport") {
                return { SignalBag: class SignalBag {
                    constructor() { this.entries = []; }
                    connect(object, signal, callback) {
                        const id = object.connect(signal, callback);
                        this.entries.push([object, id]);
                        return id;
                    }
                    disconnectAll() {
                        for (const [object, id] of this.entries.splice(0)) {
                            object.disconnect(id);
                        }
                    }
                }};
            }
            throw new Error(`unexpected module ${name}`);
        },
        imports: {
            gi: {
                Clutter: {},
                Gio: { Settings: DesktopSettings },
                GLib: { SOURCE_REMOVE: false },
                St: { Table },
                Pango: { SCALE: 1024 },
                Cinnamon: {
                    util_get_week_start() { return 0; },
                },
                CalendarPlus: {
                    DatePart: { DAY: 1, MONTH: 2, YEAR: 3, FULL: 4 },
                    CalendarSystem: {
                        new(id) {
                            return id === "gregorian" ? {
                                get_id() { return id; },
                            } : null;
                        },
                    },
                    date_same() { return true; },
                    date_is_work_day() { return true; },
                },
            },
            lang: { bind(owner, fn) { return fn.bind(owner); } },
            signals,
            ui: { main: {} },
            mainloop: {
                source_remove() { observations.removedSources += 1; },
            },
            gettext: {
                bindtextdomain() {},
                domain() { return { gettext(value) { return value; } }; },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "calendar.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__Calendar = Calendar;`,
        context,
        { filename: "calendar.js" }
    );

    context.__Calendar.prototype._buildHeader = function() {};
    const settings = { bindWithObject() {} };
    const eventsManager = {
        connect() { return nextSignalId++; },
        disconnect() { observations.eventDisconnects += 1; },
    };

    return {
        Calendar: context.__Calendar,
        settings,
        eventsManager,
        observations,
    };
}

function evaluatePanelClock() {
    const observations = { starts: 0, stops: 0 };

    const context = {
        console,
        _: (value) => value,
        global: { logError() {} },
        imports: {
            gi: {
                CalendarPlus: {
                    DatePart: { SHORT: 4, FULL: 5 },
                    time_mode_from_string(mode) { return mode; },
                    time_mode_requires_longitude(mode) {
                        return [
                            "sidereal",
                            "solar",
                            "mean-solar",
                            "roman-temporal",
                            "japanese-temporal",
                            "italian-hours",
                            "babylonian-hours",
                        ].includes(mode);
                    },
                    time_mode_requires_latitude(mode) {
                        return [
                            "roman-temporal",
                            "japanese-temporal",
                            "italian-hours",
                            "babylonian-hours",
                        ].includes(mode);
                    },
                    replace_time(label, normalTime, nativeTime) {
                        if (typeof label !== "string") {
                            throw new TypeError("label must be a string");
                        }
                        const index = label.indexOf(normalTime);
                        if (index < 0) return null;
                        return label.slice(0, index) + nativeTime +
                            label.slice(index + normalTime.length);
                    },
                },
                CinnamonDesktop: {
                    WallClock: {
                        lctime_format(domain, value) { return value; },
                    },
                },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "panelClock.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__PanelClock = { panelText, syncNativeClock };`,
        context,
        { filename: "panelClock.js" }
    );

    const systemClock = {
        get_time() { return "@500"; },
        start() { observations.starts += 1; },
        start_at_location() { observations.starts += 1; },
        stop() { observations.stops += 1; },
    };
    return { PanelClock: context.__PanelClock, systemClock, observations };
}

function testPanelClockDefensiveFormatting() {
    const { PanelClock, systemClock, observations } = evaluatePanelClock();
    const clock = {
        get_clock() { return "12:34"; },
        get_clock_for_format() { return null; },
    };
    const base = {
        mode: "decimal",
        showSeconds: false,
        latitude: 0,
        longitude: 0,
        locationConfigured: true,
        useCustomFormat: true,
        customFormat: "%Q-invalid",
        customTooltipFormat: "%Q-invalid",
        pointerInside: true,
        vertical: false,
        desktopSettings: {
            get_boolean(key) { return key === "clock-use-24h"; },
        },
        primaryCalendar: "gregorian",
    };

    assert.equal(
        PanelClock.panelText(clock, systemClock, base),
        "@500",
        "invalid custom formatting must fall back to native time"
    );

    const conventional = { ...base, mode: "standard" };
    assert.equal(
        PanelClock.panelText(clock, systemClock, conventional),
        "12:34",
        "conventional custom formatting must fall back to WallClock text"
    );

    const unconfigured = {
        ...base,
        mode: "sidereal",
        locationConfigured: false,
    };
    assert.equal(
        PanelClock.panelText(clock, systemClock, unconfigured),
        "N/A LOC",
        "location-dependent clocks must not silently assume Greenwich"
    );
    PanelClock.syncNativeClock(systemClock, unconfigured);
    assert.equal(observations.stops, 1);

    PanelClock.syncNativeClock(systemClock, base);
    assert.equal(observations.starts, 1);
}

function testLocationMigration() {
    const AppletClass = evaluateApplet();
    const applet = Object.create(AppletClass.prototype);
    const values = {
        "location-configured": false,
        latitude: -36.39,
        longitude: 145.36,
    };
    applet.settings = {
        getValue(key) { return values[key]; },
        setValue(key, value) { values[key] = value; },
    };
    applet._migrateLocationSetting();
    assert.equal(
        values["location-configured"],
        true,
        "existing non-zero coordinates must migrate to configured"
    );

    values["location-configured"] = false;
    values.latitude = 0;
    values.longitude = 0;
    applet._migrateLocationSetting();
    assert.equal(
        values["location-configured"],
        false,
        "legacy 0,0 must remain explicitly unconfigured"
    );
}

function testConstructorAtomicity() {
    const MismatchedAppletClass = evaluateApplet("3.1.9");
    assert.throws(
        () => new MismatchedAppletClass(0, 24, 7, version),
        new RegExp(`native library 3\\.1\\.9 does not match applet ${version.replaceAll(".", "\\.")}`),
        "native version mismatch must fail with the original diagnostic"
    );

    const AppletClass = evaluateApplet();
    const originalInitialise = AppletClass.prototype._initialiseState;
    const originalBuild = AppletClass.prototype._buildApplet;
    const originalDestroy = AppletClass.prototype._destroy;
    const initialiseSentinel = new Error("initialisation failed");
    const buildSentinel = new Error("construction failed");
    let destroyed = 0;

    AppletClass.prototype._destroy = function() {
        destroyed += 1;
        this._destroyed = true;
    };
    AppletClass.prototype._initialiseState = function() {
        this._destroyed = false;
        throw initialiseSentinel;
    };
    assert.throws(() => new AppletClass(0, 24, 7, version), initialiseSentinel);
    assert.equal(destroyed, 1, "failed initialisation must clean up exactly once");

    AppletClass.prototype._initialiseState = originalInitialise;
    AppletClass.prototype._buildApplet = function() { throw buildSentinel; };
    assert.throws(() => new AppletClass(0, 24, 7, version), buildSentinel);
    assert.equal(destroyed, 2, "failed construction must clean up exactly once");

    AppletClass.prototype._buildApplet = function() {};
    AppletClass.prototype._destroy = originalDestroy;
    const instance = new AppletClass(0, 24, 7, version);
    assert.equal(instance._destroyed, false);
    assert.equal(instance.primary_calendar, "gregorian");
    assert.equal(instance._primary_calendar_system.get_id(), "gregorian");
    instance._destroy();
    instance._destroy();
    assert.equal(instance._destroyed, true, "destruction must be idempotent");

    AppletClass.prototype._buildApplet = originalBuild;
}

function testModuleLoaderCompatibility() {
    const LegacyAppletClass = evaluateApplet(version, "legacy");
    assert.equal(
        LegacyAppletClass.loaderObservations.legacyCalls,
        1,
        "Cinnamon 6.4/6.6 must bootstrap runtimeSupport through require()"
    );
    assert.equal(LegacyAppletClass.loaderObservations.extensionCalls, 0);
    assert.equal(LegacyAppletClass.loaderObservations.featureCalls, 4);

    const ExtensionAppletClass = evaluateApplet(version, "extension");
    assert.equal(
        ExtensionAppletClass.loaderObservations.extensionCalls,
        1,
        "Cinnamon 6.7 must bootstrap runtimeSupport from the extension"
    );
    assert.equal(
        ExtensionAppletClass.loaderObservations.legacyCalls,
        0,
        "the 6.7 path must not fall through to the legacy loader"
    );
    assert.equal(ExtensionAppletClass.loaderObservations.featureCalls, 4);
}


function testCalendarLifecycle() {
    const { Calendar, settings, eventsManager, observations } =
        evaluateCalendar();
    const calendar = new Calendar(settings, eventsManager);

    calendar._update_id = 41;
    calendar._set_date_idle_id = 42;
    calendar.destroy();
    calendar.destroy();

    assert.equal(
        observations.removedSources,
        2,
        "calendar must release both pending Mainloop sources"
    );
    assert.equal(
        observations.desktopDisconnects,
        1,
        "calendar must disconnect its desktop-settings signal"
    );
    assert.equal(
        observations.eventDisconnects,
        3,
        "calendar must disconnect every EventsManager signal"
    );
    assert.equal(
        observations.actorDisconnects,
        2,
        "calendar must disconnect its long-lived actor signals"
    );
    assert.equal(
        observations.actorDestroys,
        1,
        "calendar actor destruction must be idempotent"
    );
    assert.equal(calendar._destroyed, true);
}

function testVisibleEventRange() {
    const { EventsManager, observations } = evaluateEventsManager();
    const manager = new EventsManager({ getValue() { return true; } }, {});
    manager._inited = true;
    manager._calendar_server = {
        status: 2,
        call_set_time_range(start, end, force) {
            observations.rangeCalls.push([start, end, force]);
        },
    };

    const first = new Date(Date.UTC(2026, 7, 2, 12, 0, 0));
    const last = new Date(Date.UTC(2026, 8, 12, 12, 0, 0));
    manager.set_visible_range(first, last, false);
    assert.equal(observations.rangeCalls.length, 1);
    assert.equal(
        observations.rangeCalls[0][1] - observations.rangeCalls[0][0],
        42 * 86400 - 1,
        "six visible weeks must be fetched as one inclusive interval"
    );

    manager.set_visible_range(first, last, false);
    assert.equal(
        observations.rangeCalls.length,
        1,
        "unchanged visible range must not trigger a duplicate fetch"
    );
    manager.set_visible_range(first, last, true);
    assert.equal(
        observations.rangeCalls.length,
        2,
        "forced refresh must re-query the current visible range"
    );
    manager.destroy();
}


function testEventsManagerLifecycle() {
    const { EventsManager, observations } = evaluateEventsManager();
    const settings = { getValue() { return true; } };
    const manager = new EventsManager(settings, {});

    manager.start_events();
    manager.start_events();
    assert.equal(observations.watches, 1, "bus watch must be idempotent");

    manager.destroy();
    manager.destroy();
    assert.equal(observations.unwatches, 1, "bus watch must be released once");
    assert.equal(observations.cancels, 1, "pending D-Bus work must be cancelled");
    assert.equal(observations.clears, 1, "native event data must be released");
    assert.equal(
        observations.timezoneMonitorDisconnects,
        1,
        "timezone monitor signal must be disconnected once"
    );
    assert.equal(
        observations.timezoneMonitorCancels,
        1,
        "timezone monitor must be cancelled once"
    );
    assert.equal(manager._destroyed, true);
}

function testEventsManagerReconnect() {
    const { EventsManager, observations } = evaluateEventsManager();
    const manager = new EventsManager({ getValue() { return true; } }, {});
    let nextSignalId = 1;
    const server = {
        status: 2,
        connect() { return nextSignalId++; },
        disconnect() { observations.serverDisconnects += 1; },
    };

    manager.start_events();
    observations.busAppeared();
    assert.equal(observations.proxyRequests, 1);
    assert.equal(observations.unwatches, 0, "service watch must survive connection");
    observations.proxyCallback(null, { server });
    assert.equal(manager._inited, true);

    observations.busVanished();
    assert.equal(manager._inited, false);
    assert.equal(manager._calendar_server, null);
    assert.equal(observations.serverDisconnects, 4);
    assert.equal(observations.clears, 1, "stale events must be dropped on restart");

    observations.busAppeared();
    assert.equal(observations.proxyRequests, 2, "service return must reconnect");
    const staleCallback = observations.proxyCallback;
    observations.busVanished();
    staleCallback(null, { server });
    assert.equal(manager._calendar_server, null, "stale proxy completion must be ignored");

    observations.busAppeared();
    assert.equal(observations.proxyRequests, 3);
    observations.proxyCallback(null, { error: new Error("transient failure") });
    observations.runNextTimeout();
    assert.equal(observations.proxyRequests, 4, "proxy failure must be retried");
    observations.proxyCallback(null, { server });
    assert.equal(manager._inited, true);

    manager.destroy();
    assert.equal(observations.unwatches, 1, "persistent watch must be released");
}

testPanelClockDefensiveFormatting();
testLocationMigration();
testConstructorAtomicity();
testModuleLoaderCompatibility();
testCalendarLifecycle();
testVisibleEventRange();
testEventsManagerLifecycle();
testEventsManagerReconnect();
console.log("JavaScript runtime lifecycle tests passed.");
