/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private algorithms for non-ICU calendar systems.
 */

#ifndef CALENDAR_PLUS_CALENDAR_CUSTOM_H
#define CALENDAR_PLUS_CALENDAR_CUSTOM_H

#include "calendar-internal.h"
#include "calendar-types.h"

G_BEGIN_DECLS

void calendar_plus_custom_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gchar *calendar_plus_custom_format(
    CalendarPlusCalendarMode mode,
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);
gint64 calendar_plus_custom_month_start(
    CalendarPlusCalendarMode mode,
    gint64 jdn);
gint64 calendar_plus_custom_add_months(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount);
gint64 calendar_plus_custom_add_years(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount);
gint calendar_plus_custom_iso_week_number(gint64 jdn);

G_END_DECLS

#endif
