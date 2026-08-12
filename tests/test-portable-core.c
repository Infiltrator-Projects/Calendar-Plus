/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Adapter-free contract tests.  This binary links CORE_SOURCES only: passing
 * proves that calendar, event and live-clock behaviour does not require
 * GObject, GVariant, GJS or a GLib main loop.
 */

#include "calendar-core.h"
#include "calendar-registry.h"
#include "clock-engine.h"
#include "event-core.h"
#include "event-source.h"
#include "julian-day.h"


#define CALENDAR_FIELD_ANY G_MININT

typedef struct
{
    const gchar *id;
    gint gregorian_year;
    gint gregorian_month;
    gint gregorian_day;
    gint64 year;
    gint month;
    gint day;
} CalendarReferenceVector;

static void
assert_calendar_field(gint64 actual,
                      gint64 expected,
                      const gchar *id,
                      const gchar *field)
{
    if (expected == CALENDAR_FIELD_ANY)
        return;

    if (actual != expected)
    {
        g_error("calendar reference mismatch for %s %s: "
                "got %" G_GINT64_FORMAT ", expected %" G_GINT64_FORMAT,
                id,
                field,
                actual,
                expected);
    }
}

static void
test_calendar_reference_vectors(void)
{
    /*
     * Stable civil/epoch anchors exercise every registered provider without
     * relying on locale-sensitive month names.  Most vectors are epoch/new-
     * year identities; Chinese year numbering is intentionally left open
     * because ICU exposes the sexagenary-cycle year separately from the
     * related Gregorian year used for display.
     */
    static const CalendarReferenceVector vectors[] = {
        { "gregorian", 2000, 1, 1, 2000, 1, 1 },
        { "julian", 2000, 1, 14, 2000, 1, 1 },
        { "iso-week", 2021, 1, 4, 2021, 1, 1 },
        { "hebrew", 2026, 9, 12, 5787, 1, 1 },
        { "islamic", 622, 7, 18, 1, 1, 1 },
        { "islamic-civil", 622, 7, 19, 1, 1, 1 },
        { "islamic-umalqura", 1882, 11, 12, 1300, 1, 1 },
        { "persian", 2026, 3, 21, 1405, 1, 1 },
        { "chinese", 2026, 2, 17, CALENDAR_FIELD_ANY, 1, 1 },
        { "indian", 2026, 3, 22, 1948, 1, 1 },
        { "coptic", 2026, 9, 11, 1743, 1, 1 },
        { "ethiopian", 2026, 9, 11, 2019, 1, 1 },
        { "buddhist", 2026, 1, 1, 2569, 1, 1 },
        { "japanese", 2019, 5, 1, 1, 5, 1 },
        { "minguo", 1912, 1, 1, 1, 1, 1 },
        { "french-republican", 1792, 9, 22, 1, 1, 1 },
        { "roman", 2000, 1, 14, 2000, 1, 1 },
        { "mayan", 2012, 12, 21, 5200, 0, 0 },
        { "bahai", 1844, 3, 21, 1, 1, 1 },
        { "international-fixed", 2026, 1, 1, 2026, 1, 1 },
        { "world", 2026, 1, 1, 2026, 1, 1 },
        { "positivist", 2026, 1, 1, 2026, 1, 1 }
    };
    gsize index;

    g_assert_cmpuint(G_N_ELEMENTS(vectors), ==,
                     calendar_plus_calendar_provider_get_count());

    for (index = 0; index < G_N_ELEMENTS(vectors); index++)
    {
        const CalendarReferenceVector *vector = &vectors[index];
        const CalendarPlusCalendarProvider *provider =
            calendar_plus_calendar_provider_from_id(vector->id);
        const gint64 jdn = calendar_plus_gregorian_to_jdn(
            vector->gregorian_year,
            vector->gregorian_month,
            vector->gregorian_day);
        CalendarPlusCalendarFields fields = { 0 };

        g_assert_nonnull(provider);
        g_assert_true(provider->fields_from_jdn(provider, jdn, &fields));
        assert_calendar_field(fields.year, vector->year, vector->id, "year");
        assert_calendar_field(fields.month, vector->month, vector->id, "month");
        assert_calendar_field(fields.day, vector->day, vector->id, "day");
        g_assert_false(fields.special);
    }
}

