/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-view.c
 *
 * Authors:
 *   Jens Finke (jens@triq.net)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000 SuSE GmbH.
 * Copyright 2001-2002 Free Software Foundation
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <glib/gstrfuncs.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktypeutils.h>
#include <gconf/gconf-client.h>

#include <bonobo/bonobo-persist-file.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-zoomable.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-stream.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <eel/eel-vfs-extensions.h>

#include "eog-image-view.h"
#include "eog-scroll-view.h"
#include "eog-file-selection.h"
#include "eog-full-screen.h"
#include "eog-hig-dialog.h"

#include <libgpi/gpi-dialog-pixbuf.h>
#include <libgpi/gpi-mgr-pixbuf.h>
#include <libgnomeprintui/gnome-print-job-preview.h>




/* Commands from the popup menu */
enum {
	POPUP_ROTATE_CLOCKWISE,
	POPUP_ROTATE_COUNTER_CLOCKWISE,
	POPUP_ZOOM_IN,
	POPUP_ZOOM_OUT,
	POPUP_ZOOM_1,
	POPUP_ZOOM_FIT,
	POPUP_CLOSE
};

/* Private part of the EogImageView structure */
struct _EogImageViewPrivate {
	EogImage              *image;
	GtkWidget             *widget;

	GConfClient	      *client;
	guint                 interp_type_notify_id;
	guint                 transparency_notify_id;
	guint                 trans_color_notify_id;

	BonoboPropertyBag     *property_bag;
	BonoboZoomable        *zoomable;
	BonoboControl         *control;

	BonoboUIComponent     *uic;

	/* Item factory for popup menu */
	GtkItemFactory *item_factory;

	/* Mouse position, relative to the image view, when the popup menu was
	 * invoked.
	 */
	int popup_x, popup_y;

	gboolean has_zoomable_frame;

	guint image_signal_id[4];
};

enum {
	PROP_INTERPOLATION,
	PROP_DITHER,
	PROP_CHECK_TYPE,
	PROP_CHECK_SIZE,
	PROP_IMAGE_WIDTH,
	PROP_IMAGE_HEIGHT,
	PROP_IMAGE_PROGRESS,
	PROP_WINDOW_WIDTH,
	PROP_WINDOW_HEIGHT,
	PROP_WINDOW_TITLE,
	PROP_WINDOW_STATUS
};

enum {
	PROP_CONTROL_TITLE
};

/* Signal IDs */
enum {
	CLOSE_ITEM_ACTIVATED,
	LAST_SIGNAL
};

static void popup_menu_cb (gpointer data, guint action, GtkWidget *widget);
static gint save_uri_cb (BonoboPersistFile *pf, const CORBA_char *text_uri,
			 CORBA_Environment *ev, void *closure);


static guint signals[LAST_SIGNAL] = { 0 };

BONOBO_CLASS_BOILERPLATE (EogImageView, eog_image_view,
			  BonoboPersistFile, BONOBO_TYPE_PERSIST_FILE);

static gboolean
view_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	EogImageView *view;
	EogImageViewPrivate *priv;
	int x, y;

	view = EOG_IMAGE_VIEW (data);
	priv = view->priv;

	if (event->button != 3)
		return FALSE;

	priv->popup_x = event->x;
	priv->popup_y = event->y;

	gdk_window_get_origin (event->window, &x, &y);

	x += event->x;
	y += event->y;

	gtk_item_factory_popup (priv->item_factory, x, y, event->button, event->time);
	return TRUE;
}

/* Callback for the popup_menu signal of the image view */
static gboolean
view_popup_menu_cb (GtkWidget *widget, gpointer data)
{
	EogImageView *view;
	EogImageViewPrivate *priv;
	int x, y;

	view = EOG_IMAGE_VIEW (data);
	priv = view->priv;

	priv->popup_x = widget->allocation.width / 2;
	priv->popup_y = widget->allocation.height / 2;

	gdk_window_get_origin (widget->window, &x, &y);
	x += priv->popup_x;
	y += priv->popup_y;

	gtk_item_factory_popup (priv->item_factory, x, y, 0, gtk_get_current_event_time ());
	return TRUE;
}

static void
verb_FullScreen_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	EogImageView *image_view;
	GtkWidget *fs;
	GList *list = NULL;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	image_view = EOG_IMAGE_VIEW (data);

	list = g_list_prepend (list, EOG_IMAGE (image_view->priv->image));
	fs = eog_full_screen_new (list, NULL);
	g_list_free (list);

	gtk_widget_show_all (fs);
}

static void 
verb_Undo_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	EogImageViewPrivate *priv;

	priv = EOG_IMAGE_VIEW (data)->priv;

	if (priv->image != NULL) {
		eog_image_undo (priv->image);
	}
}

