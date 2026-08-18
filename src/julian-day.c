/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Integer civil-date arithmetic.
 *
 * Calendar Plus uses an integral Julian Day Number (JDN) as the internal
 * date-only axis. A JDN changes at civil midnight in this module; it is not
 * the fractional astronomical Julian Date used by the clock display.
 *
 * Gregorian/Julian conversion remains Calendar-owned. Negative-safe floor
 * division and Euclidean remainder are delegated to Infiltratr Common so all
 * portable consumers share one integer contract.
 */

#include "julian-day.h"

#include <infiltratr/arithmetic.h>

gint64
calendar_plus_floor_divide(gint64 value,
                           gint64 divisor)
{
    int64_t quotient = 0;

    g_return_val_if_fail(divisor > 0, 0);
    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL), 0);
    return quotient;
}

gint64
calendar_plus_positive_modulo(gint64 value,
                              gint64 modulus)
{
    int64_t remainder = 0;

    g_return_val_if_fail(modulus > 0, 0);
    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder), 0);
    return remainder;
}

gboolean
calendar_plus_gregorian_is_leap(gint64 year)
{
    return calendar_plus_positive_modulo(year, 4) == 0 &&
           (calendar_plus_positive_modulo(year, 100) != 0 ||
            calendar_plus_positive_modulo(year, 400) == 0);
}

static gint
gregorian_month_length(gint64 year,
                       gint month)
{
    static const gint lengths[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && calendar_plus_gregorian_is_leap(year))
        return 29;
    return lengths[month - 1];
}

gboolean
calendar_plus_gregorian_date_is_valid(gint year,
                                      gint month,
                                      gint day)
{
    (void)year;
    return day >= 1 && day <= gregorian_month_length(year, month);
}

gint64
calendar_plus_gregorian_to_jdn(gint64 year,
                               gint month,
                               gint day)
{
    const gint64 a = calendar_plus_floor_divide(14 - month, 12);
    const gint64 y = year + 4800 - a;
    const gint64 m = month + 12 * a - 3;

    return day +
           calendar_plus_floor_divide(153 * m + 2, 5) +
           365 * y +
           calendar_plus_floor_divide(y, 4) -
           calendar_plus_floor_divide(y, 100) +
           calendar_plus_floor_divide(y, 400) -
           32045;
}

void
calendar_plus_jdn_to_gregorian(gint64 jdn,
                               gint *year,
                               gint *month,
                               gint *day)
{
    const gint64 a = jdn + 32044;
    const gint64 b = calendar_plus_floor_divide(4 * a + 3, 146097);
    const gint64 c = a - calendar_plus_floor_divide(146097 * b, 4);
    const gint64 d = calendar_plus_floor_divide(4 * c + 3, 1461);
    const gint64 e = c - calendar_plus_floor_divide(1461 * d, 4);
    const gint64 m = calendar_plus_floor_divide(5 * e + 2, 153);

    g_return_if_fail(year != NULL);
    g_return_if_fail(month != NULL);
    g_return_if_fail(day != NULL);

    *day = (gint)(e - calendar_plus_floor_divide(153 * m + 2, 5) + 1);
    *month = (gint)(m + 3 -
                    12 * calendar_plus_floor_divide(m, 10));
    *year = (gint)(100 * b + d - 4800 +
                   calendar_plus_floor_divide(m, 10));
}

gint64
calendar_plus_julian_to_jdn(gint64 year,
                            gint month,
                            gint day)
{
    const gint64 a = calendar_plus_floor_divide(14 - month, 12);
    const gint64 y = year + 4800 - a;
    const gint64 m = month + 12 * a - 3;

    return day +
           calendar_plus_floor_divide(153 * m + 2, 5) +
           365 * y +
           calendar_plus_floor_divide(y, 4) -
           32083;
}

void
calendar_plus_jdn_to_julian(gint64 jdn,
                            gint *year,
                            gint *month,
                            gint *day)
{
    const gint64 c = jdn + 32082;
    const gint64 d = calendar_plus_floor_divide(4 * c + 3, 1461);
    const gint64 e = c - calendar_plus_floor_divide(1461 * d, 4);
    const gint64 m = calendar_plus_floor_divide(5 * e + 2, 153);

    g_return_if_fail(year != NULL);
    g_return_if_fail(month != NULL);
    g_return_if_fail(day != NULL);

    *day = (gint)(e - calendar_plus_floor_divide(153 * m + 2, 5) + 1);
    *month = (gint)(m + 3 -
                    12 * calendar_plus_floor_divide(m, 10));
    *year = (gint)(d - 4800 +
                   calendar_plus_floor_divide(m, 10));
}

gint
calendar_plus_gregorian_day_of_year(gint year,
                                    gint month,
                                    gint day)
{
    return (gint)(calendar_plus_gregorian_to_jdn(year, month, day) -
                  calendar_plus_gregorian_to_jdn(year, 1, 1) + 1);
}

gint
calendar_plus_iso_weekday(gint64 jdn)
{
    return (gint)calendar_plus_positive_modulo(
               jdn,
               CALENDAR_PLUS_DAYS_PER_WEEK) + 1;
}

gchar *
calendar_plus_format_iso_date(gint64 jdn)
{
    gint year;
    gint month;
    gint day;

    calendar_plus_jdn_to_gregorian(jdn, &year, &month, &day);
    return g_strdup_printf("%04d-%02d-%02d", year, month, day);
}
