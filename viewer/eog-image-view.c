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
	EogImage             *image;

	GtkWidget            *ui_image;
        GtkWidget            *image_view;

	BonoboPropertyBag    *property_bag;

	BonoboUIComponent    *uic;

	BonoboItemContainer  *item_container;

	gboolean              zoom_fit;
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


static void
image_set_image_cb (EogImage *eog_image, EogImageView *image_view)
{
	Image *image;

	g_return_if_fail (eog_image != NULL);
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE (eog_image));
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	image = eog_image_get_image (eog_image);
	if (image) {
	/* FIXME: Eog's internals can't cope with different zooms on different
	   axis, this needs fixing in src/image-view.c */
/*	if (image_view->priv->zoom_fit); */
		image_view_set_image (IMAGE_VIEW (image_view->priv->image_view),
				      image);
		image_unref (image);
	}
}

static void
listener_Interpolation_cb (BonoboUIComponent           *uic,
			   const char                  *path,
			   Bonobo_UIComponent_EventType type,
			   const char                  *state,
			   gpointer                     user_data)
{
	BonoboArg *arg;
	EogImageView *image_view;
	GNOME_EOG_Interpolation interpolation;

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
	BONOBO_ARG_SET_GENERAL (arg, interpolation, TC_GNOME_EOG_Interpolation,
				GNOME_EOG_Interpolation, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag,
				       "interpolation", arg, NULL);

	bonobo_arg_release (arg);
}

static void
listener_Dither_cb (BonoboUIComponent           *uic,
		    const char                  *path,
		    Bonobo_UIComponent_EventType type,
		    const char                  *state,
		    gpointer                     user_data)
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
	BONOBO_ARG_SET_GENERAL (arg, dither, TC_GNOME_EOG_Dither,
				GNOME_EOG_Dither, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag,
				       "dither", arg, NULL);

	bonobo_arg_release (arg);
}

static void
listener_CheckType_cb (BonoboUIComponent           *uic,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
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
	BONOBO_ARG_SET_GENERAL (arg, check_type, TC_GNOME_EOG_CheckType,
				GNOME_EOG_CheckType, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag,
				       "check_type", arg, NULL);

	bonobo_arg_release (arg);
}

static void
listener_CheckSize_cb (BonoboUIComponent           *uic,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
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
	BONOBO_ARG_SET_GENERAL (arg, check_size, TC_GNOME_EOG_CheckSize,
				GNOME_EOG_CheckSize, NULL);

	bonobo_property_bag_set_value (image_view->priv->property_bag,
				       "check_size", arg, NULL);

	bonobo_arg_release (arg);
}

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
}

static void
eog_image_view_get_prop (BonoboPropertyBag *bag,
			 BonoboArg         *arg,
			 guint              arg_id,
			 gpointer           user_data)
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
eog_image_view_set_prop (BonoboPropertyBag *bag,
			 const BonoboArg   *arg,
			 guint              arg_id,
			 gpointer           user_data)
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

static Bonobo_Unknown
eog_image_view_get_object (BonoboItemContainer *item_container,
			   CORBA_char          *item_name,
			   CORBA_boolean        only_if_exists,
			   CORBA_Environment   *ev,
			   EogImageView        *image_view)
{
	g_return_val_if_fail (image_view != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), CORBA_OBJECT_NIL);

	g_message ("eog_image_view_get_object: %d - %s",
		   only_if_exists, item_name);

	return CORBA_OBJECT_NIL;
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

void
eog_image_view_set_ui_container (EogImageView      *image_view,
				 Bonobo_UIContainer ui_container)
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

float
eog_image_view_get_zoom_factor (EogImageView *image_view)
{
	ImageView *view;

	g_return_val_if_fail (image_view != NULL, 0.0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0.0);

	view = IMAGE_VIEW (image_view->priv->image_view);

	return image_view_get_zoom (view);
}

void
eog_image_view_set_zoom_factor (EogImageView *image_view,
				float         zoom_factor)
{
	ImageView *view;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));
	g_return_if_fail (zoom_factor > 0.0);

	view = IMAGE_VIEW (image_view->priv->image_view);

	image_view_set_zoom (view, zoom_factor);
}

void
eog_image_view_zoom_to_fit (EogImageView *image_view,
			    gboolean      keep_aspect_ratio)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	ui_image_zoom_fit (UI_IMAGE (image_view->priv->ui_image));
}

void
eog_image_view_set_interpolation (EogImageView           *image_view,
				  GNOME_EOG_Interpolation interpolation)
{
	GdkInterpType interp_type = GDK_INTERP_NEAREST;

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

	image_view_set_interp_type (
		IMAGE_VIEW (image_view->priv->image_view), interp_type);
}

