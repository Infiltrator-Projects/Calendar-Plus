// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Calendar conversion, navigation and six-row grid modelling.
 *
 * All externally visible results use ordinary C records.  Presentation
 * adapters are responsible for marshalling those records to GVariant, Win32,
 * another toolkit, or any later front end.  The engine resolves one provider
 * at construction and never branches on a provider's implementation family.
 */

#include "calendar-core.h"

#include "calendar-custom.h"
#include "calendar-internal.h"
#include "calendar-registry.h"
#include "icu-calendar.h"
#include "julian-day.h"

struct _CalendarPlusCalendarEngine
{
    const CalendarPlusCalendarProvider *provider;
};

static gboolean
date_to_jdn(const CalendarPlusDate *date,
            gint64 *jdn)
{
    if (!calendar_plus_date_is_valid(date) || jdn == NULL)
        return FALSE;

    *jdn = calendar_plus_gregorian_to_jdn(date->year,
                                          date->month,
                                          date->day);
    return TRUE;
}

static gboolean
date_from_jdn(gint64 jdn,
              CalendarPlusDate *date)
{
    if (date == NULL)
        return FALSE;

    calendar_plus_jdn_to_gregorian(jdn,
                                   &date->year,
                                   &date->month,
                                   &date->day);
    return calendar_plus_date_is_valid(date);
}

gboolean
calendar_plus_date_is_valid(const CalendarPlusDate *date)
{
    return date != NULL &&
           calendar_plus_gregorian_date_is_valid(date->year,
                                                  date->month,
                                                  date->day);
}

gboolean
calendar_plus_date_same(gint year_a,
                        gint month_a,
                        gint day_a,
                        gint year_b,
                        gint month_b,
                        gint day_b)
{
    const CalendarPlusDate left = { year_a, month_a, day_a };
    const CalendarPlusDate right = { year_b, month_b, day_b };

    return calendar_plus_date_is_valid(&left) &&
           calendar_plus_date_is_valid(&right) &&
           year_a == year_b && month_a == month_b && day_a == day_b;
}

static gboolean
iso_weekday_is_work_day_for_locale(gint iso_weekday,
                                   const gchar *locale)
{
    gboolean locale_known = FALSE;
    const gboolean locale_workday = locale != NULL ?
        calendar_plus_icu_is_work_day_for_locale(locale,
                                                 iso_weekday,
                                                 &locale_known) :
        calendar_plus_icu_is_work_day(iso_weekday, &locale_known);

    /*
     * ICU/CLDR is authoritative for locale weekend policy.  The fallback is
     * deliberately conventional rather than guessed from country codes so a
     * missing ICU locale never produces an invented regional rule.
     */
    return locale_known ? locale_workday :
           iso_weekday >= 1 && iso_weekday <= 5;
}

static gboolean
iso_weekday_is_work_day(gint iso_weekday)
{
    return iso_weekday_is_work_day_for_locale(iso_weekday, NULL);
}

gboolean
calendar_plus_date_is_work_day(gint year,
                               gint month,
                               gint day)
{
    const CalendarPlusDate date = { year, month, day };

    if (!calendar_plus_date_is_valid(&date))
        return FALSE;

    return iso_weekday_is_work_day(calendar_plus_iso_weekday(
        calendar_plus_gregorian_to_jdn(year, month, day)));
}

CalendarPlusCalendarEngine *
calendar_plus_calendar_engine_new(const gchar *calendar_id)
{
    const CalendarPlusCalendarProvider *provider =
        calendar_plus_calendar_provider_from_id(calendar_id);
    CalendarPlusCalendarEngine *engine;

    if (provider == NULL ||
        provider->abi_version != CALENDAR_PLUS_CALENDAR_PROVIDER_ABI)
    {
        return NULL;
    }

    engine = g_new0(CalendarPlusCalendarEngine, 1);
    engine->provider = provider;
    return engine;
}

void
calendar_plus_calendar_engine_free(CalendarPlusCalendarEngine *engine)
{
    g_free(engine);
}

