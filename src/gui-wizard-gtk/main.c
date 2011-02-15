#include <gtk/gtk.h>
#include "abrtlib.h"
#include "parse_options.h"
#include "wizard.h"

int main(int argc, char **argv)
{
    //GtkWidget *assistant;

    gtk_init(&argc, &argv);

    GtkWidget *assistant = create_assistant();
    gtk_widget_show_all(assistant);

    /* Enter main loop */
    gtk_main();
    return 0;
}