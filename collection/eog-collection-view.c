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
#include <gtk/gtkwindow.h>
#include <gtk/gtkpaned.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <bonobo/bonobo-macros.h>
#include <bonobo/bonobo-persist-file.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-zoomable.h>

#include "eog-wrap-list.h"
#include "eog-scroll-view.h"
#include "eog-collection-view.h"
#include "eog-collection-model.h"
#include "eog-collection-marshal.h"
/* #include "eog-slide-show.h" */

enum {
	PROP_WINDOW_TITLE,
	PROP_WINDOW_STATUS,
	PROP_LAST
};

static const gchar *property_name[] = {
	"window/title", 
	"window/status"
};

enum {
	PREF_LAYOUT,
	PREF_COLOR,
	PREF_LAST
};

#define PREF_PREFIX  "/apps/eog/collection"
static const gchar *pref_key[] = {
	PREF_PREFIX"/layout",
	PREF_PREFIX"/color"
};

struct _EogCollectionViewPrivate {
	EogCollectionModel      *model;

	GtkWidget               *wraplist;
	GtkWidget               *scroll_view;

	BonoboPropertyBag       *property_bag;
	BonoboZoomable          *zoomable;

	BonoboUIComponent       *uic;

	GConfClient             *client;
	guint                   notify_id[PREF_LAST];

	gint                    idle_id;
	gboolean                need_update_prop[PROP_LAST];
};


enum {
	OPEN_URI,
	LAST_SIGNAL
};

static guint eog_collection_view_signals [LAST_SIGNAL];

BONOBO_CLASS_BOILERPLATE (EogCollectionView, eog_collection_view,
			  BonoboPersistFile, BONOBO_TYPE_PERSIST_FILE);


static void
verb_SlideShow_cb (BonoboUIComponent *uic, 
		   gpointer user_data,
		   const char *cname)
{
#if 0
	EogCollectionView *view;
	GtkWidget *show;

	view = EOG_COLLECTION_VIEW (user_data);

	show = eog_slide_show_new (view->priv->model);
	gtk_widget_show (show);
#endif
}


static BonoboUIVerb collection_verbs[] = {
	BONOBO_UI_VERB ("SlideShow", verb_SlideShow_cb),
	BONOBO_UI_VERB_END
};

static void
eog_collection_view_create_ui (EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	/* Set up the UI from an XML file. */
        bonobo_ui_util_set_ui (view->priv->uic, DATADIR,
			       "eog-collection-view-ui.xml", "EogCollectionView", NULL);

	bonobo_ui_component_add_verb_list_with_data (view->priv->uic,
						     collection_verbs,
						     view);
}

void
eog_collection_view_set_ui_container (EogCollectionView      *list_view,
				      Bonobo_UIContainer ui_container)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	bonobo_ui_component_set_container (list_view->priv->uic, ui_container, NULL);

	eog_collection_view_create_ui (list_view);
}

void
eog_collection_view_unset_ui_container (EogCollectionView *list_view)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));

	bonobo_ui_component_unset_container (list_view->priv->uic, NULL);
}

static void
control_activate_cb (BonoboControl *object, gboolean state, gpointer data)
{
	EogCollectionView *view;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	view = EOG_COLLECTION_VIEW (data);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (object, NULL);
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_collection_view_set_ui_container (view, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}

	} else
		eog_collection_view_unset_ui_container (view);
}



static void
eog_collection_view_dispose (GObject *object)
{
	EogCollectionView *list_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (object));

	list_view = EOG_COLLECTION_VIEW (object);

	if (list_view->priv->model)
		g_object_unref (G_OBJECT (list_view->priv->model));
	list_view->priv->model = NULL;

	if (list_view->priv->property_bag)
		bonobo_object_unref (BONOBO_OBJECT (list_view->priv->property_bag));
	list_view->priv->property_bag = NULL;

	if (list_view->priv->client)
		g_object_unref (G_OBJECT (list_view->priv->client));
	list_view->priv->client = NULL;

	if (list_view->priv->uic)
		bonobo_object_unref (BONOBO_OBJECT (list_view->priv->uic));
	list_view->priv->uic = NULL;

	list_view->priv->wraplist = NULL;
	list_view->priv->scroll_view = NULL;

	BONOBO_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_collection_view_finalize (GObject *object)
{
	EogCollectionView *list_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (object));

	list_view = EOG_COLLECTION_VIEW (object);

	g_free (list_view->priv);

	BONOBO_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_collection_view_class_init (EogCollectionViewClass *klass)
{
	GObjectClass *object_class = (GObjectClass *)klass;

	object_class->dispose = eog_collection_view_dispose;
	object_class->finalize = eog_collection_view_finalize;
}

