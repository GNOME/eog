/*
 * Generic image loading embeddable using gdk-pixbuf.
 *
 * Author:
 *   Michael Meeks (mmeeks@gnu.org)
 *
 * TODO:
 *    Progressive loading.
 *    Do not display more than required
 *    Queue request-resize on image size change/load
 *    Save image
 */
#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_alphagamma.h>

/*
 * Number of running objects
 */ 
static int running_objects = 0;
static GnomeEmbeddableFactory *factory = NULL;

/*
 * BonoboObject data
 */
typedef struct {
	GnomeEmbeddable *bonobo_object;
	GdkPixbuf       *pixbuf;
} bonobo_object_data_t;

/*
 * View data
 */
typedef struct {
	bonobo_object_data_t *bod;
	GtkWidget            *drawing_area;
	GdkPixbuf            *scaled;
} view_data_t;

static void
release_pixbuf_cb (GnomeView *view, void *data)
{
	view_data_t *view_data = gtk_object_get_data (GTK_OBJECT (view),
						      "view_data");
	if (!view_data ||
	    !view_data->scaled)
		return;
	
	gdk_pixbuf_unref (view_data->scaled);
	view_data->scaled = NULL;
}
/*
 * Releases an image
 */
static void
release_pixbuf (bonobo_object_data_t *bod)
{
	g_return_if_fail (bod != NULL);

	if (bod->pixbuf)
		gdk_pixbuf_unref (bod->pixbuf);
	bod->pixbuf = NULL;
	
	gnome_embeddable_foreach_view (bod->bonobo_object,
				       release_pixbuf_cb,
				       NULL);
}

static void
bod_destroy_cb (GnomeEmbeddable *embeddable, bonobo_object_data_t *bod)
{
        if (!bod)
		return;

	release_pixbuf (bod);

	g_free (bod);

	running_objects--;
	if (running_objects > 0)
		return;
	/*
	 * When last object has gone unref the factory & quit.
	 */
	gnome_object_unref (GNOME_OBJECT (factory));
	gtk_main_quit ();
}

static GdkPixbuf *
get_pixbuf (view_data_t *view_data)
{
	g_return_val_if_fail (view_data != NULL, NULL);

	if (view_data->scaled)
		return view_data->scaled;
	else {
		bonobo_object_data_t *bod = view_data->bod;

		g_return_val_if_fail (bod != NULL, NULL);

		return bod->pixbuf;
	}
}

static void
redraw_view (view_data_t *view_data, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (view_data);
	ArtPixBuf *pixbuf;

	g_return_if_fail (buf != NULL);

	pixbuf = buf->art_pixbuf;

	g_return_if_fail (pixbuf != NULL);

	/* No drawing area yet ! */
	if (!view_data->drawing_area->window)
		return;

	/*
	 * Do not draw outside the region that we know how to display
	 */
	if (rect->x > pixbuf->width)
		return;

	if (rect->y > pixbuf->height)
		return;

	/*
	 * Clip the draw region
	 */
	if (rect->x + rect->width > pixbuf->width)
		rect->width = pixbuf->width - rect->x;

	if (rect->y + rect->height > pixbuf->height)
		rect->height = pixbuf->height - rect->y;

	/*
	 * Draw the exposed region.
	 */
	if (pixbuf->has_alpha)
		gdk_draw_rgb_32_image (view_data->drawing_area->window,
				       view_data->drawing_area->style->white_gc,
				       rect->x, rect->y,
				       rect->width,
				       rect->height,
				       GDK_RGB_DITHER_NORMAL,
				       pixbuf->pixels + (pixbuf->rowstride * rect->y + rect->x * 4),
				       pixbuf->rowstride);
	else
		gdk_draw_rgb_image (view_data->drawing_area->window,
				    view_data->drawing_area->style->white_gc,
				    rect->x, rect->y,
				    rect->width,
				    rect->height,
				    GDK_RGB_DITHER_NORMAL,
				    pixbuf->pixels + (pixbuf->rowstride * rect->y + rect->x * 3),
				    pixbuf->rowstride);
}

