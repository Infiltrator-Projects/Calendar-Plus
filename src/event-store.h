// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_EVENT_STORE_H
#define CALENDAR_PLUS_EVENT_STORE_H

#include "event-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALENDAR_PLUS_TYPE_EVENT_STORE \
    (calendar_plus_event_store_get_type())


/**
 * calendar_plus_event_day_relation:
 * @start_day_unix: event start local-midnight Unix timestamp
 * @end_day_unix: event end local-midnight Unix timestamp
 * @comparison_unix: any instant on the local day being compared
 *
 * Returns: bitwise-or of #CalendarPlusEventDayRelation values
 */
guint calendar_plus_event_day_relation(gint64 start_day_unix,
                                        gint64 end_day_unix,
                                        gint64 comparison_unix);

/**
 * calendar_plus_event_state:
 * @start_unix: inclusive event start instant
 * @end_unix: inclusive event end instant
 * @now_unix: current instant
 *
 * Returns: the native temporal state of the event
 */
CalendarPlusEventState calendar_plus_event_state(gint64 start_unix,
                                                  gint64 end_unix,
                                                  gint64 now_unix);

/**
 * calendar_plus_event_timing:
 * @start_unix: inclusive event start instant
 * @end_unix: inclusive event end instant
 * @now_unix: current instant
 *
 * Returns the state plus signed seconds until start and finish.  Arithmetic is
 * saturating so malformed extreme timestamps cannot overflow signed integers.
 *
 * Returns: (transfer full): an `(ixx)` tuple of state, seconds-to-start and
 *   seconds-to-finish
 */
GVariant *calendar_plus_event_timing(gint64 start_unix,
                                     gint64 end_unix,
                                     gint64 now_unix);

G_DECLARE_FINAL_TYPE(CalendarPlusEventStore,
                     calendar_plus_event_store,
                     CALENDAR_PLUS,
                     EVENT_STORE,
                     GObject)

/**
 * calendar_plus_event_store_new:
 *
 * Creates the in-process event index used by Cinnamon's agenda renderer.
 *
 * Returns: (transfer full): a new empty event store
 */
CalendarPlusEventStore *calendar_plus_event_store_new(void);

/**
 * calendar_plus_event_store_add_or_update:
 * @self: an event store
 * @event_data: a Cinnamon CalendarServer event tuple
 * @update_timestamp: monotonic timestamp for the current server refresh
 *
 * Parses and normalises one event. All-day end points and multi-day membership
 * are handled here so the JavaScript layer never has to duplicate calendar
 * interval logic. Queries test the complete interval; event length is not
 * capped and storage does not grow with event duration.
 *
 * Returns: %TRUE when visible event data changed
 */
gboolean calendar_plus_event_store_add_or_update(
    CalendarPlusEventStore *self,
    GVariant *event_data,
    gint64 update_timestamp);

/**
 * calendar_plus_event_store_remove:
 * @self: an event store
 * @serialized_ids: event identifiers separated by `::`
 *
 * Returns: %TRUE when at least one indexed event was removed
 */
gboolean calendar_plus_event_store_remove(
    CalendarPlusEventStore *self,
    const gchar *serialized_ids);

/**
 * calendar_plus_event_store_cull:
 * @self: an event store
 * @minimum_update_timestamp: oldest accepted server refresh timestamp
 *
 * Removes events not seen during the most recent server refresh.
 *
 * Returns: %TRUE when at least one indexed event was removed
 */
gboolean calendar_plus_event_store_cull(
    CalendarPlusEventStore *self,
    gint64 minimum_update_timestamp);

/**
 * calendar_plus_event_store_clear:
 * @self: an event store
 *
 * Removes all indexed events.
 */
void calendar_plus_event_store_clear(CalendarPlusEventStore *self);

/**
 * calendar_plus_event_store_refresh_timezone:
 * @self: an event store
 *
 * Recalculates cached local-day boundaries after the system timezone changes.
 * Absolute event timestamps remain unchanged.
 *
 * Returns: %TRUE when at least one cached boundary changed
 */
gboolean calendar_plus_event_store_refresh_timezone(
    CalendarPlusEventStore *self);

/**
 * calendar_plus_event_store_get_snapshot:
 * @self: an event store
 * @local_day_unix: Unix timestamp at local midnight for the requested day
 * @now_unix: current Unix timestamp, used to group today's ended timed events
 *   before all-day and active/future timed events
 *
 * Returns a revision followed by the day's already-sorted event rows. Each
 * event contains its identifier, colour, summary, all-day and multi-day
 * flags, start/end instants, start/end local-midnight values and modification
 * timestamp.
 *
 * Returns: (transfer full): a `(xa(sssbbxxxxx))` snapshot variant
 */
GVariant *calendar_plus_event_store_get_snapshot(
    CalendarPlusEventStore *self,
    gint64 local_day_unix,
    gint64 now_unix);

/**
 * calendar_plus_event_store_get_colors:
 * @self: an event store
 * @local_day_unix: Unix timestamp at local midnight for the requested day
 * @now_unix: current Unix timestamp
 *
 * Returns: (transfer full) (array zero-terminated=1): event colours in the
 *   same order as the native agenda snapshot
 */
gchar **calendar_plus_event_store_get_colors(
    CalendarPlusEventStore *self,
    gint64 local_day_unix,
    gint64 now_unix);

G_END_DECLS

#endif
