// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Badíʿ calendar conversion.
 *
 * Years 1-171 B.E. retain the historically used Western civil convention:
 * Naw-Rúz is 21 March. From 172 B.E. (2015) onward Calendar Plus implements
 * the unified rule announced by the Universal House of Justice: Tehran is the
 * reference location, the northern vernal equinox is calculated
 * astronomically, and Naw-Rúz is the Tehran civil date whose sunset follows
 * that equinox (or the following date when the equinox is after sunset).
 *
 * The equinox calculation is the Meeus March-equinox polynomial plus its
 * periodic correction, converted from TT to UTC with a modern ΔT model. The
 * supported astronomical range is 1000-3000 CE; dates outside it use the
 * historical 21-March continuation rather than pretending greater precision.
 */

#include "calendar-bahai.h"

#include "julian-day.h"
#include "time-astronomy.h"

#include <math.h>

#define floor_divide calendar_plus_floor_divide
#define gregorian_to_jdn calendar_plus_gregorian_to_jdn
#define jdn_to_gregorian calendar_plus_jdn_to_gregorian

#define JULIAN_DATE_UNIX_EPOCH 2440587.5L
#define SECONDS_PER_DAY 86400.0L
#define MICROSECONDS_PER_DAY ((gint64)86400 * G_USEC_PER_SEC)
#define TEHRAN_LATITUDE 35.6892
#define TEHRAN_LONGITUDE 51.3890
#define TEHRAN_UTC_OFFSET_SECONDS 12600
#define MODERN_BAHAI_FIRST_YEAR ((gint64)172)
#define BAHAI_TO_GREGORIAN_YEAR_OFFSET ((gint64)1843)

typedef struct
{
    long double amplitude;
    long double phase;
    long double rate;
} EquinoxPeriodicTerm;

static const EquinoxPeriodicTerm equinox_terms[] = {
    {485.0L, 324.96L, 1934.136L}, {203.0L, 337.23L, 32964.467L},
    {199.0L, 342.08L, 20.186L}, {182.0L, 27.85L, 445267.112L},
    {156.0L, 73.14L, 45036.886L}, {136.0L, 171.52L, 22518.443L},
    {77.0L, 222.54L, 65928.934L}, {74.0L, 296.72L, 3034.906L},
    {70.0L, 243.58L, 9037.513L}, {58.0L, 119.81L, 33718.147L},
    {52.0L, 297.17L, 150.678L}, {50.0L, 21.02L, 2281.226L},
    {45.0L, 247.54L, 29929.562L}, {44.0L, 325.15L, 31555.956L},
    {29.0L, 60.93L, 4443.417L}, {18.0L, 155.12L, 67555.328L},
    {17.0L, 288.79L, 4562.452L}, {16.0L, 198.04L, 62894.029L},
    {14.0L, 199.76L, 31436.921L}, {12.0L, 95.39L, 14577.848L},
    {12.0L, 287.11L, 31931.756L}, {12.0L, 320.81L, 34777.259L},
    {9.0L, 227.73L, 1222.114L}, {8.0L, 15.45L, 16859.074L}
};

static long double
degrees_to_radians(long double degrees)
{
    return degrees * G_PI / 180.0L;
}

static long double
march_equinox_jde_tt(gint year)
{
    const long double y = ((long double)year - 2000.0L) / 1000.0L;
    const long double jde0 =
        2451623.80984L + 365242.37404L * y + 0.05169L * y * y -
        0.00411L * y * y * y - 0.00057L * y * y * y * y;
    const long double t = (jde0 - 2451545.0L) / 36525.0L;
    const long double w = degrees_to_radians(35999.373L * t - 2.47L);
    const long double delta_lambda =
        1.0L + 0.0334L * cosl(w) + 0.0007L * cosl(2.0L * w);
    long double correction = 0.0L;
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(equinox_terms); index++)
    {
        correction += equinox_terms[index].amplitude *
            cosl(degrees_to_radians(
                equinox_terms[index].phase + equinox_terms[index].rate * t));
    }

    return jde0 + 0.00001L * correction / delta_lambda;
}

static long double
delta_t_seconds(gint year)
{
    if (year <= 2050)
    {
        const long double t = (long double)year - 2000.0L;
        return 62.92L + 0.32217L * t + 0.005589L * t * t;
    }
    if (year <= 2150)
    {
        const long double u = ((long double)year - 1820.0L) / 100.0L;
        return -20.0L + 32.0L * u * u -
               0.5628L * (2150.0L - (long double)year);
    }

    {
        const long double u = ((long double)year - 1820.0L) / 100.0L;
        return -20.0L + 32.0L * u * u;
    }
}

