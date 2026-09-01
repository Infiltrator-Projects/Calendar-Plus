// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include "version.h"
#include "project-info.h"

const gchar *
calendar_plus_get_version(void)
{
    return calendar_plus_project_info()->version;
}

const gchar *
calendar_plus_get_source_id(void)
{
    return calendar_plus_project_info()->source_id;
}
