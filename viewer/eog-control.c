/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-control.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000 SuSE GmbH.
 * Copyright 2001-2002 The Free Software Foundation
 */
#include <config.h>
#include <stdio.h>
#include <math.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkscrolledwindow.h>

#include <gnome.h>

#include "libeog/ui-image.h"
#include "libeog/image-view.h"
#include <eog-control.h>

/* defined in libbonoboui, but not prototyped in any installed headers. */ 
#warning FIXME: bonobo_contro_get_plug() needs to be prototyped in libbonoboui headers 
BonoboPlug *bonobo_control_get_plug(BonoboControl *control); 

/* See plug_size_allocate_cb() below */
#define BROKEN_SIZE_ALLOCATIONS 3

struct _EogControlPrivate {
	EogImageView       *image_view;

	BonoboZoomable     *zoomable;
	gboolean            has_zoomable_frame;

	/* See plug_size_allocate_cb() below */
	int size_allocations_remaining_until_we_get_it_right;
};

static GObjectClass *eog_control_parent_class;

static void
eog_control_destroy (BonoboObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	if (getenv ("DEBUG_EOG"))
		g_message ("Destroying EogControl...");

	BONOBO_OBJECT_CLASS (eog_control_parent_class)->destroy (object);
}

static void
eog_control_finalize (GObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	g_free (control->priv);

	G_OBJECT_CLASS (eog_control_parent_class)->finalize (object);
}

static void
zoomable_set_frame_cb (BonoboZoomable *zoomable, EogControl *control)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	control->priv->has_zoomable_frame = TRUE;
}

static void
zoomable_set_zoom_level_cb (BonoboZoomable *zoomable, CORBA_float new_zoom_level,
			    EogControl *control)
{
	EogControlPrivate *priv;
	double zoomx, zoomy;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	priv = control->priv;

	eog_image_view_set_zoom_factor (priv->image_view, new_zoom_level);

	eog_image_view_get_zoom_factor (priv->image_view, &zoomx, &zoomy);
	bonobo_zoomable_report_zoom_level_changed (zoomable, 
						   sqrt (zoomx * zoomy),
						   NULL);
}

static float preferred_zoom_levels[] = {
	1.0 / 100, 1.0 / 50, 1.0 / 20,
	1.0 / 10.0, 1.0 / 5.0, 1.0 / 3.0, 1.0 / 2.0, 1.0 / 1.5, 
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
        11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0
};
static const gchar *preferred_zoom_level_names[] = {
	"1:100", "1:50", "1:20", "1:10", "1:5", "1:3", "1:2", "2:3",
	"1:1", "2:1", "3:1", "4:1", "5:1", "6:1", "7:1",
	"8:1", "9:1", "10:1", "11:1", "12:1", "13:1", "14:1", "15:1",
	"16:1", "17:1", "18:1", "19:1", "20:1"
};

static const gint n_zoom_levels = (sizeof (preferred_zoom_levels) / sizeof (float));

static double
zoom_level_from_index (int index)
{
	if (index >= 0 && index < n_zoom_levels)
		return preferred_zoom_levels [index];
	else
		return 1.0;
}

static void
zoomable_zoom_in_cb (BonoboZoomable *zoomable, EogControl *control)
{
	double zoomx, zoomy;
	double zoom_level;
	double new_zoom_level;
	int index;
	int i;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_get_zoom_factor (control->priv->image_view, &zoomx, &zoomy);
	zoom_level = sqrt (zoomx * zoomy);
	index = -1;

	/* find next greater zoom level index */
	for (i = 0; i < n_zoom_levels; i++) {
		if (preferred_zoom_levels [i] > zoom_level) {
			index = i;
			break;
		}
	}

	if (index == -1) return;

	new_zoom_level = zoom_level_from_index (index);

	g_signal_emit_by_name (G_OBJECT (zoomable), "set_zoom_level",
			       new_zoom_level);
}

static void
zoomable_zoom_out_cb (BonoboZoomable *zoomable, EogControl *control)
{
	double zoomx, zoomy;
	double zoom_level;
	double new_zoom_level;
	int index, i;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_get_zoom_factor (control->priv->image_view, &zoomx, &zoomy);
	zoom_level = sqrt (zoomx * zoomy);
	index = -1;
	
	/* find next lower zoom level index */
	for (i = n_zoom_levels - 1; i >= 0; i--) {
		if (preferred_zoom_levels [i] < zoom_level) {
			index = i;
			break;
		}
	}
	if (index == -1)
		return;

	new_zoom_level = zoom_level_from_index (index);

	g_signal_emit_by_name (G_OBJECT (zoomable), "set_zoom_level",
			       new_zoom_level);
}

