/* Eye of Gnome image viewer - full-screen view mode
 *
 * Copyright (C) 2000 The Free Software Foundation
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

#ifndef FULL_SCREEN_H
#define FULL_SCREEN_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkwindow.h>
#include "image.h"

BEGIN_GNOME_DECLS



#define TYPE_FULL_SCREEN            (full_screen_get_type ())
#define FULL_SCREEN(obj)            (GTK_CHECK_CAST ((obj), TYPE_FULL_SCREEN, FullScreen))
#define FULL_SCREEN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_FULL_SCREEN,	\
				     FullScreenClass))
#define IS_FULL_SCREEN(obj)         (GTK_CHECK_TYPE ((obj), TYPE_FULL_SCREEN))
#define IS_FULL_SCREEN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_FULL_SCREEN))

typedef struct _FullScreen FullScreen;
typedef struct _FullScreenClass FullScreenClass;

struct _FullScreen {
	GtkWindow window;

	/* Private data */
	gpointer priv;
};

struct _FullScreenClass {
	GtkWindowClass parent_class;
};

GtkType full_screen_get_type (void);

GtkWidget *full_screen_new (void);

GtkWidget *full_screen_get_ui_image (FullScreen *fs);

END_GNOME_DECLS

#endif
