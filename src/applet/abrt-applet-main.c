#include "abrt-applet-application.h"

#include <libnotify/notify.h>
#include <libreport/internal_libreport.h>
#include <locale.h>

int
main (int    argc,
      char **argv)
{
    g_autoptr (GApplication) application = NULL;

    setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, LOCALEDIR);
    textdomain (PACKAGE);
#endif

    abrt_init (argv);

    application = abrt_applet_application_new ();

    return g_application_run (application, argc, argv);
}
