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

#include <gnome.h>

#include <eog-control.h>

struct _EogControlPrivate {
	EogImage           *image;
	EogImageView       *image_view;

	BonoboZoomable     *zoomable;
	float               zoom_level;
	gboolean            has_zoomable_frame;

	GtkWidget          *root;

	BonoboUIComponent  *uic;
};

POA_Bonobo_Control__vepv eog_control_vepv;

static BonoboControlClass *eog_control_parent_class;

static void
init_eog_control_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_control_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_control_vepv.Bonobo_Control_epv = bonobo_control_get_epv ();
}

static void
eog_control_destroy (GtkObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	if (control->priv->image)
		bonobo_object_unref (BONOBO_OBJECT (control->priv->image));
	control->priv->image = NULL;

	if (control->priv->root)
		gtk_widget_unref (control->priv->root);
	control->priv->root = NULL;

	GTK_OBJECT_CLASS (eog_control_parent_class)->destroy (object);
}

static void
eog_control_finalize (GtkObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	g_free (control->priv);

	GTK_OBJECT_CLASS (eog_control_parent_class)->finalize (object);
}

Bonobo_Control
eog_control_corba_object_create (BonoboObject *object)
{
	POA_Bonobo_Control *servant;
	CORBA_Environment ev;
	
	servant = (POA_Bonobo_Control *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_control_vepv;

	CORBA_exception_init (&ev);
	POA_Bonobo_Control__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Bonobo_Control) bonobo_object_activate_servant (object, servant);
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
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_set_zoom_factor
		(control->priv->image_view, new_zoom_level);
	control->priv->zoom_level = eog_image_view_get_zoom_factor
		(control->priv->image_view);

	bonobo_zoomable_report_zoom_level_changed
		(zoomable, control->priv->zoom_level);
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

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
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

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
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

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
				 new_zoom_level);
}

static void
zoomable_zoom_to_default_cb (BonoboZoomable *zoomable, EogControl *control)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", 1.0);
}

static void
verb_ZoomIn_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogControl *control;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (user_data));

	control = EOG_CONTROL (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (control->priv->zoomable),
				 "zoom_in");
}

static void
verb_ZoomOut_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogControl *control;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (user_data));

	control = EOG_CONTROL (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (control->priv->zoomable),
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

	gtk_signal_emit_by_name (GTK_OBJECT (control->priv->zoomable),
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

	gtk_signal_emit_by_name (GTK_OBJECT (control->priv->zoomable),
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
"<dockitem name=\"EogZoomToolbar\" homogeneous=\"1\" hidden=\"1\">\n"
"  <toolitem name=\"ZoomIn\" _label=\"In\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-out.xpm\" verb=\"\"/>\n"
"  <toolitem name=\"ZoomOut\" _label=\"Out\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-in.xpm\" verb=\"\"/>\n"
"  <toolitem name=\"ZoomToDefault\" _label=\"1:1\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-1.xpm\" verb=\"\"/>\n"
"  <toolitem name=\"ZoomToFit\" _label=\"Fit\" pixtype=\"filename\"\n"
"            pixname=\"eog/stock-zoom-fit.xpm\" verb=\"\"/>\n"
"</dockitem>";

static const gchar *zoom_menu =
"<placeholder name=\"ZoomMenu\">\n"
"  <menuitem name=\"ZoomIn\" _label=\"Zoom _In\" verb=\"\"/>\n"
"  <menuitem name=\"ZoomOut\" _label=\"Zoom _Out\" verb=\"\"/>\n"
"  <menuitem name=\"ZoomToDefault\" _label=\"Zoom to _Default\" verb=\"\"/>\n"
"  <menuitem name=\"ZoomToFit\" _label=\"Zoom to _Fit\" verb=\"\"/>\n"
"</placeholder>";

static void
eog_control_create_ui (EogControl *control)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	bonobo_ui_component_set_translate (control->priv->uic,
					   "/menu/EOG", zoom_menu,
					   NULL);

	bonobo_ui_component_set_translate (control->priv->uic,
					   "/", zoom_toolbar, NULL);

	bonobo_ui_component_add_verb_list_with_data
		(control->priv->uic, eog_control_verbs,
		 control);
}

static void
eog_control_set_ui_container (EogControl *control,
			      Bonobo_UIContainer ui_container)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	eog_image_view_set_ui_container (control->priv->image_view,
					 ui_container);

	bonobo_ui_component_set_container (control->priv->uic, ui_container);

	eog_control_create_ui (control);
}

static void
eog_control_unset_ui_container (EogControl *control)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_image_view_unset_ui_container (control->priv->image_view);

	bonobo_ui_component_unset_container (control->priv->uic);
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

		ui_container = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (control));
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
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	BonoboControlClass *control_class = (BonoboControlClass *)klass;

	eog_control_parent_class = gtk_type_class (bonobo_control_get_type ());

	object_class->destroy = eog_control_destroy;
	object_class->finalize = eog_control_finalize;

	control_class->activate = eog_control_activate;

	init_eog_control_corba_class ();
}

