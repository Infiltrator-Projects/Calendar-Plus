// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Built-in non-ICU calendar providers.
 *
 * Every custom calendar is described by one immutable operation table.  The
 * public CalendarSystem facade never needs to know whether a period is a
 * Julian month, ISO week, Mayan uinal or Badíʿ month: it asks the provider to
 * convert JDN <-> fields, format the fields and navigate periods.
 *
 * The table is intentionally data-driven.  Adding a new custom calendar means
 * supplying its small set of operations and registering one descriptor; it
 * does not require extending a collection of central switch statements.
 *
 * Navigation preserves the displayed day when possible and clamps it only
 * when the destination period is shorter.  Fixed-size period calendars can
 * bypass field conversion entirely, which keeps ISO-week and Mayan navigation
 * both simpler and less error-prone.
 */

#include "calendar-custom.h"
#include "calendar-ancient.h"
#include "calendar-bahai.h"
#include "calendar-historical.h"
#include "calendar-perpetual.h"
#include "calendar-reform.h"
#include "julian-day.h"

#include <glib/gi18n-lib.h>

typedef CalendarPlusCalendarMode CalendarMode;
typedef CalendarPlusCalendarFields CalendarFields;

typedef void (*FieldsFromJdnFunc)(gint64 jdn, CalendarFields *fields);
typedef gint64 (*FieldsToJdnFunc)(const CalendarFields *fields);
typedef gint (*MonthLengthFunc)(const CalendarFields *fields);
typedef gchar *(*FormatFunc)(const CalendarFields *fields,
                             CalendarPlusDatePart part);
typedef gint (*PeriodIndexFunc)(gint month);
typedef gint (*MonthFromPeriodFunc)(gint period);

typedef struct
{
    FieldsFromJdnFunc fields_from_jdn;
    FieldsToJdnFunc fields_to_jdn;
    MonthLengthFunc month_length;
    FormatFunc format;
    PeriodIndexFunc period_index;
    MonthFromPeriodFunc month_from_period;
    gint periods_per_year;
    gint fixed_period_days;
    gint fixed_year_days;
    gint period_origin_day;
    gboolean clamp_iso_weeks;
} CustomCalendarProvider;

#define floor_divide calendar_plus_floor_divide
#define positive_modulo calendar_plus_positive_modulo
#define gregorian_is_leap calendar_plus_gregorian_is_leap
#define gregorian_to_jdn calendar_plus_gregorian_to_jdn
#define jdn_to_gregorian calendar_plus_jdn_to_gregorian
#define julian_to_jdn calendar_plus_julian_to_jdn
#define jdn_to_julian calendar_plus_jdn_to_julian
#define gregorian_day_of_year calendar_plus_gregorian_day_of_year
#define iso_weekday calendar_plus_iso_weekday

static const gchar *const french_months[] = {
    "", N_("Vendémiaire"), N_("Brumaire"), N_("Frimaire"),
    N_("Nivôse"), N_("Pluviôse"), N_("Ventôse"), N_("Germinal"),
    N_("Floréal"), N_("Prairial"), N_("Messidor"), N_("Thermidor"),
    N_("Fructidor"), N_("Sans-culottides")
};

static const gchar *const roman_months[] = {
    "", N_("Ianuarius"), N_("Februarius"), N_("Martius"), N_("Aprilis"),
    N_("Maius"), N_("Iunius"), N_("Iulius"), N_("Augustus"),
    N_("September"), N_("October"), N_("November"), N_("December")
};

static const gchar *const bahai_months[] = {
    N_("Ayyám-i-Há"), N_("Bahá"), N_("Jalál"), N_("Jamál"),
    N_("‘Aẓamat"), N_("Núr"), N_("Raḥmat"), N_("Kalimát"), N_("Kamál"),
    N_("Asmá’"), N_("‘Izzat"), N_("Mashíyyat"), N_("‘Ilm"), N_("Qudrat"),
    N_("Qawl"), N_("Masá’il"), N_("Sharaf"), N_("Sulṭán"), N_("Mulk"),
    N_("‘Alá’")
};

