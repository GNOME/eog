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
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gconf/gconf-client.h>
#include <bonobo/bonobo-macros.h>

#ifdef ENABLE_EVOLUTION
#  include "Evolution-Composer.h"
#endif

#if HAVE_LIBPREVIEW
#include "eog-image-loader-preview.h"
#else
#include "eog-image-loader-simple.h"
#endif

#include "eog-item-factory-clean.h"
#include "eog-wrap-list.h"
#include "eog-collection-view.h"
#include "eog-collection-model.h"
#include "eog-collection-preferences.h"
#include "eog-collection-marshal.h"

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
	GtkWidget               *root;

	BonoboPropertyBag       *property_bag;

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

static void eog_collection_view_dispose (GObject *object);

BONOBO_CLASS_BOILERPLATE_FULL (EogCollectionView, eog_collection_view,
			       GNOME_EOG_ImageCollection,
			       BonoboObject, BONOBO_TYPE_OBJECT);


static void 
impl_GNOME_EOG_ImageCollection_openURI (PortableServer_Servant servant,
					const CORBA_char * uri,
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
eog_collection_view_create_ui (EogCollectionView *view)
{
	/* Currently we have no additional user interface. */
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

GtkWidget *
eog_collection_view_get_widget (EogCollectionView *list_view)
{
	g_return_val_if_fail (list_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (list_view), NULL);

	gtk_widget_ref (list_view->priv->root);

	return list_view->priv->root;
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
	list_view->priv->root = NULL;

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
	POA_GNOME_EOG_ImageCollection__epv *epv;

	object_class->dispose = eog_collection_view_dispose;
	object_class->finalize = eog_collection_view_finalize;

	epv = &klass->epv;
	epv->openURI = impl_GNOME_EOG_ImageCollection_openURI;
	epv->openURIList = impl_GNOME_EOG_ImageCollection_openURIList;

	eog_collection_view_signals [OPEN_URI] = 
		g_signal_new ("open_uri",
			      G_TYPE_FROM_CLASS(object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogCollectionViewClass, open_uri),
			      NULL,
			      NULL,
			      eog_collection_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
eog_collection_view_instance_init (EogCollectionView *view)
{
	view->priv = g_new0 (EogCollectionViewPrivate, 1);
	view->priv->idle_id = -1;
}

static void
handle_double_click (EogWrapList *wlist, gint n, EogCollectionView *view)
{
	gchar *uri;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));
	
	uri = eog_collection_model_get_uri (view->priv->model, n);
	if (uri == NULL) return;

	g_signal_emit (G_OBJECT (view), eog_collection_view_signals [OPEN_URI], 0, uri);

	g_free (uri);
}

static gboolean
delete_item (EogCollectionModel *model, CImage *image, gpointer user_data)
{
	GnomeVFSURI *uri, *trash = NULL, *path;
	GnomeVFSResult result = GNOME_VFS_OK;
	GtkWidget *window;
	const gchar *msg;
	EogCollectionView *view;

	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (user_data), FALSE);
	view = EOG_COLLECTION_VIEW (user_data);

	/* If the image isn't selected, do nothing */
	if (!cimage_is_selected (image))
		return (TRUE);

	uri = cimage_get_uri (image);
	result = gnome_vfs_find_directory (uri,
					GNOME_VFS_DIRECTORY_KIND_TRASH,
					&trash, FALSE, FALSE, 0777);
	if (result == GNOME_VFS_OK) {
		path = gnome_vfs_uri_append_file_name (trash, 
				gnome_vfs_uri_get_path (uri));
		gnome_vfs_uri_unref (trash);
		result = gnome_vfs_move_uri (uri, path, TRUE);
		gnome_vfs_uri_unref (path);
		if (result == GNOME_VFS_OK)
			eog_collection_model_remove_item (model, cimage_get_unique_id (image));
	}
	gnome_vfs_uri_unref (uri);

	if (result != GNOME_VFS_OK) {
		msg = gnome_vfs_result_to_string (result);
		window = gtk_widget_get_ancestor (
			GTK_WIDGET (view->priv->root), GTK_TYPE_WINDOW);
		if (window)
			gnome_error_dialog_parented (msg, GTK_WINDOW (window));
		else
			gnome_error_dialog (msg);
		return (FALSE);
	}

	return (TRUE);
}

static void
handle_delete_activate (GtkMenuItem *item, EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	eog_collection_model_foreach (view->priv->model, delete_item, view);
}

#ifdef ENABLE_EVOLUTION
static gboolean
send_item (EogCollectionModel *model, guint n, gpointer user_data)
{
	Bonobo_StorageInfo *info;
	Bonobo_Stream stream;
	Bonobo_Stream_iobuf *buffer;
	CORBA_Object composer;
	GNOME_Evolution_Composer_AttachmentData *attachment_data;
	CORBA_Environment ev;
	gchar *uri;
	CImage *image;

	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), FALSE);
	composer = user_data;

	image = eog_collection_model_get_image (model, n);
	
	/* If the image isn't selected, do nothing */
	if (!cimage_is_selected (image))
		return (TRUE);

	CORBA_exception_init (&ev);

	uri = eog_collection_model_get_uri (model, n);
	stream = bonobo_get_object (uri, "IDL:Bonobo/Stream:1.0", &ev);
	g_free (uri);
	if (BONOBO_EX (&ev)) {
		g_warning ("Could not load file: %s",
			   bonobo_exception_get_text (&ev));
		return (FALSE);
	}

	info = Bonobo_Stream_getInfo (stream, Bonobo_FIELD_CONTENT_TYPE |
					      Bonobo_FIELD_SIZE, &ev);
	if (BONOBO_EX (&ev)) {
		bonobo_object_release_unref (stream, NULL);
		g_warning ("Could not get info about stream: %s",
			   bonobo_exception_get_text (&ev));
		return (FALSE);
	}

	Bonobo_Stream_read (stream, info->size, &buffer, &ev);
	bonobo_object_release_unref (stream, NULL);
	if (BONOBO_EX (&ev)) {
		CORBA_free (info);
		g_warning ("Could not read stream: %s",
			   bonobo_exception_get_text (&ev));
		return (FALSE);
	}

	attachment_data = GNOME_Evolution_Composer_AttachmentData__alloc ();
	attachment_data->_buffer = buffer->_buffer;
	attachment_data->_length = buffer->_length;
	GNOME_Evolution_Composer_attachData (composer, info->content_type,
					     info->name, info->name, FALSE,
					     attachment_data, &ev);
	CORBA_free (info);
	CORBA_free (attachment_data);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to attach image: %s", 
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return (FALSE);
	}
	
	CORBA_exception_free (&ev);

	return (TRUE);
}

