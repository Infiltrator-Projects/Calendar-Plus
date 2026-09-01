// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#include <glib.h>
#include <infiltratr/dynlib.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>

#include "project-info.h"

/*
 * Calendar Plus only needs GtkAboutDialog's stable GTK 3 ABI. Keeping this
 * deliberately small declaration surface and loading the runtime on demand
 * avoids requiring GTK development headers for a single dialog. The binary
 * package declares the GTK 3 runtime directly.
 */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkDialog GtkDialog;
typedef struct _GtkAboutDialog GtkAboutDialog;
typedef struct _GtkCssProvider GtkCssProvider;
typedef struct _GtkStyleProvider GtkStyleProvider;
typedef struct _GdkScreen GdkScreen;

typedef struct
{
    gboolean (*init_check)(gint *argc, gchar ***argv);
    GtkWidget *(*about_dialog_new)(void);
    void (*set_program_name)(GtkAboutDialog *about, const gchar *name);
    void (*set_version)(GtkAboutDialog *about, const gchar *version);
    void (*set_comments)(GtkAboutDialog *about, const gchar *comments);
    void (*set_website)(GtkAboutDialog *about, const gchar *website);
    void (*set_website_label)(GtkAboutDialog *about, const gchar *label);
    void (*set_copyright)(GtkAboutDialog *about, const gchar *copyright_text);
    void (*set_license)(GtkAboutDialog *about, const gchar *license);
    void (*set_wrap_license)(GtkAboutDialog *about, gboolean wrap_license);
    void (*set_logo_icon_name)(GtkAboutDialog *about, const gchar *icon_name);
    void (*set_authors)(GtkAboutDialog *about, const gchar **authors);
    void (*window_set_title)(GtkWindow *window, const gchar *title);
    gint (*dialog_run)(GtkDialog *dialog);
    void (*widget_show)(GtkWidget *widget);
    void (*widget_destroy)(GtkWidget *widget);
    GtkCssProvider *(*css_provider_new)(void);
    gboolean (*css_provider_load_from_data)(GtkCssProvider *provider,
                                            const gchar *data,
                                            gssize length,
                                            GError **error);
    void (*style_context_add_provider_for_screen)(GdkScreen *screen,
                                                   GtkStyleProvider *provider,
                                                   guint priority);
} Gtk3Api;

typedef struct
{
    GdkScreen *(*screen_get_default)(void);
} Gdk3Api;

static Gtk3Api gtk_api;
static Gdk3Api gdk_api;
static InfiltratrDynlib gtk_module = INFILTRATR_DYNLIB_INIT;
static InfiltratrDynlib gdk_module = INFILTRATR_DYNLIB_INIT;

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "calendar-plus@the-infiltratr"
#endif

#define _(text) gettext(text)

static gboolean
load_gtk(void)
{
    if (!infiltratr_dynlib_open(&gtk_module, "libgtk-3.so.0"))
        return FALSE;

#define LOAD_GTK(member, symbol) \
    do { \
        if (!infiltratr_dynlib_symbol(&gtk_module, symbol, \
                                      &gtk_api.member, \
                                      sizeof gtk_api.member)) { \
            infiltratr_dynlib_close(&gtk_module); \
            gtk_api = (Gtk3Api){ 0 }; \
            return FALSE; \
        } \
    } while (0)

    LOAD_GTK(init_check, "gtk_init_check");
    LOAD_GTK(about_dialog_new, "gtk_about_dialog_new");
    LOAD_GTK(set_program_name, "gtk_about_dialog_set_program_name");
    LOAD_GTK(set_version, "gtk_about_dialog_set_version");
    LOAD_GTK(set_comments, "gtk_about_dialog_set_comments");
    LOAD_GTK(set_website, "gtk_about_dialog_set_website");
    LOAD_GTK(set_website_label, "gtk_about_dialog_set_website_label");
    LOAD_GTK(set_copyright, "gtk_about_dialog_set_copyright");
    LOAD_GTK(set_license, "gtk_about_dialog_set_license");
    LOAD_GTK(set_wrap_license, "gtk_about_dialog_set_wrap_license");
    LOAD_GTK(set_logo_icon_name, "gtk_about_dialog_set_logo_icon_name");
    LOAD_GTK(set_authors, "gtk_about_dialog_set_authors");
    LOAD_GTK(window_set_title, "gtk_window_set_title");
    LOAD_GTK(dialog_run, "gtk_dialog_run");
    LOAD_GTK(widget_show, "gtk_widget_show");
    LOAD_GTK(widget_destroy, "gtk_widget_destroy");
    LOAD_GTK(css_provider_new, "gtk_css_provider_new");
    LOAD_GTK(css_provider_load_from_data, "gtk_css_provider_load_from_data");
    LOAD_GTK(style_context_add_provider_for_screen,
             "gtk_style_context_add_provider_for_screen");

#undef LOAD_GTK
    return TRUE;
}

