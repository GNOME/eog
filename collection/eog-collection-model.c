#include "eog-collection-model.h"
#include "eog-image-loader.h"
#include "bonobo/bonobo-moniker-util.h"
#include <libgnomevfs/gnome-vfs.h>

/* Signal IDs */
enum {
	INTERVAL_CHANGED,
	INTERVAL_ADDED,
	INTERVAL_REMOVED,
	SELECTION_CHANGED,
	BASE_URI_CHANGED,
	LAST_SIGNAL
};

static void marshal_interval_notification (GtkObject *object, GtkSignalFunc func, 
					   gpointer data, GtkArg *args);
static guint eog_model_signals[LAST_SIGNAL];

typedef struct {
	EogCollectionModel *model;
	GnomeVFSURI *uri;
	GnomeVFSFileInfo *info;
	GnomeVFSHandle *handle;
} LoadingContext;

struct _EogCollectionModelPrivate {
	/* holds for every id the corresponding cimage */
	GHashTable *id_image_mapping;

	GSList *selected_images;

	/* does all the image loading work */
	EogImageLoader *loader;

	/* list of images to load with loader */ 
	GSList *images_to_load;
	
	/* base uri e.g. from a directory */
	gchar *base_uri;
};


static GtkObjectClass *parent_class;

static void
image_loading_finished_cb (EogImageLoader *loader, CImage *img, gpointer data);

static void
free_hash_image (gpointer key, gpointer value, gpointer data)
{
	gtk_object_unref (GTK_OBJECT (value));
}

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
eog_collection_model_destroy (GtkObject *obj)
{
	EogCollectionModel *model;
	
	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (obj));

	model = EOG_COLLECTION_MODEL (obj);

	if (model->priv->loader)
		gtk_object_unref (GTK_OBJECT (model->priv->loader));
	model->priv->loader = NULL;

	if (model->priv->selected_images)
		g_slist_free (model->priv->selected_images);
	model->priv->selected_images = NULL;

	g_hash_table_foreach (model->priv->id_image_mapping, 
			      (GHFunc) free_hash_image, NULL);
	g_hash_table_destroy (model->priv->id_image_mapping);
	model->priv->id_image_mapping = NULL;

	if (model->priv->base_uri)
		g_free (model->priv->base_uri);
	model->priv->base_uri = NULL;
}

static void
eog_collection_model_finalize (GtkObject *obj)
{
	EogCollectionModel *model;
	
	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (obj));

	model = EOG_COLLECTION_MODEL (obj);

	g_free (model->priv);
}

static void
eog_collection_model_init (EogCollectionModel *obj)
{
	EogCollectionModelPrivate *priv;

	priv = g_new0(EogCollectionModelPrivate, 1);
	priv->loader = NULL;
	priv->id_image_mapping = NULL;
	priv->selected_images = NULL;
	priv->images_to_load = NULL;
	priv->base_uri = NULL;
	obj->priv = priv;
}

static void
eog_collection_model_class_init (EogCollectionModelClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass*) klass;
	
	parent_class = (GtkObjectClass*) gtk_type_class (gtk_object_get_type ());

	object_class->destroy = eog_collection_model_destroy;
	object_class->finalize = eog_collection_model_finalize;

	eog_model_signals[INTERVAL_CHANGED] =
		g_signal_new ("interval_changed",
				object_class->type,
				G_SIGNAL_RUN_FIRST,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_changed),
				NULL,
				NULL,
				marshal_interval_notification,
				G_TYPE_NONE,
			       	1,
				G_TYPE_POINTER);
	eog_model_signals[INTERVAL_ADDED] =
		g_signal_new ("interval_added",
				object_class->type,
				G_SIGNAL_RUN_FIRST,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_added),
				NULL,
				NULL,
				marshal_interval_notification,
				G_TYPE_NONE,
			       	1,
				G_TYPE_POINTER);
	eog_model_signals[INTERVAL_REMOVED] =
		g_signal_new ("interval_removed",
				object_class->type,
				G_SIGNAL_RUN_FIRST,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_removed),
				NULL,
				NULL,
				marshal_interval_notification,
				G_TYPE_NONE,
			       	1,
				G_TYPE_POINTER);
	eog_model_signals[SELECTION_CHANGED] = 
		g_signal_new ("selection_changed",
				object_class->type,
				G_SIGNAL_RUN_FIRST,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, selection_changed),
				NULL,
				NULL,
				gtk_marshal_NONE__NONE,
				G_TYPE_NONE,
			       	0);
	eog_model_signals[BASE_URI_CHANGED] = 
		g_signal_new ("base_uri_changed",
				object_class->type,
				G_SIGNAL_RUN_FIRST,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, base_uri_changed),
				NULL,
				NULL,
				gtk_marshal_NONE__NONE,
				G_TYPE_NONE,
			       	0);
}

