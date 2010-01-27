/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    */

#include <dbus/dbus-shared.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#if HAVE_CONFIG_H
    #include <config.h>
#endif
#if HAVE_LOCALE_H
    #include <locale.h>
#endif
#if ENABLE_NLS
    #include <libintl.h>
    #define _(S) gettext(S)
#else
    #define _(S) (S)
#endif
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "DBusCommon.h"
#include "CCApplet.h"


static CApplet* applet;


static void Crash(DBusMessage* signal)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(signal, &in_iter);
    const char* progname;
    r = load_val(&in_iter, progname);
    /* Optional 2nd param: uid */
    const char* uid_str = NULL;
    if (r == ABRT_DBUS_MORE_FIELDS)
    {
        r = load_val(&in_iter, uid_str);
    }
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }

    //if (m_pSessionDBus->has_name("com.redhat.abrt.gui"))
    //    return;
//    uid_t uid_num = atol(uid_str);

    if (uid_str != NULL)
    {
        char *end;
        errno = 0;
        unsigned long uid_num = strtoul(uid_str, &end, 10);
        if (errno || *end != '\0' || uid_num != getuid())
        {
            return;
        }
    }

    const char* message = _("A crash in package %s has been detected");
    //applet->AddEvent(uid, progname);
    applet->SetIconTooltip(message, progname);
    applet->ShowIcon();
    applet->CrashNotify(message, progname);
}

static void QuotaExceed(DBusMessage* signal)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(signal, &in_iter);
    const char* str;
    r = load_val(&in_iter, str);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }

    //if (m_pSessionDBus->has_name("com.redhat.abrt.gui"))
    //    return;
    applet->ShowIcon();
    applet->CrashNotify("%s", str);
}

static void NameOwnerChanged(DBusMessage* signal)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(signal, &in_iter);
    const char* name;
    r = load_val(&in_iter, name);
    if (r != ABRT_DBUS_MORE_FIELDS)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }

    /* We are only interested in (dis)appearances of our daemon */
    if (strcmp(name, "com.redhat.abrt") != 0)
        return;

    const char* old_owner;
    r = load_val(&in_iter, old_owner);
    if (r != ABRT_DBUS_MORE_FIELDS)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }
    const char* new_owner;
    r = load_val(&in_iter, new_owner);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus signal %s: parameter type mismatch", __func__);
        return;
    }

// hide icon if it's visible - as NM and don't show it, if it's not
    if (!new_owner[0])
        applet->HideIcon();
}

static DBusHandlerResult handle_message(DBusConnection* conn, DBusMessage* msg, void* user_data)
{
    const char* member = dbus_message_get_member(msg);

    VERB1 log("%s(member:'%s')", __func__, member);

    int type = dbus_message_get_type(msg);
    if (type != DBUS_MESSAGE_TYPE_SIGNAL)
    {
        log("The message is not a signal. ignoring");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(member, "NameOwnerChanged") == 0)
        NameOwnerChanged(msg);
    else if (strcmp(member, "Crash") == 0)
        Crash(msg);
    else if (strcmp(member, "QuotaExceed") == 0)
        QuotaExceed(msg);

    return DBUS_HANDLER_RESULT_HANDLED;
}

//TODO: move to abrt_dbus.cpp
static void die_if_dbus_error(bool error_flag, DBusError* err, const char* msg)
{
    if (dbus_error_is_set(err))
    {
        error_msg("dbus error: %s", err->message);
        /*dbus_error_free(&err); - why bother, we will exit in a microsecond */
        error_flag = true;
    }
    if (!error_flag)
        return;
    error_msg_and_die("%s", msg);
}

int main(int argc, char** argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* Need to be thread safe */
    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();

    /* Parse options */
    int opt;
    while ((opt = getopt(argc, argv, "dv")) != -1)
    {
        switch (opt)
        {
        case 'v':
            g_verbose++;
            break;
        default:
            error_msg_and_die(
                "Usage: abrt-applet [-v]\n"
                "\nOptions:"
                "\n\t-v\tVerbose"
            );
        }
    }
    gtk_init(&argc, &argv);

    /* Prevent zombies when we spawn abrt-gui */
    signal(SIGCHLD, SIG_IGN);

    /* Initialize our (dbus_abrt) machinery: hook _system_ dbus to glib main loop.
     * (session bus is left to be handled by libnotify, see below) */
    DBusError err;
    dbus_error_init(&err);
    DBusConnection* system_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    die_if_dbus_error(system_conn == NULL, &err, "Can't connect to system dbus");
    attach_dbus_conn_to_glib_main_loop(system_conn);
    if (!dbus_connection_add_filter(system_conn, handle_message, NULL, NULL))
        error_msg_and_die("Can't add dbus filter");
    /* which messages do we want to be fed to handle_message()? */
    //signal sender=org.freedesktop.DBus -> path=/org/freedesktop/DBus; interface=org.freedesktop.DBus; member=NameOwnerChanged
    //   string "com.redhat.abrt"
    //   string ""
    //   string ":1.70"
    dbus_bus_add_match(system_conn, "type='signal',member='NameOwnerChanged'", &err);
    die_if_dbus_error(false, &err, "Can't add dbus match");
    //signal sender=:1.73 -> path=/com/redhat/abrt; interface=com.redhat.abrt; member=Crash
    //   string "coreutils-7.2-3.fc11"
    //   string "0"
    dbus_bus_add_match(system_conn, "type='signal',path='/com/redhat/abrt'", &err);
    die_if_dbus_error(false, &err, "Can't add dbus match");

    /* Initialize GUI stuff.
     * Note: inside CApplet ctor, libnotify hooks session dbus
     * to glib main loop */
    applet = new CApplet;
    /* dbus_abrt cannot handle more than one bus, and we don't really need to.
     * The only thing we want to do is to announce ourself on session dbus */
    DBusConnection* session_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    die_if_dbus_error(session_conn == NULL, &err, "Can't connect to session dbus");
    int r = dbus_bus_request_name(session_conn,
        "com.redhat.abrt.applet",
        /* flags */ DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    die_if_dbus_error(r != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER, &err,
        "Problem connecting to dbus, or applet is already running");

    /* Show disabled icon if daemon is not running */
    if (!dbus_bus_name_has_owner(system_conn, ABRTD_DBUS_NAME, &err))
    {
        const char* msg = _("ABRT service is not running");
        puts(msg);
        applet->Disable(msg);
    }

    /* dbus_bus_request_name can already read some data. Thus while dbus fd hasn't
     * any data anymore, dbus library can buffer a message or two.
     * If we don't do this, the data won't be processed until next dbus data arrives.
     */
    int cnt = 10;
    while (dbus_connection_dispatch(system_conn) != DBUS_DISPATCH_COMPLETE && --cnt)
        continue;

    /* Enter main loop */
    gtk_main();

    gdk_threads_leave();
    delete applet;
    return 0;
}
