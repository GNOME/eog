/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-data.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Michael Meeks (michael@helixcode.com)
 *
 * Copyright 2000 SuSE GmbH.
 * Copyright 2000 Helix Code, Inc.
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include <libgnomevfs/gnome-vfs.h>

#include <eog-image.h>
#include <eog-control.h>
#include <eog-embeddable.h>

#include <eog-image-io.h>

struct _EogImagePrivate {
	gchar     *filename;
        GdkPixbuf *pixbuf;
};

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static BonoboXObjectClass *eog_image_parent_class;

enum {
	SET_IMAGE_SIGNAL,
	LAST_SIGNAL
};

static guint eog_image_signals [LAST_SIGNAL];

static void
eog_image_destroy (GtkObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

	if (image->priv->pixbuf) {
		gdk_pixbuf_unref (image->priv->pixbuf);
		image->priv->pixbuf = NULL;
	}

	if (image->priv->filename)
		g_free (image->priv->filename);

	GTK_OBJECT_CLASS (eog_image_parent_class)->destroy (object);
}

static void
eog_image_finalize (GObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

	g_free (image->priv);

	G_OBJECT_CLASS (eog_image_parent_class)->finalize (object);
}

static void
eog_image_class_init (EogImageClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	GObjectClass *gobject_class = (GObjectClass *)klass;

	POA_GNOME_EOG_Image__epv *epv;

	eog_image_parent_class = gtk_type_class (PARENT_TYPE);

	gtk_object_class_add_signals (object_class, eog_image_signals, LAST_SIGNAL);

	eog_image_signals [SET_IMAGE_SIGNAL] =
                gtk_signal_new ("set_image",
                                GTK_RUN_LAST,
                                GTK_CLASS_TYPE (object_class),
                                GTK_SIGNAL_OFFSET (EogImageClass, set_image),
                                gtk_marshal_NONE__NONE,
                                GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, eog_image_signals,
				      LAST_SIGNAL);

	object_class->destroy = eog_image_destroy;
	gobject_class->finalize = eog_image_finalize;

	epv = &klass->epv;
}

static void
eog_image_init (EogImage *image)
{
	image->priv = g_new0 (EogImagePrivate, 1);
}

BONOBO_X_TYPE_FUNC_FULL (EogImage,
			 GNOME_EOG_Image,
			 PARENT_TYPE,
			 eog_image);

/*
 * Loads an Image from a Bonobo_Stream
 */
static void
load_image_from_stream (BonoboPersistStream       *ps,
			Bonobo_Stream              stream,
			Bonobo_Persist_ContentType type,
			void                      *data,
			CORBA_Environment         *ev)
{
	EogImage             *image;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	Bonobo_Stream_iobuf  *buffer;
	CORBA_long            len;
	Bonobo_StorageInfo   *info;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE (data));

	if (getenv ("DEBUG_EOG"))
		g_message ("Loading stream...");

	image = EOG_IMAGE (data);

	if (image->priv->pixbuf)
		gdk_pixbuf_unref (image->priv->pixbuf);
	image->priv->pixbuf = NULL;
	if (image->priv->filename)
		g_free (image->priv->filename);
	image->priv->filename = NULL;

	do {
		Bonobo_Stream_read (stream, 4096, &buffer, ev);
		if (ev->_major != CORBA_NO_EXCEPTION)
			goto exit_clean;

		if (buffer->_buffer &&
		     !gdk_pixbuf_loader_write (loader,
					       buffer->_buffer,
					       buffer->_length, NULL)) {
			CORBA_free (buffer);
			if (ev->_major == CORBA_NO_EXCEPTION)
				goto exit_clean;
			else
				goto exit_wrong_type;
		}
		len = buffer->_length;

		CORBA_free (buffer);
	} while (len > 0);

	gdk_pixbuf_loader_close (loader, NULL);
	image->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!image->priv->pixbuf)
		goto exit_wrong_type;

	gdk_pixbuf_ref (image->priv->pixbuf);

	info = Bonobo_Stream_getInfo (stream, 0, ev);
	if (!BONOBO_EX (ev)) {
		image->priv->filename = g_strdup (g_basename (info->name));
		CORBA_free (info);
	}

	gtk_signal_emit (GTK_OBJECT (image), eog_image_signals [SET_IMAGE_SIGNAL]);

	goto exit_clean;

 exit_wrong_type:
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

 exit_clean:
	gtk_object_unref (GTK_OBJECT (loader));
	return;
}

