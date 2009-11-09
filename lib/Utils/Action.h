/*
    Action.h - header file for action plugin

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
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

#ifndef ACTION_H_
#define ACTION_H_

#include "Plugin.h"

/**
 * An abstract class. The class defines an action plugin interface.
 */
class CAction : public CPlugin
{
    public:
        /**
         * A Method which performs particular action. As the first parameter it
         * takes an action directory. It could be either a directory of actual
         * crash or it could be a directory contains all crashes. It depends on
         * who call the plugin. The plugin can takes arguments, but the plugin
         * has to parse them by itself.
         * @param pActionDir An actual directory.
         * @param pArgs Plugin's arguments.
         */
        virtual void Run(const char *pActionDir, const char *pArgs) = 0;
};

#endif
