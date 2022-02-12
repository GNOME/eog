/* Eye of Gnome - Main Window
 *
 * Copyright (C) 2000-2008 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 * 	- Federico Mena-Quintero <federico@gnome.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __EOG_WINDOW_H__
#define __EOG_WINDOW_H__

#include "eog-list-store.h"
#include "eog-image.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gmenu.h>
#include <gtk/gtk.h>
#include <handy.h>

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

typedef enum {
	EOG_WINDOW_MODE_UNKNOWN,
	EOG_WINDOW_MODE_NORMAL,
	EOG_WINDOW_MODE_FULLSCREEN,
	EOG_WINDOW_MODE_SLIDESHOW
} EogWindowMode;

typedef enum {
	EOG_WINDOW_GALLERY_POS_BOTTOM,
	EOG_WINDOW_GALLERY_POS_LEFT,
	EOG_WINDOW_GALLERY_POS_TOP,
	EOG_WINDOW_GALLERY_POS_RIGHT
} EogWindowGalleryPos;

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
	EOG_STARTUP_DISABLE_GALLERY    = 1 << 2,
	EOG_STARTUP_SINGLE_WINDOW      = 1 << 3
} EogStartupFlags;

struct _EogWindow {
	HdyApplicationWindow win;

	EogWindowPrivate *priv;
};

struct _EogWindowClass {
	HdyApplicationWindowClass parent_class;

	void (* prepared) (EogWindow *window);
};

GType         eog_window_get_type  	(void) G_GNUC_CONST;

GtkWidget    *eog_window_new		(EogStartupFlags  flags);

EogWindowMode eog_window_get_mode       (EogWindow       *window);

void          eog_window_set_mode       (EogWindow       *window,
					 EogWindowMode    mode);

GMenu        *eog_window_get_gear_menu_section (EogWindow   *window,
						const gchar *id);

EogListStore *eog_window_get_store      (EogWindow       *window);

GtkWidget    *eog_window_get_view       (EogWindow       *window);

GtkWidget    *eog_window_get_sidebar    (EogWindow       *window);

GtkWidget    *eog_window_get_thumb_view (EogWindow       *window);

GtkWidget    *eog_window_get_thumb_nav  (EogWindow       *window);

GtkWidget    *eog_window_get_statusbar  (EogWindow       *window);

EogImage     *eog_window_get_image      (EogWindow       *window);

void          eog_window_open_file_list	(EogWindow       *window,
					 GSList          *file_list);

gboolean      eog_window_is_empty 	(EogWindow       *window);
gboolean      eog_window_is_not_initializing (const EogWindow *window);

void          eog_window_reload_image (EogWindow *window);
GtkWidget    *eog_window_get_remote_presenter (EogWindow *window);
GtkWidget    *eog_window_get_properties_dialog (EogWindow *window);

void          eog_window_show_about_dialog (EogWindow    *window);
void          eog_window_show_preferences_dialog (EogWindow *window);

void          eog_window_close          (EogWindow *window);

G_END_DECLS

#endif
