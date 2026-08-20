// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Calendar-specific integration tests for the exact timing primitives supplied
 * by Infiltratr Common. These tests exercise the boundary semantics through the
 * Calendar Plus public clock functions rather than retesting Common in isolation.
 */

#include "julian-day.h"
#include "time-formats.h"

#include <glib.h>

static void
test_decimal_exact_boundaries(void)
{
    g_autofree gchar *before = calendar_plus_format_time(
        CALENDAR_PLUS_TIME_MODE_DECIMAL, 863999, 0, TRUE, FALSE, 0.0);
    g_autofree gchar *at = calendar_plus_format_time(
        CALENDAR_PLUS_TIME_MODE_DECIMAL, 864000, 0, TRUE, FALSE, 0.0);

    g_assert_cmpstr(before, ==, "0:00:00");
    g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                         CALENDAR_PLUS_TIME_MODE_DECIMAL,
                         863999, 0, TRUE, 0.0),
                     ==, 1);

    g_assert_cmpstr(at, ==, "0:00:01");
    g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                         CALENDAR_PLUS_TIME_MODE_DECIMAL,
                         864000, 0, TRUE, 0.0),
                     ==, 864);
}

static void
test_hexadecimal_rational_boundary(void)
{
    /*
     * One hexadecimal tick is exactly 86,400,000,000 / 65,536
     * microseconds = 1,318,359.375 microseconds. The display must stay at 0000
     * for the final whole microsecond before that boundary, then advance at the
     * first representable whole-microsecond instant after it. The one-shot
     * delay is rounded upward and therefore can never fire early.
     */
    g_autofree gchar *before = calendar_plus_format_time(
        CALENDAR_PLUS_TIME_MODE_HEXADECIMAL,
        1318359, 0, FALSE, FALSE, 0.0);
    g_autofree gchar *after = calendar_plus_format_time(
        CALENDAR_PLUS_TIME_MODE_HEXADECIMAL,
        1318360, 0, FALSE, FALSE, 0.0);

    g_assert_cmpstr(before, ==, "0000");
    g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                         CALENDAR_PLUS_TIME_MODE_HEXADECIMAL,
                         1318359, 0, FALSE, 0.0),
                     ==, 1);

    g_assert_cmpstr(after, ==, "0001");
    g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                         CALENDAR_PLUS_TIME_MODE_HEXADECIMAL,
                         1318360, 0, FALSE, 0.0),
                     ==, 1319);
}

static void
test_signed_unix_boundaries(void)
{
    g_autofree gchar *negative = calendar_plus_format_time(
        CALENDAR_PLUS_TIME_MODE_UNIX, -1, 0, FALSE, FALSE, 0.0);
    g_autofree gchar *zero = calendar_plus_format_time(
        CALENDAR_PLUS_TIME_MODE_UNIX, 0, 0, FALSE, FALSE, 0.0);

    g_assert_cmpstr(negative, ==, "-1");
    g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                         CALENDAR_PLUS_TIME_MODE_UNIX,
                         -1, 0, FALSE, 0.0),
                     ==, 1);

    g_assert_cmpstr(zero, ==, "0");
    g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                         CALENDAR_PLUS_TIME_MODE_UNIX,
                         0, 0, FALSE, 0.0),
                     ==, 1000);
}

static void
test_extreme_instant_normalisation(void)
{
    /*
     * The clock API accepts the complete gint64 instant and gint UTC-offset
     * domains. Reducing each operand to a civil-day phase before addition keeps
     * these extrema defined under UBSan rather than overflowing signed integers.
     */
    const gint64 instants[] = { G_MININT64, G_MAXINT64 };
    const gint offsets[] = { G_MININT, G_MAXINT };
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(instants); index++)
    {
        g_autofree gchar *decimal = calendar_plus_format_time(
            CALENDAR_PLUS_TIME_MODE_DECIMAL,
            instants[index], offsets[index], TRUE, FALSE, 0.0);
        g_autofree gchar *internet = calendar_plus_format_time(
            CALENDAR_PLUS_TIME_MODE_INTERNET,
            instants[index], offsets[index], TRUE, FALSE, 0.0);

        g_assert_nonnull(decimal);
        g_assert_cmpstr(decimal, !=, "");
        g_assert_nonnull(internet);
        g_assert_true(g_str_has_prefix(internet, "@"));
        g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                             CALENDAR_PLUS_TIME_MODE_DECIMAL,
                             instants[index], offsets[index], TRUE, 0.0),
                         >, 0);
        g_assert_cmpuint(calendar_plus_time_delay_to_next_tick(
                             CALENDAR_PLUS_TIME_MODE_CHINESE,
                             instants[index], offsets[index], FALSE, 0.0),
                         >, 0);
    }
}

static void
assert_gregorian_round_trip(gint year,
                            gint month,
                            gint day)
{
    const gint64 jdn = calendar_plus_gregorian_to_jdn(year, month, day);
    gint actual_year = 0;
    gint actual_month = 0;
    gint actual_day = 0;

    calendar_plus_jdn_to_gregorian(jdn,
                                   &actual_year,
                                   &actual_month,
                                   &actual_day);
    g_assert_cmpint(actual_year, ==, year);
    g_assert_cmpint(actual_month, ==, month);
    g_assert_cmpint(actual_day, ==, day);
}

static void
assert_julian_round_trip(gint year,
                        gint month,
                        gint day)
{
    const gint64 jdn = calendar_plus_julian_to_jdn(year, month, day);
    gint actual_year = 0;
    gint actual_month = 0;
    gint actual_day = 0;

    calendar_plus_jdn_to_julian(jdn,
                                &actual_year,
                                &actual_month,
                                &actual_day);
    g_assert_cmpint(actual_year, ==, year);
    g_assert_cmpint(actual_month, ==, month);
    g_assert_cmpint(actual_day, ==, day);
}

static void
test_complete_civil_year_domain(void)
{
    assert_gregorian_round_trip(G_MININT, 1, 1);
    assert_gregorian_round_trip(G_MININT, 12, 31);
    assert_gregorian_round_trip(G_MAXINT, 1, 1);
    assert_gregorian_round_trip(G_MAXINT, 12, 31);

    assert_julian_round_trip(G_MININT, 1, 1);
    assert_julian_round_trip(G_MININT, 12, 31);
    assert_julian_round_trip(G_MAXINT, 1, 1);
    assert_julian_round_trip(G_MAXINT, 12, 31);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/exact-clock/decimal-boundaries",
                    test_decimal_exact_boundaries);
    g_test_add_func("/exact-clock/hex-rational-boundary",
                    test_hexadecimal_rational_boundary);
    g_test_add_func("/exact-clock/unix-signed-boundaries",
                    test_signed_unix_boundaries);
    g_test_add_func("/exact-clock/extreme-instant-normalisation",
                    test_extreme_instant_normalisation);
    g_test_add_func("/date-domain/complete-gint-years",
                    test_complete_civil_year_domain);
    return g_test_run();
}
