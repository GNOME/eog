/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-view.c
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

#include <eog-image-view.h>
#include <image-view.h>
#include <ui-image.h>

struct _EogImageViewPrivate {
	EogImage           *image;

	GtkWidget          *ui_image;
        GtkWidget          *image_view;

	BonoboPropertyBag  *property_bag;

	BonoboZoomable     *zoomable;
	float               zoom_level;
	gboolean            has_zoomable_frame;

	BonoboUIComponent  *uic;
};

enum {
	LAST_SIGNAL
};

enum {
	PROP_INTERPOLATION,
	PROP_DITHER,
	PROP_CHECK_TYPE,
	PROP_CHECK_SIZE
};

static guint eog_image_view_signals [LAST_SIGNAL];

POA_GNOME_EOG_ImageView__vepv eog_image_view_vepv;

static BonoboObjectClass *eog_image_view_parent_class;

static GNOME_EOG_Image
impl_GNOME_EOG_ImageView_getImage (PortableServer_Servant servant,
				   CORBA_Environment *ev)
{
	EogImageView *image_view = EOG_IMAGE_VIEW (bonobo_object_from_servant (servant));
	GNOME_EOG_Image image;

	image = bonobo_object_corba_objref (BONOBO_OBJECT (image_view->priv->image));
	CORBA_Object_duplicate (image, ev);
	return image;
}

/**
 * eog_image_view_get_epv:
 */
POA_GNOME_EOG_ImageView__epv *
eog_image_view_get_epv (void)
{
	POA_GNOME_EOG_ImageView__epv *epv;

	epv = g_new0 (POA_GNOME_EOG_ImageView__epv, 1);

	epv->getImage = impl_GNOME_EOG_ImageView_getImage;

	return epv;
}

static void
init_eog_image_view_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_image_view_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_image_view_vepv.GNOME_EOG_ImageView_epv = eog_image_view_get_epv ();
}

static void
eog_image_view_destroy (GtkObject *object)
{
	EogImageView *image_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (object));

	image_view = EOG_IMAGE_VIEW (object);

	if (image_view->priv->zoomable) {
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->zoomable));
		image_view->priv->zoomable = NULL;
	}

	if (image_view->priv->property_bag) {
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->property_bag));
		image_view->priv->property_bag = NULL;
	}

	if (image_view->priv->uic) {
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->uic));
		image_view->priv->uic = NULL;
	}

	if (image_view->priv->image) {
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->image));
		image_view->priv->image = NULL;
	}

	if (image_view->priv->ui_image) {
		gtk_widget_destroy (image_view->priv->ui_image);
		image_view->priv->ui_image = NULL;
	}

	if (image_view->priv->image_view) {
		gtk_widget_unref (image_view->priv->image_view);
		image_view->priv->image_view = NULL;
	}

	GTK_OBJECT_CLASS (eog_image_view_parent_class)->destroy (object);
}

static void
eog_image_view_finalize (GtkObject *object)
{
	EogImageView *image_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (object));

	image_view = EOG_IMAGE_VIEW (object);

	g_free (image_view->priv);

	GTK_OBJECT_CLASS (eog_image_view_parent_class)->finalize (object);
}

static void
eog_image_view_class_init (EogImageViewClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_image_view_parent_class = gtk_type_class (bonobo_object_get_type ());

	gtk_object_class_add_signals (object_class, eog_image_view_signals, LAST_SIGNAL);

	object_class->destroy = eog_image_view_destroy;
	object_class->finalize = eog_image_view_finalize;

	init_eog_image_view_corba_class ();
}

static void
eog_image_view_init (EogImageView *image_view)
{
	image_view->priv = g_new0 (EogImageViewPrivate, 1);
}

GtkType
eog_image_view_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogImageView",
			sizeof (EogImageView),
			sizeof (EogImageViewClass),
			(GtkClassInitFunc) eog_image_view_class_init,
			(GtkObjectInitFunc) eog_image_view_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}

GNOME_EOG_ImageView
eog_image_view_corba_object_create (BonoboObject *object)
{
	POA_GNOME_EOG_ImageView *servant;
	CORBA_Environment ev;
	
	servant = (POA_GNOME_EOG_ImageView *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_image_view_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_EOG_ImageView__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_EOG_ImageView) bonobo_object_activate_servant (object, servant);
}

static void
image_data_set_image_cb (EogImageData *image_data, EogImageView *image_view)
{
	Image *image;

	g_return_if_fail (image_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_DATA (image_data));
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	image = eog_image_data_get_image (image_data);
	image_view_set_image (IMAGE_VIEW (image_view->priv->image_view), image);
	image_unref (image);
}