const gchar *
calendar_plus_calendar_engine_get_id(
    const CalendarPlusCalendarEngine *engine)
{
    return engine != NULL && engine->provider != NULL ?
           engine->provider->id : "";
}

const gchar *
calendar_plus_calendar_engine_get_name(
    const CalendarPlusCalendarEngine *engine)
{
    return engine != NULL && engine->provider != NULL ?
           engine->provider->name : "";
}

gchar *
calendar_plus_calendar_engine_format_date(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    CalendarPlusDatePart part)
{
    gint64 jdn;

    if (engine == NULL || engine->provider == NULL ||
        !date_to_jdn(date, &jdn) ||
        part <= CALENDAR_PLUS_DATE_PART_INVALID ||
        part > CALENDAR_PLUS_DATE_PART_FULL)
    {
        return g_strdup("");
    }

    return engine->provider->format(engine->provider, jdn, part);
}

typedef gint64 (*NavigateFunc)(const CalendarPlusCalendarProvider *provider,
                               gint64 jdn,
                               gint amount);

static gboolean
navigate(const CalendarPlusCalendarEngine *engine,
         const CalendarPlusDate *date,
         gint amount,
         NavigateFunc operation,
         CalendarPlusDate *result)
{
    gint64 jdn;

    if (engine == NULL || engine->provider == NULL || operation == NULL ||
        !date_to_jdn(date, &jdn))
    {
        return FALSE;
    }

    return date_from_jdn(operation(engine->provider, jdn, amount), result);
}

static gint64
period_start_adapter(const CalendarPlusCalendarProvider *provider,
                     gint64 jdn,
                     gint amount)
{
    (void)amount;
    return provider->period_start(provider, jdn);
}

gboolean
calendar_plus_calendar_engine_period_start(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    CalendarPlusDate *result)
{
    return navigate(engine, date, 0, period_start_adapter, result);
}

gboolean
calendar_plus_calendar_engine_add_periods(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    gint amount,
    CalendarPlusDate *result)
{
    return navigate(engine, date, amount,
                    engine != NULL && engine->provider != NULL ?
                    engine->provider->add_periods : NULL,
                    result);
}

gboolean
calendar_plus_calendar_engine_add_years(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date,
    gint amount,
    CalendarPlusDate *result)
{
    return navigate(engine, date, amount,
                    engine != NULL && engine->provider != NULL ?
                    engine->provider->add_years : NULL,
                    result);
}

gchar *
calendar_plus_calendar_engine_period_key(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *date)
{
    CalendarPlusCalendarFields fields;
    gint64 jdn;

    if (engine == NULL || engine->provider == NULL ||
        !date_to_jdn(date, &jdn) ||
        !engine->provider->fields_from_jdn(engine->provider, jdn, &fields))
    {
        return g_strdup("");
    }

    return g_strdup_printf("%d:%" G_GINT64_FORMAT ":%d:%d",
                           fields.auxiliary,
                           fields.year,
                           fields.month,
                           fields.special ? 1 : 0);
}

static gboolean
fields_share_period(const CalendarPlusCalendarFields *left,
                    const CalendarPlusCalendarFields *right)
{
    return left->auxiliary == right->auxiliary &&
           left->year == right->year &&
           left->month == right->month &&
           left->special == right->special;
}

void
calendar_plus_calendar_grid_clear(CalendarPlusCalendarGrid *grid)
{
    guint index;

    if (grid == NULL)
        return;

    for (index = 0; index < CALENDAR_PLUS_CALENDAR_GRID_CELLS; index++)
        g_clear_pointer(&grid->cells[index].day_label, g_free);
    *grid = (CalendarPlusCalendarGrid){ 0 };
}

