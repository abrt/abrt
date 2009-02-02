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

CApplet::CApplet()
{
      m_nStatusIcon = Gtk::StatusIcon::create(Gtk::Stock::DIALOG_WARNING);
      m_nStatusIcon->set_visible(false);
}

CApplet::~CApplet()
{
}

void CApplet::ShowIcon()
{
    m_nStatusIcon->set_visible(true);
}

void CApplet::HideIcon()
{
    m_nStatusIcon->set_visible(false);
}

void CApplet::BlinkIcon(bool pBlink)
{
    m_nStatusIcon->set_blinking(pBlink);
}
