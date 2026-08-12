/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * Transport-neutral event-source boundary.
 */

#ifndef CALENDAR_PLUS_EVENT_SOURCE_H
#define CALENDAR_PLUS_EVENT_SOURCE_H

#include "event-core.h"

G_BEGIN_DECLS

typedef struct _CalendarPlusEventSink CalendarPlusEventSink;
typedef struct _CalendarPlusEventSource CalendarPlusEventSource;

typedef gboolean (*CalendarPlusEventSinkUpsertFunc)(
    gpointer context,
    const CalendarPlusEventInput *input);
typedef gboolean (*CalendarPlusEventSinkRemoveFunc)(gpointer context,
                                                    const gchar *id);
typedef gboolean (*CalendarPlusEventSinkCullFunc)(gpointer context,
                                                  gint64 refresh_timestamp);
typedef void (*CalendarPlusEventSinkClearFunc)(gpointer context);

struct _CalendarPlusEventSink
{
    guint abi_version;
    gpointer context;
    CalendarPlusEventSinkUpsertFunc upsert;
    CalendarPlusEventSinkRemoveFunc remove;
    CalendarPlusEventSinkCullFunc cull;
    CalendarPlusEventSinkClearFunc clear;
};

typedef gboolean (*CalendarPlusEventSourceStartFunc)(
    gpointer context,
    const CalendarPlusEventSink *sink);
typedef void (*CalendarPlusEventSourceStopFunc)(gpointer context);
typedef gboolean (*CalendarPlusEventSourceRangeFunc)(gpointer context,
                                                     gint64 start_unix,
                                                     gint64 end_unix);
typedef gboolean (*CalendarPlusEventSourceOpenFunc)(gpointer context,
                                                    const gchar *event_id);

struct _CalendarPlusEventSource
{
    guint abi_version;
    const gchar *id;
    gpointer context;
    CalendarPlusEventSourceStartFunc start;
    CalendarPlusEventSourceStopFunc stop;
    CalendarPlusEventSourceRangeFunc request_range;
    CalendarPlusEventSourceOpenFunc open_event;
};

enum
{
    CALENDAR_PLUS_EVENT_SINK_ABI = 1,
    CALENDAR_PLUS_EVENT_SOURCE_ABI = 1
};

/*
 * make_sink() writes a same-thread view that borrows @index. A source should
 * copy the small sink table during start(); a non-NULL index must outlive the
 * source connection.
 */
void calendar_plus_event_index_make_sink(CalendarPlusEventIndex *index,
                                         CalendarPlusEventSink *sink);
/*
 * Validates the ABI and callbacks required for lifecycle/range requests.
 * request_range() publishes events overlapping the requested Unix interval
 * through the bound sink; open_event may be NULL.
 */
gboolean calendar_plus_event_source_is_valid(
    const CalendarPlusEventSource *source);

G_END_DECLS

#endif
