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
    GtkStatusIcon *ap_status_icon;
    GtkWidget *ap_menu;

//        std::map<int, std::string> m_mapEvents;
    bool ap_daemon_running;
    int ap_animation_stage;
    guint ap_animator;
    unsigned ap_anim_countdown;
    bool ap_icons_loaded;
    const char *ap_last_crash_id;

    GdkPixbuf *ap_icon_stages_buff[ICON_STAGE_LAST];
};

struct applet* applet_new(const char *app_name);
void applet_destroy(struct applet *applet);

void show_icon(struct applet *applet);
void hide_icon(struct applet *applet);
void set_icon_tooltip(struct applet *applet, const char *format, ...);
void show_crash_notification(struct applet *applet, const char* crash_id, const char *format, ...);
void show_msg_notification(struct applet *applet, const char *format, ...);
void disable(struct applet *applet, const char *reason);
void enable(struct applet *applet, const char *reason);

#endif
