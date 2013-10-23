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
#include <assert.h>

#define ABRT_CONFIG_WIDGET_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), TYPE_ABRT_CONFIG_WIDGET, AbrtConfigWidgetPrivate))

#define WID(s) GTK_WIDGET(gtk_builder_get_object(self->priv->builder, s))

#define UI_FILE_NAME "abrt-config-widget.ui"

typedef struct {
    char *app_name;
    map_string_t *settings;
} AbrtAppConfiguration;

typedef struct {
    const char *name;
    GtkSwitch *widget;
    gboolean default_value;
    gboolean current_value;
    AbrtAppConfiguration *config;
} AbrtConfigWidgetOption;

enum AbrtOptions
{
    _ABRT_OPT_BEGIN_,

    ABRT_OPT_UPLOAD_COREDUMP = _ABRT_OPT_BEGIN_,
    ABRT_OPT_STEAL_DIRECTORY,
    ABRT_OPT_PRIVATE_TICKET,
    ABRT_OPT_SEND_UREPORT,
    ABRT_OPT_SHORTENED_REPORTING,
    ABRT_OPT_SILENT_SHORTENED_REPORTING,

    _ABRT_OPT_END_,
};

struct AbrtConfigWidgetPrivate {
    GtkBuilder   *builder;
    AbrtAppConfiguration *report_gtk_conf;
    AbrtAppConfiguration *abrt_applet_conf;

    AbrtConfigWidgetOption options[_ABRT_OPT_END_];
};

G_DEFINE_TYPE(AbrtConfigWidget, abrt_config_widget, GTK_TYPE_BOX)

enum {
    SN_CHANGED,
    SN_LAST_SIGNAL
} SignalNumber;

static guint s_signals[SN_LAST_SIGNAL] = { 0 };

static void abrt_config_widget_finalize(GObject *object);

static AbrtAppConfiguration *
abrt_app_configuration_new(const char *app_name)
{
    AbrtAppConfiguration *conf = xmalloc(sizeof(*conf));

    conf->app_name = xstrdup(app_name);
    conf->settings = new_map_string();

    if(!load_app_conf_file(conf->app_name, conf->settings)) {
        g_warning("Failed to load config for '%s'", conf->app_name);
    }

    return conf;
}

static void
abrt_app_configuration_set_value(AbrtAppConfiguration *conf, const char *name, const char *value)
{
    set_app_user_setting(conf->settings, name, value);
}

static const char *
abrt_app_configuration_get_value(AbrtAppConfiguration *conf, const char *name)
{
    return get_app_user_setting(conf->settings, name);
}

static void
abrt_app_configuration_save(AbrtAppConfiguration *conf)
{
    save_app_conf_file(conf->app_name, conf->settings);
}

static void
abrt_app_configuration_free(AbrtAppConfiguration *conf)
{
    if (!conf)
        return;

    free(conf->app_name);
    conf->app_name = (void *)0xDEADBEAF;

    free_map_string(conf->settings);
    conf->settings = (void *)0xDEADBEAF;
}

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
    abrt_app_configuration_free(self->priv->report_gtk_conf);
    self->priv->report_gtk_conf = NULL;

    abrt_app_configuration_free(self->priv->abrt_applet_conf);
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
    log_debug("%s : %s", option->name, val);
    abrt_app_configuration_set_value(option->config, option->name, val);
    abrt_app_configuration_save(option->config);
    emit_change(config);
}

static void
update_option_current_value(AbrtConfigWidget *self, enum AbrtOptions opid)
{
    assert((opid >= _ABRT_OPT_BEGIN_ && opid < _ABRT_OPT_END_) || !"Out of range Option ID value");

    AbrtConfigWidgetOption *option = &(self->priv->options[opid]);
    const char *val = abrt_app_configuration_get_value(option->config, option->name);
    option->current_value = val ? string_to_bool(val) : option->default_value;
}

