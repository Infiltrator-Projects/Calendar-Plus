/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Platform-neutral event index and snapshot contract.
 */

#ifndef CALENDAR_PLUS_EVENT_CORE_H
#define CALENDAR_PLUS_EVENT_CORE_H

#include "event-types.h"

G_BEGIN_DECLS

enum
{
    CALENDAR_PLUS_EVENT_MAX_ID_BYTES = 4096,
    CALENDAR_PLUS_EVENT_MAX_COLOR_BYTES = 9,
    CALENDAR_PLUS_EVENT_MAX_SUMMARY_BYTES = 65536
};

typedef struct _CalendarPlusEventIndex CalendarPlusEventIndex;

/*
 * Borrowed transport input. start_unix and end_unix are Unix seconds. Timed
 * end points are inclusive. For all-day events, end_unix is the exclusive
 * following-midnight endpoint; the index converts it to an inclusive final
 * second exactly once. modified is source metadata; update_timestamp is the
 * caller's refresh token used only for culling.
 */
typedef struct
{
    const gchar *id;
    const gchar *color;
    const gchar *summary;
    gboolean all_day;
    gint64 start_unix;
    gint64 end_unix;
    gint64 modified;
    gint64 update_timestamp;
} CalendarPlusEventInput;

typedef struct
{
    /* All strings are owned by the containing snapshot. */
    gchar *id;
    gchar *color;
    gchar *summary;
    gboolean all_day;
    gboolean multi_day;
    gint64 start_unix;
    gint64 end_unix;
    gint64 start_day_unix;
    gint64 end_day_unix;
    gint64 modified;
} CalendarPlusEvent;

typedef struct
{
    /* Opaque change token; compare for equality rather than interpreting it. */
    gint64 revision;
    gsize length;
    /* Owned array of @length deep-copied records. */
    CalendarPlusEvent *events;
} CalendarPlusEventSnapshot;

typedef struct
{
    CalendarPlusEventState state;
    gint64 seconds_until_start;
    gint64 seconds_until_end;
} CalendarPlusEventTiming;

/** Returns: (transfer full): new single-owner-thread event index. */
CalendarPlusEventIndex *calendar_plus_event_index_new(void);

/** Accepts %NULL; no cross-thread synchronisation is implied. */
void calendar_plus_event_index_free(CalendarPlusEventIndex *index);

/** Returns: whether input satisfies size, UTF-8 and colour rules. */
gboolean calendar_plus_event_input_is_valid(
    const CalendarPlusEventInput *input);
/** Inserts/replaces one event; unchanged refreshes do not advance revision. */
gboolean calendar_plus_event_index_upsert(
    CalendarPlusEventIndex *index,
    const CalendarPlusEventInput *input);
/** Removes one id and advances revision only when it existed. */
gboolean calendar_plus_event_index_remove(CalendarPlusEventIndex *index,
                                          const gchar *id);
/** Removes records older than the caller's completed refresh token. */
gboolean calendar_plus_event_index_cull(CalendarPlusEventIndex *index,
                                        gint64 minimum_update_timestamp);
/** Clears all records, advancing revision only when state changed. */
void calendar_plus_event_index_clear(CalendarPlusEventIndex *index);

/** Recomputes derived local-day membership after a timezone change. */
gboolean calendar_plus_event_index_refresh_timezone(
    CalendarPlusEventIndex *index);

/**
 * calendar_plus_event_index_snapshot:
 * @index: event index
 * @local_day_unix: any Unix instant on the requested local day
 * @now_unix: current Unix seconds used for today's agenda grouping
 *
 * Returns: (transfer full) (nullable): deep-copied immutable snapshot
 */
CalendarPlusEventSnapshot *calendar_plus_event_index_snapshot(
    CalendarPlusEventIndex *index,
    gint64 local_day_unix,
    gint64 now_unix);
/** Accepts %NULL and frees every deep-copied snapshot string. */
void calendar_plus_event_snapshot_free(CalendarPlusEventSnapshot *snapshot);

/** Returns a bitmask describing an event interval relative to one local day. */
guint calendar_plus_event_day_relation(gint64 start_day_unix,
                                       gint64 end_day_unix,
                                       gint64 comparison_unix);
/** Returns past/present/future, or INVALID for an inverted range. */
CalendarPlusEventState calendar_plus_event_state(gint64 start_unix,
                                                  gint64 end_unix,
                                                  gint64 now_unix);
/** Returns state plus saturating signed seconds to start and end. */
CalendarPlusEventTiming calendar_plus_event_calculate_timing(
    gint64 start_unix,
    gint64 end_unix,
    gint64 now_unix);

G_END_DECLS

#endif