static void
handle_send_activate (GtkMenuItem *item, EogCollectionView *view)
{
	CORBA_Object composer;
	CORBA_Environment ev;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	CORBA_exception_init (&ev);
	composer = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Mail_Composer",
					 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to start composer: %s",
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	eog_collection_model_foreach (view->priv->model, send_item, composer);

	GNOME_Evolution_Composer_show (composer, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to show composer: %s", 
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (composer, NULL);
		return;
	}

	CORBA_exception_free (&ev);
}
#endif

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
	GtkWidget *menu, *item, *label;

	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (view), FALSE);

	menu = gtk_menu_new ();
	g_signal_connect (G_OBJECT (menu), "hide",
			  G_CALLBACK (kill_popup_menu), menu);

#ifdef ENABLE_EVOLUTION
	label = gtk_label_new (_("Send"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);

	item = gtk_menu_item_new ();
	gtk_widget_show (item);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (handle_send_activate), view);
	gtk_container_add (GTK_CONTAINER (item), label);
	gtk_menu_append (GTK_MENU (menu), item);
#endif

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
			GnomeVFSURI *uri;
			gchar *basename;

			img = eog_collection_model_get_selected_image (priv->model);
			uri = cimage_get_uri (img);
			g_assert (uri != NULL);

			basename = g_path_get_basename (gnome_vfs_uri_get_path (uri));
			g_snprintf (str, 70, "Images: %i  %s (%i x %i)", nimg,
				    basename,
				    cimage_get_width (img),
				    cimage_get_height (img));

			gnome_vfs_uri_unref (uri);
			g_free (basename);
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
model_size_changed (EogCollectionModel *model, GList *id_list,  gpointer data)
{
	EogCollectionView *view;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	view = EOG_COLLECTION_VIEW (data);
	update_status_text (view);
}

static void
model_selection_changed (EogCollectionModel *model, GQuark id, gpointer data)
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