static void
zoomable_set_frame_cb (BonoboZoomable *zoomable, EogImageView *image_view)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	image_view->priv->has_zoomable_frame = TRUE;
}

static void
zoomable_set_zoom_level_cb (BonoboZoomable *zoomable, float new_zoom_level,
			    EogImageView *image_view)
{
	ImageView *view;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	view = IMAGE_VIEW (image_view->priv->image_view);

	image_view_set_zoom (view, (double) new_zoom_level);
	image_view->priv->zoom_level = image_view_get_zoom (view);

	bonobo_zoomable_report_zoom_level_changed (zoomable,
						   image_view->priv->zoom_level);
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
zoomable_zoom_in_cb (BonoboZoomable *zoomable, EogImageView *image_view)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	index = zoom_index_from_float (image_view->priv->zoom_level);
	if (index == max_preferred_zoom_levels)
		return;

	index++;
	new_zoom_level = zoom_level_from_index (index);

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", new_zoom_level);
}

static void
zoomable_zoom_out_cb (BonoboZoomable *zoomable, EogImageView *image_view)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	index = zoom_index_from_float (image_view->priv->zoom_level);
	if (index == 0)
		return;

	index--;
	new_zoom_level = zoom_level_from_index (index);

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", new_zoom_level);
}

static void
zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable, EogImageView *image_view)
{
	ImageView *view;
	float new_zoom_level;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	view = IMAGE_VIEW (image_view->priv->image_view);

	ui_image_zoom_fit (UI_IMAGE (image_view->priv->ui_image));
	new_zoom_level = image_view_get_zoom (view);

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
				 new_zoom_level);
}

static void
zoomable_zoom_to_default_cb (BonoboZoomable *zoomable, EogImageView *image_view)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", 1.0);
}

static void
listener_Interpolation_cb (BonoboUIComponent *uic, const char *path,
			   Bonobo_UIComponent_EventType type, const char *state,
			   gpointer user_data)
{
	EogImageView *image_view;
	GNOME_EOG_Interpolation interpolation;
	BonoboArg *arg;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "InterpolationNearest"))
		interpolation = GNOME_EOG_INTERPOLATION_NEAREST;
	else if (!strcmp (path, "InterpolationTiles"))
		interpolation = GNOME_EOG_INTERPOLATION_TILES;
	else if (!strcmp (path, "InterpolationBilinear"))
		interpolation = GNOME_EOG_INTERPOLATION_BILINEAR;
	else if (!strcmp (path, "InterpolationHyperbolic"))
		interpolation = GNOME_EOG_INTERPOLATION_HYPERBOLIC;
	else {
		g_warning ("Unknown interpolation type `%s'", path);
		return;
	}

	arg = bonobo_arg_new (TC_GNOME_EOG_Interpolation);
	BONOBO_ARG_SET_GENERAL (arg, interpolation, TC_GNOME_EOG_Interpolation, GNOME_EOG_Interpolation, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag, "interpolation", arg, NULL);

	bonobo_arg_release (arg);
}

static void
listener_Dither_cb (BonoboUIComponent *uic, const char *path,
		    Bonobo_UIComponent_EventType type, const char *state,
		    gpointer user_data)
{
	EogImageView *image_view;
	GNOME_EOG_Dither dither;
	BonoboArg *arg;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "DitherNone"))
		dither = GNOME_EOG_DITHER_NONE;
	else if (!strcmp (path, "DitherNormal"))
		dither = GNOME_EOG_DITHER_NORMAL;
	else if (!strcmp (path, "DitherMaximum"))
		dither = GNOME_EOG_DITHER_MAXIMUM;
	else {
		g_warning ("Unknown dither type `%s'", path);
		return;
	}

	arg = bonobo_arg_new (TC_GNOME_EOG_Dither);
	BONOBO_ARG_SET_GENERAL (arg, dither, TC_GNOME_EOG_Dither, GNOME_EOG_Dither, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag, "dither", arg, NULL);

	bonobo_arg_release (arg);
}

