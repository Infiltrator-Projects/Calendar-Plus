/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * ICU/CLDR calendar adapter.
 *
 * ICU owns field conversion, leap-month rules, era data and locale-sensitive
 * formatting for established calendars.  Calendar Plus always opens these
 * calendars in UTC and places the JDN at UTC noon.  Noon avoids any midnight
 * ambiguity while UTC ensures that host timezone and daylight-saving changes
 * cannot alter a date-only conversion.
 */


#include "icu-calendar.h"

#include <infiltratr/core.h>

#include "julian-day.h"

#include <math.h>

#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

#define MILLISECONDS_PER_DAY 86400000.0

static const UChar utc_zone[] = { 0x0055, 0x0054, 0x0043, 0 };

static gboolean
make_icu_locale(const gchar *calendar_keyword,
                gchar *locale,
                gint32 locale_capacity)
{
    UErrorCode status = U_ZERO_ERROR;

    if (calendar_keyword == NULL || *calendar_keyword == '\0' ||
        locale_capacity <= 0)
        return FALSE;

    infiltratr_copy_string(locale, (size_t)locale_capacity, uloc_getDefault());
    uloc_setKeywordValue("calendar",
                         calendar_keyword,
                         locale,
                         locale_capacity,
                         &status);
    return U_SUCCESS(status);
}

static UCalendar *
open_icu_calendar(const gchar *calendar_keyword,
                  UErrorCode *status)
{
    gchar locale[ULOC_FULLNAME_CAPACITY];

    if (!make_icu_locale(calendar_keyword, locale, G_N_ELEMENTS(locale)))
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    return ucal_open(utc_zone, 3, locale, UCAL_DEFAULT, status);
}

static UDate
jdn_to_udate(gint64 jdn)
{
    return (UDate)(jdn - CALENDAR_PLUS_UNIX_EPOCH_JDN) *
               MILLISECONDS_PER_DAY +
           MILLISECONDS_PER_DAY / 2.0;
}

static gint64
udate_to_jdn(UDate date)
{
    return (gint64)floor(date / MILLISECONDS_PER_DAY) +
           CALENDAR_PLUS_UNIX_EPOCH_JDN;
}

gboolean
calendar_plus_icu_fields_from_jdn(
    const gchar *calendar_keyword,
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *calendar;

    g_return_val_if_fail(fields != NULL, FALSE);

    calendar = open_icu_calendar(calendar_keyword, &status);
    if (U_FAILURE(status) || calendar == NULL)
        return FALSE;

    ucal_setMillis(calendar, jdn_to_udate(jdn), &status);
    fields->year = ucal_get(calendar, UCAL_YEAR, &status);
    fields->month = ucal_get(calendar, UCAL_MONTH, &status) + 1;
    fields->day = ucal_get(calendar, UCAL_DATE, &status);
    fields->auxiliary = ucal_get(calendar, UCAL_ERA, &status);
    fields->special =
        ucal_get(calendar, UCAL_IS_LEAP_MONTH, &status) != 0;
    ucal_close(calendar);

    return U_SUCCESS(status);
}

