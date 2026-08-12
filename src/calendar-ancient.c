// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Ancient-calendar engines.
 *
 * Mayan conversion uses the Goodman-Martínez-Thompson correlation constant
 * (JDN 584283) and treats the Long Count as arithmetic over kin.  The native
 * representation stores tun / uinal / kin because those are the fields the UI
 * browses; conversion remains reversible even for dates before the epoch by
 * using floor division and a positive remainder.
 *
 * Roman dates are a formatting system over the proleptic Julian calendar, not
 * a second absolute chronology.  Roman day names count inclusively backwards
 * to Kalends, Nones or Ides, which is why the apparent distance includes both
 * the current day and the target marker.
 */

#include "calendar-ancient.h"

#include "julian-day.h"

#include <glib/gi18n-lib.h>

#define floor_divide calendar_plus_floor_divide
#define positive_modulo calendar_plus_positive_modulo

#define MAYAN_EPOCH_JDN ((gint64)584283)

enum
{
    MAYAN_KIN_PER_UINAL = 20,
    MAYAN_KIN_PER_TUN = 360
};

static const gchar *const roman_month_abbreviations[] = {
    "",
    "Ian.",
    "Feb.",
    "Mar.",
    "Apr.",
    "Mai.",
    "Iun.",
    "Iul.",
    "Aug.",
    "Sep.",
    "Oct.",
    "Nov.",
    "Dec."
};

void
calendar_plus_mayan_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    const gint64 total_kin = jdn - MAYAN_EPOCH_JDN;
    const gint64 tun = floor_divide(total_kin, MAYAN_KIN_PER_TUN);
    const gint kin_in_tun =
        (gint)positive_modulo(total_kin, MAYAN_KIN_PER_TUN);

    g_return_if_fail(fields != NULL);

    fields->year = tun;
    fields->month = kin_in_tun / MAYAN_KIN_PER_UINAL;
    fields->day = kin_in_tun % MAYAN_KIN_PER_UINAL;
    fields->auxiliary = 0;
    fields->special = FALSE;
}

gint64
calendar_plus_mayan_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    g_return_val_if_fail(fields != NULL, MAYAN_EPOCH_JDN);

    return MAYAN_EPOCH_JDN +
           fields->year * MAYAN_KIN_PER_TUN +
           fields->month * MAYAN_KIN_PER_UINAL +
           fields->day;
}

gchar *
calendar_plus_roman_number(gint64 value)
{
    static const struct
    {
        gint value;
        const gchar *digits;
    } numerals[] = {
        { 1000, "M" },
        { 900, "CM" },
        { 500, "D" },
        { 400, "CD" },
        { 100, "C" },
        { 90, "XC" },
        { 50, "L" },
        { 40, "XL" },
        { 10, "X" },
        { 9, "IX" },
        { 5, "V" },
        { 4, "IV" },
        { 1, "I" }
    };
    g_autoptr(GString) result = g_string_new(NULL);
    gsize index;

    if (value <= 0)
        return g_strdup_printf("%" G_GINT64_FORMAT, value);

    for (index = 0; index < G_N_ELEMENTS(numerals); index++)
    {
        while (value >= numerals[index].value)
        {
            g_string_append(result, numerals[index].digits);
            value -= numerals[index].value;
        }
    }

    return g_string_free(g_steal_pointer(&result), FALSE);
}

static gint
julian_month_length(gint64 year,
                    gint month)
{
    if (month == 2)
        return positive_modulo(year, 4) == 0 ? 29 : 28;
    return month == 4 ||
           month == 6 ||
           month == 9 ||
           month == 11 ? 30 : 31;
}

gchar *
calendar_plus_roman_format_date(
    const CalendarPlusCalendarFields *fields)
{
    gint nones;
    gint ides;
    const gchar *marker;
    gint count = 1;
    gint target_month;
    gint64 target_year;
    g_autofree gchar *count_roman = NULL;
    g_autofree gchar *year_roman = NULL;

    g_return_val_if_fail(fields != NULL, g_strdup(""));

    nones =
        fields->month == 3 ||
        fields->month == 5 ||
        fields->month == 7 ||
        fields->month == 10 ? 7 : 5;
    ides = nones + 8;
    target_month = fields->month;
    target_year = fields->year;
    /* A.U.C. year numbering is the Julian/Gregorian year plus 753. */
    year_roman = calendar_plus_roman_number(fields->year + 753);

    if (fields->day == 1)
    {
        marker = "Kal.";
    }
    else if (fields->day < nones)
    {
        marker = "Non.";
        count = nones - fields->day + 1;
    }
    else if (fields->day == nones)
    {
        marker = "Non.";
    }
    else if (fields->day < ides)
    {
        marker = "Id.";
        count = ides - fields->day + 1;
    }
    else if (fields->day == ides)
    {
        marker = "Id.";
    }
    else
    {
        marker = "Kal.";
        /*
         * Roman dates count both the current day and the target Kalends; the
         * +2 is therefore intentional inclusive counting, not an off-by-one.
         */
        count =
            julian_month_length(fields->year, fields->month) -
            fields->day + 2;
        target_month++;
        if (target_month > 12)
        {
            target_month = 1;
            target_year++;
            g_clear_pointer(&year_roman, g_free);
            year_roman = calendar_plus_roman_number(target_year + 753);
        }
    }

    if (count == 1)
    {
        return g_strdup_printf("%s %s, %s A.U.C.",
                               marker,
                               roman_month_abbreviations[target_month],
                               year_roman);
    }
    if (count == 2)
    {
        return g_strdup_printf("prid. %s %s, %s A.U.C.",
                               marker,
                               roman_month_abbreviations[target_month],
                               year_roman);
    }

    count_roman = calendar_plus_roman_number(count);
    return g_strdup_printf("a.d. %s %s %s, %s A.U.C.",
                           count_roman,
                           marker,
                           roman_month_abbreviations[target_month],
                           year_roman);
}

gchar *
calendar_plus_mayan_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part)
{
    gint64 remaining;
    gint64 baktun;
    gint64 katun;
    gint64 tun;

    g_return_val_if_fail(fields != NULL, g_strdup(""));

    remaining = fields->year;
    baktun = floor_divide(remaining, 400);
    remaining = positive_modulo(remaining, 400);
    katun = remaining / 20;
    tun = remaining % 20;

    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup_printf(_("Uinal %d"), fields->month);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
    {
        return g_strdup_printf("%" G_GINT64_FORMAT ".%" G_GINT64_FORMAT
                               ".%" G_GINT64_FORMAT,
                               baktun,
                               katun,
                               tun);
    }

    return g_strdup_printf("%" G_GINT64_FORMAT ".%" G_GINT64_FORMAT
                           ".%" G_GINT64_FORMAT ".%d.%d",
                           baktun,
                           katun,
                           tun,
                           fields->month,
                           fields->day);
}
