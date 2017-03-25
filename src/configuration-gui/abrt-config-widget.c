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
#include <satyr/utils.h>
#include <gio/gdesktopappinfo.h>

#include "libabrt.h"
#include <assert.h>

#define ABRT_CONFIG_WIDGET_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), TYPE_ABRT_CONFIG_WIDGET, AbrtConfigWidgetPrivate))

#define WID(s) GTK_WIDGET(gtk_builder_get_object(self->priv->builder, s))

#define UI_FILE_NAME "abrt-config-widget.glade"

/* AbrtConfigWidgetPrivate:
 *     + AbrtConfigWidgetOption == "abrt-option" of GtkSwitch
 *     + AbrtConfigWidgetOption == "abrt-option" of GtkSwitch
 *     + ...
 *
 *     + AbrtAppConfiguration == config of AbrtConfigWidgetOption
 *     + AbrtAppConfiguration == config of AbrtConfigWidgetOption
 *     + ...
 */

/* This structure represents either an ABRT configuration file or a GSettings
 * schema.
 */
typedef struct {
    char *app_name;             ///< e.g abrt-applet, org.gnome.desktop.privacy
    map_string_t *settings;     ///< ABRT configuration file
    GSettings *glib_settings;   ///< GSettings
} AbrtAppConfiguration;

/* This structure represents a single switch.
 */
typedef struct {
    const char *name;           ///< e.g. ask_steal_dir, report-technical-problems
    GtkSwitch *switch_widget;
    GtkWidget *radio_button_widget[3];
    int default_value;
    int current_value;
    AbrtAppConfiguration *config;
} AbrtConfigWidgetOption;

/* Each configuration option has its own number. */
enum AbrtOptions
{
    _ABRT_OPT_BEGIN_,

    _ABRT_OPT_SWITCH_BEGIN_= _ABRT_OPT_BEGIN_,

    ABRT_OPT_STEAL_DIRECTORY= _ABRT_OPT_BEGIN_,
    ABRT_OPT_PRIVATE_TICKET,
    ABRT_OPT_SEND_UREPORT,
    ABRT_OPT_SHORTENED_REPORTING,
    ABRT_OPT_SILENT_SHORTENED_REPORTING,
    ABRT_OPT_NOTIFY_INCOMPLETE_PROBLEMS,

    _ABRT_OPT_SWITCH_END_,

    _ABRT_RADIOBUTTON_OPT_BEGIN_= _ABRT_OPT_SWITCH_END_,

    ABRT_OPT_UPLOAD_COREDUMP= _ABRT_OPT_SWITCH_END_,

    _ABRT_OPT_END_,
};

enum AbrtRadioButtonOptions
{
    _ABRT_RADIOBUTTON_OPT_ = -1,
    ABRT_RADIOBUTTON_OPT_NEVER = 0,
    ABRT_RADIOBUTTON_OPT_ALWAYS = 1,
    ABRT_RADIOBUTTON_OPT_ASK = 2,
};

/* This structure holds private data of AbrtConfigWidget
 */
struct AbrtConfigWidgetPrivate {
    GtkBuilder   *builder;
    AbrtAppConfiguration *report_gtk_conf;
    AbrtAppConfiguration *abrt_applet_conf;
    AbrtAppConfiguration *privacy_gsettings;

    /* Static array for all switches */
    AbrtConfigWidgetOption options[_ABRT_OPT_END_];
};

G_DEFINE_TYPE(AbrtConfigWidget, abrt_config_widget, GTK_TYPE_BOX)

enum {
    SN_CHANGED,
    SN_LAST_SIGNAL
} SignalNumber;

static guint s_signals[SN_LAST_SIGNAL] = { 0 };

static void abrt_config_widget_finalize(GObject *object);

/* New ABRT configuration file wrapper
 */
static AbrtAppConfiguration *
abrt_app_configuration_new(const char *app_name)
{
    AbrtAppConfiguration *conf = xmalloc(sizeof(*conf));

    conf->app_name = xstrdup(app_name);
    conf->settings = new_map_string();
    conf->glib_settings = NULL;

    if(!load_app_conf_file(conf->app_name, conf->settings)) {
        g_warning("Failed to load config for '%s'", conf->app_name);
    }

    return conf;
}

/* New GSettings wrapper
 */
