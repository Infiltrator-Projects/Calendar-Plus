/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Astronomical, scientific and seasonal native clock providers.
 *
 * Continuous astronomical calculations live in time-astronomy.c. This module
 * turns those values into panel strings and schedules their next visible
 * boundaries, including location-dependent Roman and Edo seasonal systems.
 */

#include "time-formats-internal.h"
#include "time-astronomy.h"

#include <math.h>

gchar *
format_sidereal_provider(gint64 unix_microseconds,
                         gint utc_offset_seconds,
                         gboolean show_seconds,
                         gboolean vertical,
                         gdouble latitude G_GNUC_UNUSED,
                         gdouble longitude)
{
    gint hour;
    gint minute;
    gint second;

    (void)utc_offset_seconds;
    split_clock_seconds(
        (gint64)floorl(
            calendar_plus_local_sidereal_seconds(unix_microseconds, longitude)),
        &hour, &minute, &second);
    return format_clock_fields(hour, minute, second, show_seconds, vertical,
                               "LST");
}

guint
delay_sidereal_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_local_sidereal_seconds(unix_microseconds, longitude),
        CALENDAR_PLUS_SIDEREAL_RATE,
        show_seconds);
}

gchar *
format_solar_provider(gint64 unix_microseconds,
                      gint utc_offset_seconds,
                      gboolean show_seconds,
                      gboolean vertical,
                      gdouble latitude G_GNUC_UNUSED,
                      gdouble longitude)
{
    gint hour;
    gint minute;
    gint second;

    (void)utc_offset_seconds;
    split_clock_seconds(
        (gint64)floorl(
            calendar_plus_apparent_solar_seconds(unix_microseconds, longitude)),
        &hour, &minute, &second);
    return format_clock_fields(hour, minute, second, show_seconds, vertical,
                               "SOL");
}

guint
delay_solar_provider(gint64 unix_microseconds,
                     gint utc_offset_seconds,
                     gboolean show_seconds,
                     gdouble latitude G_GNUC_UNUSED,
                     gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_apparent_solar_seconds(unix_microseconds, longitude),
        1.0L,
        show_seconds);
}

gchar *
format_julian_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gboolean vertical,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    const gint digits = show_seconds ? 5 : 3;
    const gint64 scale = show_seconds ? 100000 : 1000;
    const long double date = calendar_plus_julian_date(unix_microseconds);
    const gint64 whole_days = (gint64)floorl(date);
    gint64 fraction =
        (gint64)floorl((date - whole_days) * scale + 1.0e-10L);
    const gchar *separator = vertical ? "\n" : " ";

    (void)utc_offset_seconds;
    (void)longitude;
    if (fraction >= scale)
        fraction = scale - 1;

    return g_strdup_printf("JD%s%" G_GINT64_FORMAT ".%0*" G_GINT64_FORMAT,
                           separator, whole_days, digits, fraction);
}

guint
delay_julian_provider(gint64 unix_microseconds,
                      gint utc_offset_seconds,
                      gboolean show_seconds,
                      gdouble latitude G_GNUC_UNUSED,
                      gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY),
        show_seconds ? 100000 : 1000);
}

gchar *
format_mean_solar_provider(gint64 unix_microseconds,
                           gint utc_offset_seconds,
                           gboolean show_seconds,
                           gboolean vertical,
                           gdouble latitude G_GNUC_UNUSED,
                           gdouble longitude)
{
    gint hour;
    gint minute;
    gint second;

    (void)utc_offset_seconds;
    split_clock_seconds(
        (gint64)floorl(
            calendar_plus_mean_solar_seconds(unix_microseconds, longitude)),
        &hour, &minute, &second);
    return format_clock_fields(hour, minute, second, show_seconds, vertical,
                               "LMT");
}

