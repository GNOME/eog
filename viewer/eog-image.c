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
#include <string.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include <librsvg/rsvg.h>

#include <libgnomevfs/gnome-vfs.h>

#include <eog-image.h>
#include <eog-control.h>

#include <eog-image-io.h>

struct _EogImagePrivate {
	gchar     *filename;
        GdkPixbuf *pixbuf;
};

static GObjectClass *eog_image_parent_class;

enum {
	SET_IMAGE_SIGNAL,
	LAST_SIGNAL
};

static guint eog_image_signals [LAST_SIGNAL];

static void
eog_image_destroy (BonoboObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

	if (image->priv->pixbuf) {
		g_object_unref (image->priv->pixbuf);
		image->priv->pixbuf = NULL;
	}

	if (image->priv->filename)
		g_free (image->priv->filename);

	if (BONOBO_OBJECT_CLASS (eog_image_parent_class)->destroy)
		BONOBO_OBJECT_CLASS (eog_image_parent_class)->destroy (object);
}

static void
eog_image_finalize (GObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

	g_free (image->priv);

	if (G_OBJECT_CLASS (eog_image_parent_class)->finalize)
		G_OBJECT_CLASS (eog_image_parent_class)->finalize (object);
}

static void
eog_image_class_init (EogImageClass *klass)
{
	BonoboObjectClass *bonobo_object_class = (BonoboObjectClass *)klass;
	GObjectClass *gobject_class = (GObjectClass *)klass;

	POA_GNOME_EOG_Image__epv *epv;

	eog_image_parent_class = g_type_class_peek_parent (klass);

	eog_image_signals [SET_IMAGE_SIGNAL] =
                g_signal_new ("set_image",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, set_image),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	bonobo_object_class->destroy = eog_image_destroy;
	gobject_class->finalize = eog_image_finalize;

	epv = &klass->epv;
}

static void
eog_image_init (EogImage *image)
{
	image->priv = g_new0 (EogImagePrivate, 1);
}

