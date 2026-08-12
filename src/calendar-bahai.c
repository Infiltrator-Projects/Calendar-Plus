// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include "calendar-bahai.h"

#include "julian-day.h"

#define floor_divide calendar_plus_floor_divide
#define positive_modulo calendar_plus_positive_modulo

#define PERSIAN_EPOCH_JDN ((gint64)1948321)

/*
 * Arithmetic Persian conversion from Calendrical Calculations. Calendar Plus
 * uses it for the Badíʿ year boundary as a deterministic, offline civil-date
 * mapping; it does not attempt to model sunset at the user's location.
 */
static gint64
persian_to_jdn(gint64 year,
               gint month,
               gint day)
{
    const gint64 epbase = year - (year >= 0 ? 474 : 473);
    const gint64 epyear = 474 + positive_modulo(epbase, 2820);
    const gint64 month_days =
        month <= 7 ? (month - 1) * 31 : (month - 1) * 30 + 6;

    return day +
           month_days +
           floor_divide(epyear * 682 - 110, 2816) +
           (epyear - 1) * 365 +
           floor_divide(epbase, 2820) * 1029983 +
           PERSIAN_EPOCH_JDN - 1;
}

static void
jdn_to_persian(gint64 jdn,
               gint *year,
               gint *month,
               gint *day)
{
    const gint64 depoch = jdn - persian_to_jdn(475, 1, 1);
    const gint64 cycle = floor_divide(depoch, 1029983);
    const gint64 cyear = positive_modulo(depoch, 1029983);
    gint64 ycycle;
    gint64 yday;

    if (cyear == 1029982)
    {
        ycycle = 2820;
    }
    else
    {
        const gint64 aux1 = floor_divide(cyear, 366);
        const gint64 aux2 = positive_modulo(cyear, 366);

        ycycle =
            floor_divide(2134 * aux1 + 2816 * aux2 + 2815, 1028522) +
            aux1 + 1;
    }

    *year = (gint)(ycycle + 2820 * cycle + 474);
    if (*year <= 0)
        (*year)--;

    yday = jdn - persian_to_jdn(*year, 1, 1) + 1;
    if (yday <= 186)
        *month = (gint)floor_divide(yday - 1, 31) + 1;
    else
        *month = (gint)floor_divide(yday - 7, 30) + 1;

    *day = (gint)(jdn - persian_to_jdn(*year, *month, 1) + 1);
}

gint
calendar_plus_bahai_intercalary_days(gint64 bahai_year)
{
    const gint64 persian_year = bahai_year + 1222;

    return (gint)(persian_to_jdn(persian_year + 1, 1, 1) -
                  persian_to_jdn(persian_year, 1, 1) -
                  361);
}

void
calendar_plus_bahai_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint persian_year;
    gint persian_month;
    gint persian_day;
    gint day_index;
    gint intercalary;

    g_return_if_fail(fields != NULL);

    jdn_to_persian(jdn, &persian_year, &persian_month, &persian_day);
    fields->year = persian_year - 1222;
    day_index =
        (gint)(jdn - persian_to_jdn(persian_year, 1, 1));
    intercalary = calendar_plus_bahai_intercalary_days(fields->year);

    if (day_index < 18 * 19)
    {
        fields->month = day_index / 19 + 1;
        fields->day = day_index % 19 + 1;
        fields->special = FALSE;
    }
    else if (day_index < 18 * 19 + intercalary)
    {
        fields->month = 0;
        fields->day = day_index - 18 * 19 + 1;
        fields->special = TRUE;
    }
    else
    {
        fields->month = 19;
        fields->day = day_index - 18 * 19 - intercalary + 1;
        fields->special = FALSE;
    }

    fields->auxiliary = 0;
}

gint
calendar_plus_bahai_period_index(gint month)
{
    if (month == 0)
        return 18;
    if (month == 19)
        return 19;
    return month - 1;
}

gint
calendar_plus_bahai_month_from_period(gint period)
{
    if (period == 18)
        return 0;
    if (period == 19)
        return 19;
    return period + 1;
}

gint64
calendar_plus_bahai_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint64 start;
    gint offset;

    g_return_val_if_fail(fields != NULL, PERSIAN_EPOCH_JDN);

    start = persian_to_jdn(fields->year + 1222, 1, 1);

    if (fields->month >= 1 && fields->month <= 18)
        offset = (fields->month - 1) * 19;
    else if (fields->month == 0)
        offset = 18 * 19;
    else
        offset = 18 * 19 + calendar_plus_bahai_intercalary_days(fields->year);

    return start + offset + fields->day - 1;
}