static void
listener_CheckType_cb (BonoboUIComponent *uic, const char *path,
		       Bonobo_UIComponent_EventType type, const char *state,
		       gpointer user_data)
{
	EogImageView *image_view;
	GNOME_EOG_CheckType check_type;
	BonoboArg *arg;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "CheckTypeDark"))
		check_type = GNOME_EOG_CHECK_TYPE_DARK;
	else if (!strcmp (path, "CheckTypeMidtone"))
		check_type = GNOME_EOG_CHECK_TYPE_MIDTONE;
	else if (!strcmp (path, "CheckTypeLight"))
		check_type = GNOME_EOG_CHECK_TYPE_LIGHT;
	else if (!strcmp (path, "CheckTypeBlack"))
		check_type = GNOME_EOG_CHECK_TYPE_BLACK;
	else if (!strcmp (path, "CheckTypeGray"))
		check_type = GNOME_EOG_CHECK_TYPE_GRAY;
	else if (!strcmp (path, "CheckTypeWhite"))
		check_type = GNOME_EOG_CHECK_TYPE_WHITE;
	else {
		g_warning ("Unknown check type `%s'", path);
		return;
	}

	arg = bonobo_arg_new (TC_GNOME_EOG_CheckType);
	BONOBO_ARG_SET_GENERAL (arg, check_type, TC_GNOME_EOG_CheckType, GNOME_EOG_CheckType, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag, "check_type", arg, NULL);

	bonobo_arg_release (arg);
}

static void
listener_CheckSize_cb (BonoboUIComponent *uic, const char *path,
		       Bonobo_UIComponent_EventType type, const char *state,
		       gpointer user_data)
{
	EogImageView *image_view;
	GNOME_EOG_CheckSize check_size;
	BonoboArg *arg;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "CheckSizeSmall"))
		check_size = GNOME_EOG_CHECK_SIZE_SMALL;
	else if (!strcmp (path, "CheckSizeMedium"))
		check_size = GNOME_EOG_CHECK_SIZE_MEDIUM;
	else if (!strcmp (path, "CheckSizeLarge"))
		check_size = GNOME_EOG_CHECK_SIZE_LARGE;
	else {
		g_warning ("Unknown check size `%s'", path);
		return;
	}

	arg = bonobo_arg_new (TC_GNOME_EOG_CheckSize);
	BONOBO_ARG_SET_GENERAL (arg, check_size, TC_GNOME_EOG_CheckSize, GNOME_EOG_CheckSize, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag, "check_size", arg, NULL);

	bonobo_arg_release (arg);
}

static void
verb_ZoomIn_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogImageView *image_view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (image_view->priv->zoomable),
				 "zoom_in");
}

static void
verb_ZoomOut_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogImageView *image_view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (image_view->priv->zoomable),
				 "zoom_out");
}

static void
verb_ZoomToDefault_cb (BonoboUIComponent *uic, gpointer user_data,
		       const char *cname)
{
	EogImageView *image_view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (image_view->priv->zoomable),
				 "zoom_to_default");
}

static void
verb_ZoomToFit_cb (BonoboUIComponent *uic, gpointer user_data,
		   const char *cname)
{
	EogImageView *image_view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (image_view->priv->zoomable),
				 "zoom_to_fit");
}

static BonoboUIVerb eog_image_view_verbs[] = {
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
eog_image_view_create_ui (EogImageView *image_view)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	/* Set up the UI from an XML file. */
        bonobo_ui_util_set_ui (image_view->priv->uic, DATADIR,
			       "eog-image-view-ui.xml", "EogImageView");

	bonobo_ui_component_add_listener (image_view->priv->uic, "InterpolationNearest",
					  listener_Interpolation_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "InterpolationTiles",
					  listener_Interpolation_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "InterpolationBilinear",
					  listener_Interpolation_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "InterpolationHyperbolic",
					  listener_Interpolation_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "DitherNone",
					  listener_Dither_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "DitherNormal",
					  listener_Dither_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "DitherMaximum",
					  listener_Dither_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckTypeDark",
					  listener_CheckType_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckTypeMidtone",
					  listener_CheckType_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckTypeLight",
					  listener_CheckType_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckTypeBlack",
					  listener_CheckType_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckTypeGray",
					  listener_CheckType_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckTypeWhite",
					  listener_CheckType_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckSizeSmall",
					  listener_CheckSize_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckSizeMedium",
					  listener_CheckSize_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckSizeLarge",
					  listener_CheckSize_cb, image_view);

	if (image_view->priv->has_zoomable_frame) {
		bonobo_ui_component_set_translate (image_view->priv->uic,
						   "/menu/EOG", zoom_menu,
						   NULL);

		bonobo_ui_component_set_translate (image_view->priv->uic,
						   "/", zoom_toolbar, NULL);

		bonobo_ui_component_add_verb_list_with_data
			(image_view->priv->uic, eog_image_view_verbs,
			 image_view);
	}
}

