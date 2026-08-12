/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private Roman and Mayan calendar helpers.
 */

#ifndef CALENDAR_PLUS_CALENDAR_ANCIENT_H
#define CALENDAR_PLUS_CALENDAR_ANCIENT_H

#include "calendar-internal.h"
#include "calendar-types.h"

G_BEGIN_DECLS

void calendar_plus_mayan_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_mayan_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);
gchar *calendar_plus_mayan_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);

gchar *calendar_plus_roman_number(gint64 value);
gchar *calendar_plus_roman_format_date(
    const CalendarPlusCalendarFields *fields);

G_END_DECLS

#endif
