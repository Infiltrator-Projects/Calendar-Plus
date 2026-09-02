// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include "system-clock.h"
#include "time-formats.h"
#include "calendar-system.h"
#include "calendar-core.h"
#include "calendar-internal.h"
#include "event-store.h"
#include "icu-calendar.h" /* locale/weekend contract exercised directly */
#include "julian-day.h" /* private engine invariants exercised by this test */
#include "version.h"

#include <string.h>

#define USECONDS(value) ((gint64)(value) * G_USEC_PER_SEC)

static void
assert_time_mode(const gchar *mode,
                 CalendarPlusTimeMode parsed,
                 gint64 unix_microseconds,
                 gint utc_offset_seconds,
                 gboolean show_seconds,
                 gboolean vertical,
                 gdouble longitude,
                 const gchar *expected)
{
    g_autofree gchar *actual =
        calendar_plus_format_time(parsed,
                                  unix_microseconds,
                                  utc_offset_seconds,
                                  show_seconds,
                                  vertical,
                                  longitude);

    g_assert_cmpint(parsed, !=, CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpstr(calendar_plus_time_mode_get_id(parsed), ==, mode);
    g_assert_cmpstr(actual, ==, expected);
}

static void
assert_time_at_location(const gchar *mode,
                        gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gboolean vertical,
                        gdouble latitude,
                        gdouble longitude,
                        const gchar *expected)
{
    const CalendarPlusTimeMode parsed =
        calendar_plus_time_mode_from_string(mode);
    g_autofree gchar *actual =
        calendar_plus_format_time_at_location(
            parsed,
            unix_microseconds,
            utc_offset_seconds,
            show_seconds,
            vertical,
            latitude,
            longitude);

    g_assert_cmpint(parsed, !=, CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpstr(calendar_plus_time_mode_get_id(parsed), ==, mode);
    g_assert_cmpstr(actual, ==, expected);
}
static void
assert_time(const gchar *mode,
            gint64 unix_microseconds,
            gint utc_offset_seconds,
            gboolean show_seconds,
            gboolean vertical,
            gdouble longitude,
            const gchar *expected)
{
    const CalendarPlusTimeMode parsed =
        calendar_plus_time_mode_from_string(mode);

    assert_time_mode(mode,
                     parsed,
                     unix_microseconds,
                     utc_offset_seconds,
                     show_seconds,
                     vertical,
                     longitude,
                     expected);
}

static void
test_mode_parser(void)
{
    const gchar *valid_modes[] = {
        "decimal",
        "internet",
        "unix",
        "hexadecimal",
        "binary",
        "sidereal",
        "solar",
        "julian",
        "mean-solar",
        "modified-julian",
        "chinese-time",
        "roman-temporal",
        "japanese-temporal",
        "italian-hours",
        "babylonian-hours",
        "indian-ghati",
        "chinese-ke",
        "nuremberg-hours"
    };
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(valid_modes); index++)
    {
        g_assert_cmpint(
            calendar_plus_time_mode_from_string(valid_modes[index]),
            !=,
            CALENDAR_PLUS_TIME_MODE_INVALID);
    }

    g_assert_cmpint(calendar_plus_time_mode_from_string("standard"),
                    ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(calendar_plus_time_mode_from_string("decimal@0.0000"),
                    ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(
        calendar_plus_time_mode_from_string("roman-temporal@0.0000"),
        ==,
        CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(
        calendar_plus_time_mode_from_string("japanese-temporal@0.0000"),
        ==,
        CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(
        calendar_plus_time_mode_from_string("italian-hours@0.0000"),
        ==,
        CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(
        calendar_plus_time_mode_from_string("babylonian-hours@0.0000"),
        ==,
        CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(calendar_plus_time_mode_from_string("unknown"),
                    ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
    g_assert_cmpint(calendar_plus_time_mode_from_string(NULL),
                    ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
}

static void
test_decimal_time(void)
{
    assert_time("decimal", USECONDS(0), 0,
                TRUE, FALSE, 0.0, "0:00:00");
    assert_time("decimal", USECONDS(12 * 3600), 0,
                TRUE, FALSE, 0.0, "5:00:00");
    assert_time("decimal", USECONDS(21 * 3600 + 5 * 60 + 6), 0,
                TRUE, FALSE, 0.0, "8:78:54");
    assert_time("decimal", USECONDS(21 * 3600 + 5 * 60 + 6), 0,
                FALSE, FALSE, 0.0, "8:78");
    assert_time("decimal", USECONDS(21 * 3600 + 5 * 60 + 6), 0,
                TRUE, TRUE, 0.0, "8\n78\n54");
}

static void
test_internet_time(void)
{
    /* Internet Time is fixed to Biel Mean Time (UTC+1), without DST. */
    assert_time("internet", USECONDS(0), 0,
                FALSE, FALSE, 0.0, "@041");
    assert_time("internet", USECONDS(0), 0,
                TRUE, FALSE, 0.0, "@041.66");
    assert_time("internet", USECONDS(23 * 3600), 39600,
                FALSE, FALSE, 145.0, "@000");
}

static void
test_unix_hexadecimal_and_binary(void)
{
    assert_time("unix", USECONDS(1234567890), 0,
                FALSE, FALSE, 0.0, "1234567890");
    assert_time("hexadecimal", USECONDS(12 * 3600), 0,
                FALSE, FALSE, 0.0, "8000");
    assert_time("binary", USECONDS(21 * 3600 + 5 * 60 + 6), 0,
                FALSE, FALSE, 0.0, "10101:000101");
    assert_time("binary", USECONDS(21 * 3600 + 5 * 60 + 6), 0,
                TRUE, FALSE, 0.0, "10101:000101:000110");
}

static void
test_astronomical_times(void)
{
    /*
     * J2000 is 2000-01-01 12:00:00 UTC. Its established Greenwich mean
     * sidereal time is 18.697374558 hours.
     */
    const gint64 j2000 = USECONDS(946728000);

    assert_time("sidereal", j2000, 0,
                TRUE, FALSE, 0.0, "18:41:50 LST");
    assert_time("sidereal", j2000, 0,
                FALSE, FALSE, 15.0, "19:41 LST");

    /*
     * The compact NOAA equation-of-time approximation places apparent solar
     * noon at Greenwich just under three minutes before mean noon on J2000.
     */
    assert_time("solar", j2000, 0,
                TRUE, FALSE, 0.0, "11:57:05 SOL");

    assert_time("julian", USECONDS(0), 0,
                TRUE, FALSE, 0.0, "JD 2440587.50000");
    assert_time("julian", USECONDS(0), 0,
                FALSE, TRUE, 0.0, "JD\n2440587.500");
}

static void
test_historical_and_scientific_times(void)
{
    const gint64 j2000 = USECONDS(946728000);

    /* Fifteen degrees east is one mean-solar hour ahead of Greenwich. */
    assert_time("mean-solar", USECONDS(0), 0,
                TRUE, FALSE, 15.0, "01:00:00 LMT");

    /* MJD 40587.0 is the Unix epoch, 1970-01-01 00:00:00 UTC. */
    assert_time("modified-julian", USECONDS(0), 0,
                TRUE, FALSE, 0.0, "MJD 40587.00000");
    assert_time("modified-julian", USECONDS(0), 0,
                FALSE, TRUE, 0.0, "MJD\n40587.000");

    /* Zi spans 23:00-01:00 and Wu spans 11:00-13:00 civil time. */
    assert_time("chinese-time", USECONDS(0), 0,
                FALSE, FALSE, 0.0, "子 Zǐ (Rat)");
    assert_time("chinese-time", USECONDS(11 * 3600), 0,
                FALSE, FALSE, 0.0, "午 Wǔ (Horse)");
    /* Vertical layout changes separators only; it preserves pinyin tones. */
    assert_time("chinese-time", USECONDS(0), 0,
                FALSE, TRUE, 0.0, "子\nZǐ\n(Rat)");

    /* J2000 is just before apparent noon at Greenwich in the compact solar
     * model, placing it in the sixth Roman hour and third daytime toki. */
    assert_time_at_location("roman-temporal", j2000, 0,
                            FALSE, FALSE, 0.0, 0.0, "Hora VI");
    assert_time_at_location("japanese-temporal", j2000, 0,
                            FALSE, FALSE, 0.0, 0.0, "巳 4 Snake");

    /*
     * Equal-hour sundial conventions keep ordinary 60-minute hours but move
     * the start of the 24-hour count to the physical sunset or sunrise.
     */
    assert_time_at_location("italian-hours", j2000, 0,
                            TRUE, FALSE, 0.0, 0.0, "17:53:55 IT");
    assert_time_at_location("babylonian-hours", j2000, 0,
                            TRUE, FALSE, 0.0, 0.0, "06:00:43 BAB");
    assert_time_at_location("indian-ghati", j2000, 0,
                            FALSE, FALSE, 0.0, 0.0, "GH 15:01");
    assert_time("chinese-ke", USECONDS(0), 0,
                FALSE, FALSE, 0.0, "刻 00/100");
    assert_time_at_location("nuremberg-hours", j2000, 0,
                            TRUE, FALSE, 0.0, 0.0,
                            "06:00:43 NUR-D");
    assert_time("chinese-ke", USECONDS(12 * 3600), 0,
                FALSE, FALSE, 0.0, "刻 50/100");

    /* At a pole near the June solstice no requested solar boundary exists. */
    assert_time_at_location("roman-temporal", USECONDS(962409600), 0,
                            FALSE, FALSE, 90.0, 0.0, "N/A ROM");
    assert_time_at_location("japanese-temporal", USECONDS(962409600), 0,
                            FALSE, FALSE, 90.0, 0.0, "N/A 和時");
    assert_time_at_location("italian-hours", USECONDS(962409600), 0,
                            FALSE, FALSE, 90.0, 0.0, "N/A IT");
    assert_time_at_location("babylonian-hours", USECONDS(962409600), 0,
                            FALSE, FALSE, 90.0, 0.0, "N/A BAB");
    assert_time_at_location("indian-ghati", USECONDS(962409600), 0,
                            FALSE, FALSE, 90.0, 0.0, "N/A GH");
}

static void
test_tick_boundaries(void)
{
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_DECIMAL, 0, 0, TRUE, 0.0),
        ==,
        864);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_DECIMAL, 0, 0, FALSE, 0.0),
        ==,
        86400);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_UNIX, 0, 0, FALSE, 0.0),
        ==,
        1000);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_HEXADECIMAL, 0, 0, FALSE, 0.0),
        ==,
        1319);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_BINARY, 0, 0, FALSE, 0.0),
        ==,
        60000);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_JULIAN, 0, 0, TRUE, 0.0),
        ==,
        864);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_MEAN_SOLAR, 0, 0, TRUE, 15.0),
        ==,
        1000);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_MODIFIED_JULIAN, 0, 0, TRUE, 0.0),
        ==,
        864);
    g_assert_cmpuint(
        calendar_plus_time_delay_to_next_tick(
            CALENDAR_PLUS_TIME_MODE_CHINESE, 0, 0, FALSE, 0.0),
        ==,
        3600000);
    {
        const CalendarPlusTimeMode roman_mode =
            calendar_plus_time_mode_from_string("roman-temporal");
        const CalendarPlusTimeMode japanese_mode =
            calendar_plus_time_mode_from_string("japanese-temporal");
        const guint roman_delay =
            calendar_plus_time_delay_to_next_tick_at_location(
                roman_mode, USECONDS(946728000), 0, FALSE, 0.0, 0.0);
        const guint japanese_delay =
            calendar_plus_time_delay_to_next_tick_at_location(
                japanese_mode, USECONDS(946728000), 0, FALSE, 0.0, 0.0);

        /* Floating-point trig may round the boundary by a few milliseconds
         * across libm implementations; the physical boundary is unchanged. */
        g_assert_cmpuint(roman_delay, >=, 174200);
        g_assert_cmpuint(roman_delay, <=, 174300);
        g_assert_cmpuint(japanese_delay, >=, 174200);
        g_assert_cmpuint(japanese_delay, <=, 174300);
    }
}

