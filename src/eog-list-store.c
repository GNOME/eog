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

#include "eog-list-store.h"
#include "eog-thumbnail.h"
#include "eog-image.h"
#include "eog-job-queue.h"
#include "eog-jobs.h"

#include <string.h>

struct _EogListStorePriv {
	GList *monitors;      /* monitors for the directories */
	gint initial_image;   /* the image that should be selected firstly by the view. */
	GdkPixbuf *busy_image; /* hourglass image */
	GMutex *mutex;        /* mutex for saving the jobs in the model */
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

G_DEFINE_TYPE (EogListStore, eog_list_store, GTK_TYPE_LIST_STORE);

gboolean
eog_list_store_dump_contents (EogListStore *store);

static void
eog_list_store_finalize (GObject *object)
{
	EogListStore *store = EOG_LIST_STORE (object);
	
	if (store->priv != NULL) {
		g_free (store->priv);
		store->priv = NULL;
	}
	
	G_OBJECT_CLASS (eog_list_store_parent_class)->finalize (object);
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

	if(store->priv->busy_image != NULL) {
		g_object_unref (store->priv->busy_image);
		store->priv->busy_image = NULL;
	}

	g_mutex_free (store->priv->mutex);

	G_OBJECT_CLASS (eog_list_store_parent_class)->dispose (object);
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

	gtk_tree_model_get (model, a, 
			    EOG_LIST_STORE_EOG_IMAGE, &image_a,
			    -1);

	gtk_tree_model_get (model, b, 
			    EOG_LIST_STORE_EOG_IMAGE, &image_b,
			    -1);
	
	r_value = strcasecmp (eog_image_get_collate_key (image_a), 
			      eog_image_get_collate_key (image_b));

	g_object_unref (G_OBJECT (image_a));
	g_object_unref (G_OBJECT (image_b));

	return r_value;
}

static GdkPixbuf *
eog_list_store_get_loading_icon (void)
{
	GError *error = NULL;
	GtkIconTheme *icon_theme;
	GdkPixbuf *pixbuf;
	
	icon_theme = gtk_icon_theme_get_default();

	/* FIXME: The 16 added to EOG_LIST_STORE_THUMB_SIZE should be 
	   calculated from the BLUR_RADIUS and RECTANGLE_OUTLINE macros 
	   in eog-thumb-shadow.c */
	pixbuf = gtk_icon_theme_load_icon (icon_theme,
					   "image-loading", /* icon name */
					   EOG_LIST_STORE_THUMB_SIZE + 16, /* size */
					   0,  /* flags */
					   &error);

	if (!pixbuf) {
		g_warning ("Couldn't load icon: %s", error->message);
		g_error_free (error);
	}

	return pixbuf;
}

static void
eog_list_store_init (EogListStore *self)
{
	GType types[EOG_LIST_STORE_NUM_COLUMNS];

	types[EOG_LIST_STORE_THUMBNAIL] = GDK_TYPE_PIXBUF;
	types[EOG_LIST_STORE_EOG_IMAGE] = G_TYPE_OBJECT;
	types[EOG_LIST_STORE_THUMB_SET] = G_TYPE_BOOLEAN;
	types[EOG_LIST_STORE_EOG_JOB]   = G_TYPE_POINTER;

	gtk_list_store_set_column_types (GTK_LIST_STORE (self),
					 EOG_LIST_STORE_NUM_COLUMNS, types);

	self->priv = g_new0 (EogListStorePriv, 1);
	self->priv->monitors = NULL;
	self->priv->initial_image = -1;

	self->priv->busy_image = eog_list_store_get_loading_icon ();

	self->priv->mutex = g_mutex_new ();

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
		
		gnome_vfs_uri_unref (uri);
		g_free (str);
		g_object_unref (G_OBJECT (image));

	} while (!found && 
		 gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
	
	if (found && iter_found != NULL) {
		*iter_found = iter;
	}

	return found;
}

static void
eog_job_thumbnail_cb (EogJobThumbnail *job, gpointer data)
{
	EogListStore *store;
	GtkTreeIter iter;
	gchar *filename;
	EogImage *image;
	
	g_return_if_fail (EOG_IS_LIST_STORE (data));

	store = EOG_LIST_STORE (data);

/* 	thumbnail = g_object_ref (job->thumbnail); */
	
	filename = gnome_vfs_uri_to_string (job->uri_entry, GNOME_VFS_URI_HIDE_NONE);

	if (is_file_in_list_store (store, filename, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 
				    EOG_LIST_STORE_EOG_IMAGE, &image,
				    -1);

		eog_image_set_thumbnail (image, job->thumbnail);

		gtk_list_store_set (GTK_LIST_STORE (store), &iter, 
				    EOG_LIST_STORE_THUMBNAIL, job->thumbnail,
				    EOG_LIST_STORE_THUMB_SET, TRUE,
				    EOG_LIST_STORE_EOG_JOB, NULL,
				    -1);
/* 		g_object_unref (thumbnail); */
	}

	g_free (filename);
}

void
eog_list_store_append_image (EogListStore *store, EogImage *image)
{
	GtkTreeIter iter;

	gtk_list_store_append (GTK_LIST_STORE (store), &iter);

	gtk_list_store_set (GTK_LIST_STORE (store), &iter, 
			    EOG_LIST_STORE_EOG_IMAGE, image, 
			    EOG_LIST_STORE_THUMBNAIL, store->priv->busy_image,
			    EOG_LIST_STORE_THUMB_SET, FALSE,
			    -1);
}

void 
eog_list_store_append_image_from_uri (EogListStore *store, GnomeVFSURI *uri_entry)
{
	EogImage *image;
	
	g_return_if_fail (EOG_IS_LIST_STORE (store));

	image = eog_image_new_uri (uri_entry);

	eog_list_store_append_image (store, image);
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

		mimetype = gnome_vfs_get_mime_type (info_uri);
		if (is_file_in_list_store (store, info_uri, &iter)) {
			if (eog_image_is_supported_mime_type (mimetype)) {
				eog_list_store_thumbnail_unset (store, &iter);
				eog_list_store_thumbnail_set (store, &iter);
			} else {
				gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
			}
		} else {
			if (eog_image_is_supported_mime_type (mimetype)) {
				uri = gnome_vfs_uri_new (info_uri);
				eog_list_store_append_image_from_uri (store, uri);
				gnome_vfs_uri_unref (uri);
			}
		}
		g_free (mimetype);
		break;
	case GNOME_VFS_MONITOR_EVENT_DELETED:

		if (is_file_in_list_store (store, info_uri, &iter)) {
			gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
		}
		break;
	case GNOME_VFS_MONITOR_EVENT_CREATED:

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

/* 
 * Called for each file in a directory. Checks if the file is some
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
		if (eog_image_is_supported_mime_type (info->mime_type)) {
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

			if (GNOME_VFS_FILE_INFO_LOCAL(info)) {	
				/* Store the URI for initial 
				   image assignment */
				initial_uri = gnome_vfs_uri_dup (uri); 

				uri = gnome_vfs_uri_get_parent (uri);
				
				if (!get_uri_info (uri, info)) {
					continue;
				}

				eog_list_store_append_directory (store, uri, info);
			} else {
				eog_list_store_append_image_from_uri (store, uri);
			}

		} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR && 
			   g_list_length (uri_list) > 1) {
			eog_list_store_append_image_from_uri (store, uri);
		}
	}

	gnome_vfs_file_info_unref (info);

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
	GtkTreeIter iter;
	gchar *file;
	GnomeVFSURI *uri;

	g_return_if_fail (EOG_IS_LIST_STORE (store));
	g_return_if_fail (EOG_IS_IMAGE (image));

	uri = eog_image_get_uri (image);
	file = gnome_vfs_uri_to_string (uri, 
					GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (uri);
	
	if (is_file_in_list_store (store, file, &iter)) {
		gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
	}
	g_free (file);
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
	gchar *file;
	GtkTreeIter iter;
	gint pos = -1;
	GnomeVFSURI *uri;

	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);
	g_return_val_if_fail (EOG_IS_IMAGE (image), -1);
	
	uri = eog_image_get_uri (image);
	file = gnome_vfs_uri_to_string (uri, 
					GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (uri);

	if (is_file_in_list_store (store, file, &iter)) {
		pos = eog_list_store_get_pos_by_iter (store, &iter);
	}

	g_free (file);
	return pos;
}

