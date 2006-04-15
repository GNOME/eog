/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
 *
 * Eogince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Eogince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EOG_JOBS_H__
#define __EOG_JOBS_H__

#include "eog-image.h"
#include "eog-list-store.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct _EogJob EogJob;
typedef struct _EogJobClass EogJobClass;

typedef struct _EogJobThumbnail EogJobThumbnail;
typedef struct _EogJobThumbnailClass EogJobThumbnailClass;

typedef struct _EogJobLoad EogJobLoad;
typedef struct _EogJobLoadClass EogJobLoadClass;

typedef struct _EogJobModel EogJobModel;
typedef struct _EogJobModelClass EogJobModelClass;

typedef struct _EogJobTransform EogJobTransform;
typedef struct _EogJobTransformClass EogJobTransformClass;

#define EOG_TYPE_JOB		       (eog_job_get_type())
#define EOG_JOB(obj)		       (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_JOB, EogJob))
#define EOG_JOB_CLASS(klass)	       (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_JOB, EogJobClass))
#define EOG_IS_JOB(obj)	               (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_JOB))

#define EOG_TYPE_JOB_THUMBNAIL	       (eog_job_thumbnail_get_type())
#define EOG_JOB_THUMBNAIL(obj)	       (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_JOB_THUMBNAIL, EogJobThumbnail))
#define EOG_JOB_THUMBNAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_JOB_THUMBNAIL, EogJobThumbnailClass))
#define EOG_IS_JOB_THUMBNAIL(obj)      (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_JOB_THUMBNAIL))

#define EOG_TYPE_JOB_LOAD	       (eog_job_load_get_type())
#define EOG_JOB_LOAD(obj)	       (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_JOB_LOAD, EogJobLoad))
#define EOG_JOB_LOAD_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_JOB_LOAD, EogJobLoadClass))
#define EOG_IS_JOB_LOAD(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_JOB_LOAD))

#define EOG_TYPE_JOB_MODEL	       (eog_job_model_get_type())
#define EOG_JOB_MODEL(obj)	       (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_JOB_MODEL, EogJobModel))
#define EOG_JOB_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_JOB_MODEL, EogJobModelClass))
#define EOG_IS_JOB_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_JOB_MODEL))

#define EOG_TYPE_JOB_TRANSFORM	       (eog_job_transform_get_type())
#define EOG_JOB_TRANSFORM(obj)	       (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_JOB_TRANSFORM, EogJobTransform))
#define EOG_JOB_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_JOB_TRANSFORM, EogJobTransformClass))
#define EOG_IS_JOB_TRANSFORM(obj)      (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_JOB_TRANSFORM))

struct _EogJob
{
	GObject  parent;

	GError   *error;
	gboolean finished;
};

struct _EogJobClass
{
	GObjectClass parent_class;

	void    (* finished) (EogJob *job);
};

struct _EogJobThumbnail
{
	EogJob   parent;
	EogImage *image;
};

struct _EogJobThumbnailClass
{
	EogJobClass parent_class;
};

struct _EogJobLoad
{
	EogJob   parent;
	EogImage *image;
};

struct _EogJobLoadClass
{
	EogJobClass parent_class;
};

struct _EogJobModel
{
	EogJob       parent;
	EogListStore *store;
	GSList       *uri_list;
};

struct _EogJobModelClass
{
        EogJobClass parent_class;
};

struct _EogJobTransform
{
	EogJob       parent;
	GList        *images;
	EogTransform *trans;
};

struct _EogJobTransformClass
{
        EogJobClass parent_class;
};

/* Base job class */
GType           eog_job_get_type           (void);
void            eog_job_finished           (EogJob          *job);

/* EogJobThumbnail */
GType           eog_job_thumbnail_get_type (void);
EogJob         *eog_job_thumbnail_new      (EogImage        *image);
void            eog_job_thumbnail_run      (EogJobThumbnail *thumbnail);

/* EogJobLoad */
GType           eog_job_load_get_type      (void);
EogJob 	       *eog_job_load_new 	   (EogImage        *image);
void		eog_job_load_run 	   (EogJobLoad 	    *load);					   

/* EogJobModel */
GType 		eog_job_model_get_type     (void);
EogJob 	       *eog_job_model_new          (GSList          *uri_list);
void            eog_job_model_run          (EogJobModel     *model);

/* EogJobTransform */
GType 		eog_job_transform_get_type (void);
EogJob 	       *eog_job_transform_new      (GList           *images,
					    EogTransform    *trans);
void            eog_job_transform_run      (EogJobTransform *model);

G_END_DECLS

#endif /* __EOG_JOBS_H__ */
