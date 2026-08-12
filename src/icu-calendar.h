// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_ICU_CALENDAR_H
#define CALENDAR_PLUS_ICU_CALENDAR_H

#include "calendar-internal.h"
#include "calendar-types.h"

G_BEGIN_DECLS

gboolean calendar_plus_icu_fields_from_jdn(
    const gchar *calendar_keyword,
    gint64 jdn,
    CalendarPlusCalendarFields *fields);
gchar *calendar_plus_icu_format(CalendarPlusCalendarMode format_profile,
                                const gchar *calendar_keyword,
                                gint64 jdn,
                                CalendarPlusDatePart part);
gint64 calendar_plus_icu_month_start(const gchar *calendar_keyword,
                                     gint64 jdn);
gint64 calendar_plus_icu_add_months(const gchar *calendar_keyword,
                                    gint64 jdn,
                                    gint amount);
gint64 calendar_plus_icu_add_years(const gchar *calendar_keyword,
                                   gint64 jdn,
                                   gint amount);

G_END_DECLS

#endif
