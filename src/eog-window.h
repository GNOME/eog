/* Eye of Gnome - Main Window
 *
 * Copyright (C) 2000-2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 * 	- Federico Mena-Quintero <federico@gnu.org>
 *	- Jens Finke <jens@gnome.org>
 * Based on evince code (shell/ev-window.c) by: 
 * 	- Martin Kretzschmar <martink@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EOG_WINDOW_H__
#define __EOG_WINDOW_H__

#include "eog-list-store.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS 

typedef struct _EogWindow EogWindow;
typedef struct _EogWindowClass EogWindowClass;
typedef struct _EogWindowPrivate EogWindowPrivate;

#define EOG_TYPE_WINDOW            (eog_window_get_type ())
#define EOG_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_WINDOW, EogWindow))
#define EOG_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  EOG_TYPE_WINDOW, EogWindowClass))
#define EOG_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_WINDOW))
#define EOG_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  EOG_TYPE_WINDOW))
#define EOG_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  EOG_TYPE_WINDOW, EogWindowClass))

#define EOG_WINDOW_ERROR           (eog_window_error_quark ())

//TODO
typedef enum {
	EOG_WINDOW_ERROR_CONTROL_NOT_FOUND,
	EOG_WINDOW_ERROR_UI_NOT_FOUND,
	EOG_WINDOW_ERROR_NO_PERSIST_FILE_INTERFACE,
	EOG_WINDOW_ERROR_IO,
	EOG_WINDOW_ERROR_TRASH_NOT_FOUND,
	EOG_WINDOW_ERROR_GENERIC,
	EOG_WINDOW_ERROR_UNKNOWN
} EogWindowError;

typedef enum {
	EOG_STARTUP_FULLSCREEN         = 1 << 0,
	EOG_STARTUP_SLIDE_SHOW         = 1 << 1,
	EOG_STARTUP_DISABLE_COLLECTION = 1 << 2
} EogStartupFlags;

struct _EogWindow {
	GtkWindow win;

	EogWindowPrivate *priv;
};

struct _EogWindowClass {
	GtkWindowClass parent_class;

	void (* prepared) (EogWindow *window);
};

GType        eog_window_get_type  	(void);

GtkWidget*   eog_window_new		(EogStartupFlags  flags);

const char*  eog_window_get_uri		(EogWindow       *window);

void         eog_window_open_uri_list	(EogWindow       *window, 
					 GSList          *uri_list);

gboolean     eog_window_is_empty 	(EogWindow       *window);

G_END_DECLS

#endif