static gint64
modern_naw_ruz_jdn(gint gregorian_year)
{
    const long double jde_tt = march_equinox_jde_tt(gregorian_year);
    const long double jd_utc =
        jde_tt - delta_t_seconds(gregorian_year) / SECONDS_PER_DAY;
    const gint64 unix_microseconds = (gint64)llroundl(
        (jd_utc - JULIAN_DATE_UNIX_EPOCH) *
        SECONDS_PER_DAY * G_USEC_PER_SEC);
    const gint64 local_microseconds = calendar_plus_i64_add_saturating(
        unix_microseconds,
        calendar_plus_i64_multiply_saturating(
            TEHRAN_UTC_OFFSET_SECONDS, G_USEC_PER_SEC));
    const gint64 tehran_civil_jdn = calendar_plus_i64_add_saturating(
        floor_divide(local_microseconds, MICROSECONDS_PER_DAY),
        CALENDAR_PLUS_UNIX_EPOCH_JDN);
    long double dawn = 0.0L;
    long double dusk = 0.0L;
    const long double apparent =
        calendar_plus_apparent_solar_seconds(
            unix_microseconds, TEHRAN_LONGITUDE);

    if (!calendar_plus_solar_day_boundaries(
            unix_microseconds, TEHRAN_LATITUDE, 0.833, &dawn, &dusk))
    {
        return tehran_civil_jdn;
    }

    (void)dawn;
    return apparent < dusk ? tehran_civil_jdn : calendar_plus_i64_add_saturating(tehran_civil_jdn, 1);
}

static gint64
bahai_year_start_jdn(gint64 bahai_year)
{
    const gint64 gregorian_year = calendar_plus_i64_add_saturating(
        bahai_year, BAHAI_TO_GREGORIAN_YEAR_OFFSET);

    if (bahai_year >= MODERN_BAHAI_FIRST_YEAR &&
        gregorian_year >= 1000 && gregorian_year <= 3000)
    {
        return modern_naw_ruz_jdn((gint)gregorian_year);
    }

    g_return_val_if_fail(gregorian_year >= G_MININT &&
                         gregorian_year <= G_MAXINT,
                         gregorian_to_jdn(1844, 3, 21));
    return gregorian_to_jdn((gint)gregorian_year, 3, 21);
}

gint
calendar_plus_bahai_intercalary_days(gint64 bahai_year)
{
    return (gint)calendar_plus_i64_subtract_saturating(
        calendar_plus_i64_subtract_saturating(
            bahai_year_start_jdn(
                calendar_plus_i64_add_saturating(bahai_year, 1)),
            bahai_year_start_jdn(bahai_year)),
        361);
}

void
calendar_plus_bahai_fields_from_jdn(
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint gregorian_year;
    gint gregorian_month;
    gint gregorian_day;
    gint64 bahai_year;
    gint64 start;
    gint day_index;
    gint intercalary;

    g_return_if_fail(fields != NULL);

    jdn_to_gregorian(
        jdn, &gregorian_year, &gregorian_month, &gregorian_day);
    (void)gregorian_month;
    (void)gregorian_day;
    bahai_year =
        (gint64)gregorian_year - BAHAI_TO_GREGORIAN_YEAR_OFFSET;
    start = bahai_year_start_jdn(bahai_year);
    if (jdn < start)
    {
        bahai_year = calendar_plus_i64_subtract_saturating(
            bahai_year, 1);
        start = bahai_year_start_jdn(bahai_year);
    }
    else if (jdn >= bahai_year_start_jdn(calendar_plus_i64_add_saturating(bahai_year, 1)))
    {
        bahai_year = calendar_plus_i64_add_saturating(
            bahai_year, 1);
        start = bahai_year_start_jdn(bahai_year);
    }

    fields->year = bahai_year;
    day_index = (gint)calendar_plus_i64_subtract_saturating(
        jdn, start);
    intercalary = calendar_plus_bahai_intercalary_days(bahai_year);

    if (day_index < 18 * 19)
    {
        fields->month = day_index / 19 + 1;
        fields->day = day_index % 19 + 1;
        fields->special = FALSE;
    }
    else if (day_index < 18 * 19 + intercalary)
    {
        fields->month = 0;
        fields->day = day_index - 18 * 19 + 1;
        fields->special = TRUE;
    }
    else
    {
        fields->month = 19;
        fields->day = day_index - 18 * 19 - intercalary + 1;
        fields->special = FALSE;
    }

    fields->auxiliary = 0;
}

gint
calendar_plus_bahai_period_index(gint month)
{
    if (month == 0)
        return 18;
    if (month == 19)
        return 19;
    return month - 1;
}

gint
calendar_plus_bahai_month_from_period(gint period)
{
    if (period == 18)
        return 0;
    if (period == 19)
        return 19;
    return period + 1;
}

gint64
calendar_plus_bahai_fields_to_jdn(
    const CalendarPlusCalendarFields *fields)
{
    gint64 start;
    gint offset;

    g_return_val_if_fail(fields != NULL, gregorian_to_jdn(1844, 3, 21));
    start = bahai_year_start_jdn(fields->year);

    if (fields->month >= 1 && fields->month <= 18)
        offset = (fields->month - 1) * 19;
    else if (fields->month == 0)
        offset = 18 * 19;
    else
        offset = 18 * 19 + calendar_plus_bahai_intercalary_days(fields->year);

    return calendar_plus_i64_add_saturating(
        calendar_plus_i64_add_saturating(start, offset),
        fields->day - 1);
}
