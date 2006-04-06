/* Eye Of Gnome - Image Store
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Claudio Saavedra <csaavedra@alumnos.utalca.cl>
 *
 * Based on code by: Jens Finke <jens@triq.net>
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

#include <string.h>
#include <libgnomeui/gnome-thumbnail.h>
#include "eog-list-store.h"
#include "eog-thumbnail.h"
#include "eog-image.h"

#define EOG_LIST_STORE_THUMB_SIZE 96

static GSList *supported_mime_types = NULL;

struct _EogListStorePriv {
	GList *monitors;      /* monitors for the directories */
	gint initial_image;   /* the image that should be selected firstly by the view. */
};

typedef struct {
	EogListStore *store;
	GnomeVFSURI *uri;
	GnomeVFSFileInfo *info;
} DirLoadingContext;

typedef struct {
	GnomeVFSMonitorHandle *handle;
	const gchar *text_uri;
} MonitorHandleContext;

static GtkListStore *parent_class = NULL;

G_DEFINE_TYPE (EogListStore, eog_list_store, GTK_TYPE_LIST_STORE);

static void
eog_list_store_finalize (GObject *object)
{
	EogListStore *store = EOG_LIST_STORE (object);
	
	if (store->priv != NULL) {
		g_free (store->priv);
		store->priv = NULL;
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
foreach_monitors_free (gpointer data, gpointer user_data)
{
	MonitorHandleContext *hctx = data;
	
	gnome_vfs_monitor_cancel (hctx->handle);
	
	g_free (data);
}

static void
eog_list_store_dispose (GObject *object)
{
	EogListStore *store = EOG_LIST_STORE (object);
	g_list_foreach (store->priv->monitors, 
			foreach_monitors_free, NULL);
	g_list_free (store->priv->monitors);

	store->priv->monitors = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
eog_list_store_class_init (EogListStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = eog_list_store_finalize;
	object_class->dispose = eog_list_store_dispose;
}

/*
   Sorting functions 
*/

static gint
eog_list_store_compare_func (GtkTreeModel *model,
			     GtkTreeIter *a,
			     GtkTreeIter *b,
			     gpointer user_data)
{
	gint r_value;

	EogImage *image_a, *image_b;
	GnomeVFSURI *uri_a, *uri_b;
	const gchar *path_a, *path_b;

	gtk_tree_model_get (model, a, 
			    EOG_LIST_STORE_EOG_IMAGE, &image_a,
			    -1);

	gtk_tree_model_get (model, b, 
			    EOG_LIST_STORE_EOG_IMAGE, &image_b,
			    -1);
	
	uri_a = eog_image_get_uri (image_a);
	uri_b = eog_image_get_uri (image_b);

	path_a = gnome_vfs_uri_get_path (uri_a);
	path_b = gnome_vfs_uri_get_path (uri_b);

	r_value = strcasecmp (path_a, path_b);

	g_object_unref (G_OBJECT (image_a));
	g_object_unref (G_OBJECT (image_b));
	return r_value;
}

static void
eog_list_store_init (EogListStore *self)
{
	GType types[EOG_LIST_STORE_NUM_COLUMNS];

	types[EOG_LIST_STORE_CAPTION]   = G_TYPE_STRING;
	types[EOG_LIST_STORE_THUMBNAIL] = GDK_TYPE_PIXBUF;
	types[EOG_LIST_STORE_EOG_IMAGE] = G_TYPE_OBJECT;

	gtk_list_store_set_column_types (GTK_LIST_STORE (self),
					 EOG_LIST_STORE_NUM_COLUMNS, types);
	
	self->priv = g_new0 (EogListStorePriv, 1);
	self->priv->monitors = NULL;
	self->priv->initial_image = -1;

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (self),
						 eog_list_store_compare_func,
						 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self), 
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, 
					      GTK_SORT_ASCENDING);
}

GtkListStore*
eog_list_store_new (void)
{
        return g_object_new (EOG_TYPE_LIST_STORE, NULL);
}

void
eog_list_store_append_image (EogListStore *store, EogImage *image)
{
	GtkTreeIter iter;
	GdkPixbuf *thumbnail;
	gint width, height;

	thumbnail = eog_image_get_pixbuf_thumbnail (image);

	width = gdk_pixbuf_get_width (thumbnail);
	height = gdk_pixbuf_get_height (thumbnail);

	if (width > EOG_LIST_STORE_THUMB_SIZE ||
	    height > EOG_LIST_STORE_THUMB_SIZE) {
		GdkPixbuf *scaled;
		gfloat factor;

		if (width > height) {
			factor = (gfloat) EOG_LIST_STORE_THUMB_SIZE / (gfloat) width;
		} else {
			factor = (gfloat) EOG_LIST_STORE_THUMB_SIZE / (gfloat) height;			
		}
		
		width  = width  * factor;
		height = height * factor;
		
		scaled = gnome_thumbnail_scale_down_pixbuf (thumbnail, width, height);
		g_object_unref (G_OBJECT (thumbnail));
		thumbnail = scaled;
	}

	gtk_list_store_append (GTK_LIST_STORE (store), &iter);
	gtk_list_store_set (GTK_LIST_STORE (store), &iter, 
			    EOG_LIST_STORE_EOG_IMAGE, image, 
			    EOG_LIST_STORE_CAPTION, eog_image_get_caption (image),
			    EOG_LIST_STORE_THUMBNAIL, thumbnail,
			    -1);
	g_object_unref (G_OBJECT (thumbnail));
}

void 
eog_list_store_append_image_from_uri (EogListStore *store, GnomeVFSURI *uri_entry)
{
	g_return_if_fail (EOG_IS_LIST_STORE (store));
	
	GError *error = NULL;
	/* not sure if this is the right way to set the thumbnail */
	GdkPixbuf *pixbuf = eog_thumbnail_load (uri_entry, NULL, &error);
	EogImage *image = eog_image_new_uri (uri_entry);

	if (error == NULL) {
		eog_image_set_thumbnail (image, pixbuf);
		g_object_unref (pixbuf);
		eog_list_store_append_image (store, image);
	} else {
		g_warning ("%s\n", error->message);
		g_error_free (error);
	}
	g_object_unref (G_OBJECT (image));
}

/* ================== Directory Loading stuff ===================*/


static gint
compare_quarks (gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

static GSList*
get_supported_mime_types (void)
{
	GSList *format_list;
	GSList *it;
	GSList *list = NULL;
	gchar **mime_types;
	int i;
	
	format_list = gdk_pixbuf_get_formats ();

	for (it = format_list; it != NULL; it = it->next) {
		mime_types = gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) it->data);

		for (i = 0; mime_types[i] != NULL; i++) {
			GQuark quark;

			quark = g_quark_from_string (mime_types[i]);
			list = g_slist_prepend (list,
						GINT_TO_POINTER (quark));
		}

		g_strfreev (mime_types);
	}

	list = g_slist_sort (list, (GCompareFunc) compare_quarks);
	
	g_slist_free (format_list);

	return list;
}


