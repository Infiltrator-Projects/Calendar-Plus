// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_EVENT_STORE_PRIVATE_H
#define CALENDAR_PLUS_EVENT_STORE_PRIVATE_H

#include "event-core.h"
#include "event-store.h"

CalendarPlusEventIndex *calendar_plus_event_store_get_index(
    CalendarPlusEventStore *self);

#endif
