/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private deterministic algorithms for additional historical calendars.
 */

#ifndef CALENDAR_PLUS_CALENDAR_HISTORICAL_H
#define CALENDAR_PLUS_CALENDAR_HISTORICAL_H

#include "calendar-internal.h"
#include "calendar-types.h"

G_BEGIN_DECLS

void calendar_plus_revised_julian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_revised_julian_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);
gint calendar_plus_revised_julian_month_length(
    const CalendarPlusCalendarFields *fields);
gchar *calendar_plus_revised_julian_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);

void calendar_plus_byzantine_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_byzantine_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);
gint calendar_plus_byzantine_month_length(
    const CalendarPlusCalendarFields *fields);
gint calendar_plus_byzantine_period_index(gint month);
gint calendar_plus_byzantine_month_from_period(gint period);
gchar *calendar_plus_byzantine_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);

void calendar_plus_egyptian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_egyptian_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);
gint calendar_plus_egyptian_month_length(
    const CalendarPlusCalendarFields *fields);
gchar *calendar_plus_egyptian_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);

void calendar_plus_armenian_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gint64 calendar_plus_armenian_fields_to_jdn(
    const CalendarPlusCalendarFields *fields);
gint calendar_plus_armenian_month_length(
    const CalendarPlusCalendarFields *fields);
gchar *calendar_plus_armenian_format(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part);

G_END_DECLS

#endif
