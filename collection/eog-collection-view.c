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

#include "eog-item-factory-simple.h"
#include "eog-wrap-list.h"
#include "eog-collection-view.h"
#include "eog-collection-model.h"
#include "eog-collection-preferences.h"

struct _EogCollectionViewPrivate {
	EogCollectionModel      *model;
	EogItemFactory          *factory;

	GtkWidget               *wraplist;
	GtkWidget               *root;

	BonoboUIComponent       *uic;
	BonoboPropertyControl   *prop_control;
};

enum {
	PROP_CONTROL_TITLE
};

enum {
	OPEN_URI,
	LAST_SIGNAL
};

static guint eog_collection_view_signals [LAST_SIGNAL];

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static BonoboObjectClass *eog_collection_view_parent_class;

static void 
impl_GNOME_EOG_ImageCollection_openURI(PortableServer_Servant servant,
				       GNOME_EOG_URI uri,
				       CORBA_Environment * ev)
{
	EogCollectionView *cview;
	EogCollectionViewPrivate *priv;

	cview = EOG_COLLECTION_VIEW (bonobo_object_from_servant (servant));
	priv = cview->priv;

	if (uri == CORBA_OBJECT_NIL) return;

	eog_collection_model_set_uri (priv->model, (gchar*)uri); 
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

	if (list_view->priv->model)
		gtk_object_unref (GTK_OBJECT (list_view->priv->model));
	list_view->priv->model = NULL;

	if (list_view->priv->factory)
		gtk_object_unref (GTK_OBJECT (list_view->priv->factory));
	list_view->priv->factory = NULL;

	if (list_view->priv->prop_control)
		bonobo_object_unref (BONOBO_OBJECT (list_view->priv->prop_control));
	list_view->priv->prop_control = NULL;

	list_view->priv->wraplist = NULL;
	list_view->priv->root = NULL;

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

	if(GTK_OBJECT_CLASS (eog_collection_view_parent_class)->finalize)
		GTK_OBJECT_CLASS (eog_collection_view_parent_class)->finalize (object);
}

static void
eog_collection_view_class_init (EogCollectionViewClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	POA_GNOME_EOG_ImageCollection__epv *epv;

	eog_collection_view_parent_class = gtk_type_class (PARENT_TYPE);

	gtk_object_class_add_signals (object_class, eog_collection_view_signals, LAST_SIGNAL);

	object_class->destroy = eog_collection_view_destroy;
	object_class->finalize = eog_collection_view_finalize;

	epv = &klass->epv;
	epv->openURI = impl_GNOME_EOG_ImageCollection_openURI;

	eog_collection_view_signals [OPEN_URI] = 
		gtk_signal_new ("open_uri",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogCollectionViewClass, open_uri),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, eog_collection_view_signals, LAST_SIGNAL);

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


static void
handle_item_dbl_click (GtkObject *obj, gint n, gpointer data)
{
	EogCollectionView *view;
	gchar *uri;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));
	
	view = EOG_COLLECTION_VIEW (data);

	uri = eog_collection_model_get_uri (view->priv->model, n);
	if (uri == NULL) return;

	gtk_signal_emit (GTK_OBJECT (view), eog_collection_view_signals [OPEN_URI], uri);

	g_free (uri);
}

static void
prop_control_get_prop (BonoboPropertyBag *bag,
		       BonoboArg         *arg,
		       guint              arg_id,
		       CORBA_Environment *ev,
		       gpointer           user_data)
{
	switch (arg_id) {
	case 0:
		g_assert (arg->_type == BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, _("Layout"));
		break;
	case 1:
		g_assert (arg->_type == BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, _("Color"));
		break;
		
	default:
		g_assert_not_reached ();
	}
}

static BonoboControl *
prop_control_get_cb (BonoboPropertyControl *property_control,
		     int page_number, void *closure)
{
	EogCollectionView *cview;
	GtkWidget *widget;
	BonoboControl *control;
	BonoboPropertyBag *property_bag;

	g_return_val_if_fail (closure != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (closure), NULL);

	cview = EOG_COLLECTION_VIEW (closure);

	/* create widget */
	widget = eog_collection_preferences_create_page (cview, page_number);

	control = bonobo_control_new (widget);

	/* Property Bag */
	property_bag = bonobo_property_bag_new (prop_control_get_prop,
						NULL, control);

	bonobo_property_bag_add (property_bag, "bonobo:title",
				 page_number, BONOBO_ARG_STRING,
				 NULL, NULL, BONOBO_PROPERTY_READABLE);

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (property_bag));

	return control;
}

EogCollectionView *
eog_collection_view_construct (EogCollectionView       *list_view)
{
	EogCollectionViewPrivate *priv;

	g_return_val_if_fail (list_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (list_view), NULL);
	
	list_view->priv->uic = bonobo_ui_component_new ("EogCollectionView");

	/* construct widget */
	priv = list_view->priv;
	priv->model = eog_collection_model_new ();

	priv->factory = EOG_ITEM_FACTORY (eog_item_factory_simple_new ());

	priv->root = gtk_scrolled_window_new (NULL, NULL);

	priv->wraplist = eog_wrap_list_new ();
	gtk_container_add (GTK_CONTAINER (priv->root), priv->wraplist);
	eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), priv->model);
	eog_wrap_list_set_factory (EOG_WRAP_LIST (priv->wraplist),
				   EOG_ITEM_FACTORY (priv->factory));
	eog_wrap_list_set_col_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	eog_wrap_list_set_row_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	gtk_signal_connect (GTK_OBJECT (priv->wraplist), "item_dbl_click", 
			    handle_item_dbl_click, list_view);

	gtk_widget_show (priv->wraplist);
	gtk_widget_show (priv->root);

	/* Property Control */
	priv->prop_control = bonobo_property_control_new (prop_control_get_cb, 2, 
							  list_view);
	bonobo_object_add_interface (BONOBO_OBJECT (list_view),
				     BONOBO_OBJECT (priv->prop_control));

	return list_view;
}

EogCollectionView *
eog_collection_view_new (void)
{
	EogCollectionView *list_view;

	list_view = gtk_type_new (eog_collection_view_get_type ());

	return eog_collection_view_construct (list_view);
}


void                
eog_collection_view_set_layout_mode    (EogCollectionView *list_view,
					EogLayoutMode lm)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));

	eog_wrap_list_set_layout_mode (EOG_WRAP_LIST (list_view->priv->wraplist),
				       lm);
}

void                
eog_collection_view_set_background_color (EogCollectionView *list_view,
					  GdkColor *color)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));

	eog_wrap_list_set_background_color (EOG_WRAP_LIST (list_view->priv->wraplist),
					    color);
}


