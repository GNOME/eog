/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-embeddable.c
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

#include <eog-embeddable.h>
#include <eog-embeddable-view.h>

struct _EogEmbeddablePrivate {
	EogImage           *image;
};

POA_Bonobo_Embeddable__vepv eog_embeddable_vepv;

static BonoboEmbeddableClass *eog_embeddable_parent_class;

static void
init_eog_embeddable_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_embeddable_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_embeddable_vepv.Bonobo_Embeddable_epv = bonobo_embeddable_get_epv ();
}

static void
eog_embeddable_destroy (GtkObject *object)
{
	EogEmbeddable *embeddable;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE (object));

	embeddable = EOG_EMBEDDABLE (object);

	if (embeddable->priv->image) {
		bonobo_object_unref (BONOBO_OBJECT (embeddable->priv->image));
		embeddable->priv->image = NULL;
	}

	GTK_OBJECT_CLASS (eog_embeddable_parent_class)->destroy (object);
}

static void
eog_embeddable_finalize (GtkObject *object)
{
	EogEmbeddable *embeddable;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE (object));

	embeddable = EOG_EMBEDDABLE (object);

	g_free (embeddable->priv);

	GTK_OBJECT_CLASS (eog_embeddable_parent_class)->finalize (object);
}

static void
eog_embeddable_class_init (EogEmbeddable *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_embeddable_parent_class = gtk_type_class (bonobo_embeddable_get_type ());

	object_class->destroy  = eog_embeddable_destroy;
	object_class->finalize = eog_embeddable_finalize;

	init_eog_embeddable_corba_class ();
}

static void
eog_embeddable_init (EogEmbeddable *embeddable)
{
	embeddable->priv = g_new0 (EogEmbeddablePrivate, 1);
}

GtkType
eog_embeddable_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"EogEmbeddable",
			sizeof (EogEmbeddable),
			sizeof (EogEmbeddableClass),
			(GtkClassInitFunc)  eog_embeddable_class_init,
			(GtkObjectInitFunc) eog_embeddable_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_embeddable_get_type (), &info);
	}

	return type;
}

Bonobo_Embeddable
eog_embeddable_corba_object_create (BonoboObject *object)
{
	POA_Bonobo_Embeddable *servant;
	CORBA_Environment ev;
	
	servant = (POA_Bonobo_Embeddable *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_embeddable_vepv;

	CORBA_exception_init (&ev);
	POA_Bonobo_Embeddable__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Bonobo_Embeddable) bonobo_object_activate_servant (object, servant);
}

static BonoboView *
eog_embeddable_view_factory (BonoboEmbeddable      *object,
			     const Bonobo_ViewFrame view_frame,
			     void                  *closure)
{
	EogEmbeddable *embeddable;
	EogEmbeddableView *view;

	g_return_val_if_fail (object != NULL, NULL);
	g_return_val_if_fail (EOG_IS_EMBEDDABLE (object), NULL);

	embeddable = EOG_EMBEDDABLE (object);

	view = eog_embeddable_view_new (embeddable->priv->image);

	return BONOBO_VIEW (view);
}

static void
render_fn (GnomePrintContext         *ctx,
	   double                     width,
	   double                     height,
	   const Bonobo_PrintScissor *opt_scissor,
	   gpointer                   user_data)
{
	EogImage  *image = user_data;
	GdkPixbuf *pixbuf;
	double     matrix[6];

	g_return_if_fail (EOG_IS_IMAGE (image));

	pixbuf = eog_image_get_pixbuf (image);
	if (!pixbuf)
		return;

	art_affine_scale (matrix, width, height);
	matrix[4] = 0;
	matrix[5] = 0;

	gnome_print_gsave  (ctx);
	gnome_print_concat (ctx, matrix);

	if (gdk_pixbuf_get_has_alpha (pixbuf))
		gnome_print_rgbaimage  (ctx,
					gdk_pixbuf_get_pixels    (pixbuf),
					gdk_pixbuf_get_width     (pixbuf),
					gdk_pixbuf_get_height    (pixbuf),
					gdk_pixbuf_get_rowstride (pixbuf));
	else
		gnome_print_rgbimage  (ctx,
				       gdk_pixbuf_get_pixels    (pixbuf),
				       gdk_pixbuf_get_width     (pixbuf),
				       gdk_pixbuf_get_height    (pixbuf),
				       gdk_pixbuf_get_rowstride (pixbuf));
	gnome_print_grestore  (ctx);

	gdk_pixbuf_unref (pixbuf);
}

EogEmbeddable *
eog_embeddable_construct (EogEmbeddable *embeddable,
			  Bonobo_Embeddable corba_object,
			  EogImage *image)
{
	BonoboPrint *print;

	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (embeddable != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (EOG_IS_EMBEDDABLE (embeddable), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);

	embeddable->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image));

	if (!eog_image_add_interfaces (image, BONOBO_OBJECT (embeddable)))
		return NULL;

	print = bonobo_print_new (render_fn, image);
	bonobo_object_add_interface (BONOBO_OBJECT (embeddable),
				     BONOBO_OBJECT (print));

	return EOG_EMBEDDABLE (
		bonobo_embeddable_construct (
			BONOBO_EMBEDDABLE (embeddable),
			corba_object, eog_embeddable_view_factory, NULL));
}

EogEmbeddable *
eog_embeddable_new (EogImage *image)
{
	EogEmbeddable *embeddable;
	Bonobo_Embeddable corba_object;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	embeddable = gtk_type_new (eog_embeddable_get_type ());

	corba_object = eog_embeddable_corba_object_create (BONOBO_OBJECT (embeddable));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (embeddable));
		return NULL;
	}
	
	return eog_embeddable_construct (embeddable, corba_object, image);
}
