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

#include "CCApplet.h"
#include <iostream>
#include <cstdarg>
#include <sstream>
#include <cstdio>

static const char *DBUS_SERVICE_NAME = "org.freedesktop.DBus";
static const char *DBUS_SERVICE_PATH = "/org/freedesktop/DBus";

CApplet::CApplet(DBus::Connection &connection, const char *path, const char *name)
: DBus::ObjectProxy(connection, path, name)
{
    m_pDaemonWatcher = new DaemonWatcher(connection, DBUS_SERVICE_PATH, DBUS_SERVICE_NAME);
    m_pDaemonWatcher->ConnectStateChangeHandler(DaemonStateChange_cb,this);
    m_pStatusIcon =  gtk_status_icon_new_from_stock(GTK_STOCK_DIALOG_WARNING);
    m_bDaemonRunning = true;
    char notify_title[5] = "ABRT";
    notify_init(notify_title);
    m_pNotification =  notify_notification_new_with_status_icon("Warning!",NULL, NULL,m_pStatusIcon);
    notify_notification_set_urgency(m_pNotification,NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(m_pNotification, 5000);
    gtk_status_icon_set_visible(m_pStatusIcon,FALSE);
    // LMB click
    //TODO add some actions!
    g_signal_connect(G_OBJECT(m_pStatusIcon),"activate",GTK_SIGNAL_FUNC(CApplet::OnAppletActivate_CB), this);
    g_signal_connect(G_OBJECT(m_pStatusIcon),"popup_menu",GTK_SIGNAL_FUNC(CApplet::OnMenuPopup_cb), this);
    SetIconTooltip("Pending events: %i",m_mapEvents.size());
}

CApplet::~CApplet()
{
    delete m_pDaemonWatcher;
}
/* dbus related */
void CApplet::Crash(std::string &value)
{
    if(m_pCrashHandler)
    {
        m_pCrashHandler(value.c_str());
    }
    else
    {
        std::cout << "This is default handler, you should register your own with ConnectCrashHandler" << std::endl;
        std::cout.flush();
    }
}

void CApplet::DaemonStateChange_cb(bool running, void* data)
{
    CApplet *applet = (CApplet *)data;
    if(!running)
    {
        applet->Disable("ABRT service is not running!");
    }
    else
    {
        applet->Enable("ABRT service has been started!");
    }
}

void CApplet::ConnectCrashHandler(void (*pCrashHandler)(const char *progname))
{
    m_pCrashHandler = pCrashHandler;
}
/* --- */
void CApplet::SetIconTooltip(const char *format, ...)
{
    va_list args;
    // change to smth sane like MAX_TOOLTIP length or rewrite this whole sh*t
    int n;
    size_t size = 30;
    char *buf = new char[size];
    va_start (args, format);
    while((n = vsnprintf (buf, size, format, args)) > (int)size)
    {
        va_end (args);
        // string was larger than our buffer
        // alloc larger buffer
        size = n+1;
        delete[] buf;
        buf = new char[size];
        va_start (args, format);
    }
    va_end (args);
    if (n != -1)
    {
        notify_notification_update(m_pNotification, "Warning!",buf, NULL);
        gtk_status_icon_set_tooltip_text(m_pStatusIcon,buf);
    }
    else
    {
        gtk_status_icon_set_tooltip_text(m_pStatusIcon,"Error while setting the tooltip!");
    }
    delete[] buf;
    
}

void CApplet::OnAppletActivate_CB(GtkStatusIcon *status_icon,gpointer user_data)
{
    CApplet *applet = (CApplet *)user_data;
    FILE *gui = NULL;
    //FIXME - use fork+exec and absolute paths? or dbus?
    if(applet->m_bDaemonRunning)
    {
        gui = popen((std::string(BIN_DIR) + "/abrt-gui").c_str(),"r");
        gtk_status_icon_set_visible(applet->m_pStatusIcon,false);
    }
}

void CApplet::OnMenuPopup_cb(GtkStatusIcon *status_icon,
                            guint          button,
                            guint          activate_time,
                            gpointer       user_data)
{
    gtk_status_icon_set_visible(((CApplet *)user_data)->m_pStatusIcon, false);
}

void CApplet::ShowIcon()
{
    gtk_status_icon_set_visible(m_pStatusIcon,true);
    notify_notification_show(m_pNotification,NULL);
}

void CApplet::HideIcon()
{
    gtk_status_icon_set_visible(m_pStatusIcon,false);
}
void CApplet::Disable(const char *reason)
{
    /*
        FIXME: once we have our icon
    */
    m_bDaemonRunning = false;
    GdkPixbuf *gray_scaled;
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), GTK_STOCK_DIALOG_WARNING, 24, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    gray_scaled = gdk_pixbuf_copy(pixbuf);
    if(pixbuf){
        gdk_pixbuf_saturate_and_pixelate(pixbuf, gray_scaled, 0.0, NULL);
        gtk_status_icon_set_from_pixbuf(m_pStatusIcon, gray_scaled);
    }
    else
        std::cerr << "Cannot load icon!" << std::endl;
    SetIconTooltip(reason);
    ShowIcon();
}

void CApplet::Enable(const char *reason)
{
    /* restore the original icon*/
    m_bDaemonRunning = true;
    SetIconTooltip(reason);
    gtk_status_icon_set_from_stock(m_pStatusIcon,GTK_STOCK_DIALOG_WARNING);
    ShowIcon();
}

int CApplet::AddEvent(int pUUID, const std::string& pProgname)
{
    m_mapEvents[pUUID] = "pProgname";
    SetIconTooltip("Pending events: %i",m_mapEvents.size());
    return 0;
}

int CApplet::RemoveEvent(int pUUID)
{
     m_mapEvents.erase(pUUID);
     return 0;
}
void CApplet::BlinkIcon(bool pBlink)
{
    gtk_status_icon_set_blinking(m_pStatusIcon,pBlink);
}