/* checks if the mime type is in our static list of 
 * supported mime types.
 */
static gboolean 
is_supported_mime_type (const char *mime_type) 
{
	GQuark quark;
	GSList *result;

	if (supported_mime_types == NULL) {
		supported_mime_types = get_supported_mime_types ();
	}

	quark = g_quark_from_string (mime_type);
	
	result = g_slist_find (supported_mime_types, GINT_TO_POINTER (quark));

	return (result != NULL);
}

/**
   Searchs for a file in the store. If found and @iter_found is not NULL,
   then sets @iter_found to a #GtkTreeIter pointing to the file.
 */
static gboolean
is_file_in_list_store (EogListStore *store,
		       const gchar *info_uri,
		       GtkTreeIter *iter_found)
{
	gboolean found = FALSE;
	EogImage *image;
	GnomeVFSURI *uri;
	gchar *str;
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		return FALSE;
	}
	do {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
				    EOG_LIST_STORE_EOG_IMAGE, &image,
				    -1);
		
		uri = eog_image_get_uri (image);
		str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
		found = (strcmp (str, info_uri) == 0)? TRUE : FALSE;
		g_free (str);
		g_object_unref (G_OBJECT (image));
	} while (!found && gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
	
	if (found && iter_found != NULL) {
		*iter_found = iter;
	}

	return found;
}

