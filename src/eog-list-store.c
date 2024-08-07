/* Eye Of Gnome - Image Store
 *
 * Copyright (C) 2006-2008 The Free Software Foundation
 *
 * Author: Claudio Saavedra <csaavedra@gnome.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "eog-list-store.h"
#include "eog-thumbnail.h"
#include "eog-image.h"
#include "eog-job-scheduler.h"
#include "eog-jobs.h"
#include "eog-util.h"

#include <string.h>

struct _EogListStorePrivate {
	GHashTable *monitors;          /* Monitors for the directories */
	gint initial_image;       /* The image that should be selected firstly by the view. */
	GdkPixbuf *busy_image;    /* Loading image icon */
	GdkPixbuf *missing_image; /* Missing image icon */
	GMutex mutex;             /* Mutex for saving the jobs in the model */
};

G_DEFINE_TYPE_WITH_PRIVATE (EogListStore, eog_list_store, GTK_TYPE_LIST_STORE);

enum {
	SIGNAL_DRAW_THUMBNAIL,
	SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

static void
foreach_monitors_free (gpointer data)
{
	g_file_monitor_cancel (G_FILE_MONITOR (data));
}

static void
eog_list_store_remove_thumbnail_job (EogListStore *store, GtkTreeIter *iter);

static gboolean
foreach_model_cancel_job (GtkTreeModel *model, GtkTreePath *path,
			  GtkTreeIter *iter, gpointer data)
{
	eog_list_store_remove_thumbnail_job (EOG_LIST_STORE (model), iter);
	return FALSE;
}

static void
eog_list_store_dispose (GObject *object)
{
	EogListStore *store = EOG_LIST_STORE (object);

	gtk_tree_model_foreach (GTK_TREE_MODEL (store),
				foreach_model_cancel_job, NULL);

	if (store->priv->monitors != NULL) {
		g_hash_table_unref (store->priv->monitors);
		store->priv->monitors = NULL;
	}

	if(store->priv->busy_image != NULL) {
		g_object_unref (store->priv->busy_image);
		store->priv->busy_image = NULL;
	}

	if(store->priv->missing_image != NULL) {
		g_object_unref (store->priv->missing_image);
		store->priv->missing_image = NULL;
	}

	G_OBJECT_CLASS (eog_list_store_parent_class)->dispose (object);
}

static void
eog_list_store_finalize (GObject *object)
{
	EogListStore *store = EOG_LIST_STORE (object);

	g_mutex_clear (&store->priv->mutex);

	G_OBJECT_CLASS (eog_list_store_parent_class)->finalize (object);
}

static void
eog_list_store_class_init (EogListStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = eog_list_store_dispose;
	object_class->finalize = eog_list_store_finalize;

	signals[SIGNAL_DRAW_THUMBNAIL] =
		g_signal_new ("draw-thumbnail",
			      EOG_TYPE_LIST_STORE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogListStoreClass, draw_thumbnail),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
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

	r_value = strcmp (eog_image_get_collate_key (image_a),
			  eog_image_get_collate_key (image_b));

	g_object_unref (G_OBJECT (image_a));
	g_object_unref (G_OBJECT (image_b));

	return r_value;
}

static GdkPixbuf *
eog_list_store_get_icon (const gchar *icon_name)
{
	GError *error = NULL;
	GtkIconTheme *icon_theme;
	GdkPixbuf *pixbuf;

	icon_theme = gtk_icon_theme_get_default ();

	pixbuf = gtk_icon_theme_load_icon (icon_theme,
					   icon_name,
					   EOG_LIST_STORE_THUMB_SIZE,
					   0,
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

	self->priv = eog_list_store_get_instance_private (self);

	self->priv->monitors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, foreach_monitors_free);
	self->priv->initial_image = -1;

	self->priv->busy_image = eog_list_store_get_icon ("image-loading");
	self->priv->missing_image = eog_list_store_get_icon ("image-missing");

	g_mutex_init (&self->priv->mutex);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (self),
						 eog_list_store_compare_func,
						 NULL, NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
}

/**
 * eog_list_store_new:
 *
 * Creates a new and empty #EogListStore.
 *
 * Returns: a newly created #EogListStore.
 **/
GtkListStore*
eog_list_store_new (void)
{
        return g_object_new (EOG_TYPE_LIST_STORE, NULL);
}

/*
   Searches for a file in the store. If found and @iter_found is not NULL,
   then sets @iter_found to a #GtkTreeIter pointing to the file.
 */
static gboolean
is_file_in_list_store (EogListStore *store,
		       const gchar *info_uri,
		       GtkTreeIter *iter_found)
{
	gboolean found = FALSE;
	EogImage *image;
	GFile *file;
	gchar *str;
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		return FALSE;
	}

