/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private calendar-engine types.
 *
 * This header is deliberately excluded from GObject Introspection.  The
 * presentation layer sees the stable CalendarSystem API; native modules use
 * these types to exchange values without stringly typed or layout-dependent
 * contracts.
 */

#ifndef CALENDAR_PLUS_CALENDAR_INTERNAL_H
#define CALENDAR_PLUS_CALENDAR_INTERNAL_H

#include "calendar-core.h"

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN,
    CALENDAR_PLUS_CALENDAR_MODE_JULIAN,
    CALENDAR_PLUS_CALENDAR_MODE_ISO_WEEK,
    CALENDAR_PLUS_CALENDAR_MODE_HEBREW,
    CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC,
    CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL,
    CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA,
    CALENDAR_PLUS_CALENDAR_MODE_PERSIAN,
    CALENDAR_PLUS_CALENDAR_MODE_CHINESE,
    CALENDAR_PLUS_CALENDAR_MODE_INDIAN,
    CALENDAR_PLUS_CALENDAR_MODE_COPTIC,
    CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN,
    CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST,
    CALENDAR_PLUS_CALENDAR_MODE_JAPANESE,
    CALENDAR_PLUS_CALENDAR_MODE_MINGUO,
    CALENDAR_PLUS_CALENDAR_MODE_FRENCH_REPUBLICAN,
    CALENDAR_PLUS_CALENDAR_MODE_ROMAN,
    CALENDAR_PLUS_CALENDAR_MODE_MAYAN,
    CALENDAR_PLUS_CALENDAR_MODE_BAHAI,
    CALENDAR_PLUS_CALENDAR_MODE_INTERNATIONAL_FIXED,
    CALENDAR_PLUS_CALENDAR_MODE_WORLD,
    CALENDAR_PLUS_CALENDAR_MODE_POSITIVIST,
    CALENDAR_PLUS_CALENDAR_MODE_REVISED_JULIAN,
    CALENDAR_PLUS_CALENDAR_MODE_BYZANTINE,
    CALENDAR_PLUS_CALENDAR_MODE_EGYPTIAN_NABONASSAR,
    CALENDAR_PLUS_CALENDAR_MODE_DANGI,
    CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM,
    CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA,
    CALENDAR_PLUS_CALENDAR_MODE_ARMENIAN_TRADITIONAL,
    CALENDAR_PLUS_CALENDAR_MODE_SWEDISH_HISTORICAL,
    CALENDAR_PLUS_CALENDAR_MODE_COUNT
} CalendarPlusCalendarMode;

/*
 * A neutral calendar tuple.  "auxiliary" distinguishes eras and leap-month
 * cycles when year/month/day alone are ambiguous.  "special" identifies an
 * intercalary day or leap month.  The invariant used by period comparison is:
 *
 *   same period iff auxiliary, year, month and special all compare equal.
 */
typedef struct
{
    gint64 year;
    gint month;
    gint day;
    gint auxiliary;
    gboolean special;
} CalendarPlusCalendarFields;

enum
{
    CALENDAR_PLUS_DAYS_PER_WEEK = 7
};

/*
 * Testable internal variant of the public grid builder. Production callers
 * pass the process locale through calendar_plus_calendar_engine_build_grid();
 * deterministic regression tests may inject a CLDR locale explicitly.
 */
gboolean calendar_plus_calendar_engine_build_grid_for_locale(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *selected,
    const CalendarPlusDate *today,
    gint week_start,
    const gchar *locale,
    CalendarPlusCalendarGrid *grid);

G_END_DECLS

#endif
