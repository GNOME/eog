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

#include <libgnomecanvas/gnome-canvas.h>
#include "eog-image-list.h"

G_BEGIN_DECLS



#define EOG_TYPE_WRAP_LIST            (eog_wrap_list_get_type ())
#define EOG_WRAP_LIST(obj)            (GTK_CHECK_CAST ((obj), EOG_TYPE_WRAP_LIST, EogWrapList))
#define EOG_WRAP_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_WRAP_LIST,	\
				       EogWrapListClass))
#define EOG_IS_WRAP_LIST(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_WRAP_LIST))
#define EOG_IS_WRAP_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_WRAP_LIST))

typedef struct _EogWrapList EogWrapList;
typedef struct _EogWrapListPrivate EogWrapListPrivate;
typedef struct _EogWrapListClass EogWrapListClass;

struct _EogWrapList {
	GnomeCanvas parent_object;

	EogWrapListPrivate *priv;
};

struct _EogWrapListClass {
	GnomeCanvasClass parent_class;

	gboolean (* right_click)  (EogWrapList *wlist, gint unique_id, GdkEvent *);
	void     (* double_click) (EogWrapList *wlist, gint unique_id);
	void     (* selection_changed) (EogWrapList *list);
};


GType eog_wrap_list_get_type (void);

GtkWidget* eog_wrap_list_new (void);

void eog_wrap_list_set_model (EogWrapList *wlist, EogImageList *model);

void eog_wrap_list_set_col_spacing (EogWrapList *wlist, guint spacing);
void eog_wrap_list_set_row_spacing (EogWrapList *wlist, guint spacing);

int eog_wrap_list_get_n_selected (EogWrapList *wlist);

EogImage* eog_wrap_list_get_first_selected_image (EogWrapList *wlist);

GList* eog_wrap_list_get_selected_images (EogWrapList *wlist);

void eog_wrap_list_select_left (EogWrapList *wlist);
void eog_wrap_list_select_right (EogWrapList *wlist);
void eog_wrap_list_set_current_image (EogWrapList *wlist, EogImage *image, gboolean deselect_other);



G_END_DECLS

#endif
