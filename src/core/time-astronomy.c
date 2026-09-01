// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Astronomical clock primitives.
 *
 * These functions intentionally return continuous seconds/date values rather
 * than formatted strings. Presentation precision and timer scheduling belong
 * to time providers; this module owns only UTC/location mathematics. The
 * compact solar model follows the NOAA fractional-year approximation already
 * used by Calendar Plus rather than depending on an external ephemeris.
 */

#include "time-astronomy.h"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <math.h>

#define JULIAN_DATE_UNIX_EPOCH 2440587.5L
#define JULIAN_DATE_J2000 2451545.0L

enum
{
    SECONDS_PER_HOUR = 3600,
    SECONDS_PER_DAY = 86400
};

#define MICROSECONDS_PER_DAY ((gint64)SECONDS_PER_DAY * G_USEC_PER_SEC)

static gint64
floor_divide(gint64 value,
             gint64 divisor)
{
    int64_t quotient = 0;

    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL), 0);
    return quotient;
}

static gint64
positive_modulo(gint64 value,
                gint64 modulus)
{
    int64_t remainder = 0;

    g_return_val_if_fail(
        infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder), 0);
    return remainder;
}

static gdouble
normalise_longitude(gdouble longitude)
{
    if (!isfinite(longitude))
        return 0.0;

    return infiltratr_clamp_double(longitude, -180.0, 180.0);
}

static gdouble
normalise_latitude(gdouble latitude)
{
    if (!isfinite(latitude))
        return 0.0;

    return infiltratr_clamp_double(latitude, -90.0, 90.0);
}

/*
 * NOAA's compact model uses one fractional-year angle to approximate both the
 * equation of time and solar declination. Keeping the two values together
 * prevents the sunrise/sunset and apparent-time providers from drifting onto
 * subtly different solar models.
 */
static gboolean
solar_terms(gint64 unix_microseconds,
            gdouble *equation_minutes,
            gdouble *declination_radians)
{
    const gint64 unix_seconds =
        floor_divide(unix_microseconds, G_USEC_PER_SEC);
    g_autoptr(GDateTime) utc = g_date_time_new_from_unix_utc(unix_seconds);
    gdouble fractional_hour;
    gdouble gamma;

    if (utc == NULL)
        return FALSE;

    fractional_hour =
        g_date_time_get_hour(utc) +
        g_date_time_get_minute(utc) / 60.0 +
        (g_date_time_get_second(utc) +
         g_date_time_get_microsecond(utc) / (gdouble)G_USEC_PER_SEC) /
        3600.0;
    gamma =
        2.0 * G_PI / 365.0 *
        (g_date_time_get_day_of_year(utc) - 1 +
         (fractional_hour - 12.0) / 24.0);

    if (equation_minutes != NULL)
    {
        *equation_minutes =
            229.18 *
            (0.000075 +
             0.001868 * cos(gamma) -
             0.032077 * sin(gamma) -
             0.014615 * cos(2.0 * gamma) -
             0.040849 * sin(2.0 * gamma));
    }

    if (declination_radians != NULL)
    {
        *declination_radians =
            0.006918 -
            0.399912 * cos(gamma) +
            0.070257 * sin(gamma) -
            0.006758 * cos(2.0 * gamma) +
            0.000907 * sin(2.0 * gamma) -
            0.002697 * cos(3.0 * gamma) +
            0.001480 * sin(3.0 * gamma);
    }

    return TRUE;
}

long double
calendar_plus_julian_date(gint64 unix_microseconds)
{
    return JULIAN_DATE_UNIX_EPOCH +
           (long double)unix_microseconds / MICROSECONDS_PER_DAY;
}

long double
calendar_plus_local_sidereal_seconds(gint64 unix_microseconds,
                                     gdouble longitude)
{
    const long double days_since_j2000 =
        calendar_plus_julian_date(unix_microseconds) - JULIAN_DATE_J2000;
    const long double gmst_hours =
        18.697374558L + 24.06570982441908L * days_since_j2000;
    long double local_hours =
        fmodl(gmst_hours + normalise_longitude(longitude) / 15.0L, 24.0L);

    if (local_hours < 0.0L)
        local_hours += 24.0L;

    return local_hours * SECONDS_PER_HOUR;
}

long double
calendar_plus_mean_solar_seconds(gint64 unix_microseconds,
                                 gdouble longitude)
{
    const gint64 utc_microseconds =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);
    long double seconds =
        (long double)utc_microseconds / G_USEC_PER_SEC +
        normalise_longitude(longitude) * 240.0L;

    seconds = fmodl(seconds, SECONDS_PER_DAY);
    if (seconds < 0.0L)
        seconds += SECONDS_PER_DAY;

    return seconds;
}

long double
calendar_plus_apparent_solar_seconds(gint64 unix_microseconds,
                                     gdouble longitude)
{
    gdouble equation_minutes = 0.0;
    long double seconds;

    (void)solar_terms(unix_microseconds, &equation_minutes, NULL);
    seconds = calendar_plus_mean_solar_seconds(unix_microseconds, longitude) +
              equation_minutes * 60.0L;

    seconds = fmodl(seconds, SECONDS_PER_DAY);
    if (seconds < 0.0L)
        seconds += SECONDS_PER_DAY;

    return seconds;
}

