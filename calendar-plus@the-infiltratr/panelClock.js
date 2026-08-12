// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Panel-clock presentation policy.
 *
 * This module has no actor ownership.  It translates Calendar Plus settings
 * and Cinnamon desktop preferences into WallClock formatting, native timer
 * configuration and the final panel/date strings.  Keeping those decisions
 * out of applet.js leaves the applet controller responsible for lifecycle and
 * wiring rather than clock-format policy.
 *
 * Native time is still calculated and boundary-scheduled in libcalendar-plus;
 * CinnamonDesktop.WallClock remains authoritative for conventional localized
 * time and surrounding date text.
 */

const CalendarPlus = imports.gi.CalendarPlus;
const CinnamonDesktop = imports.gi.CinnamonDesktop;

var CLOCK_MODE_STANDARD = "standard";
var CLOCK_MODE_STANDARD_24 = "standard-24";
var CLOCK_MODE_STANDARD_12 = "standard-12";

const DAY_FORMAT = CinnamonDesktop.WallClock.lctime_format("cinnamon", "%A");
const DATE_FORMAT_SHORT = CinnamonDesktop.WallClock.lctime_format(
    "cinnamon", _("%B %-e, %Y")
);
const DATE_FORMAT_FULL = CinnamonDesktop.WallClock.lctime_format(
    "cinnamon", _("%A, %B %-e, %Y")
);

/*
 * These strings intentionally pass through CinnamonDesktop.WallClock.  That
 * preserves locale-aware day/month ordering while Calendar Plus independently
 * controls whether this applet instance displays seconds.
 */
const DEFAULT_CLOCK_FORMATS = Object.freeze({
    withDate24Seconds: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%A %B %-e, %R:%S"
    ),
    withDate12Seconds: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%A %B %-e, %-l:%M:%S %p"
    ),
    withDate24: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%A %B %-e, %R"
    ),
    withDate12: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%A %B %-e, %-l:%M %p"
    ),
    withoutDate24Seconds: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%R:%S"
    ),
    withoutDate12Seconds: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%-l:%M:%S %p"
    ),
    withoutDate24: CinnamonDesktop.WallClock.lctime_format("cinnamon", "%R"),
    withoutDate12: CinnamonDesktop.WallClock.lctime_format(
        "cinnamon", "%-l:%M %p"
    ),
});

/*
 * A native horizontal clock retains Cinnamon's localized surrounding date and
 * replaces only the conventional time token.  Longer candidates come first so
 * HH:MM cannot consume the prefix of HH:MM:SS.
 */
const CONVENTIONAL_TIME_FORMATS = Object.freeze([
    "%H:%M:%S",
    "%-l:%M:%S %p",
    "%l:%M:%S %p",
    "%I:%M:%S %p",
    "%H:%M",
    "%-l:%M %p",
    "%l:%M %p",
    "%I:%M %p",
]);

function isNativeClockMode(mode) {
    return ![
        CLOCK_MODE_STANDARD,
        CLOCK_MODE_STANDARD_24,
        CLOCK_MODE_STANDARD_12,
    ].includes(mode);
}

function uses24HourClock(mode, desktopSettings) {
    if (mode === CLOCK_MODE_STANDARD_24) {
        return true;
    }
    if (mode === CLOCK_MODE_STANDARD_12) {
        return false;
    }
    return desktopSettings.get_boolean("clock-use-24h");
}

function defaultHorizontalFormat(config) {
    const showDate = config.desktopSettings.get_boolean("clock-show-date");
    const use24 = uses24HourClock(config.mode, config.desktopSettings);
    const seconds = config.showSeconds && !isNativeClockMode(config.mode);

    if (showDate) {
        if (use24) {
            return seconds
                ? DEFAULT_CLOCK_FORMATS.withDate24Seconds
                : DEFAULT_CLOCK_FORMATS.withDate24;
        }
        return seconds
            ? DEFAULT_CLOCK_FORMATS.withDate12Seconds
            : DEFAULT_CLOCK_FORMATS.withDate12;
    }

    if (use24) {
        return seconds
            ? DEFAULT_CLOCK_FORMATS.withoutDate24Seconds
            : DEFAULT_CLOCK_FORMATS.withoutDate24;
    }
    return seconds
        ? DEFAULT_CLOCK_FORMATS.withoutDate12Seconds
        : DEFAULT_CLOCK_FORMATS.withoutDate12;
}