static void
eog_collection_view_instance_init (EogCollectionView *view)
{
	view->priv = g_new0 (EogCollectionViewPrivate, 1);
	view->priv->idle_id = -1;
	view->priv->model = NULL;
}

static void
kill_popup_menu (GtkWidget *widget, GtkMenu *menu)
{
	g_return_if_fail (GTK_IS_MENU (menu));

	g_object_unref (G_OBJECT (menu));
}

static gboolean
handle_right_click (EogWrapList *wlist, gint n, GdkEvent *event, 
		    EogCollectionView *view)
{
#if 0
	GtkWidget *menu, *item, *label;

	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (view), FALSE);

	menu = gtk_menu_new ();
	g_signal_connect (G_OBJECT (menu), "hide",
			  G_CALLBACK (kill_popup_menu), menu);


	label = gtk_label_new (_("Move to Trash"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);

	item = gtk_menu_item_new ();
	gtk_widget_show (item);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (handle_delete_activate), view);
	gtk_container_add (GTK_CONTAINER (item), label);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);

	return (TRUE);
#endif
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
		nsel = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));

		str = g_new0 (guchar, 70);

		g_snprintf (str, 70, "Images: %i/%i", nsel, nimg);
	       
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
			bonobo_event_source_notify_listeners (priv->property_bag->es,
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
handle_selection_changed (EogWrapList *list, EogCollectionView *view)
{
	EogImage *image;
	EogCollectionViewPrivate *priv;

	image = eog_wrap_list_get_first_selected_image (list);

	priv = view->priv;

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->scroll_view), image);
	
	if (image != NULL) 
		g_object_unref (image);
	
	update_status_text (view);
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
model_base_uri_changed (EogCollectionModel *model, gpointer data)
{
	EogCollectionView *view;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	view = EOG_COLLECTION_VIEW (data);
	g_print ("model_base_uri_changed ...\n");
	update_title_text (view);	
}

static void
layout_changed_cb (GConfClient *client, guint cnxn_id, 
		   GConfEntry *entry, gpointer user_data)
{
	EogCollectionView *view;
	gint layout;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);

	layout = gconf_value_get_int (entry->value);
	eog_wrap_list_set_layout_mode (EOG_WRAP_LIST (view->priv->wraplist), 
				      layout);
}

/* read configuration */
static void
init_gconf_defaults (EogCollectionView *view)
{
	EogCollectionViewPrivate *priv = NULL;
	gint layout;
	GSList *l;
	GdkColor color;

	g_return_if_fail (view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));
	
	priv = view->priv;
#if 0

	/* Make sure GConf is initialized */
	if (!gconf_is_initialized ())
		gconf_init (0, NULL, NULL);
	
	priv->client = gconf_client_get_default ();
	gconf_client_add_dir (priv->client, PREF_PREFIX, 
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	
	/* set layout mode */
	layout = gconf_client_get_int (priv->client, 
				       pref_key[PREF_LAYOUT],
				       NULL);
	eog_wrap_list_set_layout_mode (EOG_WRAP_LIST (priv->wraplist),
				       layout);
	
	/* add configuration listeners */
	priv->notify_id[PREF_LAYOUT] = 
		gconf_client_notify_add (priv->client, 
					 pref_key[PREF_LAYOUT],
					 layout_changed_cb,
					 view,
					 NULL, NULL);
#endif
}

