/* Eog Of Gnome - view of the image collection
 *
 * Copyright 2000 SuSE GmbH.
 * Copyright 2001-2003 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
 *
 * Based on code by: Martin Baulig <baulig@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-vfs-extensions.h>

#include "eog-wrap-list.h"
#include "eog-scroll-view.h"
#include "eog-collection-view.h"
#include "eog-collection-model.h"
#include "eog-collection-marshal.h"
#include "eog-full-screen.h"
#include "eog-info-view.h"
#include "eog-transform.h"
#include "eog-save-dialog.h"
#include "eog-horizontal-splitter.h"
#include "eog-vertical-splitter.h"

enum {
	PROP_WINDOW_TITLE,
	PROP_WINDOW_STATUS,
	PROP_IMAGE_PROGRESS,
	PROP_LAST
};

static const gchar *property_name[] = {
	"window/title", 
	"window/status",
	"image/progress"
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
	GtkWidget               *info_view;

	BonoboPropertyBag       *property_bag;
	BonoboZoomable          *zoomable;
	BonoboControl           *control;

	BonoboUIComponent       *uic;

	GConfClient             *client;
	guint                   notify_id[PREF_LAST];

	gint                    idle_id;
	gboolean                need_update_prop[PROP_LAST];

	EogImage                *displayed_image;
	guint                   progress_handler_id;

	gboolean                has_zoomable_frame;
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
	EogCollectionView *view;
	GtkWidget *show;
	GList *images = NULL;
	EogImage *start_image = NULL;
	gboolean free_list = FALSE;

	view = EOG_COLLECTION_VIEW (user_data);

	if (eog_wrap_list_get_n_selected (EOG_WRAP_LIST (view->priv->wraplist)) > 1) {
		images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (view->priv->wraplist));
		free_list = TRUE;
	}
	else {
		images = eog_collection_model_get_image_list (view->priv->model);
	}
	start_image = eog_wrap_list_get_first_selected_image (EOG_WRAP_LIST (view->priv->wraplist));

	show = eog_full_screen_new (images, start_image);
	g_object_unref (start_image);

	if (free_list)
		g_list_free (images);

	gtk_widget_show (show);
}

static void
verb_ImagePrev_cb (BonoboUIComponent *uic,
		   gpointer user_data,
		   const char *cname)
{
	EogCollectionView *view;
	
	view = EOG_COLLECTION_VIEW (user_data);

	eog_wrap_list_select_left (EOG_WRAP_LIST (view->priv->wraplist));
}

static void
verb_ImageNext_cb (BonoboUIComponent *uic,
		   gpointer user_data,
		   const char *cname)
{
	EogCollectionView *view;
	
	view = EOG_COLLECTION_VIEW (user_data);

	eog_wrap_list_select_right (EOG_WRAP_LIST (view->priv->wraplist));
}

static void 
verb_Undo_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	GList *images;
	GList *it;
	EogCollectionView *view;

	view = EOG_COLLECTION_VIEW (data);

	images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (view->priv->wraplist));	

	for (it = images; it != NULL; it = it->next) {
		EogImage *image = EOG_IMAGE (it->data);
		eog_image_undo (image);
	}
}

/*
 * Image saving stuff.
 *
 */

typedef struct {
	EogCollectionView *view;
	GList             *image_list;
	GtkWidget         *dlg;
	gboolean          cancel;
	gulong            finished_id;
	gulong            failed_id;
	int               n_images;
	int               count;
} SaveContext;

static const char* save_threat_cmds[] = {
	"/commands/FlipHorizontal",
	"/commands/FlipVertical",
        "/commands/Rotate90cw",
	"/commands/Rotate90ccw",
	"/commands/Rotate180",
        "/commands/Undo",
        "/commands/Save", 
	"/commands/SlideShow",
	NULL
};

static void save_image_init_loading (SaveContext *ctx);

static void
set_commands_sensitive_state (EogCollectionView *view, const char *command[], gboolean sensitive)
{
	int i = 0;
	
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	for (i = 0; command[i] != NULL; i++) {
		bonobo_ui_component_set_prop (view->priv->uic, command[i], "sensitive",
					      sensitive ? "1" : "0", NULL);
	}
}


static void
save_image_loading_failed (EogImage *image, char *message, gpointer data)
{
	SaveContext *ctx;

	ctx = (SaveContext *) data;

	g_signal_handler_disconnect (G_OBJECT (image), ctx->finished_id);
	g_signal_handler_disconnect (G_OBJECT (image), ctx->failed_id);
	
	g_object_unref (image);

	save_image_init_loading (ctx);
}

