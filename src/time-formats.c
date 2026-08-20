// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Native clock-provider registry, formatting and tick scheduling.
 *
 * Each mode is one provider containing metadata plus paired format/delay
 * callbacks. Adding a time system therefore extends one table instead of two
 * central switches. The provider boundary also guarantees that the formula
 * used to display a mode and the formula used to schedule its next update stay
 * together as one implementation unit.
 *
 * Civil-day systems reduce the absolute instant to local microseconds-of-day.
 * Astronomical and seasonal providers use UTC plus the configured geographic
 * location and delegate their continuous calculations to time-astronomy.c.
 */

#include "time-formats.h"
#include "time-astronomy.h"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/timing.h>
#include <math.h>
#include <string.h>

enum
{
    SECONDS_PER_MINUTE = 60,
    MINUTES_PER_HOUR = 60,
    HOURS_PER_DAY = 24,
    SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR,
    SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY,
    DECIMAL_SECONDS_PER_DAY = 100000,
    INTERNET_BEATS_PER_DAY = 1000,
    HEX_TICKS_PER_DAY = 65536
};

#define MICROSECONDS_PER_DAY ((gint64)SECONDS_PER_DAY * G_USEC_PER_SEC)

typedef gchar *(*TimeFormatFunc)(gint64 unix_microseconds,
                                 gint utc_offset_seconds,
                                 gboolean show_seconds,
                                 gboolean vertical,
                                 gdouble latitude,
                                 gdouble longitude);
typedef guint (*TimeDelayFunc)(gint64 unix_microseconds,
                               gint utc_offset_seconds,
                               gboolean show_seconds,
                               gdouble latitude,
                               gdouble longitude);

typedef struct
{
    guint abi_version;
    CalendarPlusTimeMode mode;
    const gchar *id;
    const gchar *settings_name;
    gboolean supports_seconds;
    gboolean requires_longitude;
    gboolean requires_latitude;
    TimeFormatFunc format;
    TimeDelayFunc next_tick;
} TimeProvider;

enum
{
    CALENDAR_PLUS_TIME_PROVIDER_ABI = 1
};

static gint64
floor_divide(gint64 value,
             gint64 divisor)
{
    int64_t quotient = 0;

    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL), 0);
    return quotient;
}

static gint64
positive_modulo(gint64 value,
                gint64 modulus)
{
    int64_t remainder = 0;

    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder), 0);
    return remainder;
}

static gint64
local_microseconds_of_day(gint64 unix_microseconds,
                          gint utc_offset_seconds)
{
    const gint64 instant_phase =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    const gint64 offset_microseconds =
        (gint64)utc_offset_seconds * G_USEC_PER_SEC;
    const gint64 offset_phase =
        positive_modulo(offset_microseconds, MICROSECONDS_PER_DAY);

    /* Both phases are below one day, so their sum cannot overflow gint64. */
    return positive_modulo(instant_phase + offset_phase,
                           MICROSECONDS_PER_DAY);
}

/*
 * Discrete civil-day displays use Common's exact rational partition primitive.
 * No floating-point product or epsilon is involved at a display boundary.
 */
static guint
fractional_day_tick(gint64 microseconds_of_day,
                    guint ticks_per_day)
{
    uint64_t tick = 0;

    g_return_val_if_fail(microseconds_of_day >= 0, 0);
    g_return_val_if_fail(ticks_per_day > 0, 0);
    g_return_val_if_fail(
        infiltratr_cycle_partition_u64((uint64_t)microseconds_of_day,
                                       (uint64_t)MICROSECONDS_PER_DAY,
                                       (uint64_t)ticks_per_day,
                                       &tick,
                                       NULL),
        0);

    /* The exact partition index is strictly below the guint partition count. */
    return (guint)tick;
}

