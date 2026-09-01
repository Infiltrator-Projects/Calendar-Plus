// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Reform-calendar conversion.
 *
 * Calendar Plus uses the arithmetic (Romme-style) continuation of the French
 * Republican calendar: the historical epoch is fixed at JDN 2375840 and leap
 * years follow the shifted Gregorian 400-year rule.  That choice is deliberate
 * because several incompatible post-historical proposals exist; callers must
 * get one deterministic rule for dates outside the original republican era.
 *
 * The Positivist calendar is year-relative to Gregorian chronology and is
 * handled in the same module because both systems are regular reform calendars
 * with explicit complementary days.
 */

#include "calendar-reform.h"

#include "calendar-helpers.h"
#include "julian-day.h"

#include <math.h>

#define floor_divide calendar_plus_floor_divide
#define positive_modulo calendar_plus_positive_modulo
#define gregorian_is_leap calendar_plus_gregorian_is_leap
#define gregorian_to_jdn calendar_plus_gregorian_to_jdn
#define jdn_to_gregorian calendar_plus_jdn_to_gregorian
#define gregorian_day_of_year calendar_plus_gregorian_day_of_year

#define FRENCH_EPOCH_JDN ((gint64)2375840)

gboolean
calendar_plus_french_is_leap(gint64 year)
{
    /*
     * Calendar Plus uses the Romme-style arithmetic continuation: apply the
     * Gregorian 4/100/400 rule to the following Republican year number. This
     * choice is deterministic for proleptic dates where historical practice
     * does not define a single answer.
     */
    const gint64 following = calendar_plus_i64_add_saturating(year, 1);

    return positive_modulo(following, 4) == 0 &&
           (positive_modulo(following, 100) != 0 ||
            positive_modulo(following, 400) == 0);
}

static gint64
french_leap_count(gint64 first_shifted_year,
                  gint64 last_shifted_year)
{
    return calendar_plus_count_multiples_inclusive(first_shifted_year,
                                     last_shifted_year,
                                     4) -
           calendar_plus_count_multiples_inclusive(first_shifted_year,
                                     last_shifted_year,
                                     100) +
           calendar_plus_count_multiples_inclusive(first_shifted_year,
                                     last_shifted_year,
                                     400);
}

static gint64
french_year_start(gint64 year)
{
    if (year >= 1)
    {
        const gint64 elapsed_years =
            calendar_plus_i64_subtract_saturating(year, 1);
        const gint64 leap_days = french_leap_count(2, year);
        const gint64 days = calendar_plus_i64_multiply_saturating(
            elapsed_years, 365);

        return calendar_plus_i64_add_saturating(
            calendar_plus_i64_add_saturating(
                FRENCH_EPOCH_JDN, days),
            leap_days);
    }

    {
        const gint64 elapsed_years =
            calendar_plus_i64_subtract_saturating(1, year);
        const gint64 days = calendar_plus_i64_multiply_saturating(
            elapsed_years, 365);
        gint64 result = calendar_plus_i64_subtract_saturating(
            FRENCH_EPOCH_JDN, days);

        return calendar_plus_i64_subtract_saturating(
            result,
            french_leap_count(
                calendar_plus_i64_add_saturating(year, 1), 1));
    }
}

void
calendar_plus_french_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    /*
     * The tropical-year division is only an initial estimate. The two loops
     * below correct it against exact integer year boundaries, so conversion
     * does not depend on floating-point rounding at New Year.
     */
    gint64 year =
        (gint64)floor(((gdouble)jdn - (gdouble)FRENCH_EPOCH_JDN) / 365.2425) + 1;
    gint64 start;
    gint day_of_year;

    g_return_if_fail(fields != NULL);

    while (jdn < french_year_start(year))
        year = calendar_plus_i64_subtract_saturating(year, 1);
    while (jdn >= french_year_start(calendar_plus_i64_add_saturating(year, 1)))
        year = calendar_plus_i64_add_saturating(year, 1);

    start = french_year_start(year);
    day_of_year = (gint)(jdn - start);

    fields->year = year;
    fields->month = day_of_year / 30 + 1;
    fields->day = day_of_year % 30 + 1;
    fields->auxiliary = 0;
    fields->special = fields->month == 13;
}

gint64
calendar_plus_french_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, FRENCH_EPOCH_JDN);

    gint64 result = french_year_start(fields->year);

    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(
            fields->month - 1, 30));
    return calendar_plus_i64_add_saturating(
        result, fields->day - 1);
}

gint
calendar_plus_positivist_month_length(gint64 year,
                                      gint month)
{
    if (month == 13)
        return gregorian_is_leap(year) ? 30 : 29;
    return 28;
}

void
calendar_plus_positivist_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint year;
    gint gregorian_month;
    gint gregorian_day;
    gint ordinal;

    g_return_if_fail(fields != NULL);

    jdn_to_gregorian(jdn, &year, &gregorian_month, &gregorian_day);
    ordinal = gregorian_day_of_year(year,
                                    gregorian_month,
                                    gregorian_day);

    fields->year = year;
    if (ordinal <= 364)
    {
        fields->month = (ordinal - 1) / 28 + 1;
        fields->day = (ordinal - 1) % 28 + 1;
    }
    else
    {
        fields->month = 13;
        fields->day = ordinal - 336;
    }

    fields->auxiliary = 0;
    fields->special = fields->day > 28;
}

gint64
calendar_plus_positivist_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL,
                         gregorian_to_jdn(1970, 1, 1));

    return gregorian_to_jdn((gint)fields->year, 1, 1) +
           (gint64)(fields->month - 1) * 28 +
           fields->day - 1;
}
