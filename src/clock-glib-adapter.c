/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * GLib main-loop implementation of the neutral clock interfaces.
 */

#include "clock-glib-adapter.h"

typedef struct
{
    CalendarPlusClockTimerFunc callback;
    gpointer callback_data;
} GlibTimerDispatch;

static gint64
glib_now(gpointer context)
{
    (void)context;
    return g_get_real_time();
}

static gint
glib_utc_offset(gpointer context,
                gint64 unix_microseconds)
{
    g_autoptr(GDateTime) local = g_date_time_new_from_unix_local(
        unix_microseconds / G_USEC_PER_SEC);

    (void)context;
    return local != NULL ?
        (gint)(g_date_time_get_utc_offset(local) / G_TIME_SPAN_SECOND) : 0;
}

static gboolean
dispatch_timer(gpointer user_data)
{
    GlibTimerDispatch *dispatch = user_data;

    dispatch->callback(dispatch->callback_data);
    return G_SOURCE_REMOVE;
}

static CalendarPlusClockTimer
glib_schedule_once(gpointer context,
                   guint delay_milliseconds,
                   CalendarPlusClockTimerFunc callback,
                   gpointer callback_data)
{
    GlibTimerDispatch *dispatch = g_new(GlibTimerDispatch, 1);
    guint source;

    (void)context;
    dispatch->callback = callback;
    dispatch->callback_data = callback_data;
    source = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                delay_milliseconds,
                                dispatch_timer,
                                dispatch,
                                g_free);
    return (CalendarPlusClockTimer)source;
}

static void
glib_cancel(gpointer context,
            CalendarPlusClockTimer timer)
{
    (void)context;
    if (timer <= G_MAXUINT)
        g_source_remove((guint)timer);
}

void
calendar_plus_clock_glib_interfaces(
    CalendarPlusClockTimeSource *time_source,
    CalendarPlusClockScheduler *scheduler)
{
    g_return_if_fail(time_source != NULL);
    g_return_if_fail(scheduler != NULL);

    time_source->abi_version = CALENDAR_PLUS_CLOCK_TIME_SOURCE_ABI;
    time_source->context = NULL;
    time_source->now = glib_now;
    time_source->utc_offset = glib_utc_offset;

    scheduler->abi_version = CALENDAR_PLUS_CLOCK_SCHEDULER_ABI;
    scheduler->context = NULL;
    scheduler->schedule_once = glib_schedule_once;
    scheduler->cancel = glib_cancel;
}