static AbrtAppConfiguration *
abrt_app_configuration_new_glib(const char *schema)
{
    AbrtAppConfiguration *conf = xmalloc(sizeof(*conf));

    conf->app_name = xstrdup(schema);
    conf->settings = NULL;
    conf->glib_settings = g_settings_new(conf->app_name);

    return conf;
}

static void
abrt_app_configuration_set_value(AbrtAppConfiguration *conf, const char *name, const char *value)
{
    if (conf->settings)
        set_app_user_setting(conf->settings, name, value);
    else if (conf->glib_settings)
        g_settings_set_boolean(conf->glib_settings, name, string_to_bool(value));
    else
        assert(!"BUG: not properly initialized AbrtAppConfiguration");
}

static const char *
abrt_app_configuration_get_value(AbrtAppConfiguration *conf, const char *name)
{
    if (conf->settings)
    {
        const char *val = get_app_user_setting(conf->settings, name);
        return (val == NULL || strcmp(val, "") == 0) ? NULL : val;
    }

    if (conf->glib_settings)
        return g_settings_get_boolean(conf->glib_settings, name) ? "yes" : "no";

    assert(!"BUG: not properly initialized AbrtAppConfiguration");
}

static void
abrt_app_configuration_save(AbrtAppConfiguration *conf)
{
    if (conf->settings)
        save_app_conf_file(conf->app_name, conf->settings);

    /* No need to save GSettings because changes are applied instantly */
}

static void
abrt_app_configuration_free(AbrtAppConfiguration *conf)
{
    if (!conf)
        return;

    free(conf->app_name);
    conf->app_name = (void *)0xDEADBEAF;

    if (conf->settings)
    {
        free_map_string(conf->settings);
        conf->settings = (void *)0xDEADBEAF;
    }

    if (conf->glib_settings)
    {
        g_object_unref(conf->glib_settings);
        conf->glib_settings = (void *)0xDEADBEAF;
    }
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

    abrt_app_configuration_free(self->priv->privacy_gsettings);
    self->priv->privacy_gsettings = NULL;

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
    AbrtConfigWidgetOption *option = g_object_get_data(G_OBJECT(object), "abrt-option");
    if (option->config == NULL)
        return;

    const gboolean state = gtk_switch_get_active(GTK_SWITCH(object));
    const char *const val = state ? "yes" : "no";

    log_debug("%s : %s", option->name, val);
    abrt_app_configuration_set_value(option->config, option->name, val);
    abrt_app_configuration_save(option->config);
    emit_change(config);
}

static void
on_radio_button_toggle(GObject       *object,
        AbrtConfigWidget *config)
{
    /* inactive radio button */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object)) == false)
        return;

    AbrtConfigWidgetOption *option = g_object_get_data(G_OBJECT(object), "abrt-option");
    if (option->config == NULL)
        return;

    /* get active radio button */
    const char *val = g_object_get_data(G_OBJECT(object), "abrt-triple-switch-value");
    log_debug("%s : %s", option->name, val);

    abrt_app_configuration_set_value(option->config, option->name, val);
    abrt_app_configuration_save(option->config);
    emit_change(config);
}

static void
update_option_switch_current_value(AbrtConfigWidget *self, enum AbrtOptions opid)
{
    assert((opid >= _ABRT_OPT_SWITCH_BEGIN_ && opid < _ABRT_OPT_SWITCH_END_) || !"Out of range Option ID value");

    AbrtConfigWidgetOption *option = &(self->priv->options[opid]);

    const char *val = NULL;
    if (option->config != NULL)
        val = abrt_app_configuration_get_value(option->config, option->name);

    option->current_value = val ? string_to_bool(val) : option->default_value;
}

static void
update_option_radio_button_current_value(AbrtConfigWidget *self, enum AbrtOptions opid)
{
    assert((opid >= _ABRT_RADIOBUTTON_OPT_BEGIN_ && opid < _ABRT_OPT_END_) || !"Out of range Option ID value");

    AbrtConfigWidgetOption *option = &(self->priv->options[opid]);

    const char *val = NULL;
    if (option->config != NULL)
        val = abrt_app_configuration_get_value(option->config, option->name);

    if (val == NULL)
        option->current_value = option->default_value;
    else if (string_to_bool(val))
        option->current_value = ABRT_RADIOBUTTON_OPT_ALWAYS;
    else
        option->current_value = ABRT_RADIOBUTTON_OPT_NEVER;
}

