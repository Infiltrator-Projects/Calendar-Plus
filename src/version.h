// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_VERSION_H
#define CALENDAR_PLUS_VERSION_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * calendar_plus_get_version:
 *
 * Returns the Calendar Plus source version used to build the loaded native
 * library.  This is intentionally part of the introspected ABI so the GJS
 * front end can reject a stale or mismatched native library instead of
 * failing later with misleading secondary errors.
 *
 * Returns: (transfer none): the semantic source version
 */
const gchar *calendar_plus_get_version(void);

/**
 * calendar_plus_get_source_id:
 *
 * Returns a deterministic source identity compiled into the native library.
 * Release builds use `calendar-plus-<version>`; packagers may override the
 * build macro when they have a reproducible downstream source identifier.
 *
 * Returns: (transfer none): the native source identity
 */
const gchar *calendar_plus_get_source_id(void);

G_END_DECLS

#endif