static gint
load_uri_cb (BonoboPersistFile *pf, const CORBA_char *text_uri,
	     CORBA_Environment *ev, void *closure)
{
	EogCollectionViewPrivate *priv;
	
	priv = EOG_COLLECTION_VIEW (closure)->priv;

	if (text_uri == CORBA_OBJECT_NIL) return;

	if (priv->model == NULL) {
		priv->model = eog_collection_model_new ();
		eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), priv->model);

		/* construct widget */
#if 0
		g_signal_connect (G_OBJECT (priv->model), "image-added", 
				  G_CALLBACK (model_size_changed),
				  list_view);
		g_signal_connect (G_OBJECT (priv->model), "image-removed", 
				  G_CALLBACK (model_size_changed),
				  list_view);
		g_signal_connect (G_OBJECT (priv->model), "selection-changed",
				  G_CALLBACK (model_selection_changed),
				  list_view);
		g_signal_connect (G_OBJECT (priv->model), "base-uri-changed",
				  G_CALLBACK (model_base_uri_changed),
				  list_view);
#endif

	}

	g_print ("load_uri_cb: %s\n", (char*) text_uri);

	eog_collection_model_add_uri (priv->model, (gchar*)text_uri); 

	return 0;
}

static GtkWidget*
create_user_interface (EogCollectionView *list_view)
{
	EogCollectionViewPrivate *priv;
	GtkWidget *paned;
	GtkWidget *sw;

	priv = list_view->priv;

	/* the image view for the full size image */
 	priv->scroll_view = eog_scroll_view_new ();

	/* the wrap list for all the thumbnails */
	priv->wraplist = eog_wrap_list_new ();
	priv->model = eog_collection_model_new ();
	eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), priv->model);
	eog_wrap_list_set_col_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	eog_wrap_list_set_row_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	g_signal_connect (G_OBJECT (priv->wraplist), "selection_changed",
			  G_CALLBACK (handle_selection_changed), list_view);
/*
	g_signal_connect (G_OBJECT (priv->wraplist), "double_click", 
			  G_CALLBACK (handle_double_click), list_view);
	g_signal_connect (G_OBJECT (priv->wraplist), "right_click",
			  G_CALLBACK (handle_right_click), list_view);
*/

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (sw), priv->wraplist);

	/* put it all together */
	paned = gtk_vpaned_new ();
	
	gtk_paned_pack1 (GTK_PANED (paned), priv->scroll_view, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (paned), sw, TRUE, TRUE);

	gtk_widget_show_all (paned);
	gtk_widget_show_all (sw);


	return paned;
}


EogCollectionView *
eog_collection_view_construct (EogCollectionView *list_view)
{
	EogCollectionViewPrivate *priv = NULL;
	BonoboControl *control;
	BonoboZoomable *zoomable;
	GtkWidget *root;

	g_return_val_if_fail (list_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (list_view), NULL);
	
	priv = list_view->priv;
	
	root = create_user_interface (list_view);

	bonobo_persist_file_construct (BONOBO_PERSIST_FILE (list_view),
				       load_uri_cb, NULL,
				       "OAFIID:GNOME_EOG_CollectionControl",
				       list_view);

	/* interface Bonobo::Control */
	control = bonobo_control_new (root);
	g_signal_connect (control, "activate", G_CALLBACK (control_activate_cb), list_view);

	bonobo_object_add_interface (BONOBO_OBJECT (list_view),
				     BONOBO_OBJECT (control));


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
	bonobo_control_set_properties (BONOBO_CONTROL (control), 
				       BONOBO_OBJREF (priv->property_bag),
				       NULL);
	
	/* read user defined configuration */
	init_gconf_defaults (list_view);

	/* UI Component */
	priv->uic = bonobo_ui_component_new ("EogCollectionView");

	bonobo_object_dump_interfaces (BONOBO_OBJECT (list_view));

	return list_view;
}

EogCollectionView *
eog_collection_view_new (void)
{
	EogCollectionView *list_view;

	list_view = 
		EOG_COLLECTION_VIEW (g_object_new (EOG_TYPE_COLLECTION_VIEW, NULL));

	return eog_collection_view_construct (list_view);
}