function configureWallClock(clock, config) {
    if (!clock) {
        return;
    }

    let format;
    if (config.useCustomFormat) {
        format = config.customFormat;
        if (config.pointerInside) {
            format += config.customTooltipFormat;
        }
    } else if (config.vertical) {
        const hour = uses24HourClock(config.mode, config.desktopSettings)
            ? "%H"
            : "%l";
        const seconds = config.showSeconds && !isNativeClockMode(config.mode);
        format = seconds ? `${hour}%n%M%n%S` : `${hour}%n%M`;
    } else {
        format = defaultHorizontalFormat(config);
    }

    if (!clock.set_format_string(format)) {
        global.logError("calendar-plus@the-infiltratr: invalid panel time format.");
        clock.set_format_string("~CLOCK FORMAT ERROR~ %l:%M %p");
    }
}

function syncNativeClock(systemClock, config) {
    if (!systemClock) {
        return;
    }
    if (!isNativeClockMode(config.mode)) {
        systemClock.stop();
        return;
    }

    let nativeMode = config.mode;
    if (config.mode === "roman-temporal" ||
        config.mode === "japanese-temporal") {
        const latitude = Number.isFinite(config.latitude)
            ? Math.max(-90, Math.min(90, config.latitude))
            : 0;
        nativeMode = `${config.mode}@${latitude.toFixed(4)}`;
    }

    systemClock.start(
        nativeMode,
        config.showSeconds,
        config.vertical,
        config.longitude
    );
}

function nativePanelText(clock, systemClock, config) {
    const nativeTime = systemClock.get_time();
    if (config.vertical) {
        return nativeTime;
    }

    const shellText = config.useCustomFormat && config.pointerInside
        ? clock.get_clock_for_format(config.customFormat)
        : clock.get_clock();

    for (const pattern of CONVENTIONAL_TIME_FORMATS) {
        const normalTime = clock.get_clock_for_format(pattern);
        const replaced = CalendarPlus.replace_time(
            shellText,
            normalTime,
            nativeTime
        );
        if (replaced !== null) {
            return replaced.capitalize();
        }
    }

    /* A custom pattern may intentionally contain no conventional time token. */
    return nativeTime;
}

function panelText(clock, systemClock, config) {
    if (isNativeClockMode(config.mode)) {
        return systemClock ? nativePanelText(clock, systemClock, config) : null;
    }
    if (config.useCustomFormat && config.pointerInside) {
        return clock.get_clock_for_format(config.customFormat);
    }

    const text = clock.get_clock();
    return config.useCustomFormat ? text : text.capitalize();
}

function todayDisplay(clock, primaryCalendarSystem, config) {
    const today = new Date();
    const args = [today.getFullYear(), today.getMonth() + 1, today.getDate()];

    let shortDate;
    let tooltip;
    if (config.primaryCalendar === "gregorian") {
        shortDate = clock.get_clock_for_format(DATE_FORMAT_SHORT).capitalize();
        tooltip = clock.get_clock_for_format(DATE_FORMAT_FULL).capitalize();
    } else {
        shortDate = primaryCalendarSystem.format_date_part(
            ...args,
            CalendarPlus.DatePart.SHORT
        );
        tooltip = primaryCalendarSystem.format_date_part(
            ...args,
            CalendarPlus.DatePart.FULL
        );
    }

    if (config.useCustomFormat) {
        try {
            tooltip = clock
                .get_clock_for_format(config.customTooltipFormat)
                .capitalize();
        } catch (error) {
            global.logError(
                "calendar-plus@the-infiltratr: invalid tooltip time format."
            );
        }
    }

    return { today, args, shortDate, tooltip };
}

function dayName(clock) {
    return clock.get_clock_for_format(DAY_FORMAT).capitalize();
}
