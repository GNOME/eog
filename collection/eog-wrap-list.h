/* EogWrapList - view of the image collection
 *
 * Copyright (C) 2001 The Free Software Foundation
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EOG_WRAP_LIST_H
#define EOG_WRAP_LIST_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include "eog-collection-model.h"
#include "eog-item-factory.h"

BEGIN_GNOME_DECLS



#define EOG_TYPE_WRAP_LIST            (eog_wrap_list_get_type ())
#define EOG_WRAP_LIST(obj)            (GTK_CHECK_CAST ((obj), EOG_TYPE_WRAP_LIST, EogWrapList))
#define EOG_WRAP_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_WRAP_LIST,	\
				       EogWrapListClass))
#define EOG_IS_WRAP_LIST(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_WRAP_LIST))
#define EOG_IS_WRAP_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_WRAP_LIST))

typedef enum {
	EOG_LAYOUT_MODE_VERTICAL,
	EOG_LAYOUT_MODE_HORIZONTAL,
	EOG_LAYOUT_MODE_RECTANGLE
} EogLayoutMode;

typedef struct _EogWrapList EogWrapList;
typedef struct _EogWrapListPrivate EogWrapListPrivate;
typedef struct _EogWrapListClass EogWrapListClass;

struct _EogWrapList {
	GnomeCanvas parent_object;

	EogWrapListPrivate *priv;
};

struct _EogWrapListClass {
	GnomeCanvasClass parent_class;

	gboolean (* right_click)  (EogWrapList *, gint unique_id, GdkEvent *);
	void     (* double_click) (EogWrapList *, gint unique_id);
};


GtkType eog_wrap_list_get_type (void);

GtkWidget* eog_wrap_list_new (void);

void eog_wrap_list_set_model (EogWrapList *wlist, EogCollectionModel *model);
void eog_wrap_list_set_factory (EogWrapList *wlist, EogItemFactory *factory);

void eog_wrap_list_set_col_spacing (EogWrapList *wlist, guint spacing);
void eog_wrap_list_set_row_spacing (EogWrapList *wlist, guint spacing);

void eog_wrap_list_set_layout_mode (EogWrapList *wlist, EogLayoutMode lm);

void eog_wrap_list_set_background_color (EogWrapList *wlist, GdkColor *color);



END_GNOME_DECLS

#endif