static void
save_image_loading_finished (EogImage *image, gpointer data)
{
	SaveContext *ctx;

	ctx = (SaveContext *) data;

	g_signal_handler_disconnect (G_OBJECT (image), ctx->finished_id);
	g_signal_handler_disconnect (G_OBJECT (image), ctx->failed_id);

	if (eog_image_is_modified (image) && !ctx->cancel) {
			GError *error = NULL;
			GnomeVFSURI *uri;
			
			uri = eog_image_get_uri (image);
			
			if (!eog_image_save (image, uri, &error)) {
				/* FIXME: indicate failed state to the user */
			}
			gnome_vfs_uri_unref (uri);
	}

	if (image != ctx->view->priv->displayed_image) {
		eog_image_free_mem (image);
	}

	g_object_unref (image);

	save_image_init_loading (ctx);
}


static void
save_image_init_loading (SaveContext *ctx)
{
	if (ctx->image_list == NULL || ctx->cancel) {
		if (ctx->cancel) 
			eog_save_dialog_update (EOG_SAVE_DIALOG (ctx->dlg), (double) ctx->count / (double) ctx->n_images,
						_("Cancel saving"));
		else 
			eog_save_dialog_update (EOG_SAVE_DIALOG (ctx->dlg), 1.0,
						_("Saving finished"));
		
		gtk_dialog_response (GTK_DIALOG (ctx->dlg), GTK_RESPONSE_OK);
	}
	else {
		EogImage *image = EOG_IMAGE (ctx->image_list->data);

		eog_save_dialog_update (EOG_SAVE_DIALOG (ctx->dlg), (double) ctx->count++ / (double) ctx->n_images,
					eog_image_get_caption (image));

		ctx->finished_id = g_signal_connect (image, "loading_finished", G_CALLBACK (save_image_loading_finished), ctx);
		ctx->failed_id = g_signal_connect (image, "loading_failed", G_CALLBACK (save_image_loading_failed), ctx);
		ctx->image_list = g_list_delete_link (ctx->image_list, ctx->image_list);

		g_object_ref (image);

		eog_image_load (image, EOG_IMAGE_LOAD_COMPLETE);
	}

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static gboolean
save_dlg_close (gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (data));
	return FALSE;
}

static gboolean
save_dlg_delete_cb (GtkWidget *dialog,
		    GdkEvent *event,
		    gpointer user_data)
{
	SaveContext *ctx;

	ctx = (SaveContext*) user_data;
	ctx->cancel = TRUE;

	return TRUE;
}

static void
save_dlg_response_cb (GtkDialog *dialog,
		      gint response_id,
		      gpointer user_data)
{
	SaveContext *ctx;
	EogCollectionView *view;

	ctx = (SaveContext*) user_data;

	switch (response_id) {
	case GTK_RESPONSE_OK:
		view = ctx->view;
		if (ctx->image_list != NULL) {
			g_list_free (ctx->image_list);
		}
		g_free (ctx);
		g_timeout_add (1250, save_dlg_close, dialog);

		/* reenable commands that may disturb the save state */
		set_commands_sensitive_state (view, save_threat_cmds, TRUE);

		break;

	default:
		ctx->cancel = TRUE;
	}
}

static void 
verb_Save_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	EogCollectionView *view;
	EogCollectionViewPrivate *priv;
	SaveContext *ctx;

	view = EOG_COLLECTION_VIEW (data);
	priv = view->priv;

	ctx = g_new0 (SaveContext, 1);
	ctx->image_list = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));
	ctx->view = view;
	ctx->cancel = FALSE;

	if (ctx->image_list == NULL) {
		g_free (ctx);
		return;
	}

	ctx->n_images = g_list_length (ctx->image_list);
	ctx->count = 0;

	/* disable commands that may disturb the save state */
	set_commands_sensitive_state (view, save_threat_cmds, FALSE);

	ctx->dlg = eog_save_dialog_new ();

	bonobo_control_set_transient_for (BONOBO_CONTROL (priv->control), GTK_WINDOW (ctx->dlg), NULL);
	g_signal_connect (G_OBJECT (ctx->dlg), "delete-event", G_CALLBACK (save_dlg_delete_cb), ctx);
	g_signal_connect (G_OBJECT (ctx->dlg), "response", G_CALLBACK (save_dlg_response_cb), ctx);

	gtk_widget_show_now (GTK_WIDGET (ctx->dlg));

	save_image_init_loading (ctx);
}


/*
 *  Image transforming stuff.
 *
 */

typedef struct {
	EogCollectionView *view;
	float max_progress;
	int counter;
} ProgressData;

