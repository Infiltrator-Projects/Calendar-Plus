/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Shannon Smith
 *
 * ICU runtime ABI bridge.
 *
 * Calendar Plus uses ICU's stable C API but deliberately does not bind its
 * generic binary to one ICU SONAME/symbol suffix.  ICU renames public C
 * symbols at build time (for example ucal_open_74 and ucal_open_76).  This
 * bridge resolves the installed ICU major at runtime and exposes the ordinary
 * unsuffixed C names to the rest of Calendar Plus.
 *
 * U_DISABLE_RENAMING is set by the Calendar Plus build, so icu-calendar.c
 * calls these functions rather than a build-host-specific ICU symbol.  The
 * ICU headers still provide the authoritative ABI types and constants.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <infiltratr/dynlib.h>

#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

#ifdef _WIN32
#include <windows.h>
static INIT_ONCE bridge_once = INIT_ONCE_STATIC_INIT;
#else
#include <pthread.h>
static pthread_once_t bridge_once = PTHREAD_ONCE_INIT;
#endif

typedef struct
{
    InfiltratrDynlib uc;
    InfiltratrDynlib i18n;
    int available;

    const char *(*uloc_get_default)(void);
    int32_t (*uloc_set_keyword_value)(const char *, const char *, char *,
                                      int32_t, UErrorCode *);
    char *(*u_str_to_utf8)(char *, int32_t, int32_t *, const UChar *,
                           int32_t, UErrorCode *);
    UChar *(*u_str_from_utf8)(UChar *, int32_t, int32_t *, const char *,
                              int32_t, UErrorCode *);

    UCalendar *(*ucal_open_fn)(const UChar *, int32_t, const char *,
                               UCalendarType, UErrorCode *);
    void (*ucal_close_fn)(UCalendar *);
    UDate (*ucal_get_millis)(const UCalendar *, UErrorCode *);
    void (*ucal_set_millis)(UCalendar *, UDate, UErrorCode *);
    void (*ucal_add_fn)(UCalendar *, UCalendarDateFields, int32_t,
                        UErrorCode *);
    int32_t (*ucal_get_fn)(const UCalendar *, UCalendarDateFields,
                           UErrorCode *);
    void (*ucal_set_fn)(UCalendar *, UCalendarDateFields, int32_t);
    int32_t (*ucal_get_limit)(const UCalendar *, UCalendarDateFields,
                              UCalendarLimitType, UErrorCode *);

    UDateFormat *(*udat_open_fn)(UDateFormatStyle, UDateFormatStyle,
                                 const char *, const UChar *, int32_t,
                                 const UChar *, int32_t, UErrorCode *);
    void (*udat_close_fn)(UDateFormat *);
    int32_t (*udat_format_fn)(const UDateFormat *, UDate, UChar *, int32_t,
                              UFieldPosition *, UErrorCode *);
} IcuBridgeApi;

static IcuBridgeApi bridge_api;

static int
load_symbol(const InfiltratrDynlib *module,
            const char *base_name,
            const char *major,
            void *destination,
            size_t destination_size)
{
    char versioned_name[96];

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(versioned_name,
                   sizeof versioned_name,
                   "%s_%s",
                   base_name,
                   major);
    if (infiltratr_dynlib_symbol(module,
                                 versioned_name,
                                 destination,
                                 destination_size))
    {
        return 1;
    }
    return infiltratr_dynlib_symbol(module,
                                    base_name,
                                    destination,
                                    destination_size) ? 1 : 0;
}

static void
close_bridge_modules(IcuBridgeApi *api)
{
    infiltratr_dynlib_close(&api->i18n);
    infiltratr_dynlib_close(&api->uc);
    *api = (IcuBridgeApi){ 0 };
}

static int
try_icu_major(const char *major,
              IcuBridgeApi *api)
{
    char uc_name[64];
    char i18n_name[64];

#ifdef _WIN32
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(uc_name, sizeof uc_name, "icuuc%s.dll", major);
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(i18n_name, sizeof i18n_name, "icuin%s.dll", major);
#else
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(uc_name, sizeof uc_name, "libicuuc.so.%s", major);
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(i18n_name, sizeof i18n_name, "libicui18n.so.%s", major);
#endif

    if (!infiltratr_dynlib_open(&api->uc, uc_name) ||
        !infiltratr_dynlib_open(&api->i18n, i18n_name))
    {
        close_bridge_modules(api);
        return 0;
    }

#define LOAD_UC(member_, symbol_) \
    do { \
        if (!load_symbol(&api->uc, symbol_, major, \
                         (void *)&api->member_, sizeof api->member_)) \
            goto missing_symbol; \
    } while (0)
#define LOAD_I18N(member_, symbol_) \
    do { \
        if (!load_symbol(&api->i18n, symbol_, major, \
                         (void *)&api->member_, sizeof api->member_)) \
            goto missing_symbol; \
    } while (0)

    LOAD_UC(uloc_get_default, "uloc_getDefault");
    LOAD_UC(uloc_set_keyword_value, "uloc_setKeywordValue");
    LOAD_UC(u_str_to_utf8, "u_strToUTF8");
    LOAD_UC(u_str_from_utf8, "u_strFromUTF8");

    LOAD_I18N(ucal_open_fn, "ucal_open");
    LOAD_I18N(ucal_close_fn, "ucal_close");
    LOAD_I18N(ucal_get_millis, "ucal_getMillis");
    LOAD_I18N(ucal_set_millis, "ucal_setMillis");
    LOAD_I18N(ucal_add_fn, "ucal_add");
    LOAD_I18N(ucal_get_fn, "ucal_get");
    LOAD_I18N(ucal_set_fn, "ucal_set");
    LOAD_I18N(ucal_get_limit, "ucal_getLimit");
    LOAD_I18N(udat_open_fn, "udat_open");
    LOAD_I18N(udat_close_fn, "udat_close");
    LOAD_I18N(udat_format_fn, "udat_format");

#undef LOAD_I18N
#undef LOAD_UC

    api->available = 1;
    return 1;

missing_symbol:
#undef LOAD_I18N
#undef LOAD_UC
    close_bridge_modules(api);
    return 0;
}

