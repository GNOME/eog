#include <string.h>
#include "eog-collection-model.h"
#include "bonobo/bonobo-moniker-util.h"
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnome/gnome-macros.h>

#include "eog-collection-marshal.h"

/* Signal IDs */
enum {
	PREPARED,
	IMAGE_ADDED,
	IMAGE_REMOVED,
	BASE_URI_CHANGED,
	LAST_SIGNAL
};

static guint eog_model_signals[LAST_SIGNAL];

typedef struct {
	EogCollectionModel *model;
	GnomeVFSURI *uri;
	GnomeVFSFileInfo *info;
	GnomeVFSHandle *handle;
} LoadingContext;

struct _EogCollectionModelPrivate {
	GList *image_list;

	/* base uri e.g. from a directory */
	gchar *base_uri;
};

static void eog_collection_model_class_init (EogCollectionModelClass *klass);
static void eog_collection_model_instance_init (EogCollectionModel *object);
static void eog_collection_model_dispose (GObject *object);
static void eog_collection_model_finalize (GObject *object);

GNOME_CLASS_BOILERPLATE (EogCollectionModel, eog_collection_model,
			 GObject, G_TYPE_OBJECT);

static void
loading_context_free (LoadingContext *ctx)
{
	if (ctx->uri)
		gnome_vfs_uri_unref (ctx->uri);
	ctx->uri = NULL;

	if (ctx->info)
		gnome_vfs_file_info_unref (ctx->info);
	ctx->info = NULL;
	
	if (ctx->handle)
		gnome_vfs_close (ctx->handle);
	ctx->handle = NULL;
	
	g_free (ctx);
}

static void
eog_collection_model_dispose (GObject *obj)
{
	EogCollectionModel *model;
	
	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (obj));

	model = EOG_COLLECTION_MODEL (obj);

	if (model->priv->image_list != NULL) {
		g_list_foreach (model->priv->image_list, 
				(GFunc) g_object_unref, NULL);
		g_list_free (model->priv->image_list);
		model->priv->image_list = NULL;
	}

	if (model->priv->base_uri)
		g_free (model->priv->base_uri);
	model->priv->base_uri = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (obj));
}

static void
eog_collection_model_finalize (GObject *obj)
{
	EogCollectionModel *model;
	
	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (obj));

	model = EOG_COLLECTION_MODEL (obj);

	g_free (model->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (obj));
}

static void
eog_collection_model_instance_init (EogCollectionModel *obj)
{
	EogCollectionModelPrivate *priv;

	priv = g_new0(EogCollectionModelPrivate, 1);
	priv->image_list = NULL;
	priv->base_uri = NULL;
	obj->priv = priv;
}