static void
image_progress_cb (EogImage *image, float progress, gpointer data)
{
	ProgressData *pd;
	float total;
	BonoboArg *arg;
	EogCollectionViewPrivate *priv;

	if (EOG_IS_COLLECTION_VIEW (data)) {
		pd = NULL;
		priv = EOG_COLLECTION_VIEW (data)->priv;
	}
	else {
		pd = (ProgressData*) data;
		priv = pd->view->priv;
	}

	if (!pd) {
		total = progress;
	}
	else {
		total = ((float) pd->counter + progress) / pd->max_progress;
	}

	arg = bonobo_arg_new (BONOBO_ARG_FLOAT);
	BONOBO_ARG_SET_FLOAT (arg, total);

	bonobo_event_source_notify_listeners (priv->property_bag->es,
					      "image/progress",
					      arg, NULL);
	bonobo_arg_release (arg);
}

static void 
apply_transformation (EogCollectionView *view, EogTransform *trans)
{
	GList *images;
	GList *it;
	gint id;
	ProgressData *data;

	images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (view->priv->wraplist));	
	data = g_new0 (ProgressData, 1);
	data->view = view;
	data->max_progress = g_list_length (images);
	data->counter = 0;

	for (it = images; it != NULL; it = it->next) {
		EogImage *image = EOG_IMAGE (it->data);

		id = g_signal_connect (G_OBJECT (image), "progress", (GCallback) image_progress_cb, data);

		eog_image_transform (image, trans);

		g_signal_handler_disconnect (image, id);
		data->counter++;
	}

	g_free (data);
	g_object_unref (trans);
}

static void
verb_FlipHorizontal_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	apply_transformation (EOG_COLLECTION_VIEW (data), eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL));
}

static void
verb_FlipVertical_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	apply_transformation (EOG_COLLECTION_VIEW (data), eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL));
}

static void
verb_Rotate90ccw_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	apply_transformation (EOG_COLLECTION_VIEW (data), eog_transform_rotate_new (270));
}

static void
verb_Rotate90cw_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	apply_transformation (EOG_COLLECTION_VIEW (data), eog_transform_rotate_new (90));
}

static void
verb_Rotate180_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	apply_transformation (EOG_COLLECTION_VIEW (data), eog_transform_rotate_new (180));
}

static void
verb_ZoomIn_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogCollectionView *view;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);

	eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (view->priv->scroll_view), FALSE);
}

static void
verb_ZoomOut_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogCollectionView *view;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);

	eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (view->priv->scroll_view), FALSE);
}

static void
verb_ZoomToDefault_cb (BonoboUIComponent *uic, gpointer user_data,
		       const char *cname)
{
	EogCollectionView *view;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);

	eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (view->priv->scroll_view), 1.0);
}

static void
verb_ZoomToFit_cb (BonoboUIComponent *uic, gpointer user_data,
		   const char *cname)
{
	EogCollectionView *view;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (user_data));

	view = EOG_COLLECTION_VIEW (user_data);

	eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (view->priv->scroll_view));
}

static BonoboUIVerb eog_zoom_verbs[] = {
	BONOBO_UI_VERB ("ZoomIn",        verb_ZoomIn_cb),
	BONOBO_UI_VERB ("ZoomOut",       verb_ZoomOut_cb),
	BONOBO_UI_VERB ("ZoomToDefault", verb_ZoomToDefault_cb),
	BONOBO_UI_VERB ("ZoomToFit",     verb_ZoomToFit_cb),
	BONOBO_UI_VERB_END
};

static BonoboUIVerb collection_verbs[] = {
	BONOBO_UI_VERB ("SlideShow", verb_SlideShow_cb),
	BONOBO_UI_VERB ("ImageNext", verb_ImageNext_cb),
	BONOBO_UI_VERB ("ImagePrev", verb_ImagePrev_cb),
	BONOBO_UI_VERB ("FlipHorizontal",verb_FlipHorizontal_cb),
	BONOBO_UI_VERB ("FlipVertical",  verb_FlipVertical_cb),
	BONOBO_UI_VERB ("Rotate90cw",    verb_Rotate90cw_cb),
	BONOBO_UI_VERB ("Rotate90ccw",   verb_Rotate90ccw_cb),
	BONOBO_UI_VERB ("Rotate180",     verb_Rotate180_cb),
	BONOBO_UI_VERB ("Undo",          verb_Undo_cb),
	BONOBO_UI_VERB ("Save",          verb_Save_cb),
	BONOBO_UI_VERB_END
};