typedef void (* IntervalNotificationFunc) (GtkObject *object, GList *id_list,
					   gpointer data);

static void
marshal_interval_notification (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	IntervalNotificationFunc rfunc;

	rfunc = (IntervalNotificationFunc) func;
	(* func) (object, GTK_VALUE_POINTER (args[0]), data);
}


GtkType 
eog_collection_model_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"EogCollectionModel",
			sizeof (EogCollectionModel),
			sizeof (EogCollectionModelClass),
			(GtkClassInitFunc)  eog_collection_model_class_init,
			(GtkObjectInitFunc) eog_collection_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (
			gtk_object_get_type (), &info);
	}

	return type;
}

void
eog_collection_model_construct (EogCollectionModel *model)
{
	EogImageLoader *loader;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
       
	/* init loader */
	loader = eog_image_loader_new (92,68);
	gtk_signal_connect (GTK_OBJECT (loader), "loading_finished", 
			    image_loading_finished_cb, model);

	model->priv->loader = loader;

	/* init hash table */
	model->priv->id_image_mapping = g_hash_table_new ((GHashFunc) g_direct_hash, 
							  (GCompareFunc) g_direct_equal);
}

EogCollectionModel*
eog_collection_model_new (void)
{
	EogCollectionModel *model;
	
	model = gtk_type_new (EOG_TYPE_COLLECTION_MODEL);

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
do_foreach (gpointer key, gpointer value, gpointer user_data)
{
	ForeachData *data = user_data;

	if (data->cont)
		data->cont = data->func (data->model, GPOINTER_TO_INT (key),
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
	g_hash_table_foreach (model->priv->id_image_mapping, do_foreach, 
			      foreach_data);
	g_free (foreach_data);
}

typedef struct {
	EogCollectionModel *model;
	guint id;
} RemoveItemData;

static gboolean
remove_item_idle (gpointer user_data)
{
	EogCollectionModelPrivate *priv;
	RemoveItemData *data = user_data;
	CImage *image;
	GList *id_list;

	priv = data->model->priv;

	image = g_hash_table_lookup (priv->id_image_mapping,
				     GINT_TO_POINTER (data->id));
	if (!image) {
		g_warning ("Could not find image %i!", data->id);
		return (FALSE);
	}

	g_hash_table_remove (priv->id_image_mapping, GINT_TO_POINTER(data->id));

	if (g_slist_find (priv->selected_images, image)) {
		priv->selected_images = g_slist_remove (priv->selected_images,
							image);
		gtk_signal_emit (GTK_OBJECT (data->model),
				 eog_model_signals [SELECTION_CHANGED]);
	}

	id_list = g_list_append (NULL, GINT_TO_POINTER (data->id));
	gtk_signal_emit (GTK_OBJECT (data->model),
			 eog_model_signals [INTERVAL_REMOVED], id_list);

	g_free (data);

	return (FALSE);
}

void
eog_collection_model_remove_item (EogCollectionModel *model, guint id)
{
	RemoveItemData *data;

	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	/*
	 * We need to do that in the idle loop as there could still be
	 * some loading or a g_hash_table_foreach running.
	 */
	data = g_new0 (RemoveItemData, 1);
	data->model = model;
	data->id = id;
	gtk_idle_add (remove_item_idle, data);
}

static gboolean
directory_visit_cb (const gchar *rel_path,
		    GnomeVFSFileInfo *info,
		    gboolean recursing_will_loop,
		    gpointer data,
		    gboolean *recurse)
{
	CImage *img;
	LoadingContext *ctx;
	GnomeVFSURI *uri;
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;
	GList *id_list = NULL;
	gint id;
	static gint count = 0;
	
	ctx = (LoadingContext*) data;
	model = ctx->model;
	priv = model->priv;

	g_print ("rel_path: %s\n", rel_path);
	uri = gnome_vfs_uri_append_file_name (ctx->uri, rel_path);
	g_print ("uri.toString(): %s\n", gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));

	img = cimage_new_uri (uri);			
	gnome_vfs_uri_unref (uri);
	id = cimage_get_unique_id (img);

	/* add image infos to internal lists */
	g_hash_table_insert (priv->id_image_mapping,
			     GINT_TO_POINTER (id),
			     img);
	id_list = g_list_append (id_list, 
				 GINT_TO_POINTER (id));
	gtk_signal_emit (GTK_OBJECT (model), 
			 eog_model_signals [INTERVAL_ADDED],
			 id_list);
	
	eog_image_loader_start (priv->loader, img);

	if (count++ % 50 == 0)
		while (gtk_events_pending ())
			gtk_main_iteration ();

	return TRUE;
}

static gboolean  
directory_filter_cb (const GnomeVFSFileInfo *info, gpointer data)
{
	g_print ("check for mime type: %s\n", info->mime_type);
	return (g_strncasecmp (info->mime_type, "image/", 6) == 0);	
}

static gint
real_dir_loading (LoadingContext *ctx)
{
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;
	GnomeVFSDirectoryFilter *filter;

	g_return_val_if_fail (ctx->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY, FALSE);

	model = ctx->model;
	priv = model->priv;

	filter = gnome_vfs_directory_filter_new_custom (directory_filter_cb,
							GNOME_VFS_DIRECTORY_FILTER_NEEDS_MIMETYPE,
							NULL);

	gnome_vfs_directory_visit_uri (ctx->uri,
				       GNOME_VFS_FILE_INFO_DEFAULT |
				       GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
				       filter,
				       GNOME_VFS_DIRECTORY_VISIT_DEFAULT,
				       directory_visit_cb,
				       ctx);

	loading_context_free (ctx);
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
		CImage *img;
		GList *id_list = NULL;
		gint id;

		img = cimage_new_uri (ctx->uri);			
		id = cimage_get_unique_id (img);
		
		/* add image infos to internal lists */
		g_hash_table_insert (priv->id_image_mapping,
				     GINT_TO_POINTER (id),
				     img);
		id_list = g_list_append (id_list, 
					 GINT_TO_POINTER (id));
		gtk_signal_emit (GTK_OBJECT (model), 
				 eog_model_signals [INTERVAL_ADDED],
				 id_list);
		
		eog_image_loader_start (priv->loader, img);
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
eog_collection_model_set_uri (EogCollectionModel *model, 
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
			gtk_idle_add ((GtkFunction) real_dir_loading, ctx);
		else if (ctx->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
			gtk_idle_add ((GtkFunction) real_file_loading, ctx);
		else {
			loading_context_free (ctx);
			g_warning (_("Can't handle URI: %s"), text_uri);
			return;
		}
	} else {
		g_warning (_("Can't handle URI: %s"), text_uri);
		return;
	}	
	
	if (priv->base_uri == NULL) {
		priv->base_uri = g_strdup (gnome_vfs_uri_get_basename (ctx->uri));
		gtk_signal_emit (GTK_OBJECT (model), eog_model_signals [BASE_URI_CHANGED]);
	} else {
		g_free (priv->base_uri);
		priv->base_uri = g_strdup("multiple");
		gtk_signal_emit (GTK_OBJECT (model), eog_model_signals [BASE_URI_CHANGED]);
	}
}

void 
eog_collection_model_set_uri_list (EogCollectionModel *model,
				   GList *uri_list)
{
	GList *node;
	LoadingContext *ctx;
	gchar *text_uri;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	node = uri_list;

	while (node != NULL) {
		text_uri = (gchar*) node->data;
		ctx = prepare_context (model, text_uri);

		if (ctx != NULL) {
			if (ctx->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
				gtk_idle_add ((GtkFunction) real_dir_loading, ctx);
			else if (ctx->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
				gtk_idle_add ((GtkFunction) real_file_loading, ctx);
			else {
				loading_context_free (ctx);
				g_warning (_("Can't handle URI: %s"), text_uri);
				return;
			}
		} else {
			g_warning (_("Can't handle URI: %s"), text_uri);
			return;
		}	

		node = node->next;
	}
	
	if (model->priv->base_uri != NULL)
		g_free (model->priv->base_uri);
	model->priv->base_uri = g_strdup ("multiple");
	gtk_signal_emit (GTK_OBJECT (model), eog_model_signals [BASE_URI_CHANGED]);
}

void
image_loading_finished_cb (EogImageLoader *loader, CImage *img, gpointer model)
{
	GList *id_list = NULL;

	id_list = g_list_append (id_list, 
				 GINT_TO_POINTER (cimage_get_unique_id (img)));

	gtk_signal_emit (GTK_OBJECT (model), 
			 eog_model_signals [INTERVAL_CHANGED],
			 id_list);
}


gint
eog_collection_model_get_length (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), 0);

	return g_hash_table_size (model->priv->id_image_mapping);
}


CImage*
eog_collection_model_get_image (EogCollectionModel *model,
				guint unique_id)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	return CIMAGE (g_hash_table_lookup (model->priv->id_image_mapping,
					    GINT_TO_POINTER (unique_id)));
}

gchar*
eog_collection_model_get_uri (EogCollectionModel *model,
			      guint unique_id)
{
	CImage *img;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	img = eog_collection_model_get_image (model, unique_id);
	if (img == NULL) return NULL;
	
	return gnome_vfs_uri_to_string (cimage_get_uri (img), GNOME_VFS_URI_HIDE_NONE);
}

static void
select_all_images (EogCollectionModel *model)
{
	EogCollectionModelPrivate *priv;

	priv = model->priv;

	g_slist_free (priv->selected_images);
	priv->selected_images = NULL;

	/* FIXME: not finished yet */
	gtk_signal_emit (GTK_OBJECT (model), 
			 eog_model_signals [SELECTION_CHANGED]);
}

static void
unselect_all_images (EogCollectionModel *model)
{
	GSList *node;
	GList *id_list = NULL;
	CImage *img;
	guint id;

	for (node = model->priv->selected_images; node; node = node->next) {
		g_return_if_fail (IS_CIMAGE (node->data));
		img = CIMAGE (node->data);
		id = cimage_get_unique_id (img);

		cimage_set_select_status (img, FALSE);
		id_list = g_list_append (id_list, GINT_TO_POINTER (id));
	}

	if (model->priv->selected_images)
		g_slist_free (model->priv->selected_images);
	model->priv->selected_images = NULL;

	if (id_list)
		gtk_signal_emit (GTK_OBJECT (model), 
				 eog_model_signals [INTERVAL_CHANGED],
				 id_list);

	gtk_signal_emit (GTK_OBJECT (model), 
			 eog_model_signals [SELECTION_CHANGED]);
}

void
eog_collection_model_set_select_status (EogCollectionModel *model,
					guint id, gboolean status)
{
	CImage *image;
	GList *id_list;

	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	image = eog_collection_model_get_image (model, id);
	if (status == cimage_is_selected (image))
		return;

	cimage_set_select_status (image, status);
	if (status)
		model->priv->selected_images = g_slist_append (
					model->priv->selected_images, image);
	else
		model->priv->selected_images = g_slist_remove (
					model->priv->selected_images, image);
	id_list = g_list_append (NULL,
			GINT_TO_POINTER (cimage_get_unique_id (image)));

	gtk_signal_emit (GTK_OBJECT (model),
			 eog_model_signals [INTERVAL_CHANGED], id_list);
	
	gtk_signal_emit (GTK_OBJECT (model),
			 eog_model_signals [SELECTION_CHANGED]);
}

void
eog_collection_model_set_select_status_all (EogCollectionModel *model, 
					    gboolean status)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	if (status) 
		select_all_images (model);
	else
		unselect_all_images (model);
}