static gchar *
utf16_to_utf8(const UChar *source,
              gint32 source_length)
{
    UErrorCode status = U_ZERO_ERROR;
    gint32 required = 0;
    gchar *result;

    u_strToUTF8(NULL, 0, &required, source, source_length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
        return g_strdup("");

    status = U_ZERO_ERROR;
    result = g_malloc((gsize)required + 1);
    u_strToUTF8(result,
                required + 1,
                NULL,
                source,
                source_length,
                &status);
    if (U_FAILURE(status))
    {
        g_free(result);
        return g_strdup("");
    }

    return result;
}

static const gchar *
pattern_for_part(CalendarPlusCalendarMode mode,
                 CalendarPlusDatePart part)
{
    const gboolean era_calendar =
        mode == CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_JAPANESE ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_MINGUO;
    const gboolean omit_era =
        mode == CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_PERSIAN ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_CHINESE ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_INDIAN ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_COPTIC ||
        mode == CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN;

    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return "d";
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return "LLLL";
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
    {
        if (mode == CALENDAR_PLUS_CALENDAR_MODE_CHINESE)
            return "r (U)";
        return era_calendar ? "G y" : "y";
    }
    if (part == CALENDAR_PLUS_DATE_PART_FULL)
    {
        if (mode == CALENDAR_PLUS_CALENDAR_MODE_CHINESE)
            return "EEEE, d MMMM r (U)";
        if (era_calendar)
            return "EEEE, d MMMM G y";
        return omit_era ? "EEEE, d MMMM y" : "EEEE, d MMMM y G";
    }

    if (mode == CALENDAR_PLUS_CALENDAR_MODE_CHINESE)
        return "d MMMM r (U)";
    if (era_calendar)
        return "d MMMM G y";
    return omit_era ? "d MMMM y" : "d MMMM y G";
}

gchar *
calendar_plus_icu_format(CalendarPlusCalendarMode format_profile,
                         const gchar *calendar_keyword,
                         gint64 jdn,
                         CalendarPlusDatePart part)
{
    UErrorCode status = U_ZERO_ERROR;
    gchar locale[ULOC_FULLNAME_CAPACITY];
    UChar pattern_utf16[128];
    gint32 pattern_length;
    UDateFormat *formatter;
    UChar stack_buffer[256];
    UChar *output = stack_buffer;
    gint32 capacity = G_N_ELEMENTS(stack_buffer);
    gint32 length;
    gchar *result;
    const gchar *pattern = pattern_for_part(format_profile, part);

    if (!make_icu_locale(calendar_keyword, locale, G_N_ELEMENTS(locale)))
        return g_strdup("");

    u_strFromUTF8(pattern_utf16,
                  G_N_ELEMENTS(pattern_utf16),
                  &pattern_length,
                  pattern,
                  -1,
                  &status);
    if (U_FAILURE(status))
        return g_strdup("");

    formatter = udat_open(UDAT_PATTERN,
                          UDAT_PATTERN,
                          locale,
                          utc_zone,
                          3,
                          pattern_utf16,
                          pattern_length,
                          &status);
    if (U_FAILURE(status) || formatter == NULL)
        return g_strdup("");

    length = udat_format(formatter,
                         jdn_to_udate(jdn),
                         output,
                         capacity,
                         NULL,
                         &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
    {
        status = U_ZERO_ERROR;
        capacity = length + 1;
        output = g_new(UChar, (gsize)capacity);
        length = udat_format(formatter,
                             jdn_to_udate(jdn),
                             output,
                             capacity,
                             NULL,
                             &status);
    }

    result = U_FAILURE(status) ? g_strdup("") :
             utf16_to_utf8(output, length);
    if (output != stack_buffer)
        g_free(output);
    udat_close(formatter);
    return result;
}

static gint64
icu_add(const gchar *calendar_keyword,
        gint64 jdn,
        UCalendarDateFields field,
        gint amount)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *calendar = open_icu_calendar(calendar_keyword, &status);
    gint original_day;
    gint maximum_day;
    UDate result;

    if (U_FAILURE(status) || calendar == NULL)
        return jdn;

    /*
     * Change the month or year from day one, then restore the original day
     * with an explicit clamp. This makes the navigation rule independent of
     * ICU's leniency behaviour: preserve the day when the target permits it.
     */
    ucal_setMillis(calendar, jdn_to_udate(jdn), &status);
    original_day = ucal_get(calendar, UCAL_DATE, &status);
    ucal_set(calendar, UCAL_DATE, 1);
    ucal_add(calendar, field, amount, &status);
    maximum_day = ucal_getLimit(calendar,
                                UCAL_DATE,
                                UCAL_ACTUAL_MAXIMUM,
                                &status);
    ucal_set(calendar, UCAL_DATE, MIN(original_day, maximum_day));
    result = ucal_getMillis(calendar, &status);
    ucal_close(calendar);

    return U_SUCCESS(status) ? udate_to_jdn(result) : jdn;
}

gint64
calendar_plus_icu_month_start(const gchar *calendar_keyword,
                              gint64 jdn)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *calendar = open_icu_calendar(calendar_keyword, &status);
    UDate result;

    if (U_FAILURE(status) || calendar == NULL)
        return jdn;

    ucal_setMillis(calendar, jdn_to_udate(jdn), &status);
    ucal_set(calendar, UCAL_DATE, 1);
    result = ucal_getMillis(calendar, &status);
    ucal_close(calendar);
    return U_SUCCESS(status) ? udate_to_jdn(result) : jdn;
}

gint64
calendar_plus_icu_add_months(const gchar *calendar_keyword,
                             gint64 jdn,
                             gint amount)
{
    return icu_add(calendar_keyword, jdn, UCAL_MONTH, amount);
}

gint64
calendar_plus_icu_add_years(const gchar *calendar_keyword,
                            gint64 jdn,
                            gint amount)
{
    return icu_add(calendar_keyword, jdn, UCAL_YEAR, amount);
}
