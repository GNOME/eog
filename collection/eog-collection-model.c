#include "eog-collection-model.h"
#include "eog-image-loader.h"
#include "bonobo/bonobo-moniker-util.h"

/* Signal IDs */
enum {
	INTERVAL_CHANGED,
	INTERVAL_ADDED,
	INTERVAL_REMOVED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static void marshal_interval_notification (GtkObject *object, GtkSignalFunc func, 
					   gpointer data, GtkArg *args);
static guint eog_model_signals[LAST_SIGNAL];

typedef enum {
	LC_BONOBO_STORAGE,
	LC_BONOBO_STREAM
} LCType; 

typedef struct {
	EogCollectionModel *model;
	gchar *uri;
	LCType type;
	Bonobo_Unknown storage_stream;
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
		gtk_signal_new ("interval_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_changed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	eog_model_signals[INTERVAL_ADDED] =
		gtk_signal_new ("interval_added",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_added),
				marshal_interval_notification,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	eog_model_signals[INTERVAL_REMOVED] =
		gtk_signal_new ("interval_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_removed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	eog_model_signals[SELECTION_CHANGED] = 
		gtk_signal_new ("selection_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, eog_model_signals, LAST_SIGNAL);

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
	
	model = gtk_type_new (EOG_COLLECTION_MODEL_TYPE);

	eog_collection_model_construct (model);

	return model;
}

static gint
real_storage_loading (LoadingContext *ctx)
{
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;
	CORBA_Environment ev;
	Bonobo_Storage_DirectoryList *dir_list;
	gint i;

	g_return_val_if_fail (ctx->type == LC_BONOBO_STORAGE, FALSE);

	model = ctx->model;
	priv = model->priv;

	CORBA_exception_init (&ev);

	dir_list = Bonobo_Storage_listContents (ctx->storage_stream, "",
						Bonobo_FIELD_CONTENT_TYPE,
						&ev);

	/* Create a list of images to load and their
	 * initial image object.
	 */
	for (i = 0; i < dir_list->_length; i++) {
		Bonobo_StorageInfo info;

		info = (Bonobo_StorageInfo) dir_list->_buffer[i];
		
		if(g_strncasecmp(info.content_type, "image/", 6) == 0) {
			CImage *img;
			gchar *uri;
			GList *id_list = NULL;
			gint id;

			uri = g_strconcat (ctx->uri, "/", info.name, NULL);
			img = cimage_new (uri);			
			g_free (uri);
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

		/* update gui every 20th time */
		if (i % 20 == 0)
			while (gtk_events_pending ())
				gtk_main_iteration ();
	}

	g_free (ctx->uri);
	Bonobo_Unknown_unref (ctx->storage_stream, &ev);
	CORBA_Object_release (ctx->storage_stream, &ev);
	g_free (ctx);

	CORBA_exception_free (&ev);

	return FALSE;
}

static gint
real_stream_loading (LoadingContext *ctx)
{
	CORBA_Environment ev;
	Bonobo_StorageInfo *info;
	EogCollectionModel *model;
	EogCollectionModelPrivate *priv;

	g_return_val_if_fail (ctx->type == LC_BONOBO_STREAM, FALSE);

	model = ctx->model;
	priv = model->priv;
	CORBA_exception_init (&ev);
	
	info =  Bonobo_Stream_getInfo (ctx->storage_stream, 
				       Bonobo_FIELD_CONTENT_TYPE, 
				       &ev);
	
	if(g_strncasecmp(info->content_type, "image/", 6) == 0) {
		CImage *img;
		GList *id_list = NULL;
		gint id;

		img = cimage_new (ctx->uri);			
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

	g_free (ctx->uri);
	Bonobo_Unknown_unref (ctx->storage_stream, &ev);
	CORBA_Object_release (ctx->storage_stream, &ev);
	g_free (ctx);
	
	CORBA_exception_free (&ev);
	return FALSE;
}

static LoadingContext*
prepare_context (EogCollectionModel *model, const gchar *uri) 
{
	CORBA_Environment ev;
	LoadingContext *ctx;

	CORBA_exception_init (&ev);
	ctx = g_new0 (LoadingContext, 1);

	g_message ("Prepare context for URI: %s", uri);
	
	/* check for BonoboStorage interface */
	ctx->storage_stream = bonobo_get_object (uri, "IDL:Bonobo/Storage:1.0", &ev);
	if (ev._major == CORBA_NO_EXCEPTION) {
		ctx->type = LC_BONOBO_STORAGE;
	} else {
		ev._major = CORBA_NO_EXCEPTION;
		/* if failed, check for BonoboStream interface */
		ctx->storage_stream = bonobo_get_object (uri, "IDL:Bonobo/Stream:1.0", &ev);
		if (ev._major == CORBA_NO_EXCEPTION) {
			ctx->type = LC_BONOBO_STREAM;
		} else {
			CORBA_exception_free (&ev);
			g_free (ctx);
			return NULL;
		}
	}

	ctx->uri = g_strdup (uri);
	ctx->model = model;

	CORBA_exception_free (&ev);
	return ctx;
}

void
eog_collection_model_set_uri (EogCollectionModel *model, 
			      const gchar *uri)
{
	EogCollectionModelPrivate *priv;
	LoadingContext *ctx;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (uri != NULL);
	
	priv = model->priv;

	ctx = prepare_context (model, uri);

	if (ctx != NULL) {
		if (ctx->type == LC_BONOBO_STORAGE)
			gtk_idle_add ((GtkFunction) real_storage_loading, ctx);
		else
			gtk_idle_add ((GtkFunction) real_stream_loading, ctx);
	} else {
		g_warning (_("Can't handle URI: %s"), uri);
	}	
	
	if (priv->base_uri == NULL)
		priv->base_uri = g_strdup (uri);
	else {
		g_free (priv->base_uri);
		priv->base_uri = g_strdup("multiple"));
	}
}

void 
eog_collection_model_set_uri_list (EogCollectionModel *model,
				   GList *uri_list)
{
	GList *node;
	LoadingContext *ctx;
	gchar *uri;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	node = uri_list;

	while (node != NULL) {
		uri = (gchar*) node->data;
		ctx = prepare_context (model, uri);
		
		if (ctx != NULL) {
			if (ctx->type == LC_BONOBO_STORAGE)
				gtk_idle_add ((GtkFunction) real_storage_loading, ctx);
			else
				gtk_idle_add ((GtkFunction) real_stream_loading, ctx);
		} else {
			g_warning ("Can't handle URI: %s", uri);
		}	

		node = node->next;
	}
	
	if (model->priv->base_uri != NULL)
		g_free (model->priv->base_uri);
	model->priv->base_uri = g_strdup ("multiple");
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
	
	return cimage_get_uri (img);
}

static void
select_all_images (EogCollectionModel *model)
{
	EogCollectionModelPrivate *priv;

	priv = model->priv;

	g_slist_free (priv->selected_images);
	priv->selected_images = NULL;

	
}

static void
unselect_all_images (EogCollectionModel *model)
{
	EogCollectionModelPrivate *priv;
	GSList *node;
	GList *id_list = NULL;

	priv = model->priv;

	node = priv->selected_images;
	while (node) {
		CImage *img = (CImage*) node->data;

		cimage_set_select_status (img, FALSE);
		id_list = g_list_append (id_list,
					 GINT_TO_POINTER (cimage_get_unique_id (img)));

		node = node->next;
	}

	if (priv->selected_images)
		g_slist_free (priv->selected_images);
	priv->selected_images = NULL;

	if (id_list)
		gtk_signal_emit (GTK_OBJECT (model), 
				 eog_model_signals [INTERVAL_CHANGED],
				 id_list);
}


void eog_collection_model_set_select_status_all (EogCollectionModel *model, 
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
}


gchar*
eog_collection_model_get_base_uri (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	if (g_strcasecmp ("multiple", model->priv->base_uri) == 0)
		return NULL;
	else
		return model->priv->base_uri;
}