static void
connect_switch_with_option(AbrtConfigWidget *self, enum AbrtOptions opid, const char *switch_name)
{
    assert((opid >= _ABRT_OPT_SWITCH_BEGIN_ && opid < _ABRT_OPT_SWITCH_END_) || !"Out of range Option ID value");

    AbrtConfigWidgetOption *option = &(self->priv->options[opid]);
    update_option_switch_current_value(self, opid);

    GtkSwitch *gsw = GTK_SWITCH(WID(switch_name));
    option->switch_widget = gsw;
    gtk_switch_set_active(gsw, (gboolean)option->current_value);

    g_object_set_data(G_OBJECT(gsw), "abrt-option", option);
    g_signal_connect(G_OBJECT(gsw), "notify::active",
            G_CALLBACK(on_switch_activate), self);

    /* If the option has no config, make the corresponding insensitive. */
    gtk_widget_set_sensitive(GTK_WIDGET(gsw), option->config != NULL);
}

static void
connect_radio_buttons_with_option(AbrtConfigWidget *self, enum AbrtOptions opid,
                 const char *btn_always_name, const char *btn_never_name,
                 const char *btn_ask_name)
{
    assert((opid >= _ABRT_RADIOBUTTON_OPT_BEGIN_ && opid < _ABRT_OPT_END_) || !"Out of range Option ID value");

    AbrtConfigWidgetOption *option = &(self->priv->options[opid]);
    update_option_radio_button_current_value(self, opid);

    GtkWidget *btn_always = WID(btn_always_name);
    GtkWidget *btn_never = WID(btn_never_name);
    GtkWidget *btn_ask = WID(btn_ask_name);

    option->radio_button_widget[ABRT_RADIOBUTTON_OPT_ALWAYS] = btn_always;
    option->radio_button_widget[ABRT_RADIOBUTTON_OPT_NEVER] = btn_never;
    option->radio_button_widget[ABRT_RADIOBUTTON_OPT_ASK] = btn_ask;

    GtkWidget *active_button = option->radio_button_widget[option->current_value];
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);

    g_object_set_data(G_OBJECT(btn_always), "abrt-option", option);
    g_object_set_data(G_OBJECT(btn_always), "abrt-triple-switch-value", (char *)"yes");
    g_object_set_data(G_OBJECT(btn_never), "abrt-option", option);
    g_object_set_data(G_OBJECT(btn_never), "abrt-triple-switch-value", (char *)"no");
    g_object_set_data(G_OBJECT(btn_ask), "abrt-option", option);
    g_object_set_data(G_OBJECT(btn_ask), "abrt-triple-switch-value", NULL);

    g_signal_connect(btn_always, "toggled", G_CALLBACK(on_radio_button_toggle), self);
    g_signal_connect(btn_never, "toggled", G_CALLBACK(on_radio_button_toggle), self);
    g_signal_connect(btn_ask, "toggled", G_CALLBACK(on_radio_button_toggle), self);

    /* If the option has no config, make the corresponding insensitive. */
    gtk_widget_set_sensitive(GTK_WIDGET(btn_always), option->config != NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(btn_never), option->config != NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(btn_ask), option->config != NULL);
}

static void
pp_launcher_clicked(GtkButton *launcher, gpointer *unused_data)
{
    GDesktopAppInfo *app = g_object_get_data(G_OBJECT(launcher), "launched-app");
    GError *err = NULL;
    if (!g_app_info_launch(G_APP_INFO(app), NULL, NULL, &err))
    {
        perror_msg("Could not launch '%s': %s",
                g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO (app)),
                err->message);
    }
}

static void
os_release_callback(char *key, char *value, void *data)
{
    if (strcmp(key, "PRIVACY_POLICY") == 0)
        *(char **)data = value;
    else
        free(value);
    free(key);
}