	do {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
				    EOG_LIST_STORE_EOG_IMAGE, &image,
				    -1);
		if (!image)
			continue;

		file = eog_image_get_file (image);
		str = g_file_get_uri (file);

		found = (strcmp (str, info_uri) == 0)? TRUE : FALSE;

		g_object_unref (file);
		g_free (str);
		g_object_unref (G_OBJECT (image));

	} while (!found &&
		 gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

	if (found && iter_found != NULL) {
		*iter_found = iter;
	}

	return found;
}

static gboolean
is_file_in_list_store_file (EogListStore *store,
			   GFile *file,
			   GtkTreeIter *iter_found)
{
	gchar *uri_str;
	gboolean result;

	uri_str = g_file_get_uri (file);

	result = is_file_in_list_store (store, uri_str, iter_found);

	g_free (uri_str);

	return result;
}

static void
eog_job_thumbnail_cb (EogJobThumbnail *job, gpointer data)
{
	EogListStore *store;
	GtkTreeIter iter;
	EogImage *image;
	GdkPixbuf *thumbnail;
	GFile *file;

	g_return_if_fail (EOG_IS_LIST_STORE (data));

	store = EOG_LIST_STORE (data);

	file = eog_image_get_file (job->image);

	if (is_file_in_list_store_file (store, file, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
				    EOG_LIST_STORE_EOG_IMAGE, &image,
				    -1);

		if (job->thumbnail) {
			eog_image_set_thumbnail (image, job->thumbnail);

			/* Getting the thumbnail, in case it needed
 			 * transformations */
			thumbnail = eog_image_get_thumbnail (image);
		} else {
			thumbnail = g_object_ref (store->priv->missing_image);
		}

		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    EOG_LIST_STORE_THUMBNAIL, thumbnail,
				    EOG_LIST_STORE_THUMB_SET, TRUE,
				    EOG_LIST_STORE_EOG_JOB, NULL,
				    -1);

		g_object_unref (image);
		g_object_unref (thumbnail);
	}

	g_object_unref (file);

	g_signal_emit (store, signals[SIGNAL_DRAW_THUMBNAIL], 0);
}

static void
on_image_changed (EogImage *image, EogListStore *store)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gint pos;

	pos = eog_list_store_get_pos_by_image (store, image);
	path = gtk_tree_path_new_from_indices (pos, -1);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	eog_list_store_thumbnail_refresh (store, &iter);
	gtk_tree_path_free (path);
}

/**
 * eog_list_store_remove:
 * @store: An #EogListStore.
 * @iter: A #GtkTreeIter.
 *
 * Removes the image pointed by @iter from @store.
 **/
static void
eog_list_store_remove (EogListStore *store, GtkTreeIter *iter)
{
	EogImage *image;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);

	g_signal_handlers_disconnect_by_func (image, on_image_changed, store);
	g_object_unref (image);

	gtk_list_store_remove (GTK_LIST_STORE (store), iter);
}

static void
eog_list_store_remove_directory (EogListStore *store, gchar *directory)
{
	GList *refs = NULL;
	GList *node;

	GtkTreeIter iter;
	EogImage *image;
	GFile *file;

	GFile *dir_file = g_file_new_for_uri (directory);

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
		return;

	do {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, EOG_LIST_STORE_EOG_IMAGE, &image, -1);
		if (!image)
			continue;

		file = eog_image_get_file (image);
		if (g_file_has_parent (file, dir_file)) {
			GtkTreeRowReference  *rowref;
			GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
			rowref = gtk_tree_row_reference_new(GTK_TREE_MODEL (store), path);
			refs = g_list_prepend (refs, rowref);
			gtk_tree_path_free (path);
		}
		g_object_unref (file);
		g_object_unref (image);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

	for (node = refs;  node != NULL;  node = node->next) {
		GtkTreePath *path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);

		if (path) {
			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
				eog_list_store_remove (store, &iter);
			gtk_tree_path_free (path);
		}
	}

	g_list_foreach(refs, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free(refs);
	g_object_unref (dir_file);
}

