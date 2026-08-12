// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include "event-source.h"

static gboolean
sink_upsert(gpointer context,
            const CalendarPlusEventInput *input)
{
    return calendar_plus_event_index_upsert(context, input);
}

static gboolean
sink_remove(gpointer context,
            const gchar *id)
{
    return calendar_plus_event_index_remove(context, id);
}

static gboolean
sink_cull(gpointer context,
          gint64 refresh_timestamp)
{
    return calendar_plus_event_index_cull(context, refresh_timestamp);
}

static void
sink_clear(gpointer context)
{
    calendar_plus_event_index_clear(context);
}

void
calendar_plus_event_index_make_sink(CalendarPlusEventIndex *index,
                                    CalendarPlusEventSink *sink)
{
    g_return_if_fail(sink != NULL);

    sink->abi_version = CALENDAR_PLUS_EVENT_SINK_ABI;
    sink->context = index;
    sink->upsert = sink_upsert;
    sink->remove = sink_remove;
    sink->cull = sink_cull;
    sink->clear = sink_clear;
}

gboolean
calendar_plus_event_source_is_valid(const CalendarPlusEventSource *source)
{
    return source != NULL &&
           source->abi_version == CALENDAR_PLUS_EVENT_SOURCE_ABI &&
           source->id != NULL && source->id[0] != '\0' &&
           source->start != NULL && source->stop != NULL &&
           source->request_range != NULL;
}