static void
abrt_config_widget_init(AbrtConfigWidget *self)
{
    GError *error = NULL;

    self->priv = ABRT_CONFIG_WIDGET_GET_PRIVATE(self);

    self->priv->builder = gtk_builder_new();
    gtk_builder_set_translation_domain(self->priv->builder, GETTEXT_PACKAGE);

    gtk_builder_add_from_file(self->priv->builder, UI_FILE_NAME, &error);
    if(error != NULL) {
        log_debug("Failed to load '%s': %s", UI_FILE_NAME, error->message);
        g_error_free(error);
        error = NULL;
        gtk_builder_add_from_file(self->priv->builder, ABRT_UI_DIR "/" UI_FILE_NAME, &error);
        if(error != NULL) {
            g_warning("Failed to load '%s': %s", ABRT_UI_DIR "/" UI_FILE_NAME, error->message);
            g_error_free(error);
            return;
        }
    }

    /* Load configuration */
    load_abrt_conf();

    self->priv->report_gtk_conf = abrt_app_configuration_new("report-gtk");
    self->priv->abrt_applet_conf = abrt_app_configuration_new("abrt-applet");
    self->priv->privacy_gsettings = abrt_app_configuration_new_glib("org.gnome.desktop.privacy");

    /* Initialize options */
    /* report-gtk */
    self->priv->options[ABRT_OPT_STEAL_DIRECTORY].name = "ask_steal_dir";
    self->priv->options[ABRT_OPT_STEAL_DIRECTORY].default_value = TRUE;
    self->priv->options[ABRT_OPT_STEAL_DIRECTORY].config = self->priv->report_gtk_conf;

    self->priv->options[ABRT_OPT_UPLOAD_COREDUMP].name = "abrt_analyze_upload_coredump";
    self->priv->options[ABRT_OPT_UPLOAD_COREDUMP].default_value = ABRT_RADIOBUTTON_OPT_ASK;
    self->priv->options[ABRT_OPT_UPLOAD_COREDUMP].config = self->priv->report_gtk_conf;
    self->priv->options[ABRT_OPT_PRIVATE_TICKET].name = CREATE_PRIVATE_TICKET;
    self->priv->options[ABRT_OPT_PRIVATE_TICKET].default_value = FALSE;
    self->priv->options[ABRT_OPT_PRIVATE_TICKET].config = self->priv->report_gtk_conf;

    /* abrt-applet */
    self->priv->options[ABRT_OPT_SEND_UREPORT].name = "report-technical-problems";
    self->priv->options[ABRT_OPT_SEND_UREPORT].default_value =
            string_to_bool(abrt_app_configuration_get_value(self->priv->privacy_gsettings,
                                                            "report-technical-problems"));
    {
        /* Get the container widget for the lauch button and warnings */
        GtkWidget *hbox_auto_reporting = WID("hbox_auto_reporting");
        assert(hbox_auto_reporting);

        /* Be able to use another desktop file while debugging */
        const char *gpp_app = getenv("ABRT_PRIVACY_APP_DESKTOP");
        if (gpp_app == NULL)
            gpp_app = "gnome-privacy-panel.desktop";

        GDesktopAppInfo *app = g_desktop_app_info_new(gpp_app);
        char *message = NULL;
        char *markup = NULL;
        if (!app)
        {
            /* Make the switch editable */
            self->priv->options[ABRT_OPT_SEND_UREPORT].config = self->priv->privacy_gsettings;

            char *os_release = xmalloc_open_read_close("/etc/os-release", /*no size limit*/NULL);
            char *privacy_policy = NULL;

            /* Try to get the value of PRIVACY_POLICY from /etc/os-release */
            sr_parse_os_release(os_release, os_release_callback, (void *)&privacy_policy);

            message = xasprintf(_("The configuration option above has been moved to GSettings and "
                                  "the switch is linked to the value of the setting 'report-technical-problems' "
                                  "from the schema 'org.gnome.desktop.privacy'."));

            /* Do not add Privacy Policy link if /etc/os-release does not contain PRIVACY_POLICY */
            if (privacy_policy != NULL)
                markup = xasprintf("<i>%s</i>\n\n<a href=\"%s\">Privacy Policy</a>", message, privacy_policy);
            else
                markup = xasprintf("<i>%s</i>", message);

            free(privacy_policy);
            free(os_release);
        }
        else
        {
            /* Make the switch read-only */
            self->priv->options[ABRT_OPT_SEND_UREPORT].config = NULL;

            message = xasprintf(_("The configuration option above can be configured in"));
            markup = xasprintf("<i>%s</i>", message);

            GtkWidget *launcher = gtk_button_new_with_label(g_app_info_get_display_name(G_APP_INFO(app)));

            /* Here we could pass the launcher to pp_launcher_clicked() as the
             * 4th argument of g_signal_connect() but we would leek the
             * launcher's memory. Therefore we need to find a way how to free
             * the launcher when it is not needed anymore. GtkWidget inherits
             * from GObject which offers a functionality for attaching an
             * arbitrary data to its instances. The last argument is a function
             * called to destroy the arbirarty data when the instance is being
             * destroyed. */
            g_object_set_data_full(G_OBJECT(launcher), "launched-app", app, g_object_unref);
            g_signal_connect(launcher, "clicked", G_CALLBACK(pp_launcher_clicked), NULL);

            /* Make the launcher button narrow, otherwise it would expand to
             * the width of the warninig. */
            gtk_widget_set_hexpand(launcher, FALSE);
            gtk_widget_set_vexpand(launcher, FALSE);

            /* Make the launcher button aligned on center of the warning. */
            gtk_widget_set_halign(launcher, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(launcher, GTK_ALIGN_CENTER);

            gtk_box_pack_end(GTK_BOX(hbox_auto_reporting), launcher, false, false, 0);
        }


        GtkWidget *lbl = gtk_label_new(message);
        gtk_label_set_markup(GTK_LABEL(lbl), markup);
        /* Do not expand the window by too long warning. */
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        /* Let users to copy the warning. */
        gtk_label_set_selectable(GTK_LABEL(lbl), TRUE);

        free(markup);
        free(message);

        gtk_box_pack_start(GTK_BOX(hbox_auto_reporting), lbl, false, false, 0);
    }

    self->priv->options[ABRT_OPT_SHORTENED_REPORTING].name = "ShortenedReporting";
    self->priv->options[ABRT_OPT_SHORTENED_REPORTING].default_value = g_settings_shortenedreporting;
    self->priv->options[ABRT_OPT_SHORTENED_REPORTING].config = self->priv->abrt_applet_conf;

    self->priv->options[ABRT_OPT_SILENT_SHORTENED_REPORTING].name = "SilentShortenedReporting";
    self->priv->options[ABRT_OPT_SILENT_SHORTENED_REPORTING].default_value = FALSE;
    self->priv->options[ABRT_OPT_SILENT_SHORTENED_REPORTING].config = self->priv->abrt_applet_conf;

    self->priv->options[ABRT_OPT_NOTIFY_INCOMPLETE_PROBLEMS].name = "NotifyIncompleteProblems";
    self->priv->options[ABRT_OPT_NOTIFY_INCOMPLETE_PROBLEMS].default_value = FALSE;
    self->priv->options[ABRT_OPT_NOTIFY_INCOMPLETE_PROBLEMS].config = self->priv->abrt_applet_conf;

    /* Connect radio buttons with options */
    connect_radio_buttons_with_option(self, ABRT_OPT_UPLOAD_COREDUMP,
                                        "bg_always", "bg_never", "bg_ask" );

    /* Connect widgets with options */
    connect_switch_with_option(self, ABRT_OPT_STEAL_DIRECTORY, "switch_steal_directory");
    connect_switch_with_option(self, ABRT_OPT_PRIVATE_TICKET, "switch_private_ticket");
    connect_switch_with_option(self, ABRT_OPT_SEND_UREPORT, "switch_send_ureport");
    connect_switch_with_option(self, ABRT_OPT_SHORTENED_REPORTING, "switch_shortened_reporting");
    connect_switch_with_option(self, ABRT_OPT_SILENT_SHORTENED_REPORTING, "switch_silent_shortened_reporting");
    connect_switch_with_option(self, ABRT_OPT_NOTIFY_INCOMPLETE_PROBLEMS, "switch_notify_incomplete_problems");

#if ((GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 13) || (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION == 13 && GTK_MICRO_VERSION == 1))
    /* https://developer.gnome.org/gtk3/3.13/GtkWidget.html#gtk-widget-reparent */
    /* gtk_widget_reparent has been deprecated since version 3.13.2 and should not be used in newly-written code. */
    gtk_widget_reparent(WID("grid"), GTK_WIDGET(self));
#else
    gtk_container_remove(GTK_CONTAINER(WID("window1")), WID("grid"));
    gtk_container_add(GTK_CONTAINER(self), WID("grid"));
#endif

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
    for(unsigned i = _ABRT_OPT_SWITCH_BEGIN_; i < _ABRT_OPT_SWITCH_END_; ++i)
        gtk_switch_set_active(self->priv->options[i].switch_widget, self->priv->options[i].default_value);

    for(unsigned i = _ABRT_RADIOBUTTON_OPT_BEGIN_; i < _ABRT_OPT_END_; ++i)
    {
        unsigned default_value = self->priv->options[i].default_value;
        GtkWidget *radio_button = self->priv->options[i].radio_button_widget[default_value];
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
    }
}