static void
split_clock_seconds(gint64 whole_seconds,
                    gint *hour,
                    gint *minute,
                    gint *second)
{
    const gint64 normalised =
        positive_modulo(whole_seconds, SECONDS_PER_DAY);

    *hour = (gint)(normalised / SECONDS_PER_HOUR);
    *minute = (gint)((normalised / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR);
    *second = (gint)(normalised % SECONDS_PER_MINUTE);
}

static gchar *
format_clock_fields(gint hour,
                    gint minute,
                    gint second,
                    gboolean show_seconds,
                    gboolean vertical,
                    const gchar *suffix)
{
    const gchar *separator = vertical ? "\n" : ":";
    const gchar *suffix_separator = vertical ? "\n" : " ";

    if (show_seconds)
    {
        return g_strdup_printf("%02d%s%02d%s%02d%s%s",
                               hour, separator, minute, separator, second,
                               suffix_separator, suffix);
    }

    return g_strdup_printf("%02d%s%02d%s%s",
                           hour, separator, minute, suffix_separator, suffix);
}

static guint
delay_seconds_to_milliseconds(long double seconds)
{
    uint64_t milliseconds = 0;

    if (!infiltratr_seconds_to_milliseconds_ceil(seconds, &milliseconds))
        return 1;
    return milliseconds > G_MAXUINT ? G_MAXUINT : (guint)milliseconds;
}

static guint
delay_continuous_microseconds_to_milliseconds(long double microseconds)
{
    return delay_seconds_to_milliseconds(
        microseconds / (long double)G_USEC_PER_SEC);
}

static guint
delay_exact_microseconds_to_milliseconds(uint64_t microseconds)
{
    uint64_t milliseconds = 0;

    if (!infiltratr_microseconds_to_milliseconds_ceil(microseconds,
                                                      &milliseconds))
    {
        return 1;
    }
    return milliseconds > G_MAXUINT ? G_MAXUINT : (guint)milliseconds;
}

/*
 * Integral periodic clocks use exact Euclidean phase arithmetic. Exact
 * boundaries return one full period, so callers never need an epsilon repair.
 */
static guint
delay_for_integer_period(gint64 position_microseconds,
                         gint64 period_microseconds)
{
    uint64_t remaining = 0;

    if (!infiltratr_i64_period_remaining(position_microseconds,
                                         period_microseconds,
                                         &remaining))
    {
        return 1;
    }
    return delay_exact_microseconds_to_milliseconds(remaining);
}

static guint
delay_for_day_ticks(gint64 microseconds_of_day,
                    guint ticks_per_day)
{
    uint64_t remaining = 0;

    if (microseconds_of_day < 0 || ticks_per_day == 0 ||
        !infiltratr_cycle_partition_u64((uint64_t)microseconds_of_day,
                                        (uint64_t)MICROSECONDS_PER_DAY,
                                        (uint64_t)ticks_per_day,
                                        NULL,
                                        &remaining))
    {
        return 1;
    }
    return delay_exact_microseconds_to_milliseconds(remaining);
}

static guint
delay_for_clock_seconds(long double clock_seconds,
                        long double clock_rate,
                        gboolean show_seconds)
{
    const long double unit = show_seconds ? 1.0L : 60.0L;
    long double remaining = 0.0L;

    if (!isfinite(clock_rate) || clock_rate <= 0.0L ||
        !infiltratr_period_remaining(clock_seconds, unit, &remaining))
    {
        return 1;
    }

    return delay_seconds_to_milliseconds(remaining / clock_rate);
}

static gchar *
format_decimal_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gboolean vertical,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    const gint64 local =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds);
    const guint ticks_per_day = show_seconds ? DECIMAL_SECONDS_PER_DAY : 1000;
    const guint ticks = fractional_day_tick(local, ticks_per_day);
    const gchar *separator = vertical ? "\n" : ":";

    (void)longitude;
    if (show_seconds)
    {
        return g_strdup_printf("%u%s%02u%s%02u",
                               ticks / 10000, separator,
                               (ticks / 100) % 100, separator, ticks % 100);
    }

    return g_strdup_printf("%u%s%02u", ticks / 100, separator, ticks % 100);
}

