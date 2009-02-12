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

CApplet::CApplet()
{
    m_nStatusIcon = Gtk::StatusIcon::create(Gtk::Stock::DIALOG_WARNING);
    m_nStatusIcon->set_visible(false);
    // LMB click
    m_nStatusIcon->signal_activate().connect(sigc::mem_fun(*this, &CApplet::OnAppletActivate_CB));
    m_nStatusIcon->signal_popup_menu().connect(sigc::mem_fun(*this, &CApplet::OnMenuPopup_cb));
    SetIconTooltip("Pending events: %i",m_mapEvents.size());

}

CApplet::~CApplet()
{
}

void CApplet::SetIconTooltip(const char *format, ...)
{
    va_list args;
    // change to smth sane like MAX_TOOLTIP length or rewrite this whole sh*t
    size_t n,size = 10; 
    char *buf = new char[size];
    va_start (args, format);
    while((n = vsnprintf (buf, size, format, args)) > size)
    {
        // string was larger than our buffer
        // alloc larger buffer
        size = n+1;
        delete[] buf;
        buf = new char[size];
    }
    va_end (args);
    if (n != -1)
    {
        m_nStatusIcon->set_tooltip(Glib::ustring((const char*)buf));
    }
    else
    {
        m_nStatusIcon->set_tooltip("Error while setting tooltip!");
    }
    delete[] buf;
}

void CApplet::OnAppletActivate_CB()
{
    m_nStatusIcon->set_visible(false);
    //std::cout << "Activate" << std::endl;
    //if(m_pMenuPopup)
      //m_pMenuPopup->show();
    
}

void CApplet::OnMenuPopup_cb(guint button, guint32 activate_time)
{
    /* for now just hide the icon on RMB */
    m_nStatusIcon->set_blinking(false);
}

void CApplet::ShowIcon()
{
    m_nStatusIcon->set_visible(true);
}

void CApplet::HideIcon()
{
    m_nStatusIcon->set_visible(false);
}

int CApplet::AddEvent(int pUUID, const std::string& pProgname)
{
    m_mapEvents[pUUID] = "pProgname";
    SetIconTooltip("Pending events: %i",m_mapEvents.size());
}

int CApplet::RemoveEvent(int pUUID)
{
     m_mapEvents.erase(pUUID);
}
void CApplet::BlinkIcon(bool pBlink)
{
    m_nStatusIcon->set_blinking(pBlink);
}