static void
zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable, EogControl *control)
{
	double zoomx, zoomy;
	double new_zoom_level;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_zoom_to_fit (control->priv->image_view, TRUE);
	eog_image_view_get_zoom_factor (control->priv->image_view, &zoomx, &zoomy);
	new_zoom_level = sqrt (zoomx * zoomy);

	g_signal_emit_by_name (G_OBJECT (zoomable), "set_zoom_level",
			       new_zoom_level);
}

static void
zoomable_zoom_to_default_cb (BonoboZoomable *zoomable, EogControl *control)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	g_signal_emit_by_name (G_OBJECT (zoomable), "set_zoom_level", 1.0);
}

static void
verb_ZoomIn_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogControl *control;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (user_data));

	control = EOG_CONTROL (user_data);

	g_signal_emit_by_name (G_OBJECT (control->priv->zoomable),
			       "zoom_in");
}

static void
verb_ZoomOut_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogControl *control;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (user_data));

	control = EOG_CONTROL (user_data);

	g_signal_emit_by_name (G_OBJECT (control->priv->zoomable),
			       "zoom_out");
}

static void
verb_ZoomToDefault_cb (BonoboUIComponent *uic, gpointer user_data,
		       const char *cname)
{
	EogControl *control;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (user_data));

	control = EOG_CONTROL (user_data);

	g_signal_emit_by_name (G_OBJECT (control->priv->zoomable),
			       "zoom_to_default");
}

static void
verb_ZoomToFit_cb (BonoboUIComponent *uic, gpointer user_data,
		   const char *cname)
{
	EogControl *control;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (user_data));

	control = EOG_CONTROL (user_data);

	g_signal_emit_by_name (G_OBJECT (control->priv->zoomable),
			       "zoom_to_fit");
}

static BonoboUIVerb eog_control_verbs[] = {
	BONOBO_UI_VERB ("ZoomIn",        verb_ZoomIn_cb),
	BONOBO_UI_VERB ("ZoomOut",       verb_ZoomOut_cb),
	BONOBO_UI_VERB ("ZoomToDefault", verb_ZoomToDefault_cb),
	BONOBO_UI_VERB ("ZoomToFit",     verb_ZoomToFit_cb),
	BONOBO_UI_VERB_END
};

static void
eog_control_create_ui (EogControl *control)
{
	BonoboUIComponent *uic;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	uic = bonobo_control_get_ui_component (BONOBO_CONTROL (control));

	bonobo_ui_util_set_ui (uic, DATADIR, "eog-image-view-ctrl-ui.xml", "EOG", NULL);

	bonobo_ui_component_add_verb_list_with_data (uic, eog_control_verbs,
						     control);
}

static void
eog_control_set_ui_container (EogControl *control,
			      Bonobo_UIContainer ui_container)
{
	BonoboUIComponent *uic;
	
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	eog_image_view_set_ui_container (control->priv->image_view,
					 ui_container);

	uic = bonobo_control_get_ui_component (BONOBO_CONTROL (control));
	bonobo_ui_component_set_container (uic, ui_container, NULL);

	if (!control->priv->has_zoomable_frame)
		eog_control_create_ui (control);
}

static void
eog_control_unset_ui_container (EogControl *control)
{
	BonoboUIComponent *uic;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_unset_ui_container (control->priv->image_view);

	uic = bonobo_control_get_ui_component (BONOBO_CONTROL (control));
	bonobo_ui_component_unset_container (uic, NULL);
}

static void
eog_control_activate (BonoboControl *object, gboolean state)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (control), NULL);
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_control_set_ui_container (control, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else
		eog_control_unset_ui_container (control);

	if (BONOBO_CONTROL_CLASS (eog_control_parent_class)->activate)
		BONOBO_CONTROL_CLASS (eog_control_parent_class)->activate (object, state);
}

static void
eog_control_class_init (EogControl *klass)
{
	GObjectClass *gobject_class = (GObjectClass *)klass;
	BonoboObjectClass *bonobo_object_class = (BonoboObjectClass *)klass;
	BonoboControlClass *control_class = (BonoboControlClass *)klass;

	eog_control_parent_class = g_type_class_peek_parent (klass);

	bonobo_object_class->destroy = eog_control_destroy;
	gobject_class->finalize = eog_control_finalize;
	control_class->activate = eog_control_activate;
}