static void
test_label_replacement(void)
{
    g_autofree gchar *replaced =
        calendar_plus_replace_time("Tuesday, July 28, 22:03",
                                   "22:03",
                                   "@918");
    g_autofree gchar *missing =
        calendar_plus_replace_time("Tuesday, July 28",
                                   "22:03",
                                   "@918");

    g_assert_cmpstr(replaced, ==, "Tuesday, July 28, @918");
    g_assert_null(missing);
}

static void
test_clock_lifecycle(void)
{
    g_autoptr(CalendarPlusSystemClock) clock =
        calendar_plus_system_clock_new();
    g_autofree gchar *current_time = NULL;

    g_assert_false(calendar_plus_system_clock_is_running(clock));

    calendar_plus_system_clock_start(clock,
                                     "decimal",
                                     TRUE,
                                     FALSE,
                                     0.0);
    g_assert_true(calendar_plus_system_clock_is_running(clock));

    current_time = calendar_plus_system_clock_get_time(clock);
    g_assert_nonnull(current_time);
    g_assert_nonnull(strchr(current_time, ':'));

    calendar_plus_system_clock_start(clock,
                                     "internet",
                                     FALSE,
                                     FALSE,
                                     0.0);
    g_clear_pointer(&current_time, g_free);
    current_time = calendar_plus_system_clock_get_time(clock);
    g_assert_true(g_str_has_prefix(current_time, "@"));

    calendar_plus_system_clock_start_at_location(
        clock, "roman-temporal", FALSE, FALSE, 0.0, 0.0);
    g_clear_pointer(&current_time, g_free);
    current_time = calendar_plus_system_clock_get_time(clock);
    g_assert_true(g_str_has_prefix(current_time, "Hora") ||
                  g_str_has_prefix(current_time, "Vigilia"));

    calendar_plus_system_clock_stop(clock);
    g_assert_false(calendar_plus_system_clock_is_running(clock));

    /* Stop is deliberately idempotent for Cinnamon applet teardown. */
    calendar_plus_system_clock_stop(clock);
    g_assert_false(calendar_plus_system_clock_is_running(clock));
}