BONOBO_TYPE_FUNC_FULL (EogImage,
		       GNOME_EOG_Image,
		       BONOBO_TYPE_OBJECT,
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
	RsvgHandle           *handle = rsvg_handle_new ();
	Bonobo_Stream_iobuf  *buffer;
	CORBA_long            len;
	Bonobo_StorageInfo   *info;
	gboolean              write_loader = TRUE;
	gboolean              write_handle = TRUE;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE (data));

	if (getenv ("DEBUG_EOG"))
		g_message ("Loading stream...");

	image = EOG_IMAGE (data);

	if (image->priv->pixbuf)
		g_object_unref (image->priv->pixbuf);
	image->priv->pixbuf = NULL;
	if (image->priv->filename)
		g_free (image->priv->filename);
	image->priv->filename = NULL;

	do {
		Bonobo_Stream_read (stream, 4096, &buffer, ev);
		if (ev->_major != CORBA_NO_EXCEPTION)
			goto exit_clean;

		if (buffer->_buffer) {
			if (write_handle && 
			    !rsvg_handle_write (handle,
						buffer->_buffer,
						buffer->_length,
						NULL))
				write_handle = FALSE;
			if (write_loader &&
			    !gdk_pixbuf_loader_write (loader,
						      buffer->_buffer,
						      buffer->_length, NULL))
				write_loader = FALSE;

			if (!write_loader && !write_handle) {
				CORBA_free (buffer);
				if (ev->_major == CORBA_NO_EXCEPTION)
					goto exit_clean;
				else
					goto exit_wrong_type;
			}
		}
		len = buffer->_length;

		CORBA_free (buffer);
	} while (len > 0);

	gdk_pixbuf_loader_close (loader, NULL);
	rsvg_handle_close (handle, NULL);
	image->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!image->priv->pixbuf)
		image->priv->pixbuf = rsvg_handle_get_pixbuf (handle);

	if (!image->priv->pixbuf)
		goto exit_wrong_type;

	g_object_ref (image->priv->pixbuf);

	info = Bonobo_Stream_getInfo (stream, 0, ev);
	if (!BONOBO_EX (ev)) {
		image->priv->filename = g_path_get_basename (info->name);
		CORBA_free (info);
	}

	g_signal_emit (G_OBJECT (image), eog_image_signals [SET_IMAGE_SIGNAL], 0);

	goto exit_clean;

 exit_wrong_type:
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

 exit_clean:
	g_object_unref (G_OBJECT (loader));
	rsvg_handle_free (handle);
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
	RsvgHandle      *rsvg_handle;
	GnomeVFSURI     *uri;
	GnomeVFSFileSize bytes_read = 0;
	guchar *buffer;
	gboolean write_rsvg   = TRUE;
	gboolean write_pixbuf = TRUE;
	int retval = 0;

	g_return_val_if_fail (closure != NULL, -1);
	g_return_val_if_fail (EOG_IS_IMAGE (closure), -1);

	if (getenv ("DEBUG_EOG"))
		g_message ("Loading file '%s'...", text_uri);

	image = EOG_IMAGE (closure);

	if (image->priv->pixbuf)
		g_object_unref (image->priv->pixbuf);
	image->priv->pixbuf = NULL;
	if (image->priv->filename)
		g_free (image->priv->filename);
	image->priv->filename = NULL;

	uri = gnome_vfs_uri_new (text_uri);

	/* open uri */
	result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK)
		goto file_exit_error;

	loader = gdk_pixbuf_loader_new ();
	rsvg_handle = rsvg_handle_new ();
	buffer = g_new0 (guchar, 4096);
	while (TRUE) {
		result = gnome_vfs_read (handle, buffer,
					 4096, &bytes_read);
		if (result != GNOME_VFS_OK)
			break;
		
		if (write_pixbuf && !gdk_pixbuf_loader_write (loader, buffer, bytes_read, NULL))
			write_pixbuf = FALSE;

		if (write_rsvg && !rsvg_handle_write (rsvg_handle, buffer, bytes_read, NULL))
			write_rsvg = FALSE;

		if (!write_pixbuf && !write_rsvg)
			break;
	}

	if (result != GNOME_VFS_ERROR_EOF)
		goto file_exit_error;
	
	gdk_pixbuf_loader_close (loader, NULL);
	rsvg_handle_close (rsvg_handle, NULL);

	image->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!image->priv->pixbuf)
		image->priv->pixbuf = rsvg_handle_get_pixbuf (rsvg_handle);

	if (!image->priv->pixbuf)
		goto file_exit_error;

	g_object_ref (image->priv->pixbuf);

	image->priv->filename = gnome_vfs_uri_extract_short_name (uri);
      
	g_signal_emit (G_OBJECT (image), eog_image_signals [SET_IMAGE_SIGNAL], 0);

 file_exit_error:
	gnome_vfs_uri_unref (uri);
	if (loader)
		g_object_unref (G_OBJECT (loader));
	if (rsvg_handle)
		rsvg_handle_free (rsvg_handle);

	return image->priv->pixbuf ? 0 : -1;
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
#if NEED_GNOME2_PORTING
		else if (!strcmp (item_name, "embeddable"))
			object = (BonoboObject *) eog_embeddable_new (image);
#endif
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
					    NULL, "OAFIID:GNOME_EOG_Image", image);
	if (!stream) {
		bonobo_object_unref (BONOBO_OBJECT (to_aggregate));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (to_aggregate),
				     BONOBO_OBJECT (stream));

	/* Interface Bonobo::PersistFile */
	file = bonobo_persist_file_new (load_image_from_file, NULL,
					"OAFIID:GNOME_EOG_Image", image);
	if (!file) {
		bonobo_object_unref (BONOBO_OBJECT (to_aggregate));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (to_aggregate),
				     BONOBO_OBJECT (file));

	/* BonoboItemContainer */
	item_container = bonobo_item_container_new ();

	g_signal_connect (G_OBJECT (item_container),
			  "get_object",
			  G_CALLBACK (eog_image_get_object),
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
	
	image = EOG_IMAGE (g_object_new (EOG_IMAGE_TYPE, NULL));

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
		g_object_ref (image->priv->pixbuf);

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