static void
eog_control_init (EogControl *control)
{
	control->priv = g_new0 (EogControlPrivate, 1);
}

GtkType
eog_control_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"EogControl",
			sizeof (EogControl),
			sizeof (EogControlClass),
			(GtkClassInitFunc)  eog_control_class_init,
			(GtkObjectInitFunc) eog_control_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (
			bonobo_control_get_type (), &info);
	}

	return type;
}

EogControl *
eog_control_construct (EogControl    *control,
		       Bonobo_Control corba_object,
		       EogImage      *image)
{
	BonoboControl     *retval;
	BonoboPropertyBag *property_bag;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (control != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (EOG_IS_CONTROL (control), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);

	control->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image));

	if (!eog_image_add_interfaces (image, BONOBO_OBJECT (control)))
		return NULL;

	control->priv->image_view = eog_image_view_new (image, FALSE);
	control->priv->root = eog_image_view_get_widget (control->priv->image_view);
	if (!control->priv->image_view) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (control->priv->image_view));

	/* Interface Bonobo::Zoomable */
	control->priv->zoomable = bonobo_zoomable_new ();

	gtk_signal_connect (GTK_OBJECT (control->priv->zoomable),
			    "set_frame",
			    GTK_SIGNAL_FUNC (zoomable_set_frame_cb),
			    control);
	gtk_signal_connect (GTK_OBJECT (control->priv->zoomable),
			    "set_zoom_level",
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_cb),
			    control);
	gtk_signal_connect (GTK_OBJECT (control->priv->zoomable),
			    "zoom_in",
			    GTK_SIGNAL_FUNC (zoomable_zoom_in_cb),
			    control);
	gtk_signal_connect (GTK_OBJECT (control->priv->zoomable),
			    "zoom_out",
			    GTK_SIGNAL_FUNC (zoomable_zoom_out_cb),
			    control);
	gtk_signal_connect (GTK_OBJECT (control->priv->zoomable),
			    "zoom_to_fit",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_fit_cb),
			    control);
	gtk_signal_connect (GTK_OBJECT (control->priv->zoomable),
			    "zoom_to_default",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_default_cb),
			    control);

	control->priv->zoom_level = 1.0;
	bonobo_zoomable_set_parameters_full (control->priv->zoomable,
					     control->priv->zoom_level,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [max_preferred_zoom_levels],
					     TRUE, TRUE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     max_preferred_zoom_levels + 1);

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (control->priv->zoomable));

	retval = bonobo_control_construct (BONOBO_CONTROL (control),
					   corba_object,
					   control->priv->root);
	if (!retval)
		return NULL;

	property_bag = eog_image_view_get_property_bag (control->priv->image_view);
	bonobo_control_set_properties (BONOBO_CONTROL (control), property_bag);
	bonobo_object_unref (BONOBO_OBJECT (property_bag));

	control->priv->uic = bonobo_control_get_ui_component (
		BONOBO_CONTROL (control));
	
	return control;
}

EogControl *
eog_control_new (EogImage *image)
{
	EogControl *control;
	Bonobo_Control corba_object;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	control = gtk_type_new (eog_control_get_type ());

	corba_object = eog_control_corba_object_create (BONOBO_OBJECT (control));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}
	
	return eog_control_construct (control, corba_object, image);
}