static gchar *
calendar_format(const gchar *calendar_id,
                gint year,
                gint month,
                gint day,
                const gchar *part)
{
    g_autoptr(CalendarPlusCalendarSystem) calendar =
        calendar_plus_calendar_system_new(calendar_id);

    g_assert_nonnull(calendar);
    return calendar_plus_calendar_system_format_date(calendar,
                                                      year,
                                                      month,
                                                      day,
                                                      part);
}

static void
parse_iso_date(const gchar *iso_date,
               gint *year,
               gint *month,
               gint *day)
{
    g_auto(GStrv) fields = g_strsplit(iso_date, "-", 3);

    g_assert_nonnull(fields[0]);
    g_assert_nonnull(fields[1]);
    g_assert_nonnull(fields[2]);
    *year = (gint)g_ascii_strtoll(fields[0], NULL, 10);
    *month = (gint)g_ascii_strtoll(fields[1], NULL, 10);
    *day = (gint)g_ascii_strtoll(fields[2], NULL, 10);
}

static void
test_calendar_catalogue(void)
{
    const gchar *calendar_ids[] = {
        "gregorian",
        "julian",
        "iso-week",
        "hebrew",
        "islamic",
        "islamic-civil",
        "islamic-umalqura",
        "persian",
        "chinese",
        "indian",
        "coptic",
        "ethiopian",
        "buddhist",
        "japanese",
        "minguo",
        "french-republican",
        "roman",
        "mayan",
        "bahai",
        "international-fixed",
        "world",
        "positivist",
        "revised-julian",
        "byzantine",
        "egyptian-nabonassar",
        "dangi",
        "ethiopic-amete-alem",
        "islamic-tbla",
        "armenian-traditional",
        "swedish-historical"
    };
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(calendar_ids); index++)
    {
        g_autoptr(CalendarPlusCalendarSystem) calendar =
            calendar_plus_calendar_system_new(calendar_ids[index]);
        g_autofree gchar *formatted = NULL;
        g_autofree gchar *month_start = NULL;
        g_autofree gchar *month_key = NULL;

        g_assert_nonnull(calendar);
        g_assert_cmpstr(
            calendar_plus_calendar_system_get_id(calendar),
            ==,
            calendar_ids[index]);
        g_assert_cmpstr(
            calendar_plus_calendar_system_get_name(calendar),
            !=,
            "");

        formatted =
            calendar_plus_calendar_system_format_date(calendar,
                                                      2026,
                                                      7,
                                                      29,
                                                      "short");
        month_start =
            calendar_plus_calendar_system_month_start(calendar,
                                                      2026,
                                                      7,
                                                      29);
        month_key =
            calendar_plus_calendar_system_month_key(calendar,
                                                    2026,
                                                    7,
                                                    29);
        g_assert_cmpstr(formatted, !=, "");
        g_assert_cmpstr(month_start, !=, "");
        g_assert_cmpstr(month_key, !=, "");
    }

    g_assert_null(calendar_plus_calendar_system_new("unknown"));
    g_assert_null(calendar_plus_calendar_system_new(NULL));
}

