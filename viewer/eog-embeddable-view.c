/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-embeddable-view.c
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

#include "gtkscrollframe.h"
#include "image-view.h"
#include "eog-embeddable-view.h"

struct _EogEmbeddableViewPrivate {
	EogImage           *image;
	EogImageView       *image_view;

	GtkWidget          *root;
};

POA_Bonobo_View__vepv eog_embeddable_view_vepv;

static BonoboViewClass *eog_embeddable_view_parent_class;

static void
init_eog_embeddable_view_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_embeddable_view_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_embeddable_view_vepv.Bonobo_Control_epv = bonobo_control_get_epv ();
	eog_embeddable_view_vepv.Bonobo_View_epv = bonobo_view_get_epv ();
}

static void
eog_embeddable_view_activate (BonoboControl *control, gboolean state)
{
	EogEmbeddableView *embeddable_view;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE_VIEW (control));

	embeddable_view = EOG_EMBEDDABLE_VIEW (control);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (control);
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_image_view_set_ui_container (
				embeddable_view->priv->image_view, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else {
		eog_image_view_unset_ui_container (embeddable_view->priv->image_view);
	}

	if (BONOBO_CONTROL_CLASS (eog_embeddable_view_parent_class)->activate)
		BONOBO_CONTROL_CLASS (eog_embeddable_view_parent_class)->activate (control, state);
}

static void
eog_embeddable_view_destroy (GtkObject *object)
{
	EogEmbeddableView *embeddable_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE_VIEW (object));

	embeddable_view = EOG_EMBEDDABLE_VIEW (object);

	if (embeddable_view->priv->image) {
		bonobo_object_unref (BONOBO_OBJECT (embeddable_view->priv->image));
		embeddable_view->priv->image = NULL;
	}

	if (embeddable_view->priv->root) {
		gtk_widget_unref (embeddable_view->priv->root);
		embeddable_view->priv->root = NULL;
	}

	GTK_OBJECT_CLASS (eog_embeddable_view_parent_class)->destroy (object);
}

static void
eog_embeddable_view_finalize (GtkObject *object)
{
	EogEmbeddableView *embeddable_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE_VIEW (object));

	embeddable_view = EOG_EMBEDDABLE_VIEW (object);

	g_free (embeddable_view->priv);

	GTK_OBJECT_CLASS (eog_embeddable_view_parent_class)->finalize (object);
}

static void
eog_embeddable_view_class_init (EogEmbeddableView *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	BonoboControlClass *control_class = (BonoboControlClass *)klass;

	eog_embeddable_view_parent_class = gtk_type_class (bonobo_view_get_type ());

	object_class->destroy = eog_embeddable_view_destroy;
	object_class->finalize = eog_embeddable_view_finalize;

	control_class->activate = eog_embeddable_view_activate;

	init_eog_embeddable_view_corba_class ();
}

static void
eog_embeddable_view_init (EogEmbeddableView *embeddable_view)
{
	embeddable_view->priv = g_new0 (EogEmbeddableViewPrivate, 1);
}

GtkType
eog_embeddable_view_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogEmbeddableView",
			sizeof (EogEmbeddableView),
			sizeof (EogEmbeddableViewClass),
			(GtkClassInitFunc) eog_embeddable_view_class_init,
			(GtkObjectInitFunc) eog_embeddable_view_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_view_get_type (), &info);
	}

	return type;
}

Bonobo_View
eog_embeddable_view_corba_object_create (BonoboObject *object)
{
	POA_Bonobo_View *servant;
	CORBA_Environment ev;
	
	servant = (POA_Bonobo_View *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_embeddable_view_vepv;

	CORBA_exception_init (&ev);
	POA_Bonobo_View__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Bonobo_View) bonobo_object_activate_servant (object, servant);
}

#define EOG_DEBUG

static void
configure_size (EogEmbeddableView *view,
		GtkAllocation     *allocation)
{
	GdkPixbuf *pixbuf;
	double     zoomx, zoomy;

	if (!view->priv->image)
		return;

	if (!allocation)
		allocation = &view->priv->root->allocation;

/*	g_warning ("Configure size allocation '%d' '%d'",
	allocation->width, allocation->height);*/

	pixbuf = eog_image_get_pixbuf (
		view->priv->image);

	if (!pixbuf)
		return;

	zoomx = (1.0 * allocation->width) /
		gdk_pixbuf_get_width (pixbuf);
	zoomy = (1.0 * allocation->height) /
		gdk_pixbuf_get_height (pixbuf);

	gdk_pixbuf_unref (pixbuf);

/*	g_warning ("Set to zoom %f %f, pixbuf %d %d",
		   zoomx, zoomy,
		   gdk_pixbuf_get_width (pixbuf),
		   gdk_pixbuf_get_height (pixbuf));*/

	eog_image_view_set_zoom (view->priv->image_view,
				 zoomx, zoomy);
}

static void
view_size_allocate_cb (GtkWidget         *drawing_area,
		       GtkAllocation     *allocation,
		       EogEmbeddableView *view)
{
	configure_size (view, allocation);

}

static void
set_image_cb (EogImage          *image,
	      EogEmbeddableView *view)
{
	configure_size (view, NULL);
}

EogEmbeddableView *
eog_embeddable_view_construct (EogEmbeddableView *embeddable_view,
			       Bonobo_View        corba_object,
			       EogImage          *image)
{
	BonoboView        *retval;
	BonoboPropertyBag *property_bag;

	g_return_val_if_fail (embeddable_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_EMBEDDABLE_VIEW (embeddable_view), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	embeddable_view->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image));

	embeddable_view->priv->image_view = eog_image_view_new (image, TRUE);
	embeddable_view->priv->root = eog_image_view_get_widget (embeddable_view->priv->image_view);
	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (embeddable_view->priv->root),
				     GTK_POLICY_NEVER,
				     GTK_POLICY_NEVER);

	gtk_signal_connect (GTK_OBJECT (image), "set_image",
			    GTK_SIGNAL_FUNC (set_image_cb), embeddable_view);

	gtk_signal_connect (GTK_OBJECT (embeddable_view->priv->root), "size_allocate",
			    GTK_SIGNAL_FUNC (view_size_allocate_cb), embeddable_view);

	bonobo_object_add_interface (BONOBO_OBJECT (embeddable_view),
				     BONOBO_OBJECT (embeddable_view->priv->image_view));

	retval = bonobo_view_construct (
		BONOBO_VIEW (embeddable_view), corba_object,
		embeddable_view->priv->root);

	if (!retval)
		return NULL;

	property_bag = eog_image_view_get_property_bag (embeddable_view->priv->image_view);
	bonobo_control_set_properties (BONOBO_CONTROL (embeddable_view), property_bag);
	bonobo_object_unref (BONOBO_OBJECT (property_bag));

	return embeddable_view;
}

EogEmbeddableView *
eog_embeddable_view_new (EogImage *image)
{
	EogEmbeddableView *embeddable_view;
	Bonobo_View corba_object;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	embeddable_view = gtk_type_new (eog_embeddable_view_get_type ());

	corba_object = eog_embeddable_view_corba_object_create (BONOBO_OBJECT (embeddable_view));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (embeddable_view));
		return NULL;
	}
	
	return eog_embeddable_view_construct (embeddable_view, corba_object, image);
}