static void
apply_transformation (EogImageView *view, EogTransform *trans)
{
	EogImageViewPrivate *priv;

	priv = view->priv;

	if (priv->image != NULL) {
		eog_image_transform (priv->image, trans);
	}

	g_object_unref (trans);
}

static void
verb_FlipHorizontal_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL));
}

static void
verb_FlipVertical_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL));
}

static void
verb_Rotate90ccw_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_rotate_new (270));
}

static void
verb_Rotate90cw_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_rotate_new (90));
}

static void
verb_Rotate180_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_rotate_new (180));
}

static void
verb_ZoomIn_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogImageView *view;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	view = EOG_IMAGE_VIEW (user_data);

	eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (view->priv->widget), FALSE);
}

static void
verb_ZoomOut_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogImageView *view;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	view = EOG_IMAGE_VIEW (user_data);

	eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (view->priv->widget), FALSE);
}

static void
verb_ZoomToDefault_cb (BonoboUIComponent *uic, gpointer user_data,
		       const char *cname)
{
	EogImageView *view;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	view = EOG_IMAGE_VIEW (user_data);

	eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (view->priv->widget), 1.0);
}

static void
verb_ZoomToFit_cb (BonoboUIComponent *uic, gpointer user_data,
		   const char *cname)
{
	EogImageView *view;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	view = EOG_IMAGE_VIEW (user_data);

	eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (view->priv->widget));
}

static void
verb_SaveAs_cb (BonoboUIComponent *uic, gpointer user_data,
		const char *cname)
{
	EogImageView     *image_view;
	GtkWidget        *dlg;
	int              response;
	gchar            *filename = NULL;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	if (image_view->priv->image == NULL) return;

	dlg = eog_file_selection_new (EOG_FILE_SELECTION_SAVE);
	gtk_widget_show_all (dlg);

	response = gtk_dialog_run (GTK_DIALOG (dlg));

	if (response == GTK_RESPONSE_OK) {
		filename = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (dlg)));
	}

	gtk_widget_destroy (dlg);

	if (response == GTK_RESPONSE_OK) {
		save_uri_cb (NULL, filename, NULL, image_view);
	}

	if (filename != NULL) {
		g_free (filename);
	}
}

static void
on_ok_clicked (GPIDialog *d, EogImageView *iv)
{
	gtk_idle_add ((GtkFunction) gtk_object_destroy, d);
}

static void
on_cancel_clicked (GPIDialog *d, EogImageView *iv)
{
	gtk_idle_add ((GtkFunction) gtk_object_destroy, d);
}

static void
verb_PrintSetup_cb (BonoboUIComponent *uic, gpointer user_data,
		    const char *cname)
{
	EogImageView *iv = EOG_IMAGE_VIEW (user_data);
	GdkPixbuf *pixbuf = eog_image_get_pixbuf (iv->priv->image);
	GPIDialogPixbuf *d = gpi_dialog_pixbuf_new (pixbuf);

	g_signal_connect (d, "ok_clicked", G_CALLBACK (on_ok_clicked), iv);
	g_signal_connect (d, "cancel_clicked",
			  G_CALLBACK (on_cancel_clicked), iv);
	gtk_widget_show (GTK_WIDGET (d));
}

static void
verb_PrintPreview_cb (BonoboUIComponent *uic, gpointer user_data,
		      const char *cname)
{
	EogImageView *iv = EOG_IMAGE_VIEW (user_data);
	GdkPixbuf *pixbuf = eog_image_get_pixbuf (iv->priv->image);
	GPIMgrPixbuf *mgr = gpi_mgr_pixbuf_new (pixbuf);
	GnomePrintJob *job = gpi_mgr_get_job (GPI_MGR (mgr));
	GtkWidget *d;

	d = gnome_print_job_preview_new (job, _("Preview"));
	g_object_unref (job);
	g_object_unref (mgr);
	gtk_widget_show (d);
}

static void
verb_Print_cb (BonoboUIComponent *uic, gpointer user_data,
	       const char *cname)
{
	EogImageView *iv = EOG_IMAGE_VIEW (user_data);
	GdkPixbuf *pixbuf = eog_image_get_pixbuf (iv->priv->image);
	GPIMgrPixbuf *mgr = gpi_mgr_pixbuf_new (pixbuf);
	GnomePrintJob *job = gpi_mgr_get_job (GPI_MGR (mgr));

	gnome_print_job_print (job);
	g_object_unref (job);
	g_object_unref (mgr);
}

