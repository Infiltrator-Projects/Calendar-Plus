// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_CALENDAR_REGISTRY_H
#define CALENDAR_PLUS_CALENDAR_REGISTRY_H

#include "calendar-internal.h"
#include "calendar-types.h"

G_BEGIN_DECLS

/*
 * Internal calendar-provider ABI.
 *
 * The interface is intentionally independent of CalendarPlusCalendarMode and
 * any ICU/native backend choice.  A provider implementation may embed this
 * structure as the first member of a larger private structure; callbacks then
 * receive the interface address and recover their private implementation data
 * without exposing a built-in enum or backend-specific field to core callers.
 */
typedef struct _CalendarPlusCalendarProvider CalendarPlusCalendarProvider;

typedef gboolean (*CalendarPlusProviderFieldsFunc)(
    const CalendarPlusCalendarProvider *provider,
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
typedef gchar *(*CalendarPlusProviderFormatFunc)(
    const CalendarPlusCalendarProvider *provider,
    gint64 jdn,
    CalendarPlusDatePart part);
typedef gint64 (*CalendarPlusProviderNavigateFunc)(
    const CalendarPlusCalendarProvider *provider,
    gint64 jdn,
    gint amount);
typedef gint64 (*CalendarPlusProviderPeriodStartFunc)(
    const CalendarPlusCalendarProvider *provider,
    gint64 jdn);

struct _CalendarPlusCalendarProvider
{
    guint abi_version;
    const gchar *id;
    const gchar *name;
    CalendarPlusProviderFieldsFunc fields_from_jdn;
    CalendarPlusProviderFormatFunc format;
    CalendarPlusProviderPeriodStartFunc period_start;
    CalendarPlusProviderNavigateFunc add_periods;
    CalendarPlusProviderNavigateFunc add_years;
};

enum
{
    CALENDAR_PLUS_CALENDAR_PROVIDER_ABI = 1
};

const CalendarPlusCalendarProvider *calendar_plus_calendar_provider_from_id(
    const gchar *calendar_id);
gsize calendar_plus_calendar_provider_get_count(void);
const CalendarPlusCalendarProvider *calendar_plus_calendar_provider_at(
    gsize index);

G_END_DECLS

#endif
