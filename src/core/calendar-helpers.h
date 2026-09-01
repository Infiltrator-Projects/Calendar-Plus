// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_CALENDAR_HELPERS_H
#define CALENDAR_PLUS_CALENDAR_HELPERS_H

#include "calendar-internal.h"

G_BEGIN_DECLS

gint64 calendar_plus_count_multiples_inclusive(gint64 first,
                                               gint64 last,
                                               gint64 divisor);
gchar *calendar_plus_format_named_date(
    const CalendarPlusCalendarFields *fields,
    CalendarPlusDatePart part,
    const gchar *const *months,
    const gchar *year_suffix);

G_END_DECLS

#endif
