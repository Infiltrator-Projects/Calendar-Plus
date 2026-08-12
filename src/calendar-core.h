/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Platform-neutral calendar engine contract.
 *
 * This header deliberately depends on GLib base types only.  It contains no
 * GObject, GVariant, Cinnamon or operating-system UI types, so another front
 * end can consume the same calendar model without adopting the GJS adapter.
 */

#ifndef CALENDAR_PLUS_CALENDAR_CORE_H
#define CALENDAR_PLUS_CALENDAR_CORE_H

#include "calendar-types.h"

G_BEGIN_DECLS

enum
{
    CALENDAR_PLUS_CALENDAR_GRID_CELLS = 42
};

typedef struct _CalendarPlusCalendarEngine CalendarPlusCalendarEngine;

/* Proleptic Gregorian civil date used at every platform boundary. */
typedef struct
{
    gint year;
    gint month;
    gint day;
} CalendarPlusDate;

typedef struct
{
    /* Owned UTF-8 label. calendar_plus_calendar_grid_clear() releases it. */
    gchar *day_label;
    /* Absolute proleptic-Gregorian coordinate for event and selection use. */
    CalendarPlusDate date;
    /* Table row 2..7 and logical weekday column 0..6. */
    gint row;
    gint column;
    /* ISO week number on the row's Thursday cell; zero on other cells. */
    gint week_number;
    gboolean is_work_day;
    gboolean is_today;
    gboolean is_selected;
    gboolean is_current_period;
    gboolean is_top_row;
    gboolean is_left_edge;
} CalendarPlusCalendarCell;

typedef struct
{
    CalendarPlusCalendarCell cells[CALENDAR_PLUS_CALENDAR_GRID_CELLS];
} CalendarPlusCalendarGrid;

typedef struct
{
    const gchar *id;
    const gchar *name;
} CalendarPlusCalendarDescriptor;

/* Returns an owned engine, or NULL for an unknown stable calendar id. */
CalendarPlusCalendarEngine *calendar_plus_calendar_engine_new(
    const gchar *calendar_id);
/* Accepts NULL. */
void calendar_plus_calendar_engine_free(CalendarPlusCalendarEngine *engine);

/* Returned strings are borrowed, process-lifetime provider metadata. */
const gchar *calendar_plus_calendar_engine_get_id(
    const CalendarPlusCalendarEngine *engine);
const gchar *calendar_plus_calendar_engine_get_name(
    const CalendarPlusCalendarEngine *engine);

/* Returns a newly allocated UTF-8 string; invalid input returns "". */
gchar *calendar_plus_calendar_engine_format_date(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    CalendarPlusDatePart part);
/* Navigation functions write @result only on success. */
gboolean calendar_plus_calendar_engine_period_start(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    CalendarPlusDate *result);
gboolean calendar_plus_calendar_engine_add_periods(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    gint amount,
    CalendarPlusDate *result);
gboolean calendar_plus_calendar_engine_add_years(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    gint amount,
    CalendarPlusDate *result);
/* Returns an allocated stable comparison key; invalid input returns "". */
gchar *calendar_plus_calendar_engine_period_key(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date);

/*
 * Fills all 42 cells on success.  Each day_label is owned by @grid; call
 * calendar_plus_calendar_grid_clear() when the model is no longer needed.
 * Clearing a completed or zeroed grid more than once is safe. @week_start uses
 * 0=Sunday through 6=Saturday; values outside that range fail.
 */
gboolean calendar_plus_calendar_engine_build_grid(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *selected,
    const CalendarPlusDate *today,
    gint week_start,
    CalendarPlusCalendarGrid *grid);
void calendar_plus_calendar_grid_clear(CalendarPlusCalendarGrid *grid);

/* Catalogue descriptors borrow immutable provider id/name strings. */
gsize calendar_plus_calendar_catalogue_get_count(void);
gboolean calendar_plus_calendar_catalogue_get(
    gsize index,
    CalendarPlusCalendarDescriptor *descriptor);

gboolean calendar_plus_date_is_valid(const CalendarPlusDate *date);
gboolean calendar_plus_date_same(gint year_a,
                                 gint month_a,
                                 gint day_a,
                                 gint year_b,
                                 gint month_b,
                                 gint day_b);
gboolean calendar_plus_date_is_work_day(gint year,
                                        gint month,
                                        gint day);

G_END_DECLS

#endif