static void
eog_image_view_get_prop (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id, gpointer user_data)
{
	EogImageView *image_view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	switch (arg_id) {
	case PROP_INTERPOLATION:
		g_assert (arg->_type == TC_GNOME_EOG_Interpolation);
		* (GNOME_EOG_Interpolation *) arg->_value = eog_image_view_get_interpolation (image_view);
		break;
	case PROP_DITHER:
		g_assert (arg->_type == TC_GNOME_EOG_Dither);
		* (GNOME_EOG_Dither *) arg->_value = eog_image_view_get_dither (image_view);
		break;
	case PROP_CHECK_TYPE:
		g_assert (arg->_type == TC_GNOME_EOG_CheckType);
		* (GNOME_EOG_CheckType *) arg->_value = eog_image_view_get_check_type (image_view);
		break;
	case PROP_CHECK_SIZE:
		g_assert (arg->_type == TC_GNOME_EOG_CheckSize);
		* (GNOME_EOG_CheckSize *) arg->_value = eog_image_view_get_check_size (image_view);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
eog_image_view_set_prop (BonoboPropertyBag *bag, const BonoboArg *arg, guint arg_id, gpointer user_data)
{
	EogImageView *image_view;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	switch (arg_id) {
	case PROP_INTERPOLATION:
		g_assert (arg->_type == TC_GNOME_EOG_Interpolation);
		eog_image_view_set_interpolation (image_view, * (GNOME_EOG_Interpolation *) arg->_value);
		break;
	case PROP_DITHER:
		g_assert (arg->_type == TC_GNOME_EOG_Dither);
		eog_image_view_set_dither (image_view, * (GNOME_EOG_Dither *) arg->_value);
		break;
	case PROP_CHECK_TYPE:
		g_assert (arg->_type == TC_GNOME_EOG_CheckType);
		eog_image_view_set_check_type (image_view, * (GNOME_EOG_CheckType *) arg->_value);
		break;
	case PROP_CHECK_SIZE:
		g_assert (arg->_type == TC_GNOME_EOG_CheckSize);
		eog_image_view_set_check_size (image_view, * (GNOME_EOG_CheckSize *) arg->_value);
		break;
	default:
		g_assert_not_reached ();
	}
}

EogImageView *
eog_image_view_construct (EogImageView *image_view,
			  GNOME_EOG_ImageView corba_object,
			  EogImage *image)
{
	BonoboObject *retval;

	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	
	retval = bonobo_object_construct (BONOBO_OBJECT (image_view), corba_object);
	if (retval == NULL)
		return NULL;

	image_view->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->image));

	gtk_signal_connect (GTK_OBJECT (image), "set_image",
			    GTK_SIGNAL_FUNC (image_data_set_image_cb),
			    image_view);

	image_view->priv->ui_image = ui_image_new ();
	image_view->priv->image_view = ui_image_get_image_view (UI_IMAGE (image_view->priv->ui_image));
	gtk_widget_ref (image_view->priv->image_view);

	gtk_widget_show (image_view->priv->ui_image);

	/*
	 * Property Bag
	 */
	image_view->priv->property_bag = bonobo_property_bag_new (eog_image_view_get_prop,
								  eog_image_view_set_prop,
								  image_view);

	bonobo_property_bag_add (image_view->priv->property_bag, "interpolation", PROP_INTERPOLATION,
				 TC_GNOME_EOG_Interpolation, NULL, _("Interpolation"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "dither", PROP_DITHER,
				 TC_GNOME_EOG_Dither, NULL, _("Dither"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "check_type", PROP_CHECK_TYPE,
				 TC_GNOME_EOG_CheckType, NULL, _("Check Type"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "check_size", PROP_CHECK_SIZE,
				 TC_GNOME_EOG_CheckSize, NULL, _("Check Size"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);

	/*
	 * Interface Bonobo::Zoomable 
	 */
	image_view->priv->zoomable = bonobo_zoomable_new ();

	gtk_signal_connect (GTK_OBJECT (image_view->priv->zoomable),
			    "set_frame",
			    GTK_SIGNAL_FUNC (zoomable_set_frame_cb),
			    image_view);
	gtk_signal_connect (GTK_OBJECT (image_view->priv->zoomable),
			    "set_zoom_level",
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_cb),
			    image_view);
	gtk_signal_connect (GTK_OBJECT (image_view->priv->zoomable),
			    "zoom_in",
			    GTK_SIGNAL_FUNC (zoomable_zoom_in_cb),
			    image_view);
	gtk_signal_connect (GTK_OBJECT (image_view->priv->zoomable),
			    "zoom_out",
			    GTK_SIGNAL_FUNC (zoomable_zoom_out_cb),
			    image_view);
	gtk_signal_connect (GTK_OBJECT (image_view->priv->zoomable),
			    "zoom_to_fit",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_fit_cb),
			    image_view);
	gtk_signal_connect (GTK_OBJECT (image_view->priv->zoomable),
			    "zoom_to_default",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_default_cb),
			    image_view);

	image_view->priv->zoom_level = 1.0;
	bonobo_zoomable_set_parameters_full (image_view->priv->zoomable,
					     image_view->priv->zoom_level,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [max_preferred_zoom_levels],
					     TRUE, TRUE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     max_preferred_zoom_levels + 1);

	bonobo_object_add_interface (BONOBO_OBJECT (image_view),
				     BONOBO_OBJECT (image_view->priv->zoomable));


	image_view->priv->uic = bonobo_ui_component_new ("EogImageView");

	return image_view;
}

EogImageView *
eog_image_view_new (EogImage *image)
{
	EogImageView *image_view;
	GNOME_EOG_ImageView corba_object;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	image_view = gtk_type_new (eog_image_view_get_type ());

	corba_object = eog_image_view_corba_object_create (BONOBO_OBJECT (image_view));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (image_view));
		return NULL;
	}
	
	return eog_image_view_construct (image_view, corba_object, image);
}

