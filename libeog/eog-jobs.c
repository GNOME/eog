/* Eye Of Gnome - Jobs  
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on evince code (shell/ev-jobs.c) by: 
 * 	- Martin Kretzschmar <martink@gnome.org>
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

#include "eog-jobs.h"
#include "eog-job-queue.h"
#include "eog-image.h"
#include "eog-transform.h"
#include "eog-list-store.h"
#include "eog-thumbnail.h"

#define EOG_JOB_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_JOB, EogJobPrivate))

G_DEFINE_TYPE (EogJob, eog_job, G_TYPE_OBJECT);
G_DEFINE_TYPE (EogJobThumbnail, eog_job_thumbnail, EOG_TYPE_JOB);
G_DEFINE_TYPE (EogJobLoad, eog_job_load, EOG_TYPE_JOB);
G_DEFINE_TYPE (EogJobModel, eog_job_model, EOG_TYPE_JOB);
G_DEFINE_TYPE (EogJobTransform, eog_job_transform, EOG_TYPE_JOB);

enum
{
	SIGNAL_FINISHED,
	SIGNAL_PROGRESS,
	SIGNAL_LAST_SIGNAL
};

static guint job_signals[SIGNAL_LAST_SIGNAL] = { 0 };

static void eog_job_init (EogJob *job) 
{
	job->mutex = g_mutex_new();
	job->progress = 0.0;
}

static void
eog_job_dispose (GObject *object)
{
	EogJob *job;

	job = EOG_JOB (object);

	if (job->error) {
		g_error_free (job->error);
		job->error = NULL;
	}

	if (job->mutex) {
		g_mutex_free (job->mutex);
		job->mutex = NULL;
	}

	(* G_OBJECT_CLASS (eog_job_parent_class)->dispose) (object);
}

static void
eog_job_class_init (EogJobClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = eog_job_dispose;

	job_signals [SIGNAL_FINISHED] =
		g_signal_new ("finished",
			      EOG_TYPE_JOB,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogJobClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	job_signals [SIGNAL_PROGRESS] =
		g_signal_new ("progress",
			      EOG_TYPE_JOB,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogJobClass, progress),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE, 1,
			      G_TYPE_FLOAT);
}

void
eog_job_finished (EogJob *job)
{
	g_return_if_fail (EOG_IS_JOB (job));

	g_signal_emit (job, job_signals[SIGNAL_FINISHED], 0);
}

static gboolean
notify_progress (gpointer data)
{
	EogJob *job = EOG_JOB (data);

	g_signal_emit (job, job_signals[SIGNAL_PROGRESS], 0, job->progress);

	return FALSE;
}

void
eog_job_set_progress (EogJob *job, float progress)
{
	g_return_if_fail (EOG_IS_JOB (job));
	g_return_if_fail (progress >= 0.0 && progress <= 1.0);

	g_mutex_lock (job->mutex);
	job->progress = progress;
	g_mutex_unlock (job->mutex);

	g_idle_add (notify_progress, job);
}

static void eog_job_thumbnail_init (EogJobThumbnail *job) { /* Do Nothing */ }

static void
eog_job_thumbnail_dispose (GObject *object)
{
	EogJobThumbnail *job;

	job = EOG_JOB_THUMBNAIL (object);

	if (job->uri_entry) {
		gnome_vfs_uri_unref (job->uri_entry);
		job->uri_entry = NULL;
	}
	
	if (job->thumbnail) {
		g_object_unref (job->thumbnail);
		job->thumbnail = NULL;
	}

	(* G_OBJECT_CLASS (eog_job_thumbnail_parent_class)->dispose) (object);
}

static void
eog_job_thumbnail_class_init (EogJobThumbnailClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = eog_job_thumbnail_dispose;
}

static void eog_job_load_init (EogJobLoad *job) { /* Do Nothing */ }

static void
eog_job_load_dispose (GObject *object)
{
	EogJobLoad *job;

	job = EOG_JOB_LOAD (object);

	if (job->image) {
		g_object_unref (job->image);
		job->image = NULL;
	}

	(* G_OBJECT_CLASS (eog_job_load_parent_class)->dispose) (object);
}

static void
eog_job_load_class_init (EogJobLoadClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = eog_job_load_dispose;
}

static void eog_job_model_init (EogJobModel *job) { /* Do Nothing */ }

static void
eog_job_model_dispose (GObject *object)
{
	EogJobModel *job;

	job = EOG_JOB_MODEL (object);

	(* G_OBJECT_CLASS (eog_job_model_parent_class)->dispose) (object);
}

static void
eog_job_model_class_init (EogJobModelClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = eog_job_model_dispose;
}

static void eog_job_transform_init (EogJobTransform *job) { /* Do Nothing */ }

static void
eog_job_transform_dispose (GObject *object)
{
	EogJobTransform *job;

	job = EOG_JOB_TRANSFORM (object);

	if (job->trans) {
		g_object_unref (job->trans);
		job->trans = NULL;
	}

	g_list_foreach (job->images, (GFunc) g_object_unref, NULL);
	g_list_free (job->images);

	(* G_OBJECT_CLASS (eog_job_transform_parent_class)->dispose) (object);
}

static void
eog_job_transform_class_init (EogJobTransformClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = eog_job_transform_dispose;
}

EogJob *
eog_job_thumbnail_new (GnomeVFSURI *uri_entry)
{
	EogJobThumbnail *job;

	job = g_object_new (EOG_TYPE_JOB_THUMBNAIL, NULL);

	if (uri_entry) {
		gnome_vfs_uri_ref (uri_entry);
		job->uri_entry = uri_entry;
	}

	return EOG_JOB (job);
}

