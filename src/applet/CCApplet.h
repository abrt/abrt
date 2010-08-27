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
#include <libnotify/notify.h>

enum ICON_STAGES
{
    ICON_DEFAULT,
    ICON_STAGE1,
    ICON_STAGE2,
    ICON_STAGE3,
    ICON_STAGE4,
    ICON_STAGE5,
    /* this must be always the last */
    ICON_STAGE_LAST
};

struct applet {
    GtkStatusIcon* m_pStatusIcon;
    GtkWidget *m_pMenu;

//        std::map<int, std::string> m_mapEvents;
    bool m_bDaemonRunning;
    int m_iAnimationStage;
    guint m_iAnimator;
    unsigned m_iAnimCountdown;
    bool m_bIconsLoaded;
    const char *m_pLastCrashID;

    GdkPixbuf *icon_stages_buff[ICON_STAGE_LAST];
};

struct applet* applet_new(const char *app_name);
void applet_destroy(struct applet *applet);

void ShowIcon(struct applet *applet);
void HideIcon(struct applet *applet);
void SetIconTooltip(struct applet *applet, const char *format, ...);
void CrashNotify(struct applet *applet, const char* crash_id, const char *format, ...);
void MessageNotify(struct applet *applet, const char *format, ...);
void Disable(struct applet *applet, const char *reason);
void Enable(struct applet *applet, const char *reason);

// static in next patch
void OnAppletActivate_CB(GtkStatusIcon *status_icon, gpointer user_data);
//this action should open the reporter dialog directly, without showing the main window
void action_report(NotifyNotification *notification, gchar *action, gpointer user_data);
//this action should open the main window
void action_open_gui(NotifyNotification *notification, gchar *action, gpointer user_data);
void OnMenuPopup_cb(GtkStatusIcon *status_icon,
                    guint          button,
                    guint          activate_time,
                    gpointer       user_data);
gboolean update_icon(void *data);
void animate_icon(struct applet *applet);
void stop_animate_icon(struct applet *applet);
bool load_icons(struct applet *applet);

#endif
