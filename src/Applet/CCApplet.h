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
        int m_iPendingEvents;
	public:
        CApplet();
        ~CApplet();
        void ShowIcon();
        void HideIcon();
        //void DisableIcon();
        void BlinkIcon(bool pBlink);
        void SetIconToolip(const Glib::ustring& tip);
    protected:
        void OnAppletActivate_CB();
        Glib::RefPtr<Gtk::UIManager> m_refUIManager;
        Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;
        Gtk::Menu* m_pMenuPopup;
};

#endif /*CC_APPLET_H_*/
