// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_TIME_FORMATS_INTERNAL_H
#define CALENDAR_PLUS_TIME_FORMATS_INTERNAL_H

#include "time-formats.h"

G_BEGIN_DECLS

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

gint64 calendar_plus_time_floor_divide(gint64 value, gint64 divisor);
gint64 calendar_plus_time_positive_modulo(gint64 value, gint64 modulus);
gint64 calendar_plus_time_local_microseconds_of_day(gint64 unix_microseconds, gint utc_offset_seconds);
guint calendar_plus_time_fractional_day_tick(gint64 microseconds_of_day, guint ticks_per_day);
void calendar_plus_time_split_clock_seconds(gint64 whole_seconds, gint *hour, gint *minute, gint *second);
gchar *calendar_plus_time_format_clock_fields(gint hour, gint minute, gint second, gboolean show_seconds, gboolean vertical, const gchar *suffix);
guint calendar_plus_time_delay_continuous_microseconds_to_milliseconds(long double microseconds);
guint calendar_plus_time_delay_for_integer_period(gint64 position_microseconds, gint64 period_microseconds);
guint calendar_plus_time_delay_for_day_ticks(gint64 microseconds_of_day, guint ticks_per_day);
guint calendar_plus_time_delay_for_clock_seconds(long double clock_seconds, long double clock_rate, gboolean show_seconds);

#define floor_divide calendar_plus_time_floor_divide
#define positive_modulo calendar_plus_time_positive_modulo
#define local_microseconds_of_day calendar_plus_time_local_microseconds_of_day
#define fractional_day_tick calendar_plus_time_fractional_day_tick
#define split_clock_seconds calendar_plus_time_split_clock_seconds
#define format_clock_fields calendar_plus_time_format_clock_fields
#define delay_continuous_microseconds_to_milliseconds calendar_plus_time_delay_continuous_microseconds_to_milliseconds
#define delay_for_integer_period calendar_plus_time_delay_for_integer_period
#define delay_for_day_ticks calendar_plus_time_delay_for_day_ticks
#define delay_for_clock_seconds calendar_plus_time_delay_for_clock_seconds

gchar *format_decimal_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_decimal_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_internet_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_internet_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_unix_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_unix_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_hexadecimal_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_hexadecimal_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_binary_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_binary_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_sidereal_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_sidereal_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_solar_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_solar_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_julian_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_julian_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_mean_solar_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_mean_solar_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_modified_julian_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_modified_julian_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_chinese_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_chinese_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_roman_temporal_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_roman_temporal_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_japanese_temporal_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_japanese_temporal_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_italian_hours_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_italian_hours_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_babylonian_hours_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_babylonian_hours_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_indian_ghati_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_indian_ghati_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_chinese_ke_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_chinese_ke_provider(gint64, gint, gboolean, gdouble, gdouble);
gchar *format_nuremberg_hours_provider(gint64, gint, gboolean, gboolean, gdouble, gdouble);
guint delay_nuremberg_hours_provider(gint64, gint, gboolean, gdouble, gdouble);

G_END_DECLS

#endif
