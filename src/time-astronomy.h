/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Private astronomical time primitives shared by clock providers.
 */

#ifndef CALENDAR_PLUS_TIME_ASTRONOMY_H
#define CALENDAR_PLUS_TIME_ASTRONOMY_H

#include <glib.h>

G_BEGIN_DECLS

#define CALENDAR_PLUS_SIDEREAL_RATE 1.002737909350795L

/* Astronomical Julian Date; its integer-day boundary is noon UTC. */
long double calendar_plus_julian_date(gint64 unix_microseconds);
/* Seconds in [0, 86400); longitude is degrees east of Greenwich. */
long double calendar_plus_local_sidereal_seconds(gint64 unix_microseconds,
                                                  gdouble longitude);
/* Local mean solar seconds in [0, 86400); longitude is east-positive. */
long double calendar_plus_mean_solar_seconds(gint64 unix_microseconds,
                                              gdouble longitude);
/* Apparent solar seconds in [0, 86400); longitude is east-positive. */
long double calendar_plus_apparent_solar_seconds(gint64 unix_microseconds,
                                                  gdouble longitude);
/*
 * Returns apparent-solar dawn/dusk boundaries for the current solar date.
 * @solar_depression_degrees is the positive angle of the Sun's centre below
 * the ideal horizon. 0.833 degrees models observed sunrise/sunset including
 * standard refraction and the solar radius; larger values model twilight.
 * FALSE means that the requested boundary does not occur at this latitude.
 */
gboolean calendar_plus_solar_day_boundaries(
    gint64 unix_microseconds,
    gdouble latitude,
    gdouble solar_depression_degrees,
    long double *dawn_seconds,
    long double *dusk_seconds);

G_END_DECLS

#endif