static void
eog_control_init (EogControl *control)
{
	EogControlPrivate *priv;

	priv = g_new0 (EogControlPrivate, 1);
	control->priv = priv;

	priv->size_allocations_remaining_until_we_get_it_right = BROKEN_SIZE_ALLOCATIONS;
}

BONOBO_TYPE_FUNC (EogControl, BONOBO_TYPE_CONTROL, eog_control);

/* FIXME: This is a horrible hack.  The plug in the control gets size_allocated
 * *three* times, sometimes more, before the user can actually interact with it.
 * So we count this number of allocations and manually fit-to-window when we hit
 * the limit.  This needs to be tracked down in GTK+ and Bonobo, but it is a total
 * bitch to fix.
 */
static void
plug_size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	EogControl *control;
	EogControlPrivate *priv;

	control = EOG_CONTROL (data);
	priv = control->priv;

	if (priv->size_allocations_remaining_until_we_get_it_right == 0)
		return;

	priv->size_allocations_remaining_until_we_get_it_right--;
	if (priv->size_allocations_remaining_until_we_get_it_right == 0)
		eog_image_view_zoom_to_fit (priv->image_view, TRUE);
}

/* Callback used when the image view's zoom factor changes */
static void
zoom_changed_cb (ImageView *view, gpointer data)
{
	EogControl *control;
	EogControlPrivate *priv;
	double zoomx, zoomy;

	control = EOG_CONTROL (data);
	priv = control->priv;

	eog_image_view_get_zoom_factor (priv->image_view, &zoomx, &zoomy);
	bonobo_zoomable_report_zoom_level_changed (priv->zoomable, sqrt (zoomx * zoomy), NULL);
}

EogControl *
eog_control_construct (EogControl    *control,
		       EogImage      *image)
{
	GtkWidget             *widget;
	BonoboPropertyBag     *pb;
	EogControlPrivate     *priv;
	BonoboPlug            *plug;
	ImageView             *image_view;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (control != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (EOG_IS_CONTROL (control), NULL);

	priv = control->priv;

	if (!eog_image_add_interfaces (image, BONOBO_OBJECT (control)))
		return NULL;

	/* Create the image-view */
	priv->image_view = eog_image_view_new (image, FALSE, FALSE);
	if (!priv->image_view) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}
	widget = eog_image_view_get_widget (priv->image_view);
	image_view = IMAGE_VIEW (ui_image_get_image_view (UI_IMAGE (widget)));

	bonobo_control_construct (BONOBO_CONTROL (control), widget);

	g_signal_connect (image_view, "zoom_changed",
			  G_CALLBACK (zoom_changed_cb), control);

	plug = bonobo_control_get_plug (BONOBO_CONTROL (control));
	g_signal_connect (plug, "size_allocate",
			  G_CALLBACK (plug_size_allocate_cb), control);
	
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (priv->image_view));

	/* Interface Bonobo::Zoomable */
	control->priv->zoomable = bonobo_zoomable_new ();

	g_signal_connect (G_OBJECT (priv->zoomable),
			  "set_frame",
			  G_CALLBACK (zoomable_set_frame_cb),
			  control);
	g_signal_connect (G_OBJECT (priv->zoomable),
			  "set_zoom_level",
			  G_CALLBACK (zoomable_set_zoom_level_cb),
			  control);
	g_signal_connect (G_OBJECT (priv->zoomable),
			  "zoom_in",
			  G_CALLBACK (zoomable_zoom_in_cb),
			  control);
	g_signal_connect (G_OBJECT (priv->zoomable),
			  "zoom_out",
			  G_CALLBACK (zoomable_zoom_out_cb),
			  control);
	g_signal_connect (G_OBJECT (priv->zoomable),
			  "zoom_to_fit",
			  G_CALLBACK (zoomable_zoom_to_fit_cb),
			  control);
	g_signal_connect (G_OBJECT (priv->zoomable),
			  "zoom_to_default",
			  G_CALLBACK (zoomable_zoom_to_default_cb),
			  control);

	bonobo_zoomable_set_parameters_full (priv->zoomable,
					     1.0,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [n_zoom_levels-1],
					     TRUE, TRUE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     n_zoom_levels);
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (priv->zoomable));

	pb = eog_image_view_get_property_bag (priv->image_view);
	bonobo_control_set_properties (BONOBO_CONTROL (control), 
				       BONOBO_OBJREF (pb), 
				       NULL);

	return control;
}

EogControl *
eog_control_new (EogImage *image)
{
	EogControl *control;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	if (getenv ("DEBUG_EOG"))
		g_message ("Creating EogControl...");

	control = g_object_new (EOG_CONTROL_TYPE, NULL);

	return eog_control_construct (control, image);
}