typedef struct
{
    gint64 now;
    gint utc_offset;
    CalendarPlusClockTimer next_timer;
    CalendarPlusClockTimerFunc callback;
    gpointer callback_data;
    guint last_delay;
    guint schedules;
    guint cancellations;
    guint ticks;
    CalendarPlusClockEngine *engine;
    gboolean reconfigure_on_tick;
    CalendarPlusClockConfig next_config;
} FakeClock;

static gint64
fake_now(gpointer context)
{
    return ((FakeClock *)context)->now;
}

static gint
fake_utc_offset(gpointer context,
                gint64 unix_microseconds)
{
    (void)unix_microseconds;
    return ((FakeClock *)context)->utc_offset;
}

static CalendarPlusClockTimer
fake_schedule(gpointer context,
              guint delay,
              CalendarPlusClockTimerFunc callback,
              gpointer callback_data)
{
    FakeClock *clock = context;

    clock->last_delay = delay;
    clock->callback = callback;
    clock->callback_data = callback_data;
    clock->schedules++;
    clock->next_timer++;
    if (clock->next_timer == (CalendarPlusClockTimer)0)
        clock->next_timer++;
    return clock->next_timer;
}

static void
fake_cancel(gpointer context,
            CalendarPlusClockTimer timer)
{
    FakeClock *clock = context;

    g_assert_cmpuint(timer, ==, clock->next_timer);
    clock->cancellations++;
    clock->callback = NULL;
    clock->callback_data = NULL;
}

static void
fake_tick(gpointer context)
{
    FakeClock *clock = context;

    clock->ticks++;
    if (clock->reconfigure_on_tick)
    {
        clock->reconfigure_on_tick = FALSE;
        g_assert_true(calendar_plus_clock_engine_start(clock->engine,
                                                       &clock->next_config));
    }
}

static void
fake_fire(FakeClock *clock)
{
    CalendarPlusClockTimerFunc callback = clock->callback;
    gpointer callback_data = clock->callback_data;

    g_assert_nonnull(callback);
    clock->callback = NULL;
    clock->callback_data = NULL;
    callback(callback_data);
}

static void
test_clock_interfaces(void)
{
    FakeClock clock = {
        .now = 12 * (gint64)G_USEC_PER_SEC * 60 * 60
    };
    const CalendarPlusClockTimeSource time_source = {
        CALENDAR_PLUS_CLOCK_TIME_SOURCE_ABI,
        &clock,
        fake_now,
        fake_utc_offset
    };
    const CalendarPlusClockScheduler scheduler = {
        CALENDAR_PLUS_CLOCK_SCHEDULER_ABI,
        &clock,
        fake_schedule,
        fake_cancel
    };
    CalendarPlusClockConfig config = {
        CALENDAR_PLUS_TIME_MODE_DECIMAL,
        FALSE,
        FALSE,
        0.0
    };
    CalendarPlusClockEngine *engine =
        calendar_plus_clock_engine_new(&time_source,
                                       &scheduler,
                                       fake_tick,
                                       &clock);
    g_autofree gchar *formatted = NULL;

    g_assert_nonnull(engine);
    clock.engine = engine;
    g_assert_true(calendar_plus_clock_engine_start(engine, &config));
    g_assert_true(calendar_plus_clock_engine_is_running(engine));
    g_assert_cmpuint(clock.schedules, ==, 1);
    g_assert_cmpuint(clock.last_delay, >, 0);
    formatted = calendar_plus_clock_engine_format(engine);
    g_assert_cmpstr(formatted, ==, "5:00");

    clock.next_config = config;
    clock.next_config.show_seconds = TRUE;
    clock.reconfigure_on_tick = TRUE;
    fake_fire(&clock);
    g_assert_cmpuint(clock.ticks, ==, 1);
    g_assert_cmpuint(clock.schedules, ==, 2);
    g_assert_cmpuint(clock.cancellations, ==, 0);

    config = clock.next_config;
    g_assert_true(calendar_plus_clock_engine_start(engine, &config));
    g_assert_cmpuint(clock.schedules, ==, 2);
    config.vertical = TRUE;
    g_assert_true(calendar_plus_clock_engine_start(engine, &config));
    g_assert_cmpuint(clock.cancellations, ==, 1);
    g_assert_cmpuint(clock.schedules, ==, 3);

    calendar_plus_clock_engine_stop(engine);
    g_assert_false(calendar_plus_clock_engine_is_running(engine));
    g_assert_cmpuint(clock.cancellations, ==, 2);
    calendar_plus_clock_engine_free(engine);
}