static void
test_locale_workday_policy(void)
{
    gboolean known = FALSE;

    g_assert_true(
        calendar_plus_icu_is_work_day_for_locale("en_US", 1, &known));
    g_assert_true(known);

    known = FALSE;
    g_assert_false(
        calendar_plus_icu_is_work_day_for_locale("en_US", 7, &known));
    g_assert_true(known);

    /*
     * Saudi Arabia's CLDR weekend is Friday/Saturday. This proves that the
     * calendar no longer hard-codes a Saturday/Sunday Western weekend.
     */
    known = FALSE;
    g_assert_false(
        calendar_plus_icu_is_work_day_for_locale("ar_SA", 5, &known));
    g_assert_true(known);

    known = FALSE;
    g_assert_false(
        calendar_plus_icu_is_work_day_for_locale("ar_SA", 6, &known));
    g_assert_true(known);

    known = FALSE;
    g_assert_true(
        calendar_plus_icu_is_work_day_for_locale("ar_SA", 7, &known));
    g_assert_true(known);
}

static void
test_locale_workday_grid_policy(void)
{
    CalendarPlusCalendarEngine *engine =
        calendar_plus_calendar_engine_new("gregorian");
    const CalendarPlusDate selected = { 2026, 8, 15 };
    const CalendarPlusDate today = { 2026, 8, 15 };
    CalendarPlusCalendarGrid grid = { 0 };
    gboolean saw_friday = FALSE;
    gboolean saw_saturday = FALSE;
    gboolean saw_sunday = FALSE;
    guint index;

    g_assert_nonnull(engine);
    g_assert_true(calendar_plus_calendar_engine_build_grid_for_locale(
        engine, &selected, &today, 0, "ar_SA", &grid));

    for (index = 0; index < CALENDAR_PLUS_CALENDAR_GRID_CELLS; index++)
    {
        const CalendarPlusCalendarCell *cell = &grid.cells[index];

        if (cell->date.year != 2026 || cell->date.month != 8)
            continue;

        if (cell->date.day == 14)
        {
            saw_friday = TRUE;
            g_assert_false(cell->is_work_day);
        }
        else if (cell->date.day == 15)
        {
            saw_saturday = TRUE;
            g_assert_false(cell->is_work_day);
        }
        else if (cell->date.day == 16)
        {
            saw_sunday = TRUE;
            g_assert_true(cell->is_work_day);
        }
    }

    g_assert_true(saw_friday);
    g_assert_true(saw_saturday);
    g_assert_true(saw_sunday);
    calendar_plus_calendar_grid_clear(&grid);

    /*
     * Keep the grid contract's rejection branches exercised alongside the
     * locale regression. These are caller-boundary failures, not assertions
     * about implementation structure.
     */
    g_assert_false(calendar_plus_calendar_engine_build_grid_for_locale(
        NULL, &selected, &today, 0, "ar_SA", &grid));
    g_assert_false(calendar_plus_calendar_engine_build_grid_for_locale(
        engine, &selected, &today, -1, "ar_SA", &grid));
    g_assert_false(calendar_plus_calendar_engine_build_grid_for_locale(
        engine, &selected, &today, 7, "ar_SA", &grid));
    {
        const CalendarPlusDate invalid = { 2026, 2, 30 };
        g_assert_false(calendar_plus_calendar_engine_build_grid_for_locale(
            engine, &invalid, &today, 0, "ar_SA", &grid));
    }
    g_assert_false(calendar_plus_calendar_engine_build_grid_for_locale(
        engine, &selected, &today, 0, "ar_SA", NULL));

    calendar_plus_calendar_engine_free(engine);
}

