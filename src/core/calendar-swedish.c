/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Swedish historical civil calendar.
 *
 * Sweden used the Julian calendar through 28 February 1700. From 1 March
 * 1700 through 30 February 1712 the Swedish style was exactly one civil date
 * ahead of Julian because the Julian leap day of 1700 had been omitted while
 * the 1704 and 1708 leap days were retained. The unique 30 February 1712
 * restored alignment with Julian; 1 March 1712 through 17 February 1753 was
 * Julian again. Sweden then omitted eleven civil dates, so 17 February 1753
 * was followed by 1 March 1753 in the Gregorian calendar.
 *
 * This provider models those actual civil labels rather than inventing a
 * proleptic Swedish rule outside the historical transitions: earlier dates are
 * Julian and later dates Gregorian. Navigation is implemented here rather
 * than through the regular-calendar helper because February 1712 has 30 days
 * and February 1753 has only 17 valid Swedish civil dates.
 */

#include "calendar-swedish.h"

#include "julian-day.h"

#include <glib/gi18n-lib.h>

#define floor_divide calendar_plus_floor_divide
#define gregorian_to_jdn calendar_plus_gregorian_to_jdn
#define jdn_to_gregorian calendar_plus_jdn_to_gregorian
#define julian_to_jdn calendar_plus_julian_to_jdn
#define jdn_to_julian calendar_plus_jdn_to_julian
#define positive_modulo calendar_plus_positive_modulo

static const gchar *const months[] = {
    "", N_("January"), N_("February"), N_("March"), N_("April"),
    N_("May"), N_("June"), N_("July"), N_("August"), N_("September"),
    N_("October"), N_("November"), N_("December")
};

static gint
normal_month_length(gint64 year,
                    gint month,
                    gboolean gregorian)
{
    gboolean leap;

    if (gregorian)
    {
        leap = positive_modulo(year, 4) == 0 &&
               (positive_modulo(year, 100) != 0 ||
                positive_modulo(year, 400) == 0);
    }
    else
    {
        leap = positive_modulo(year, 4) == 0;
    }

    if (month == 2)
        return leap ? 29 : 28;
    return month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31;
}

static gint64
swedish_style_start_jdn(void)
{
    /* Swedish 1700-03-01 coincides with Julian 1700-02-29. */
    return julian_to_jdn(1700, 3, 1) - 1;
}

static gint64
swedish_style_last_jdn(void)
{
    /* Swedish 1712-02-30 coincides with Julian 1712-02-29. */
    return julian_to_jdn(1712, 2, 29);
}

static gint64
gregorian_adoption_jdn(void)
{
    /* Swedish 1753-02-17 is followed immediately by Gregorian 1753-03-01. */
    return gregorian_to_jdn(1753, 3, 1);
}

void
calendar_plus_swedish_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    const gint64 style_start = swedish_style_start_jdn();
    const gint64 style_last = swedish_style_last_jdn();
    const gint64 gregorian_start = gregorian_adoption_jdn();
    gint year;
    gint month;
    gint day;

    g_return_if_fail(fields != NULL);

    /* The two genuine-Julian eras share one conversion path. */
    if (jdn < style_start || (jdn > style_last && jdn < gregorian_start))
    {
        jdn_to_julian(jdn, &year, &month, &day);
    }
    else if (jdn < style_last)
    {
        /* During Swedish style the displayed date is Julian + one day. */
        jdn_to_julian(jdn + 1, &year, &month, &day);
    }
    else if (jdn == style_last)
    {
        year = 1712;
        month = 2;
        day = 30;
    }
    else
    {
        jdn_to_gregorian(jdn, &year, &month, &day);
    }

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month,
        .day = day,
        .auxiliary = 0,
        .special = (year == 1712 && month == 2 && day == 30)
    };
}

gint64
calendar_plus_swedish_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, gregorian_to_jdn(2000, 1, 1));

    if (fields->year < 1700 ||
        (fields->year == 1700 && fields->month < 3))
    {
        return julian_to_jdn((gint)fields->year, fields->month, fields->day);
    }

    if (fields->year < 1712 ||
        (fields->year == 1712 && fields->month < 3))
    {
        if (fields->year == 1712 && fields->month == 2 && fields->day == 30)
            return swedish_style_last_jdn();
        return julian_to_jdn((gint)fields->year, fields->month, fields->day) - 1;
    }

    if (fields->year < 1753 ||
        (fields->year == 1753 && fields->month < 3))
    {
        return julian_to_jdn((gint)fields->year, fields->month, fields->day);
    }

    return gregorian_to_jdn((gint)fields->year, fields->month, fields->day);
}

gint
calendar_plus_swedish_month_length(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, 0);
    g_return_val_if_fail(fields->month >= 1 && fields->month <= 12, 0);

    if (fields->year == 1700 && fields->month == 2)
        return 28;
    if (fields->year == 1712 && fields->month == 2)
        return 30;
    if (fields->year == 1753 && fields->month == 2)
        return 17;

    return normal_month_length(fields->year,
                               fields->month,
                               fields->year > 1753 ||
                               (fields->year == 1753 && fields->month >= 3));
}

gchar *
calendar_plus_swedish_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part)
{
    const gchar *month_name;

    g_return_val_if_fail(fields != NULL, g_strdup(""));
    g_return_val_if_fail(fields->month >= 1 && fields->month <= 12,
                         g_strdup(""));

    month_name = _(months[fields->month]);
    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup(month_name);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
        return g_strdup_printf("%" G_GINT64_FORMAT, fields->year);

    return g_strdup_printf(_("%d %s %lld"),
                           fields->day,
                           month_name,
                           (long long)fields->year);
}

gint64
calendar_plus_swedish_month_start(gint64 jdn)
{
    CalendarPlusCalendarFields fields;

    calendar_plus_swedish_fields_from_jdn(jdn, &fields);
    fields.day = 1;
    fields.special = FALSE;
    return calendar_plus_swedish_fields_to_jdn(&fields);
}

gint64
calendar_plus_swedish_add_months(gint64 jdn,
                                 gint amount)
{
    CalendarPlusCalendarFields fields;
    gint64 serial;

    calendar_plus_swedish_fields_from_jdn(jdn, &fields);
    serial = fields.year * 12 + (fields.month - 1) + amount;
    fields.year = floor_divide(serial, 12);
    fields.month = (gint)positive_modulo(serial, 12) + 1;
    fields.day = MIN(fields.day, calendar_plus_swedish_month_length(&fields));
    fields.special = fields.year == 1712 &&
                     fields.month == 2 && fields.day == 30;
    return calendar_plus_swedish_fields_to_jdn(&fields);
}

gint64
calendar_plus_swedish_add_years(gint64 jdn,
                                gint amount)
{
    CalendarPlusCalendarFields fields;

    calendar_plus_swedish_fields_from_jdn(jdn, &fields);
    fields.year += amount;
    fields.day = MIN(fields.day, calendar_plus_swedish_month_length(&fields));
    fields.special = fields.year == 1712 &&
                     fields.month == 2 && fields.day == 30;
    return calendar_plus_swedish_fields_to_jdn(&fields);
}
