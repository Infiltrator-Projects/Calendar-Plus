/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Deterministic historical calendar models not supplied by ICU.
 *
 * Revised Julian follows Milanković's 1923 leap rule: ordinary fourth years
 * leap, century years leap only when their remainder modulo 900 is 200 or 600.
 * The arithmetic continuation is anchored to 2000-01-01, when Revised Julian
 * and Gregorian dates coincide.
 *
 * Byzantine Anno Mundi uses the Julian calendar, a 1 September year boundary
 * and the Constantinopolitan creation era whose first year begins 1 September
 * 5509 BC. Calendar Plus uses astronomical year numbering internally but
 * formats the historical Anno Mundi year without changing Julian month/day.
 *
 * Egyptian civil dates use Ptolemy's Nabonassar era as the absolute epoch:
 * 1 Thoth, year 1 = JDN 1448638 (26 February 747 BC in the proleptic Julian
 * calendar). This makes the otherwise wandering 365-day Egyptian civil year
 * deterministic. It must not be confused with pharaonic regnal-year dating.
 */

#include "calendar-historical.h"

#include "julian-day.h"

#include <glib/gi18n-lib.h>
#include <math.h>

#define floor_divide calendar_plus_floor_divide
#define positive_modulo calendar_plus_positive_modulo
#define gregorian_to_jdn calendar_plus_gregorian_to_jdn
#define jdn_to_julian calendar_plus_jdn_to_julian
#define julian_to_jdn calendar_plus_julian_to_jdn

#define REVISED_JULIAN_ANCHOR_YEAR ((gint64)2000)
#define EGYPTIAN_NABONASSAR_EPOCH_JDN ((gint64)1448638)

static const gchar *const common_months[] = {
    "", N_("January"), N_("February"), N_("March"), N_("April"),
    N_("May"), N_("June"), N_("July"), N_("August"), N_("September"),
    N_("October"), N_("November"), N_("December")
};

static const gchar *const egyptian_months[] = {
    "", N_("Thoth"), N_("Phaophi"), N_("Hathyr"), N_("Choiak"),
    N_("Tybi"), N_("Mecheir"), N_("Phamenoth"), N_("Pharmouthi"),
    N_("Pachons"), N_("Payni"), N_("Epiphi"), N_("Mesore"),
    N_("Epagomenal days")
};

static gint64
count_multiples_inclusive(gint64 first,
                          gint64 last,
                          gint64 divisor)
{
    if (first > last)
        return 0;

    return floor_divide(last, divisor) -
           floor_divide(first - 1, divisor);
}

static gint64
count_congruence_inclusive(gint64 first,
                           gint64 last,
                           gint64 modulus,
                           gint64 residue)
{
    if (first > last)
        return 0;

    return floor_divide(last - residue, modulus) -
           floor_divide(first - 1 - residue, modulus);
}

static gboolean
revised_julian_is_leap(gint64 year)
{
    const gint64 century_remainder = positive_modulo(year, 900);

    if (positive_modulo(year, 4) != 0)
        return FALSE;
    if (positive_modulo(year, 100) != 0)
        return TRUE;

    return century_remainder == 200 || century_remainder == 600;
}

static gint64
revised_julian_leaps_inclusive(gint64 first,
                               gint64 last)
{
    return count_multiples_inclusive(first, last, 4) -
           count_multiples_inclusive(first, last, 100) +
           count_congruence_inclusive(first, last, 900, 200) +
           count_congruence_inclusive(first, last, 900, 600);
}

static gint64
revised_julian_year_start(gint64 year)
{
    const gint64 anchor = gregorian_to_jdn(2000, 1, 1);

    if (year >= REVISED_JULIAN_ANCHOR_YEAR)
    {
        return anchor +
               (year - REVISED_JULIAN_ANCHOR_YEAR) * 365 +
               revised_julian_leaps_inclusive(
                   REVISED_JULIAN_ANCHOR_YEAR, year - 1);
    }

    return anchor -
           (REVISED_JULIAN_ANCHOR_YEAR - year) * 365 -
           revised_julian_leaps_inclusive(
               year, REVISED_JULIAN_ANCHOR_YEAR - 1);
}

