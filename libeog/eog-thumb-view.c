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

#include <gtk/gtk.h>

#include "eog-thumb-view.h"
#include "eog-list-store.h"
#include "eog-image.h"

struct _EogThumbViewPrivate {
	/* put something here */ 
	guint nothing;
};

static void eog_thumb_view_class_init (EogThumbViewClass *class);
static void eog_thumb_view_init (EogThumbView *admin);
static void eog_thumb_view_finalize (GObject *object);
static void eog_thumb_view_destroy (GtkObject *object);

EogImage *
eog_thumb_view_get_image_from_path (EogThumbView *view, GtkTreePath *path);


static GtkIconViewClass *parent_class = NULL;

#define EOG_THUMB_VIEW_SPACING 16

GType
eog_thumb_view_get_type (void)
{
	static GType type = 0;
  
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EogThumbViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eog_thumb_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EogThumbView),
			0,              /* n_preallocs */
			(GInstanceInitFunc) eog_thumb_view_init,
			0
		};
		
		type = g_type_register_static (GTK_TYPE_ICON_VIEW,
					       "EogThumbView",
					       &info, 0);
	}
	
	return type;
}

static void
eog_thumb_view_class_init (EogThumbViewClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	gobject_class->finalize = eog_thumb_view_finalize;
	object_class->destroy = eog_thumb_view_destroy;
}

static void
eog_thumb_view_init (EogThumbView *thumb_view)
{
	thumb_view->priv = g_new0 (EogThumbViewPrivate, 1);
	
 	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (thumb_view),
 					  GTK_SELECTION_MULTIPLE);

	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (thumb_view), 
					 EOG_LIST_STORE_THUMBNAIL);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (thumb_view), 
				       EOG_LIST_STORE_CAPTION);
	
	gtk_icon_view_set_column_spacing (GTK_ICON_VIEW (thumb_view), 
					  EOG_THUMB_VIEW_SPACING);

	gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (thumb_view), 
					  EOG_THUMB_VIEW_SPACING);
}

static void
eog_thumb_view_finalize (GObject *object)
{
	EogThumbView *thumb_view;
	g_return_if_fail (EOG_IS_THUMB_VIEW (object));
	thumb_view = EOG_THUMB_VIEW (object);
	
	/* free whatever is needed here */
	
	g_free (thumb_view->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
eog_thumb_view_destroy (GtkObject *object)
{
	EogThumbView *thumb_view;
	g_return_if_fail (EOG_IS_THUMB_VIEW (object));
	thumb_view = EOG_THUMB_VIEW (object);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
eog_thumb_view_new (void)
{
	EogThumbView *thumb_view;
	thumb_view = g_object_new (EOG_TYPE_THUMB_VIEW, NULL);
	return GTK_WIDGET (thumb_view);
}

void
eog_thumb_view_set_model (EogThumbView *view, EogListStore *store)
{
	g_return_if_fail (EOG_IS_THUMB_VIEW (view));
	g_return_if_fail (EOG_IS_LIST_STORE (store));
	
	gint index = eog_list_store_get_initial_pos (store);

	gtk_icon_view_set_model (GTK_ICON_VIEW (view), GTK_TREE_MODEL (store));

	if (index >= 0) {
		GtkTreePath *path = gtk_tree_path_new_from_indices (index, -1);
		gtk_icon_view_select_path (GTK_ICON_VIEW (view), path);
		gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (view), path, FALSE, 0, 0);
		gtk_tree_path_free (path);
	}
}

static void
eog_thumb_view_get_n_selected_helper (GtkIconView *icon_view,
				      GtkTreePath *path,
				      gpointer data)
{
	/* data is of type (guint *) */
	(*(guint *) data) ++;
}

guint
eog_thumb_view_get_n_selected (EogThumbView *view)
{
	guint count = 0;
	gtk_icon_view_selected_foreach (GTK_ICON_VIEW (view),
					eog_thumb_view_get_n_selected_helper,
					(&count));
	return count;
}

EogImage *
eog_thumb_view_get_first_selected_image (EogThumbView *view)
{
	/* The returned list is not sorted! We need to find the 
	   smaller tree path value => tricky and expensive. Do we really need this?
	*/
	EogImage *image;
	GList *list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (view));

	if (list == NULL) {
		return NULL;
	}

	GtkTreePath *path = (GtkTreePath *) (list->data);

	/* debugging purposes */
	gchar *text_path;
	image = eog_thumb_view_get_image_from_path (view, path);
	text_path = gtk_tree_path_to_string (path);
	g_free (text_path);

	g_list_foreach (list, (GFunc) gtk_tree_path_free , NULL);
	g_list_free (list);

	return image;
}

EogImage *
eog_thumb_view_get_image_from_path (EogThumbView *view, GtkTreePath *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	EogImage *image;

	model = gtk_icon_view_get_model (GTK_ICON_VIEW (view));
	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);
			    
	return image;
}

