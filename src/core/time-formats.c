// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Native clock-provider registry, shared timing primitives and public dispatch.
 * Each mode owns metadata plus paired format/delay callbacks; specialised
 * implementations live in the civil, astronomy and historical modules.
 */

#include "time-formats.h"
#include "time-formats-internal.h"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/timing.h>
#include <math.h>
#include <string.h>

typedef gchar *(*TimeFormatFunc)(gint64, gint, gboolean, gboolean, gdouble, gdouble);
typedef guint (*TimeDelayFunc)(gint64, gint, gboolean, gdouble, gdouble);

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

enum { CALENDAR_PLUS_TIME_PROVIDER_ABI = 1 };

gint64
calendar_plus_time_floor_divide(gint64 value, gint64 divisor)
{
    int64_t quotient = 0;
    g_return_val_if_fail(infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL), 0);
    return quotient;
}

gint64
calendar_plus_time_positive_modulo(gint64 value, gint64 modulus)
{
    int64_t remainder = 0;
    g_return_val_if_fail(infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder), 0);
    return remainder;
}

gint64
calendar_plus_time_local_microseconds_of_day(gint64 unix_microseconds, gint utc_offset_seconds)
{
    const gint64 instant_phase = positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    const gint64 offset_microseconds = (gint64)utc_offset_seconds * G_USEC_PER_SEC;
    const gint64 offset_phase = positive_modulo(offset_microseconds, MICROSECONDS_PER_DAY);
    return positive_modulo(instant_phase + offset_phase, MICROSECONDS_PER_DAY);
}

guint
calendar_plus_time_fractional_day_tick(gint64 microseconds_of_day, guint ticks_per_day)
{
    uint64_t tick = 0;
    g_return_val_if_fail(microseconds_of_day >= 0, 0);
    g_return_val_if_fail(ticks_per_day > 0, 0);
    g_return_val_if_fail(infiltratr_cycle_partition_u64((uint64_t)microseconds_of_day,
                                                        (uint64_t)MICROSECONDS_PER_DAY,
                                                        (uint64_t)ticks_per_day,
                                                        &tick, NULL), 0);
    return (guint)tick;
}

