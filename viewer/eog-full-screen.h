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

#ifndef _EOG_FULL_SCREEN_H_
#define _EOG_FULL_SCREEN_H_

#include <glib/gmacros.h>
#include <gtk/gtkwindow.h>
#include <eog-image-view.h>

G_BEGIN_DECLS

#define EOG_TYPE_FULL_SCREEN            (eog_full_screen_get_type ())
#define EOG_FULL_SCREEN(obj)            (GTK_CHECK_CAST ((obj), EOG_TYPE_FULL_SCREEN, EogFullScreen))
#define EOG_FULL_SCREEN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_FULL_SCREEN, EogFullScreenClass))
#define EOG_IS_FULL_SCREEN(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_FULL_SCREEN))
#define EOG_IS_FULL_SCREEN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_FULL_SCREEN))

typedef struct _EogFullScreen        EogFullScreen;
typedef struct _EogFullScreenPrivate EogFullScreenPrivate;
typedef struct _EogFullScreenClass   EogFullScreenClass;

struct _EogFullScreen {
	GtkWindow window;

	EogFullScreenPrivate *priv;
};

struct _EogFullScreenClass {
	GtkWindowClass parent_class;
};

GtkType    eog_full_screen_get_type (void);
GtkWidget *eog_full_screen_new (EogImage *eog_image);

G_END_DECLS

#endif /* _EOG_FULL_SCREEN_H_ */
