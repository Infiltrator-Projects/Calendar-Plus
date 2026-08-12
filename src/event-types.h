// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_EVENT_TYPES_H
#define CALENDAR_PLUS_EVENT_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * CalendarPlusEventState:
 * @CALENDAR_PLUS_EVENT_STATE_INVALID: end precedes start
 * @CALENDAR_PLUS_EVENT_STATE_PAST: inclusive end is earlier than now
 * @CALENDAR_PLUS_EVENT_STATE_FUTURE: inclusive start is later than now
 * @CALENDAR_PLUS_EVENT_STATE_PRESENT: now lies inside the inclusive interval
 */
typedef enum
{
    CALENDAR_PLUS_EVENT_STATE_INVALID = 0,
    CALENDAR_PLUS_EVENT_STATE_PAST,
    CALENDAR_PLUS_EVENT_STATE_FUTURE,
    CALENDAR_PLUS_EVENT_STATE_PRESENT
} CalendarPlusEventState;

/**
 * CalendarPlusEventDayRelation:
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_NONE: no boundary relation
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_STARTS_ON_DAY: start is on comparison day
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_ON_DAY: end is on comparison day
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_BEFORE_DAY: start day precedes the
 *   comparison day
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_ENDED_BEFORE_DAY: end day precedes the
 *   comparison day
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_AFTER_DAY: end day follows the
 *   comparison day
 * @CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_AFTER_DAY: start day follows the
 *   comparison day
 *
 * Bit flags comparing local-midnight event boundaries with one local day.
 */
typedef enum
{
    CALENDAR_PLUS_EVENT_DAY_RELATION_NONE = 0,
    CALENDAR_PLUS_EVENT_DAY_RELATION_STARTS_ON_DAY = 1 << 0,
    CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_ON_DAY = 1 << 1,
    CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_BEFORE_DAY = 1 << 2,
    CALENDAR_PLUS_EVENT_DAY_RELATION_ENDED_BEFORE_DAY = 1 << 3,
    CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_AFTER_DAY = 1 << 4,
    CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_AFTER_DAY = 1 << 5
} CalendarPlusEventDayRelation;

G_END_DECLS

#endif
