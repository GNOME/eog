/* Eye of Gnome image viewer - user interface for image views
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include <gtk/gtkscrolledwindow.h>

G_BEGIN_DECLS

#define TYPE_UI_IMAGE            (ui_image_get_type ())
#define UI_IMAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_UI_IMAGE, UIImage))
#define UI_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_UI_IMAGE, UIImageClass))
#define IS_UI_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_UI_IMAGE))
#define IS_UI_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_UI_IMAGE))


typedef struct _UIImage UIImage;
typedef struct _UIImageClass UIImageClass;

typedef struct _UIImagePrivate UIImagePrivate;

struct _UIImage {
	GtkScrolledWindow sf;

	/* Private data */
	UIImagePrivate *priv;
};

struct _UIImageClass {
	GtkScrolledWindowClass parent_class;
};


GtkType ui_image_get_type (void);

GtkWidget *ui_image_new (void);
GtkWidget *ui_image_construct (UIImage *ui);

GtkWidget *ui_image_get_image_view (UIImage *ui);

void ui_image_zoom_fit (UIImage *ui);

void ui_image_fit_to_screen (UIImage *ui);



G_END_DECLS

#endif
