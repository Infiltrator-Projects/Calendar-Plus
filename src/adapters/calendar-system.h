// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_CALENDAR_SYSTEM_H
#define CALENDAR_PLUS_CALENDAR_SYSTEM_H

#include "calendar-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALENDAR_PLUS_TYPE_CALENDAR_SYSTEM \
    (calendar_plus_calendar_system_get_type())


/**
 * calendar_plus_date_same:
 * @year_a: first proleptic Gregorian year
 * @month_a: first Gregorian month
 * @day_a: first Gregorian day
 * @year_b: second proleptic Gregorian year
 * @month_b: second Gregorian month
 * @day_b: second Gregorian day
 *
 * Returns: %TRUE when both valid Gregorian inputs name the same civil date
 */
gboolean calendar_plus_date_same(gint year_a,
                                 gint month_a,
                                 gint day_a,
                                 gint year_b,
                                 gint month_b,
                                 gint day_b);

/**
 * calendar_plus_date_is_work_day:
 * @year: proleptic Gregorian year
 * @month: Gregorian month
 * @day: Gregorian day
 *
 * Returns the conventional Monday-through-Friday classification used by the
 * Cinnamon calendar theme.  Weekend policy remains presentation-independent
 * and has one tested native implementation.
 *
 * Returns: %TRUE for Monday through Friday
 */
gboolean calendar_plus_date_is_work_day(gint year,
                                        gint month,
                                        gint day);

G_DECLARE_FINAL_TYPE(CalendarPlusCalendarSystem,
                     calendar_plus_calendar_system,
                     CALENDAR_PLUS,
                     CALENDAR_SYSTEM,
                     GObject)

/**
 * calendar_plus_calendar_system_new:
 * @calendar_id: a stable Calendar Plus calendar setting value
 *
 * Creates a date-only calendar converter. The object is deliberately
 * independent of the panel clock: appointments keep their absolute Gregorian
 * dates while this object controls how those dates are presented and browsed.
 *
 * Returns: (transfer full) (nullable): a calendar converter, or %NULL when
 *   @calendar_id is unknown
 */
CalendarPlusCalendarSystem *calendar_plus_calendar_system_new(
    const gchar *calendar_id);

/**
 * calendar_plus_calendar_system_get_id:
 * @self: a calendar converter
 *
 * Returns: (transfer none): the stable settings identifier
 */
const gchar *calendar_plus_calendar_system_get_id(
    CalendarPlusCalendarSystem *self);

/**
 * calendar_plus_calendar_system_get_name:
 * @self: a calendar converter
 *
 * Returns: (transfer none): the human-readable calendar name
 */
const gchar *calendar_plus_calendar_system_get_name(
    CalendarPlusCalendarSystem *self);

/**
 * calendar_plus_calendar_system_format_date:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 * @part: one of `day`, `month`, `year`, `short`, or `full`
 *
 * Formats a Gregorian civil date in this object's selected calendar.
 *
 * Returns: (transfer full): a newly allocated UTF-8 string
 */
gchar *calendar_plus_calendar_system_format_date(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    const gchar *part);

/**
 * calendar_plus_calendar_system_format_date_part:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 * @part: a typed date component
 *
 * Formats a valid Gregorian civil date using a typed selector.  This is the
 * preferred 3.x API; format_date() remains as a source-compatible 2.x wrapper.
 *
 * Returns: (transfer full): a newly allocated UTF-8 string, or an empty string
 *   when the date or selector is invalid
 */
gchar *calendar_plus_calendar_system_format_date_part(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    CalendarPlusDatePart part);

/**
 * calendar_plus_calendar_system_month_start:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 *
 * Finds the first Gregorian civil date in the selected calendar period.
 * Monthless systems use their natural browsing period: an ISO week or a
 * twenty-day Mayan uinal.
 *
 * Returns: (transfer full): an ISO `YYYY-MM-DD` date
 */
gchar *calendar_plus_calendar_system_month_start(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day);

/**
 * calendar_plus_calendar_system_add_months:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 * @amount: signed number of selected-calendar periods
 *
 * Moves a date by months, ISO weeks, or Mayan uinals as appropriate and
 * clamps the day when the destination period is shorter.
 *
 * Returns: (transfer full): an ISO `YYYY-MM-DD` date
 */
gchar *calendar_plus_calendar_system_add_months(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount);

/**
 * calendar_plus_calendar_system_add_years:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 * @amount: signed number of selected-calendar years
 *
 * Moves a date by years in the selected calendar, preserving its month and
 * day where possible.
 *
 * Returns: (transfer full): an ISO `YYYY-MM-DD` date
 */
gchar *calendar_plus_calendar_system_add_years(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount);


/**
 * calendar_plus_calendar_system_month_start_parts:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 *
 * Typed equivalent of month_start() for GJS.  Returning integers avoids
 * reparsing an ISO string in the presentation layer.
 *
 * Returns: (transfer full) (nullable): an `(iii)` Gregorian year/month/day
 */
GVariant *calendar_plus_calendar_system_month_start_parts(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day);

/**
 * calendar_plus_calendar_system_add_months_parts:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 * @amount: signed number of selected-calendar periods
 *
 * Returns: (transfer full) (nullable): an `(iii)` Gregorian year/month/day
 */
GVariant *calendar_plus_calendar_system_add_months_parts(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount);

/**
 * calendar_plus_calendar_system_add_years_parts:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 * @amount: signed number of selected-calendar years
 *
 * Returns: (transfer full) (nullable): an `(iii)` Gregorian year/month/day
 */
GVariant *calendar_plus_calendar_system_add_years_parts(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount);

/**
 * calendar_plus_calendar_system_month_key:
 * @self: a calendar converter
 * @gregorian_year: proleptic Gregorian year
 * @gregorian_month: Gregorian month from 1 through 12
 * @gregorian_day: Gregorian day of month
 *
 * Returns a stable key for comparing whether two absolute dates occupy the
 * same displayed calendar period.
 *
 * Returns: (transfer full): a newly allocated period key
 */
gchar *calendar_plus_calendar_system_month_key(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day);

/**
 * calendar_plus_calendar_system_build_grid:
 * @self: a calendar converter
 * @selected_year: selected proleptic Gregorian year
 * @selected_month: selected Gregorian month from 1 through 12
 * @selected_day: selected Gregorian day of month
 * @today_year: current proleptic Gregorian year
 * @today_month: current Gregorian month from 1 through 12
 * @today_day: current Gregorian day of month
 * @week_start: first weekday, 0=Sunday through 6=Saturday
 *
 * Builds all 42 visible cells for Cinnamon's six-row calendar. The native
 * engine owns period-boundary, day-label, week-number, workday and selection
 * calculations; JavaScript only creates actors from the returned model.
 *
 * Each tuple contains the displayed day label, Gregorian year/month/day,
 * Cinnamon table row, logical weekday column, ISO week number (non-zero only
 * on that row's Thursday), and the workday/today/selected/current-period/
 * top-row/left-edge flags.
 *
 * Returns: (transfer full): an `a(siiiiiibbbbbb)` grid variant
 */
GVariant *calendar_plus_calendar_system_build_grid(
    CalendarPlusCalendarSystem *self,
    gint selected_year,
    gint selected_month,
    gint selected_day,
    gint today_year,
    gint today_month,
    gint today_day,
    gint week_start);

G_END_DECLS

#endif
