/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8 c-style: "K&R" -*- */
/**
 * eog-image-list-model.c
 *
 * Authors:
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000 Free Software Foundation.
 */

#include <gtk/gtk.h>
#include "eog-image-list-model.h"
#include "cimage.h"
#include <libgnome/libgnome.h>

struct _EogImageListModelPrivate {
	/* list of all images */
	GList *image_list;
};


static void eog_image_list_model_class_init (EogImageListModelClass *class);
static void eog_image_list_model_init (EogImageListModel *model);
static void eog_image_list_model_destroy (GtkObject *object);

static GnomeIconListModelClass *parent_class;

static guint eilm_get_length (GnomeListModel *model, gpointer data);
static void eilm_get_icon (GnomeIconListModel *model, 
			   guint n, 
                           CImage **image,
			   gpointer data);

/**
 * eog_image_list_model_get_type:
 * @void:
 *
 * Registers the #EogImageListModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #EogImageListModel class.
 **/
GtkType 
eog_image_list_model_get_type(void)
{
	static GtkType eog_image_list_model_type = 0;

	if (!eog_image_list_model_type) {
		static const GtkTypeInfo eog_image_list_model_info = {
			"EogImageListModel",
			sizeof (EogImageListModel),
			sizeof (EogImageListModelClass),
			(GtkClassInitFunc) eog_image_list_model_class_init,
			(GtkObjectInitFunc) eog_image_list_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		eog_image_list_model_type = gtk_type_unique (GNOME_TYPE_ICON_LIST_MODEL, 
							     &eog_image_list_model_info);
	}

	return eog_image_list_model_type;
}

/* Class initialization function for the image list model. */
void
eog_image_list_model_class_init(EogImageListModelClass *klass)
{
	GtkObjectClass *obj_klass;

	parent_class = gtk_type_class(gnome_icon_list_model_get_type ());

	obj_klass = (GtkObjectClass*) klass;
	obj_klass->destroy = eog_image_list_model_destroy;
}

/* Object initialization function for the image list model. */
void
eog_image_list_model_init(EogImageListModel *model)
{
	EogImageListModelPrivate *priv;

	priv = g_new0(EogImageListModelPrivate, 1);
	model->priv = priv;
}

/* Destroy handler for the image list model. */
void 
eog_image_list_model_destroy (GtkObject *object)
{
	EogImageListModel *model;
	EogImageListModelPrivate *priv;
        GList *tmp;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EOG_IMAGE_LIST_MODEL (object));

	model = EOG_IMAGE_LIST_MODEL(object);
	priv = model->priv;

        tmp = priv->image_list;
        while (tmp) {
                gtk_object_unref (GTK_OBJECT (tmp->data));
                tmp = g_list_next (tmp);
        }
	g_list_free (priv->image_list);
	priv->image_list = NULL;

	g_free (priv);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/**
 * eog_image_list_model_new:
 * @void:
 *
 * Creates a new image list model implementation. 
 *
 * Return value: The newly-created object.
 **/
GtkObject*
eog_image_list_model_new(void)
{
	EogImageListModel *model;

	model = EOG_IMAGE_LIST_MODEL (gtk_type_new (TYPE_EOG_IMAGE_LIST_MODEL));
	model->priv->image_list = NULL;

	/* create model */
	gtk_signal_connect (GTK_OBJECT (model), "get_length",
			    GTK_SIGNAL_FUNC (eilm_get_length),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "get_icon",
			    GTK_SIGNAL_FUNC (eilm_get_icon),
			    model);

	return GTK_OBJECT(model);
}

/* Queries the number of images that are managed in this model. */
/* Implementation of gnome_list_model_get_length. */
guint 
eilm_get_length (GnomeListModel *model, gpointer data)
{
	GList *image_list = NULL;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (IS_EOG_IMAGE_LIST_MODEL (model), 0);

	image_list = EOG_IMAGE_LIST_MODEL (model)->priv->image_list;
	
	return g_list_length (image_list);
}

/* Returns the appropriate pixbuf and caption for icon number n. */
/* Implementation of gnome_icon_list_model_get_icon. */
void 
eilm_get_icon (GnomeIconListModel *model, 
               guint n, 
               CImage **image, 
               gpointer data)
{
	GList *image_list;
	
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));
	g_return_if_fail (IS_EOG_IMAGE_LIST_MODEL (model));
	
	*image = NULL;

	image_list = EOG_IMAGE_LIST_MODEL(model)->priv->image_list;
	
	if ((n < 0) || (n > g_list_length (image_list)))
	{
		g_warning ("Wrong icon number (out of range): %i", n);
		return;
	}
	    
	*image = (CImage*) g_list_nth_data (image_list, n);
	g_assert (image != NULL);

}


/**
 * eog_image_list_model_add_image:
 * @model: Image list model.
 * @image: The image to add.
 *
 * Adds a single image to the model.
 **/
void 
eog_image_list_model_add_image(EogImageListModel *model, CImage *image)
{
	GList *image_list = NULL;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_LIST_MODEL (model));
	g_return_if_fail (image != NULL);

	image_list = g_list_append (image_list, image);
	
	eog_image_list_model_add_images (model, image_list);
}

/**
 * eog_image_list_model_add_images:
 * @model: Image list model.
 * @images: List of Image* objects.
 *
 * Adds all images in the list to the model.
 **/
void 
eog_image_list_model_add_images(EogImageListModel *model, GList *images)
{
	EogImageListModelPrivate *priv;
	gint old_length, new_length;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_LIST_MODEL (model));

	if (images == NULL) return;

	priv = model->priv;

	/* add images to internal list */
	old_length = g_list_length (priv->image_list);
        priv->image_list = g_list_concat (priv->image_list, images);

	new_length = g_list_length (priv->image_list);

	/* update model */
	gnome_list_model_interval_added (GNOME_LIST_MODEL (model), 
					 old_length, 
					 new_length-old_length);
}


/**
 * eog_image_list_model_has_images:
 * @model: Image list model.
 *
 * Queries the model if it contains any images.
 *
 * Return value: TRUE if there is at least one image, else FALSE.
 **/
gboolean 
eog_image_list_model_has_images (EogImageListModel *model)
{
	GList *image_list;
	
	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (IS_EOG_IMAGE_LIST_MODEL (model), FALSE);
	
	image_list = model->priv->image_list;

	return (g_list_length (image_list) != 0);
}


CImage*
eog_image_list_model_next_image_to_load (EogImageListModel *model) 
{
        GList *iterator;

        g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (IS_EOG_IMAGE_LIST_MODEL (model), NULL);

        iterator = model->priv->image_list;

        while (iterator) {
                if (!cimage_has_thumbnail (CIMAGE (iterator->data)) &&
                    !cimage_has_loading_failed (CIMAGE (iterator->data))) {
                        break;
                }
                iterator = iterator->next;
        }
        
        if (iterator) {
                gtk_object_ref (GTK_OBJECT (iterator->data));
                return CIMAGE (iterator->data);
        } else {
                return NULL;
        }
}