static void
configure_size (view_data_t *view_data, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (view_data);
	ArtPixBuf *pixbuf;

	g_return_if_fail (buf != NULL);
	g_return_if_fail (view_data != NULL);

	pixbuf = buf->art_pixbuf;

	gtk_widget_set_usize (
		view_data->drawing_area,
		pixbuf->width,
		pixbuf->height);

	rect->x = 0;
	rect->y = 0;
	rect->width  = pixbuf->width;
	rect->height = pixbuf->height;
}

static void
redraw_all_cb (GnomeView *view, void *data)
{
	GdkRectangle rect;
	view_data_t *view_data;

	g_return_if_fail (view != NULL);

	view_data = gtk_object_get_data (GTK_OBJECT (view),
					 "view_data");
	configure_size (view_data, &rect);
	
	redraw_view (view_data, &rect);
}

static void
view_update (view_data_t *view_data)
{
	GdkRectangle rect;

	g_return_if_fail (view_data != NULL);

	configure_size (view_data, &rect);
		
	redraw_view (view_data, &rect);
}

/*
 * Loads an Image from a GNOME_Stream
 */
static int
load_image_from_stream (GnomePersistStream *ps, GNOME_Stream stream, void *data)
{
	int                   retval = 0;
	bonobo_object_data_t *bod = data;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	GNOME_Stream_iobuf   *buffer;
	CORBA_Environment     ev;

	CORBA_exception_init (&ev);

	do {
		buffer = GNOME_Stream_iobuf__alloc ();
		GNOME_Stream_read (stream, 4096, &buffer, &ev);
		if (buffer->_buffer &&
		    (ev._major != CORBA_NO_EXCEPTION ||
		     !gdk_pixbuf_loader_write (loader,
					       buffer->_buffer,
					       buffer->_length))) {
			if (ev._major != CORBA_NO_EXCEPTION)
				g_warning ("Fatal error loading from stream");
			else
				g_warning ("Fatal image format error");

			gdk_pixbuf_loader_close (loader);
			gtk_object_destroy (GTK_OBJECT (loader));
			CORBA_free (buffer);
			CORBA_exception_free (&ev);
			return -1;
		}
		CORBA_free (buffer);
	} while (buffer->_length > 0);

	bod->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	gdk_pixbuf_ref (bod->pixbuf);
	gdk_pixbuf_loader_close (loader);
	gtk_object_destroy (GTK_OBJECT (loader));

	if (!bod->pixbuf)
		retval = -1;
	else
		gnome_embeddable_foreach_view (bod->bonobo_object,
					       redraw_all_cb, bod);
	
	CORBA_exception_free (&ev);

	return retval;
}

static void
destroy_view (GnomeView *view, view_data_t *view_data)
{
	g_return_if_fail (view_data != NULL);

	if (view_data->scaled)
		gdk_pixbuf_unref (view_data->scaled);
	view_data->scaled = NULL;

	g_free (view_data);
}

static int
drawing_area_exposed (GtkWidget *widget, GdkEventExpose *event, view_data_t *view_data)
{
	if (!view_data->bod->pixbuf)
		return TRUE;
	
	redraw_view (view_data, &event->area);

	return TRUE;
}

static GdkPixbuf *
gdk_pixbuf_scale (const GdkPixbuf *pixbuf, gint w, gint h)
{
	art_u8 *pixels;
	gint rowstride;
	double affine[6];
	ArtPixBuf *art_pixbuf = NULL;
	GdkPixbuf *copy = NULL;

	affine[1] = affine[2] = affine[4] = affine[5] = 0;

	affine[0] = w / (double)(pixbuf->art_pixbuf->width);
	affine[3] = h / (double)(pixbuf->art_pixbuf->height);

	/* rowstride = w * pixbuf->art_pixbuf->n_channels; */
	rowstride = w * 3;

	pixels = art_alloc (h * rowstride);
	art_rgb_pixbuf_affine (pixels, 0, 0, w, h, rowstride,
			       pixbuf->art_pixbuf,
			       affine, ART_FILTER_NEAREST, NULL);

	if (pixbuf->art_pixbuf->has_alpha)
		/* should be rgba */
		art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride);
	else
		art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride);

	copy = gdk_pixbuf_new_from_art_pixbuf (art_pixbuf);

	if (!copy)
		art_free (pixels);

	return copy;
}