static guint
delay_decimal_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        show_seconds ? DECIMAL_SECONDS_PER_DAY : 1000);
}

/* @000 is midnight at UTC+01:00; the host's civil timezone is not involved. */
static gint64
internet_microseconds(gint64 unix_microseconds)
{
    const gint64 instant_phase =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);

    return positive_modulo(
        instant_phase + (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC,
        MICROSECONDS_PER_DAY);
}

static gchar *
format_internet_provider(gint64 unix_microseconds,
                         gint utc_offset_seconds,
                         gboolean show_seconds,
                         gboolean vertical,
                         gdouble latitude G_GNUC_UNUSED,
                         gdouble longitude)
{
    const guint ticks_per_day =
        show_seconds ? INTERNET_BEATS_PER_DAY * 100 : INTERNET_BEATS_PER_DAY;
    const guint ticks =
        fractional_day_tick(internet_microseconds(unix_microseconds),
                            ticks_per_day);

    (void)utc_offset_seconds;
    (void)vertical;
    (void)longitude;
    return show_seconds ?
        g_strdup_printf("@%03u.%02u", ticks / 100, ticks % 100) :
        g_strdup_printf("@%03u", ticks);
}

static guint
delay_internet_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        internet_microseconds(unix_microseconds),
        show_seconds ? INTERNET_BEATS_PER_DAY * 100 : INTERNET_BEATS_PER_DAY);
}

static gchar *
format_unix_provider(gint64 unix_microseconds,
                     gint utc_offset_seconds,
                     gboolean show_seconds,
                     gboolean vertical,
                     gdouble latitude G_GNUC_UNUSED,
                     gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)vertical;
    (void)longitude;
    return g_strdup_printf("%" G_GINT64_FORMAT,
                           floor_divide(unix_microseconds, G_USEC_PER_SEC));
}

static guint
delay_unix_provider(gint64 unix_microseconds,
                    gint utc_offset_seconds,
                    gboolean show_seconds,
                    gdouble latitude G_GNUC_UNUSED,
                    gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)longitude;
    return delay_for_integer_period(unix_microseconds, G_USEC_PER_SEC);
}

static gchar *
format_hexadecimal_provider(gint64 unix_microseconds,
                            gint utc_offset_seconds,
                            gboolean show_seconds,
                            gboolean vertical,
                            gdouble latitude G_GNUC_UNUSED,
                            gdouble longitude)
{
    (void)show_seconds;
    (void)vertical;
    (void)longitude;
    return g_strdup_printf(
        "%04X",
        fractional_day_tick(
            local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
            HEX_TICKS_PER_DAY));
}

static guint
delay_hexadecimal_provider(gint64 unix_microseconds,
                           gint utc_offset_seconds,
                           gboolean show_seconds,
                           gdouble latitude G_GNUC_UNUSED,
                           gdouble longitude)
{
    (void)show_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        HEX_TICKS_PER_DAY);
}

static void
write_binary(gchar *destination,
             guint value,
             guint bits)
{
    guint bit;

    for (bit = 0; bit < bits; bit++)
    {
        const guint shift = bits - bit - 1;
        destination[bit] = (value & (1U << shift)) ? '1' : '0';
    }
    destination[bits] = '\0';
}

static gchar *
format_binary_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gboolean vertical,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    gint hour;
    gint minute;
    gint second;
    gchar hour_bits[6];
    gchar minute_bits[7];
    gchar second_bits[7];
    const gchar *separator = vertical ? "\n" : ":";
    const gint64 local =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds);

    (void)longitude;
    split_clock_seconds(local / G_USEC_PER_SEC, &hour, &minute, &second);
    write_binary(hour_bits, (guint)hour, 5);
    write_binary(minute_bits, (guint)minute, 6);
    write_binary(second_bits, (guint)second, 6);

    return show_seconds ?
        g_strdup_printf("%s%s%s%s%s", hour_bits, separator, minute_bits,
                        separator, second_bits) :
        g_strdup_printf("%s%s%s", hour_bits, separator, minute_bits);
}