static BonoboUIVerb eog_zoom_verbs[] = {
	BONOBO_UI_VERB ("ZoomIn",        verb_ZoomIn_cb),
	BONOBO_UI_VERB ("ZoomOut",       verb_ZoomOut_cb),
	BONOBO_UI_VERB ("ZoomToDefault", verb_ZoomToDefault_cb),
	BONOBO_UI_VERB ("ZoomToFit",     verb_ZoomToFit_cb),
	BONOBO_UI_VERB_END
};

static BonoboUIVerb eog_verbs[] = {
	BONOBO_UI_VERB ("SaveAs",        verb_SaveAs_cb),
	BONOBO_UI_VERB ("FullScreen",    verb_FullScreen_cb),
	BONOBO_UI_VERB ("Undo",          verb_Undo_cb),
	BONOBO_UI_VERB ("FlipHorizontal",verb_FlipHorizontal_cb),
	BONOBO_UI_VERB ("FlipVertical",  verb_FlipVertical_cb),
	BONOBO_UI_VERB ("Rotate90cw",    verb_Rotate90cw_cb),
	BONOBO_UI_VERB ("Rotate90ccw",   verb_Rotate90ccw_cb),
	BONOBO_UI_VERB ("Rotate180",     verb_Rotate180_cb),
	BONOBO_UI_VERB ("PrintSetup",    verb_PrintSetup_cb),
	BONOBO_UI_VERB ("Print",         verb_Print_cb),
	BONOBO_UI_VERB ("PrintPreview",  verb_PrintPreview_cb),
	BONOBO_UI_VERB_END
};

static void
eog_image_view_create_ui (EogImageView *image_view)
{
	EogImageViewPrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	priv = image_view->priv;

	/* Set up the UI from XML file. */
        bonobo_ui_util_set_ui (priv->uic, DATADIR,
			       "eog-image-view-ui.xml", "EogImageView", NULL);
	bonobo_ui_component_set_prop (priv->uic, "/menu/Edit/Eog EditPreferences Separator",
				      "hidden", "0", NULL);
	bonobo_ui_component_add_verb_list_with_data (priv->uic, eog_verbs,
						     image_view);

	if (!priv->has_zoomable_frame) {
		bonobo_ui_util_set_ui (priv->uic, DATADIR, "eog-image-view-ctrl-ui.xml", "EOG", NULL);

		bonobo_ui_component_add_verb_list_with_data (priv->uic, eog_zoom_verbs,
							     image_view);
	}
}

/* ***************************************************************************
 * Start of property-bag related code
 * ***************************************************************************/
static void
eog_image_view_get_prop (BonoboPropertyBag *bag,
			 BonoboArg         *arg,
			 guint              arg_id,
			 CORBA_Environment *ev,
			 gpointer           user_data)
{
	EogImageView *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);
	priv = image_view->priv;

	/* FIXME: add props for transparency and interpolation */
	switch (arg_id) {
	case PROP_IMAGE_WIDTH: {
		int width = 0;
		int height = 0;

		g_assert (arg->_type == BONOBO_ARG_INT);

		if (priv->image != NULL) {
			eog_image_get_size (priv->image, &width, &height);
		}
		BONOBO_ARG_SET_INT (arg, width);

		break;
	}
	case PROP_IMAGE_HEIGHT: {
		int width = 0;
		int height = 0;

		g_assert (arg->_type == BONOBO_ARG_INT);

		if (priv->image != NULL) {
			eog_image_get_size (priv->image, &width, &height);
		}
		BONOBO_ARG_SET_INT (arg, height);

		break;
	}
	case PROP_WINDOW_TITLE: {
		g_assert (arg->_type == BONOBO_ARG_STRING);

		if (priv->image != NULL) {
			BONOBO_ARG_SET_STRING (arg, eog_image_get_caption (priv->image));
		}

		break;
	}
	case PROP_WINDOW_STATUS: {
		gchar *text = NULL;
		int zoom = 0;
		int width = 0;
		int height = 0;

		g_assert (arg->_type == BONOBO_ARG_STRING);

		zoom = floor (100 * eog_scroll_view_get_zoom (EOG_SCROLL_VIEW (priv->widget)));

		if (priv->image == NULL) {
			text = g_strdup (" ");
		}
		else {
			eog_image_get_size (priv->image, &width, &height);

			if ((width > 0) && (height > 0)) {
				text = g_strdup_printf ("%i x %i %s    %i%%", 
							width, height, 
							_("pixel"), zoom);
			} 
			else { 
				text = g_strdup_printf ("%i%%", zoom);
			} 
		}

		BONOBO_ARG_SET_STRING (arg, text);
		g_free (text);
		break;
	}
	case PROP_WINDOW_WIDTH:
	case PROP_WINDOW_HEIGHT: {
		int width = -1;
		int height = -1;

		g_assert (arg->_type == BONOBO_ARG_INT);

		if (priv->image != NULL) {
			int sw, sh;
			int img_width, img_height;
			eog_image_get_size (priv->image, &img_width, &img_height);

			sw = gdk_screen_width ();
			sh = gdk_screen_height ();

			if ((img_width >= sw) || (img_height >= sh)) {
				double factor;
				if (img_width > img_height) {
					factor = (sw * 0.75) / (double) img_width;
				}
				else {
					factor = (sh * 0.75) / (double) img_height;
				}
				width = img_width * factor;
				height = img_height * factor;
			}
			else {
				width = img_width;
				height = img_height;
			}
		}

		if (arg_id == PROP_WINDOW_WIDTH)
			BONOBO_ARG_SET_INT (arg, width);
		else
			BONOBO_ARG_SET_INT (arg, height);
		break;
	}
	default:
		g_assert_not_reached ();
	}
}

