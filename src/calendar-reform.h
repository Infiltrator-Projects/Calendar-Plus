/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private French Republican and Positivist calendar helpers.
 */

#ifndef CALENDAR_PLUS_CALENDAR_REFORM_H
#define CALENDAR_PLUS_CALENDAR_REFORM_H

#include "calendar-internal.h"

G_BEGIN_DECLS

gboolean calendar_plus_french_is_leap(gint64 year);
void calendar_plus_french_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_french_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);

gint calendar_plus_positivist_month_length(gint64 year,
                                           gint month);
void calendar_plus_positivist_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_positivist_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);

G_END_DECLS

#endif