static guint
delay_binary_provider(gint64 unix_microseconds,
                      gint utc_offset_seconds,
                      gboolean show_seconds,
                      gdouble latitude G_GNUC_UNUSED,
                      gdouble longitude)
{
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        show_seconds ? SECONDS_PER_DAY : MINUTES_PER_HOUR * HOURS_PER_DAY);
}

static gchar *
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

static guint
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

static gchar *
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

static guint
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

static gchar *
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

static guint
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

static gchar *
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

static guint
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

static gchar *
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

static guint
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

static const gchar *const chinese_branches[] = {
    "子 Zǐ (Rat)",
    "丑 Chǒu (Ox)",
    "寅 Yín (Tiger)",
    "卯 Mǎo (Rabbit)",
    "辰 Chén (Dragon)",
    "巳 Sì (Snake)",
    "午 Wǔ (Horse)",
    "未 Wèi (Goat)",
    "申 Shēn (Monkey)",
    "酉 Yǒu (Rooster)",
    "戌 Xū (Dog)",
    "亥 Hài (Boar)"
};

static gchar *
format_chinese_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds G_GNUC_UNUSED,
                        gboolean vertical,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    const gint64 shifted = positive_modulo(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds) +
        (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC,
        MICROSECONDS_PER_DAY);
    const guint branch = (guint)(shifted /
        ((gint64)2 * SECONDS_PER_HOUR * G_USEC_PER_SEC));

    (void)longitude;
    if (!vertical)
        return g_strdup(chinese_branches[branch]);

    /* Keep narrow panels readable without inventing a finer historical unit. */
    {
        g_auto(GStrv) fields = g_strsplit(chinese_branches[branch], " ", 3);
        return g_strdup_printf("%s\n%s\n%s", fields[0], fields[1], fields[2]);
    }
}

