/* Eye of Gnome image viewer - main window widget
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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

#ifndef WINDOW_H
#define WINDOW_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-app.h>

BEGIN_GNOME_DECLS



#define TYPE_WINDOW            (window_get_type ())
#define WINDOW(obj)            (GTK_CHECK_CAST ((obj), TYPE_WINDOW, Window))
#define WINDOW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_WINDOW, WindowClass))
#define IS_WINDOW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_WINDOW))
#define IS_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_WINDOW))


typedef struct _Window Window;
typedef struct _WindowClass WindowClass;


struct _Window {
	GnomeApp app;

	/* Private data */
	gpointer priv;
};

struct _WindowClass {
	GnomeAppClass parent_class;
};


GtkType window_get_type (void);
GtkWidget *window_new (void);
void window_construct (Window *window);

void window_open_image (Window *window, const char *filename);



END_GNOME_DECLS

#endif
