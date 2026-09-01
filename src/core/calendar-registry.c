/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Built-in calendar-provider registry.
 *
 * Calendar identifiers are persisted in Cinnamon settings, so IDs are stable
 * data rather than implementation details.  Each provider exposes the same
 * backend-neutral operation table.  CalendarSystem resolves that interface
 * once and never branches on ICU versus native implementation afterwards.
 *
 * BuiltinCalendarProvider embeds the generic interface first. The built-in
 * adapters can therefore recover their private enum or ICU keyword while the
 * interface itself remains independent of that private state.
 */

#include "calendar-registry.h"

#include "calendar-custom.h"
#include "calendar-swedish.h"
#include "icu-calendar.h"

#include <glib/gi18n-lib.h>
#include <infiltratr/core.h>

typedef struct
{
    CalendarPlusCalendarProvider interface;
    CalendarPlusCalendarMode mode;
    const gchar *icu_keyword;
} BuiltinCalendarProvider;

static const BuiltinCalendarProvider *
builtin_provider(const CalendarPlusCalendarProvider *provider)
{
    /* The first-member invariant makes this container cast well-defined. */
    return (const BuiltinCalendarProvider *)provider;
}

static gboolean
icu_fields(const CalendarPlusCalendarProvider *provider,
           gint64 jdn,
           CalendarPlusCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_fields_from_jdn(
        builtin->icu_keyword, jdn, fields);
}

static gchar *
icu_format(const CalendarPlusCalendarProvider *provider,
           gint64 jdn,
           CalendarPlusDatePart part)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_format(
        builtin->mode, builtin->icu_keyword, jdn, part);
}

static gint64
icu_period_start(const CalendarPlusCalendarProvider *provider,
                 gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_month_start(builtin->icu_keyword, jdn);
}

static gint64
icu_add_periods(const CalendarPlusCalendarProvider *provider,
                gint64 jdn,
                gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_add_months(
        builtin->icu_keyword, jdn, amount);
}

static gint64
icu_add_years(const CalendarPlusCalendarProvider *provider,
              gint64 jdn,
              gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_icu_add_years(
        builtin->icu_keyword, jdn, amount);
}

static gboolean
custom_fields(const CalendarPlusCalendarProvider *provider,
              gint64 jdn,
              CalendarPlusCalendarFields *fields)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    calendar_plus_custom_fields_from_jdn(builtin->mode, jdn, fields);
    return TRUE;
}

static gchar *
custom_format(const CalendarPlusCalendarProvider *provider,
              gint64 jdn,
              CalendarPlusDatePart part)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);
    CalendarPlusCalendarFields fields;

    calendar_plus_custom_fields_from_jdn(builtin->mode, jdn, &fields);
    return calendar_plus_custom_format(builtin->mode, &fields, part);
}

static gint64
custom_period_start(const CalendarPlusCalendarProvider *provider,
                    gint64 jdn)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_custom_month_start(builtin->mode, jdn);
}

static gint64
custom_add_periods(const CalendarPlusCalendarProvider *provider,
                   gint64 jdn,
                   gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_custom_add_months(builtin->mode, jdn, amount);
}

static gint64
custom_add_years(const CalendarPlusCalendarProvider *provider,
                 gint64 jdn,
                 gint amount)
{
    const BuiltinCalendarProvider *builtin = builtin_provider(provider);

    return calendar_plus_custom_add_years(builtin->mode, jdn, amount);
}

static gboolean
swedish_fields(const CalendarPlusCalendarProvider *provider,
               gint64 jdn,
               CalendarPlusCalendarFields *fields)
{
    (void)provider;
    calendar_plus_swedish_fields_from_jdn(jdn, fields);
    return TRUE;
}

static gchar *
swedish_format(const CalendarPlusCalendarProvider *provider,
               gint64 jdn,
               CalendarPlusDatePart part)
{
    CalendarPlusCalendarFields fields;

    (void)provider;
    calendar_plus_swedish_fields_from_jdn(jdn, &fields);
    return calendar_plus_swedish_format(&fields, part);
}

static gint64
swedish_period_start(const CalendarPlusCalendarProvider *provider,
                     gint64 jdn)
{
    (void)provider;
    return calendar_plus_swedish_month_start(jdn);
}

static gint64
swedish_add_periods(const CalendarPlusCalendarProvider *provider,
                    gint64 jdn,
                    gint amount)
{
    (void)provider;
    return calendar_plus_swedish_add_months(jdn, amount);
}

static gint64
swedish_add_years(const CalendarPlusCalendarProvider *provider,
                  gint64 jdn,
                  gint amount)
{
    (void)provider;
    return calendar_plus_swedish_add_years(jdn, amount);
}

#define PROVIDER_INTERFACE(id_, name_, fields_, format_, start_, add_, years_) \
    { \
        CALENDAR_PLUS_CALENDAR_PROVIDER_ABI, \
        id_, N_(name_), fields_, format_, start_, add_, years_ \
    }

#define ICU_PROVIDER(mode_, id_, name_, keyword_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(id_, name_, \
                           icu_fields, icu_format, icu_period_start, \
                           icu_add_periods, icu_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, keyword_ \
    }