static void
eog_image_view_set_prop (BonoboPropertyBag *bag,
			 const BonoboArg   *arg,
			 guint              arg_id,
			 CORBA_Environment *ev,
			 gpointer           user_data)
{
	EogImageView        *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);
	priv = image_view->priv;

	/* FIXME: add transp style, transp color and interpolation properties */
}

GConfClient*
eog_image_view_get_client (EogImageView *image_view)
{
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	return image_view->priv->client;
}


/* ***************************************************************************
 * Constructor, destructor, etc.
 * ***************************************************************************/

static void
eog_image_view_destroy (BonoboObject *object)
{
	EogImageView *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (object));

	image_view = EOG_IMAGE_VIEW (object);
	priv = image_view->priv;

	gconf_client_notify_remove (priv->client, priv->interp_type_notify_id);
	gconf_client_notify_remove (priv->client, priv->transparency_notify_id);
	gconf_client_notify_remove (priv->client, priv->trans_color_notify_id);
	g_object_unref (G_OBJECT (priv->client));

	bonobo_object_unref (BONOBO_OBJECT (priv->property_bag));
	bonobo_object_unref (BONOBO_OBJECT (priv->uic));

	if (priv->image != NULL) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	BONOBO_CALL_PARENT (BONOBO_OBJECT_CLASS, destroy, (object));
}

static void
eog_image_view_finalize (GObject *object)
{
	EogImageView *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (object));

	image_view = EOG_IMAGE_VIEW (object);
	priv = image_view->priv;

	g_object_unref (G_OBJECT (priv->item_factory));
	priv->item_factory = NULL;

	g_free (priv);
	image_view->priv = NULL;

	BONOBO_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_image_view_class_init (EogImageViewClass *klass)
{
	BonoboObjectClass *bonobo_object_class = (BonoboObjectClass *)klass;
	GObjectClass *gobject_class = (GObjectClass *)klass;

	bonobo_object_class->destroy = eog_image_view_destroy;
	gobject_class->finalize = eog_image_view_finalize;

	signals[CLOSE_ITEM_ACTIVATED] =
		g_signal_new ("close_item_activated",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageViewClass, close_item_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	klass->close_item_activated = NULL;
}

static void
eog_image_view_instance_init (EogImageView *image_view)
{
	image_view->priv = g_new0 (EogImageViewPrivate, 1);
	image_view->priv->image = NULL;
}


static void
interp_type_changed_cb (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	EogImageView *view;
	gboolean interpolate = TRUE;

	view = EOG_IMAGE_VIEW (user_data);

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_BOOL) {
		interpolate = gconf_value_get_bool (entry->value);
	}

	eog_scroll_view_set_antialiasing (EOG_SCROLL_VIEW (view->priv->widget), interpolate);
}

static void
transparency_changed_cb (GConfClient *client,
			 guint        cnxn_id,
			 GConfEntry  *entry,
			 gpointer     user_data)
{
	EogImageViewPrivate *priv;
	const char *value = NULL;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	priv = EOG_IMAGE_VIEW (user_data)->priv;

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		value = gconf_value_get_string (entry->value);
	}

	if (g_strcasecmp (value, "COLOR") == 0) {
		GdkColor color;
		char *color_str;

		color_str = gconf_client_get_string (priv->client,
						     GCONF_EOG_VIEW_TRANS_COLOR, NULL);
		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
							  TRANSP_COLOR, &color);
		}
	}
	else if (g_strcasecmp (value, "CHECK_PATTERN") == 0) {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
						  TRANSP_CHECKED, 0);
	}
	else {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
						  TRANSP_BACKGROUND, 0);
	}
}

