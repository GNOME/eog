/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-collection-view.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000 SuSE GmbH.
 * Copyright 2001 The Free Software Foundation
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

enum {
	PROP_WINDOW_TITLE,
	PROP_WINDOW_STATUS,
	PROP_LAST
};

static const gchar *property_name[] = {
	"window/title", 
	"window/status"
};

struct _EogCollectionViewPrivate {
	EogCollectionModel      *model;
	EogItemFactory          *factory;

	GtkWidget               *wraplist;
	GtkWidget               *root;

	BonoboPropertyBag       *property_bag;

	BonoboUIComponent       *uic;
	BonoboPropertyControl   *prop_control;

	gint                    idle_id;
	gboolean                need_update_prop[PROP_LAST];
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
impl_GNOME_EOG_ImageCollection_openURI (PortableServer_Servant servant,
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
impl_GNOME_EOG_ImageCollection_openURIList (PortableServer_Servant servant,
					    const GNOME_EOG_URIList *uri_list,
					    CORBA_Environment * ev)
{
	EogCollectionView *cview;
	EogCollectionViewPrivate *priv;
        GList *list;
        guint i;

	cview = EOG_COLLECTION_VIEW (bonobo_object_from_servant (servant));
	priv = cview->priv;
	
        list = NULL;
#ifdef COLLECTION_DEBUG
	g_print ("uri_list->_length: %i\n", uri_list->_length);
#endif
        for (i = 0; i < uri_list->_length; i++) {
                list = g_list_append
                        (list, g_strdup (uri_list->_buffer[i]));
        }
        
	eog_collection_model_set_uri_list (priv->model, list);

	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);
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

	if (list_view->priv->property_bag)
		bonobo_object_unref (BONOBO_OBJECT (list_view->priv->property_bag));
	list_view->priv->property_bag = NULL;

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
	epv->openURIList = impl_GNOME_EOG_ImageCollection_openURIList;

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
eog_collection_view_init (EogCollectionView *view)
{
	view->priv = g_new0 (EogCollectionViewPrivate, 1);
	view->priv->idle_id = -1;
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
eog_collection_view_get_prop (BonoboPropertyBag *bag,
			      BonoboArg         *arg,
			      guint              arg_id,
			      CORBA_Environment *ev,
			      gpointer           user_data)
{
	EogCollectionView *view;
	EogCollectionViewPrivate *priv;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);
	priv = view->priv;
	
	switch (arg_id) {
	case PROP_WINDOW_TITLE: {
		gchar *base_uri;
		gchar *title;

		base_uri = eog_collection_model_get_base_uri (priv->model);

		if (base_uri == NULL)
			title = g_strdup (_("Collection View"));
		else {
			if (g_strncasecmp ("file:", base_uri, 5) == 0)
				title = g_strdup ((base_uri+5*sizeof(guchar)));
			else
				title = g_strdup (base_uri);
		}

		BONOBO_ARG_SET_STRING (arg, title);
		g_free (title);
		break;
	}
	case PROP_WINDOW_STATUS: {
		gchar *str;
		gint nimg, nsel;
			
		nimg = eog_collection_model_get_length (priv->model);
		nsel = eog_collection_model_get_selected_length (priv->model);

		str = g_new0 (guchar, 70);

		if (nsel == 0)
			g_snprintf (str, 70, "Images: %i", nimg);
		else if (nsel == 1) {
			CImage *img; 
			gchar *uri;

			img = eog_collection_model_get_selected_image (priv->model);
			uri = cimage_get_uri (img);
			g_snprintf (str, 70, "Images: %i  %s (%i x %i)", nimg,
				    g_basename (uri),
				    cimage_get_width (img),
				    cimage_get_height (img));
			g_free (uri);
		} else
			g_snprintf (str, 70, "Images: %i  Selected: %i", nimg, nsel);
	       
		BONOBO_ARG_SET_STRING (arg, str);
		g_free (str);
		break;
	}
	default:
		g_assert_not_reached ();
	}
}

static void
eog_collection_view_set_prop (BonoboPropertyBag *bag,
			      const BonoboArg   *arg,
			      guint              arg_id,
			      CORBA_Environment *ev,
			      gpointer           user_data)
{
	EogCollectionView *view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);

	switch (arg_id) {
		/* all properties are read only yet */
	default:
		g_assert_not_reached ();
	}
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



static gint
update_properties (EogCollectionView *view)
{
	EogCollectionViewPrivate *priv;
	BonoboArg *arg;
	gint p;

	g_return_val_if_fail (view != NULL, FALSE);

	priv = view->priv;

	arg = bonobo_arg_new (BONOBO_ARG_STRING);

	for (p = 0; p < PROP_LAST; p++) {
		if (priv->need_update_prop[p]) {
			eog_collection_view_get_prop (NULL, arg,
						      p, NULL,
						      view);
		
			g_print ("notify %s listners.\n", property_name[p]);
			bonobo_property_bag_notify_listeners (priv->property_bag,
							      property_name[p],
							      arg, NULL);
			priv->need_update_prop[p] = FALSE;
		}
	}

	bonobo_arg_release (arg);
	priv->idle_id = -1;

	return FALSE;
}

static void 
update_status_text (EogCollectionView *view)
{
	view->priv->need_update_prop [PROP_WINDOW_STATUS] = TRUE;
	if (view->priv->idle_id == -1) {
		view->priv->idle_id = gtk_idle_add ((GtkFunction) update_properties, view);
	}	
}

static void 
update_title_text (EogCollectionView *view)
{
	view->priv->need_update_prop [PROP_WINDOW_TITLE] = TRUE;
	if (view->priv->idle_id == -1) {
		view->priv->idle_id = gtk_idle_add ((GtkFunction) update_properties, view);
	}	
}

static void
model_size_changed (EogCollectionModel *model, GList *id_list,  gpointer data)
{
	EogCollectionView *view;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	view = EOG_COLLECTION_VIEW (data);
	update_status_text (view);
}

static void
model_selection_changed (EogCollectionModel *model, gpointer data)
{
	EogCollectionView *view;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	view = EOG_COLLECTION_VIEW (data);
	update_status_text (view);
}

static void
model_base_uri_changed (EogCollectionModel *model, gpointer data)
{
	EogCollectionView *view;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	view = EOG_COLLECTION_VIEW (data);
	g_print ("model_base_uri_changed ...\n");
	update_title_text (view);	
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
	g_assert (EOG_IS_COLLECTION_VIEW (list_view));
	priv->model = eog_collection_model_new ();
	gtk_signal_connect (GTK_OBJECT (priv->model), "interval_added", 
			    model_size_changed,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (priv->model), "interval_removed", 
			    model_size_changed,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (priv->model), "selection_changed",
			    model_selection_changed,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (priv->model), "base_uri_changed",
			    model_base_uri_changed,
			    list_view);

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

	/* Property Bag */
	priv->property_bag = bonobo_property_bag_new (eog_collection_view_get_prop,
						      eog_collection_view_set_prop,
						      list_view);
	bonobo_property_bag_add (priv->property_bag, property_name[0], PROP_WINDOW_TITLE,
				 BONOBO_ARG_STRING, NULL, _("Window Title"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, property_name[1], PROP_WINDOW_STATUS,
				 BONOBO_ARG_STRING, NULL, _("Status Text"),
				 BONOBO_PROPERTY_READABLE);

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

BonoboPropertyBag*
eog_collection_view_get_property_bag (EogCollectionView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (view), NULL);

	return view->priv->property_bag;
}