#define CUSTOM_PROVIDER(mode_, id_, name_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(id_, name_, \
                           custom_fields, custom_format, custom_period_start, \
                           custom_add_periods, custom_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, NULL \
    }

#define SWEDISH_PROVIDER(mode_, id_, name_) \
    [CALENDAR_PLUS_CALENDAR_MODE_##mode_] = { \
        PROVIDER_INTERFACE(id_, name_, \
                           swedish_fields, swedish_format, swedish_period_start, \
                           swedish_add_periods, swedish_add_years), \
        CALENDAR_PLUS_CALENDAR_MODE_##mode_, NULL \
    }

static const BuiltinCalendarProvider providers[] = {
    ICU_PROVIDER(GREGORIAN, "gregorian", "Gregorian", "gregorian"),
    CUSTOM_PROVIDER(JULIAN, "julian", "Julian"),
    CUSTOM_PROVIDER(ISO_WEEK, "iso-week", "ISO week calendar"),
    ICU_PROVIDER(HEBREW, "hebrew", "Hebrew", "hebrew"),
    ICU_PROVIDER(ISLAMIC, "islamic", "Islamic (astronomical approximation)", "islamic"),
    ICU_PROVIDER(ISLAMIC_CIVIL, "islamic-civil", "Islamic (civil/tabular)", "islamic-civil"),
    ICU_PROVIDER(ISLAMIC_UMM_AL_QURA, "islamic-umalqura", "Islamic (Umm al-Qura)", "islamic-umalqura"),
    ICU_PROVIDER(PERSIAN, "persian", "Persian (Solar Hijri)", "persian"),
    ICU_PROVIDER(CHINESE, "chinese", "Chinese traditional", "chinese"),
    ICU_PROVIDER(INDIAN, "indian", "Indian National (Saka)", "indian"),
    ICU_PROVIDER(COPTIC, "coptic", "Coptic", "coptic"),
    ICU_PROVIDER(ETHIOPIAN, "ethiopian", "Ethiopian", "ethiopic"),
    ICU_PROVIDER(BUDDHIST, "buddhist", "Buddhist", "buddhist"),
    ICU_PROVIDER(JAPANESE, "japanese", "Japanese imperial era", "japanese"),
    ICU_PROVIDER(MINGUO, "minguo", "Minguo (Republic of China)", "roc"),
    CUSTOM_PROVIDER(FRENCH_REPUBLICAN, "french-republican", "French Republican"),
    CUSTOM_PROVIDER(ROMAN, "roman", "Roman"),
    CUSTOM_PROVIDER(MAYAN, "mayan", "Mayan Long Count"),
    CUSTOM_PROVIDER(BAHAI, "bahai", "Bahá’í (Badíʿ)"),
    CUSTOM_PROVIDER(INTERNATIONAL_FIXED, "international-fixed", "International Fixed"),
    CUSTOM_PROVIDER(WORLD, "world", "World Calendar"),
    CUSTOM_PROVIDER(POSITIVIST, "positivist", "Positivist"),
    CUSTOM_PROVIDER(REVISED_JULIAN, "revised-julian", "Revised Julian"),
    CUSTOM_PROVIDER(BYZANTINE, "byzantine", "Byzantine (Anno Mundi)"),
    CUSTOM_PROVIDER(EGYPTIAN_NABONASSAR, "egyptian-nabonassar",
                    "Egyptian civil (Nabonassar era)"),
    ICU_PROVIDER(DANGI, "dangi", "Dangi (traditional Korean)", "dangi"),
    ICU_PROVIDER(ETHIOPIC_AMETE_ALEM, "ethiopic-amete-alem",
                 "Ethiopic (Amete Alem)", "ethiopic-amete-alem"),
    ICU_PROVIDER(ISLAMIC_TBLA, "islamic-tbla",
                 "Islamic (tabular, astronomical epoch)", "islamic-tbla"),
    CUSTOM_PROVIDER(ARMENIAN_TRADITIONAL, "armenian-traditional",
                    "Armenian traditional (365-day)"),
    SWEDISH_PROVIDER(SWEDISH_HISTORICAL, "swedish-historical",
                     "Swedish historical (1700–1753)")
};

G_STATIC_ASSERT(G_N_ELEMENTS(providers) == CALENDAR_PLUS_CALENDAR_MODE_COUNT);

const CalendarPlusCalendarProvider *
calendar_plus_calendar_provider_from_id(const gchar *calendar_id)
{
    guint index;

    for (index = 0; index < G_N_ELEMENTS(providers); index++)
    {
        const CalendarPlusCalendarProvider *provider =
            &providers[index].interface;

        if (infiltratr_string_equal(calendar_id, provider->id))
            return provider;
    }

    return NULL;
}

gsize
calendar_plus_calendar_provider_get_count(void)
{
    return G_N_ELEMENTS(providers);
}

const CalendarPlusCalendarProvider *
calendar_plus_calendar_provider_at(gsize index)
{
    if (index >= G_N_ELEMENTS(providers))
        return NULL;

    return &providers[index].interface;
}