guint
delay_mean_solar_provider(gint64 unix_microseconds,
                          gint utc_offset_seconds,
                          gboolean show_seconds,
                          gdouble latitude G_GNUC_UNUSED,
                          gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_for_clock_seconds(
        calendar_plus_mean_solar_seconds(unix_microseconds, longitude),
        1.0L,
        show_seconds);
}

gchar *
format_modified_julian_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gboolean vertical,
                                gdouble latitude G_GNUC_UNUSED,
                                gdouble longitude)
{
    const gint digits = show_seconds ? 5 : 3;
    const gint64 scale = show_seconds ? 100000 : 1000;
    const long double date =
        calendar_plus_julian_date(unix_microseconds) - 2400000.5L;
    const gint64 whole_days = (gint64)floorl(date);
    gint64 fraction =
        (gint64)floorl((date - whole_days) * scale + 1.0e-10L);
    const gchar *separator = vertical ? "\n" : " ";

    (void)utc_offset_seconds;
    (void)longitude;
    if (fraction >= scale)
        fraction = scale - 1;

    return g_strdup_printf("MJD%s%" G_GINT64_FORMAT ".%0*" G_GINT64_FORMAT,
                           separator, whole_days, digits, fraction);
}

guint
delay_modified_julian_provider(gint64 unix_microseconds,
                               gint utc_offset_seconds,
                               gboolean show_seconds,
                               gdouble latitude G_GNUC_UNUSED,
                               gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY),
        show_seconds ? 100000 : 1000);
}

typedef struct
{
    gboolean daylight;
    guint index;
    long double seconds_to_next;
} SeasonalPeriod;

/*
 * Split apparent-solar daytime and nighttime independently. This is the common
 * mathematical primitive behind Roman unequal hours and Edo Japanese toki;
 * each provider supplies its historically appropriate boundary altitude and
 * number of subdivisions.
 */
static gboolean
seasonal_period_at(gint64 unix_microseconds,
                   gdouble latitude,
                   gdouble longitude,
                   gdouble solar_depression_degrees,
                   guint daylight_parts,
                   guint night_parts,
                   SeasonalPeriod *period)
{
    long double dawn;
    long double dusk;
    const long double solar =
        calendar_plus_apparent_solar_seconds(unix_microseconds, longitude);
    long double span;
    long double position;
    long double unit;
    guint parts;

    g_return_val_if_fail(period != NULL, FALSE);

    if (!calendar_plus_solar_day_boundaries(unix_microseconds,
                                            latitude,
                                            solar_depression_degrees,
                                            &dawn,
                                            &dusk))
    {
        return FALSE;
    }

    if (solar >= dawn && solar < dusk)
    {
        period->daylight = TRUE;
        span = dusk - dawn;
        position = solar - dawn;
        parts = daylight_parts;
    }
    else
    {
        period->daylight = FALSE;
        span = SECONDS_PER_DAY - (dusk - dawn);
        position = solar >= dusk ?
            solar - dusk : SECONDS_PER_DAY - dusk + solar;
        parts = night_parts;
    }

    unit = span / parts;
    period->index = MIN((guint)floorl(position / unit), parts - 1);
    period->seconds_to_next =
        (period->index + 1) * unit - position;
    if (period->seconds_to_next <= 0.0L)
        period->seconds_to_next = unit;
    return TRUE;
}

static const gchar *const roman_numerals[] = {
    "I", "II", "III", "IV", "V", "VI",
    "VII", "VIII", "IX", "X", "XI", "XII"
};

gchar *
format_roman_temporal_provider(gint64 unix_microseconds,
                               gint utc_offset_seconds,
                               gboolean show_seconds G_GNUC_UNUSED,
                               gboolean vertical,
                               gdouble latitude,
                               gdouble longitude)
{
    SeasonalPeriod period;
    const gchar *separator = vertical ? "\n" : " ";

    (void)utc_offset_seconds;
    if (!seasonal_period_at(unix_microseconds,
                            latitude,
                            longitude,
                            0.833,
                            12,
                            4,
                            &period))
    {
        return g_strdup(vertical ? "N/A\nROM" : "N/A ROM");
    }

    return period.daylight ?
        g_strdup_printf("Hora%s%s", separator, roman_numerals[period.index]) :
        g_strdup_printf("Vigilia%s%s", separator, roman_numerals[period.index]);
}

