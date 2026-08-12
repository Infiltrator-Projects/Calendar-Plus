// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_CALENDAR_TYPES_H
#define CALENDAR_PLUS_CALENDAR_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * CalendarPlusDatePart:
 * @CALENDAR_PLUS_DATE_PART_INVALID: invalid date component
 * @CALENDAR_PLUS_DATE_PART_DAY: displayed day within the selected period
 * @CALENDAR_PLUS_DATE_PART_MONTH: period or month heading
 * @CALENDAR_PLUS_DATE_PART_YEAR: year or era heading
 * @CALENDAR_PLUS_DATE_PART_SHORT: compact complete date
 * @CALENDAR_PLUS_DATE_PART_FULL: complete date including weekday where known
 *
 * Calendar-format selectors shared by the portable engine and presentation
 * adapters.  Keeping the enum outside the GObject facade prevents the core
 * from acquiring a presentation dependency merely to name a date component.
 */
typedef enum
{
    CALENDAR_PLUS_DATE_PART_INVALID = 0,
    CALENDAR_PLUS_DATE_PART_DAY,
    CALENDAR_PLUS_DATE_PART_MONTH,
    CALENDAR_PLUS_DATE_PART_YEAR,
    CALENDAR_PLUS_DATE_PART_SHORT,
    CALENDAR_PLUS_DATE_PART_FULL
} CalendarPlusDatePart;

G_END_DECLS

#endif