static void
initialise_bridge(void)
{
    static const char *const majors[] = {
        "80", "79", "78", "77", "76", "75", "74", "73", "72"
    };
    size_t index;

    for (index = 0; index < sizeof majors / sizeof majors[0]; index++)
    {
        IcuBridgeApi candidate = { 0 };

        if (try_icu_major(majors[index], &candidate))
        {
            bridge_api = candidate;
            return;
        }
    }
}

#ifdef _WIN32
static BOOL CALLBACK
initialise_bridge_windows(PINIT_ONCE once,
                          PVOID parameter,
                          PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;
    initialise_bridge();
    return TRUE;
}
#endif

static IcuBridgeApi *
get_bridge(void)
{
#ifdef _WIN32
    (void)InitOnceExecuteOnce(&bridge_once,
                              initialise_bridge_windows,
                              NULL,
                              NULL);
#else
    (void)pthread_once(&bridge_once, initialise_bridge);
#endif
    return bridge_api.available ? &bridge_api : NULL;
}

static void
set_missing(UErrorCode *status)
{
    if (status != NULL)
        *status = U_MISSING_RESOURCE_ERROR;
}

const char *
uloc_getDefault(void)
{
    IcuBridgeApi *api = get_bridge();
    return api != NULL ? api->uloc_get_default() : "en_US";
}

int32_t
uloc_setKeywordValue(const char *keyword_name,
                     const char *keyword_value,
                     char *buffer,
                     int32_t capacity,
                     UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return 0;
    }
    return api->uloc_set_keyword_value(keyword_name,
                                       keyword_value,
                                       buffer,
                                       capacity,
                                       status);
}

char *
u_strToUTF8(char *destination,
            int32_t destination_capacity,
            int32_t *destination_length,
            const UChar *source,
            int32_t source_length,
            UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return NULL;
    }
    return api->u_str_to_utf8(destination,
                              destination_capacity,
                              destination_length,
                              source,
                              source_length,
                              status);
}

UChar *
u_strFromUTF8(UChar *destination,
              int32_t destination_capacity,
              int32_t *destination_length,
              const char *source,
              int32_t source_length,
              UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return NULL;
    }
    return api->u_str_from_utf8(destination,
                                destination_capacity,
                                destination_length,
                                source,
                                source_length,
                                status);
}

UCalendar *
ucal_open(const UChar *zone_id,
          int32_t zone_id_length,
          const char *locale,
          UCalendarType type,
          UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return NULL;
    }
    return api->ucal_open_fn(zone_id,
                             zone_id_length,
                             locale,
                             type,
                             status);
}

void
ucal_close(UCalendar *calendar)
{
    IcuBridgeApi *api = get_bridge();
    if (api != NULL)
        api->ucal_close_fn(calendar);
}

UDate
ucal_getMillis(const UCalendar *calendar,
               UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return 0.0;
    }
    return api->ucal_get_millis(calendar, status);
}

void
ucal_setMillis(UCalendar *calendar,
               UDate date,
               UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return;
    }
    api->ucal_set_millis(calendar, date, status);
}

void
ucal_add(UCalendar *calendar,
         UCalendarDateFields field,
         int32_t amount,
         UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return;
    }
    api->ucal_add_fn(calendar, field, amount, status);
}

int32_t
ucal_get(const UCalendar *calendar,
         UCalendarDateFields field,
         UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return 0;
    }
    return api->ucal_get_fn(calendar, field, status);
}

void
ucal_set(UCalendar *calendar,
         UCalendarDateFields field,
         int32_t value)
{
    IcuBridgeApi *api = get_bridge();
    if (api != NULL)
        api->ucal_set_fn(calendar, field, value);
}

int32_t
ucal_getLimit(const UCalendar *calendar,
              UCalendarDateFields field,
              UCalendarLimitType type,
              UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return 0;
    }
    return api->ucal_get_limit(calendar, field, type, status);
}

UDateFormat *
udat_open(UDateFormatStyle time_style,
          UDateFormatStyle date_style,
          const char *locale,
          const UChar *timezone_id,
          int32_t timezone_id_length,
          const UChar *pattern,
          int32_t pattern_length,
          UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return NULL;
    }
    return api->udat_open_fn(time_style,
                             date_style,
                             locale,
                             timezone_id,
                             timezone_id_length,
                             pattern,
                             pattern_length,
                             status);
}

void
udat_close(UDateFormat *format)
{
    IcuBridgeApi *api = get_bridge();
    if (api != NULL)
        api->udat_close_fn(format);
}

int32_t
udat_format(const UDateFormat *format,
            UDate date_to_format,
            UChar *result,
            int32_t result_length,
            UFieldPosition *position,
            UErrorCode *status)
{
    IcuBridgeApi *api = get_bridge();
    if (api == NULL)
    {
        set_missing(status);
        return 0;
    }
    return api->udat_format_fn(format,
                               date_to_format,
                               result,
                               result_length,
                               position,
                               status);
}