static const gchar *const fixed_months[] = {
    "", N_("January"), N_("February"), N_("March"), N_("April"),
    N_("May"), N_("June"), N_("Sol"), N_("July"), N_("August"),
    N_("September"), N_("October"), N_("November"), N_("December")
};

static const gchar *const common_months[] = {
    "", N_("January"), N_("February"), N_("March"), N_("April"),
    N_("May"), N_("June"), N_("July"), N_("August"), N_("September"),
    N_("October"), N_("November"), N_("December")
};

static const gchar *const positivist_months[] = {
    "", N_("Moses"), N_("Homer"), N_("Aristotle"), N_("Archimedes"),
    N_("Caesar"), N_("Saint Paul"), N_("Charlemagne"), N_("Dante"),
    N_("Gutenberg"), N_("Shakespeare"), N_("Descartes"), N_("Frederick"),
    N_("Bichat")
};

static gint
iso_weeks_in_year(gint year)
{
    const gint january_first = iso_weekday(gregorian_to_jdn(year, 1, 1));

    return january_first == 4 ||
           (january_first == 3 && gregorian_is_leap(year)) ? 53 : 52;
}

static void
iso_fields_from_jdn(gint64 jdn,
                    CalendarFields *fields)
{
    gint year;
    gint month;
    gint day;
    gint week;
    const gint weekday = iso_weekday(jdn);

    jdn_to_gregorian(jdn, &year, &month, &day);
    week = (gregorian_day_of_year(year, month, day) - weekday + 10) / 7;

    if (week < 1)
    {
        year--;
        week = iso_weeks_in_year(year);
    }
    else if (week > iso_weeks_in_year(year))
    {
        year++;
        week = 1;
    }

    *fields = (CalendarFields){
        .year = year,
        .month = week,
        .day = weekday,
        .auxiliary = 0,
        .special = FALSE
    };
}

static gint64
iso_fields_to_jdn(const CalendarFields *fields)
{
    const gint64 january_fourth =
        gregorian_to_jdn((gint)fields->year, 1, 4);
    const gint64 week_one_monday =
        january_fourth - (iso_weekday(january_fourth) - 1);

    return week_one_monday +
           (gint64)(fields->month - 1) * CALENDAR_PLUS_DAYS_PER_WEEK +
           fields->day - 1;
}

static void
julian_fields_from_jdn(gint64 jdn,
                       CalendarFields *fields)
{
    gint year;
    gint month;
    gint day;

    jdn_to_julian(jdn, &year, &month, &day);
    *fields = (CalendarFields){
        .year = year,
        .month = month,
        .day = day,
        .auxiliary = 0,
        .special = FALSE
    };
}

static gint64
julian_fields_to_jdn(const CalendarFields *fields)
{
    return julian_to_jdn(fields->year, fields->month, fields->day);
}

static gint
julian_month_length(const CalendarFields *fields)
{
    if (fields->month == 2)
        return positive_modulo(fields->year, 4) == 0 ? 29 : 28;

    return fields->month == 4 || fields->month == 6 ||
           fields->month == 9 || fields->month == 11 ? 30 : 31;
}

static gint
iso_month_length(const CalendarFields *fields)
{
    (void)fields;
    return 7;
}

static gint
french_month_length(const CalendarFields *fields)
{
    return fields->month == 13 ?
           (calendar_plus_french_is_leap(fields->year) ? 6 : 5) : 30;
}

static gint
mayan_month_length(const CalendarFields *fields)
{
    (void)fields;
    return 20;
}

static gint
bahai_month_length(const CalendarFields *fields)
{
    return fields->month == 0 ?
           calendar_plus_bahai_intercalary_days(fields->year) : 19;
}

static gint
fixed_month_length(const CalendarFields *fields)
{
    return calendar_plus_fixed_month_length(fields->year, fields->month);
}