typedef struct
{
    CalendarPlusClockEngine *engine;
    guint ticks;
} DestroyingTick;

static void
destroying_tick(gpointer context)
{
    DestroyingTick *state = context;

    state->ticks++;
    calendar_plus_clock_engine_free(state->engine);
    state->engine = NULL;
}

static void
test_clock_destroy_during_tick(void)
{
    FakeClock clock = { .now = (gint64)G_USEC_PER_SEC };
    const CalendarPlusClockTimeSource time_source = {
        CALENDAR_PLUS_CLOCK_TIME_SOURCE_ABI,
        &clock,
        fake_now,
        fake_utc_offset
    };
    const CalendarPlusClockScheduler scheduler = {
        CALENDAR_PLUS_CLOCK_SCHEDULER_ABI,
        &clock,
        fake_schedule,
        fake_cancel
    };
    const CalendarPlusClockConfig config = {
        CALENDAR_PLUS_TIME_MODE_UNIX,
        FALSE,
        FALSE,
        0.0
    };
    DestroyingTick state = { NULL, 0 };

    state.engine = calendar_plus_clock_engine_new(&time_source,
                                                  &scheduler,
                                                  destroying_tick,
                                                  &state);
    g_assert_nonnull(state.engine);
    g_assert_true(calendar_plus_clock_engine_start(state.engine, &config));
    fake_fire(&clock);
    g_assert_null(state.engine);
    g_assert_cmpuint(state.ticks, ==, 1);
    g_assert_cmpuint(clock.schedules, ==, 1);
}

static void
test_calendar_records(void)
{
    CalendarPlusCalendarEngine *engine;
    const CalendarPlusDate selected = { 2026, 7, 29 };
    const CalendarPlusDate today = { 2026, 7, 29 };
    CalendarPlusDate start = { 0 };
    CalendarPlusCalendarGrid grid = { 0 };
    CalendarPlusCalendarDescriptor descriptor = { 0 };
    guint selected_count = 0;
    guint item;

    g_assert_cmpuint(calendar_plus_calendar_catalogue_get_count(), ==, 22);
    for (item = 0; item < calendar_plus_calendar_catalogue_get_count(); item++)
    {
        g_assert_true(calendar_plus_calendar_catalogue_get(item, &descriptor));
        g_assert_nonnull(descriptor.id);
        g_assert_nonnull(descriptor.name);
        g_assert_cmpstr(descriptor.id, !=, "");
        g_assert_cmpstr(descriptor.name, !=, "");
    }
    g_assert_false(calendar_plus_calendar_catalogue_get(22, &descriptor));

    engine = calendar_plus_calendar_engine_new("gregorian");
    g_assert_nonnull(engine);
    g_assert_true(calendar_plus_calendar_engine_period_start(engine,
                                                             &selected,
                                                             &start));
    g_assert_cmpint(start.year, ==, 2026);
    g_assert_cmpint(start.month, ==, 7);
    g_assert_cmpint(start.day, ==, 1);
    g_assert_true(calendar_plus_calendar_engine_build_grid(engine,
                                                           &selected,
                                                           &today,
                                                           0,
                                                           &grid));
    for (item = 0; item < CALENDAR_PLUS_CALENDAR_GRID_CELLS; item++)
    {
        const CalendarPlusCalendarCell *cell = &grid.cells[item];

        g_assert_nonnull(cell->day_label);
        g_assert_cmpint(cell->row, ==, 2 + (gint)(item / 7));
        g_assert_cmpint(cell->column, ==, (gint)(item % 7));
        if (cell->is_selected)
            selected_count++;
    }
    g_assert_cmpuint(selected_count, ==, 1);
    calendar_plus_calendar_grid_clear(&grid);
    calendar_plus_calendar_engine_free(engine);
}