gboolean
calendar_plus_solar_day_boundaries(gint64 unix_microseconds,
                                   gdouble latitude,
                                   gdouble solar_depression_degrees,
                                   long double *dawn_seconds,
                                   long double *dusk_seconds)
{
    const gdouble safe_latitude = normalise_latitude(latitude);
    gdouble declination = 0.0;
    gdouble latitude_radians;
    gdouble zenith_radians;
    gdouble cosine_hour_angle;
    long double hour_angle;
    long double half_day_seconds;

    g_return_val_if_fail(dawn_seconds != NULL, FALSE);
    g_return_val_if_fail(dusk_seconds != NULL, FALSE);

    if (!isfinite(solar_depression_degrees) ||
        solar_depression_degrees < 0.0 ||
        solar_depression_degrees >= 90.0 ||
        !solar_terms(unix_microseconds, NULL, &declination))
    {
        return FALSE;
    }

    latitude_radians = safe_latitude * G_PI / 180.0;
    zenith_radians =
        (90.0 + solar_depression_degrees) * G_PI / 180.0;

    if (fabs(cos(latitude_radians) * cos(declination)) < 1.0e-12)
        return FALSE;

    cosine_hour_angle =
        (cos(zenith_radians) /
         (cos(latitude_radians) * cos(declination))) -
        tan(latitude_radians) * tan(declination);

    /* No crossing means polar day/night for the requested solar altitude. */
    if (cosine_hour_angle < -1.0 || cosine_hour_angle > 1.0)
        return FALSE;

    hour_angle = acosl(infiltratr_clamp_double(
        cosine_hour_angle, -1.0, 1.0));
    half_day_seconds =
        hour_angle * SECONDS_PER_DAY / (2.0L * G_PI);
    *dawn_seconds = SECONDS_PER_DAY / 2.0L - half_day_seconds;
    *dusk_seconds = SECONDS_PER_DAY / 2.0L + half_day_seconds;
    return TRUE;
}

/*
 * Convert the same compact NOAA solar model into UTC boundary instants. The
 * equation of time and declination are sampled at UTC noon for the associated
 * UTC date, matching the usual low-cost sunrise/sunset approximation. The
 * result is intentionally an instant rather than a local clock field so
 * historical equal-hour systems can measure ordinary SI hours from a moving
 * sunrise/sunset origin.
 */
gboolean
calendar_plus_solar_boundary_instants(gint64 unix_microseconds,
                                      gdouble latitude,
                                      gdouble longitude,
                                      gdouble solar_depression_degrees,
                                      gint64 *dawn_microseconds,
                                      gint64 *dusk_microseconds)
{
    const gint64 safety_margin = 2 * MICROSECONDS_PER_DAY;
    const gdouble safe_latitude = normalise_latitude(latitude);
    const gdouble safe_longitude = normalise_longitude(longitude);
    gint64 day_index;
    gint64 day_start;
    gint64 noon;
    gdouble equation_minutes = 0.0;
    gdouble declination = 0.0;
    gdouble latitude_radians;
    gdouble zenith_radians;
    gdouble cosine_hour_angle;
    long double hour_angle;
    long double solar_noon_minutes;
    long double hour_angle_minutes;
    long double dawn_value;
    long double dusk_value;

    g_return_val_if_fail(dawn_microseconds != NULL, FALSE);
    g_return_val_if_fail(dusk_microseconds != NULL, FALSE);

    if (unix_microseconds < G_MININT64 + safety_margin ||
        unix_microseconds > G_MAXINT64 - safety_margin ||
        !isfinite(solar_depression_degrees) ||
        solar_depression_degrees < 0.0 ||
        solar_depression_degrees >= 90.0)
    {
        return FALSE;
    }

    day_index = floor_divide(unix_microseconds, MICROSECONDS_PER_DAY);
    day_start = day_index * MICROSECONDS_PER_DAY;
    noon = day_start + MICROSECONDS_PER_DAY / 2;

    if (!solar_terms(noon, &equation_minutes, &declination))
        return FALSE;

    latitude_radians = safe_latitude * G_PI / 180.0;
    zenith_radians =
        (90.0 + solar_depression_degrees) * G_PI / 180.0;

    if (fabs(cos(latitude_radians) * cos(declination)) < 1.0e-12)
        return FALSE;

    cosine_hour_angle =
        (cos(zenith_radians) /
         (cos(latitude_radians) * cos(declination))) -
        tan(latitude_radians) * tan(declination);
    if (cosine_hour_angle < -1.0 || cosine_hour_angle > 1.0)
        return FALSE;

    hour_angle = acosl(infiltratr_clamp_double(
        cosine_hour_angle, -1.0, 1.0));
    solar_noon_minutes =
        720.0L - 4.0L * safe_longitude - equation_minutes;
    hour_angle_minutes =
        hour_angle * 180.0L / G_PI * 4.0L;

    dawn_value =
        (long double)day_start +
        (solar_noon_minutes - hour_angle_minutes) *
        60.0L * G_USEC_PER_SEC;
    dusk_value =
        (long double)day_start +
        (solar_noon_minutes + hour_angle_minutes) *
        60.0L * G_USEC_PER_SEC;

    if (dawn_value < (long double)G_MININT64 ||
        dawn_value > (long double)G_MAXINT64 ||
        dusk_value < (long double)G_MININT64 ||
        dusk_value > (long double)G_MAXINT64)
    {
        return FALSE;
    }

    *dawn_microseconds = (gint64)llroundl(dawn_value);
    *dusk_microseconds = (gint64)llroundl(dusk_value);
    return TRUE;
}