static guint
delay_chinese_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds G_GNUC_UNUSED,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    const gint64 shifted =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds) +
        (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC;

    (void)longitude;
    return delay_for_integer_period(
        shifted,
        (gint64)2 * SECONDS_PER_HOUR * G_USEC_PER_SEC);
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

static gchar *
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

static guint
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

static gchar *
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

static guint
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

#define TIME_PROVIDER(mode_, token_, id_, label_, seconds_, longitude_, latitude_) \
    [CALENDAR_PLUS_TIME_MODE_##mode_] = { \
        CALENDAR_PLUS_TIME_PROVIDER_ABI, \
        CALENDAR_PLUS_TIME_MODE_##mode_, id_, label_, seconds_, longitude_, \
        latitude_, format_##token_##_provider, delay_##token_##_provider \
    }

/*
 * Stable time-provider registry. Existing enum values remain fixed because the
 * enum is part of the public C API; new modes are therefore appended. Provider
 * metadata remains the single source for settings generation and capability
 * discovery.
 */
static const TimeProvider time_providers[] = {
    TIME_PROVIDER(DECIMAL, decimal, "decimal",
                  "Decimal time (10-hour day)", TRUE, FALSE, FALSE),
    TIME_PROVIDER(INTERNET, internet, "internet",
                  "Internet Time (@000 to @999)", TRUE, FALSE, FALSE),
    TIME_PROVIDER(UNIX, unix, "unix",
                  "Unix time (epoch seconds)", FALSE, FALSE, FALSE),
    TIME_PROVIDER(HEXADECIMAL, hexadecimal, "hexadecimal",
                  "Hexadecimal time (0000 to FFFF)", FALSE, FALSE, FALSE),
    TIME_PROVIDER(BINARY, binary, "binary",
                  "Binary clock", TRUE, FALSE, FALSE),
    TIME_PROVIDER(SIDEREAL, sidereal, "sidereal",
                  "Local sidereal time", TRUE, TRUE, FALSE),
    TIME_PROVIDER(SOLAR, solar, "solar",
                  "Apparent solar time", TRUE, TRUE, FALSE),
    TIME_PROVIDER(JULIAN, julian, "julian",
                  "Julian Date", TRUE, FALSE, FALSE),
    TIME_PROVIDER(MEAN_SOLAR, mean_solar, "mean-solar",
                  "Local mean solar time", TRUE, TRUE, FALSE),
    TIME_PROVIDER(MODIFIED_JULIAN, modified_julian, "modified-julian",
                  "Modified Julian Date", TRUE, FALSE, FALSE),
    TIME_PROVIDER(CHINESE, chinese, "chinese-time",
                  "Traditional Chinese double-hours", FALSE, FALSE, FALSE),
    TIME_PROVIDER(ROMAN_TEMPORAL, roman_temporal, "roman-temporal",
                  "Roman temporal time", FALSE, TRUE, TRUE),
    TIME_PROVIDER(JAPANESE_TEMPORAL, japanese_temporal, "japanese-temporal",
                  "Edo Japanese seasonal time", FALSE, TRUE, TRUE)
};

G_STATIC_ASSERT(G_N_ELEMENTS(time_providers) ==
                CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL + 1);

/*
 * Latitude-dependent historical modes keep the published C/GI ABI unchanged.
 * The Cinnamon adapter appends "@<latitude>" to the internal mode token. The
 * parser packs that latitude into otherwise-unused high enum bits, allowing
 * the existing CalendarPlusClockConfig layout and SystemClock.start() method
 * to carry location through the neutral engine without enlarging a published
 * structure or adding a second GI method. Ordinary callers continue to use
 * the documented stable mode IDs and receive equatorial seasonal time.
 */
#define LOCATION_MODE_TAG ((guint)0x40000000U)
#define LOCATION_MODE_BASE_MASK ((guint)0x000000ffU)
#define LOCATION_LATITUDE_SCALE 10000.0
#define LOCATION_LATITUDE_BIAS 90.0

static CalendarPlusTimeMode
encode_location_mode(CalendarPlusTimeMode mode,
                     gdouble latitude)
{
    const gdouble safe_latitude = isfinite(latitude) ?
        infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const guint latitude_code = (guint)llround(
        (safe_latitude + LOCATION_LATITUDE_BIAS) * LOCATION_LATITUDE_SCALE);
    const guint packed = LOCATION_MODE_TAG |
        (latitude_code << 8) | ((guint)mode & LOCATION_MODE_BASE_MASK);

    /*
     * The high bits are an internal transport encoding, not a new published
     * enum member. decode_location_mode() removes them before provider lookup.
     */
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return (CalendarPlusTimeMode)packed;
}

static CalendarPlusTimeMode
decode_location_mode(CalendarPlusTimeMode mode,
                     gdouble *latitude)
{
    const guint packed = (guint)mode;

    if ((packed & LOCATION_MODE_TAG) != 0U)
    {
        const guint latitude_code =
            (packed & ~LOCATION_MODE_TAG) >> 8;

        if (latitude != NULL)
        {
            *latitude = latitude_code / LOCATION_LATITUDE_SCALE -
                        LOCATION_LATITUDE_BIAS;
        }
        return (CalendarPlusTimeMode)(packed & LOCATION_MODE_BASE_MASK);
    }

    if (latitude != NULL)
        *latitude = 0.0;
    return mode;
}

static const TimeProvider *
time_provider_for_mode(CalendarPlusTimeMode mode)
{
    const CalendarPlusTimeMode base_mode = decode_location_mode(mode, NULL);
    const TimeProvider *provider;

    if (base_mode < CALENDAR_PLUS_TIME_MODE_DECIMAL ||
        base_mode > CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL)
    {
        return NULL;
    }

    provider = &time_providers[base_mode];
    return provider->abi_version == CALENDAR_PLUS_TIME_PROVIDER_ABI ?
           provider : NULL;
}

CalendarPlusTimeMode
calendar_plus_time_mode_from_string(const gchar *mode)
{
    gsize index;

    if (mode == NULL)
        return CALENDAR_PLUS_TIME_MODE_INVALID;

    for (index = 0; index < G_N_ELEMENTS(time_providers); index++)
    {
        const TimeProvider *provider = &time_providers[index];

        if (infiltratr_string_equal(mode, provider->id))
            return provider->mode;

        if (provider->requires_latitude)
        {
            const gsize id_length = strlen(provider->id);

            if (infiltratr_string_starts_with(mode, provider->id) &&
                mode[id_length] == '@')
            {
                gdouble latitude = 0.0;

                if (infiltratr_parse_double(
                        mode + id_length + 1, &latitude))
                {
                    return encode_location_mode(provider->mode, latitude);
                }
            }
        }
    }

    return CALENDAR_PLUS_TIME_MODE_INVALID;
}

const gchar *
calendar_plus_time_mode_get_id(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL ? provider->id : NULL;
}

gsize
calendar_plus_time_mode_get_count(void)
{
    return G_N_ELEMENTS(time_providers) - 1;
}

CalendarPlusTimeMode
calendar_plus_time_mode_get_at(gsize index)
{
    return index < calendar_plus_time_mode_get_count() ?
           time_providers[index + 1].mode : CALENDAR_PLUS_TIME_MODE_INVALID;
}

const gchar *
calendar_plus_time_mode_get_name(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL ? provider->settings_name : NULL;
}

gboolean
calendar_plus_time_mode_supports_seconds(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL && provider->supports_seconds;
}

gboolean
calendar_plus_time_mode_requires_longitude(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL && provider->requires_longitude;
}

gchar *
calendar_plus_format_time(CalendarPlusTimeMode mode,
                          gint64 unix_microseconds,
                          gint utc_offset_seconds,
                          gboolean show_seconds,
                          gboolean vertical,
                          gdouble longitude)
{
    gdouble latitude;
    const CalendarPlusTimeMode base_mode = decode_location_mode(mode, &latitude);
    const TimeProvider *provider = time_provider_for_mode(base_mode);

    if (provider == NULL)
        return g_strdup("");

    return provider->format(unix_microseconds,
                            utc_offset_seconds,
                            show_seconds,
                            vertical,
                            latitude,
                            longitude);
}

guint
calendar_plus_time_delay_to_next_tick(CalendarPlusTimeMode mode,
                                      gint64 unix_microseconds,
                                      gint utc_offset_seconds,
                                      gboolean show_seconds,
                                      gdouble longitude)
{
    gdouble latitude;
    const CalendarPlusTimeMode base_mode = decode_location_mode(mode, &latitude);
    const TimeProvider *provider = time_provider_for_mode(base_mode);

    if (provider == NULL)
        return 1000;

    return provider->next_tick(unix_microseconds,
                               utc_offset_seconds,
                               show_seconds,
                               latitude,
                               longitude);
}

gchar *
calendar_plus_replace_time(const gchar *label,
                           const gchar *conventional_time,
                           const gchar *replacement_time)
{
    const gchar *match;
    gsize prefix_length;

    g_return_val_if_fail(label != NULL, NULL);
    g_return_val_if_fail(replacement_time != NULL, NULL);

    if (conventional_time == NULL || conventional_time[0] == '\0')
        return NULL;

    match = g_strstr_len(label, -1, conventional_time);
    if (match == NULL)
        return NULL;

    prefix_length = (gsize)(match - label);
    return g_strdup_printf("%.*s%s%s",
                           (gint)prefix_length,
                           label,
                           replacement_time,
                           match + strlen(conventional_time));
}