guint
delay_roman_temporal_provider(gint64 unix_microseconds,
                              gint utc_offset_seconds,
                              gboolean show_seconds G_GNUC_UNUSED,
                              gdouble latitude,
                              gdouble longitude)
{
    SeasonalPeriod period;

    (void)utc_offset_seconds;
    if (!seasonal_period_at(unix_microseconds,
                            latitude,
                            longitude,
                            0.833,
                            12,
                            4,
                            &period))
    {
        return 3600000;
    }

    return delay_continuous_microseconds_to_milliseconds(
        period.seconds_to_next * G_USEC_PER_SEC);
}

typedef struct
{
    const gchar *character;
    guint number;
    const gchar *animal;
} JapaneseToki;

static const JapaneseToki japanese_day_toki[] = {
    { "卯", 6, "Rabbit" },
    { "辰", 5, "Dragon" },
    { "巳", 4, "Snake" },
    { "午", 9, "Horse" },
    { "未", 8, "Goat" },
    { "申", 7, "Monkey" }
};

static const JapaneseToki japanese_night_toki[] = {
    { "酉", 6, "Rooster" },
    { "戌", 5, "Dog" },
    { "亥", 4, "Boar" },
    { "子", 9, "Rat" },
    { "丑", 8, "Ox" },
    { "寅", 7, "Tiger" }
};

/* Kansei-calendar dawn/dusk: solar centre 7°21′40″ below the horizon. */
#define JAPANESE_DAWN_DEPRESSION (7.0 + 21.0 / 60.0 + 40.0 / 3600.0)

gchar *
format_japanese_temporal_provider(gint64 unix_microseconds,
                                  gint utc_offset_seconds,
                                  gboolean show_seconds G_GNUC_UNUSED,
                                  gboolean vertical,
                                  gdouble latitude,
                                  gdouble longitude)
{
    SeasonalPeriod period;
    const JapaneseToki *toki;

    (void)utc_offset_seconds;
    if (!seasonal_period_at(unix_microseconds,
                            latitude,
                            longitude,
                            JAPANESE_DAWN_DEPRESSION,
                            6,
                            6,
                            &period))
    {
        return g_strdup(vertical ? "N/A\n和時" : "N/A 和時");
    }

    toki = period.daylight ?
        &japanese_day_toki[period.index] :
        &japanese_night_toki[period.index];
    return vertical ?
        g_strdup_printf("%s %u\n%s", toki->character, toki->number,
                        toki->animal) :
        g_strdup_printf("%s %u %s", toki->character, toki->number,
                        toki->animal);
}

guint
delay_japanese_temporal_provider(gint64 unix_microseconds,
                                 gint utc_offset_seconds,
                                 gboolean show_seconds G_GNUC_UNUSED,
                                 gdouble latitude,
                                 gdouble longitude)
{
    SeasonalPeriod period;

    (void)utc_offset_seconds;
    if (!seasonal_period_at(unix_microseconds,
                            latitude,
                            longitude,
                            JAPANESE_DAWN_DEPRESSION,
                            6,
                            6,
                            &period))
    {
        return 3600000;
    }

    return delay_continuous_microseconds_to_milliseconds(
        period.seconds_to_next * G_USEC_PER_SEC);
}


typedef enum
{
    SOLAR_ORIGIN_SUNRISE,
    SOLAR_ORIGIN_SUNSET
} SolarOrigin;

/*
 * Locate the moving origin around an instant. Searching neighbouring UTC dates
 * is necessary because a local solar event can fall on the preceding or
 * following UTC date near the date line. We deliberately stop after two days:
 * when a sunrise/sunset does not occur because of polar day/night, these clock
 * conventions are undefined rather than extrapolated.
 */
