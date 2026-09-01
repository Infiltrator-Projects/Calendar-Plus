/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * CalendarServer/GVariant adapter for the neutral event index.
 *
 * Required tuple fields are type-checked and string fields are size-limited
 * here, then mapped once into CalendarPlusEventInput. Event semantics never
 * depend on tuple positions or a desktop-specific transport schema.
 */

#include "event-store-private.h"

static GVariant *
unwrap_variant(GVariant *value)
{
    GVariant *current = g_variant_ref(value);

    while (g_variant_is_of_type(current, G_VARIANT_TYPE_VARIANT))
    {
        GVariant *nested = g_variant_get_variant(current);

        g_variant_unref(current);
        current = nested;
    }
    return current;
}

static GVariant *
event_child(GVariant *event_data,
            gsize index)
{
    g_autoptr(GVariant) child =
        g_variant_get_child_value(event_data, index);

    return unwrap_variant(child);
}

static gchar *
variant_dup_string_limited(GVariant *value,
                           gsize maximum_bytes,
                           gboolean allow_empty)
{
    gsize length;
    const gchar *text;

    if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
        return NULL;

    text = g_variant_get_string(value, &length);
    if (length > maximum_bytes || (!allow_empty && length == 0))
        return NULL;
    return g_strndup(text, length);
}

static gboolean
variant_get_int64(GVariant *value,
                  gint64 *result)
{
    switch (g_variant_classify(value))
    {
    case G_VARIANT_CLASS_BYTE:
        *result = (gint64)g_variant_get_byte(value);
        return TRUE;
    case G_VARIANT_CLASS_INT16:
        *result = (gint64)g_variant_get_int16(value);
        return TRUE;
    case G_VARIANT_CLASS_UINT16:
        *result = (gint64)g_variant_get_uint16(value);
        return TRUE;
    case G_VARIANT_CLASS_INT32:
        *result = (gint64)g_variant_get_int32(value);
        return TRUE;
    case G_VARIANT_CLASS_UINT32:
        *result = (gint64)g_variant_get_uint32(value);
        return TRUE;
    case G_VARIANT_CLASS_INT64:
        *result = g_variant_get_int64(value);
        return TRUE;
    case G_VARIANT_CLASS_UINT64:
    {
        const guint64 unsigned_value = g_variant_get_uint64(value);

        if (unsigned_value > G_MAXINT64)
            return FALSE;
        *result = (gint64)unsigned_value;
        return TRUE;
    }
    default:
        return FALSE;
    }
}

GVariant *
calendar_plus_event_timing(gint64 start_unix,
                           gint64 end_unix,
                           gint64 now_unix)
{
    const CalendarPlusEventTiming timing =
        calendar_plus_event_calculate_timing(start_unix,
                                             end_unix,
                                             now_unix);

    return g_variant_ref_sink(
        g_variant_new("(ixx)",
                      (gint)timing.state,
                      timing.seconds_until_start,
                      timing.seconds_until_end));
}

gboolean
calendar_plus_event_store_add_or_update(CalendarPlusEventStore *self,
                                        GVariant *event_data,
                                        gint64 update_timestamp)
{
    g_autoptr(GVariant) tuple = NULL;
    g_autoptr(GVariant) id_value = NULL;
    g_autoptr(GVariant) color_value = NULL;
    g_autoptr(GVariant) summary_value = NULL;
    g_autoptr(GVariant) all_day_value = NULL;
    g_autoptr(GVariant) start_value = NULL;
    g_autoptr(GVariant) end_value = NULL;
    g_autoptr(GVariant) modified_value = NULL;
    g_autofree gchar *id = NULL;
    g_autofree gchar *color = NULL;
    g_autofree gchar *summary = NULL;
    CalendarPlusEventInput input;

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    if (event_data == NULL)
        return FALSE;

    tuple = unwrap_variant(event_data);
    if (g_variant_classify(tuple) != G_VARIANT_CLASS_TUPLE ||
        g_variant_n_children(tuple) < 7)
    {
        return FALSE;
    }

    id_value = event_child(tuple, 0);
    color_value = event_child(tuple, 1);
    summary_value = event_child(tuple, 2);
    all_day_value = event_child(tuple, 3);
    start_value = event_child(tuple, 4);
    end_value = event_child(tuple, 5);
    modified_value = event_child(tuple, 6);

    id = variant_dup_string_limited(id_value,
                                    CALENDAR_PLUS_EVENT_MAX_ID_BYTES,
                                    FALSE);
    color = variant_dup_string_limited(color_value,
                                       CALENDAR_PLUS_EVENT_MAX_COLOR_BYTES,
                                       FALSE);
    summary = variant_dup_string_limited(
        summary_value, CALENDAR_PLUS_EVENT_MAX_SUMMARY_BYTES, TRUE);

    input.id = id;
    input.color = color;
    input.summary = summary;
    input.update_timestamp = update_timestamp;
    if (id == NULL || color == NULL || summary == NULL ||
        !g_variant_is_of_type(all_day_value, G_VARIANT_TYPE_BOOLEAN) ||
        !variant_get_int64(start_value, &input.start_unix) ||
        !variant_get_int64(end_value, &input.end_unix) ||
        !variant_get_int64(modified_value, &input.modified))
    {
        return FALSE;
    }

    input.all_day = g_variant_get_boolean(all_day_value);
    return calendar_plus_event_index_upsert(
        calendar_plus_event_store_get_index(self), &input);
}

GVariant *
calendar_plus_event_store_get_snapshot(CalendarPlusEventStore *self,
                                       gint64 local_day_unix,
                                       gint64 now_unix)
{
    CalendarPlusEventSnapshot *snapshot;
    GVariantBuilder rows;
    gsize item;
    GVariant *result;

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), NULL);
    snapshot = calendar_plus_event_index_snapshot(
        calendar_plus_event_store_get_index(self),
        local_day_unix,
        now_unix);
    if (snapshot == NULL)
        return NULL;

    g_variant_builder_init(&rows, G_VARIANT_TYPE("a(sssbbxxxxx)"));
    for (item = 0; item < snapshot->length; item++)
    {
        const CalendarPlusEvent *event = &snapshot->events[item];

        g_variant_builder_add(&rows,
                              "(sssbbxxxxx)",
                              event->id,
                              event->color,
                              event->summary,
                              event->all_day,
                              event->multi_day,
                              event->start_unix,
                              event->end_unix,
                              event->start_day_unix,
                              event->end_day_unix,
                              event->modified);
    }

    result = g_variant_ref_sink(
        g_variant_new("(x@a(sssbbxxxxx))",
                      snapshot->revision,
                      g_variant_builder_end(&rows)));
    calendar_plus_event_snapshot_free(snapshot);
    return result;
}