GList *
eog_thumb_view_get_selected_images (EogThumbView *view)
{
	GList *l, *item;
	GList *list = NULL;

	GtkTreePath *path;

	l = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (view));

	for (item = l; item != NULL; item = item->next) {
		path = (GtkTreePath *) item->data;
		list = g_list_prepend (list, eog_thumb_view_get_image_from_path (view, path));
		gtk_tree_path_free (path);
	}

	g_list_free (l);
	list = g_list_reverse (list);
	return list;
}

void
eog_thumb_view_set_current_image (EogThumbView *view, EogImage *image,
				  gboolean deselect_other)
{
	GtkTreePath *path;
	EogListStore *store;
	gint pos;

	store = EOG_LIST_STORE (gtk_icon_view_get_model (GTK_ICON_VIEW (view)));
	pos = eog_list_store_get_pos_by_image (store, image);
	path = gtk_tree_path_new_from_indices (pos, -1);

	if (path == NULL) {
		return;
	}

       if (deselect_other) {
		gtk_icon_view_unselect_all (GTK_ICON_VIEW (view));
	}
	
	gtk_icon_view_select_path (GTK_ICON_VIEW (view), path);
	gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (view), path, FALSE, 0, 0);

	gtk_tree_path_free (path);
}

void
eog_thumb_view_select_single (EogThumbView *view, 
			      EogThumbViewSelectionChange change)
{
	g_return_if_fail (EOG_IS_THUMB_VIEW (view));
	GtkTreePath *path = NULL;
	GtkTreeModel *model;
	GList *list;
	gint n_items;

	model = gtk_icon_view_get_model (GTK_ICON_VIEW (view));
	n_items = gtk_tree_model_iter_n_children (model, NULL);
	
	if (eog_thumb_view_get_n_selected (view) == 0) {
		switch (change) {
		case EOG_THUMB_VIEW_SELECT_RIGHT:
		case EOG_THUMB_VIEW_SELECT_FIRST:
			path = gtk_tree_path_new_first ();
			break;
		case EOG_THUMB_VIEW_SELECT_LEFT:
		case EOG_THUMB_VIEW_SELECT_LAST:
			path = gtk_tree_path_new_from_indices (n_items - 1, -1);
		}
	} else {
		list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (view));
		path = gtk_tree_path_copy ((GtkTreePath *) list->data);
		g_list_foreach (list, (GFunc) gtk_tree_path_free , NULL);
		g_list_free (list);
		
		gtk_icon_view_unselect_all (GTK_ICON_VIEW (view));
		
		switch (change) {
		case EOG_THUMB_VIEW_SELECT_LEFT:
			if (!gtk_tree_path_prev (path)) {
				gtk_tree_path_free (path);
				path = gtk_tree_path_new_from_indices (n_items - 1, -1);
			}
			break;
		case EOG_THUMB_VIEW_SELECT_RIGHT:
			if (gtk_tree_path_get_indices (path) [0] == n_items - 1) {
				gtk_tree_path_free (path);
				path = gtk_tree_path_new_first ();
			} else {
				gtk_tree_path_next (path);
			}
			break;
		case EOG_THUMB_VIEW_SELECT_FIRST:
			gtk_tree_path_free (path);
			path = gtk_tree_path_new_first ();
			break;
		case EOG_THUMB_VIEW_SELECT_LAST:
			gtk_tree_path_free (path);
			path = gtk_tree_path_new_from_indices (n_items - 1, -1);
		}
	}

	gtk_icon_view_select_path (GTK_ICON_VIEW (view), path);
	gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (view), path, FALSE, 0, 0);
	gtk_tree_path_free (path);
}
