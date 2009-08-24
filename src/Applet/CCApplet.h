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

#ifndef CC_APPLET_H_
#define CC_APPLET_H_

#include <gtk/gtk.h>
#include <map>
#include <string>
#include <DBusClientProxy.h>
#include<libnotify/notify.h>

class CApplet
: public CDBusClient_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
    private:
        static const gchar *menu_xml;

        GtkStatusIcon* m_pStatusIcon;
        GObject *m_pMenu;
        GtkBuilder *m_pBuilder;
        GObject *m_pmiHide;
        GObject *m_pmiQuit;

        NotifyNotification *m_pNotification;
        std::map<int, std::string > m_mapEvents;
        DaemonWatcher *m_pDaemonWatcher;
        bool m_bDaemonRunning;
    public:
        CApplet(DBus::Connection &connection, const char *path, const char *name);
        ~CApplet();
        void ShowIcon();
        void HideIcon();
        //void DisableIcon();
        void BlinkIcon(bool pBlink);
        void SetIconTooltip(const char *format, ...);
        void Disable(const char *reason);
        void Enable(const char *reason);
        // create some event storage, to let user choose
        // or ask the daemon every time?
        // maybe just events which occured during current session
        // map::
        int AddEvent(int pUUID, const std::string& pProgname);
        int RemoveEvent(int pUUID);
        void ConnectCrashHandler(void (*pCrashHandler)(const char *progname));
        static void DaemonStateChange_cb(bool running, void* data);

    protected:
        //@@TODO applet menus
        static void OnAppletActivate_CB(GtkStatusIcon *status_icon,gpointer user_data);
        static void OnMenuPopup_cb(GtkStatusIcon *status_icon,
                            guint          button,
                            guint          activate_time,
                            gpointer       user_data);
        static void onHide_cb(GtkMenuItem *menuitem, gpointer applet);
    private:
        /* dbus stuff */
        void Crash(std::string &value);

        /* the real signal handler called to handle the signal */
        void (*m_pCrashHandler)(const char *progname);
};

#endif /*CC_APPLET_H_*/