static void
test_historical_calendar_references(void)
{
    g_autofree gchar *julian =
        calendar_format("julian", 2026, 7, 29, "short");
    g_autofree gchar *iso =
        calendar_format("iso-week", 2026, 7, 29, "short");
    g_autofree gchar *french =
        calendar_format("french-republican", 1792, 9, 22, "short");
    g_autofree gchar *roman =
        calendar_format("roman", 2026, 7, 29, "short");
    g_autofree gchar *mayan =
        calendar_format("mayan", 2012, 12, 21, "short");
    g_autofree gchar *bahai =
        calendar_format("bahai", 2026, 3, 21, "short");

    g_assert_cmpstr(julian, ==, "16 July 2026");
    g_assert_cmpstr(iso, ==, "2026-W31-3");
    g_assert_cmpstr(french, ==, "1 Vendémiaire, An I");
    g_assert_cmpstr(roman, ==, "a.d. XVII Kal. Aug., MMDCCLXXIX A.U.C.");
    {
        /* Gregorian 8-13 March 2024 are Julian 24-29 February. */
        g_autofree gchar *bis_vi =
            calendar_format("roman", 2024, 3, 8, "short");
        g_autofree gchar *vi =
            calendar_format("roman", 2024, 3, 9, "short");
        g_autofree gchar *v =
            calendar_format("roman", 2024, 3, 10, "short");
        g_autofree gchar *pridie =
            calendar_format("roman", 2024, 3, 13, "short");

        g_assert_cmpstr(bis_vi, ==,
                        "a.d. bis VI Kal. Mar., MMDCCLXXVII A.U.C.");
        g_assert_cmpstr(vi, ==,
                        "a.d. VI Kal. Mar., MMDCCLXXVII A.U.C.");
        g_assert_cmpstr(v, ==,
                        "a.d. V Kal. Mar., MMDCCLXXVII A.U.C.");
        g_assert_cmpstr(pridie, ==,
                        "prid. Kal. Mar., MMDCCLXXVII A.U.C.");
    }
    g_assert_cmpstr(mayan, ==,
                    "13.0.0.0.0 · 4 Ajaw · 3 K’ank’in");
    g_assert_cmpstr(bahai, ==, "1 Bahá 183 B.E.");
    {
        g_autofree gchar *swedish_1700 = calendar_format("swedish-historical", 1700, 3, 11, "short");
        g_autofree gchar *swedish_1712 = calendar_format("swedish-historical", 1712, 3, 11, "short");
        g_autofree gchar *swedish_1753 = calendar_format("swedish-historical", 1753, 3, 1, "short");
        g_assert_cmpstr(swedish_1700, ==, "1 March 1700");
        g_assert_cmpstr(swedish_1712, ==, "30 February 1712");
        g_assert_cmpstr(swedish_1753, ==, "1 March 1753");
    }
    {
        g_autofree gchar *bahai_2015 =
            calendar_format("bahai", 2015, 3, 21, "short");
        g_autofree gchar *bahai_2016 =
            calendar_format("bahai", 2016, 3, 20, "short");

        g_assert_cmpstr(bahai_2015, ==, "1 Bahá 172 B.E.");
        g_assert_cmpstr(bahai_2016, ==, "1 Bahá 173 B.E.");
    }
}

static void
test_calendar_navigation(void)
{
    g_autoptr(CalendarPlusCalendarSystem) julian =
        calendar_plus_calendar_system_new("julian");
    g_autoptr(CalendarPlusCalendarSystem) iso =
        calendar_plus_calendar_system_new("iso-week");
    g_autoptr(CalendarPlusCalendarSystem) fixed =
        calendar_plus_calendar_system_new("international-fixed");
    g_autofree gchar *julian_start =
        calendar_plus_calendar_system_month_start(julian,
                                                  2026,
                                                  7,
                                                  29);
    g_autofree gchar *julian_next =
        calendar_plus_calendar_system_add_months(julian,
                                                 2026,
                                                 7,
                                                 29,
                                                 1);
    g_autofree gchar *iso_start =
        calendar_plus_calendar_system_month_start(iso,
                                                  2026,
                                                  7,
                                                  29);
    g_autofree gchar *iso_next =
        calendar_plus_calendar_system_add_months(iso,
                                                 2026,
                                                 7,
                                                 29,
                                                 1);
    g_autofree gchar *leap_day =
        calendar_plus_calendar_system_format_date(fixed,
                                                  2024,
                                                  6,
                                                  17,
                                                  "short");

    g_assert_cmpstr(julian_start, ==, "2026-07-14");
    g_assert_cmpstr(julian_next, ==, "2026-08-29");
    g_assert_cmpstr(iso_start, ==, "2026-07-27");
    g_assert_cmpstr(iso_next, ==, "2026-08-05");
    g_assert_cmpstr(leap_day, ==, "Leap Day, 2024");
}