static gboolean
solar_origin_window(gint64 unix_microseconds,
                    gdouble latitude,
                    gdouble longitude,
                    SolarOrigin origin,
                    gint64 *previous,
                    gint64 *next)
{
    gint64 best_previous = G_MININT64;
    gint64 best_next = G_MAXINT64;
    gint offset;

    g_return_val_if_fail(previous != NULL, FALSE);

    for (offset = -2; offset <= 2; offset++)
    {
        const gint64 delta = (gint64)offset * MICROSECONDS_PER_DAY;
        gint64 sample;
        gint64 dawn;
        gint64 dusk;
        gint64 candidate;

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

        candidate = origin == SOLAR_ORIGIN_SUNRISE ? dawn : dusk;
        if (candidate <= unix_microseconds && candidate > best_previous)
            best_previous = candidate;
        if (candidate > unix_microseconds && candidate < best_next)
            best_next = candidate;
    }

    if (best_previous == G_MININT64)
        return FALSE;

    *previous = best_previous;
    if (next != NULL)
        *next = best_next;
    return TRUE;
}

static gchar *
format_equal_hours_from_solar_origin(gint64 unix_microseconds,
                                     gboolean show_seconds,
                                     gboolean vertical,
                                     gdouble latitude,
                                     gdouble longitude,
                                     SolarOrigin origin,
                                     const gchar *suffix)
{
    gint64 start;
    gint64 elapsed_microseconds;
    gint64 whole_seconds;
    gint hour;
    gint minute;
    gint second;
    const gchar *separator = vertical ? "\n" : ":";

    if (!solar_origin_window(unix_microseconds,
                             latitude,
                             longitude,
                             origin,
                             &start,
                             NULL))
    {
        return g_strdup_printf(vertical ? "N/A\n%s" : "N/A %s", suffix);
    }

    elapsed_microseconds = unix_microseconds - start;
    whole_seconds = floor_divide(elapsed_microseconds, G_USEC_PER_SEC);
    hour = (gint)(whole_seconds / SECONDS_PER_HOUR);
    minute = (gint)((whole_seconds / SECONDS_PER_MINUTE) %
                    MINUTES_PER_HOUR);
    second = (gint)(whole_seconds % SECONDS_PER_MINUTE);

    if (show_seconds)
    {
        return g_strdup_printf("%02d%s%02d%s%02d%s%s",
                               hour, separator, minute, separator, second,
                               vertical ? "\n" : " ", suffix);
    }

    return g_strdup_printf("%02d%s%02d%s%s",
                           hour, separator, minute,
                           vertical ? "\n" : " ", suffix);
}

