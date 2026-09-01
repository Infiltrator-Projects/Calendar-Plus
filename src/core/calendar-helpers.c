// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include "calendar-helpers.h"
#include "integer-math.h"

#include <glib/gi18n-lib.h>

gint64
calendar_plus_count_multiples_inclusive(gint64 first,
                                        gint64 last,
                                        gint64 divisor)
{
    gint64 first_quotient;
    gint64 first_remainder;
    gint64 last_quotient;
    gint64 first_multiple_index;
    gint64 distance;

    g_return_val_if_fail(divisor > 0, 0);
    if (first > last)
        return 0;

    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(first, divisor,
                                    &first_quotient, &first_remainder), 0);
    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(last, divisor,
                                    &last_quotient, NULL), 0);

    first_multiple_index = first_remainder == 0 ?
        first_quotient :
        calendar_plus_i64_add_saturating(first_quotient, 1);
    distance = infiltratr_i64_subtract_saturating(
        last_quotient, first_multiple_index);
    return distance < 0 ? 0 :
        calendar_plus_i64_add_saturating(distance, 1);
}

gchar *
calendar_plus_format_named_date(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part,
    const gchar *const *months,
    const gchar *year_suffix)
{
    const gchar *month_name;

    g_return_val_if_fail(fields != NULL, g_strdup(""));
    g_return_val_if_fail(months != NULL, g_strdup(""));
    g_return_val_if_fail(fields->month >= 1, g_strdup(""));

    month_name = _(months[fields->month]);
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