static void
eog_collection_view_create_ui (EogCollectionView *view)
{
	EogCollectionViewPrivate *priv;

	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	priv = view->priv;

	/* Set up the UI from an XML file. */
	bonobo_ui_util_set_ui (priv->uic, DATADIR,
						   "eog-collection-view-ui.xml", "EogCollectionView", NULL);

	bonobo_ui_component_set_prop (priv->uic, "/menu/Edit/Eog EditPreferences Separator",
								  "hidden", "0", NULL);
	bonobo_ui_component_add_verb_list_with_data (priv->uic,
						     collection_verbs,
						     view);

	if (!priv->has_zoomable_frame) {
		bonobo_ui_util_set_ui (priv->uic, DATADIR, "eog-image-view-ctrl-ui.xml", "EogCollectionView", NULL);
		
		bonobo_ui_component_add_verb_list_with_data (priv->uic, eog_zoom_verbs,
							     view);
	}
}

static void
eog_collection_view_set_ui_container (EogCollectionView      *list_view,
				      Bonobo_UIContainer ui_container)
{
	g_return_if_fail (list_view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (list_view));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	bonobo_ui_component_set_container (list_view->priv->uic, ui_container, NULL);

	eog_collection_view_create_ui (list_view);
}

static void
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
	EogCollectionViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (object));

	list_view = EOG_COLLECTION_VIEW (object);
	priv = list_view->priv;

	if (priv->displayed_image != NULL) {
		g_object_unref (priv->displayed_image);
		priv->displayed_image = NULL;
	}

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

	view->priv->displayed_image = NULL;
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
#else
	return FALSE;
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
			title = eel_format_uri_for_display (base_uri);
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

	if (priv->displayed_image != image) {
		if (priv->displayed_image != NULL) {
			g_signal_handler_disconnect (priv->displayed_image, priv->progress_handler_id);

			eog_image_free_mem (priv->displayed_image);
			g_object_unref (priv->displayed_image);
			priv->displayed_image = NULL;
		}
		
		priv->progress_handler_id = g_signal_connect (image, "progress", G_CALLBACK (image_progress_cb), view);
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->scroll_view), image);
		if (priv->info_view != NULL) {
			eog_info_view_set_image (EOG_INFO_VIEW (priv->info_view), image);
		}
		
		priv->displayed_image = image;
	}
		
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

static void
zoomable_set_frame_cb (BonoboZoomable *zoomable, EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	view->priv->has_zoomable_frame = TRUE;
}

static void
zoomable_set_zoom_level_cb (BonoboZoomable *zoomable, CORBA_float new_zoom_level,
			    EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (view->priv->scroll_view), new_zoom_level);
}

static void
zoomable_zoom_in_cb (BonoboZoomable *zoomable, EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (view->priv->scroll_view), FALSE);
}

static void
zoomable_zoom_out_cb (BonoboZoomable *zoomable, EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (view->priv->scroll_view), FALSE);
}

static void
zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable, EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (view->priv->scroll_view));
}

static void
zoomable_zoom_to_default_cb (BonoboZoomable *zoomable, EogCollectionView *view)
{
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));

	eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (view->priv->scroll_view), 1.0);
}

static void
view_zoom_changed_cb (GtkWidget *widget, double zoom, gpointer data)
{
	EogCollectionView *view;
	EogCollectionViewPrivate *priv;

	view = EOG_COLLECTION_VIEW (data);
	priv = view->priv;

	/* inform zoom interface listeners */
	bonobo_zoomable_report_zoom_level_changed (priv->zoomable, zoom, NULL);
}

/* read configuration */
static void
init_gconf_defaults (EogCollectionView *view)
{
#if 0
	EogCollectionViewPrivate *priv = NULL;
	gint layout;
	GSList *l;
	GdkColor color;

	g_return_if_fail (view != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (view));
	
	priv = view->priv;

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
	EogCollectionView *view;

	view = EOG_COLLECTION_VIEW (closure);
	priv = view->priv;

	if (text_uri == CORBA_OBJECT_NIL) return 0;

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

	eog_collection_model_add_uri (priv->model, (gchar*)text_uri); 

	return 0;
}

static GtkWidget*
create_user_interface (EogCollectionView *list_view)
{
	EogCollectionViewPrivate *priv;
#if HAVE_EXIF
	GtkWidget *hpaned;
#endif
	GtkWidget *vpaned;
	GtkWidget *sw;
	GtkWidget *frame;
	
	priv = list_view->priv;

	/* the upper part contains the image view, the
	 * lower part contains the thumbnail list
	 */
	vpaned = eog_vertical_splitter_new ();

	/* the image view for the full size image */
 	priv->scroll_view = eog_scroll_view_new ();
	g_object_set (G_OBJECT (priv->scroll_view), "height_request", 250, NULL);
	g_signal_connect (G_OBJECT (priv->scroll_view),
			  "zoom_changed",
			  (GCallback) view_zoom_changed_cb,
			  list_view);
	frame = gtk_widget_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_IN, NULL);
	gtk_container_add (GTK_CONTAINER (frame), priv->scroll_view);