/**
 * eog_list_store_append_image:
 * @store: An #EogListStore.
 * @image: An #EogImage.
 *
 * Adds an #EogImage to @store. The thumbnail of the image is not
 * loaded and will only be loaded if the thumbnail is made visible.
 *
 **/
void
eog_list_store_append_image (EogListStore *store, EogImage *image)
{
	GtkTreeIter iter;

	g_signal_connect (image, "changed",
 			  G_CALLBACK (on_image_changed),
 			  store);

	gtk_list_store_insert_with_values (GTK_LIST_STORE (store), &iter, -1,
			    EOG_LIST_STORE_EOG_IMAGE, image,
			    EOG_LIST_STORE_THUMBNAIL, store->priv->busy_image,
			    EOG_LIST_STORE_THUMB_SET, FALSE,
			    -1);
}

static void
eog_list_store_append_image_from_file (EogListStore *store,
				       GFile *file,
				       const gchar *caption)
{
	EogImage *image;

	g_return_if_fail (EOG_IS_LIST_STORE (store));

	image = eog_image_new_file (file, caption);

	eog_list_store_append_image (store, image);

	g_object_unref (image);
}

static void
file_monitor_changed_cb (GFileMonitor *monitor,
			 GFile *file,
			 GFile *other_file,
			 GFileMonitorEvent event,
			 EogListStore *store)
{
	const char *mimetype;
	GFileInfo *file_info;
	GtkTreeIter iter;
	EogImage *image;

	switch (event) {
	case G_FILE_MONITOR_EVENT_MOVED_IN:
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE ","
					       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					       0, NULL, NULL);
		if (file_info == NULL) {
			break;
		}
		mimetype = eog_util_get_content_type_with_fallback (file_info);

		if (is_file_in_list_store_file (store, file, &iter)) {
			if (eog_image_is_supported_mime_type (mimetype)) {
				gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
						    EOG_LIST_STORE_EOG_IMAGE, &image,
						    -1);
				eog_image_file_changed (image);
				g_object_unref (image);
				eog_list_store_thumbnail_refresh (store, &iter);
			} else {
				eog_list_store_remove (store, &iter);
			}
		} else {
			if (eog_image_is_supported_mime_type (mimetype)) {
				const gchar *caption;

				caption = g_file_info_get_display_name (file_info);
				eog_list_store_append_image_from_file (store, file, caption);
			}
		}
		g_object_unref (file_info);
		break;
	case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
	case G_FILE_MONITOR_EVENT_UNMOUNTED:
	case G_FILE_MONITOR_EVENT_MOVED_OUT:
	case G_FILE_MONITOR_EVENT_DELETED:
		if (is_file_in_list_store_file (store, file, &iter)) {
			eog_list_store_remove (store, &iter);
		} else {
			gchar *directory = g_file_get_uri (file);
			if (g_hash_table_contains(store->priv->monitors, directory)) {
				gint num_directories = g_hash_table_size (store->priv->monitors);
				if (num_directories > 1)
					eog_list_store_remove_directory (store, directory);
				else {
					gtk_list_store_clear (GTK_LIST_STORE (store));
				}
				g_hash_table_remove(store->priv->monitors, directory);
			}
			g_free (directory);
		}
		break;
	case G_FILE_MONITOR_EVENT_CREATED:
		if (!is_file_in_list_store_file (store, file, NULL)) {
			file_info = g_file_query_info (file,
						       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
						       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE ","
						       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
						       0, NULL, NULL);
			if (file_info == NULL) {
				break;
			}
			mimetype = eog_util_get_content_type_with_fallback (file_info);

			if (eog_image_is_supported_mime_type (mimetype)) {
				const gchar *caption;

				caption = g_file_info_get_display_name (file_info);
				eog_list_store_append_image_from_file (store, file, caption);
			}
			g_object_unref (file_info);
		}
		break;
	case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
					       0, NULL, NULL);
		if (file_info == NULL) {
			break;
		}
		mimetype = eog_util_get_content_type_with_fallback (file_info);
		if (is_file_in_list_store_file (store, file, &iter) &&
		    eog_image_is_supported_mime_type (mimetype)) {
			eog_list_store_thumbnail_refresh (store, &iter);
		}
		g_object_unref (file_info);
		break;
	case G_FILE_MONITOR_EVENT_RENAMED:
		file_info = g_file_query_info (other_file,
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE ","
					       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					       0, NULL, NULL);
		if (file_info == NULL) {
			break;
		}
		mimetype = eog_util_get_content_type_with_fallback (file_info);

		if (is_file_in_list_store_file (store, other_file, &iter)) {
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
					    EOG_LIST_STORE_EOG_IMAGE, &image,
					    -1);
			eog_image_file_changed (image);
			g_object_unref (image);
			eog_list_store_thumbnail_refresh (store, &iter);
		} else if (eog_image_is_supported_mime_type (mimetype)) {
			const gchar *caption;
			caption = g_file_info_get_display_name (file_info);
			eog_list_store_append_image_from_file (store, other_file, caption);
		}

		if (is_file_in_list_store_file (store, file, &iter)) {
			eog_list_store_remove (store, &iter);
		}

		g_object_unref (file_info);
		break;
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_MOVED:
		break;
	}
}

