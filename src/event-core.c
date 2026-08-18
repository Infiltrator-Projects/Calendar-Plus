// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Event normalisation, interval indexing, ordering and snapshots.
 *
 * Inputs and results are ordinary C records.  Transport-specific tuple
 * parsing and presentation-specific marshalling belong to adapters.  Long
 * events remain one record: day membership is an interval query rather than
 * a per-day expansion, so storage is independent of event duration.
 */

#include "event-core.h"

#include <string.h>

typedef struct
{
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
    gint64 last_update_timestamp;
    guint64 sequence;
} EventRecord;

struct _CalendarPlusEventIndex
{
    GHashTable *events_by_id;
    guint64 next_sequence;
    gint64 revision;
};

static void
event_record_free(EventRecord *event)
{
    if (event == NULL)
        return;

    g_free(event->id);
    g_free(event->color);
    g_free(event->summary);
    g_free(event);
}

static void
touch_index(CalendarPlusEventIndex *index)
{
    const gint64 now = g_get_monotonic_time();

    /* Preserve a strict change token even inside one monotonic microsecond. */
    index->revision = now > index->revision ? now : index->revision + 1;
}

static gint64
local_day_start(gint64 unix_time)
{
    g_autoptr(GDateTime) instant =
        g_date_time_new_from_unix_local(unix_time);
    g_autoptr(GDateTime) midnight = NULL;

    if (instant == NULL)
        return unix_time;

    midnight = g_date_time_new_local(g_date_time_get_year(instant),
                                     g_date_time_get_month(instant),
                                     g_date_time_get_day_of_month(instant),
                                     0,
                                     0,
                                     0.0);
    return midnight != NULL ? g_date_time_to_unix(midnight) : unix_time;
}

static gint64
saturating_difference(gint64 left,
                      gint64 right)
{
    if (right > 0 && left < G_MININT64 + right)
        return G_MININT64;
    if (right < 0 && left > G_MAXINT64 + right)
        return G_MAXINT64;
    return left - right;
}