static void
vfs_monitor_dir_cb (GnomeVFSMonitorHandle *handle,
		    const gchar *monitor_uri,
		    const gchar *info_uri,
		    GnomeVFSMonitorEventType event_type,
		    gpointer user_data)
{
	EogListStore *store = EOG_LIST_STORE (user_data);
	GnomeVFSURI *uri = NULL;
	GtkTreeIter iter;
	gchar *mimetype;

	switch (event_type) {
	case GNOME_VFS_MONITOR_EVENT_CHANGED:
		g_print ("a file was modified\n");
		mimetype = gnome_vfs_get_mime_type (info_uri);
		if (is_file_in_list_store (store, info_uri, &iter)) {
			if (is_supported_mime_type (mimetype)) {
				/* update EogImage (easy and ugly way) */
				gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
				uri = gnome_vfs_uri_new (info_uri);
				eog_list_store_append_image_from_uri (store, uri);
				gnome_vfs_uri_unref (uri);
			} else {
				gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
			}
		} else {
			if (is_supported_mime_type (mimetype)) {
				uri = gnome_vfs_uri_new (info_uri);
				eog_list_store_append_image_from_uri (store, uri);
				gnome_vfs_uri_unref (uri);
			}
		}
		g_free (mimetype);
		break;
	case GNOME_VFS_MONITOR_EVENT_DELETED:
		g_print ("a file was deleted\n");		
		if (is_file_in_list_store (store, info_uri, &iter)) {
			gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
		}
		break;
	case GNOME_VFS_MONITOR_EVENT_CREATED:
		g_print ("a file was created\n");
		if (is_file_in_list_store (store, info_uri, NULL)) {
			uri = gnome_vfs_uri_new (info_uri);
			eog_list_store_append_image_from_uri (store, uri);
			gnome_vfs_uri_unref (uri);
		}
		break;
	case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
	case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
	case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
		break;
	}
}

/* Called for each file in a directory. Checks if the file is some
 * sort of image. If so, it creates an image object and adds it to the
 * list.
 */
static gboolean
directory_visit_cb (const gchar *rel_uri,
		    GnomeVFSFileInfo *info,
		    gboolean recursing_will_loop,
		    gpointer data,
		    gboolean *recurse)
{
	GnomeVFSURI *uri;
	EogListStore *store;
	gboolean load_uri = FALSE;
	DirLoadingContext *ctx;
	
	ctx = (DirLoadingContext*) data;
	store = ctx->store;
	
        if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) > 0 &&
            !g_str_has_prefix (info->name, ".")) {
		if (is_supported_mime_type (info->mime_type)) {
			load_uri = TRUE;
		}
	}

	if (load_uri) {
		uri = gnome_vfs_uri_append_file_name (ctx->uri, rel_uri);
		eog_list_store_append_image_from_uri (store, uri);
	}

	return TRUE;
}

static void 
eog_list_store_append_directory (EogListStore *store, 
				 GnomeVFSURI *uri, 
				 GnomeVFSFileInfo *info)
{
	DirLoadingContext ctx;
	MonitorHandleContext *hctx = g_new0(MonitorHandleContext, 1);

	hctx->text_uri = gnome_vfs_uri_get_path (uri);
	
	g_return_if_fail (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);

	ctx.uri = uri;
	ctx.store = store;
	ctx.info = info;
	
	gnome_vfs_monitor_add  (&hctx->handle, hctx->text_uri,
				GNOME_VFS_MONITOR_DIRECTORY,
				vfs_monitor_dir_cb,
				store);

	/* prepend seems more efficient to me, we don't need this list
	   to be sorted */
	store->priv->monitors = g_list_prepend (store->priv->monitors, hctx);
	
	/* Forcing slow MIME type checking, so we don't need to make 
	   workarounds for files with strange extensions (#333551) */
	gnome_vfs_directory_visit_uri (uri,
				       GNOME_VFS_FILE_INFO_DEFAULT |
				       GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE | 
				       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
				       GNOME_VFS_DIRECTORY_VISIT_DEFAULT,
				       directory_visit_cb,
				       &ctx);
	
}


