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
    
#include "DBusManager.h"
#include <cstring>
#include <iostream>
#include <climits>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv){
    GMainLoop *mainloop;
    mainloop = g_main_loop_new(NULL, FALSE);
    CDBusManager dm;
    try
    {
        dm.RegisterService();
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
        return -1;
    }
    while(1)
    {
        dm.SendMessage("Crash","Svete");
        sleep(1);
    }
    g_main_loop_run(mainloop);
    /*
    //no sanity check, it's just a testing program!
    if (argc < 2){
        std::cout << "Usage: " << argv[0] << " {s|c}" << std::endl;
        return 0;
    }
    //service
    if (strcmp(argv[1], "s") == 0){
        std::cout << "Service: " << std::endl;
        CDBusManager dm;
        try
        {
            dm.RegisterService();
        }
        catch(std::string err)
        {
            std::cerr << err << std::endl;
            return -1;
        }
        dm.LoopSend();
            
        g_main_loop_run(mainloop);
    }
    //client
    else if (strcmp(argv[1], "c") == 0){
        CDBusManager dm;
        try
        {
            dm.ConnectToService();
        }
        catch(std::string error)
        {
            std::cerr << error << std::endl;
            return -1;
        }
        dm.RegisterToMessage("Crash",G_CALLBACK(print_cb),NULL,NULL);
        g_main_loop_run(mainloop);
    }
    */
    return 0;
}
