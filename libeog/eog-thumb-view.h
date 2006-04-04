/* Eye Of Gnome - Thumbnail View
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Claudio Saavedra <csaavedra@alumnos.utalca.cl>
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

#ifndef EOG_THUMB_VIEW_H
#define EOG_THUMB_VIEW_H

#include "eog-image.h"
#include "eog-list-store.h"

#define EOG_TYPE_THUMB_VIEW            (eog_thumb_view_get_type ())
#define EOG_THUMB_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_THUMB_VIEW, EogThumbView))
#define EOG_THUMB_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  EOG_TYPE_THUMB_VIEW, EogThumbViewClass))
#define EOG_IS_THUMB_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_THUMB_VIEW))
#define EOG_IS_THUMB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  EOG_TYPE_THUMB_VIEW))
#define EOG_THUMB_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  EOG_TYPE_THUMB_VIEW, EogThumbViewClass))

typedef struct _EogThumbViewPrivate EogThumbViewPrivate;
typedef struct _EogThumbView        EogThumbView;
typedef struct _EogThumbViewClass   EogThumbViewClass;

struct _EogThumbView {
	GtkIconView icon_view;
	EogThumbViewPrivate *priv;
};

struct _EogThumbViewClass {
	 GtkIconViewClass icon_view_class;
};

GType eog_thumb_view_get_type (void);

GtkWidget * eog_thumb_view_new (void);

void
eog_thumb_view_set_model (EogThumbView *view, EogListStore *store);

typedef enum {
	EOG_THUMB_VIEW_SELECT_LEFT = 0,
	EOG_THUMB_VIEW_SELECT_RIGHT,
	EOG_THUMB_VIEW_SELECT_FIRST,
	EOG_THUMB_VIEW_SELECT_LAST
} EogThumbViewSelectionChange;

guint
eog_thumb_view_get_n_selected (EogThumbView *view);

EogImage *
eog_thumb_view_get_first_selected_image (EogThumbView *view);

GList *
eog_thumb_view_get_selected_images (EogThumbView *view);

void
eog_thumb_view_select_single (EogThumbView *view, 
			      EogThumbViewSelectionChange change);

void
eog_thumb_view_set_current_image (EogThumbView *view, EogImage *image,
				  gboolean deselect_other);

#endif /* EOG_THUMB_VIEW_H */