#if HAVE_EXIF
	/* If we have additional information through libexif, we 
	 * create an info view widget and put it to left of the 
	 * image view. Using an eog_horizontal_splitter for this. 
	 */
	priv->info_view = gtk_widget_new (EOG_TYPE_INFO_VIEW, NULL);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), priv->info_view);

	/* left side holds the image view, right side the info view */
	hpaned = eog_horizontal_splitter_new (); 
	gtk_paned_pack1 (GTK_PANED (hpaned), frame, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (hpaned), sw, FALSE, TRUE);
	gtk_widget_show_all (hpaned);

	gtk_paned_pack1 (GTK_PANED (vpaned), hpaned, TRUE, TRUE);

#else
	/* If libexif isn't available we put the image view (frame)
	 * directly into the vpaned created above.
	 */
	priv->info_view = NULL;
	gtk_widget_show_all (frame);
	gtk_paned_pack1 (GTK_PANED (vpaned), frame, TRUE, TRUE);	
#endif

	/* the wrap list for all the thumbnails */
	priv->wraplist = eog_wrap_list_new ();
	g_object_set (G_OBJECT (priv->wraplist), 
		      "height_request", 200, 
		      "width_request", 500,
		      NULL);
	eog_wrap_list_set_col_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	eog_wrap_list_set_row_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	g_signal_connect (G_OBJECT (priv->wraplist), "selection_changed",
			  G_CALLBACK (handle_selection_changed), list_view);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), priv->wraplist);
	gtk_widget_show_all (sw);
	
	gtk_paned_pack2 (GTK_PANED (vpaned), sw, TRUE, TRUE);

	/* by default make the wrap list keyboard active */
	gtk_widget_grab_focus (priv->wraplist);

	gtk_widget_show (vpaned);

	return vpaned;
}


static EogCollectionView *
eog_collection_view_construct (EogCollectionView *list_view)
{
	EogCollectionViewPrivate *priv = NULL;
	BonoboControl *control;
	GtkWidget *root;
	BonoboZoomable *zoomable;

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
	priv->control = control;
	g_signal_connect (control, "activate", G_CALLBACK (control_activate_cb), list_view);

	bonobo_object_add_interface (BONOBO_OBJECT (list_view),
				     BONOBO_OBJECT (control));


	/* Interface Bonobo::Zoomable */
	zoomable = bonobo_zoomable_new ();
	priv->zoomable = zoomable;

	g_signal_connect (G_OBJECT (zoomable),
			  "set_frame",
			  G_CALLBACK (zoomable_set_frame_cb),
			  list_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "set_zoom_level",
			  G_CALLBACK (zoomable_set_zoom_level_cb),
			  list_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_in",
			  G_CALLBACK (zoomable_zoom_in_cb),
			  list_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_out",
			  G_CALLBACK (zoomable_zoom_out_cb),
			  list_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_to_fit",
			  G_CALLBACK (zoomable_zoom_to_fit_cb),
			  list_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_to_default",
			  G_CALLBACK (zoomable_zoom_to_default_cb),
			  list_view);

	bonobo_zoomable_set_parameters (zoomable,
					1.0,   /* current */
					0.01,  /* min */
					20.0,  /* max */
					TRUE,
					TRUE);

	bonobo_object_add_interface (BONOBO_OBJECT (list_view),
				     BONOBO_OBJECT (zoomable));


	/* Property Bag */
	priv->property_bag = bonobo_property_bag_new (eog_collection_view_get_prop,
						      eog_collection_view_set_prop,
						      list_view);
	bonobo_property_bag_add (priv->property_bag, property_name[PROP_WINDOW_TITLE], PROP_WINDOW_TITLE,
				 BONOBO_ARG_STRING, NULL, _("Window Title"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, property_name[PROP_WINDOW_STATUS], PROP_WINDOW_STATUS,
				 BONOBO_ARG_STRING, NULL, _("Status Text"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "image/progress", PROP_IMAGE_PROGRESS,
				 BONOBO_ARG_FLOAT, NULL, _("Progress of Image Loading"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_control_set_properties (BONOBO_CONTROL (control), 
				       BONOBO_OBJREF (priv->property_bag),
				       NULL);
	
	/* read user defined configuration */
	init_gconf_defaults (list_view);

	/* UI Component */
	priv->uic = bonobo_ui_component_new ("EogCollectionView");

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