static guint
delay_equal_hours_from_solar_origin(gint64 unix_microseconds,
                                    gboolean show_seconds,
                                    gdouble latitude,
                                    gdouble longitude,
                                    SolarOrigin origin)
{
    gint64 start;
    gint64 next;
    gint64 elapsed;
    guint tick_delay;

    if (!solar_origin_window(unix_microseconds,
                             latitude,
                             longitude,
                             origin,
                             &start,
                             &next))
    {
        return 3600000;
    }

    elapsed = unix_microseconds - start;
    tick_delay = delay_for_integer_period(
        elapsed,
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

/*
 * Italian hours (horae ab occasu Solis) are equal hours counted from sunset.
 * Historical local practice sometimes offset the reset from literal sunset;
 * Calendar Plus intentionally uses the unambiguous strict-sunset convention.
 */
gchar *
format_italian_hours_provider(gint64 unix_microseconds,
                              gint utc_offset_seconds,
                              gboolean show_seconds,
                              gboolean vertical,
                              gdouble latitude,
                              gdouble longitude)
{
    (void)utc_offset_seconds;
    return format_equal_hours_from_solar_origin(
        unix_microseconds, show_seconds, vertical, latitude, longitude,
        SOLAR_ORIGIN_SUNSET, "IT");
}

guint
delay_italian_hours_provider(gint64 unix_microseconds,
                             gint utc_offset_seconds,
                             gboolean show_seconds,
                             gdouble latitude,
                             gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_equal_hours_from_solar_origin(
        unix_microseconds, show_seconds, latitude, longitude,
        SOLAR_ORIGIN_SUNSET);
}

/*
 * "Babylonian hours" here uses the historical European gnomonic term for
 * equal hours elapsed from sunrise (horae ab ortu Solis). It is not presented
 * as a reconstruction of ancient Mesopotamian civil timekeeping.
 */
gchar *
format_babylonian_hours_provider(gint64 unix_microseconds,
                                 gint utc_offset_seconds,
                                 gboolean show_seconds,
                                 gboolean vertical,
                                 gdouble latitude,
                                 gdouble longitude)
{
    (void)utc_offset_seconds;
    return format_equal_hours_from_solar_origin(
        unix_microseconds, show_seconds, vertical, latitude, longitude,
        SOLAR_ORIGIN_SUNRISE, "BAB");
}

guint
delay_babylonian_hours_provider(gint64 unix_microseconds,
                                gint utc_offset_seconds,
                                gboolean show_seconds,
                                gdouble latitude,
                                gdouble longitude)
{
    (void)utc_offset_seconds;
    return delay_equal_hours_from_solar_origin(
        unix_microseconds, show_seconds, latitude, longitude,
        SOLAR_ORIGIN_SUNRISE);
}

/*
 * Indian ghaṭī time: sixty ghaṭīs in a mean 24-hour day, with one ghaṭī
 * equal to 24 SI minutes and one vighaṭī to 24 SI seconds. The historical
 * day is presented from computed sunrise; the display resets at the next
 * computed sunrise even when seasonal sunrise drift makes the interval differ
 * slightly from exactly 24 mean hours.
 */
gchar *
format_indian_ghati_provider(gint64 unix_microseconds,
                             gint utc_offset_seconds,
                             gboolean show_seconds G_GNUC_UNUSED,
                             gboolean vertical,
                             gdouble latitude,
                             gdouble longitude)
{
    gint64 start;
    gint64 elapsed_seconds;
    gint ghati;
    gint vighati;

    (void)utc_offset_seconds;
    if (!solar_origin_window(unix_microseconds, latitude, longitude,
                             SOLAR_ORIGIN_SUNRISE, &start, NULL))
    {
        return g_strdup(vertical ? "N/A\nGH" : "N/A GH");
    }

    elapsed_seconds =
        floor_divide(unix_microseconds - start, G_USEC_PER_SEC);
    ghati = (gint)(elapsed_seconds /
                   ((gint64)24 * SECONDS_PER_MINUTE));
    vighati = (gint)((elapsed_seconds / 24) % 60);

    return g_strdup_printf(vertical ? "GH\n%02d:%02d" : "GH %02d:%02d",
                           ghati, vighati);
}

guint
delay_indian_ghati_provider(gint64 unix_microseconds,
                            gint utc_offset_seconds,
                            gboolean show_seconds G_GNUC_UNUSED,
                            gdouble latitude,
                            gdouble longitude)
{
    gint64 start;
    gint64 next;
    guint tick_delay;

    (void)utc_offset_seconds;
    if (!solar_origin_window(unix_microseconds, latitude, longitude,
                             SOLAR_ORIGIN_SUNRISE, &start, &next))
    {
        return 3600000;
    }

    tick_delay = delay_for_integer_period(
        unix_microseconds - start, (gint64)24 * G_USEC_PER_SEC);
    if (next != G_MAXINT64)
    {
        const guint reset_delay =
            delay_continuous_microseconds_to_milliseconds(
                (long double)(next - unix_microseconds));
        return MIN(tick_delay, reset_delay);
    }

    return tick_delay;
}