/*
 * This callback will be invoked when the container assigns us a size.
 */
static void
view_size_allocate_cb (GtkWidget *drawing_area, GtkAllocation *allocation,
		       view_data_t *view_data)
{
	const GdkPixbuf *buf;
	GdkPixbuf       *view_buf;

	g_return_if_fail (view_data != NULL);
	g_return_if_fail (allocation != NULL);
	g_return_if_fail (view_data->bod != NULL);

	buf = view_data->bod->pixbuf;

	if (!view_data->bod->pixbuf ||
	    !view_data->bod->pixbuf->art_pixbuf)
		return;

	if (allocation->width  == buf->art_pixbuf->width &&
	    allocation->height == buf->art_pixbuf->height) {
		if (view_data->scaled) {
			gdk_pixbuf_unref (view_data->scaled);
			view_data->scaled = NULL;
		}
		return;
	}

	view_buf = view_data->scaled;
	if (view_buf) {
		if (allocation->width  == view_buf->art_pixbuf->width &&
		    allocation->height == view_buf->art_pixbuf->height)
			return;
		else {
			view_data->scaled = NULL;
			gdk_pixbuf_unref (view_buf);
			view_buf = NULL;
		}
	}

	view_data->scaled = gdk_pixbuf_scale (buf,
					      allocation->width,
					      allocation->height);
	view_update (view_data);
}

static GnomeView *
view_factory (GnomeEmbeddable *bonobo_object,
	      const GNOME_ViewFrame view_frame,
	      void *data)
{
        GnomeView *view;
	bonobo_object_data_t *bod = data;
	view_data_t *view_data = g_new (view_data_t, 1);

	view_data->bod = bod;
	view_data->drawing_area = gtk_drawing_area_new ();
	view_data->scaled = NULL;

	gtk_signal_connect (
		GTK_OBJECT (view_data->drawing_area),
		"expose_event",
		GTK_SIGNAL_FUNC (drawing_area_exposed), view_data);

        gtk_widget_show (view_data->drawing_area);
        view = gnome_view_new (view_data->drawing_area);
	gtk_signal_connect (GTK_OBJECT (view_data->drawing_area), "size_allocate",
			    GTK_SIGNAL_FUNC (view_size_allocate_cb), view_data);

	gtk_object_set_data (GTK_OBJECT (view), "view_data",
			     view_data);

	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (destroy_view), view_data);

        return view;
}

static GnomeObject *
bonobo_object_factory (GnomeEmbeddableFactory *this, void *data)
{
	GnomeEmbeddable *bonobo_object;
	GnomePersistStream *stream;
	bonobo_object_data_t *bod;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (this->goad_id != NULL, NULL);

	bod = g_new0 (bonobo_object_data_t, 1);
	if (!bod)
		return NULL;

	/*
	 * Creates the BonoboObject server
	 */
	bonobo_object = gnome_embeddable_new (view_factory, bod);
	if (bonobo_object == NULL) {
		g_free (bod);
		return NULL;
	}

	bod->pixbuf = NULL;

	/*
	 * Interface GNOME::PersistStream 
	 */
	stream = gnome_persist_stream_new (load_image_from_stream,
					   NULL, bod);
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (bonobo_object));
		g_free (bod);
		return NULL;
	}

	bod->bonobo_object = bonobo_object;

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (bod_destroy_cb), bod);
	/*
	 * Bind the interfaces
	 */
	gnome_object_add_interface (GNOME_OBJECT (bonobo_object),
				    GNOME_OBJECT (stream));

	return GNOME_OBJECT (bonobo_object);
}

static void
init_bonobo_image_generic_factory (void)
{
	factory = gnome_embeddable_factory_new (
		"embeddable-factory:image-generic",
		bonobo_object_factory, NULL);
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table (
		"bonobo-image-generic", "1.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("I could not initialize Bonobo"));

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
	init_server_factory (argc, argv);

	init_bonobo_image_generic_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}
