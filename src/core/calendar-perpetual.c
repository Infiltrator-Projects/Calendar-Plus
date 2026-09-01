// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Perpetual-calendar conversion.
 *
 * International Fixed and World Calendar dates are derived from the Gregorian
 * civil year rather than from a separate astronomical epoch.  Conversion
 * therefore begins with the Gregorian ordinal day, removes or recognises the
 * intercalary days, and maps the remaining sequence into regular periods.
 * Keeping the exceptional days explicit is important: they do not behave like
 * ordinary month days and must round-trip without shifting the following day.
 */

#include "calendar-perpetual.h"

#include "julian-day.h"

#define gregorian_is_leap calendar_plus_gregorian_is_leap
#define gregorian_to_jdn calendar_plus_gregorian_to_jdn
#define jdn_to_gregorian calendar_plus_jdn_to_gregorian
#define gregorian_day_of_year calendar_plus_gregorian_day_of_year

enum
{
    MONTHS_PER_GREGORIAN_YEAR = 12
};

gint
calendar_plus_fixed_month_length(gint64 year,
                                 gint month)
{
    if (month == 6 && gregorian_is_leap(year))
        return 29;
    if (month == 13)
        return 29;
    return 28;
}

void
calendar_plus_fixed_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint year;
    gint month;
    gint day;
    gint ordinal;

    g_return_if_fail(fields != NULL);

    jdn_to_gregorian(jdn, &year, &month, &day);
    ordinal = gregorian_day_of_year(year, month, day);
    fields->year = year;
    fields->special = FALSE;

    /*
     * In leap years, Fixed-calendar Leap Day follows 28 June at Gregorian
     * ordinal 169.
     */
    if (gregorian_is_leap(year) && ordinal == 169)
    {
        fields->month = 6;
        fields->day = 29;
        fields->special = TRUE;
    }
    else
    {
        if (gregorian_is_leap(year) && ordinal > 169)
            ordinal--;

        /* Year Day follows month 13 and sits outside the 13×28 week cycle. */
        if (ordinal == 365)
        {
            fields->month = 13;
            fields->day = 29;
            fields->special = TRUE;
        }
        else
        {
            fields->month = (ordinal - 1) / 28 + 1;
            fields->day = (ordinal - 1) % 28 + 1;
        }
    }

    fields->auxiliary = 0;
}

gint64
calendar_plus_fixed_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint ordinal;

    g_return_val_if_fail(fields != NULL,
                         gregorian_to_jdn(1970, 1, 1));

    if (fields->month == 6 && fields->day == 29)
        ordinal = 169;
    else if (fields->month == 13 && fields->day == 29)
        ordinal = gregorian_is_leap(fields->year) ? 366 : 365;
    else
    {
        ordinal = (fields->month - 1) * 28 + fields->day;
        if (gregorian_is_leap(fields->year) && ordinal > 168)
            ordinal++;
    }

    return calendar_plus_i64_add_saturating(
        gregorian_to_jdn((gint)fields->year, 1, 1),
        ordinal - 1);
}

static const gint world_base_lengths[MONTHS_PER_GREGORIAN_YEAR] = {
    31, 30, 30,
    31, 30, 30,
    31, 30, 30,
    31, 30, 30
};

gint
calendar_plus_world_month_length(gint64 year,
                                 gint month)
{
    gint length;

    g_return_val_if_fail(month >= 1 &&
                         month <= MONTHS_PER_GREGORIAN_YEAR,
                         0);

    length = world_base_lengths[month - 1];
    if (month == 6 && gregorian_is_leap(year))
        length++;
    if (month == 12)
        length++;

    return length;
}

gint64
calendar_plus_world_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint ordinal;
    gint cursor;

    g_return_val_if_fail(fields != NULL,
                         gregorian_to_jdn(1970, 1, 1));

    ordinal = fields->day;
    for (cursor = 1; cursor < fields->month; cursor++)
        ordinal += calendar_plus_world_month_length(fields->year, cursor);

    return gregorian_to_jdn((gint)fields->year, 1, 1) + ordinal - 1;
}

void
calendar_plus_world_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint year;
    gint gregorian_month;
    gint gregorian_day;
    gint remaining;
    gint month = 1;

    g_return_if_fail(fields != NULL);

    jdn_to_gregorian(jdn, &year, &gregorian_month, &gregorian_day);
    remaining = gregorian_day_of_year(year,
                                      gregorian_month,
                                      gregorian_day);
    while (remaining > calendar_plus_world_month_length(year, month))
    {
        remaining -= calendar_plus_world_month_length(year, month);
        month++;
    }

    fields->year = year;
    fields->month = month;
    fields->day = remaining;
    fields->auxiliary = 0;
    fields->special =
        (month == 6 && remaining == 31) ||
        (month == 12 && remaining == 31);
}