EogImage *
eog_list_store_get_image_by_pos (EogListStore *store, const gint pos)
{
	EogImage *image = NULL;
	GtkTreeIter iter;

	g_return_val_if_fail (EOG_IS_LIST_STORE (store), NULL);

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
eog_list_store_length (EogListStore *store)
{
	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);

	return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL);
}

gint
eog_list_store_get_initial_pos (EogListStore *store)
{
	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);

	return store->priv->initial_image;
}

void
eog_list_store_thumbnail_set (EogListStore *store, 
			      GtkTreeIter *iter)
{
	EogJob *job;
	EogImage *image;
	gboolean *thumb_set;
	GnomeVFSURI *uri;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 
			    EOG_LIST_STORE_THUMB_SET, &thumb_set,
			    -1);

	if (thumb_set) {
		return;
	}
	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 
			    EOG_LIST_STORE_EOG_IMAGE, &image, 
			    -1);
	
	uri = eog_image_get_uri (image);
	job = eog_job_thumbnail_new (uri);
	gnome_vfs_uri_unref (uri);

	g_signal_connect (job,
			  "finished",
			  G_CALLBACK (eog_job_thumbnail_cb),
			  store);
	
	g_mutex_lock (store->priv->mutex);
	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    EOG_LIST_STORE_EOG_JOB, job, 
			    -1);
	eog_job_queue_add_job (job);
	g_mutex_unlock (store->priv->mutex);
	g_object_unref (job);
	g_object_unref (image);
}

void
eog_list_store_thumbnail_unset (EogListStore *store, 
				GtkTreeIter *iter)
{
	EogImage *image;
	EogJob *job;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    EOG_LIST_STORE_EOG_JOB, &job,
			    -1);

	if (job != NULL) {
		g_mutex_lock (store->priv->mutex);
		eog_job_queue_remove_job (job);
		gtk_list_store_set (GTK_LIST_STORE (store), iter,
				    EOG_LIST_STORE_EOG_JOB, NULL,
				    -1);
		g_mutex_unlock (store->priv->mutex);
	}

	eog_image_set_thumbnail (image, NULL);
	g_object_unref (image);

	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    EOG_LIST_STORE_THUMBNAIL, store->priv->busy_image,
			    EOG_LIST_STORE_THUMB_SET, FALSE,
			    -1);	
}