/*
 * Called for each file in a directory. Checks if the file is some
 * sort of image. If so, it creates an image object and adds it to the
 * list.
 */
static void
directory_visit (GFile *directory,
		 GFileInfo *children_info,
		 EogListStore *store)
{
	GFile *child;
	gboolean load_uri = FALSE;
	const char *mime_type, *name;

	mime_type = eog_util_get_content_type_with_fallback (children_info);
	name = g_file_info_get_name (children_info);

        if (!g_str_has_prefix (name, ".")) {
		if (eog_image_is_supported_mime_type (mime_type)) {
			load_uri = TRUE;
		}
	}

	if (load_uri) {
		const gchar *caption;

		child = g_file_get_child (directory, name);
		caption = g_file_info_get_display_name (children_info);
		eog_list_store_append_image_from_file (store, child, caption);
		g_object_unref(child);
	}
}

static void
eog_list_store_append_directory (EogListStore *store,
				 GFile *file,
				 GFileType file_type)
{
	GFileMonitor *file_monitor;
	GFileEnumerator *file_enumerator;
	GFileInfo *file_info;

	g_return_if_fail (file_type == G_FILE_TYPE_DIRECTORY);

	file_monitor = g_file_monitor_directory (file,
						 G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);

	if (file_monitor != NULL) {
		g_signal_connect (file_monitor, "changed",
				  G_CALLBACK (file_monitor_changed_cb), store);

		g_hash_table_insert(store->priv->monitors, g_file_get_uri (file), file_monitor);
	}

	file_enumerator = g_file_enumerate_children (file,
						     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
						     G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE ","
						     G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
						     G_FILE_ATTRIBUTE_STANDARD_NAME,
						     0, NULL, NULL);
	file_info = g_file_enumerator_next_file (file_enumerator, NULL, NULL);

	while (file_info != NULL)
	{
		directory_visit (file, file_info, store);
		g_object_unref (file_info);
		file_info = g_file_enumerator_next_file (file_enumerator, NULL, NULL);
	}
	g_object_unref (file_enumerator);
}

/**
 * eog_list_store_add_files:
 * @store: An #EogListStore.
 * @file_list: (element-type GFile): A %NULL-terminated list of #GFile's.
 *
 * Adds a list of #GFile's to @store. The given list
 * must be %NULL-terminated.
 *
 * If any of the #GFile's in @file_list is a directory, all the images
 * in that directory will be added to @store. If the list of files contains
 * only one file and this is a regular file, then all the images in the same
 * directory will be added as well to @store.
 *
 **/
