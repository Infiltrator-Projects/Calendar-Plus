/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Civil and discrete native clock providers.
 *
 * These providers partition either the local civil day or an absolute epoch
 * into exact display units. Shared integer/rational timing primitives live in
 * time-formats.c so formatting and next-boundary scheduling use the same
 * arithmetic.
 */

#include "time-formats-internal.h"

gchar *
format_decimal_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gboolean vertical,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    const gint64 local =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds);
    const guint ticks_per_day = show_seconds ? DECIMAL_SECONDS_PER_DAY : 1000;
    const guint ticks = fractional_day_tick(local, ticks_per_day);
    const gchar *separator = vertical ? "\n" : ":";

    (void)longitude;
    if (show_seconds)
    {
        return g_strdup_printf("%u%s%02u%s%02u",
                               ticks / 10000, separator,
                               (ticks / 100) % 100, separator, ticks % 100);
    }

    return g_strdup_printf("%u%s%02u", ticks / 100, separator, ticks % 100);
}

guint
delay_decimal_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        show_seconds ? DECIMAL_SECONDS_PER_DAY : 1000);
}

/* @000 is midnight at UTC+01:00; the host's civil timezone is not involved. */
static gint64
internet_microseconds(gint64 unix_microseconds)
{
    const gint64 instant_phase =
        positive_modulo(unix_microseconds, MICROSECONDS_PER_DAY);

    return positive_modulo(
        instant_phase + (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC,
        MICROSECONDS_PER_DAY);
}

gchar *
format_internet_provider(gint64 unix_microseconds,
                         gint utc_offset_seconds,
                         gboolean show_seconds,
                         gboolean vertical,
                         gdouble latitude G_GNUC_UNUSED,
                         gdouble longitude)
{
    const guint ticks_per_day =
        show_seconds ? INTERNET_BEATS_PER_DAY * 100 : INTERNET_BEATS_PER_DAY;
    const guint ticks =
        fractional_day_tick(internet_microseconds(unix_microseconds),
                            ticks_per_day);

    (void)utc_offset_seconds;
    (void)vertical;
    (void)longitude;
    return show_seconds ?
        g_strdup_printf("@%03u.%02u", ticks / 100, ticks % 100) :
        g_strdup_printf("@%03u", ticks);
}

guint
delay_internet_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        internet_microseconds(unix_microseconds),
        show_seconds ? INTERNET_BEATS_PER_DAY * 100 : INTERNET_BEATS_PER_DAY);
}

gchar *
format_unix_provider(gint64 unix_microseconds,
                     gint utc_offset_seconds,
                     gboolean show_seconds,
                     gboolean vertical,
                     gdouble latitude G_GNUC_UNUSED,
                     gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)vertical;
    (void)longitude;
    return g_strdup_printf("%" G_GINT64_FORMAT,
                           floor_divide(unix_microseconds, G_USEC_PER_SEC));
}

guint
delay_unix_provider(gint64 unix_microseconds,
                    gint utc_offset_seconds,
                    gboolean show_seconds,
                    gdouble latitude G_GNUC_UNUSED,
                    gdouble longitude)
{
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)longitude;
    return delay_for_integer_period(unix_microseconds, G_USEC_PER_SEC);
}

gchar *
format_hexadecimal_provider(gint64 unix_microseconds,
                            gint utc_offset_seconds,
                            gboolean show_seconds,
                            gboolean vertical,
                            gdouble latitude G_GNUC_UNUSED,
                            gdouble longitude)
{
    (void)show_seconds;
    (void)vertical;
    (void)longitude;
    return g_strdup_printf(
        "%04X",
        fractional_day_tick(
            local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
            HEX_TICKS_PER_DAY));
}

guint
delay_hexadecimal_provider(gint64 unix_microseconds,
                           gint utc_offset_seconds,
                           gboolean show_seconds,
                           gdouble latitude G_GNUC_UNUSED,
                           gdouble longitude)
{
    (void)show_seconds;
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        HEX_TICKS_PER_DAY);
}

static void
write_binary(gchar *destination,
             guint value,
             guint bits)
{
    guint bit;

    for (bit = 0; bit < bits; bit++)
    {
        const guint shift = bits - bit - 1;
        destination[bit] = (value & (1U << shift)) ? '1' : '0';
    }
    destination[bits] = '\0';
}