EogImage *
eog_image_view_get_image (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->image));
	return image_view->priv->image;
}

BonoboPropertyBag *
eog_image_view_get_property_bag (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->property_bag));
	return image_view->priv->property_bag;
}

BonoboZoomable *
eog_image_view_get_zoomable (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->zoomable));
	return image_view->priv->zoomable;
}

void
eog_image_view_set_ui_container (EogImageView *image_view, Bonobo_UIContainer ui_container)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	bonobo_ui_component_set_container (image_view->priv->uic, ui_container);

	eog_image_view_create_ui (image_view);
}

void
eog_image_view_unset_ui_container (EogImageView *image_view)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	bonobo_ui_component_unset_container (image_view->priv->uic);
}

GtkWidget *
eog_image_view_get_widget (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	gtk_widget_ref (image_view->priv->ui_image);
	return image_view->priv->ui_image;
}

void
eog_image_view_set_interpolation (EogImageView *image_view,
				  GNOME_EOG_Interpolation interpolation)
{
	GdkInterpType interp_type;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	switch (interpolation) {
	case GNOME_EOG_INTERPOLATION_NEAREST:
		interp_type = GDK_INTERP_NEAREST;
		break;
	case GNOME_EOG_INTERPOLATION_TILES:
		interp_type = GDK_INTERP_TILES;
		break;
	case GNOME_EOG_INTERPOLATION_BILINEAR:
		interp_type = GDK_INTERP_BILINEAR;
		break;
	case GNOME_EOG_INTERPOLATION_HYPERBOLIC:
		interp_type = GDK_INTERP_HYPER;
		break;
	default:
		g_assert_not_reached ();
	}

	image_view_set_interp_type (IMAGE_VIEW (image_view->priv->image_view), interp_type);
}

GNOME_EOG_Interpolation
eog_image_view_get_interpolation (EogImageView *image_view)
{
	GdkInterpType interp_type;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	interp_type = image_view_get_interp_type (IMAGE_VIEW (image_view->priv->image_view));
	switch (interp_type) {
	case GDK_INTERP_NEAREST:
		return GNOME_EOG_INTERPOLATION_NEAREST;
	case GDK_INTERP_TILES:
		return GNOME_EOG_INTERPOLATION_TILES;
	case GDK_INTERP_BILINEAR:
		return GNOME_EOG_INTERPOLATION_BILINEAR;
	case GDK_INTERP_HYPER:
		return GNOME_EOG_INTERPOLATION_HYPERBOLIC;
	default:
		g_assert_not_reached ();
	}

	return 0;
}

void
eog_image_view_set_dither (EogImageView *image_view, GNOME_EOG_Dither eog_dither)
{
	GdkRgbDither dither;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	switch (eog_dither) {
	case GNOME_EOG_DITHER_NONE:
		dither = GDK_RGB_DITHER_NONE;
		break;
	case GNOME_EOG_DITHER_NORMAL:
		dither = GDK_RGB_DITHER_NORMAL;
		break;
	case GNOME_EOG_DITHER_MAXIMUM:
		dither = GDK_RGB_DITHER_MAX;
		break;
	default:
		g_assert_not_reached ();
	}

	image_view_set_dither (IMAGE_VIEW (image_view->priv->image_view), dither);
}

