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


struct _EogCollectionModelPrivate {
	Bonobo_Unknown storage;
	gchar *uri;

	GList *image_list;

	EogImageLoader *loader;
	GList *last_loaded_image;
	guint idle_handler_id;
};


static GtkObjectClass *parent_class;

static void
image_loading_finished_cb (EogImageLoader *loader, CImage *img, gpointer data);

static void
eog_collection_model_destroy (GtkObject *obj)
{
	EogCollectionModel *model;
	CORBA_Environment ev;
	
	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (obj));

	CORBA_exception_init (&ev);
	model = EOG_COLLECTION_MODEL (obj);

	if (model->priv->loader)
		gtk_object_unref (GTK_OBJECT (model->priv->loader));
	model->priv->loader = NULL;

	if (model->priv->storage != CORBA_OBJECT_NIL)
	        CORBA_Object_release (model->priv->storage, &ev);
	model->priv->storage = CORBA_OBJECT_NIL;

	if (model->priv->uri)
		g_free (model->priv->uri);
	model->priv->uri = NULL;

	CORBA_exception_free (&ev);
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
	priv->storage = CORBA_OBJECT_NIL;
	priv->loader = NULL;
	priv->uri = NULL;
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
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	eog_model_signals[INTERVAL_ADDED] =
		gtk_signal_new ("interval_added",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_added),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	eog_model_signals[INTERVAL_REMOVED] =
		gtk_signal_new ("interval_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, interval_removed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	eog_model_signals[SELECTION_CHANGED] = 
		gtk_signal_new ("selection_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionModelClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, eog_model_signals, LAST_SIGNAL);

}

typedef void (* IntervalNotificationFunc) (GtkObject *object, guint start, guint length,
					   gpointer data);

static void
marshal_interval_notification (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	IntervalNotificationFunc rfunc;

	rfunc = (IntervalNotificationFunc) func;
	(* func) (object, GTK_VALUE_UINT (args[0]), GTK_VALUE_UINT (args[1]), data);
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
       
	loader = eog_image_loader_new (100,100);
        eog_image_loader_set_model (loader, model);
	gtk_signal_connect (GTK_OBJECT (loader), "loading_finished", 
			    image_loading_finished_cb, model);

	model->priv->loader = loader;
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
real_image_loading (EogCollectionModel *model)
{
	EogCollectionModelPrivate *priv;
	CORBA_Environment ev;
	Bonobo_Storage_DirectoryList *dir_list;
	gint i;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), FALSE);
	g_assert (model->priv->storage != CORBA_OBJECT_NIL);

	priv = model->priv;

	/* remove idle handler */
	gtk_idle_remove (priv->idle_handler_id);
	priv->idle_handler_id = 0;

	CORBA_exception_init (&ev);

	dir_list = Bonobo_Storage_listContents (priv->storage, "",
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
			img = cimage_new (info.name);			
			priv->image_list = g_list_append (priv->image_list, img);
		}

		/* update gui every 20th time */
		if (i % 20 == 0)
			while (gtk_events_pending ())
				gtk_main_iteration ();
	}
	
	/* 
	 * Start the image loading through EogImageLoader.
	 */
	priv->last_loaded_image = NULL;
	eog_image_loader_start (priv->loader);

	return TRUE;
}
	      

LoadingContext*
eog_collection_model_get_next_loading_context (EogCollectionModel *model)
{
	EogCollectionModelPrivate *priv;
	CORBA_Environment ev;
	GList *current_image;
	CImage *img;
	LoadingContext *lctx;
	gchar *path;
	gboolean loading_failed = TRUE;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);
	g_assert (model->priv->storage != CORBA_OBJECT_NIL);

	priv = model->priv;

	if (priv->last_loaded_image == NULL) {
		current_image = priv->image_list;
	} else {
		current_image = priv->last_loaded_image->next;
	}

	CORBA_exception_init (&ev);

	lctx = g_new0 (LoadingContext, 1);
	while (loading_failed) {
		if (current_image == NULL) return NULL;
	
		img = (CImage*) current_image->data;
	
		path = cimage_get_path (img);
		lctx->stream = Bonobo_Storage_openStream (priv->storage, 
							  path,
							  Bonobo_Storage_READ, &ev);
		g_free (path);
	
		if (ev._major != CORBA_NO_EXCEPTION) {
			cimage_set_loading_failed (img);
			current_image = current_image->next;
		} else {
			loading_failed = FALSE;
		}
	}

	lctx->image = (CImage*)current_image->data;
	gtk_object_ref (GTK_OBJECT (lctx->image));

	priv->last_loaded_image = current_image;

	CORBA_exception_free (&ev);

	return lctx;
}

void
eog_collection_model_set_uri (EogCollectionModel *model, 
			      const gchar *uri)
{
	CORBA_Environment ev;
	EogCollectionModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (uri != NULL);
	
	priv = model->priv;

	/* FIXME: Currently, we don't support multiple
	 *        uris or replacing one with another.
	 */
	g_assert (model->priv->storage == CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	priv->storage = bonobo_get_object (uri, "IDL:Bonobo/Storage:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		priv->storage = CORBA_OBJECT_NIL;
		g_warning (_("Couldn't retrieve storage from uri.\n"));
		return;
	}
	priv->uri = g_strdup (uri);

	model->priv->idle_handler_id = gtk_idle_add ((GtkFunction) real_image_loading, model);
	
	CORBA_exception_free (&ev);
}

void
image_loading_finished_cb (EogImageLoader *loader, CImage *img, gpointer model)
{
	gtk_signal_emit (GTK_OBJECT (model), 
			 eog_model_signals [INTERVAL_CHANGED],
			 cimage_get_unique_id (img), 1);
}


gint
eog_collection_model_get_length (EogCollectionModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), 0);

	return g_list_length (model->priv->image_list);
}


CImage*
eog_collection_model_get_image (EogCollectionModel *model,
				guint unique_id)
{
	static int n_call = 1;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	if (0 <= unique_id < g_list_length (model->priv->image_list)) {
		CImage *img = (CImage*) g_list_nth_data (model->priv->image_list, unique_id);
		return img;
	} else
		return NULL;
}

gchar*
eog_collection_model_get_uri (EogCollectionModel *model,
			      guint unique_id)
{
	CImage *img;
	gchar *uri;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	img = eog_collection_model_get_image (model, unique_id);
	if (img == NULL) return NULL;
	
	uri = g_strconcat (model->priv->uri, "/", cimage_get_path (img), NULL);
	
	g_print ("image uri is: %s\n", uri);

	return uri;
}

GList*
eog_collection_model_get_images (EogCollectionModel *model,
				 guint min_id, guint len)
{
	return NULL;
}

GList*
eog_collection_model_get_selection (EogCollectionModel *model)
{
	return NULL;
}


GList*
eog_collection_model_get_selection_in_range (EogCollectionModel *model,
					     guint min_id, guint len)
{
	return NULL;
}
				    
