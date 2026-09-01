/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * GVariant marshalling for CalendarSystem's GJS ABI.
 *
 * The tuple layouts are Cinnamon adapter contracts, not calendar-engine data
 * structures. Keeping them here lets other native front ends use
 * calendar-core.h without understanding GVariant or GObject Introspection.
 */

#include "calendar-system-private.h"

static GVariant *
date_to_variant(const CalendarPlusDate *date)
{
    return g_variant_ref_sink(
        g_variant_new("(iii)", date->year, date->month, date->day));
}

typedef gboolean (*DateOperation)(const CalendarPlusCalendarEngine *engine,
                                  const CalendarPlusDate *date,
                                  gint amount,
                                  CalendarPlusDate *result);

static gboolean
period_start_operation(const CalendarPlusCalendarEngine *engine,
                       const CalendarPlusDate *date,
                       gint amount,
                       CalendarPlusDate *result)
{
    (void)amount;
    return calendar_plus_calendar_engine_period_start(engine, date, result);
}

static GVariant *
navigate_to_variant(CalendarPlusCalendarSystem *self,
                    gint year,
                    gint month,
                    gint day,
                    gint amount,
                    DateOperation operation)
{
    const CalendarPlusDate input = { year, month, day };
    CalendarPlusDate result;
    CalendarPlusCalendarEngine *engine =
        calendar_plus_calendar_system_get_engine(self);

    if (engine == NULL || !operation(engine, &input, amount, &result))
        return NULL;
    return date_to_variant(&result);
}

GVariant *
calendar_plus_calendar_system_month_start_parts(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               0,
                               period_start_operation);
}

GVariant *
calendar_plus_calendar_system_add_months_parts(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               amount,
                               calendar_plus_calendar_engine_add_periods);
}

GVariant *
calendar_plus_calendar_system_add_years_parts(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_to_variant(self,
                               gregorian_year,
                               gregorian_month,
                               gregorian_day,
                               amount,
                               calendar_plus_calendar_engine_add_years);
}

GVariant *
calendar_plus_calendar_system_build_grid(
    CalendarPlusCalendarSystem *self,
    gint selected_year,
    gint selected_month,
    gint selected_day,
    gint today_year,
    gint today_month,
    gint today_day,
    gint week_start)
{
    const CalendarPlusDate selected = {
        selected_year, selected_month, selected_day
    };
    const CalendarPlusDate today = { today_year, today_month, today_day };
    CalendarPlusCalendarGrid grid;
    GVariantBuilder cells;
    guint index;

    if (!calendar_plus_calendar_engine_build_grid(
            calendar_plus_calendar_system_get_engine(self),
            &selected,
            &today,
            week_start,
            &grid))
    {
        return NULL;
    }

    g_variant_builder_init(&cells,
                           G_VARIANT_TYPE("a(siiiiiibbbbbb)"));
    for (index = 0; index < CALENDAR_PLUS_CALENDAR_GRID_CELLS; index++)
    {
        const CalendarPlusCalendarCell *cell = &grid.cells[index];

        g_variant_builder_add(&cells,
                              "(siiiiiibbbbbb)",
                              cell->day_label,
                              cell->date.year,
                              cell->date.month,
                              cell->date.day,
                              cell->row,
                              cell->column,
                              cell->week_number,
                              cell->is_work_day,
                              cell->is_today,
                              cell->is_selected,
                              cell->is_current_period,
                              cell->is_top_row,
                              cell->is_left_edge);
    }

    calendar_plus_calendar_grid_clear(&grid);
    return g_variant_ref_sink(g_variant_builder_end(&cells));
}