static void
eog_collection_model_class_init (EogCollectionModelClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;
	
	object_class->dispose = eog_collection_model_dispose;
	object_class->finalize = eog_collection_model_finalize;


	eog_model_signals[PREPARED] = 
		g_signal_new ("prepared",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogCollectionModelClass, prepared),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	eog_model_signals[IMAGE_ADDED] =
		g_signal_new ("image-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogCollectionModelClass, image_added),
			      NULL,
			      NULL,
			      eog_collection_marshal_VOID__OBJECT_INT,
			      G_TYPE_NONE,
			      2,
			      EOG_TYPE_IMAGE,
			      G_TYPE_INT);
	eog_model_signals[IMAGE_REMOVED] =
		g_signal_new ("image-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogCollectionModelClass, image_removed),
			      NULL,
			      NULL,
			      eog_collection_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
	eog_model_signals[BASE_URI_CHANGED] = 
		g_signal_new ("base-uri-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogCollectionModelClass, base_uri_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

void
eog_collection_model_construct (EogCollectionModel *model)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

}

EogCollectionModel*
eog_collection_model_new (void)
{
	EogCollectionModel *model;
	
	model = EOG_COLLECTION_MODEL (g_object_new (EOG_TYPE_COLLECTION_MODEL, NULL));

	eog_collection_model_construct (model);

	return model;
}

typedef struct {
	EogCollectionModel *model;
	EogCollectionModelForeachFunc func;
	gpointer data;
	gboolean cont;
} ForeachData;

static void
do_foreach (gpointer value, gpointer user_data)
{
	ForeachData *data = user_data;

	if (data->cont)
		data->cont = data->func (data->model, EOG_IMAGE (value),
					 data->data);
}

void
eog_collection_model_foreach (EogCollectionModel *model,
			      EogCollectionModelForeachFunc func,
			      gpointer data)
{
	ForeachData *foreach_data;

	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	foreach_data = g_new0 (ForeachData, 1);
	foreach_data->model = model;
	foreach_data->func = func;
	foreach_data->data = data;
	foreach_data->cont = TRUE;
	g_list_foreach (model->priv->image_list, do_foreach, 
			      foreach_data);
	g_free (foreach_data);
}

void
eog_collection_model_remove_item (EogCollectionModel *model, EogImage *image)
{
	EogCollectionModelPrivate *priv;

	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	priv = model->priv;

	priv->image_list = g_list_remove (priv->image_list, image);
	g_object_unref (image);
}

static gboolean
directory_visit_cb (const gchar *rel_uri,
		    GnomeVFSFileInfo *info,
		    gboolean recursing_will_loop,
		    gpointer data,
		    gboolean *recurse)
{
	EogImage *image;
	LoadingContext *ctx;
	GnomeVFSURI *uri;
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;
	
	ctx = (LoadingContext*) data;
	model = ctx->model;
	priv = model->priv;

	if (g_strncasecmp (info->mime_type, "image/", 6) != 0) {
		return TRUE;
	}

	/* FIXME: escape uri */
	uri = gnome_vfs_uri_append_file_name (ctx->uri, rel_uri);	

	image = eog_image_new_uri (uri, EOG_IMAGE_LOAD_DEFAULT);			
	gnome_vfs_uri_unref (uri);

	priv->image_list = g_list_prepend (priv->image_list, image);

	return TRUE;
}

static int
compare_filename_cb (gconstpointer a, gconstpointer b)
{
	EogImage *img_a;
	EogImage *img_b;

	img_a = EOG_IMAGE (a);
	img_b = EOG_IMAGE (b);

	return strcmp (eog_image_get_collate_key (img_a), eog_image_get_collate_key (img_b));
}

static gint
real_dir_loading (LoadingContext *ctx)
{
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;

	g_return_val_if_fail (ctx->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY, FALSE);

	model = ctx->model;
	priv = model->priv;

	gnome_vfs_directory_visit_uri (ctx->uri,
				       GNOME_VFS_FILE_INFO_DEFAULT |
				       GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
				       GNOME_VFS_DIRECTORY_VISIT_DEFAULT,
				       directory_visit_cb,
				       ctx);

	loading_context_free (ctx);

	priv->image_list = g_list_sort (priv->image_list, compare_filename_cb);

	g_signal_emit (G_OBJECT (model), eog_model_signals[PREPARED], 0);

	return FALSE;
}
	

static gint
real_file_loading (LoadingContext *ctx)
{
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;
	GnomeVFSResult result;

	g_return_val_if_fail (ctx->info->type == GNOME_VFS_FILE_TYPE_REGULAR, FALSE);

	model = ctx->model;
	priv = model->priv;

	result = gnome_vfs_get_file_info_uri (ctx->uri,
					      ctx->info, 
					      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);

	if (result != GNOME_VFS_OK) {
		g_warning ("Error while obtaining file informations.\n");
		return FALSE;
	}
	
	if(g_strncasecmp(ctx->info->mime_type, "image/", 6) == 0) {
		EogImage *image;

		image = eog_image_new_uri (ctx->uri, EOG_IMAGE_LOAD_DEFAULT);			
		
		priv->image_list = g_list_insert_sorted (priv->image_list, image, compare_filename_cb);

		g_signal_emit (G_OBJECT (model), 
			       eog_model_signals[IMAGE_ADDED], 0, image, (int) g_list_index (priv->image_list, image));
	}

	loading_context_free (ctx);

	return FALSE;
}


static LoadingContext*
prepare_context (EogCollectionModel *model, const gchar *text_uri) 
{
	LoadingContext *ctx;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	ctx = g_new0 (LoadingContext, 1);
	ctx->uri = gnome_vfs_uri_new (text_uri);

#ifdef COLLECTION_DEBUG
	g_message ("Prepare context for URI: %s", text_uri);
#endif

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info_uri (ctx->uri, info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result != GNOME_VFS_OK) {
		loading_context_free (ctx);
		return NULL;
	}

	ctx->info = info;
	ctx->handle = 0;
	ctx->model = model;

	return ctx;
}

void
eog_collection_model_add_uri (EogCollectionModel *model, 
			      const gchar *text_uri)
{
	EogCollectionModelPrivate *priv;
	LoadingContext *ctx;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (text_uri != NULL);
	
	priv = model->priv;

	ctx = prepare_context (model, text_uri);
	
	if (ctx != NULL) {
		if (ctx->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
			g_idle_add ((GtkFunction) real_dir_loading, ctx);
		else if (ctx->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
			g_idle_add ((GtkFunction) real_file_loading, ctx);
		else {
			loading_context_free (ctx);
			g_warning ("Can't handle URI: %s", text_uri);
			return;
		}
	} else {
		g_warning ("Can't handle URI: %s", text_uri);
		return;
	}	
	
	if (priv->base_uri == NULL) {
		priv->base_uri = gnome_vfs_uri_to_string (ctx->uri, GNOME_VFS_URI_HIDE_NONE);
	} 
	else {
		g_free (priv->base_uri);
		priv->base_uri = g_strdup("multiple");
	}
	g_signal_emit_by_name (G_OBJECT (model), "base-uri-changed");
}


gint
eog_collection_model_get_length (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), 0);

	return g_list_length (model->priv->image_list);
}


EogImage*
eog_collection_model_get_image (EogCollectionModel *model,
				int position)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	return g_list_nth_data (model->priv->image_list, position);
}

gchar*
eog_collection_model_get_base_uri (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	if (!model->priv->base_uri) 
		return NULL;
	else
		return model->priv->base_uri;
}

GList*
eog_collection_model_get_image_list (EogCollectionModel *model)
{
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	return model->priv->image_list;
}