void
eog_job_thumbnail_run (EogJobThumbnail *job)
{
	g_return_if_fail (EOG_IS_JOB_THUMBNAIL (job));
	
	if (EOG_JOB (job)->error) {
	        g_error_free (EOG_JOB (job)->error);
		EOG_JOB (job)->error = NULL;
	}

	job->thumbnail = eog_thumbnail_load (job->uri_entry,
					     &EOG_JOB (job)->error);

	if (EOG_JOB (job)->error) {
		g_warning ("%s\n", EOG_JOB (job)->error->message);
	}

	EOG_JOB (job)->finished = TRUE;
}

EogJob *
eog_job_load_new (EogImage *image)
{
	EogJobLoad *job;

	job = g_object_new (EOG_TYPE_JOB_LOAD, NULL);

	if (image) {
		job->image = g_object_ref (image);
	}
	
	return EOG_JOB (job);
}

void
eog_job_load_run (EogJobLoad *job)
{
	g_return_if_fail (EOG_IS_JOB_LOAD (job));

	if (EOG_JOB (job)->error) {
	        g_error_free (EOG_JOB (job)->error);
		EOG_JOB (job)->error = NULL;
	}

	eog_image_load (EOG_IMAGE (job->image),
			EOG_IMAGE_DATA_ALL,
			EOG_JOB (job),
			&EOG_JOB (job)->error);
	
	EOG_JOB (job)->finished = TRUE;
}

EogJob *
eog_job_model_new (GSList *uri_list)
{
	EogJobModel *job;

	job = g_object_new (EOG_TYPE_JOB_MODEL, NULL);

	job->uri_list = uri_list;

	return EOG_JOB (job);
}

static GnomeVFSFileType
check_uri_file_type (GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
	GnomeVFSResult result;
	GnomeVFSFileType type = GNOME_VFS_FILE_TYPE_UNKNOWN;

	g_return_val_if_fail (uri != NULL, GNOME_VFS_FILE_TYPE_UNKNOWN);
	g_return_val_if_fail (info != NULL, GNOME_VFS_FILE_TYPE_UNKNOWN);

	gnome_vfs_file_info_clear (info);
	
	result = gnome_vfs_get_file_info_uri (uri, info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	
	if (result == GNOME_VFS_OK &&
	    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) != 0) {
		type = info->type;
	}

	return type;
}

static void
filter_files (GSList *files, GList **file_list, GList **error_list)
{
	GSList *it;
	GnomeVFSFileInfo *info;
	
	info = gnome_vfs_file_info_new ();

	for (it = files; it != NULL; it = it->next) {
		GnomeVFSURI *uri;
		GnomeVFSFileType type = GNOME_VFS_FILE_TYPE_UNKNOWN;

		uri = (GnomeVFSURI *) it->data;

		if (uri != NULL) {
			type = check_uri_file_type (uri, info);
		}

		switch (type) {
		case GNOME_VFS_FILE_TYPE_REGULAR:
		case GNOME_VFS_FILE_TYPE_DIRECTORY:
			*file_list = g_list_prepend (*file_list, gnome_vfs_uri_ref (uri));
			break;
		default:
			*error_list = g_list_prepend (*error_list, 
						      gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
			break;
		}

		gnome_vfs_uri_unref (uri);
	}

	*file_list  = g_list_reverse (*file_list);
	*error_list = g_list_reverse (*error_list);

	gnome_vfs_file_info_unref (info);
}

void
eog_job_model_run (EogJobModel *job)
{
	GList *file_list = NULL;
	GList *error_list = NULL;

	g_return_if_fail (EOG_IS_JOB_MODEL (job));

	filter_files (job->uri_list, &file_list, &error_list);

	job->store = EOG_LIST_STORE (eog_list_store_new ());
	
	eog_list_store_add_uris (job->store, file_list);

	EOG_JOB (job)->finished = TRUE;
}

EogJob *
eog_job_transform_new (GList *images, EogTransform *trans)
{
	EogJobTransform *job;

	job = g_object_new (EOG_TYPE_JOB_TRANSFORM, NULL);

	if (trans) {
		job->trans = g_object_ref (trans);
	} else {
		job->trans = NULL;
	}

	job->images = images;

	return EOG_JOB (job);
}

static gboolean
eog_job_transform_image_modified (gpointer data)
{
	g_return_val_if_fail (EOG_IS_IMAGE (data), FALSE);

	eog_image_modified (EOG_IMAGE (data));
	g_object_unref (G_OBJECT (data));

	return FALSE;
}

void
eog_job_transform_run (EogJobTransform *job)
{
	GList *it;

	g_return_if_fail (EOG_IS_JOB_TRANSFORM (job));

	if (EOG_JOB (job)->error) {
	        g_error_free (EOG_JOB (job)->error);
		EOG_JOB (job)->error = NULL;
	}

	for (it = job->images; it != NULL; it = it->next) {
		EogImage *image = EOG_IMAGE (it->data);
		
		if (job->trans == NULL) {
			eog_image_undo (image);
		} else {
			eog_image_transform (image, job->trans);
		}
		
		if (eog_image_is_modified (image) || job->trans == NULL) {
			g_object_ref (image);
			g_idle_add (eog_job_transform_image_modified, image);
		}
	}
	
	EOG_JOB (job)->finished = TRUE;
}

