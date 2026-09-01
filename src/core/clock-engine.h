/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Platform-neutral live-clock engine.
 */

#ifndef CALENDAR_PLUS_CLOCK_ENGINE_H
#define CALENDAR_PLUS_CLOCK_ENGINE_H

#include "time-formats.h"

G_BEGIN_DECLS

typedef struct _CalendarPlusClockEngine CalendarPlusClockEngine;
/* Zero is reserved for scheduling failure/no active timer. */
typedef guintptr CalendarPlusClockTimer;
typedef void (*CalendarPlusClockTimerFunc)(gpointer user_data);
typedef void (*CalendarPlusClockTickFunc)(gpointer user_data);

typedef gint64 (*CalendarPlusClockNowFunc)(gpointer context);
typedef gint (*CalendarPlusClockUtcOffsetFunc)(gpointer context,
                                              gint64 unix_microseconds);
typedef CalendarPlusClockTimer (*CalendarPlusClockScheduleFunc)(
    gpointer context,
    guint delay_milliseconds,
    CalendarPlusClockTimerFunc callback,
    gpointer callback_data);
typedef void (*CalendarPlusClockCancelFunc)(gpointer context,
                                           CalendarPlusClockTimer timer);

typedef struct
{
    guint abi_version;
    gpointer context;
    /* Microseconds since 1970-01-01 00:00:00 UTC. */
    CalendarPlusClockNowFunc now;
    /* Local UTC offset in seconds, including daylight saving at the instant. */
    CalendarPlusClockUtcOffsetFunc utc_offset;
} CalendarPlusClockTimeSource;

typedef struct
{
    guint abi_version;
    gpointer context;
    /*
     * Arms one callback after at least @delay_milliseconds and returns a
     * non-zero handle.  It is a one-shot: the callback must run at most once.
     */
    CalendarPlusClockScheduleFunc schedule_once;
    /* Prevents a pending callback for @timer; never called with zero. */
    CalendarPlusClockCancelFunc cancel;
} CalendarPlusClockScheduler;

typedef struct
{
    CalendarPlusTimeMode mode;
    gboolean show_seconds;
    gboolean vertical;
    /* Degrees north; ignored by modes that do not require latitude. */
    gdouble latitude;
    /* Degrees east of Greenwich; ignored by modes that do not require it. */
    gdouble longitude;
} CalendarPlusClockConfig;

enum
{
    CALENDAR_PLUS_CLOCK_TIME_SOURCE_ABI = 1,
    CALENDAR_PLUS_CLOCK_SCHEDULER_ABI = 1
};

/**
 * calendar_plus_clock_interfaces_are_valid:
 * @time_source: time-source interface table
 * @scheduler: one-shot scheduler interface table
 *
 * Returns: whether both ABI versions and required callbacks are valid.
 */
gboolean calendar_plus_clock_interfaces_are_valid(
    const CalendarPlusClockTimeSource *time_source,
    const CalendarPlusClockScheduler *scheduler);
/**
 * calendar_plus_clock_engine_new:
 * @time_source: valid time-source interface
 * @scheduler: valid one-shot scheduler interface
 * @tick: callback after each visible transition
 * @tick_data: borrowed callback context
 *
 * The engine copies both tables but borrows their contexts and @tick_data.
 *
 * Returns: (transfer full) (nullable): engine, or %NULL for invalid interfaces
 */
CalendarPlusClockEngine *calendar_plus_clock_engine_new(
    const CalendarPlusClockTimeSource *time_source,
    const CalendarPlusClockScheduler *scheduler,
    CalendarPlusClockTickFunc tick,
    gpointer tick_data);
/** Accepts %NULL and cancels any armed one-shot before release. */
void calendar_plus_clock_engine_free(CalendarPlusClockEngine *engine);

/**
 * calendar_plus_clock_engine_start:
 * @engine: clock engine
 * @config: display/scheduling configuration
 *
 * Latitude is finite-clamped to -90..90 and longitude to -180..180.
 *
 * Returns: %TRUE only when the first timer was successfully armed.
 */
gboolean calendar_plus_clock_engine_start(
    CalendarPlusClockEngine *engine,
    const CalendarPlusClockConfig *config);
/** Cancels the armed one-shot and makes the engine idle. */
void calendar_plus_clock_engine_stop(CalendarPlusClockEngine *engine);

/** Returns: whether the engine currently owns a non-zero timer handle. */
gboolean calendar_plus_clock_engine_is_running(
    const CalendarPlusClockEngine *engine);
/**
 * calendar_plus_clock_engine_format:
 * @engine: clock engine
 *
 * Returns: (transfer full): allocated current native-clock text
 */
gchar *calendar_plus_clock_engine_format(
    const CalendarPlusClockEngine *engine);

G_END_DECLS

#endif