static void
connect_switch_with_option(AbrtConfigWidget *self, enum AbrtOptions opid, const char *switch_name)
{
    assert((opid >= _ABRT_OPT_BEGIN_ && opid < _ABRT_OPT_END_) || !"Out of range Option ID value");

    AbrtConfigWidgetOption *option = &(self->priv->options[opid]);
    update_option_current_value(self, opid);

    GtkSwitch *gsw = GTK_SWITCH(WID(switch_name));
    option->widget = gsw;
    gtk_switch_set_active(gsw, option->current_value);
    g_object_set_data(G_OBJECT(gsw), "abrt-option", option);
    g_signal_connect(G_OBJECT(gsw), "notify::active",
            G_CALLBACK(on_switch_activate), self);
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

    self->priv->report_gtk_conf = abrt_app_configuration_new("report-gtk");
    self->priv->abrt_applet_conf = abrt_app_configuration_new("abrt-applet");

    /* Initialize options */
    /* report-gtk */
    self->priv->options[ABRT_OPT_STEAL_DIRECTORY].name = "ask_steal_dir";
    self->priv->options[ABRT_OPT_STEAL_DIRECTORY].default_value = TRUE;
    self->priv->options[ABRT_OPT_STEAL_DIRECTORY].config = self->priv->report_gtk_conf;

    self->priv->options[ABRT_OPT_UPLOAD_COREDUMP].name = "abrt_analyze_smart_ask_upload_coredump";
    self->priv->options[ABRT_OPT_UPLOAD_COREDUMP].default_value = TRUE;
    self->priv->options[ABRT_OPT_UPLOAD_COREDUMP].config = self->priv->report_gtk_conf;

    self->priv->options[ABRT_OPT_PRIVATE_TICKET].name = CREATE_PRIVATE_TICKET;
    self->priv->options[ABRT_OPT_PRIVATE_TICKET].default_value = FALSE;
    self->priv->options[ABRT_OPT_PRIVATE_TICKET].config = self->priv->report_gtk_conf;

    /* abrt-applet */
    self->priv->options[ABRT_OPT_SEND_UREPORT].name = "AutoreportingEnabled";
    self->priv->options[ABRT_OPT_SEND_UREPORT].default_value = g_settings_autoreporting;
    self->priv->options[ABRT_OPT_SEND_UREPORT].config = self->priv->abrt_applet_conf;

    self->priv->options[ABRT_OPT_SHORTENED_REPORTING].name = "ShortenedReporting";
    self->priv->options[ABRT_OPT_SHORTENED_REPORTING].default_value = g_settings_shortenedreporting;
    self->priv->options[ABRT_OPT_SHORTENED_REPORTING].config = self->priv->abrt_applet_conf;

    self->priv->options[ABRT_OPT_SILENT_SHORTENED_REPORTING].name = "SilentShortenedReporting";
    self->priv->options[ABRT_OPT_SILENT_SHORTENED_REPORTING].default_value = FALSE;
    self->priv->options[ABRT_OPT_SILENT_SHORTENED_REPORTING].config = self->priv->abrt_applet_conf;

    /* Connect widgets with options */
    connect_switch_with_option(self, ABRT_OPT_UPLOAD_COREDUMP, "switch_upload_coredump");
    connect_switch_with_option(self, ABRT_OPT_STEAL_DIRECTORY, "switch_steal_directory");
    connect_switch_with_option(self, ABRT_OPT_PRIVATE_TICKET, "switch_private_ticket");
    connect_switch_with_option(self, ABRT_OPT_SEND_UREPORT, "switch_send_ureport");
    connect_switch_with_option(self, ABRT_OPT_SHORTENED_REPORTING, "switch_shortened_reporting");
    connect_switch_with_option(self, ABRT_OPT_SILENT_SHORTENED_REPORTING, "switch_silent_shortened_reporting");

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
abrt_config_widget_reset_to_defaults(AbrtConfigWidget *self)
{
    for(unsigned i = _ABRT_OPT_BEGIN_; i < _ABRT_OPT_END_; ++i)
        gtk_switch_set_active(self->priv->options[i].widget, self->priv->options[i].default_value);
}