static void
test_calendar_round_trips(void)
{
    const gchar *calendar_ids[] = {
        "gregorian",
        "julian",
        "iso-week",
        "hebrew",
        "islamic",
        "islamic-civil",
        "islamic-umalqura",
        "persian",
        "chinese",
        "indian",
        "coptic",
        "ethiopian",
        "buddhist",
        "japanese",
        "minguo",
        "french-republican",
        "roman",
        "mayan",
        "bahai",
        "international-fixed",
        "world",
        "positivist",
        "revised-julian",
        "byzantine",
        "egyptian-nabonassar",
        "dangi",
        "ethiopic-amete-alem",
        "islamic-tbla",
        "armenian-traditional",
        "swedish-historical"
    };
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(calendar_ids); index++)
    {
        g_autoptr(CalendarPlusCalendarSystem) calendar =
            calendar_plus_calendar_system_new(calendar_ids[index]);
        g_autofree gchar *month_start =
            calendar_plus_calendar_system_month_start(calendar,
                                                      2026,
                                                      7,
                                                      29);
        g_autofree gchar *original_key =
            calendar_plus_calendar_system_month_key(calendar,
                                                    2026,
                                                    7,
                                                    29);
        g_autofree gchar *start_key = NULL;
        g_autofree gchar *next_month =
            calendar_plus_calendar_system_add_months(calendar,
                                                     2026,
                                                     7,
                                                     29,
                                                     1);
        g_autofree gchar *previous_month = NULL;
        g_autofree gchar *next_year =
            calendar_plus_calendar_system_add_years(calendar,
                                                    2026,
                                                    7,
                                                    29,
                                                    1);
        g_autofree gchar *previous_year = NULL;
        g_autoptr(GVariant) grid =
            calendar_plus_calendar_system_build_grid(calendar,
                                                     2026,
                                                     7,
                                                     29,
                                                     2026,
                                                     7,
                                                     29,
                                                     1);
        gint year;
        gint month;
        gint day;

        g_assert_nonnull(grid);
        g_assert_cmpuint(g_variant_n_children(grid), ==, 42);

        parse_iso_date(month_start, &year, &month, &day);
        start_key =
            calendar_plus_calendar_system_month_key(calendar,
                                                    year,
                                                    month,
                                                    day);
        g_assert_cmpstr(start_key, ==, original_key);

        parse_iso_date(next_month, &year, &month, &day);
        previous_month =
            calendar_plus_calendar_system_add_months(calendar,
                                                     year,
                                                     month,
                                                     day,
                                                     -1);
        g_assert_cmpstr(previous_month, ==, "2026-07-29");

        parse_iso_date(next_year, &year, &month, &day);
        previous_year =
            calendar_plus_calendar_system_add_years(calendar,
                                                    year,
                                                    month,
                                                    day,
                                                    -1);
        g_assert_cmpstr(previous_year, ==, "2026-07-29");
    }
}

static void
test_calendar_grid(void)
{
    g_autoptr(CalendarPlusCalendarSystem) gregorian =
        calendar_plus_calendar_system_new("gregorian");
    g_autoptr(GVariant) grid =
        calendar_plus_calendar_system_build_grid(gregorian,
                                                 2026,
                                                 7,
                                                 29,
                                                 2026,
                                                 7,
                                                 29,
                                                 0);
    guint selected_count = 0;
    guint today_count = 0;
    guint week_number_count = 0;
    gsize index;

    g_assert_nonnull(grid);
    g_assert_true(
        g_variant_is_of_type(grid,
                             G_VARIANT_TYPE("a(siiiiiibbbbbb)")));
    g_assert_cmpuint(g_variant_n_children(grid), ==, 42);

    for (index = 0; index < g_variant_n_children(grid); index++)
    {
        g_autoptr(GVariant) cell =
            g_variant_get_child_value(grid, index);
        const gchar *day_label;
        gint year;
        gint month;
        gint day;
        gint row;
        gint column;
        gint week_number;
        gboolean is_work_day;
        gboolean is_today;
        gboolean is_selected;
        gboolean is_current_period;
        gboolean is_top_row;
        gboolean is_left_edge;

        g_variant_get(cell,
                      "(&siiiiiibbbbbb)",
                      &day_label,
                      &year,
                      &month,
                      &day,
                      &row,
                      &column,
                      &week_number,
                      &is_work_day,
                      &is_today,
                      &is_selected,
                      &is_current_period,
                      &is_top_row,
                      &is_left_edge);

        g_assert_cmpstr(day_label, !=, "");
        g_assert_true(calendar_plus_gregorian_date_is_valid(year,
                                                             month,
                                                             day));
        g_assert_cmpint(row, ==, 2 + (gint)index / 7);
        g_assert_cmpint(column, ==, (gint)index % 7);
        g_assert_cmpint(is_top_row, ==, index < 7);
        g_assert_cmpint(is_left_edge, ==, column == 0);
        g_assert_cmpint(is_work_day,
                        ==,
                        calendar_plus_date_is_work_day(year,
                                                       month,
                                                       day));

        if (week_number > 0)
            week_number_count++;
        if (is_today)
            today_count++;
        if (is_selected)
        {
            selected_count++;
            g_assert_cmpint(year, ==, 2026);
            g_assert_cmpint(month, ==, 7);
            g_assert_cmpint(day, ==, 29);
            g_assert_true(is_current_period);
            g_assert_true(is_work_day);
        }

        if (index == 0)
        {
            g_assert_cmpint(year, ==, 2026);
            g_assert_cmpint(month, ==, 6);
            g_assert_cmpint(day, ==, 28);
            g_assert_false(is_current_period);
            g_assert_false(is_work_day);
        }
    }

    g_assert_cmpuint(selected_count, ==, 1);
    g_assert_cmpuint(today_count, ==, 1);
    g_assert_cmpuint(week_number_count, ==, 6);
}

static gint64
test_local_time(gint year,
                gint month,
                gint day,
                gint hour,
                gint minute)
{
    g_autoptr(GDateTime) value =
        g_date_time_new_local(year,
                              month,
                              day,
                              hour,
                              minute,
                              0.0);

    g_assert_nonnull(value);
    return g_date_time_to_unix(value);
}