static gint
world_month_length(const CalendarFields *fields)
{
    return calendar_plus_world_month_length(fields->year, fields->month);
}

static gint
positivist_month_length(const CalendarFields *fields)
{
    return calendar_plus_positivist_month_length(fields->year, fields->month);
}

static gint
default_period_index(gint month)
{
    return month - 1;
}

static gint
default_month_from_period(gint period)
{
    return period + 1;
}

static gchar *
format_named_date(const CalendarFields *fields,
                  CalendarPlusDatePart part,
                  const gchar *const *months)
{
    const gchar *month_name = _(months[fields->month]);

    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup(month_name);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
        return g_strdup_printf("%" G_GINT64_FORMAT, fields->year);

    return g_strdup_printf(_("%d %s %lld"),
                           fields->day,
                           month_name,
                           (long long)fields->year);
}

static gchar *
format_julian(const CalendarFields *fields,
              CalendarPlusDatePart part)
{
    return format_named_date(fields, part, common_months);
}

static gchar *
format_iso(const CalendarFields *fields,
           CalendarPlusDatePart part)
{
    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup_printf(_("Week %02d"), fields->month);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
        return g_strdup_printf("%" G_GINT64_FORMAT, fields->year);

    return g_strdup_printf("%04" G_GINT64_FORMAT "-W%02d-%d",
                           fields->year,
                           fields->month,
                           fields->day);
}