static gboolean
load_gdk(void)
{
    if (!infiltratr_dynlib_open(&gdk_module, "libgdk-3.so.0"))
        return FALSE;
    if (!infiltratr_dynlib_symbol(&gdk_module,
                                  "gdk_screen_get_default",
                                  &gdk_api.screen_get_default,
                                  sizeof gdk_api.screen_get_default))
    {
        infiltratr_dynlib_close(&gdk_module);
        gdk_api = (Gdk3Api){ 0 };
        return FALSE;
    }
    return TRUE;
}

static void
apply_typography(void)
{
    static const gchar css[] =
        "* { font-family: \"MB Corpo S Title WEB\"; font-weight: 400; }"
        ".title, .heading { font-family: \"MB Corpo A Title Cond WEB\"; "
        "font-weight: 400; }"
        "button, button label { font-family: \"MB Corpo S Title WEB\"; "
        "font-weight: 700; }";
    GtkCssProvider *provider;
    GdkScreen *screen;

    if (!load_gdk())
        return;

    screen = gdk_api.screen_get_default();
    provider = gtk_api.css_provider_new();
    if (screen != NULL && provider != NULL &&
        gtk_api.css_provider_load_from_data(provider, css, -1, NULL))
    {
        /*
         * GTK keeps the style provider active for this short-lived helper.
         * The process exits immediately after the modal About dialog closes.
         */
        gtk_api.style_context_add_provider_for_screen(
            screen, (GtkStyleProvider *) provider, 600U);
    }

    infiltratr_dynlib_close(&gdk_module);
    gdk_api = (Gdk3Api){ 0 };
}

static void
print_metadata(void)
{
    if (infiltratr_project_info_print(stdout,
                                      calendar_plus_project_info()) != 0)
        fputs("Unable to print project metadata.\n", stderr);
}

static const gchar *
build_label(const gchar *profile)
{
    if (infiltratr_string_equal(profile, "native"))
        return "Native / local machine compile";
    if (infiltratr_string_equal(profile, "generic"))
        return "Generic / APT package";
    return "Source / development build";
}

static GtkWidget *
create_about_dialog(void)
{
    static const gchar *authors[] = {
        "Shannon Smith — Author and project maintainer",
        NULL
    };
    static const gchar *license =
        "Calendar Plus is free software licensed under the GNU General Public "
        "License version 3 or, at your option, any later version "
        "(GPL-3.0-or-later).\n\n"
        "See LICENSE in the source package for the complete licence text.";
    GtkWidget *widget = gtk_api.about_dialog_new();
    GtkAboutDialog *about = (GtkAboutDialog *) widget;
    const InfiltratrProjectInfo *info = calendar_plus_project_info();
    char comments[512];
    (void) snprintf(
        comments,
        sizeof comments,
        "%s\n\nBuild: %s",
        _(info->comments),
        build_label(info->build_profile));

    gtk_api.window_set_title((GtkWindow *) widget, _("About Calendar Plus"));
    gtk_api.set_program_name(about, _(info->program_name));
    gtk_api.set_version(about, info->version);
    gtk_api.set_logo_icon_name(about, info->icon_name);
    gtk_api.set_comments(about, comments);
    gtk_api.set_authors(about, authors);
    gtk_api.set_website(about, info->website);
    gtk_api.set_website_label(about, _("Website"));
    gtk_api.set_copyright(about, _(info->copyright_text));
    gtk_api.set_license(about, license);
    gtk_api.set_wrap_license(about, TRUE);
    return widget;
}
int
main(int argc,
     char **argv)
{
    GtkWidget *dialog;

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, "/usr/share/locale");
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    if (argc == 2 && infiltratr_string_equal(argv[1], "--print-metadata"))
    {
        print_metadata();
        return 0;
    }
    if (argc != 1)
    {
        fprintf(stderr, "Usage: %s [--print-metadata]\n", argv[0]);
        return 2;
    }
    if (!load_gtk())
    {
        fprintf(stderr, "%s\n", _("Unable to load GTK 3"));
        return 1;
    }
    if (!gtk_api.init_check(&argc, &argv))
    {
        fprintf(stderr, "%s\n", _("Unable to open a graphical display."));
        return 1;
    }

    apply_typography();
    dialog = create_about_dialog();
    gtk_api.widget_show(dialog);
    (void) gtk_api.dialog_run((GtkDialog *) dialog);
    gtk_api.widget_destroy(dialog);
    infiltratr_dynlib_close(&gtk_module);
    return 0;
}