/** Idea taken from EogImageList original implementation */

static gboolean
get_uri_info (GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
	GnomeVFSResult result;
	
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (info != NULL, FALSE);
	
	gnome_vfs_file_info_clear (info);
	result = gnome_vfs_get_file_info_uri (uri, info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
					      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	
	return (result == GNOME_VFS_OK);
}

void
eog_list_store_add_uris (EogListStore *store, GList *uri_list) 
{
	GList *it;
	GnomeVFSFileInfo *info;
	GnomeVFSURI *initial_uri = NULL;
	GtkTreeIter iter;

	if (uri_list == NULL) {
		return;
	}
	
	info = gnome_vfs_file_info_new ();
	g_print ("adding....\n");
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      -1, GTK_SORT_ASCENDING);
	
	for (it = uri_list; it != NULL; it = it->next) {
		GnomeVFSURI *uri = (GnomeVFSURI*) it->data;

		if (!get_uri_info (uri, info))
			continue;
			
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			eog_list_store_append_directory (store, uri, info);
		} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR && 
			   g_list_length (uri_list) == 1) {
			/* Store the URI for initial image assignment */
			initial_uri = gnome_vfs_uri_dup (uri); 
			uri = gnome_vfs_uri_get_parent (uri);
			
			if (!get_uri_info (uri, info)) {
				continue;
			}
			
			eog_list_store_append_directory (store, uri, info);
		} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR && 
			 g_list_length (uri_list) > 1) {
			eog_list_store_append_image_from_uri (store, uri);
		}
	}

	gnome_vfs_file_info_unref (info);
	g_print ("adding finished\n");

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
	
	if (initial_uri && 
	    is_file_in_list_store (store, 
				   gnome_vfs_uri_to_string (initial_uri, GNOME_VFS_URI_HIDE_NONE), 
				   &iter)) {
		store->priv->initial_image = eog_list_store_get_pos_by_iter (store, &iter);
		gnome_vfs_uri_unref (initial_uri);
	} else {
		store->priv->initial_image = 0;
	}
}

void
eog_list_store_remove_image (EogListStore *store, EogImage *image)
{
	g_return_if_fail (EOG_IS_LIST_STORE (store));
	g_return_if_fail (EOG_IS_IMAGE (image));

	GtkTreeIter iter;
	const gchar *path = gnome_vfs_uri_to_string (eog_image_get_uri (image), GNOME_VFS_URI_HIDE_NONE);
	
	if (is_file_in_list_store (store, path, &iter)) {
		gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
	}
}

GtkListStore *
eog_list_store_new_from_glist (GList *list)
{
	GList *it;

	GtkListStore *store = eog_list_store_new ();
	
	for (it = list; it != NULL; it = it->next) {
		eog_list_store_append_image (EOG_LIST_STORE (store), 
					     EOG_IMAGE (it->data));
	}
	return store;
}

gint
eog_list_store_get_pos_by_image (EogListStore *store, EogImage *image)
{
	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);
	g_return_val_if_fail (EOG_IS_IMAGE (image), -1);

	const gchar *file = gnome_vfs_uri_to_string (eog_image_get_uri (image), GNOME_VFS_URI_HIDE_NONE);
	GtkTreeIter iter;
	gint pos = -1;
	if (is_file_in_list_store (store, file, &iter)) {
		pos = eog_list_store_get_pos_by_iter (store, &iter);
	}
	return pos;
}

EogImage *
eog_list_store_get_image_by_pos (EogListStore *store, const gint pos)
{
	g_return_val_if_fail (EOG_IS_LIST_STORE (store), NULL);
	EogImage *image = NULL;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, NULL, pos)) {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 
				    EOG_LIST_STORE_EOG_IMAGE, &image,
				    -1);
	}
	return image;
}

gint
eog_list_store_get_pos_by_iter (EogListStore *store, 
				GtkTreeIter *iter)
{
	gint *indices;
	GtkTreePath *path;
	gint pos;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
	indices = gtk_tree_path_get_indices (path);
	pos = indices [0];
	gtk_tree_path_free (path);

	return pos;
}

gint
eog_list_store_get_initial_pos (EogListStore *store)
{
	return store->priv->initial_image;
}
