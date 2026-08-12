// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include "project-info.h"

#define N_(text) text

#ifndef CALENDAR_PLUS_VERSION
#define CALENDAR_PLUS_VERSION "0.0.0-unknown"
#endif

#ifndef CALENDAR_PLUS_SOURCE_ID
#define CALENDAR_PLUS_SOURCE_ID "calendar-plus-unknown"
#endif

#ifndef CALENDAR_PLUS_BUILD_PROFILE
#define CALENDAR_PLUS_BUILD_PROFILE "unknown"
#endif

const InfiltratrProjectInfo *
calendar_plus_project_info(void)
{
    static const InfiltratrProjectInfo info = {
        .struct_size = sizeof(InfiltratrProjectInfo),
        .abi_version = INFILTRATR_PROJECT_INFO_ABI,
        .program_name = N_("Calendar Plus"),
        .executable_name = "calendar-plus",
        .application_id = "calendar-plus@the-infiltratr",
        .version = CALENDAR_PLUS_VERSION,
        .source_id = CALENDAR_PLUS_SOURCE_ID,
        .build_profile = CALENDAR_PLUS_BUILD_PROFILE,
        .author = "Shannon Smith",
        .website = "https://github.com/The-Infiltratr",
        .license_id = "GPL-3.0-or-later",
        .comments = N_("A native C-backed Cinnamon clock and calendar authored "
                       "by Shannon Smith, with multiple time and calendar "
                       "systems."),
        .icon_name = "gnome-calendar",
        .copyright_text = N_("Copyright © 2026 Shannon Smith\n\n"
                             "This program comes with absolutely no warranty.\n"
                             "See the GNU GPL v3+ License for details.")
    };
    return &info;
}
