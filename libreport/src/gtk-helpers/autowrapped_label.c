/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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
#include <gtk/gtk.h>
#include "libreport.h"

/*
 * GTK doesn't re-wrap GtkLabels which have line wrapping set to true.
 * The line wrapped label in VBox looks like this:
 * |----------------------------------------|
 * |----------------------------------------|
 * | word word word word                    |
 * | word word word word                    |
 * | word word                              |
 * |----------------------------------------|
 * |----------------------------------------|
 * So every project copy-pastes this code to make labels widen
 * and shrink horizontally on resize.
 */

static void rewrap_label_to_parent_size(GtkWidget *widget,
                GtkAllocation *allocation,
                gpointer data)
{
    GtkLabel *label = GTK_LABEL(widget);
    PangoLayout *layout = gtk_label_get_layout(label);

    int lw_old, lh_old;
    pango_layout_get_pixel_size(layout, &lw_old, &lh_old);

    /* Already right size? */
    if (lw_old == allocation->width)
        return;

    /* Rewrap text to new width */
    pango_layout_set_width(layout, allocation->width * PANGO_SCALE);

    /* Did text height change as a result? */
    int lh;
    pango_layout_get_pixel_size(layout, NULL, &lh);
    if (lh != lh_old) /* yes, resize label height */
        gtk_widget_set_size_request(widget, -1, lh);
}

void make_label_autowrap_on_resize(GtkLabel *label)
{
    // So far, only tested to work on labels which were set up as:
    //gtk_label_set_justify(label, GTK_JUSTIFY_LEFT);
    //gtk_misc_set_alignment(GTK_MISC(label), /*x,yalign:*/ 0.0, 0.0);
    // yalign != 0 definitely breaks things!
    // also, <property name="ypad">NONZERO</property> would be bad

    /* Makes no sense on non-wrapped labels, so we can as well
     * set wrapping to "on" unconditionally, istead of making it a requirement
     */
    gtk_label_set_line_wrap(label, TRUE);
    g_signal_connect(G_OBJECT(label), "size-allocate", G_CALLBACK(rewrap_label_to_parent_size), NULL);

    // So far, only tested to work on labels which were set up as:
    //gtk_box_pack_start(box, label, /*expand*/ false, /*fill*/ false, /*padding*/ 0);
}

static void fixer(GtkWidget *widget, gpointer data_unused)
{
    if (GTK_IS_CONTAINER(widget))
    {
        gtk_container_foreach((GtkContainer*)widget, fixer, NULL);
        return;
    }
    if (GTK_IS_LABEL(widget))
    {
        GtkLabel *label = (GtkLabel*)widget;
        //const char *txt = gtk_label_get_label(label);
        GtkMisc *misc = (GtkMisc*)widget;
        gfloat yalign; //= 1;
        gint ypad; //= 1;
        if (gtk_label_get_line_wrap(label)
         && (gtk_misc_get_alignment(misc, NULL, &yalign), yalign == 0)
         && (gtk_misc_get_padding(misc, NULL, &ypad), ypad == 0)
        ) {
            //log("label '%s' set to wrap", txt);
            make_label_autowrap_on_resize(label);
            return;
        }
        //log("label '%s' not set to wrap %g %d", txt, yalign, ypad);
    }
}

void fix_all_wrapped_labels(GtkWidget *widget)
{
    fixer(widget, NULL);
}
