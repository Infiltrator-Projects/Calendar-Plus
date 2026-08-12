// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_TIME_FORMATS_H
#define CALENDAR_PLUS_TIME_FORMATS_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * CalendarPlusTimeMode:
 * @CALENDAR_PLUS_TIME_MODE_INVALID: unknown or conventional mode
 * @CALENDAR_PLUS_TIME_MODE_DECIMAL: local ten-hour decimal day
 * @CALENDAR_PLUS_TIME_MODE_INTERNET: Swatch-style Internet Time
 * @CALENDAR_PLUS_TIME_MODE_UNIX: seconds since the Unix epoch
 * @CALENDAR_PLUS_TIME_MODE_HEXADECIMAL: local day divided into 65536 ticks
 * @CALENDAR_PLUS_TIME_MODE_BINARY: conventional local fields in binary
 * @CALENDAR_PLUS_TIME_MODE_SIDEREAL: local mean sidereal time
 * @CALENDAR_PLUS_TIME_MODE_SOLAR: local apparent solar time
 * @CALENDAR_PLUS_TIME_MODE_JULIAN: astronomical Julian Date
 * @CALENDAR_PLUS_TIME_MODE_MEAN_SOLAR: local mean solar time
 * @CALENDAR_PLUS_TIME_MODE_MODIFIED_JULIAN: Modified Julian Date
 * @CALENDAR_PLUS_TIME_MODE_CHINESE: traditional Chinese double-hours
 * @CALENDAR_PLUS_TIME_MODE_ROMAN_TEMPORAL: Roman seasonal daylight hours and night watches
 * @CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL: Edo Japanese seasonal time
 *
 * Native time systems supported by Calendar Plus. Conventional 12-hour and
 * 24-hour modes remain with CinnamonDesktop.WallClock for locale handling.
 */
typedef enum
{
    CALENDAR_PLUS_TIME_MODE_INVALID = 0,
    CALENDAR_PLUS_TIME_MODE_DECIMAL,
    CALENDAR_PLUS_TIME_MODE_INTERNET,
    CALENDAR_PLUS_TIME_MODE_UNIX,
    CALENDAR_PLUS_TIME_MODE_HEXADECIMAL,
    CALENDAR_PLUS_TIME_MODE_BINARY,
    CALENDAR_PLUS_TIME_MODE_SIDEREAL,
    CALENDAR_PLUS_TIME_MODE_SOLAR,
    CALENDAR_PLUS_TIME_MODE_JULIAN,
    CALENDAR_PLUS_TIME_MODE_MEAN_SOLAR,
    CALENDAR_PLUS_TIME_MODE_MODIFIED_JULIAN,
    CALENDAR_PLUS_TIME_MODE_CHINESE,
    CALENDAR_PLUS_TIME_MODE_ROMAN_TEMPORAL,
    CALENDAR_PLUS_TIME_MODE_JAPANESE_TEMPORAL
} CalendarPlusTimeMode;

/**
 * calendar_plus_time_mode_from_string:
 * @mode: a settings-schema clock-mode value
 *
 * Converts a stable settings value to its native enum.
 *
 * Returns: the matching mode, or %CALENDAR_PLUS_TIME_MODE_INVALID
 */
CalendarPlusTimeMode calendar_plus_time_mode_from_string(const gchar *mode);

/**
 * calendar_plus_time_mode_get_id:
 * @mode: a native clock mode
 *
 * Returns: (transfer none) (nullable): the stable settings identifier, or
 *   %NULL for an invalid mode
 */
const gchar *calendar_plus_time_mode_get_id(CalendarPlusTimeMode mode);

/**
 * calendar_plus_time_mode_get_count:
 *
 * Returns: number of registered native time providers
 */
gsize calendar_plus_time_mode_get_count(void);

/**
 * calendar_plus_time_mode_get_at:
 * @index: zero-based provider catalogue index
 *
 * Returns: the registered mode, or %CALENDAR_PLUS_TIME_MODE_INVALID
 */
CalendarPlusTimeMode calendar_plus_time_mode_get_at(gsize index);

/**
 * calendar_plus_time_mode_get_name:
 * @mode: a native clock mode
 *
 * Returns: (transfer none) (nullable): stable English presentation label
 */
const gchar *calendar_plus_time_mode_get_name(CalendarPlusTimeMode mode);

/**
 * calendar_plus_time_mode_supports_seconds:
 * @mode: a native clock mode
 *
 * Returns: whether the per-applet seconds switch changes this mode's
 *   presentation precision
 */
gboolean calendar_plus_time_mode_supports_seconds(
    CalendarPlusTimeMode mode);

/**
 * calendar_plus_time_mode_requires_longitude:
 * @mode: a native clock mode
 *
 * Returns: whether the result depends on the configured astronomical
 *   longitude
 */
gboolean calendar_plus_time_mode_requires_longitude(
    CalendarPlusTimeMode mode);

/**
 * calendar_plus_format_time:
 * @mode: a native clock mode
 * @unix_microseconds: microseconds since 1970-01-01 00:00:00 UTC
 * @utc_offset_seconds: local UTC offset, including daylight saving
 * @show_seconds: whether the mode should show its finer practical unit
 * @vertical: whether clock fields should be stacked for a vertical panel
 * @longitude: degrees east of Greenwich in the range -180 through 180
 *
 * Deterministically formats one of Calendar Plus's native time systems. The
 * explicit instant and UTC offset keep this function directly testable.
 *
 * Returns: (transfer full): a newly allocated panel-clock string
 */
gchar *calendar_plus_format_time(CalendarPlusTimeMode mode,
                                 gint64 unix_microseconds,
                                 gint utc_offset_seconds,
                                 gboolean show_seconds,
                                 gboolean vertical,
                                 gdouble longitude);

/**
 * calendar_plus_time_delay_to_next_tick:
 * @mode: a native clock mode
 * @unix_microseconds: microseconds since 1970-01-01 00:00:00 UTC
 * @utc_offset_seconds: local UTC offset, including daylight saving
 * @show_seconds: whether the mode shows its finer practical unit
 * @longitude: degrees east of Greenwich in the range -180 through 180
 *
 * Calculates how long the live native clock should wait before its visible
 * value can change. The result is rounded upward because GLib timers operate
 * in whole milliseconds.
 *
 * Returns: milliseconds until the next display boundary, at least one
 */
guint calendar_plus_time_delay_to_next_tick(CalendarPlusTimeMode mode,
                                             gint64 unix_microseconds,
                                             gint utc_offset_seconds,
                                             gboolean show_seconds,
                                             gdouble longitude);

/**
 * calendar_plus_replace_time:
 * @label: a complete Cinnamon panel-clock label
 * @conventional_time: the conventional time substring to replace
 * @replacement_time: a native time-system replacement
 *
 * Replaces the first exact occurrence of @conventional_time in @label.
 *
 * Returns: (transfer full) (nullable): a newly allocated replacement label,
 *   or %NULL when @conventional_time was not found
 */
gchar *calendar_plus_replace_time(const gchar *label,
                                  const gchar *conventional_time,
                                  const gchar *replacement_time);

G_END_DECLS

#endif