GNOME_EOG_Dither
eog_image_view_get_dither (EogImageView *image_view)
{
	GdkRgbDither dither;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	dither = image_view_get_dither (IMAGE_VIEW (image_view->priv->image_view));
	switch (dither) {
	case GDK_RGB_DITHER_NONE:
		return GNOME_EOG_DITHER_NONE;
	case GDK_RGB_DITHER_NORMAL:
		return GNOME_EOG_DITHER_NORMAL;
	case GDK_RGB_DITHER_MAX:
		return GNOME_EOG_DITHER_MAXIMUM;
	default:
		g_assert_not_reached ();
	}

	return 0;
}

void
eog_image_view_set_check_type (EogImageView *image_view, GNOME_EOG_CheckType eog_check_type)
{
	CheckType check_type;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	switch (eog_check_type) {
	case GNOME_EOG_CHECK_TYPE_DARK:
		check_type = CHECK_TYPE_DARK;
		break;
	case GNOME_EOG_CHECK_TYPE_MIDTONE:
		check_type = CHECK_TYPE_MIDTONE;
		break;
	case GNOME_EOG_CHECK_TYPE_LIGHT:
		check_type = CHECK_TYPE_LIGHT;
		break;
	case GNOME_EOG_CHECK_TYPE_BLACK:
		check_type = CHECK_TYPE_BLACK;
		break;
	case GNOME_EOG_CHECK_TYPE_GRAY:
		check_type = CHECK_TYPE_GRAY;
		break;
	case GNOME_EOG_CHECK_TYPE_WHITE:
		check_type = CHECK_TYPE_WHITE;
		break;
	default:
		g_assert_not_reached ();
	}

	image_view_set_check_type (IMAGE_VIEW (image_view->priv->image_view), check_type);
}

GNOME_EOG_CheckType
eog_image_view_get_check_type (EogImageView *image_view)
{
	CheckType check_type;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	check_type = image_view_get_check_type (IMAGE_VIEW (image_view->priv->image_view));
	switch (check_type) {
	case CHECK_TYPE_DARK:
		return GNOME_EOG_CHECK_TYPE_DARK;
	case CHECK_TYPE_MIDTONE:
		return GNOME_EOG_CHECK_TYPE_MIDTONE;
	case CHECK_TYPE_LIGHT:
		return GNOME_EOG_CHECK_TYPE_LIGHT;
	case CHECK_TYPE_BLACK:
		return GNOME_EOG_CHECK_TYPE_BLACK;
	case CHECK_TYPE_GRAY:
		return GNOME_EOG_CHECK_TYPE_GRAY;
	case CHECK_TYPE_WHITE:
		return GNOME_EOG_CHECK_TYPE_WHITE;
	default:
		g_assert_not_reached ();
	}

	return 0;
}

void
eog_image_view_set_check_size (EogImageView *image_view, GNOME_EOG_CheckSize eog_check_size)
{
	CheckSize check_size;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	switch (eog_check_size) {
	case GNOME_EOG_CHECK_SIZE_SMALL:
		check_size = CHECK_SIZE_SMALL;
		break;
	case GNOME_EOG_CHECK_SIZE_MEDIUM:
		check_size = CHECK_SIZE_MEDIUM;
		break;
	case GNOME_EOG_CHECK_SIZE_LARGE:
		check_size = CHECK_SIZE_LARGE;
		break;
	default:
		g_assert_not_reached ();
	}

	image_view_set_check_size (IMAGE_VIEW (image_view->priv->image_view), check_size);
}

GNOME_EOG_CheckSize
eog_image_view_get_check_size (EogImageView *image_view)
{
	CheckSize check_size;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	check_size = image_view_get_check_size (IMAGE_VIEW (image_view->priv->image_view));
	switch (check_size) {
	case CHECK_SIZE_SMALL:
		return GNOME_EOG_CHECK_SIZE_SMALL;
	case CHECK_SIZE_MEDIUM:
		return GNOME_EOG_CHECK_SIZE_MEDIUM;
	case CHECK_SIZE_LARGE:
		return GNOME_EOG_CHECK_SIZE_LARGE;
	default:
		g_assert_not_reached ();
	}

	return 0;
}