static void
trans_color_changed_cb (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	EogImageViewPrivate *priv;
	GdkColor color;
	char *value;
	const char *color_str;

	priv = EOG_IMAGE_VIEW (user_data)->priv;

	value = gconf_client_get_string (priv->client, GCONF_EOG_VIEW_TRANSPARENCY, NULL);

	if (g_strcasecmp (value, "COLOR") != 0) return;

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		color_str = gconf_value_get_string (entry->value);

		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
							  TRANSP_COLOR, &color);
		}
	}
}

/* Callback for the item factory's popup menu */
static void
popup_menu_cb (gpointer data, guint action, GtkWidget *widget)
{
	EogImageView *image_view;
	EogImageViewPrivate *priv;

	image_view = EOG_IMAGE_VIEW (data);
	priv = image_view->priv;

	switch (action) {
	case POPUP_ZOOM_IN:
		eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (priv->widget), FALSE);
		break;

	case POPUP_ZOOM_OUT:
		eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (priv->widget), FALSE);
		break;

	case POPUP_ZOOM_1:
		eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (priv->widget), 1.0);
		break;

	case POPUP_ZOOM_FIT:
		eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (priv->widget));
		break;

	case POPUP_CLOSE:
		g_signal_emit (image_view, signals[CLOSE_ITEM_ACTIVATED], 0);
		break;
	case POPUP_ROTATE_COUNTER_CLOCKWISE:
		apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_rotate_new (270));
		break;
	case POPUP_ROTATE_CLOCKWISE:
		apply_transformation (EOG_IMAGE_VIEW (data), eog_transform_rotate_new (90));
		break;
	default:
		g_assert_not_reached ();
	}
}

static GtkItemFactoryEntry popup_entries[] = {
	{ N_("/Rotate C_lockwise"), NULL, popup_menu_cb, POPUP_ROTATE_CLOCKWISE,
	  "<Item>", NULL },
	{ N_("/Rotate Counte_r Clockwise"), NULL, popup_menu_cb, POPUP_ROTATE_COUNTER_CLOCKWISE,
	  "<Item>", NULL },
	{ "/sep", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/_Zoom In"), NULL, popup_menu_cb, POPUP_ZOOM_IN,
	  "<StockItem>", GTK_STOCK_ZOOM_IN },
	{ N_("/Zoom _Out"), NULL, popup_menu_cb, POPUP_ZOOM_OUT,
	  "<StockItem>", GTK_STOCK_ZOOM_OUT },
	{ N_("/_Normal Size"), NULL, popup_menu_cb, POPUP_ZOOM_1,
	  "<StockItem>", GTK_STOCK_ZOOM_100 },
	{ N_("/Best _Fit"), NULL, popup_menu_cb, POPUP_ZOOM_FIT,
	  "<StockItem>", GTK_STOCK_ZOOM_FIT },
	{ "/sep", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/_Close"), NULL, popup_menu_cb, POPUP_CLOSE,
	  "<StockItem>", GTK_STOCK_CLOSE }
};

static int n_popup_entries = sizeof (popup_entries) / sizeof (popup_entries[0]);

/* Translate function for the GTK+ item factory.  Sigh. */
static gchar *
item_factory_translate_cb (const gchar *path, gpointer data)
{
	return _(path);
}

/* Sets up a GTK+ item factory for the image view */
static void
setup_item_factory (EogImageView *image_view, gboolean need_close_item)
{
	EogImageViewPrivate *priv;
	
	priv = image_view->priv;

	priv->item_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);
	gtk_item_factory_set_translate_func (priv->item_factory, item_factory_translate_cb,
					     NULL, NULL);
	gtk_item_factory_create_items (priv->item_factory,
				       need_close_item ? n_popup_entries : n_popup_entries - 2,
				       popup_entries,
				       image_view);
}


static void
control_set_ui_container (EogImageView *view,
			  Bonobo_UIContainer ui_container)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	bonobo_ui_component_set_container (view->priv->uic, ui_container, NULL);

	eog_image_view_create_ui (view);
}

static void
control_unset_ui_container (EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	bonobo_ui_component_unset_container (view->priv->uic, NULL);
}

static void
control_activate_cb (BonoboControl *object, gboolean state, gpointer data)
{
	EogImageView *view;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	view = EOG_IMAGE_VIEW (data);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (object, NULL);
		if (ui_container != CORBA_OBJECT_NIL) {
			control_set_ui_container (view, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}

	} else
		control_unset_ui_container (view);
}

static void
image_size_prepared_cb (EogImage *img, int width, int height, gpointer data)
{
	EogImageView *view;
	EogImageViewPrivate *priv;
	BonoboArg *arg;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	view = EOG_IMAGE_VIEW (data);
	priv = view->priv;

	arg = bonobo_arg_new (BONOBO_ARG_INT);

	eog_image_view_get_prop (NULL, arg, PROP_WINDOW_WIDTH, NULL, view);
	bonobo_event_source_notify_listeners (priv->property_bag->es,
					      "window/width",
					      arg, NULL);

	eog_image_view_get_prop (NULL, arg, PROP_WINDOW_HEIGHT, NULL, view);
	bonobo_event_source_notify_listeners (priv->property_bag->es,
					      "window/height",
					      arg, NULL);
	bonobo_arg_release (arg);
}