static gchar *
format_roman(const CalendarFields *fields,
             CalendarPlusDatePart part)
{
    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return calendar_plus_roman_number(fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup(_(roman_months[fields->month]));
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
        return calendar_plus_roman_number(fields->year + 753);

    return calendar_plus_roman_format_date(fields);
}

static gchar *
format_french(const CalendarFields *fields,
              CalendarPlusDatePart part)
{
    const gchar *month_name = _(french_months[fields->month]);
    g_autofree gchar *roman_year = NULL;

    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup(month_name);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
        return calendar_plus_roman_number(fields->year);

    roman_year = calendar_plus_roman_number(fields->year);
    return g_strdup_printf(_("%d %s, An %s"),
                           fields->day,
                           month_name,
                           roman_year);
}

static gchar *
format_bahai(const CalendarFields *fields,
             CalendarPlusDatePart part)
{
    const gchar *month_name = _(bahai_months[fields->month]);

    if (part == CALENDAR_PLUS_DATE_PART_DAY)
        return g_strdup_printf("%d", fields->day);
    if (part == CALENDAR_PLUS_DATE_PART_MONTH)
        return g_strdup(month_name);
    if (part == CALENDAR_PLUS_DATE_PART_YEAR)
        return g_strdup_printf("%" G_GINT64_FORMAT, fields->year);

    return g_strdup_printf(_("%d %s %lld B.E."),
                           fields->day,
                           month_name,
                           (long long)fields->year);
}

static gchar *
format_fixed(const CalendarFields *fields,
             CalendarPlusDatePart part)
{
    if (part == CALENDAR_PLUS_DATE_PART_DAY && fields->special)
        return g_strdup(fields->month == 6 ? "L" : "Y");
    if (fields->special &&
        part != CALENDAR_PLUS_DATE_PART_MONTH &&
        part != CALENDAR_PLUS_DATE_PART_YEAR)
    {
        return g_strdup_printf("%s, %" G_GINT64_FORMAT,
                               fields->month == 6 ?
                               _("Leap Day") : _("Year Day"),
                               fields->year);
    }

    return format_named_date(fields, part, fixed_months);
}

static gchar *
format_world(const CalendarFields *fields,
             CalendarPlusDatePart part)
{
    if (part == CALENDAR_PLUS_DATE_PART_DAY && fields->special)
        return g_strdup(fields->month == 6 ? "L" : "W");
    if (fields->special &&
        part != CALENDAR_PLUS_DATE_PART_MONTH &&
        part != CALENDAR_PLUS_DATE_PART_YEAR)
    {
        return g_strdup_printf("%s, %" G_GINT64_FORMAT,
                               fields->month == 6 ?
                               _("Leapyear Day") : _("Worldsday"),
                               fields->year);
    }

    return format_named_date(fields, part, common_months);
}

static gchar *
format_positivist(const CalendarFields *fields,
                  CalendarPlusDatePart part)
{
    if (part == CALENDAR_PLUS_DATE_PART_DAY && fields->special)
        return g_strdup(fields->day == 29 ? "D" : "H");
    if (fields->special &&
        part != CALENDAR_PLUS_DATE_PART_MONTH &&
        part != CALENDAR_PLUS_DATE_PART_YEAR)
    {
        return g_strdup_printf("%s, %" G_GINT64_FORMAT,
                               fields->day == 29 ?
                               _("Festival of All the Dead") :
                               _("Festival of Holy Women"),
                               fields->year);
    }

    return format_named_date(fields, part, positivist_months);
}

/*
 * Sparse, mode-indexed provider registry.  Empty slots correspond to ICU
 * calendars and are intentionally rejected by custom_provider().
 */
static const CustomCalendarProvider custom_providers[CALENDAR_PLUS_CALENDAR_MODE_COUNT] = {
    [CALENDAR_PLUS_CALENDAR_MODE_JULIAN] = {
        julian_fields_from_jdn, julian_fields_to_jdn, julian_month_length,
        format_julian, default_period_index, default_month_from_period,
        12, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_ISO_WEEK] = {
        iso_fields_from_jdn, iso_fields_to_jdn, iso_month_length,
        format_iso, default_period_index, default_month_from_period,
        0, 7, 0, 1, TRUE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_FRENCH_REPUBLICAN] = {
        calendar_plus_french_fields_from_jdn,
        calendar_plus_french_fields_to_jdn,
        french_month_length, format_french,
        default_period_index, default_month_from_period,
        13, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_ROMAN] = {
        julian_fields_from_jdn, julian_fields_to_jdn, julian_month_length,
        format_roman, default_period_index, default_month_from_period,
        12, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_MAYAN] = {
        calendar_plus_mayan_fields_from_jdn,
        calendar_plus_mayan_fields_to_jdn,
        mayan_month_length, calendar_plus_mayan_format,
        default_period_index, default_month_from_period,
        18, 20, 360, 0, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_BAHAI] = {
        calendar_plus_bahai_fields_from_jdn,
        calendar_plus_bahai_fields_to_jdn,
        bahai_month_length, format_bahai,
        calendar_plus_bahai_period_index,
        calendar_plus_bahai_month_from_period,
        20, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_INTERNATIONAL_FIXED] = {
        calendar_plus_fixed_fields_from_jdn,
        calendar_plus_fixed_fields_to_jdn,
        fixed_month_length, format_fixed,
        default_period_index, default_month_from_period,
        13, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_WORLD] = {
        calendar_plus_world_fields_from_jdn,
        calendar_plus_world_fields_to_jdn,
        world_month_length, format_world,
        default_period_index, default_month_from_period,
        12, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_POSITIVIST] = {
        calendar_plus_positivist_fields_from_jdn,
        calendar_plus_positivist_fields_to_jdn,
        positivist_month_length, format_positivist,
        default_period_index, default_month_from_period,
        13, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_REVISED_JULIAN] = {
        calendar_plus_revised_julian_fields_from_jdn,
        calendar_plus_revised_julian_fields_to_jdn,
        calendar_plus_revised_julian_month_length,
        calendar_plus_revised_julian_format,
        default_period_index, default_month_from_period,
        12, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_BYZANTINE] = {
        calendar_plus_byzantine_fields_from_jdn,
        calendar_plus_byzantine_fields_to_jdn,
        calendar_plus_byzantine_month_length,
        calendar_plus_byzantine_format,
        calendar_plus_byzantine_period_index,
        calendar_plus_byzantine_month_from_period,
        12, 0, 0, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_EGYPTIAN_NABONASSAR] = {
        calendar_plus_egyptian_fields_from_jdn,
        calendar_plus_egyptian_fields_to_jdn,
        calendar_plus_egyptian_month_length,
        calendar_plus_egyptian_format,
        default_period_index, default_month_from_period,
        13, 0, 365, 1, FALSE
    },
    [CALENDAR_PLUS_CALENDAR_MODE_ARMENIAN_TRADITIONAL] = {
        calendar_plus_armenian_fields_from_jdn,
        calendar_plus_armenian_fields_to_jdn,
        calendar_plus_armenian_month_length,
        calendar_plus_armenian_format,
        default_period_index, default_month_from_period,
        13, 0, 365, 1, FALSE
    }
};

static const CustomCalendarProvider *
custom_provider(CalendarMode mode)
{
    const CustomCalendarProvider *provider;

    if (mode < 0 || mode >= CALENDAR_PLUS_CALENDAR_MODE_COUNT)
        return NULL;

    provider = &custom_providers[mode];
    return provider->fields_from_jdn != NULL ? provider : NULL;
}

void
calendar_plus_custom_fields_from_jdn(CalendarMode mode,
                                     gint64 jdn,
                                     CalendarFields *fields)
{
    const CustomCalendarProvider *provider = custom_provider(mode);

    g_return_if_fail(fields != NULL);
    *fields = (CalendarFields){0};

    if (provider != NULL)
        provider->fields_from_jdn(jdn, fields);
}

gchar *
calendar_plus_custom_format(CalendarMode mode,
                            const CalendarFields *fields,
                            CalendarPlusDatePart part)
{
    const CustomCalendarProvider *provider = custom_provider(mode);

    if (provider == NULL || fields == NULL || provider->format == NULL)
        return g_strdup("");

    return provider->format(fields, part);
}

gint64
calendar_plus_custom_month_start(CalendarMode mode,
                                 gint64 jdn)
{
    const CustomCalendarProvider *provider = custom_provider(mode);
    CalendarFields fields;

    if (provider == NULL)
        return CALENDAR_PLUS_UNIX_EPOCH_JDN;

    provider->fields_from_jdn(jdn, &fields);
    fields.day = provider->period_origin_day;
    return provider->fields_to_jdn(&fields);
}

gint64
calendar_plus_custom_add_months(CalendarMode mode,
                                gint64 jdn,
                                gint amount)
{
    const CustomCalendarProvider *provider = custom_provider(mode);
    CalendarFields fields;
    gint64 serial;
    gint period;

    if (provider == NULL)
        return jdn;

    if (provider->fixed_period_days > 0)
        return jdn + (gint64)amount * provider->fixed_period_days;

    provider->fields_from_jdn(jdn, &fields);
    period = provider->period_index(fields.month);
    serial = fields.year * provider->periods_per_year + period + amount;
    fields.year = floor_divide(serial, provider->periods_per_year);
    period = (gint)positive_modulo(serial, provider->periods_per_year);
    fields.month = provider->month_from_period(period);
    fields.day = MIN(fields.day, provider->month_length(&fields));

    return provider->fields_to_jdn(&fields);
}

gint64
calendar_plus_custom_add_years(CalendarMode mode,
                               gint64 jdn,
                               gint amount)
{
    const CustomCalendarProvider *provider = custom_provider(mode);
    CalendarFields fields;

    if (provider == NULL)
        return jdn;

    if (provider->fixed_year_days > 0)
        return jdn + (gint64)amount * provider->fixed_year_days;

    provider->fields_from_jdn(jdn, &fields);
    fields.year += amount;
    if (provider->clamp_iso_weeks)
        fields.month = MIN(fields.month, iso_weeks_in_year((gint)fields.year));
    fields.day = MIN(fields.day, provider->month_length(&fields));

    return provider->fields_to_jdn(&fields);
}

gint
calendar_plus_custom_iso_week_number(gint64 jdn)
{
    CalendarFields fields;

    iso_fields_from_jdn(jdn, &fields);
    return fields.month;
}
