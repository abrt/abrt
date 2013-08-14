/*
 *  Copyright (C) 2013  Red Hat
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _ABRT_CONFIG_WIDGET_H
#define _ABRT_CONFIG_WIDGET_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

G_BEGIN_DECLS

#define TYPE_ABRT_CONFIG_WIDGET            (abrt_config_widget_get_type())
#define ABRT_CONFIG_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ABRT_CONFIG_WIDGET, AbrtConfigWidget))
#define ABRT_CONFIG_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_ABRT_CONFIG_WIDGET, AbrtConfigWidgetClass))
#define IS_ABRT_CONFIG_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ABRT_CONFIG_WIDGET))
#define IS_ABRT_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_ABRT_CONFIG_WIDGET))
#define ABRT_CONFIG_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_ABRT_CONFIG_WIDGET, AbrtConfigWidgetClass))

typedef struct _AbrtConfigWidget        AbrtConfigWidget;
typedef struct _AbrtConfigWidgetClass   AbrtConfigWidgetClass;
typedef struct AbrtConfigWidgetPrivate  AbrtConfigWidgetPrivate;

struct _AbrtConfigWidget {
   GtkBox    parent_instance;
   AbrtConfigWidgetPrivate *priv;
};

struct _AbrtConfigWidgetClass {
   GtkBoxClass parent_class;

   void (*changed)(AbrtConfigWidget *config);
};

GType abrt_config_widget_get_type (void) G_GNUC_CONST;

AbrtConfigWidget *abrt_config_widget_new();

void abrt_config_widget_reset_to_defaults(AbrtConfigWidget *self);

void abrt_config_widget_save_chnages(AbrtConfigWidget *config);

gboolean abrt_config_widget_get_changed(AbrtConfigWidget *config);

G_END_DECLS

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ABRT_CONFIG_WIDGET_H */

