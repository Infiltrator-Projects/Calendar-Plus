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
    /* Degrees east of Greenwich; ignored by modes that do not require it. */
    gdouble longitude;
} CalendarPlusClockConfig;

enum
{
    CALENDAR_PLUS_CLOCK_TIME_SOURCE_ABI = 1,
    CALENDAR_PLUS_CLOCK_SCHEDULER_ABI = 1
};

gboolean calendar_plus_clock_interfaces_are_valid(
    const CalendarPlusClockTimeSource *time_source,
    const CalendarPlusClockScheduler *scheduler);
/*
 * The engine copies both interface tables but borrows their contexts and
 * @tick_data.  Those objects must outlive the engine.  Returns NULL when an
 * interface ABI or required callback is invalid.
 */
CalendarPlusClockEngine *calendar_plus_clock_engine_new(
    const CalendarPlusClockTimeSource *time_source,
    const CalendarPlusClockScheduler *scheduler,
    CalendarPlusClockTickFunc tick,
    gpointer tick_data);
void calendar_plus_clock_engine_free(CalendarPlusClockEngine *engine);

/* Longitude is finite-clamped to -180..180; returns FALSE if no timer armed. */
gboolean calendar_plus_clock_engine_start(
    CalendarPlusClockEngine *engine,
    const CalendarPlusClockConfig *config);
void calendar_plus_clock_engine_stop(CalendarPlusClockEngine *engine);
gboolean calendar_plus_clock_engine_is_running(
    const CalendarPlusClockEngine *engine);
/* Returns a newly allocated UTF-8 representation of the current instant. */
gchar *calendar_plus_clock_engine_format(
    const CalendarPlusClockEngine *engine);

G_END_DECLS

#endif
