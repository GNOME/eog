/*
 * Generic image loading embeddable using gdk-pixbuf.
 *
 * Author:
 *   Michael Meeks (mmeeks@gnu.org)
 *
 * TODO:
 *    Get rid of temp file!
 *    Progressive loading.
 *    Do not display more than required
 *    Queue request-resize on image size change/load
 *    Save image
 */
/*#define GENERIC_IMAGE*/
#ifdef  GENERIC_IMAGE
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
/*
 * BonoboObject data
 */
typedef struct {
	GnomeEmbeddable *bonobo_object;
	GdkPixbuf       *pixbuf;
	char            *repoid;
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
bod_destroy (bonobo_object_data_t *bod)
{
        if (!bod)
		return;

	release_pixbuf (bod);

	if (bod->repoid)
		g_free (bod->repoid);
	bod->repoid = NULL;

	g_free (bod);
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
redraw_all_cb (GnomeView *view, bonobo_object_data_t *bod)
{
	GdkRectangle rect;
	view_data_t *view_data = gtk_object_get_data (GTK_OBJECT (view),
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
 * Callback for reading from a stream.
 */
static char *
copy_to_file (GNOME_Stream stream)
{
	char *tmpn;
	int   fd;
	CORBA_Environment ev;
	GNOME_Stream_iobuf *buffer;

	tmpn = tempnam ("/tmp", "gimage");
	g_return_val_if_fail (tmpn != NULL, NULL);

	g_warning ("Opening tmp file '%s'", tmpn);
	fd = open (tmpn, O_WRONLY | O_CREAT | O_EXCL,
		   S_IWUSR | S_IRUSR);
	perror ("tmp file error ");
	g_return_val_if_fail (fd != -1, NULL);

	CORBA_exception_init (&ev);

	do {
		buffer = GNOME_Stream_iobuf__alloc ();
		GNOME_Stream_read (stream, 4096, &buffer, &ev);
		write (fd, buffer->_buffer, buffer->_length);
		CORBA_free (buffer);
	} while (buffer->_length > 0);

	CORBA_exception_free (&ev);

	close (fd);

	return tmpn;
}

/*
 * Loads an Image from a GNOME_Stream
 */
static int
load_image_from_stream (GnomePersistStream *ps, GNOME_Stream stream, void *data)
{
	bonobo_object_data_t *bod = data;
	char *name;

	release_pixbuf (bod);

	name = copy_to_file (stream);
	if (!name)
		return -1;

	bod->pixbuf = gdk_pixbuf_new_from_file (name);
	if (!bod->pixbuf)
		return -1;

	unlink (name);
	free (name);

	gnome_embeddable_foreach_view (bod->bonobo_object,
				       redraw_all_cb, bod);
	return 0;
}

static void
destroy_view (GnomeView *view, view_data_t *view_data)
{
	g_warning ("Fixme: destroy view");
/*	view_data->bod->views = g_list_remove (view_data->bod->views, view_data);
	gtk_object_unref (GTK_OBJECT (view_data->drawing_area));*/

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

static void
view_size_query_cb (GnomeView *view, int *desired_width, int *desired_height,
		    view_data_t *view_data)
{
	ArtPixBuf *pixbuf;
	
	g_return_if_fail (view_data != NULL);
	g_return_if_fail (view_data->bod != NULL);
	g_return_if_fail (view_data->bod->pixbuf != NULL);

	pixbuf = view_data->bod->pixbuf->art_pixbuf;

	*desired_width  = pixbuf->width;
	*desired_height = pixbuf->height;
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
	g_return_if_fail (view_data->bod->pixbuf != NULL);

	buf = view_data->bod->pixbuf;

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

	gtk_signal_connect (GTK_OBJECT (view), "size_query",
			    GTK_SIGNAL_FUNC (view_size_query_cb), view_data);
	gtk_object_set_data (GTK_OBJECT (view), "view_data",
			     view_data);

	gtk_signal_connect (
		GTK_OBJECT (view), "destroy",
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

	{ /* Generate the objects goad id */
		char *p = this->goad_id;
		
		while (*p && *p != ':') {
			p++;
		}
		bod->repoid = g_strconcat ("embeddable", p, NULL);
		g_warning ("Creating object '%s'\n", bod->repoid);
	}

	bod->pixbuf = NULL;

	/*
	 * Interface GNOME::PersistStream 
	 */
	stream = gnome_persist_stream_new (bod->repoid,
					   load_image_from_stream,
					   NULL,
					   bod);
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (bonobo_object));
		g_free (bod->repoid);
		g_free (bod);
		return NULL;
	}

	bod->bonobo_object = bonobo_object;

	/*
	 * Bind the interfaces
	 */
	gnome_object_add_interface (GNOME_OBJECT (bonobo_object),
				    GNOME_OBJECT (stream));
	return (GnomeObject *) bonobo_object;
}

static void
init_bonobo_image_generic_factory (void)
{
	GnomeEmbeddableFactory *factory;
	
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

#endif /* GENERIC_IMAGE */
int
main (int argc, char *argv [])
{
#ifdef GENERIC_IMAGE
	init_server_factory (argc, argv);

	init_bonobo_image_generic_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
#endif /* GENERIC_IMAGE */
	return 0;
}