static void
image_progress_cb (EogImage *img, float progress, gpointer data)
{
	EogImageView *view;
	EogImageViewPrivate *priv;
	BonoboArg *arg;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	view = EOG_IMAGE_VIEW (data);
	priv = view->priv;

	arg = bonobo_arg_new (BONOBO_ARG_FLOAT);
	BONOBO_ARG_SET_FLOAT (arg, progress);

	bonobo_event_source_notify_listeners (priv->property_bag->es,
					      "image/progress",
					      arg, NULL);
	bonobo_arg_release (arg);
}

static void
image_changed_cb (EogImage *img, gpointer data)
{
	EogImageView *view;
	BonoboArg *arg;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));
	g_return_if_fail (EOG_IS_IMAGE (img));
	
	view = EOG_IMAGE_VIEW (data);

	/* update interested window status listeners */
	arg = bonobo_arg_new (BONOBO_ARG_STRING);
	eog_image_view_get_prop (NULL, arg, PROP_WINDOW_STATUS, NULL, view);

	bonobo_event_source_notify_listeners (view->priv->property_bag->es,
					      "window/status",
					      arg, NULL);
	bonobo_arg_release (arg);
}

static void
image_loading_failed_cb (EogImage *img, const char* message, gpointer data)
{
	EogImageView *view; 
	EogImageViewPrivate *priv;
	GtkWidget *dlg;
	BonoboArg *arg;
	char *body;
	char *caption;
	char *exp;
	int caption_len;
	int exp_len;
	int msg_len;

	view = EOG_IMAGE_VIEW (data);
	priv = view->priv;

	/* assemble error message */
	caption = eog_image_get_caption (img);
	caption_len = strlen (caption);

	if (message == NULL) {
		exp = _("Loading of image %s failed.");
		exp_len = strlen (exp);
		
		body = g_new0 (char, caption_len + exp_len);
		g_snprintf (body, caption_len + exp_len, exp, caption);
	}
	else {
		exp = _("Loading of image %s failed.\nReason: %s.");
		exp_len = strlen (exp);
		msg_len = strlen (message);

		body = g_new0 (char, caption_len + exp_len + msg_len);
		g_snprintf (body, caption_len + exp_len + msg_len, exp, caption, message);
	}

	/* show error dialog */
	dlg = eog_hig_dialog_new (GTK_STOCK_DIALOG_ERROR, _("Loading failed"), body, FALSE);
	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);
	g_signal_connect_swapped (G_OBJECT (dlg), "response", G_CALLBACK (gtk_widget_destroy), dlg);
	bonobo_control_set_transient_for (BONOBO_CONTROL (priv->control), GTK_WINDOW (dlg), NULL);
	gtk_widget_show (dlg);

	g_free (body);
	g_object_unref (priv->image);
	priv->image = NULL;
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->widget), NULL);

	/* update interested window status listeners */
	arg = bonobo_arg_new (BONOBO_ARG_STRING);
	eog_image_view_get_prop (NULL, arg, PROP_WINDOW_STATUS, NULL, view);

	bonobo_event_source_notify_listeners (priv->property_bag->es,
					      "window/status",
					      arg, NULL);
	bonobo_arg_release (arg);
}

static gint
load_uri_cb (BonoboPersistFile *pf, const CORBA_char *text_uri,
	     CORBA_Environment *ev, void *closure)
{
	EogImageView *view;
	EogImageViewPrivate *priv;
	EogImage *image;
	char *valid_uri;

	view = EOG_IMAGE_VIEW (closure);
	priv = view->priv;

	if (priv->image != NULL) {
		int i;

		for (i = 0; i < 4; i++) {
			g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_signal_id[i]);
			priv->image_signal_id[i] = 0;
		}
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	valid_uri = eel_make_uri_from_input (text_uri);

	image = eog_image_new (valid_uri);
	priv->image_signal_id[0] = g_signal_connect (image, "loading_size_prepared", G_CALLBACK (image_size_prepared_cb), view);
	priv->image_signal_id[1] = g_signal_connect (image, "progress", G_CALLBACK (image_progress_cb), view);
	priv->image_signal_id[2] = g_signal_connect (image, "image_changed", G_CALLBACK (image_changed_cb), view);
	priv->image_signal_id[3] = g_signal_connect (image, "loading_failed", G_CALLBACK (image_loading_failed_cb), view);

	priv->image = image;

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->widget), image);
	g_free (valid_uri);

	return 0;
}