static void
test_native_contract_helpers(void)
{
    const gint64 monday = test_local_time(2026, 8, 10, 12, 0);
    const gint64 friday = test_local_time(2026, 8, 14, 12, 0);
    const gint64 saturday = test_local_time(2026, 8, 15, 12, 0);
    const gint64 sunday = test_local_time(2026, 8, 16, 12, 0);
    const gint64 start = test_local_time(2026, 8, 14, 9, 0);
    const gint64 end = test_local_time(2026, 8, 14, 10, 0);
    const gint64 now = test_local_time(2026, 8, 14, 9, 30);
    g_autoptr(GVariant) timing =
        calendar_plus_event_timing(start, end, now);
    gint state;
    gint64 seconds_to_start;
    gint64 seconds_to_finish;
    guint relation;

    g_assert_cmpstr(calendar_plus_get_version(), ==, CALENDAR_PLUS_VERSION);
    g_assert_cmpstr(calendar_plus_get_source_id(), ==,
                    CALENDAR_PLUS_SOURCE_ID);

    g_assert_true(calendar_plus_date_same(2026, 8, 14,
                                          2026, 8, 14));
    g_assert_false(calendar_plus_date_same(2026, 8, 14,
                                           2026, 8, 15));
    g_assert_false(calendar_plus_date_same(2026, 2, 30,
                                           2026, 2, 30));
    g_assert_true(calendar_plus_date_is_work_day(2026, 8, 10));
    g_assert_true(calendar_plus_date_is_work_day(2026, 8, 14));
    g_assert_false(calendar_plus_date_is_work_day(2026, 8, 15));
    g_assert_false(calendar_plus_date_is_work_day(2026, 8, 16));
    (void)monday;
    (void)friday;

    relation = calendar_plus_event_day_relation(
        test_local_time(2026, 8, 13, 0, 0),
        test_local_time(2026, 8, 15, 0, 0),
        test_local_time(2026, 8, 14, 12, 0));
    g_assert_true((relation &
                   CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_BEFORE_DAY) != 0);
    g_assert_true((relation &
                   CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_AFTER_DAY) != 0);
    g_assert_false((relation &
                    CALENDAR_PLUS_EVENT_DAY_RELATION_STARTS_ON_DAY) != 0);

    g_assert_cmpint(calendar_plus_event_state(start, end, now),
                    ==,
                    CALENDAR_PLUS_EVENT_STATE_PRESENT);
    g_assert_cmpint(calendar_plus_event_state(start, end, start - 1),
                    ==,
                    CALENDAR_PLUS_EVENT_STATE_FUTURE);
    g_assert_cmpint(calendar_plus_event_state(start, end, end + 1),
                    ==,
                    CALENDAR_PLUS_EVENT_STATE_PAST);
    g_assert_cmpint(calendar_plus_event_state(end, start, now),
                    ==,
                    CALENDAR_PLUS_EVENT_STATE_INVALID);

    g_variant_get(timing,
                  "(ixx)",
                  &state,
                  &seconds_to_start,
                  &seconds_to_finish);
    g_assert_cmpint(state, ==, CALENDAR_PLUS_EVENT_STATE_PRESENT);
    g_assert_cmpint(seconds_to_start, ==, -30 * 60);
    g_assert_cmpint(seconds_to_finish, ==, 30 * 60);

    g_assert_cmpint(saturday, >, friday);
    g_assert_cmpint(sunday, >, saturday);
}

static GVariant *
test_event_variant(const gchar *id,
                   const gchar *color,
                   const gchar *summary,
                   gboolean all_day,
                   gint64 start,
                   gint64 end,
                   gint64 modified)
{
    return g_variant_ref_sink(
        g_variant_new("(sssbxxx)",
                      id,
                      color,
                      summary,
                      all_day,
                      start,
                      end,
                      modified));
}

static void
test_event_store(void)
{
    const gint64 day = test_local_time(2026, 7, 29, 0, 0);
    const gint64 next_day = test_local_time(2026, 7, 30, 0, 0);
    const gint64 now = test_local_time(2026, 7, 29, 12, 0);
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GVariant) past =
        test_event_variant("past",
                           "#111111",
                           "Past",
                           FALSE,
                           test_local_time(2026, 7, 29, 9, 0),
                           test_local_time(2026, 7, 29, 10, 0),
                           1);
    g_autoptr(GVariant) all_day =
        test_event_variant("all-day",
                           "#222222",
                           "All day",
                           TRUE,
                           day,
                           next_day,
                           1);
    g_autoptr(GVariant) future =
        test_event_variant("future",
                           "#333333",
                           "Future",
                           FALSE,
                           test_local_time(2026, 7, 29, 15, 0),
                           test_local_time(2026, 7, 29, 16, 0),
                           1);
    g_autoptr(GVariant) snapshot = NULL;
    g_autoptr(GVariant) rows = NULL;
    gint64 revision;
    g_auto(GStrv) colors = NULL;

    g_assert_true(
        calendar_plus_event_store_add_or_update(store, future, 100));
    g_assert_true(
        calendar_plus_event_store_add_or_update(store, all_day, 100));
    g_assert_true(
        calendar_plus_event_store_add_or_update(store, past, 100));

    snapshot =
        calendar_plus_event_store_get_snapshot(store, day, now);
    g_assert_true(
        g_variant_is_of_type(snapshot,
                             G_VARIANT_TYPE("(xa(sssbbxxxxx))")));
    g_variant_get(snapshot,
                  "(x@a(sssbbxxxxx))",
                  &revision,
                  &rows);
    g_assert_cmpint(revision, >, 0);
    g_assert_cmpuint(g_variant_n_children(rows), ==, 3);

    {
        const gchar *expected_ids[] = {
            "past", "all-day", "future"
        };
        gsize index;

        for (index = 0; index < G_N_ELEMENTS(expected_ids); index++)
        {
            g_autoptr(GVariant) row =
                g_variant_get_child_value(rows, index);
            g_autoptr(GVariant) id =
                g_variant_get_child_value(row, 0);

            g_assert_cmpstr(g_variant_get_string(id, NULL),
                            ==,
                            expected_ids[index]);
        }
    }

    {
        g_autoptr(GVariant) row =
            g_variant_get_child_value(rows, 1);
        gboolean returned_all_day;
        gboolean multi_day;
        gint64 start;
        gint64 end;
        gint64 start_day;
        gint64 end_day;
        gint64 modified;
        const gchar *id;
        const gchar *color;
        const gchar *summary;

        g_variant_get(row,
                      "(&s&s&sbbxxxxx)",
                      &id,
                      &color,
                      &summary,
                      &returned_all_day,
                      &multi_day,
                      &start,
                      &end,
                      &start_day,
                      &end_day,
                      &modified);
        g_assert_true(returned_all_day);
        g_assert_false(multi_day);
        g_assert_cmpint(start, ==, day);
        g_assert_cmpint(end, ==, next_day - 1);
        g_assert_cmpint(start_day, ==, day);
        g_assert_cmpint(end_day, ==, day);
        g_assert_cmpint(modified, ==, 1);
    }

    colors = calendar_plus_event_store_get_colors(store, day, now);
    if (colors == NULL)
        g_error("event store returned a NULL colour vector");

    g_assert_cmpuint(g_strv_length(colors), ==, 3);
    g_assert_cmpstr(colors[0], ==, "#111111");
    g_assert_cmpstr(colors[1], ==, "#222222");
    g_assert_cmpstr(colors[2], ==, "#333333");

    g_assert_true(calendar_plus_event_store_remove(store,
                                                   "past::future"));
    g_assert_false(calendar_plus_event_store_remove(store, "missing"));
    g_assert_true(calendar_plus_event_store_cull(store, 101));

    g_clear_pointer(&snapshot, g_variant_unref);
    snapshot =
        calendar_plus_event_store_get_snapshot(store, day, now);
    g_clear_pointer(&rows, g_variant_unref);
    g_variant_get(snapshot,
                  "(x@a(sssbbxxxxx))",
                  &revision,
                  &rows);
    g_assert_cmpuint(g_variant_n_children(rows), ==, 0);
}

