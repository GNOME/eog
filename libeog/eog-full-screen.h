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

#include <glib-object.h>
#include <glib/gmacros.h>
#include <gtk/gtkwindow.h>
#include "eog-image.h"
#include "eog-image-list.h"

#ifdef HAVE_SUNKEYSYM_H
#include <X11/Sunkeysym.h>
#endif

#ifndef HAVE_SUNKEYSYM_H
#define SunXK_F36 0x1005FF10
#endif

G_BEGIN_DECLS

#define EOG_TYPE_FULL_SCREEN            (eog_full_screen_get_type ())
#define EOG_FULL_SCREEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_FULL_SCREEN, EogFullScreen))
#define EOG_FULL_SCREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_FULL_SCREEN, EogFullScreenClass))
#define EOG_IS_FULL_SCREEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_FULL_SCREEN))
#define EOG_IS_FULL_SCREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_FULL_SCREEN))

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

GType      eog_full_screen_get_type (void);
GtkWidget *eog_full_screen_new (EogImageList *list, EogImage *start_image);

gboolean   eog_full_screen_enable_SunF36 (void);

G_END_DECLS

#endif /* _EOG_FULL_SCREEN_H_ */