static gint
save_uri_cb (BonoboPersistFile *pf, const CORBA_char *text_uri,
	     CORBA_Environment *ev, void *closure)
{
	EogImageView *view;
	GnomeVFSURI *uri;
	GError *error = NULL;
	GtkWidget *dialog;
	gboolean result;

	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (closure), 1);
	g_return_val_if_fail (text_uri != NULL, 1);

	view = EOG_IMAGE_VIEW (closure);
	if (view->priv->image == NULL) return FALSE;

	/* FIXME: what kind of escaping do we need here? */
	uri = gnome_vfs_uri_new (text_uri);

	result = eog_image_save (view->priv->image, uri, &error);

	if (result) {
		dialog = eog_hig_dialog_new (GTK_STOCK_DIALOG_INFO,
					     _("Image successfully saved"), NULL, FALSE);
	}
	else {
		dialog = eog_hig_dialog_new (GTK_STOCK_DIALOG_ERROR,
					     _("Image saving failed"), error->message, FALSE);
	}
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	g_signal_connect_swapped (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);
	gtk_widget_show (dialog);

	if (error != NULL) {
		g_error_free (error);
	}

	return result;
}

static void
zoomable_set_frame_cb (BonoboZoomable *zoomable, EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	view->priv->has_zoomable_frame = TRUE;
}

static void
zoomable_set_zoom_level_cb (BonoboZoomable *zoomable, CORBA_float new_zoom_level,
			    EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (view->priv->widget), new_zoom_level);
}

static void
zoomable_zoom_in_cb (BonoboZoomable *zoomable, EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (view->priv->widget), FALSE);
}

static void
zoomable_zoom_out_cb (BonoboZoomable *zoomable, EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (view->priv->widget), FALSE);
}

static void
zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable, EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (view->priv->widget));
}

static void
zoomable_zoom_to_default_cb (BonoboZoomable *zoomable, EogImageView *view)
{
	g_return_if_fail (EOG_IS_IMAGE_VIEW (view));

	eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (view->priv->widget), 1.0);
}

static void
init_gconf_defaults (EogImageView *view)
{
	EogImageViewPrivate *priv;
	char *transp_str;

	priv = view->priv;

	/* setup gconf stuff */
	if (!gconf_is_initialized ())
		gconf_init (0, NULL, NULL);

	priv->client = gconf_client_get_default ();
	gconf_client_add_dir (priv->client,
			      GCONF_EOG_VIEW_DIR,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);


	/* get preference values from gconf */
	eog_scroll_view_set_antialiasing (EOG_SCROLL_VIEW (priv->widget),
					  gconf_client_get_bool (priv->client, GCONF_EOG_VIEW_INTERP_TYPE, NULL));

	transp_str = gconf_client_get_string (priv->client,
					      GCONF_EOG_VIEW_TRANSPARENCY, NULL);
	if (g_strcasecmp (transp_str, "COLOR") == 0) {
		GdkColor color;
		char *color_str;

		color_str = gconf_client_get_string (priv->client,
						     GCONF_EOG_VIEW_TRANS_COLOR, NULL);
		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
							  TRANSP_COLOR, &color);
		}
	}
	else if (g_strcasecmp (transp_str, "CHECK_PATTERN") == 0) {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
						  TRANSP_CHECKED, 0);
	}
	else {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->widget),
						  TRANSP_BACKGROUND, 0);
	}

	/* add gconf listeners */
	priv->interp_type_notify_id =
		gconf_client_notify_add (priv->client,
					 GCONF_EOG_VIEW_INTERP_TYPE,
					 interp_type_changed_cb,
					 view, NULL, NULL);
	priv->transparency_notify_id =
		gconf_client_notify_add (priv->client,
					 GCONF_EOG_VIEW_TRANSPARENCY,
					 transparency_changed_cb,
					 view, NULL, NULL);
	priv->trans_color_notify_id =
		gconf_client_notify_add (priv->client,
					 GCONF_EOG_VIEW_TRANS_COLOR,
					 trans_color_changed_cb,
					 view, NULL, NULL);
}

static void
view_zoom_changed_cb (GtkWidget *widget, double zoom, gpointer data)
{
	EogImageView *view;
	EogImageViewPrivate *priv;
	BonoboArg *arg;

	view = EOG_IMAGE_VIEW (data);
	priv = view->priv;

	/* inform zoom interface listeners */
	bonobo_zoomable_report_zoom_level_changed (priv->zoomable, zoom, NULL);

	/* FIXME: normally it should be sufficient to just notify
	 * the bonobo_zoomable listeners.
	 */
	/* also update interested window status listeners */
	arg = bonobo_arg_new (BONOBO_ARG_STRING);
	eog_image_view_get_prop (NULL, arg, PROP_WINDOW_STATUS, NULL, view);

	bonobo_event_source_notify_listeners (priv->property_bag->es,
					      "window/status",
					      arg, NULL);
	bonobo_arg_release (arg);

}