static void
test_event_store_multiday(void)
{
    const gint64 first_day = test_local_time(2026, 10, 3, 0, 0);
    const gint64 middle_day = test_local_time(2026, 10, 4, 0, 0);
    const gint64 final_day = test_local_time(2026, 10, 5, 0, 0);
    g_autoptr(CalendarPlusEventStore) store =
        calendar_plus_event_store_new();
    g_autoptr(GVariant) event =
        test_event_variant("conference",
                           "#8844cc",
                           "Conference",
                           FALSE,
                           test_local_time(2026, 10, 3, 20, 0),
                           test_local_time(2026, 10, 5, 8, 0),
                           7);
    gint64 days[] = {
        first_day,
        middle_day,
        final_day
    };
    gsize index;

    g_assert_true(
        calendar_plus_event_store_add_or_update(store, event, 500));

    for (index = 0; index < G_N_ELEMENTS(days); index++)
    {
        g_autoptr(GVariant) snapshot =
            calendar_plus_event_store_get_snapshot(store,
                                                   days[index],
                                                   test_local_time(2026,
                                                                   10,
                                                                   4,
                                                                   12,
                                                                   0));
        g_autoptr(GVariant) rows = NULL;
        g_autoptr(GVariant) row = NULL;
        gint64 revision;
        gboolean multi_day;

        g_variant_get(snapshot,
                      "(x@a(sssbbxxxxx))",
                      &revision,
                      &rows);
        g_assert_cmpuint(g_variant_n_children(rows), ==, 1);
        row = g_variant_get_child_value(rows, 0);
        g_variant_get_child(row, 4, "b", &multi_day);
        g_assert_true(multi_day);
    }
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/time-formats/mode-parser", test_mode_parser);
    g_test_add_func("/time-formats/decimal", test_decimal_time);
    g_test_add_func("/time-formats/internet", test_internet_time);
    g_test_add_func("/time-formats/unix-hex-binary",
                    test_unix_hexadecimal_and_binary);
    g_test_add_func("/time-formats/astronomical",
                    test_astronomical_times);
    g_test_add_func("/time-formats/historical-scientific",
                    test_historical_and_scientific_times);
    g_test_add_func("/time-formats/tick-boundaries",
                    test_tick_boundaries);
    g_test_add_func("/time-formats/label-replacement",
                    test_label_replacement);
    g_test_add_func("/system-clock/lifecycle", test_clock_lifecycle);
    g_test_add_func("/calendar-system/catalogue", test_calendar_catalogue);
    g_test_add_func("/calendar-system/locale-workdays",
                    test_locale_workday_policy);
    g_test_add_func("/calendar-system/locale-workday-grid",
                    test_locale_workday_grid_policy);
    g_test_add_func("/calendar-system/references",
                    test_historical_calendar_references);
    g_test_add_func("/calendar-system/navigation",
                    test_calendar_navigation);
    g_test_add_func("/calendar-system/round-trips",
                    test_calendar_round_trips);
    g_test_add_func("/calendar-system/grid", test_calendar_grid);
    g_test_add_func("/native-contract/helpers",
                    test_native_contract_helpers);
    g_test_add_func("/event-store/index-order-cull",
                    test_event_store);
    g_test_add_func("/event-store/multiday",
                    test_event_store_multiday);

    return g_test_run();
}
