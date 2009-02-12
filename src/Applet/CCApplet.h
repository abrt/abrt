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

#include <gtkmm.h>

class CApplet
{
    private:
        Glib::RefPtr<Gtk::StatusIcon> m_nStatusIcon;
        std::map<int, std::string > m_mapEvents;
	public:
        CApplet();
        ~CApplet();
        void ShowIcon();
        void HideIcon();
        //void DisableIcon();
        void BlinkIcon(bool pBlink);
        void SetIconTooltip(const char *format, ...);
        // create some event storage, to let user choose
        // or ask the daemon every time?
        // maybe just events which occured during current session
        // map::
        int AddEvent(int pUUID, const std::string& pProgname);
        int RemoveEvent(int pUUID);
    protected:
        //@@TODO applet menus
        void OnAppletActivate_CB();
        void OnMenuPopup_cb(guint button, guint32 activate_time);
        //menu
        Glib::RefPtr<Gtk::UIManager> m_refUIManager;
        Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;
        Gtk::Menu* m_pMenuPopup;
};

#endif /*CC_APPLET_H_*/
