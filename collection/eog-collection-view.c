/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-collection-view.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000 SuSE GmbH.
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtklabel.h>

#include <gnome.h>

#include "gnome-icon-item-factory.h"
#include "gnome-wrap-list.h"
#include "eog-image-list-model.h"
#include "eog-image-selection-model.h"
#include "eog-collection-view.h"
#include "eog-image-loader.h"

struct _EogCollectionViewPrivate {
	GnomeListModel          *model;
	GnomeListSelectionModel *sel_model;
	GnomeIconItemFactory    *factory;
	EogImageLoader          *loader;

	GtkWidget               *wraplist;
	GtkWidget               *root;

	BonoboUIComponent       *uic;
};

enum {
	LAST_SIGNAL
};


static guint eog_collection_view_signals [LAST_SIGNAL];

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static BonoboObjectClass *eog_collection_view_parent_class;

static void 
impl_GNOME_EOG_ImageCollection_addImages(PortableServer_Servant servant,
				       const GNOME_EOG_Files * list,
				       CORBA_Environment * ev)
{
	GList *image_list = NULL;
	int i;
	EogCollectionView *cview;
	EogCollectionViewPrivate *priv;
	
	cview = EOG_COLLECTION_VIEW (bonobo_object_from_servant (servant));
	priv = cview->priv;

	if (list == CORBA_OBJECT_NIL) return;
	
	for (i = 0; i < list->_length; i++) {
		CImage *img;

		img = cimage_new ((char*)list->_buffer[i]);
		
		image_list = g_list_append (image_list, img);
	}

	eog_image_list_model_add_images (EOG_IMAGE_LIST_MODEL (priv->model),
					 image_list);
	eog_image_loader_start (priv->loader);
}


static void
eog_collection_view_create_ui (EogCollectionView *list_view)
{
	/* Currently we have no additinal user interface. */
}

void
eog_collection_view_set_ui_container (EogCollectionView      *list_view,
				      Bonobo_UIContainer ui_container)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	bonobo_ui_component_set_container (list_view->priv->uic, ui_container);

	eog_collection_view_create_ui (list_view);
}

void
eog_collection_view_unset_ui_container (EogCollectionView *list_view)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));

	bonobo_ui_component_unset_container (list_view->priv->uic);
}

GtkWidget *
eog_collection_view_get_widget (EogCollectionView *list_view)
{
	g_return_val_if_fail (list_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (list_view), NULL);

	gtk_widget_ref (list_view->priv->root);

	return list_view->priv->root;
}

static void
eog_collection_view_destroy (GtkObject *object)
{
	EogCollectionView *list_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (object));

	list_view = EOG_COLLECTION_VIEW (object);

	if (list_view->priv->wraplist)
		gtk_widget_unref (list_view->priv->wraplist);
	list_view->priv->wraplist = NULL;

	GTK_OBJECT_CLASS (eog_collection_view_parent_class)->destroy (object);
}

static void
eog_collection_view_finalize (GtkObject *object)
{
	EogCollectionView *list_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (object));

	list_view = EOG_COLLECTION_VIEW (object);

	g_free (list_view->priv);

	GTK_OBJECT_CLASS (eog_collection_view_parent_class)->finalize (object);
}

static void
eog_collection_view_class_init (EogCollectionViewClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	POA_GNOME_EOG_ImageCollection__epv *epv;

	eog_collection_view_parent_class = gtk_type_class (bonobo_object_get_type ());

	gtk_object_class_add_signals (object_class, eog_collection_view_signals, LAST_SIGNAL);

	object_class->destroy = eog_collection_view_destroy;
	object_class->finalize = eog_collection_view_finalize;

	epv = &klass->epv;
	epv->addImages = impl_GNOME_EOG_ImageCollection_addImages;
}

static void
eog_collection_view_init (EogCollectionView *image_view)
{
	image_view->priv = g_new0 (EogCollectionViewPrivate, 1);
}


BONOBO_X_TYPE_FUNC_FULL (EogCollectionView, 
			 GNOME_EOG_ImageCollection,
			 PARENT_TYPE,
			 eog_collection_view);


EogCollectionView *
eog_collection_view_construct (EogCollectionView       *list_view)
{
	EogCollectionViewPrivate *priv;
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;

	g_return_val_if_fail (list_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (list_view), NULL);
	
	list_view->priv->uic = bonobo_ui_component_new ("EogCollectionView");

	/* construct widget */
	priv = list_view->priv;
	priv->model = GNOME_LIST_MODEL (eog_image_list_model_new ());

        priv->sel_model = GNOME_LIST_SELECTION_MODEL (eog_image_selection_model_new ());
	priv->loader = eog_image_loader_new (100, 100);
	eog_image_loader_set_model (priv->loader, EOG_IMAGE_LIST_MODEL (priv->model));

	priv->root = gtk_scrolled_window_new (NULL, NULL);

	priv->wraplist = gtk_type_new(GNOME_TYPE_WRAP_LIST);
	gtk_container_add (GTK_CONTAINER (priv->root), priv->wraplist);
	gnome_list_view_set_model (GNOME_LIST_VIEW (priv->wraplist), priv->model);
	gnome_list_view_set_selection_model (GNOME_LIST_VIEW (priv->wraplist), priv->sel_model);
	gnome_wrap_list_set_mode (GNOME_WRAP_LIST (priv->wraplist), GNOME_WRAP_LIST_ROW_MAJOR);
	gnome_wrap_list_set_item_size (GNOME_WRAP_LIST (priv->wraplist), 120, 120 );
	gnome_wrap_list_set_row_spacing (GNOME_WRAP_LIST (priv->wraplist), 20);
	gnome_wrap_list_set_col_spacing (GNOME_WRAP_LIST (priv->wraplist), 20);
	gnome_wrap_list_set_shadow_type (GNOME_WRAP_LIST (priv->wraplist), GTK_SHADOW_IN);
	gnome_wrap_list_set_use_unit_scrolling (GNOME_WRAP_LIST (priv->wraplist), TRUE);

	priv->factory = gtk_type_new (GNOME_TYPE_ICON_ITEM_FACTORY);
	gnome_icon_item_factory_set_item_metrics (priv->factory, 120, 120, 100, 100);
	gnome_list_view_set_list_item_factory (GNOME_LIST_VIEW (priv->wraplist),
					       GNOME_LIST_ITEM_FACTORY (priv->factory));
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->root), 
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->root));
	vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->root));
	gtk_widget_set_scroll_adjustments (GTK_WIDGET (priv->wraplist), hadj, vadj);
	
	gtk_widget_show (priv->wraplist);
	gtk_widget_show (priv->root);


	return list_view;
}

EogCollectionView *
eog_collection_view_new (void)
{
	EogCollectionView *list_view;

	list_view = gtk_type_new (eog_collection_view_get_type ());

	return eog_collection_view_construct (list_view);
}