void
eog_list_store_add_files (EogListStore *store, GList *file_list)
{
	GList *it;
	GFileInfo *file_info;
	GFileType file_type;
	GFile *initial_file = NULL;
	GtkTreeIter iter;

	if (file_list == NULL) {
		return;
	}

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	for (it = file_list; it != NULL; it = it->next) {
		GFile *file = (GFile *) it->data;
		gchar *caption = NULL;

		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_STANDARD_TYPE","
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
					       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE","
					       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					       0, NULL, NULL);
		if (file_info == NULL) {
			continue;
		}

		caption = g_strdup (g_file_info_get_display_name (file_info));
		file_type = g_file_info_get_file_type (file_info);

		/* Workaround for gvfs backends that don't set the GFileType. */
		if (G_UNLIKELY (file_type == G_FILE_TYPE_UNKNOWN)) {
			const gchar *ctype;

			ctype = eog_util_get_content_type_with_fallback (file_info);

			/* If the content type is supported adjust file_type */
			if (eog_image_is_supported_mime_type (ctype))
				file_type = G_FILE_TYPE_REGULAR;
		}

		g_object_unref (file_info);

		if (file_type == G_FILE_TYPE_DIRECTORY) {
			eog_list_store_append_directory (store, file, file_type);
		} else if (file_type == G_FILE_TYPE_REGULAR &&
			   g_list_length (file_list) == 1) {

			initial_file = g_file_dup (file);

			file = g_file_get_parent (file);
			file_info = g_file_query_info (file,
						       G_FILE_ATTRIBUTE_STANDARD_TYPE,
						       0, NULL, NULL);

			/* If we can't get a file_info,
			   file_type will stay as G_FILE_TYPE_REGULAR */
			if (file_info != NULL) {
				file_type = g_file_info_get_file_type (file_info);
				g_object_unref (file_info);
			}

			if (file_type == G_FILE_TYPE_DIRECTORY) {
				eog_list_store_append_directory (store, file, file_type);

				if (!is_file_in_list_store_file (store,
								 initial_file,
								 &iter)) {
					eog_list_store_append_image_from_file (store, initial_file, caption);
				}
			} else {
				eog_list_store_append_image_from_file (store, initial_file, caption);
			}
			g_object_unref (file);
		} else if (file_type == G_FILE_TYPE_REGULAR &&
			   g_list_length (file_list) > 1) {
			eog_list_store_append_image_from_file (store, file, caption);
		}

		g_free (caption);
	}

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	if (initial_file &&
	    is_file_in_list_store_file (store, initial_file, &iter)) {
		store->priv->initial_image = eog_list_store_get_pos_by_iter (store, &iter);
		g_object_unref (initial_file);
	} else {
		store->priv->initial_image = 0;
	}
}

/**
 * eog_list_store_remove_image:
 * @store: An #EogListStore.
 * @image: An #EogImage.
 *
 * Removes @image from @store.
 **/
void
eog_list_store_remove_image (EogListStore *store, EogImage *image)
{
	GtkTreeIter iter;
	GFile *file;

	g_return_if_fail (EOG_IS_LIST_STORE (store));
	g_return_if_fail (EOG_IS_IMAGE (image));

	file = eog_image_get_file (image);

	if (is_file_in_list_store_file (store, file, &iter)) {
		eog_list_store_remove (store, &iter);
	}
	g_object_unref (file);
}

/**
 * eog_list_store_new_from_glist:
 * @list: (element-type EogImage): a %NULL-terminated list of #EogImage's.
 *
 * Creates a new #EogListStore from a list of #EogImage's.
 * The given list must be %NULL-terminated.
 *
 * Returns: a new #EogListStore.
 **/
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

/**
 * eog_list_store_get_pos_by_image:
 * @store: An #EogListStore.
 * @image: An #EogImage.
 *
 * Gets the position where @image is stored in @store. If @image
 * is not stored in @store, -1 is returned.
 *
 * Returns: the position of @image in @store or -1 if not found.
 **/
gint
eog_list_store_get_pos_by_image (EogListStore *store, EogImage *image)
{
	GtkTreeIter iter;
	gint pos = -1;
	GFile *file;

	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);
	g_return_val_if_fail (EOG_IS_IMAGE (image), -1);

	file = eog_image_get_file (image);

	if (is_file_in_list_store_file (store, file, &iter)) {
		pos = eog_list_store_get_pos_by_iter (store, &iter);
	}

	g_object_unref (file);
	return pos;
}

/**
 * eog_list_store_get_image_by_pos:
 * @store: An #EogListStore.
 * @pos: the position of the required #EogImage.
 *
 * Gets the #EogImage in the position @pos of @store. If there is
 * no image at position @pos, %NULL is returned.
 *
 * Returns: (transfer full): the #EogImage in position @pos or %NULL.
 *
 **/
