/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Nuremberg historical hours.
 *
 * Surviving Nuremberg instruments carry separate equal-hour counts for the
 * daylight and night portions of the civil day.  The count begins again at
 * sunrise and again at sunset, so noon may be hour 8 near midsummer but only
 * hour 4 near midwinter.  Calendar Plus displays elapsed equal hours within
 * the current daylight/night span and resets at the computed physical solar
 * boundary.  This is distinct from Italian hours (one sunset-to-sunset count)
 * and Babylonian hours (one sunrise-to-sunrise count).
 */

#include "time-formats-internal.h"
#include "time-astronomy.h"

#include <math.h>

typedef enum
{
    NUREMBERG_BOUNDARY_SUNRISE,
    NUREMBERG_BOUNDARY_SUNSET
} NurembergBoundary;

static gboolean
nuremberg_boundary_window(gint64 unix_microseconds,
                          gdouble latitude,
                          gdouble longitude,
                          gint64 *previous,
                          gint64 *next,
                          NurembergBoundary *previous_kind)
{
    gint64 best_previous = G_MININT64;
    gint64 best_next = G_MAXINT64;
    NurembergBoundary best_kind = NUREMBERG_BOUNDARY_SUNRISE;
    gint offset;

    g_return_val_if_fail(previous != NULL, FALSE);
    g_return_val_if_fail(previous_kind != NULL, FALSE);

    for (offset = -2; offset <= 2; offset++)
    {
        const gint64 delta = (gint64)offset * MICROSECONDS_PER_DAY;
        gint64 sample;
        gint64 dawn;
        gint64 dusk;
        gint64 candidates[2];
        NurembergBoundary kinds[2] = {
            NUREMBERG_BOUNDARY_SUNRISE,
            NUREMBERG_BOUNDARY_SUNSET
        };
        guint index;

        if ((delta < 0 && unix_microseconds < G_MININT64 - delta) ||
            (delta > 0 && unix_microseconds > G_MAXINT64 - delta))
        {
            continue;
        }

        sample = unix_microseconds + delta;
        if (!calendar_plus_solar_boundary_instants(sample,
                                                   latitude,
                                                   longitude,
                                                   0.833,
                                                   &dawn,
                                                   &dusk))
        {
            continue;
        }

        candidates[0] = dawn;
        candidates[1] = dusk;
        for (index = 0; index < G_N_ELEMENTS(candidates); index++)
        {
            const gint64 candidate = candidates[index];

            if (candidate <= unix_microseconds && candidate > best_previous)
            {
                best_previous = candidate;
                best_kind = kinds[index];
            }
            if (candidate > unix_microseconds && candidate < best_next)
                best_next = candidate;
        }
    }

    if (best_previous == G_MININT64)
        return FALSE;

    *previous = best_previous;
    *previous_kind = best_kind;
    if (next != NULL)
        *next = best_next;
    return TRUE;
}

gchar *
format_nuremberg_hours_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gboolean vertical,
                                gdouble latitude,
                                gdouble longitude)
{
    gint64 start;
    gint64 elapsed_seconds;
    gint hour;
    gint minute;
    gint second;
    NurembergBoundary boundary;
    const gchar *period;
    const gchar *separator = vertical ? "\n" : ":";

    (void)utc_offset_seconds;
    if (!nuremberg_boundary_window(unix_microseconds,
                                   latitude,
                                   longitude,
                                   &start,
                                   NULL,
                                   &boundary))
    {
        return g_strdup(vertical ? "N/A\nNUR" : "N/A NUR");
    }

    elapsed_seconds = floor_divide(
        unix_microseconds - start, G_USEC_PER_SEC);
    hour = (gint)(elapsed_seconds / SECONDS_PER_HOUR);
    minute = (gint)((elapsed_seconds / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR);
    second = (gint)(elapsed_seconds % SECONDS_PER_MINUTE);
    period = boundary == NUREMBERG_BOUNDARY_SUNRISE ? "NUR-D" : "NUR-N";

    if (show_seconds)
    {
        return g_strdup_printf("%02d%s%02d%s%02d%s%s",
                               hour, separator, minute, separator, second,
                               vertical ? "\n" : " ", period);
    }

    return g_strdup_printf("%02d%s%02d%s%s",
                           hour, separator, minute,
                           vertical ? "\n" : " ", period);
}

guint
delay_nuremberg_hours_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gdouble latitude,
                                gdouble longitude)
{
    gint64 start;
    gint64 next;
    NurembergBoundary boundary;
    guint tick_delay;

    (void)utc_offset_seconds;
    if (!nuremberg_boundary_window(unix_microseconds,
                                   latitude,
                                   longitude,
                                   &start,
                                   &next,
                                   &boundary))
    {
        return 3600000;
    }

    (void)boundary;
    tick_delay = delay_for_integer_period(
        unix_microseconds - start,
        (show_seconds ? 1 : SECONDS_PER_MINUTE) * (gint64)G_USEC_PER_SEC);

    if (next != G_MAXINT64)
    {
        const guint reset_delay =
            delay_continuous_microseconds_to_milliseconds(
                (long double)(next - unix_microseconds));
        return MIN(tick_delay, reset_delay);
    }

    return tick_delay;
}
