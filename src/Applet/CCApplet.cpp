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
    m_pStatusIcon =  gtk_status_icon_new_from_stock(GTK_STOCK_DIALOG_WARNING);
    gtk_status_icon_set_visible(m_pStatusIcon,FALSE);
    // LMB click
    //TODO add some actions!
    //gtk_signal_connect(m_pStatusIcon,"activate",CApplet::OnAppletActivate_CB, this);
    //gtk_signal_connect(m_pStatusIcon,"popup_menu",CApplet::OnMenuPopup_cb, this);
    SetIconTooltip("Pending events: %i",m_mapEvents.size());

}

CApplet::~CApplet()
{
}

void CApplet::SetIconTooltip(const char *format, ...)
{
    va_list args;
    // change to smth sane like MAX_TOOLTIP length or rewrite this whole sh*t
    size_t n,size = 30;
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
        gtk_status_icon_set_tooltip(m_pStatusIcon,buf);
    }
    else
    {
        gtk_status_icon_set_tooltip(m_pStatusIcon,"Error while setting tooltip!");
    }
    delete[] buf;
    
}

void CApplet::OnAppletActivate_CB()
{
    gtk_status_icon_set_visible(m_pStatusIcon,false);
}

void CApplet::OnMenuPopup_cb(guint button, guint32 activate_time)
{
    /* for now just hide the icon on RMB */
    gtk_status_icon_set_blinking(m_pStatusIcon, false);
}

void CApplet::ShowIcon()
{
    gtk_status_icon_set_visible(m_pStatusIcon,true);
}

void CApplet::HideIcon()
{
    gtk_status_icon_set_visible(m_pStatusIcon,false);
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
    gtk_status_icon_set_blinking(m_pStatusIcon,pBlink);
}