/*
 * Saves an Image to a Bonobo_Stream
 */
static void
save_image_to_stream (BonoboPersistStream *ps, Bonobo_Stream stream,
		      Bonobo_Persist_ContentType type, void *data,
		      CORBA_Environment *ev)
{
	EogImage *image;
	gboolean retval = FALSE;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE (data));

	if (getenv ("DEBUG_EOG"))
		g_message ("Trying to save '%s' to stream...", type);

	image = EOG_IMAGE (data);

	if ((type == CORBA_OBJECT_NIL) ||
	    !strcmp (type, "image/png") ||
	    !strcmp (type, "image/x-png") ||
	    !strcmp (type, ""))
		retval = eog_image_save_png (image, stream, ev);
	else if (!strcmp (type, "image/xpm") ||
		 !strcmp (type, "image/x-xpixmap"))
		retval = eog_image_save_xpm (image, stream, ev);
	else if (!strcmp (type, "image/jpeg"))
		retval = eog_image_save_jpeg (image, stream, ev);

	if (retval)
		return;

	if (!BONOBO_EX (ev))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);

	return;
}

/*
 * Loads an Image from a Bonobo_File
 */
static gint
load_image_from_file (BonoboPersistFile *pf, const CORBA_char *text_uri,
		      CORBA_Environment *ev, void *closure)
{
	EogImage *image;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	GdkPixbufLoader *loader;
	GnomeVFSURI     *uri;
	GnomeVFSFileSize bytes_read = 0;
	guchar *buffer;

	g_return_val_if_fail (closure != NULL, -1);
	g_return_val_if_fail (EOG_IS_IMAGE (closure), -1);

	if (getenv ("DEBUG_EOG"))
		g_message ("Loading file '%s'...", text_uri);

	image = EOG_IMAGE (closure);

	if (image->priv->pixbuf)
		gdk_pixbuf_unref (image->priv->pixbuf);
	image->priv->pixbuf = NULL;
	if (image->priv->filename)
		g_free (image->priv->filename);
	image->priv->filename = NULL;

	uri = gnome_vfs_uri_new (text_uri);

	/* open uri */
	result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (uri);
		return -1;
	}

	loader = gdk_pixbuf_loader_new ();
	buffer = g_new0 (guchar, 4096);
	while (TRUE) {
		result = gnome_vfs_read (handle, buffer,
					 4096, &bytes_read);
		if (result != GNOME_VFS_OK)
			break;
		
		if(!gdk_pixbuf_loader_write (loader, buffer, bytes_read, NULL))
			break;
	}

	if (result != GNOME_VFS_ERROR_EOF) {
		gdk_pixbuf_loader_close (loader, NULL);
		gnome_vfs_uri_unref (uri);
		return -1;
	}
	
	gdk_pixbuf_loader_close (loader, NULL);
	image->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (!image->priv->pixbuf) {
		gnome_vfs_uri_unref (uri);
		return -1;
	}

	gdk_pixbuf_ref (image->priv->pixbuf);

	image->priv->filename = gnome_vfs_uri_extract_short_name (uri);
	
	gnome_vfs_uri_unref (uri);

	gtk_signal_emit (GTK_OBJECT (image), eog_image_signals [SET_IMAGE_SIGNAL]);

	return 0;
}