GNOME_EOG_Interpolation
eog_image_view_get_interpolation (EogImageView *image_view)
{
	GdkInterpType interp_type = GNOME_EOG_INTERPOLATION_NEAREST;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	interp_type = image_view_get_interp_type (
		IMAGE_VIEW (image_view->priv->image_view));
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
eog_image_view_set_dither (EogImageView    *image_view,
			   GNOME_EOG_Dither eog_dither)
{
	GdkRgbDither dither = GDK_RGB_DITHER_NONE;

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
	GdkRgbDither dither = GNOME_EOG_DITHER_NONE;

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
eog_image_view_set_check_type (EogImageView       *image_view,
			       GNOME_EOG_CheckType eog_check_type)
{
	CheckType check_type = CHECK_TYPE_GRAY;

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

	image_view_set_check_type (
		IMAGE_VIEW (image_view->priv->image_view), check_type);
}

GNOME_EOG_CheckType
eog_image_view_get_check_type (EogImageView *image_view)
{
	CheckType check_type = GNOME_EOG_CHECK_TYPE_GRAY;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	check_type = image_view_get_check_type (
		IMAGE_VIEW (image_view->priv->image_view));

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
eog_image_view_set_check_size (EogImageView       *image_view,
			       GNOME_EOG_CheckSize eog_check_size)
{
	CheckSize check_size = GNOME_EOG_CHECK_SIZE_MEDIUM;

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

	image_view_set_check_size (
		IMAGE_VIEW (image_view->priv->image_view), check_size);
}

GNOME_EOG_CheckSize
eog_image_view_get_check_size (EogImageView *image_view)
{
	CheckSize check_size = GNOME_EOG_CHECK_SIZE_MEDIUM;

	g_return_val_if_fail (image_view != NULL, 0);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), 0);

	check_size = image_view_get_check_size (
		IMAGE_VIEW (image_view->priv->image_view));

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

	if (image_view->priv->property_bag)
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->property_bag));
	image_view->priv->property_bag = NULL;

	if (image_view->priv->image)
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->image));
	image_view->priv->image = NULL;

	if (image_view->priv->uic)
		bonobo_object_unref (BONOBO_OBJECT (image_view->priv->uic));
	image_view->priv->uic = NULL;

	if (image_view->priv->ui_image)
		gtk_widget_destroy (image_view->priv->ui_image);
	image_view->priv->ui_image = NULL;

	if (image_view->priv->image_view)
		gtk_widget_unref (image_view->priv->image_view);
	image_view->priv->image_view = NULL;

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

	if (!type) {
		GtkTypeInfo info = {
			"EogImageView",
			sizeof (EogImageView),
			sizeof (EogImageViewClass),
			(GtkClassInitFunc)  eog_image_view_class_init,
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

EogImageView *
eog_image_view_construct (EogImageView       *image_view,
			  GNOME_EOG_ImageView corba_object,
			  EogImage           *image,
			  gboolean            zoom_fit)
{
	BonoboObject *retval;

	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	
	retval = bonobo_object_construct (
		BONOBO_OBJECT (image_view), corba_object);
	if (!retval)
		return NULL;

	image_view->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->image));
	image_view->priv->zoom_fit = zoom_fit;

	gtk_signal_connect (GTK_OBJECT (image), "set_image",
			    GTK_SIGNAL_FUNC (image_set_image_cb),
			    image_view);

	image_view->priv->ui_image = ui_image_new ();
	image_view->priv->image_view = ui_image_get_image_view (UI_IMAGE (image_view->priv->ui_image));
	gtk_widget_ref (image_view->priv->image_view);

	image_set_image_cb (image, image_view);
	gtk_widget_show (image_view->priv->ui_image);

	/* Property Bag */
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

	image_view->priv->uic = bonobo_ui_component_new ("EogImageView");

	/* BonoboItemContainer */
	image_view->priv->item_container = bonobo_item_container_new ();
	gtk_signal_connect (GTK_OBJECT (image_view->priv->item_container),
			    "get_object",
			    GTK_SIGNAL_FUNC (eog_image_view_get_object),
			    image_view);
	bonobo_object_add_interface (BONOBO_OBJECT (image_view),
				     BONOBO_OBJECT (image_view->priv->item_container));

	return image_view;
}

EogImageView *
eog_image_view_new (EogImage *image,
		    gboolean  zoom_fit)
{
	EogImageView *image_view;
	GNOME_EOG_ImageView corba_object;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	image_view = gtk_type_new (eog_image_view_get_type ());

	corba_object = eog_image_view_corba_object_create (
		BONOBO_OBJECT (image_view));

	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (image_view));
		return NULL;
	}
	
	return eog_image_view_construct (image_view, corba_object, image, zoom_fit);
}
