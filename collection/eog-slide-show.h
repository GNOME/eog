/* Eye of Gnome image viewer - full-screen view mode
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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

#ifndef _EOG_SLIDE_SHOW_H_
#define _EOG_SLIDE_SHOW_H_

#include <glib/gmacros.h>
#include <gtk/gtkwindow.h>
#include <eog-collection-model.h>

G_BEGIN_DECLS

#define EOG_TYPE_SLIDE_SHOW            (eog_slide_show_get_type ())
#define EOG_SLIDE_SHOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_SLIDE_SHOW, EogSlideShow))
#define EOG_SLIDE_SHOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_SLIDE_SHOW, EogSlideShowClass))
#define EOG_IS_SLIDE_SHOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_SLIDE_SHOW))
#define EOG_IS_SLIDE_SHOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_SLIDE_SHOW))

typedef struct _EogSlideShow        EogSlideShow;
typedef struct _EogSlideShowPrivate EogSlideShowPrivate;
typedef struct _EogSlideShowClass   EogSlideShowClass;

struct _EogSlideShow {
	GtkWindow window;

	EogSlideShowPrivate *priv;
};

struct _EogSlideShowClass {
	GtkWindowClass parent_class;
};

GType      eog_slide_show_get_type (void);
GtkWidget *eog_slide_show_new (EogCollectionModel *model);

G_END_DECLS

#endif /* _EOG_SLIDE_SHOW_H_ */