typedef struct
{
    CalendarPlusEventSink sink;
    gboolean started;
    gboolean stopped;
} FakeEventSource;

static gboolean
fake_source_start(gpointer context,
                  const CalendarPlusEventSink *sink)
{
    FakeEventSource *source = context;

    g_assert_nonnull(sink);
    g_assert_cmpuint(sink->abi_version, ==, CALENDAR_PLUS_EVENT_SINK_ABI);
    source->sink = *sink;
    source->started = TRUE;
    return TRUE;
}

static void
fake_source_stop(gpointer context)
{
    ((FakeEventSource *)context)->stopped = TRUE;
}

static gboolean
fake_source_range(gpointer context,
                  gint64 start_unix,
                  gint64 end_unix)
{
    FakeEventSource *source = context;
    const CalendarPlusEventInput input = {
        "portable-event",
        "#123456",
        "Portable source",
        FALSE,
        start_unix + 3600,
        end_unix - 3600,
        1,
        10
    };

    return source->sink.upsert(source->sink.context, &input);
}

static gboolean
fake_source_open(gpointer context,
                 const gchar *event_id)
{
    (void)context;
    return g_strcmp0(event_id, "portable-event") == 0;
}

static void
test_event_source_and_snapshot(void)
{
    const gint64 day = 1785283200;
    FakeEventSource state = { { 0 }, FALSE, FALSE };
    CalendarPlusEventIndex *index = calendar_plus_event_index_new();
    CalendarPlusEventSink sink;
    CalendarPlusEventSource source = {
        CALENDAR_PLUS_EVENT_SOURCE_ABI,
        "test-source",
        &state,
        fake_source_start,
        fake_source_stop,
        fake_source_range,
        fake_source_open
    };
    CalendarPlusEventSnapshot *snapshot;

    g_assert_nonnull(index);
    calendar_plus_event_index_make_sink(index, &sink);
    g_assert_true(calendar_plus_event_source_is_valid(&source));
    g_assert_true(source.start(source.context, &sink));
    g_assert_true(source.request_range(source.context, day, day + 86400));
    g_assert_true(source.open_event(source.context, "portable-event"));

    snapshot = calendar_plus_event_index_snapshot(index,
                                                  day,
                                                  day + 43200);
    g_assert_nonnull(snapshot);
    g_assert_cmpuint(snapshot->length, ==, 1);
    g_assert_cmpstr(snapshot->events[0].id, ==, "portable-event");
    g_assert_cmpstr(snapshot->events[0].summary, ==, "Portable source");
    calendar_plus_event_snapshot_free(snapshot);

    source.stop(source.context);
    g_assert_true(state.started);
    g_assert_true(state.stopped);
    calendar_plus_event_index_free(index);
}

static void
test_time_catalogue(void)
{
    gsize item;

    g_assert_cmpuint(calendar_plus_time_mode_get_count(), ==, 13);
    for (item = 0; item < calendar_plus_time_mode_get_count(); item++)
    {
        const CalendarPlusTimeMode mode =
            calendar_plus_time_mode_get_at(item);

        g_assert_cmpint(mode, !=, CALENDAR_PLUS_TIME_MODE_INVALID);
        g_assert_nonnull(calendar_plus_time_mode_get_id(mode));
        g_assert_nonnull(calendar_plus_time_mode_get_name(mode));
    }
    g_assert_cmpint(calendar_plus_time_mode_get_at(13), ==,
                    CALENDAR_PLUS_TIME_MODE_INVALID);
}

int
main(int argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/portable/clock-interfaces", test_clock_interfaces);
    g_test_add_func("/portable/clock-destroy-during-tick",
                    test_clock_destroy_during_tick);
    g_test_add_func("/portable/calendar-records", test_calendar_records);
    g_test_add_func("/portable/calendar-reference-vectors",
                    test_calendar_reference_vectors);
    g_test_add_func("/portable/event-source", test_event_source_and_snapshot);
    g_test_add_func("/portable/time-catalogue", test_time_catalogue);
    return g_test_run();
}