static gint
gregorian_shape_month_length(gint64 year,
                             gint month,
                             gboolean leap)
{
    (void)year;

    if (month == 2)
        return leap ? 29 : 28;
    return month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31;
}

gint
calendar_plus_revised_julian_month_length(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, 0);
    g_return_val_if_fail(fields->month >= 1 && fields->month <= 12, 0);

    return gregorian_shape_month_length(
        fields->year,
        fields->month,
        revised_julian_is_leap(fields->year));
}

gint64
calendar_plus_revised_julian_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint64 jdn;
    gint month;

    g_return_val_if_fail(fields != NULL, gregorian_to_jdn(2000, 1, 1));

    jdn = revised_julian_year_start(fields->year);
    for (month = 1; month < fields->month; month++)
    {
        CalendarPlusCalendarFields cursor = *fields;
        cursor.month = month;
        jdn += calendar_plus_revised_julian_month_length(&cursor);
    }

    return jdn + fields->day - 1;
}

void
calendar_plus_revised_julian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    const gint64 anchor = gregorian_to_jdn(2000, 1, 1);
    gint64 year;
    gint64 start;
    gint remaining;
    gint month = 1;

    g_return_if_fail(fields != NULL);

    year = (gint64)floorl(
        (long double)(jdn - anchor) / 365.2422222222222L) +
        REVISED_JULIAN_ANCHOR_YEAR;

    while (jdn < revised_julian_year_start(year))
        year--;
    while (jdn >= revised_julian_year_start(year + 1))
        year++;

    start = revised_julian_year_start(year);
    remaining = (gint)(jdn - start);
    while (month < 12)
    {
        CalendarPlusCalendarFields cursor = {
            .year = year,
            .month = month
        };
        const gint length =
            calendar_plus_revised_julian_month_length(&cursor);

        if (remaining < length)
            break;
        remaining -= length;
        month++;
    }

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month,
        .day = remaining + 1,
        .auxiliary = 0,
        .special = FALSE
    };
}

static gchar *
format_named_date(const CalendarPlusCalendarFields *fields,
                  CalendarPlusDatePart part,
                  const gchar *const *months,
                  const gchar *year_suffix)
{
    const gchar *month_name = _(months[fields->month]);

    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup(month_name);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
    {
        return year_suffix != NULL ?
            g_strdup_printf("%" G_GINT64_FORMAT " %s",
                            fields->year, year_suffix) :
            g_strdup_printf("%" G_GINT64_FORMAT, fields->year);
    }

    return year_suffix != NULL ?
        g_strdup_printf(_("%d %s %lld %s"),
                        fields->day, month_name,
                        (long long)fields->year, year_suffix) :
        g_strdup_printf(_("%d %s %lld"),
                        fields->day, month_name,
                        (long long)fields->year);
}

gchar *
calendar_plus_revised_julian_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part)
{
    g_return_val_if_fail(fields != NULL, g_strdup(""));
    return format_named_date(fields, part, common_months, NULL);
}

void
calendar_plus_byzantine_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint julian_year;
    gint month;
    gint day;

    g_return_if_fail(fields != NULL);

    jdn_to_julian(jdn, &julian_year, &month, &day);
    *fields = (CalendarPlusCalendarFields){
        .year = (gint64)julian_year + (month >= 9 ? 5509 : 5508),
        .month = month,
        .day = day,
        .auxiliary = 0,
        .special = FALSE
    };
}

gint64
calendar_plus_byzantine_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint64 julian_year;

    g_return_val_if_fail(fields != NULL, julian_to_jdn(2000, 1, 1));

    julian_year =
        fields->year - (fields->month >= 9 ? 5509 : 5508);
    g_return_val_if_fail(julian_year >= G_MININT &&
                         julian_year <= G_MAXINT,
                         julian_to_jdn(2000, 1, 1));

    return julian_to_jdn((gint)julian_year, fields->month, fields->day);
}

gint
calendar_plus_byzantine_month_length(
    const CalendarPlusCalendarFields *fields)
{
    gint64 julian_year;

    g_return_val_if_fail(fields != NULL, 0);
    g_return_val_if_fail(fields->month >= 1 && fields->month <= 12, 0);

    julian_year = fields->year - (fields->month >= 9 ? 5509 : 5508);
    return gregorian_shape_month_length(
        julian_year,
        fields->month,
        positive_modulo(julian_year, 4) == 0);
}

