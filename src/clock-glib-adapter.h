// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_CLOCK_GLIB_ADAPTER_H
#define CALENDAR_PLUS_CLOCK_GLIB_ADAPTER_H

#include "clock-engine.h"

void calendar_plus_clock_glib_interfaces(
    CalendarPlusClockTimeSource *time_source,
    CalendarPlusClockScheduler *scheduler);

#endif