static void
color_changed_cb (GConfClient *client, guint cnxn_id, 
		   GConfEntry *entry, gpointer user_data)
{
	EogCollectionView *view;
	GdkColor color;
	GSList *l = NULL;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));
	g_return_if_fail (entry->value->type == GCONF_VALUE_LIST);

	view = EOG_COLLECTION_VIEW (user_data);
	
	g_assert (gconf_value_get_list_type (entry->value) == GCONF_VALUE_INT);
	
	l = gconf_client_get_list (client, pref_key[PREF_COLOR],
					   GCONF_VALUE_INT, NULL);
	
	color.red = GPOINTER_TO_UINT (l->data);
	color.green = GPOINTER_TO_UINT (l->next->data);
	color.blue = GPOINTER_TO_UINT (l->next->next->data);

	eog_wrap_list_set_background_color (EOG_WRAP_LIST (view->priv->wraplist),
					    &color);
}

/* read configuration */
static void
set_configuration_values (EogCollectionView *view)
{
	EogCollectionViewPrivate *priv = NULL;
	gint layout;
	GSList *l;
	GdkColor color;

	g_return_if_fail (view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));
	
	priv = view->priv;
	
	/* set layout mode */
	layout = gconf_client_get_int (priv->client, 
				       pref_key[PREF_LAYOUT],
				       NULL);
	eog_wrap_list_set_layout_mode (EOG_WRAP_LIST (priv->wraplist),
				       layout);
	
	/* set background color */
	l = gconf_client_get_list (priv->client, pref_key[PREF_COLOR],
				   GCONF_VALUE_INT, NULL);
	if (l) {
		color.red = GPOINTER_TO_UINT (l->data);
		color.green = GPOINTER_TO_UINT (l->next->data);
		color.blue = GPOINTER_TO_UINT (l->next->next->data);
	} else {
		color.red = 57015;   /* default gtk color */
		color.green = 57015;
		color.blue = 57015;
	}
	eog_wrap_list_set_background_color (EOG_WRAP_LIST (priv->wraplist),
					    &color);
	/* add configuration listeners */
	priv->notify_id[PREF_LAYOUT] = 
		gconf_client_notify_add (priv->client, 
					 pref_key[PREF_LAYOUT],
					 layout_changed_cb,
					 view,
					 NULL, NULL);
	priv->notify_id[PREF_COLOR] = 
		gconf_client_notify_add (priv->client, 
					 pref_key[PREF_COLOR],
					 color_changed_cb,
					 view,
					 NULL, NULL);
}

EogCollectionView *
eog_collection_view_construct (EogCollectionView *list_view)
{
	EogCollectionViewPrivate *priv = NULL;
	EogItemFactory *factory;
	EogImageLoader *loader;

	g_return_val_if_fail (list_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (list_view), NULL);
	
	priv = list_view->priv;
	
	priv->uic = bonobo_ui_component_new ("EogCollectionView");

	/* Make sure GConf is initialized */
	if (!gconf_is_initialized ())
		gconf_init (0, NULL, NULL);
	
	priv->client = gconf_client_get_default ();
	gconf_client_add_dir (priv->client, PREF_PREFIX, 
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	/* construct widget */
	g_assert (EOG_IS_COLLECTION_VIEW (list_view));
	priv->model = eog_collection_model_new ();
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

#if HAVE_LIBPREVIEW
	loader = eog_image_loader_preview_new (96, 96);
#else
	loader = eog_image_loader_simple_new (96, 96);
#endif
	factory = EOG_ITEM_FACTORY (eog_item_factory_clean_new (loader));

	priv->root = gtk_scrolled_window_new (NULL, NULL);

	priv->wraplist = eog_wrap_list_new ();
	gtk_container_add (GTK_CONTAINER (priv->root), priv->wraplist);
	eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), priv->model);
	eog_wrap_list_set_factory (EOG_WRAP_LIST (priv->wraplist), factory);
	eog_wrap_list_set_col_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	eog_wrap_list_set_row_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	g_signal_connect (G_OBJECT (priv->wraplist), "double_click", 
			  G_CALLBACK (handle_double_click), list_view);
	g_signal_connect (G_OBJECT (priv->wraplist), "right_click",
			  G_CALLBACK (handle_right_click), list_view);

	gtk_widget_show (priv->wraplist);
	gtk_widget_show (priv->root);

	g_object_unref (G_OBJECT (factory));
	g_object_unref (G_OBJECT (loader));

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

	/* read user defined configuration */
	set_configuration_values (list_view);

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

BonoboPropertyBag*
eog_collection_view_get_property_bag (EogCollectionView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (view), NULL);

	bonobo_object_ref (BONOBO_OBJECT (view->priv->property_bag));

	return view->priv->property_bag;
}