static EogImageView *
eog_image_view_construct (EogImageView *image_view, gboolean need_close_item)
{
	EogImageViewPrivate *priv;
	BonoboControl *control;
	BonoboZoomable *zoomable;

	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	priv = image_view->priv;

	/* create the widgets */
	priv->image = NULL;

	priv->widget = eog_scroll_view_new ();
	gtk_widget_show (priv->widget);

	g_signal_connect (G_OBJECT (priv->widget),
			  "zoom_changed",
			  (GCallback) view_zoom_changed_cb,
			  image_view);
	g_signal_connect (priv->widget,
			  "button_press_event",
			  G_CALLBACK (view_button_press_event_cb),
			  image_view);
	g_signal_connect (priv->widget,
			  "popup_menu",
			  G_CALLBACK (view_popup_menu_cb),
			  image_view);

	/* interface Bonobo::PersistFile */
	bonobo_persist_file_construct (BONOBO_PERSIST_FILE (image_view),
				       load_uri_cb, save_uri_cb,
				       "OAFIID:GNOME_EOG_Control",
				       image_view);


	/* interface Bonobo::Control */
	control = bonobo_control_new (priv->widget);
	priv->control = control;
	g_signal_connect (control, "activate", G_CALLBACK (control_activate_cb), image_view);

	bonobo_object_add_interface (BONOBO_OBJECT (image_view),
				     BONOBO_OBJECT (control));

	/* Interface Bonobo::Zoomable */
	zoomable = bonobo_zoomable_new ();
	priv->zoomable = zoomable;

	g_signal_connect (G_OBJECT (zoomable),
			  "set_frame",
			  G_CALLBACK (zoomable_set_frame_cb),
			  image_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "set_zoom_level",
			  G_CALLBACK (zoomable_set_zoom_level_cb),
			  image_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_in",
			  G_CALLBACK (zoomable_zoom_in_cb),
			  image_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_out",
			  G_CALLBACK (zoomable_zoom_out_cb),
			  image_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_to_fit",
			  G_CALLBACK (zoomable_zoom_to_fit_cb),
			  image_view);
	g_signal_connect (G_OBJECT (zoomable),
			  "zoom_to_default",
			  G_CALLBACK (zoomable_zoom_to_default_cb),
			  image_view);

	bonobo_zoomable_set_parameters (zoomable,
					1.0,   /* current */
					0.01,  /* min */
					20.0,  /* max */
					TRUE,
					TRUE);

	bonobo_object_add_interface (BONOBO_OBJECT (image_view),
				     BONOBO_OBJECT (zoomable));


	/* Property Bag */
	priv->property_bag = bonobo_property_bag_new (eog_image_view_get_prop,
						      eog_image_view_set_prop,
						      image_view);
	bonobo_property_bag_add (priv->property_bag, "image/width", PROP_IMAGE_WIDTH,
				 BONOBO_ARG_INT, NULL, _("Image Width"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "image/height", PROP_IMAGE_HEIGHT,
				 BONOBO_ARG_INT, NULL, _("Image Height"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "window/title", PROP_WINDOW_TITLE,
				 BONOBO_ARG_STRING, NULL, _("Window Title"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "window/status", PROP_WINDOW_STATUS,
				 BONOBO_ARG_STRING, NULL, _("Statusbar Text"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "window/width", PROP_WINDOW_WIDTH,
				 BONOBO_ARG_INT, NULL, _("Desired Window Width"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "window/height", PROP_WINDOW_HEIGHT,
				 BONOBO_ARG_INT, NULL, _("Desired Window Height"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (priv->property_bag, "image/progress", PROP_IMAGE_PROGRESS,
				 BONOBO_ARG_FLOAT, NULL, _("Progress of Image Loading"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_control_set_properties (BONOBO_CONTROL (control),
				       BONOBO_OBJREF (priv->property_bag),
				       NULL);

	init_gconf_defaults (image_view);

	/* UI Component */
	priv->uic = bonobo_ui_component_new ("EogImageView");

	/* init popup menu */
	setup_item_factory (image_view, need_close_item);

	return image_view;
}

EogImageView *
eog_image_view_new (gboolean need_close_item)
{
	EogImageView *image_view;

	image_view = g_object_new (EOG_IMAGE_VIEW_TYPE, NULL);

	return eog_image_view_construct (image_view, need_close_item);
}