void eog_collection_model_toggle_select_status (EogCollectionModel *model,
						guint id)
{
	EogCollectionModelPrivate *priv;
	GList *id_list = NULL;
	CImage *image;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	priv = model->priv;

	image = eog_collection_model_get_image (model, id);
	cimage_toggle_select_status (image);
	if (cimage_is_selected (image)) {
		priv->selected_images = 
			g_slist_append (priv->selected_images,
					image);
	} else {
		priv->selected_images = 
			g_slist_remove (priv->selected_images,
					image);
	}

	id_list = g_list_append (id_list, GINT_TO_POINTER (id));

	if (id_list != NULL) 
		gtk_signal_emit (GTK_OBJECT (model), 
				 eog_model_signals [INTERVAL_CHANGED],
				 id_list);

	gtk_signal_emit (GTK_OBJECT (model), 
			 eog_model_signals [SELECTION_CHANGED]);
}


gchar*
eog_collection_model_get_base_uri (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	if (!model->priv->base_uri) return NULL;

	if (g_strcasecmp ("multiple", model->priv->base_uri) == 0)
		return NULL;
	else
		return model->priv->base_uri;
}


gint
eog_collection_model_get_selected_length (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), 0);
	
	return g_slist_length (model->priv->selected_images);
}

CImage*
eog_collection_model_get_selected_image (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), 0);
	
	if (eog_collection_model_get_selected_length (model) == 1) {
		return CIMAGE (model->priv->selected_images->data);
	} else
		return NULL;
}
