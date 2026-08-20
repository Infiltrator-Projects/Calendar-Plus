/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Integer date arithmetic on a midnight-based Julian Day Number axis.
 * Gregorian and Julian years use astronomical numbering, including year 0.
 * Weekdays are ISO 1=Monday through 7=Sunday. Conversion callers are
 * responsible for validating civil dates before converting them.
 *
 * Calendar Plus exposes civil years as gint through CalendarPlusDate. The JDN
 * conversion arithmetic is deliberately implemented in gint64 and is tested at
 * both G_MININT and G_MAXINT, so every representable Calendar Plus civil year
 * has substantial intermediate headroom. JDN-to-date callers must likewise
 * supply JDNs originating from that civil domain or from a calendar provider
 * operating on it; arbitrary extremes of the gint64 number line are not a
 * claimed civil-date domain.
 */

#ifndef CALENDAR_PLUS_JULIAN_DAY_H
#define CALENDAR_PLUS_JULIAN_DAY_H

#include "calendar-internal.h"

G_BEGIN_DECLS

#define CALENDAR_PLUS_UNIX_EPOCH_JDN ((gint64)2440588)

gint64 calendar_plus_floor_divide(gint64 value, gint64 divisor);
gint64 calendar_plus_positive_modulo(gint64 value, gint64 modulus);
gboolean calendar_plus_gregorian_is_leap(gint64 year);
gboolean calendar_plus_gregorian_date_is_valid(gint year,
                                                gint month,
                                                gint day);
gint64 calendar_plus_gregorian_to_jdn(gint64 year,
                                      gint month,
                                      gint day);
void calendar_plus_jdn_to_gregorian(gint64 jdn,
                                    gint *year,
                                    gint *month,
                                    gint *day);
gint64 calendar_plus_julian_to_jdn(gint64 year,
                                   gint month,
                                   gint day);
void calendar_plus_jdn_to_julian(gint64 jdn,
                                 gint *year,
                                 gint *month,
                                 gint *day);
gint calendar_plus_gregorian_day_of_year(gint year,
                                         gint month,
                                         gint day);
gint calendar_plus_iso_weekday(gint64 jdn);
gchar *calendar_plus_format_iso_date(gint64 jdn);

G_END_DECLS

#endif
