/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private International Fixed and World Calendar helpers.
 */

#ifndef CALENDAR_PLUS_CALENDAR_PERPETUAL_H
#define CALENDAR_PLUS_CALENDAR_PERPETUAL_H

#include "calendar-internal.h"

G_BEGIN_DECLS

gint calendar_plus_fixed_month_length(gint64 year,
                                      gint month);
void calendar_plus_fixed_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_fixed_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);

gint calendar_plus_world_month_length(gint64 year,
                                      gint month);
void calendar_plus_world_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_world_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);

G_END_DECLS

#endif