static Bonobo_Unknown
eog_image_get_object (BonoboItemContainer *item_container,
		      CORBA_char *item_name, CORBA_boolean only_if_exists,
		      CORBA_Environment *ev, EogImage *image)
{
	Bonobo_Unknown corba_object;
	BonoboObject *object = NULL;
	GSList *params, *c;

	g_return_val_if_fail (image != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), CORBA_OBJECT_NIL);

	if (getenv ("DEBUG_EOG"))
		g_message ("Trying to get object '%s'...", item_name);

	params = eog_util_split_string (item_name, ";");
	for (c = params; c; c = c->next) {
		gchar *name = c->data;

		if ((!strcmp (name, "control") || !strcmp (name, "embeddable"))
		    && (object != NULL)) {
			g_message ("eog_image_get_object: "
				   "can only return one kind of an Object");
			continue;
		}

		if (!strcmp (name, "control"))
			object = (BonoboObject *) eog_control_new (image);
		else if (!strcmp (item_name, "embeddable"))
			object = (BonoboObject *) eog_embeddable_new (image);
		else
			g_message ("eog_image_get_object: "
				   "unknown parameter `%s'",
				   name);
	}

	g_slist_foreach (params, (GFunc) g_free, NULL);
	g_slist_free (params);

	if (object == NULL)
		return NULL;

	corba_object = bonobo_object_corba_objref (object);
	return bonobo_object_dup_ref (corba_object, ev);
}

BonoboObject *
eog_image_add_interfaces (EogImage     *image,
			  BonoboObject *to_aggregate)
{
	BonoboPersistFile   *file;
	BonoboPersistStream *stream;
	BonoboItemContainer *item_container;
	
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (BONOBO_IS_OBJECT (to_aggregate), NULL);

	/* Interface Bonobo::PersistStream */
	stream = bonobo_persist_stream_new (load_image_from_stream, 
					    save_image_to_stream,
					    NULL, NULL, image);
	if (!stream) {
		bonobo_object_unref (BONOBO_OBJECT (to_aggregate));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (to_aggregate),
				     BONOBO_OBJECT (stream));

	/* Interface Bonobo::PersistFile */
	file = bonobo_persist_file_new (load_image_from_file, NULL, NULL /*FIXME: "iid this interface is aggragated to"*/, image);
	if (!file) {
		bonobo_object_unref (BONOBO_OBJECT (to_aggregate));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (to_aggregate),
				     BONOBO_OBJECT (file));

	/* BonoboItemContainer */
	item_container = bonobo_item_container_new ();

	gtk_signal_connect (GTK_OBJECT (item_container),
			    "get_object",
			    GTK_SIGNAL_FUNC (eog_image_get_object),
			    image);

	bonobo_object_add_interface (BONOBO_OBJECT (to_aggregate),
				     BONOBO_OBJECT (item_container));

	return to_aggregate;
}

EogImage *
eog_image_construct (EogImage *image)
{
	BonoboObject *retval;

	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	retval = eog_image_add_interfaces (image, BONOBO_OBJECT (image));

	if (!retval)
		return NULL;

	/* Currently we do very little of substance in Image */

	return image;
}

EogImage *
eog_image_new (void)
{
	EogImage *image;

	/* Make sure GnomeVFS is initialized */
	if (!gnome_vfs_initialized ())
		if (!gnome_vfs_init ()) {
			g_warning (_("Couldn't initialize GnomeVFS!\n"));
			return (NULL);
		}
	
	image = gtk_type_new (eog_image_get_type ());

	return eog_image_construct (image);
}

const gchar *
eog_image_get_filename (EogImage *image)
{
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	
	return (image->priv->filename);
}

GdkPixbuf *
eog_image_get_pixbuf (EogImage *image)
{
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	if (image->priv->pixbuf)
		gdk_pixbuf_ref (image->priv->pixbuf);

	return image->priv->pixbuf;
}

void
eog_image_save_to_stream (EogImage *image, Bonobo_Stream stream, 
			  Bonobo_Persist_ContentType type, 
			  CORBA_Environment *ev)
{
	g_return_if_fail (EOG_IS_IMAGE (image));
	
	save_image_to_stream (NULL, stream, type, image, ev);
}

void
eog_image_load_from_stream (EogImage *image, Bonobo_Stream stream,
			    CORBA_Environment *ev)
{
	g_return_if_fail (EOG_IS_IMAGE (image));

	load_image_from_stream (NULL, stream, "", image, ev);
}