void
calendar_plus_time_split_clock_seconds(gint64 whole_seconds, gint *hour, gint *minute, gint *second)
{
    const gint64 normalised = positive_modulo(whole_seconds, SECONDS_PER_DAY);
    *hour = (gint)(normalised / SECONDS_PER_HOUR);
    *minute = (gint)((normalised / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR);
    *second = (gint)(normalised % SECONDS_PER_MINUTE);
}

gchar *
calendar_plus_time_format_clock_fields(gint hour, gint minute, gint second,
                                       gboolean show_seconds, gboolean vertical,
                                       const gchar *suffix)
{
    const gchar *separator = vertical ? "\n" : ":";
    const gchar *suffix_separator = vertical ? "\n" : " ";
    if (show_seconds)
        return g_strdup_printf("%02d%s%02d%s%02d%s%s", hour, separator, minute,
                               separator, second, suffix_separator, suffix);
    return g_strdup_printf("%02d%s%02d%s%s", hour, separator, minute,
                           suffix_separator, suffix);
}

static guint
delay_seconds_to_milliseconds(long double seconds)
{
    uint64_t milliseconds = 0;
    if (!infiltratr_seconds_to_milliseconds_ceil(seconds, &milliseconds))
        return 1;
    return milliseconds > G_MAXUINT ? G_MAXUINT : (guint)milliseconds;
}

guint
calendar_plus_time_delay_continuous_microseconds_to_milliseconds(long double microseconds)
{
    return delay_seconds_to_milliseconds(microseconds / (long double)G_USEC_PER_SEC);
}

static guint
delay_exact_microseconds_to_milliseconds(uint64_t microseconds)
{
    uint64_t milliseconds = 0;
    if (!infiltratr_microseconds_to_milliseconds_ceil(microseconds, &milliseconds))
        return 1;
    return milliseconds > G_MAXUINT ? G_MAXUINT : (guint)milliseconds;
}

guint
calendar_plus_time_delay_for_integer_period(gint64 position_microseconds, gint64 period_microseconds)
{
    uint64_t remaining = 0;
    if (!infiltratr_i64_period_remaining(position_microseconds, period_microseconds, &remaining))
        return 1;
    return delay_exact_microseconds_to_milliseconds(remaining);
}

guint
calendar_plus_time_delay_for_day_ticks(gint64 microseconds_of_day, guint ticks_per_day)
{
    uint64_t remaining = 0;
    if (microseconds_of_day < 0 || ticks_per_day == 0 ||
        !infiltratr_cycle_partition_u64((uint64_t)microseconds_of_day,
                                        (uint64_t)MICROSECONDS_PER_DAY,
                                        (uint64_t)ticks_per_day, NULL, &remaining))
        return 1;
    return delay_exact_microseconds_to_milliseconds(remaining);
}

guint
calendar_plus_time_delay_for_clock_seconds(long double clock_seconds,
                                           long double clock_rate,
                                           gboolean show_seconds)
{
    const long double unit = show_seconds ? 1.0L : 60.0L;
    long double remaining = 0.0L;
    if (!isfinite(clock_rate) || clock_rate <= 0.0L ||
        !infiltratr_period_remaining(clock_seconds, unit, &remaining))
        return 1;
    return delay_seconds_to_milliseconds(remaining / clock_rate);
}

#define TIME_PROVIDER(mode_, token_, id_, label_, seconds_, longitude_, latitude_) \
    [CALENDAR_PLUS_TIME_MODE_##mode_] = { CALENDAR_PLUS_TIME_PROVIDER_ABI, \
        CALENDAR_PLUS_TIME_MODE_##mode_, id_, label_, seconds_, longitude_, latitude_, \
        format_##token_##_provider, delay_##token_##_provider }

static const TimeProvider time_providers[] = {
    TIME_PROVIDER(DECIMAL, decimal, "decimal", "French Republican decimal time (10-hour day)", TRUE, FALSE, FALSE),
    TIME_PROVIDER(INTERNET, internet, "internet", "Internet Time (@000 to @999)", TRUE, FALSE, FALSE),
    TIME_PROVIDER(UNIX, unix, "unix", "Unix time (epoch seconds)", FALSE, FALSE, FALSE),
    TIME_PROVIDER(HEXADECIMAL, hexadecimal, "hexadecimal", "Hexadecimal time (0000 to FFFF)", FALSE, FALSE, FALSE),
    TIME_PROVIDER(BINARY, binary, "binary", "Binary clock", TRUE, FALSE, FALSE),
    TIME_PROVIDER(SIDEREAL, sidereal, "sidereal", "Local sidereal time", TRUE, TRUE, FALSE),
    TIME_PROVIDER(SOLAR, solar, "solar", "Apparent solar time", TRUE, TRUE, FALSE),
    TIME_PROVIDER(JULIAN, julian, "julian", "Astronomical Julian Date (JD)", TRUE, FALSE, FALSE),
    TIME_PROVIDER(MEAN_SOLAR, mean_solar, "mean-solar", "Local mean solar time", TRUE, TRUE, FALSE),
    TIME_PROVIDER(MODIFIED_JULIAN, modified_julian, "modified-julian", "Modified Julian Date (MJD)", TRUE, FALSE, FALSE),
    TIME_PROVIDER(CHINESE, chinese, "chinese-time", "Traditional Chinese double-hours", FALSE, FALSE, FALSE),
    TIME_PROVIDER(ROMAN_TEMPORAL, roman_temporal, "roman-temporal", "Roman temporal time", FALSE, TRUE, TRUE),
    TIME_PROVIDER(JAPANESE_TEMPORAL, japanese_temporal, "japanese-temporal", "Edo Japanese seasonal time", FALSE, TRUE, TRUE),
    TIME_PROVIDER(ITALIAN_HOURS, italian_hours, "italian-hours", "Italian hours (from sunset)", TRUE, TRUE, TRUE),
    TIME_PROVIDER(BABYLONIAN_HOURS, babylonian_hours, "babylonian-hours", "Babylonian hours (from sunrise)", TRUE, TRUE, TRUE),
    TIME_PROVIDER(INDIAN_GHATI, indian_ghati, "indian-ghati", "Indian ghaṭī time (from sunrise)", FALSE, TRUE, TRUE),
    TIME_PROVIDER(CHINESE_KE, chinese_ke, "chinese-ke", "Chinese hundred-kè time", FALSE, FALSE, FALSE),
    TIME_PROVIDER(NUREMBERG_HOURS, nuremberg_hours, "nuremberg-hours", "Nuremberg hours (sunrise/sunset reset)", TRUE, TRUE, TRUE)
};

G_STATIC_ASSERT(G_N_ELEMENTS(time_providers) == CALENDAR_PLUS_TIME_MODE_NUREMBERG_HOURS + 1);

static const TimeProvider *
time_provider_for_mode(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider;
    if (mode < CALENDAR_PLUS_TIME_MODE_DECIMAL || mode > CALENDAR_PLUS_TIME_MODE_NUREMBERG_HOURS)
        return NULL;
    provider = &time_providers[mode];
    return provider->abi_version == CALENDAR_PLUS_TIME_PROVIDER_ABI ? provider : NULL;
}

CalendarPlusTimeMode
calendar_plus_time_mode_from_string(const gchar *mode)
{
    gsize index;
    if (mode == NULL)
        return CALENDAR_PLUS_TIME_MODE_INVALID;
    for (index = 0; index < G_N_ELEMENTS(time_providers); index++)
        if (infiltratr_string_equal(mode, time_providers[index].id))
            return time_providers[index].mode;
    return CALENDAR_PLUS_TIME_MODE_INVALID;
}

const gchar *calendar_plus_time_mode_get_id(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL ? provider->id : NULL;
}

gsize calendar_plus_time_mode_get_count(void) { return G_N_ELEMENTS(time_providers) - 1; }

CalendarPlusTimeMode calendar_plus_time_mode_get_at(gsize index)
{
    return index < calendar_plus_time_mode_get_count() ? time_providers[index + 1].mode : CALENDAR_PLUS_TIME_MODE_INVALID;
}

const gchar *calendar_plus_time_mode_get_name(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL ? provider->settings_name : NULL;
}

gboolean calendar_plus_time_mode_supports_seconds(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL && provider->supports_seconds;
}

gboolean calendar_plus_time_mode_requires_longitude(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL && provider->requires_longitude;
}

gboolean calendar_plus_time_mode_requires_latitude(CalendarPlusTimeMode mode)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    return provider != NULL && provider->requires_latitude;
}

gchar *
calendar_plus_format_time_at_location(CalendarPlusTimeMode mode, gint64 unix_microseconds,
                                      gint utc_offset_seconds, gboolean show_seconds,
                                      gboolean vertical, gdouble latitude, gdouble longitude)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    if (provider == NULL)
        return g_strdup("");
    return provider->format(unix_microseconds, utc_offset_seconds, show_seconds,
                            vertical, safe_latitude, safe_longitude);
}