EogImage *
eog_list_store_get_image_by_pos (EogListStore *store, gint pos)
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

/**
 * eog_list_store_get_pos_by_iter:
 * @store: An #EogListStore.
 * @iter: A #GtkTreeIter pointing to an image in @store.
 *
 * Gets the position of the image pointed by @iter.
 *
 * Returns: The position of the image pointed by @iter.
 **/
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

/**
 * eog_list_store_length:
 * @store: An #EogListStore.
 *
 * Returns the number of images in the store.
 *
 * Returns: The number of images in @store.
 **/
gint
eog_list_store_length (EogListStore *store)
{
	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);

	return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL);
}

/**
 * eog_list_store_get_initial_pos:
 * @store: An #EogListStore.
 *
 * Gets the position of the #EogImage that should be loaded first.
 * If not set, it returns -1.
 *
 * Returns: the position of the image to be loaded first or -1.
 *
 **/
gint
eog_list_store_get_initial_pos (EogListStore *store)
{
	g_return_val_if_fail (EOG_IS_LIST_STORE (store), -1);

	return store->priv->initial_image;
}

static void
eog_list_store_remove_thumbnail_job (EogListStore *store,
				     GtkTreeIter *iter)
{
	EogJob *job;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    EOG_LIST_STORE_EOG_JOB, &job,
			    -1);

	if (job != NULL) {
		g_mutex_lock (&store->priv->mutex);
		eog_job_cancel (job);
		gtk_list_store_set (GTK_LIST_STORE (store), iter,
				    EOG_LIST_STORE_EOG_JOB, NULL,
				    -1);
		g_mutex_unlock (&store->priv->mutex);
	}


}

static void
eog_list_store_add_thumbnail_job (EogListStore *store, GtkTreeIter *iter)
{
	EogImage *image;
	EogJob *job;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    EOG_LIST_STORE_EOG_JOB, &job,
			    -1);

	if (job != NULL) {
		g_object_unref (image);
		return;
	}

	job = eog_job_thumbnail_new (image);

	g_signal_connect (job,
			  "finished",
			  G_CALLBACK (eog_job_thumbnail_cb),
			  store);

	g_mutex_lock (&store->priv->mutex);
	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    EOG_LIST_STORE_EOG_JOB, job,
			    -1);
	eog_job_scheduler_add_job (job);
	g_mutex_unlock (&store->priv->mutex);
	g_object_unref (job);
	g_object_unref (image);
}

/**
 * eog_list_store_thumbnail_set:
 * @store: An #EogListStore.
 * @iter: A #GtkTreeIter pointing to an image in @store.
 *
 * Sets the thumbnail for the image pointed by @iter.
 *
 **/
void
eog_list_store_thumbnail_set (EogListStore *store,
			      GtkTreeIter *iter)
{
	gboolean thumb_set = FALSE;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    EOG_LIST_STORE_THUMB_SET, &thumb_set,
			    -1);

	if (thumb_set) {
		return;
	}

	eog_list_store_add_thumbnail_job (store, iter);
}

/**
 * eog_list_store_thumbnail_unset:
 * @store: An #EogListStore.
 * @iter: A #GtkTreeIter pointing to an image in @store.
 *
 * Unsets the thumbnail for the image pointed by @iter, changing
 * it to a "busy" icon.
 *
 **/
void
eog_list_store_thumbnail_unset (EogListStore *store,
				GtkTreeIter *iter)
{
	EogImage *image;

	eog_list_store_remove_thumbnail_job (store, iter);

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);
	eog_image_set_thumbnail (image, NULL);
	g_object_unref (image);

	gtk_list_store_set (GTK_LIST_STORE (store), iter,
			    EOG_LIST_STORE_THUMBNAIL, store->priv->busy_image,
			    EOG_LIST_STORE_THUMB_SET, FALSE,
			    -1);
}

/**
 * eog_list_store_thumbnail_refresh:
 * @store: An #EogListStore.
 * @iter: A #GtkTreeIter pointing to an image in @store.
 *
 * Refreshes the thumbnail for the image pointed by @iter.
 *
 **/
void
eog_list_store_thumbnail_refresh (EogListStore *store,
				  GtkTreeIter *iter)
{
	eog_list_store_remove_thumbnail_job (store, iter);
	eog_list_store_add_thumbnail_job (store, iter);
}