gint
calendar_plus_byzantine_period_index(gint month)
{
    g_return_val_if_fail(month >= 1 && month <= 12, 0);
    return month >= 9 ? month - 9 : month + 3;
}

gint
calendar_plus_byzantine_month_from_period(gint period)
{
    g_return_val_if_fail(period >= 0 && period < 12, 9);
    return period < 4 ? period + 9 : period - 3;
}

gchar *
calendar_plus_byzantine_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part)
{
    g_return_val_if_fail(fields != NULL, g_strdup(""));
    return format_named_date(fields, part, common_months, "A.M.");
}

void
calendar_plus_egyptian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    const gint64 elapsed = jdn - EGYPTIAN_NABONASSAR_EPOCH_JDN;
    const gint64 year = floor_divide(elapsed, 365) + 1;
    const gint day_of_year = (gint)positive_modulo(elapsed, 365);

    g_return_if_fail(fields != NULL);

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = day_of_year < 360 ? day_of_year / 30 + 1 : 13,
        .day = day_of_year < 360 ? day_of_year % 30 + 1 :
               day_of_year - 360 + 1,
        .auxiliary = 0,
        .special = day_of_year >= 360
    };
}

gint64
calendar_plus_egyptian_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint offset;

    g_return_val_if_fail(fields != NULL, EGYPTIAN_NABONASSAR_EPOCH_JDN);

    offset = fields->month <= 12 ?
        (fields->month - 1) * 30 :
        360;

    return EGYPTIAN_NABONASSAR_EPOCH_JDN +
           (fields->year - 1) * 365 +
           offset + fields->day - 1;
}

gint
calendar_plus_egyptian_month_length(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, 0);
    g_return_val_if_fail(fields->month >= 1 && fields->month <= 13, 0);

    return fields->month == 13 ? 5 : 30;
}

gchar *
calendar_plus_egyptian_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part)
{
    g_return_val_if_fail(fields != NULL, g_strdup(""));
    return format_named_date(fields, part, egyptian_months, NULL);
}

/*
 * Traditional Armenian wandering year.
 *
 * The numbered Armenian era begins 11 July 552 in the Julian calendar.
 * Each year has twelve 30-day months followed by five Aweleacʿ epagomenal
 * days and no leap day; this is deliberately separate from Sarkawag's reform.
 */
#define ARMENIAN_EPOCH_JDN (calendar_plus_julian_to_jdn(552, 7, 11))

static const gchar *const armenian_months[] = {
    "", N_("Nawasard"), N_("Hoṙi"), N_("Sahmi"), N_("Trē"),
    N_("Kʿałocʿ"), N_("Aracʿ"), N_("Mehekan"), N_("Areg"),
    N_("Ahekan"), N_("Mareri"), N_("Margacʿ"), N_("Hroticʿ"),
    N_("Aweleacʿ")
};

void
calendar_plus_armenian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    const gint64 elapsed = jdn - ARMENIAN_EPOCH_JDN;
    const gint64 year = floor_divide(elapsed, 365) + 1;
    const gint day_of_year = (gint)positive_modulo(elapsed, 365);

    g_return_if_fail(fields != NULL);
    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = day_of_year < 360 ? day_of_year / 30 + 1 : 13,
        .day = day_of_year < 360 ? day_of_year % 30 + 1 :
               day_of_year - 360 + 1,
        .auxiliary = 0,
        .special = day_of_year >= 360
    };
}

gint64
calendar_plus_armenian_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint offset;

    g_return_val_if_fail(fields != NULL, ARMENIAN_EPOCH_JDN);
    offset = fields->month <= 12 ? (fields->month - 1) * 30 : 360;
    return ARMENIAN_EPOCH_JDN + (fields->year - 1) * 365 +
           offset + fields->day - 1;
}

gint
calendar_plus_armenian_month_length(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, 0);
    g_return_val_if_fail(fields->month >= 1 && fields->month <= 13, 0);
    return fields->month == 13 ? 5 : 30;
}

gchar *
calendar_plus_armenian_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part)
{
    g_return_val_if_fail(fields != NULL, g_strdup(""));
    return format_named_date(fields, part, armenian_months, NULL);
}
