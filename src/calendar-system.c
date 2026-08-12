// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * GObject facade for the platform-neutral calendar engine.
 *
 * This file owns object lifetime, translated provider names and the legacy
 * string API.  GVariant marshalling lives in calendar-gvariant-adapter.c;
 * conversion, navigation and grid semantics live in calendar-core.c.
 */

#include "calendar-system-private.h"

#include "julian-day.h"

#include <glib/gi18n-lib.h>
#include <infiltratr/core.h>

struct _CalendarPlusCalendarSystem
{
    GObject parent_instance;

    CalendarPlusCalendarEngine *engine;
};

G_DEFINE_TYPE(CalendarPlusCalendarSystem,
              calendar_plus_calendar_system,
              G_TYPE_OBJECT)

static void
calendar_plus_calendar_system_dispose(GObject *object)
{
    CalendarPlusCalendarSystem *self =
        CALENDAR_PLUS_CALENDAR_SYSTEM(object);

    g_clear_pointer(&self->engine, calendar_plus_calendar_engine_free);
    G_OBJECT_CLASS(calendar_plus_calendar_system_parent_class)->dispose(object);
}

static void
calendar_plus_calendar_system_class_init(
    CalendarPlusCalendarSystemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = calendar_plus_calendar_system_dispose;
}

static void
calendar_plus_calendar_system_init(CalendarPlusCalendarSystem *self)
{
    self->engine = calendar_plus_calendar_engine_new("gregorian");
}

CalendarPlusCalendarSystem *
calendar_plus_calendar_system_new(const gchar *calendar_id)
{
    CalendarPlusCalendarEngine *engine =
        calendar_plus_calendar_engine_new(calendar_id);
    CalendarPlusCalendarSystem *self;

    if (engine == NULL)
        return NULL;

    self = g_object_new(CALENDAR_PLUS_TYPE_CALENDAR_SYSTEM, NULL);
    g_clear_pointer(&self->engine, calendar_plus_calendar_engine_free);
    self->engine = engine;
    return self;
}

CalendarPlusCalendarEngine *
calendar_plus_calendar_system_get_engine(CalendarPlusCalendarSystem *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_CALENDAR_SYSTEM(self), NULL);
    return self->engine;
}

const gchar *
calendar_plus_calendar_system_get_id(CalendarPlusCalendarSystem *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_CALENDAR_SYSTEM(self), "");
    return calendar_plus_calendar_engine_get_id(self->engine);
}

const gchar *
calendar_plus_calendar_system_get_name(CalendarPlusCalendarSystem *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_CALENDAR_SYSTEM(self), "");
    return _(calendar_plus_calendar_engine_get_name(self->engine));
}

static CalendarPlusDatePart
date_part_from_string(const gchar *part)
{
    if (infiltratr_string_equal(part, "day"))
        return CALENDAR_PLUS_DATE_PART_DAY;
    if (infiltratr_string_equal(part, "month"))
        return CALENDAR_PLUS_DATE_PART_MONTH;
    if (infiltratr_string_equal(part, "year"))
        return CALENDAR_PLUS_DATE_PART_YEAR;
    if (infiltratr_string_equal(part, "short"))
        return CALENDAR_PLUS_DATE_PART_SHORT;
    if (infiltratr_string_equal(part, "full"))
        return CALENDAR_PLUS_DATE_PART_FULL;
    return CALENDAR_PLUS_DATE_PART_INVALID;
}

gchar *
calendar_plus_calendar_system_format_date(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    const gchar *part)
{
    return calendar_plus_calendar_system_format_date_part(
        self,
        gregorian_year,
        gregorian_month,
        gregorian_day,
        date_part_from_string(part));
}

gchar *
calendar_plus_calendar_system_format_date_part(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    CalendarPlusDatePart part)
{
    const CalendarPlusDate date = {
        gregorian_year, gregorian_month, gregorian_day
    };

    g_return_val_if_fail(CALENDAR_PLUS_IS_CALENDAR_SYSTEM(self),
                         g_strdup(""));
    return calendar_plus_calendar_engine_format_date(self->engine,
                                                     &date,
                                                     part);
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

static gchar *
navigate_as_iso(CalendarPlusCalendarSystem *self,
                gint year,
                gint month,
                gint day,
                gint amount,
                DateOperation operation)
{
    const CalendarPlusDate input = { year, month, day };
    CalendarPlusDate result;

    g_return_val_if_fail(CALENDAR_PLUS_IS_CALENDAR_SYSTEM(self),
                         g_strdup(""));
    if (!operation(self->engine, &input, amount, &result))
        return g_strdup("");

    return calendar_plus_format_iso_date(
        calendar_plus_gregorian_to_jdn(result.year,
                                       result.month,
                                       result.day));
}

gchar *
calendar_plus_calendar_system_month_start(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day)
{
    return navigate_as_iso(self,
                           gregorian_year,
                           gregorian_month,
                           gregorian_day,
                           0,
                           period_start_operation);
}

gchar *
calendar_plus_calendar_system_add_months(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_as_iso(self,
                           gregorian_year,
                           gregorian_month,
                           gregorian_day,
                           amount,
                           calendar_plus_calendar_engine_add_periods);
}

gchar *
calendar_plus_calendar_system_add_years(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day,
    gint amount)
{
    return navigate_as_iso(self,
                           gregorian_year,
                           gregorian_month,
                           gregorian_day,
                           amount,
                           calendar_plus_calendar_engine_add_years);
}

gchar *
calendar_plus_calendar_system_month_key(
    CalendarPlusCalendarSystem *self,
    gint gregorian_year,
    gint gregorian_month,
    gint gregorian_day)
{
    const CalendarPlusDate date = {
        gregorian_year, gregorian_month, gregorian_day
    };

    g_return_val_if_fail(CALENDAR_PLUS_IS_CALENDAR_SYSTEM(self),
                         g_strdup(""));
    return calendar_plus_calendar_engine_period_key(self->engine, &date);
}
