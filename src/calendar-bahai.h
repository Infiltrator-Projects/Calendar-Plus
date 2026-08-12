/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private Badíʿ calendar helpers.
 */

#ifndef CALENDAR_PLUS_CALENDAR_BAHAI_H
#define CALENDAR_PLUS_CALENDAR_BAHAI_H

#include "calendar-internal.h"

G_BEGIN_DECLS

gint calendar_plus_bahai_intercalary_days(gint64 bahai_year);
gint calendar_plus_bahai_period_index(gint month);
gint calendar_plus_bahai_month_from_period(gint period);
void calendar_plus_bahai_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_bahai_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);

G_END_DECLS

#endif
