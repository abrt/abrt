/*
 *  Copyright (C) 2012  Red Hat
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "abrt-config-widget.h"

#include "libabrt.h"

#define ABRT_CONFIG_WIDGET_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), TYPE_ABRT_CONFIG_WIDGET, AbrtConfigWidgetPrivate))

#define WID(s) GTK_WIDGET(gtk_builder_get_object(self->priv->builder, s))

#define UI_FILE_NAME "abrt-config-widget.ui"

typedef struct {
    const char *name;
    map_string_t *config;
} AbrtConfigWidgetOption;

struct AbrtConfigWidgetPrivate {
    GtkBuilder   *builder;
    map_string_t *report_gtk_conf;
    map_string_t *abrt_applet_conf;

    AbrtConfigWidgetOption opt_upload_coredump;
    AbrtConfigWidgetOption opt_steal_directory;
    AbrtConfigWidgetOption opt_send_ureport;
    AbrtConfigWidgetOption opt_shortened_reporting;
    AbrtConfigWidgetOption opt_silent_shortened_reporting;
};

G_DEFINE_TYPE(AbrtConfigWidget, abrt_config_widget, GTK_TYPE_BOX)

enum {
    SN_CHANGED,
    SN_LAST_SIGNAL
} SignalNumber;

static guint s_signals[SN_LAST_SIGNAL] = { 0 };

static void abrt_config_widget_finalize(GObject *object);

static void
abrt_config_widget_class_init(AbrtConfigWidgetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = abrt_config_widget_finalize;

    g_type_class_add_private(klass, sizeof(AbrtConfigWidgetPrivate));

    s_signals[SN_CHANGED] = g_signal_new ("changed",
                             G_TYPE_FROM_CLASS (klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(AbrtConfigWidgetClass, changed),
                             /*accumulator*/NULL, /*accu_data*/NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, /*n_params*/0);
}

static void
abrt_config_widget_finalize(GObject *object)
{
    AbrtConfigWidget *self;

    self = ABRT_CONFIG_WIDGET(object);
    if(self->priv->builder) {
        g_object_unref(self->priv->builder);
        self->priv->builder = NULL;
    }

    /* Clean up */
    free_map_string(self->priv->report_gtk_conf);
    self->priv->report_gtk_conf = NULL;

    free_map_string(self->priv->abrt_applet_conf);
    self->priv->abrt_applet_conf = NULL;

    G_OBJECT_CLASS(abrt_config_widget_parent_class)->finalize(object);
}

static void
emit_change(AbrtConfigWidget *config)
{
    g_signal_emit(config, s_signals[SN_CHANGED], 0);
}

static void
on_switch_activate(GObject       *object,
        GParamSpec     *spec,
        AbrtConfigWidget *config)
{
    const gboolean state = gtk_switch_get_active(GTK_SWITCH(object));
    const char *const val = state ? "yes" : "no";

    AbrtConfigWidgetOption *option = g_object_get_data(G_OBJECT(object), "abrt-option");
    VERB3 log("%s : %s", option->name, val);
    set_app_user_setting(option->config, option->name, val);
    emit_change(config);
}

static void
connect_switch_with_option(GtkSwitch *gsw, AbrtConfigWidget *config, AbrtConfigWidgetOption *option, gboolean def)
{
    const char *val = get_app_user_setting(option->config, option->name);
    const gboolean state = val ? string_to_bool(val) : def;

    gtk_switch_set_active(gsw, state);
    g_object_set_data(G_OBJECT(gsw), "abrt-option", option);
    g_signal_connect(G_OBJECT(gsw), "notify::active",
            G_CALLBACK(on_switch_activate), config);
}

static void
abrt_config_widget_init(AbrtConfigWidget *self)
{
    GError *error = NULL;

    self->priv = ABRT_CONFIG_WIDGET_GET_PRIVATE(self);

    self->priv->builder = gtk_builder_new();
    gtk_builder_set_translation_domain(self->priv->builder, GETTEXT_PACKAGE);

    gtk_builder_add_from_file(self->priv->builder, ABRT_UI_DIR "/" UI_FILE_NAME, &error);
    if(error != NULL) {
        g_warning("Failed to load '%s': %s", ABRT_UI_DIR "/" UI_FILE_NAME, error->message);
        g_error_free(error);
        error = NULL;
        gtk_builder_add_from_file(self->priv->builder, UI_FILE_NAME, &error);
        if(error != NULL) {
            g_warning("Failed to load '%s': %s", UI_FILE_NAME, error->message);
            g_error_free(error);
            return;
        }
    }

    /* Load configuration */
    load_abrt_conf();

    self->priv->report_gtk_conf = new_map_string();
    if(!load_app_conf_file("report-gtk", self->priv->report_gtk_conf)) {
        g_warning("Failed to load config for '%s'", "report-gtk");
    }

    self->priv->abrt_applet_conf = new_map_string();
    if(!load_app_conf_file("abrt-applet", self->priv->abrt_applet_conf)) {
        g_warning("Failed to load config for '%s'", "abrt-applet");
    }

    /* Initialize options */
    /* report-gtk */
    self->priv->opt_steal_directory.name = "ask_steal_dir";
    self->priv->opt_steal_directory.config = self->priv->report_gtk_conf;

    self->priv->opt_upload_coredump.name = "abrt_analyze_smart_ask_upload_coredump";
    self->priv->opt_upload_coredump.config = self->priv->report_gtk_conf;

    /* abrt-applet */
    self->priv->opt_send_ureport.name = "AutoreportingEnabled";
    self->priv->opt_send_ureport.config = self->priv->abrt_applet_conf;

    self->priv->opt_shortened_reporting.name = "ShortenedReporting";
    self->priv->opt_shortened_reporting.config = self->priv->abrt_applet_conf;

    self->priv->opt_silent_shortened_reporting.name = "SilentShortenedReporting";
    self->priv->opt_silent_shortened_reporting.config = self->priv->abrt_applet_conf;

    /* Connect widgets with options */
    connect_switch_with_option(GTK_SWITCH(WID("switch_upload_coredump")), self,
            &(self->priv->opt_upload_coredump), /* default: */ FALSE);
    connect_switch_with_option(GTK_SWITCH(WID("switch_steal_directory")), self,
            &(self->priv->opt_steal_directory), /* default: */ FALSE);
    connect_switch_with_option(GTK_SWITCH(WID("switch_send_ureport")), self,
            &(self->priv->opt_send_ureport), g_settings_autoreporting);
    connect_switch_with_option(GTK_SWITCH(WID("switch_shortened_reporting")), self,
            &(self->priv->opt_shortened_reporting), g_settings_shortenedreporting);
    connect_switch_with_option(GTK_SWITCH(WID("switch_silent_shortened_reporting")), self,
            &(self->priv->opt_silent_shortened_reporting), FALSE);

    gtk_widget_reparent(WID("grid"), GTK_WIDGET(self));

    /* Set the initial state of the properties */
    gtk_widget_show_all(GTK_WIDGET(self));
}

AbrtConfigWidget *
abrt_config_widget_new()
{
    return g_object_new(TYPE_ABRT_CONFIG_WIDGET, NULL);
}

void
abrt_config_widget_save_chnages(AbrtConfigWidget *config)
{
    /* Save configuration */
    save_app_conf_file("report-gtk", config->priv->report_gtk_conf);
    save_app_conf_file("abrt-applet", config->priv->abrt_applet_conf);
}