gboolean
calendar_plus_calendar_engine_build_grid_for_locale(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *selected,
    const CalendarPlusDate *today,
    gint week_start,
    const gchar *locale,
    CalendarPlusCalendarGrid *grid)
{
    CalendarPlusCalendarFields selected_fields;
    gint64 selected_jdn;
    gint64 today_jdn;
    gint64 period_start;
    gint64 first_cell;
    gint start_weekday;
    guint index;

    if (grid == NULL)
        return FALSE;
    *grid = (CalendarPlusCalendarGrid){ 0 };

    if (engine == NULL || engine->provider == NULL ||
        week_start < 0 || week_start >= CALENDAR_PLUS_DAYS_PER_WEEK ||
        !date_to_jdn(selected, &selected_jdn) ||
        !date_to_jdn(today, &today_jdn) ||
        !engine->provider->fields_from_jdn(engine->provider,
                                           selected_jdn,
                                           &selected_fields))
    {
        return FALSE;
    }

    period_start = engine->provider->period_start(engine->provider,
                                                   selected_jdn);
    start_weekday = calendar_plus_iso_weekday(period_start) %
                    CALENDAR_PLUS_DAYS_PER_WEEK;
    first_cell = period_start - calendar_plus_positive_modulo(
        start_weekday - week_start, CALENDAR_PLUS_DAYS_PER_WEEK);

    for (index = 0; index < CALENDAR_PLUS_CALENDAR_GRID_CELLS; index++)
    {
        CalendarPlusCalendarCell *cell = &grid->cells[index];
        const gint64 cell_jdn = first_cell + (gint64)index;
        const gint iso_weekday = calendar_plus_iso_weekday(cell_jdn);
        const gint weekday = iso_weekday % CALENDAR_PLUS_DAYS_PER_WEEK;
        CalendarPlusCalendarFields fields;

        if (!date_from_jdn(cell_jdn, &cell->date) ||
            !engine->provider->fields_from_jdn(engine->provider,
                                               cell_jdn,
                                               &fields))
        {
            calendar_plus_calendar_grid_clear(grid);
            return FALSE;
        }

        cell->day_label = engine->provider->format(
            engine->provider, cell_jdn, CALENDAR_PLUS_DATE_PART_DAY);
        if (cell->day_label == NULL)
        {
            calendar_plus_calendar_grid_clear(grid);
            return FALSE;
        }

        cell->row = 2 + (gint)(index / CALENDAR_PLUS_DAYS_PER_WEEK);
        cell->column = (gint)(index % CALENDAR_PLUS_DAYS_PER_WEEK);
        cell->week_number = weekday == 4 ?
            calendar_plus_custom_iso_week_number(cell_jdn) : 0;
        cell->is_work_day =
            iso_weekday_is_work_day_for_locale(iso_weekday, locale);
        cell->is_today = cell_jdn == today_jdn;
        cell->is_selected = cell_jdn == selected_jdn;
        cell->is_current_period = fields_share_period(&selected_fields,
                                                      &fields);
        cell->is_top_row = index < CALENDAR_PLUS_DAYS_PER_WEEK;
        cell->is_left_edge = weekday == week_start;
    }

    return TRUE;
}

gboolean
calendar_plus_calendar_engine_build_grid(
    const CalendarPlusCalendarEngine *engine,
    const CalendarPlusDate *selected,
    const CalendarPlusDate *today,
    gint week_start,
    CalendarPlusCalendarGrid *grid)
{
    return calendar_plus_calendar_engine_build_grid_for_locale(
        engine, selected, today, week_start, NULL, grid);
}

gsize
calendar_plus_calendar_catalogue_get_count(void)
{
    return calendar_plus_calendar_provider_get_count();
}

gboolean
calendar_plus_calendar_catalogue_get(
    gsize index,
    CalendarPlusCalendarDescriptor *descriptor)
{
    const CalendarPlusCalendarProvider *provider;

    if (descriptor == NULL)
        return FALSE;

    provider = calendar_plus_calendar_provider_at(index);
    if (provider == NULL)
        return FALSE;

    descriptor->id = provider->id;
    descriptor->name = provider->name;
    return TRUE;
}
