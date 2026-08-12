// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * GObject lifetime facade for the platform-neutral event index.
 *
 * CalendarServer tuple parsing and GVariant snapshot encoding are isolated in
 * event-gvariant-adapter.c. This object owns the GObject lifetime and the
 * compatibility API for `::`-separated removal identifiers.
 */

#include "event-store-private.h"

struct _CalendarPlusEventStore
{
    GObject parent_instance;

    CalendarPlusEventIndex *index;
};

G_DEFINE_TYPE(CalendarPlusEventStore,
              calendar_plus_event_store,
              G_TYPE_OBJECT)

static void
calendar_plus_event_store_dispose(GObject *object)
{
    CalendarPlusEventStore *self = CALENDAR_PLUS_EVENT_STORE(object);

    g_clear_pointer(&self->index, calendar_plus_event_index_free);
    G_OBJECT_CLASS(calendar_plus_event_store_parent_class)->dispose(object);
}

static void
calendar_plus_event_store_class_init(CalendarPlusEventStoreClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = calendar_plus_event_store_dispose;
}

static void
calendar_plus_event_store_init(CalendarPlusEventStore *self)
{
    self->index = calendar_plus_event_index_new();
}

CalendarPlusEventStore *
calendar_plus_event_store_new(void)
{
    return g_object_new(CALENDAR_PLUS_TYPE_EVENT_STORE, NULL);
}

CalendarPlusEventIndex *
calendar_plus_event_store_get_index(CalendarPlusEventStore *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), NULL);
    return self->index;
}

gboolean
calendar_plus_event_store_remove(CalendarPlusEventStore *self,
                                 const gchar *serialized_ids)
{
    g_auto(GStrv) ids = NULL;
    gboolean changed = FALSE;
    gsize item;

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    if (serialized_ids == NULL || serialized_ids[0] == '\0')
        return FALSE;

    ids = g_strsplit(serialized_ids, "::", -1);
    for (item = 0; ids[item] != NULL; item++)
    {
        if (ids[item][0] != '\0')
        {
            changed |= calendar_plus_event_index_remove(self->index,
                                                        ids[item]);
        }
    }
    return changed;
}

gboolean
calendar_plus_event_store_cull(CalendarPlusEventStore *self,
                               gint64 minimum_update_timestamp)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    return calendar_plus_event_index_cull(self->index,
                                          minimum_update_timestamp);
}

void
calendar_plus_event_store_clear(CalendarPlusEventStore *self)
{
    g_return_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self));
    calendar_plus_event_index_clear(self->index);
}

gboolean
calendar_plus_event_store_refresh_timezone(CalendarPlusEventStore *self)
{
    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), FALSE);
    return calendar_plus_event_index_refresh_timezone(self->index);
}

gchar **
calendar_plus_event_store_get_colors(CalendarPlusEventStore *self,
                                     gint64 local_day_unix,
                                     gint64 now_unix)
{
    CalendarPlusEventSnapshot *snapshot;
    gchar **colors;
    gsize item;

    g_return_val_if_fail(CALENDAR_PLUS_IS_EVENT_STORE(self), NULL);
    snapshot = calendar_plus_event_index_snapshot(self->index,
                                                  local_day_unix,
                                                  now_unix);
    if (snapshot == NULL)
        return NULL;

    colors = g_new0(gchar *, snapshot->length + 1);
    for (item = 0; item < snapshot->length; item++)
        colors[item] = g_strdup(snapshot->events[item].color);

    calendar_plus_event_snapshot_free(snapshot);
    return colors;
}
