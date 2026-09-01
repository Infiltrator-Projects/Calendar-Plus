/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private implementation of Sweden's historical civil calendar transitions.
 */

#ifndef CALENDAR_PLUS_CALENDAR_SWEDISH_H
#define CALENDAR_PLUS_CALENDAR_SWEDISH_H

#include "calendar-internal.h"
#include "calendar-types.h"

G_BEGIN_DECLS

void calendar_plus_swedish_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_swedish_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);
gint calendar_plus_swedish_month_length(
    const CalendarPlusCalendarFields *fields);
gchar *calendar_plus_swedish_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);
gint64 calendar_plus_swedish_month_start(gint64 jdn);
gint64 calendar_plus_swedish_add_months(gint64 jdn, gint amount);
gint64 calendar_plus_swedish_add_years(gint64 jdn, gint amount);

G_END_DECLS

#endif
