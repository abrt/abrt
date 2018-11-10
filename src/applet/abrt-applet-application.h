#pragma once

#include <gio/gio.h>

#define ABRT_APPLET_TYPE_APPLICATION abrt_applet_application_get_type ()
G_DECLARE_FINAL_TYPE (AbrtAppletApplication, abrt_applet_application,
                      ABRT_APPLET, APPLICATION, GApplication)

GApplication *abrt_applet_application_new (void);
