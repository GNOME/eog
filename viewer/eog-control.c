/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-control.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000 SuSE GmbH.
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkscrolledwindow.h>

#include <gnome.h>

#include <eog-control.h>

struct _EogControlPrivate {
	EogImageView       *image_view;

	BonoboZoomable     *zoomable;
	float               zoom_level;
	gboolean            has_zoomable_frame;
};

static GObjectClass *eog_control_parent_class;

static void
eog_control_destroy (BonoboObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	if (getenv ("DEBUG_EOG"))
		g_message ("Destroying EogControl...");

	control = EOG_CONTROL (object);

//BEWARE: After this has been added by bonobo_object_add_interface, 
//        we don't own this anymore. Therefore, we cannot touch it.
//	bonobo_object_unref (BONOBO_OBJECT (control->priv->image_view));
//	bonobo_object_unref (BONOBO_OBJECT (control->priv->zoomable));

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
zoomable_set_zoom_level_cb (BonoboZoomable *zoomable, float new_zoom_level,
			    EogControl *control)
{
	EogControlPrivate *priv;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	priv = control->priv;

	eog_image_view_set_zoom_factor (priv->image_view, new_zoom_level);
	priv->zoom_level = eog_image_view_get_zoom_factor (priv->image_view);
	bonobo_zoomable_report_zoom_level_changed (zoomable, priv->zoom_level, 
						   NULL);
}

static float preferred_zoom_levels[] = {
	1.0 / 10.0, 1.0 / 9.0, 1.0 / 8.0, 1.0 / 7.0, 1.0 / 6.0,
	1.0 / 5.0, 1.0 / 4.0, 1.0 / 3.0, 1.0 / 2.0, 1.0, 2.0,
	3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0
};
static const gchar *preferred_zoom_level_names[] = {
	"1:10", "1:9", "1:8", "1:7", "1:6", "1:5", "1:4", "1:3",
	"1:2", "1:1", "2:1", "3:1", "4:1", "5:1", "6:1", "7:1",
	"8:1", "9:1", "10:1"
};

static const gint max_preferred_zoom_levels = (sizeof (preferred_zoom_levels) /
					       sizeof (float)) - 1;

static int
zoom_index_from_float (float zoom_level)
{
	int i;

	for (i = 0; i < max_preferred_zoom_levels; i++) {
		float this, epsilon;

		/* if we're close to a zoom level */
		this = preferred_zoom_levels [i];
		epsilon = this * 0.01;

		if (zoom_level < this+epsilon)
			return i;
	}

	return max_preferred_zoom_levels;
}

static float
zoom_level_from_index (int index)
{
	if (index > max_preferred_zoom_levels)
		index = max_preferred_zoom_levels;

	return preferred_zoom_levels [index];
}

static void
zoomable_zoom_in_cb (BonoboZoomable *zoomable, EogControl *control)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	index = zoom_index_from_float (control->priv->zoom_level);
	if (index == max_preferred_zoom_levels)
		return;

	index++;
	new_zoom_level = zoom_level_from_index (index);

	g_signal_emit_by_name (G_OBJECT (zoomable), "set_zoom_level",
			       new_zoom_level);
}

static void
zoomable_zoom_out_cb (BonoboZoomable *zoomable, EogControl *control)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	index = zoom_index_from_float (control->priv->zoom_level);
	if (index == 0)
		return;

	index--;
	new_zoom_level = zoom_level_from_index (index);

	g_signal_emit_by_name (G_OBJECT (zoomable), "set_zoom_level",
			       new_zoom_level);
}

static void
zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable, EogControl *control)
{
	float new_zoom_level;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_zoom_to_fit (control->priv->image_view, TRUE);
	new_zoom_level = eog_image_view_get_zoom_factor
		(control->priv->image_view);

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

static const gchar *zoom_toolbar =
"<dockitem name=\"Toolbar\">\n"
"  <separator/>\n"
"  <toolitem name=\"ZoomIn\" _label=\"In\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-in.xpm\" verb=\"\"/>\n"
"  <toolitem name=\"ZoomOut\" _label=\"Out\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-out.xpm\" verb=\"\"/>\n"
"  <toolitem name=\"ZoomToDefault\" _label=\"1:1\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-1.xpm\" verb=\"\"/>\n"
"  <toolitem name=\"ZoomToFit\" _label=\"Fit\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-fit.xpm\" verb=\"\"/>\n"
"</dockitem>";

static const gchar *zoom_menu =
"<placeholder name=\"ZoomOperations\">\n"
"  <menuitem name=\"ZoomIn\" _label=\"Zoom _In\" verb=\"\"/>\n"
"  <menuitem name=\"ZoomOut\" _label=\"Zoom _Out\" verb=\"\"/>\n"
"  <menuitem name=\"ZoomToDefault\" _label=\"Zoom to _Default\" verb=\"\"/>\n"
"  <menuitem name=\"ZoomToFit\" _label=\"Zoom to _Fit\" verb=\"\"/>\n"
"</placeholder>";

static void
eog_control_create_ui (EogControl *control)
{
	BonoboUIComponent *uic;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	uic = bonobo_control_get_ui_component (BONOBO_CONTROL (control));

	bonobo_ui_component_set_translate (uic, "/menu/ViewPlaceholder/View", 
					   zoom_menu, NULL);
	bonobo_ui_component_set_translate (uic, "/", zoom_toolbar, NULL);

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
	control->priv = g_new0 (EogControlPrivate, 1);
}

BONOBO_TYPE_FUNC (EogControl, BONOBO_TYPE_CONTROL, eog_control);

EogControl *
eog_control_construct (EogControl    *control,
		       EogImage      *image)
{
	GtkWidget             *widget;
	BonoboControl         *retval;
	BonoboPropertyBag     *pb;
	BonoboPropertyControl *pc;
	EogControlPrivate     *priv;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (control != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (EOG_IS_CONTROL (control), NULL);

	priv = control->priv;

	if (!eog_image_add_interfaces (image, BONOBO_OBJECT (control)))
		return NULL;

	/* Create the image-view */
	priv->image_view = eog_image_view_new (image, FALSE);
	if (!priv->image_view) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}
	widget = eog_image_view_get_widget (priv->image_view);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget),
					     GTK_SHADOW_IN);

	retval = bonobo_control_construct (BONOBO_CONTROL (control), widget);
	
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

	priv->zoom_level = 1.0;
	bonobo_zoomable_set_parameters_full (priv->zoomable,
					     priv->zoom_level,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [max_preferred_zoom_levels],
					     TRUE, TRUE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     max_preferred_zoom_levels + 1);
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (priv->zoomable));

#if NEED_GNOME2_PORTING
	pb = eog_image_view_get_property_bag (priv->image_view);
	bonobo_control_set_properties (BONOBO_CONTROL (control), 
				       BONOBO_OBJREF (pb), 
				       NULL);
	bonobo_object_unref (BONOBO_OBJECT (pb));

	pc = eog_image_view_get_property_control (priv->image_view);
//FIXME: Ok, it seems crazy to get something, unref it, and process it further.
//       But: bonobo_object_add_interface seems to need objects with
//       ref_count == 1, otherwise, eog-image-viewer will never exit (even if
//       it is no longer needed). If you don't believe me, put
//       bonobo_object_unref after bonobo_object_add_interface and check it
//       out...
	bonobo_object_unref (BONOBO_OBJECT (pc));
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (pc));
#endif
	
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