gchar *
calendar_plus_format_time(CalendarPlusTimeMode mode, gint64 unix_microseconds,
                          gint utc_offset_seconds, gboolean show_seconds,
                          gboolean vertical, gdouble longitude)
{
    return calendar_plus_format_time_at_location(mode, unix_microseconds,
        utc_offset_seconds, show_seconds, vertical, 0.0, longitude);
}

guint
calendar_plus_time_delay_to_next_tick_at_location(CalendarPlusTimeMode mode,
    gint64 unix_microseconds, gint utc_offset_seconds, gboolean show_seconds,
    gdouble latitude, gdouble longitude)
{
    const TimeProvider *provider = time_provider_for_mode(mode);
    const gdouble safe_latitude = isfinite(latitude) ? infiltratr_clamp_double(latitude, -90.0, 90.0) : 0.0;
    const gdouble safe_longitude = isfinite(longitude) ? infiltratr_clamp_double(longitude, -180.0, 180.0) : 0.0;
    if (provider == NULL)
        return 1000;
    return provider->next_tick(unix_microseconds, utc_offset_seconds, show_seconds,
                               safe_latitude, safe_longitude);
}

guint
calendar_plus_time_delay_to_next_tick(CalendarPlusTimeMode mode, gint64 unix_microseconds,
                                      gint utc_offset_seconds, gboolean show_seconds,
                                      gdouble longitude)
{
    return calendar_plus_time_delay_to_next_tick_at_location(mode, unix_microseconds,
        utc_offset_seconds, show_seconds, 0.0, longitude);
}

gchar *
calendar_plus_replace_time(const gchar *label, const gchar *conventional_time,
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
    return g_strdup_printf("%.*s%s%s", (gint)prefix_length, label,
                           replacement_time, match + strlen(conventional_time));
}