guint
calendar_plus_event_day_relation(gint64 start_day_unix,
                                 gint64 end_day_unix,
                                 gint64 comparison_unix)
{
    const gint64 start_day = local_day_start(start_day_unix);
    const gint64 end_day = local_day_start(end_day_unix);
    const gint64 comparison_day = local_day_start(comparison_unix);
    guint relation = CALENDAR_PLUS_EVENT_DAY_RELATION_NONE;

    if (start_day == comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_STARTS_ON_DAY;
    if (end_day == comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_ON_DAY;
    if (start_day < comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_BEFORE_DAY;
    if (end_day < comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_ENDED_BEFORE_DAY;
    if (end_day > comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_ENDS_AFTER_DAY;
    if (start_day > comparison_day)
        relation |= CALENDAR_PLUS_EVENT_DAY_RELATION_STARTED_AFTER_DAY;

    return relation;
}

CalendarPlusEventState
calendar_plus_event_state(gint64 start_unix,
                          gint64 end_unix,
                          gint64 now_unix)
{
    if (end_unix < start_unix)
        return CALENDAR_PLUS_EVENT_STATE_INVALID;
    if (end_unix < now_unix)
        return CALENDAR_PLUS_EVENT_STATE_PAST;
    if (start_unix > now_unix)
        return CALENDAR_PLUS_EVENT_STATE_FUTURE;
    return CALENDAR_PLUS_EVENT_STATE_PRESENT;
}

CalendarPlusEventTiming
calendar_plus_event_calculate_timing(gint64 start_unix,
                                     gint64 end_unix,
                                     gint64 now_unix)
{
    const CalendarPlusEventTiming result = {
        calendar_plus_event_state(start_unix, end_unix, now_unix),
        saturating_difference(start_unix, now_unix),
        saturating_difference(end_unix, now_unix)
    };

    return result;
}

static gboolean
text_is_valid(const gchar *text,
              gsize maximum_bytes,
              gboolean allow_empty)
{
    gsize length;

    if (text == NULL)
        return FALSE;
    length = strlen(text);
    return length <= maximum_bytes && (allow_empty || length > 0) &&
           g_utf8_validate(text, (gssize)length, NULL);
}

static gboolean
color_is_valid(const gchar *color)
{
    gsize index;
    const gsize length = color != NULL ? strlen(color) : 0;

    if (length != 4 && length != 7 && length != 9)
        return FALSE;
    if (color[0] != '#')
        return FALSE;

    for (index = 1; index < length; index++)
    {
        if (!g_ascii_isxdigit(color[index]))
            return FALSE;
    }
    return TRUE;
}

gboolean
calendar_plus_event_input_is_valid(const CalendarPlusEventInput *input)
{
    return input != NULL &&
           text_is_valid(input->id,
                         CALENDAR_PLUS_EVENT_MAX_ID_BYTES,
                         FALSE) &&
           text_is_valid(input->color,
                         CALENDAR_PLUS_EVENT_MAX_COLOR_BYTES,
                         FALSE) &&
           color_is_valid(input->color) &&
           text_is_valid(input->summary,
                         CALENDAR_PLUS_EVENT_MAX_SUMMARY_BYTES,
                         TRUE);
}

static EventRecord *
event_record_from_input(const CalendarPlusEventInput *input)
{
    EventRecord *event;

    if (!calendar_plus_event_input_is_valid(input))
        return NULL;

    event = g_new0(EventRecord, 1);
    event->id = g_strdup(input->id);
    event->color = g_strdup(input->color);
    event->summary = g_strdup(input->summary);
    event->all_day = input->all_day;
    event->start_unix = input->start_unix;
    event->end_unix = input->end_unix;
    event->modified = input->modified;
    event->last_update_timestamp = input->update_timestamp;

    /* The source contract supplies all-day end at following midnight. */
    if (event->all_day && event->end_unix > G_MININT64)
        event->end_unix--;
    if (event->end_unix < event->start_unix)
        event->end_unix = event->start_unix;

    event->start_day_unix = local_day_start(event->start_unix);
    event->end_day_unix = local_day_start(event->end_unix);
    event->multi_day = event->start_day_unix != event->end_day_unix;
    return event;
}

CalendarPlusEventIndex *
calendar_plus_event_index_new(void)
{
    CalendarPlusEventIndex *index = g_new0(CalendarPlusEventIndex, 1);

    index->events_by_id =
        g_hash_table_new_full(g_str_hash,
                              g_str_equal,
                              NULL,
                              (GDestroyNotify)event_record_free);
    index->revision = g_get_monotonic_time();
    return index;
}

void
calendar_plus_event_index_free(CalendarPlusEventIndex *index)
{
    if (index == NULL)
        return;

    g_hash_table_destroy(index->events_by_id);
    index->events_by_id = NULL;
    g_free(index);
}

gboolean
calendar_plus_event_index_remove(CalendarPlusEventIndex *index,
                                 const gchar *id)
{
    EventRecord *event;

    if (index == NULL || id == NULL)
        return FALSE;

    event = g_hash_table_lookup(index->events_by_id, id);
    if (event == NULL)
        return FALSE;

    g_hash_table_steal(index->events_by_id, id);
    event_record_free(event);
    touch_index(index);
    return TRUE;
}

gboolean
calendar_plus_event_index_upsert(CalendarPlusEventIndex *index,
                                 const CalendarPlusEventInput *input)
{
    EventRecord *existing;
    EventRecord *replacement;

    if (index == NULL)
        return FALSE;

    replacement = event_record_from_input(input);
    if (replacement == NULL)
        return FALSE;

    existing = g_hash_table_lookup(index->events_by_id, replacement->id);
    if (existing != NULL &&
        existing->modified == replacement->modified &&
        existing->all_day == replacement->all_day &&
        existing->start_unix == replacement->start_unix &&
        existing->end_unix == replacement->end_unix &&
        g_strcmp0(existing->color, replacement->color) == 0 &&
        g_strcmp0(existing->summary, replacement->summary) == 0)
    {
        existing->last_update_timestamp = input->update_timestamp;
        event_record_free(replacement);
        return FALSE;
    }

    replacement->sequence = existing != NULL ?
        existing->sequence : ++index->next_sequence;
    if (existing != NULL)
    {
        g_hash_table_steal(index->events_by_id, existing->id);
        event_record_free(existing);
    }

    g_hash_table_insert(index->events_by_id,
                        replacement->id,
                        replacement);
    touch_index(index);
    return TRUE;
}

gboolean
calendar_plus_event_index_cull(CalendarPlusEventIndex *index,
                               gint64 minimum_update_timestamp)
{
    g_autoptr(GPtrArray) stale_ids =
        g_ptr_array_new_with_free_func(g_free);
    GHashTableIter iter;
    gpointer value;
    guint item;

    if (index == NULL)
        return FALSE;

    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        const EventRecord *event = value;

        if (event->last_update_timestamp < minimum_update_timestamp)
            g_ptr_array_add(stale_ids, g_strdup(event->id));
    }

    for (item = 0; item < stale_ids->len; item++)
    {
        const gchar *id = g_ptr_array_index(stale_ids, item);

        calendar_plus_event_index_remove(index, id);
    }
    return stale_ids->len > 0;
}

void
calendar_plus_event_index_clear(CalendarPlusEventIndex *index)
{
    gboolean changed;

    if (index == NULL)
        return;

    changed = g_hash_table_size(index->events_by_id) > 0;
    g_hash_table_remove_all(index->events_by_id);
    if (changed)
        touch_index(index);
}

gboolean
calendar_plus_event_index_refresh_timezone(CalendarPlusEventIndex *index)
{
    GHashTableIter iter;
    gpointer value;
    gboolean changed = FALSE;

    if (index == NULL)
        return FALSE;

    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        EventRecord *event = value;
        const gint64 start_day = local_day_start(event->start_unix);
        const gint64 end_day = local_day_start(event->end_unix);
        const gboolean multi_day = start_day != end_day;

        if (start_day != event->start_day_unix ||
            end_day != event->end_day_unix ||
            multi_day != event->multi_day)
        {
            event->start_day_unix = start_day;
            event->end_day_unix = end_day;
            event->multi_day = multi_day;
            changed = TRUE;
        }
    }

    if (changed)
        touch_index(index);
    return changed;
}

static gint
compare_records(gconstpointer left,
                gconstpointer right)
{
    const EventRecord *a = *(EventRecord *const *)left;
    const EventRecord *b = *(EventRecord *const *)right;

    if (a->start_unix < b->start_unix)
        return -1;
    if (a->start_unix > b->start_unix)
        return 1;
    if (a->end_unix < b->end_unix)
        return -1;
    if (a->end_unix > b->end_unix)
        return 1;
    if (a->sequence < b->sequence)
        return -1;
    if (a->sequence > b->sequence)
        return 1;
    return g_strcmp0(a->id, b->id);
}

static GPtrArray *
ordered_records(CalendarPlusEventIndex *index,
                gint64 local_day_unix,
                gint64 now_unix)
{
    const gint64 requested_day = local_day_start(local_day_unix);
    GPtrArray *sorted = g_ptr_array_new();
    GPtrArray *ordered;
    GHashTableIter iter;
    gpointer value;
    guint item;

    g_hash_table_iter_init(&iter, index->events_by_id);
    while (g_hash_table_iter_next(&iter, NULL, &value))
    {
        const EventRecord *event = value;

        if (event->start_day_unix <= requested_day &&
            event->end_day_unix >= requested_day)
        {
            g_ptr_array_add(sorted, value);
        }
    }
    g_ptr_array_sort(sorted, compare_records);

    if (requested_day != local_day_start(now_unix))
        return sorted;

    /* Today's agenda groups ended timed, all-day, then active/future events. */
    ordered = g_ptr_array_new();
    for (item = 0; item < sorted->len; item++)
    {
        EventRecord *event = g_ptr_array_index(sorted, item);

        if (!event->all_day && event->end_unix < now_unix)
            g_ptr_array_add(ordered, event);
    }
    for (item = 0; item < sorted->len; item++)
    {
        EventRecord *event = g_ptr_array_index(sorted, item);

        if (event->all_day)
            g_ptr_array_add(ordered, event);
    }
    for (item = 0; item < sorted->len; item++)
    {
        EventRecord *event = g_ptr_array_index(sorted, item);

        if (!event->all_day && event->end_unix >= now_unix)
            g_ptr_array_add(ordered, event);
    }

    g_ptr_array_unref(sorted);
    return ordered;
}

CalendarPlusEventSnapshot *
calendar_plus_event_index_snapshot(CalendarPlusEventIndex *index,
                                   gint64 local_day_unix,
                                   gint64 now_unix)
{
    g_autoptr(GPtrArray) ordered = NULL;
    CalendarPlusEventSnapshot *snapshot;
    guint item;

    if (index == NULL)
        return NULL;

    ordered = ordered_records(index, local_day_unix, now_unix);
    snapshot = g_new0(CalendarPlusEventSnapshot, 1);
    snapshot->revision = index->revision;
    snapshot->length = ordered->len;
    snapshot->events = g_new0(CalendarPlusEvent, snapshot->length);

    for (item = 0; item < ordered->len; item++)
    {
        const EventRecord *source = g_ptr_array_index(ordered, item);
        CalendarPlusEvent *target = &snapshot->events[item];

        target->id = g_strdup(source->id);
        target->color = g_strdup(source->color);
        target->summary = g_strdup(source->summary);
        target->all_day = source->all_day;
        target->multi_day = source->multi_day;
        target->start_unix = source->start_unix;
        target->end_unix = source->end_unix;
        target->start_day_unix = source->start_day_unix;
        target->end_day_unix = source->end_day_unix;
        target->modified = source->modified;
    }

    return snapshot;
}

void
calendar_plus_event_snapshot_free(CalendarPlusEventSnapshot *snapshot)
{
    gsize item;

    if (snapshot == NULL)
        return;

    for (item = 0; item < snapshot->length; item++)
    {
        g_free(snapshot->events[item].id);
        g_free(snapshot->events[item].color);
        g_free(snapshot->events[item].summary);
    }
    g_free(snapshot->events);
    g_free(snapshot);
}