gchar *
format_binary_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds,
                       gboolean vertical,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    gint hour;
    gint minute;
    gint second;
    gchar hour_bits[6];
    gchar minute_bits[7];
    gchar second_bits[7];
    const gchar *separator = vertical ? "\n" : ":";
    const gint64 local =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds);

    (void)longitude;
    split_clock_seconds(local / G_USEC_PER_SEC, &hour, &minute, &second);
    write_binary(hour_bits, (guint)hour, 5);
    write_binary(minute_bits, (guint)minute, 6);
    write_binary(second_bits, (guint)second, 6);

    return show_seconds ?
        g_strdup_printf("%s%s%s%s%s", hour_bits, separator, minute_bits,
                        separator, second_bits) :
        g_strdup_printf("%s%s%s", hour_bits, separator, minute_bits);
}

guint
delay_binary_provider(gint64 unix_microseconds,
                      gint utc_offset_seconds,
                      gboolean show_seconds,
                      gdouble latitude G_GNUC_UNUSED,
                      gdouble longitude)
{
    (void)longitude;
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        show_seconds ? SECONDS_PER_DAY : MINUTES_PER_HOUR * HOURS_PER_DAY);
}

static const gchar *const chinese_branches[] = {
    "子 Zǐ (Rat)",
    "丑 Chǒu (Ox)",
    "寅 Yín (Tiger)",
    "卯 Mǎo (Rabbit)",
    "辰 Chén (Dragon)",
    "巳 Sì (Snake)",
    "午 Wǔ (Horse)",
    "未 Wèi (Goat)",
    "申 Shēn (Monkey)",
    "酉 Yǒu (Rooster)",
    "戌 Xū (Dog)",
    "亥 Hài (Boar)"
};

gchar *
format_chinese_provider(gint64 unix_microseconds,
                        gint utc_offset_seconds,
                        gboolean show_seconds G_GNUC_UNUSED,
                        gboolean vertical,
                        gdouble latitude G_GNUC_UNUSED,
                        gdouble longitude)
{
    const gint64 shifted = positive_modulo(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds) +
        (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC,
        MICROSECONDS_PER_DAY);
    const guint branch = (guint)(shifted /
        ((gint64)2 * SECONDS_PER_HOUR * G_USEC_PER_SEC));

    (void)longitude;
    if (!vertical)
        return g_strdup(chinese_branches[branch]);

    /* Keep narrow panels readable without inventing a finer historical unit. */
    {
        g_auto(GStrv) fields = g_strsplit(chinese_branches[branch], " ", 3);
        return g_strdup_printf("%s\n%s\n%s", fields[0], fields[1], fields[2]);
    }
}

guint
delay_chinese_provider(gint64 unix_microseconds,
                       gint utc_offset_seconds,
                       gboolean show_seconds G_GNUC_UNUSED,
                       gdouble latitude G_GNUC_UNUSED,
                       gdouble longitude)
{
    const gint64 shifted =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds) +
        (gint64)SECONDS_PER_HOUR * G_USEC_PER_SEC;

    (void)longitude;
    return delay_for_integer_period(
        shifted,
        (gint64)2 * SECONDS_PER_HOUR * G_USEC_PER_SEC);
}


/*
 * A documented Han-era convention divided one civil day into one hundred kè.
 * Calendar Plus exposes that exact equal partition without implying that the
 * convention was uniform across every Chinese dynasty.
 */
gchar *
format_chinese_ke_provider(gint64 unix_microseconds,
                           gint utc_offset_seconds,
                           gboolean show_seconds G_GNUC_UNUSED,
                           gboolean vertical,
                           gdouble latitude G_GNUC_UNUSED,
                           gdouble longitude G_GNUC_UNUSED)
{
    const gint64 local =
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds);
    const guint ke = fractional_day_tick(local, 100);

    return g_strdup_printf(vertical ? "刻\n%02u/100" : "刻 %02u/100", ke);
}

guint
delay_chinese_ke_provider(gint64 unix_microseconds,
                          gint utc_offset_seconds,
                          gboolean show_seconds G_GNUC_UNUSED,
                          gdouble latitude G_GNUC_UNUSED,
                          gdouble longitude G_GNUC_UNUSED)
{
    return delay_for_day_ticks(
        local_microseconds_of_day(unix_microseconds, utc_offset_seconds),
        100);
}
