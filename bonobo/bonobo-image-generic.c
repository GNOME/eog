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
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, Eazel, Inc.
 */
#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <gnome.h>
#if USING_OAF
#include <liboaf/liboaf.h>
#else
#include <libgnorba/gnorba.h>
#endif

#include <bonobo.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_alphagamma.h>

#undef EOG_DEBUG

/*
 * Number of running objects
 */ 
static int running_objects = 0;
static BonoboGenericFactory    *image_factory = NULL;
static BonoboGenericFactory    *animator_factory = NULL;

/*
 * BonoboObject data
 */
typedef struct {
	BonoboEmbeddable *bonobo_object;
	GdkPixbuf        *pixbuf;
} bonobo_object_data_t;

/*
 * View data
 */
typedef struct {
	bonobo_object_data_t *bod;
	GtkWidget            *drawing_area;
        GtkWidget            *scrolled_window;
	GdkPixbuf            *scaled;
        gboolean              size_allocated;
} view_data_t;

static void
release_pixbuf_cb (BonoboView *view, void *data)
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
	
	bonobo_embeddable_foreach_view (bod->bonobo_object,
					release_pixbuf_cb,
					NULL);
}

static void
bod_destroy_cb (BonoboEmbeddable *embeddable, bonobo_object_data_t *bod)
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
	bonobo_object_unref (BONOBO_OBJECT (image_factory));
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
render_pixbuf (GdkPixbuf *buf, GtkWidget *dest_widget,
	       GdkRectangle *rect)
{
	g_return_if_fail (buf != NULL);

	/* No drawing area yet ! */
	if (!dest_widget || !dest_widget->window)
		return;

	/*
	 * Do not draw outside the region that we know how to display
	 */
	if (rect->x > gdk_pixbuf_get_width (buf))
		return;

	if (rect->y > gdk_pixbuf_get_height (buf))
		return;

	/*
	 * Clip the draw region
	 */
	if (rect->x + rect->width > gdk_pixbuf_get_width (buf))
		rect->width = gdk_pixbuf_get_width (buf) - rect->x;

	if (rect->y + rect->height > gdk_pixbuf_get_height (buf))
		rect->height = gdk_pixbuf_get_height (buf) - rect->y;

	/*
	 * Draw the exposed region.
	 */
	if (gdk_pixbuf_get_has_alpha (buf))
		gdk_draw_rgb_32_image (dest_widget->window,
				       dest_widget->style->white_gc,
				       rect->x, rect->y,
				       rect->width,
				       rect->height,
				       GDK_RGB_DITHER_NORMAL,
				       gdk_pixbuf_get_pixels (buf)
				       + (gdk_pixbuf_get_rowstride (buf) * rect->y + rect->x * 4),
				       gdk_pixbuf_get_rowstride (buf));
	else
		gdk_draw_rgb_image (dest_widget->window,
				    dest_widget->style->white_gc,
				    rect->x, rect->y,
				    rect->width,
				    rect->height,
				    GDK_RGB_DITHER_NORMAL,
				    gdk_pixbuf_get_pixels (buf)
				    + (gdk_pixbuf_get_rowstride (buf) * rect->y + rect->x * 3),
				    gdk_pixbuf_get_rowstride (buf));
}

static void
redraw_view (view_data_t *view_data, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (view_data);

	g_return_if_fail (buf != NULL);

	/*
	 * Don't actually render unless our size has been allocated,
	 * so we don't screw up the size allocation process by drawing
	 * an unscaled image too early.
	 */

	if (view_data->size_allocated)
	        render_pixbuf (buf, view_data->drawing_area, rect);
}

static void
configure_size (view_data_t *view_data, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (view_data);

	g_return_if_fail (buf != NULL);
	g_return_if_fail (view_data != NULL);

	/*
	 * Don't configure the size if it hsan't gotten allocated, to
	 * avoid messing with size_allocate process.
	 */
	if (view_data->size_allocated) {
		gtk_widget_set_usize (view_data->drawing_area,
				      gdk_pixbuf_get_width (buf),
				      gdk_pixbuf_get_height (buf));
	  
		rect->x = 0;
		rect->y = 0;
		rect->width  = gdk_pixbuf_get_width (buf);
		rect->height = gdk_pixbuf_get_height (buf);
	}
}

static void
redraw_all_cb (BonoboView *view, void *data)
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
 * Loads an Image from a Bonobo_Stream
 */
static int
load_image_from_stream (BonoboPersistStream *ps, Bonobo_Stream stream, void *data)
{
	int                   retval = 0;
	bonobo_object_data_t *bod = data;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	Bonobo_Stream_iobuf   *buffer;
	CORBA_Environment     ev;

	CORBA_exception_init (&ev);

	do {
		buffer = Bonobo_Stream_iobuf__alloc ();
		Bonobo_Stream_read (stream, 4096, &buffer, &ev);
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
		bonobo_embeddable_foreach_view (bod->bonobo_object,
						redraw_all_cb, bod);
	
	CORBA_exception_free (&ev);

	return retval;
}

static void
destroy_view (BonoboView *view, view_data_t *view_data)
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

	view_data->size_allocated = TRUE;

	buf = view_data->bod->pixbuf;

	if (!view_data->bod->pixbuf)
		return;

#ifdef EOG_DEBUG
	g_warning ("Size allocate");
#endif

	if (allocation->width  == gdk_pixbuf_get_width (buf) &&
	    allocation->height == gdk_pixbuf_get_height (buf)) {
		if (view_data->scaled) {
			gdk_pixbuf_unref (view_data->scaled);
			view_data->scaled = NULL;
		}
		return;
	}

	view_buf = view_data->scaled;
	if (view_buf) {
		if (allocation->width  == gdk_pixbuf_get_width (view_buf) &&
		    allocation->height == gdk_pixbuf_get_height (view_buf)) {
#ifdef EOG_DEBUG
			g_warning ("Correct size %d, %d", allocation->width, allocation->height);
#endif
			return;
		} else {
			view_data->scaled = NULL;
			gdk_pixbuf_unref (view_buf);
			view_buf = NULL;
		}
	}

#ifdef EOG_DEBUG
	g_warning ("Re-scale");
#endif
	/* FIXME: should we be FILTER_TILES / FILTER_NEAREST ? */
	view_data->scaled = gdk_pixbuf_scale_simple (buf, allocation->width,
						     allocation->height,
						     ART_FILTER_TILES);
	view_update (view_data);
}

static void
render_fn (GnomePrintContext         *ctx,
	   double                     width,
	   double                     height,
	   const Bonobo_PrintScissor *opt_scissor,
	   gpointer                   user_data)
{
	bonobo_object_data_t *bod = user_data;
	GdkPixbuf            *buf;
	double                matrix[6];

	g_return_if_fail (bod != NULL);
	g_return_if_fail (bod->pixbuf != NULL);

	buf = bod->pixbuf;

#ifdef EOG_DEBUG
	g_warning ("Printing %g %g", width, height);
#endif

	art_affine_scale (matrix, width, height);
	matrix[4] = 0;
	matrix[5] = 0;

	gnome_print_gsave     (ctx);
	gnome_print_concat    (ctx, matrix);

	if (gdk_pixbuf_get_has_alpha (buf))
		gnome_print_rgbaimage  (ctx,
					gdk_pixbuf_get_pixels    (buf),
					gdk_pixbuf_get_width     (buf),
					gdk_pixbuf_get_height    (buf),
					gdk_pixbuf_get_rowstride (buf));
	else
		gnome_print_rgbimage  (ctx,
				       gdk_pixbuf_get_pixels    (buf),
				       gdk_pixbuf_get_width     (buf),
				       gdk_pixbuf_get_height    (buf),
				       gdk_pixbuf_get_rowstride (buf));
	gnome_print_grestore  (ctx);
}

static BonoboView *
view_factory_common (BonoboEmbeddable *bonobo_object,
		     GtkWidget        *scrolled_window,
		     const Bonobo_ViewFrame view_frame,
		     void *data)
{
        BonoboView *view;
	bonobo_object_data_t *bod = data;
	view_data_t *view_data = g_new (view_data_t, 1);
	GtkWidget   *root;

	view_data->bod = bod;
	view_data->scaled = NULL;
	view_data->drawing_area = gtk_drawing_area_new ();
	view_data->size_allocated = TRUE;
	view_data->scrolled_window = scrolled_window;

	gtk_signal_connect (
		GTK_OBJECT (view_data->drawing_area),
		"expose_event",
		GTK_SIGNAL_FUNC (drawing_area_exposed), view_data);

	if (scrolled_window) {
		root = scrolled_window;
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (root), 
						       view_data->drawing_area);
	} else
		root = view_data->drawing_area;

	gtk_widget_show_all (root);
	view = bonobo_view_new (root);

	gtk_object_set_data (GTK_OBJECT (view), "view_data",
			     view_data);

	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (destroy_view), view_data);

        return view;
}

static BonoboView *
scaled_view_factory (BonoboEmbeddable *bonobo_object,
		     const Bonobo_ViewFrame view_frame,
		     void *data)
{
        BonoboView  *view;
	view_data_t *view_data;

	view = view_factory_common (bonobo_object, NULL, view_frame, data);

	view_data = gtk_object_get_data (GTK_OBJECT (view), "view_data");

	gtk_signal_connect (GTK_OBJECT (view_data->drawing_area), "size_allocate",
			    GTK_SIGNAL_FUNC (view_size_allocate_cb), view_data);

        return view;
}

static BonoboView *
scrollable_view_factory (BonoboEmbeddable *bonobo_object,
			 const Bonobo_ViewFrame view_frame,
			 void *data)
{
        BonoboView *view;
	view_data_t *view_data;
	GtkWidget   *scroll;

	scroll = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	view = view_factory_common (bonobo_object, scroll, view_frame, data);

	view_data = gtk_object_get_data (GTK_OBJECT (view), "view_data");

	view_data->size_allocated = TRUE;

        return view;
}

static BonoboObject *
bonobo_object_factory (BonoboGenericFactory *this, const char *goad_id, void *data)
{
	BonoboEmbeddable     *bonobo_object;
	BonoboPersistStream  *stream;
	bonobo_object_data_t *bod;
	BonoboPrint          *print;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (this->goad_id != NULL, NULL);

	bod = g_new0 (bonobo_object_data_t, 1);
	if (!bod)
		return NULL;

	/*
	 * Creates the BonoboObject server
	 */

#if USING_OAF
	if (!strcmp (goad_id, "OAFIID:eog_image-generic:0d77ee99-ce0d-4463-94ec-99969f567f33"))
#else
	if (!strcmp (goad_id, "embeddable:image-generic"))
#endif
		bonobo_object = bonobo_embeddable_new (scaled_view_factory, bod);


#if USING_OAF
	else if (!strcmp (goad_id, "OAFIID:eog-image-viewer:f67c01c8-a44d-4c50-8ce1-dc893c961876"))
#else
	else if (!strcmp (goad_id, "eog-image-viewer"))
#endif
		bonobo_object = bonobo_embeddable_new (scrollable_view_factory, bod);

	else {
		g_free (bod);
		return NULL;
	}

	if (bonobo_object == NULL) {
		g_free (bod);
		return NULL;
	}

	bod->pixbuf = NULL;

	/*
	 * Interface Bonobo::PersistStream 
	 */
	stream = bonobo_persist_stream_new (load_image_from_stream,
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
	bonobo_object_add_interface (BONOBO_OBJECT (bonobo_object),
				     BONOBO_OBJECT (stream));

	/*
	 * Interface Bonobo::PersistStream 
	 */
	print = bonobo_print_new (render_fn, bod);
	bonobo_object_add_interface (BONOBO_OBJECT (bonobo_object),
				     BONOBO_OBJECT (print));

	return BONOBO_OBJECT (bonobo_object);
}

static void
init_bonobo_image_generic_factory (void)
{
#if USING_OAF
	image_factory = bonobo_generic_factory_new_multi 
		("OAFIID:eog_viewer_factory:777e0cdf-2b79-4e36-93d8-e9d490c9c4b8",
		 bonobo_object_factory, NULL);
#else
        image_factory = bonobo_generic_factory_new_multi 
	        ("embeddable-factory:image-generic",
		 bonobo_object_factory, NULL);
#endif
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

#ifdef USING_OAF
        gnome_init_with_popt_table("bonobo-image-generic", VERSION,
				   argc, argv,
				   oaf_popt_options, 0, NULL); 
	oaf_init (argc, argv);
#else
	gnome_CORBA_init_with_popt_table (
		"bonobo-image-generic", "1.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);
#endif

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("I could not initialize Bonobo"));

	CORBA_exception_free (&ev);
}

#define PROP_RUNNING 1
#define PROP_FNAME   2

typedef struct {
	GtkWidget          *drawing_area;
	GdkPixbufAnimation *animation;
	GdkPixbuf          *frame;
	GList              *cur_frame;
	guint               timeout;
	
	gboolean            animate;
	char               *fname;
} AnimationState;

static void
animation_state_clean (AnimationState *as)
{
	if (as->animation)
		gdk_pixbuf_animation_unref (as->animation);
	as->animation = NULL;

	if (as->frame)
		gdk_pixbuf_unref (as->frame);
	as->frame = NULL;

	as->cur_frame    = NULL;
}

static gint
skip_frame (AnimationState *as)
{
	GdkPixbufFrame *frame;
	GdkPixbuf      *layer;
	int w, h;

	g_return_val_if_fail (as != NULL, FALSE);

	if (!as->animate)
		return FALSE;

	if (!as->cur_frame) {
		g_warning ("No frame to render");
		return FALSE;
	}

	frame = as->cur_frame->data;
	g_return_val_if_fail (frame != NULL, FALSE);
	g_return_val_if_fail (gdk_pixbuf_frame_get_pixbuf (frame) != NULL, FALSE);

	w = gdk_pixbuf_get_width (gdk_pixbuf_frame_get_pixbuf (frame));
	h = gdk_pixbuf_get_height (gdk_pixbuf_frame_get_pixbuf (frame));

	if (gdk_pixbuf_frame_get_x_offset (frame) + w
	    > gdk_pixbuf_get_width (as->frame))
		w = gdk_pixbuf_get_width (as->frame)
		  - gdk_pixbuf_frame_get_y_offset (frame);

	if (gdk_pixbuf_frame_get_y_offset (frame) + h
	    > gdk_pixbuf_get_height (as->frame))
		h = gdk_pixbuf_get_height (as->frame)
		  - gdk_pixbuf_frame_get_y_offset (frame);

/*	printf ("Rendering patch of size (%d %d) at (%d %d) for delay %d mode %d %d\n",
		w, h, frame->x_offset, frame->y_offset, frame->delay_time,
		frame->action, frame->pixbuf->art_pixbuf->has_alpha);*/

	layer = gdk_pixbuf_frame_get_pixbuf (frame);

	if (!as->cur_frame->prev ||
	    gdk_pixbuf_frame_get_action (frame) == GDK_PIXBUF_FRAME_REVERT) {
		gdk_pixbuf_copy_area (layer, 0, 0, w, h, as->frame,
				      gdk_pixbuf_frame_get_x_offset (frame),
				      gdk_pixbuf_frame_get_y_offset (frame));
	} else
		gdk_pixbuf_composite (layer, as->frame,
				      gdk_pixbuf_frame_get_x_offset (frame),
				      gdk_pixbuf_frame_get_y_offset (frame),
				      w, h,
				      gdk_pixbuf_frame_get_x_offset (frame),
				      gdk_pixbuf_frame_get_y_offset (frame),
				      1.0, 1.0,
				      ART_FILTER_NEAREST, 255);

	gtk_widget_queue_draw_area (as->drawing_area,
				    gdk_pixbuf_frame_get_x_offset (frame),
				    gdk_pixbuf_frame_get_y_offset (frame),
				    w, h);
	as->timeout = gtk_timeout_add (gdk_pixbuf_frame_get_delay_time (frame) * 10,
				       (GtkFunction) skip_frame, as);

	as->cur_frame = as->cur_frame->next;
	if (!as->cur_frame)
		as->cur_frame = gdk_pixbuf_animation_get_frames (as->animation);

	return FALSE;
}

static void
animation_init (AnimationState *as, char *fname)
{
	as->fname = g_strdup (fname);
	as->animation = gdk_pixbuf_animation_new_from_file (fname);

	if (as->animation) {
		GdkPixbufFrame *frame;

		as->cur_frame = gdk_pixbuf_animation_get_frames (as->animation);
		g_return_if_fail (as->cur_frame != NULL);

		frame = as->cur_frame->data;

		/*
		 *  Pray we never have alpha on the base frame, since this
		 * seriously sods up the use of it on subsequent frames as a mask.
		 */
		as->frame = gdk_pixbuf_copy (gdk_pixbuf_frame_get_pixbuf (frame));
		
		skip_frame (as);

		/* re-size */
		gtk_widget_set_usize (as->drawing_area,
				      gdk_pixbuf_get_width  (as->frame),
				      gdk_pixbuf_get_height (as->frame));
	} else
		g_warning ("Error loading animation '%s'", fname);
}

static void
animation_destroy (BonoboView *view, AnimationState *as)
{
	g_return_if_fail (as != NULL);

	animation_state_clean (as);
	as->drawing_area = NULL;

	gtk_timeout_remove (as->timeout);

	if (as->fname)
		g_free (as->fname);
	as->fname = NULL;

	g_free (as);
}

static int
animation_area_exposed (GtkWidget *widget, GdkEventExpose *event,
			AnimationState *as)
{
	GdkPixbufFrame *frame;

	g_return_val_if_fail (as != NULL, TRUE);

	if (!as->frame)
		return TRUE;

	if (!as->cur_frame && !as->cur_frame->data)
		return TRUE;

/*	render_pixbuf (as->frame, as->drawing_area, &event->area);*/
/* FIXME: herin lies the problem ... this needs to be fixed */
	frame = as->cur_frame->data;

	render_pixbuf (gdk_pixbuf_frame_get_pixbuf (frame), as->drawing_area, &event->area);

	return TRUE;
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	AnimationState *as = user_data;

	switch (arg_id) {

	case PROP_RUNNING:
		if (BONOBO_ARG_GET_BOOLEAN (arg)) {
			as->animate = TRUE;
			skip_frame (as);
		} else
			as->animate = FALSE;
		break;

	case PROP_FNAME:
		animation_state_clean (as);
		animation_init (as, BONOBO_ARG_GET_STRING (arg));
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	AnimationState *as = user_data;

	switch (arg_id) {

	case PROP_RUNNING:
		BONOBO_ARG_SET_BOOLEAN (arg, as->animate);
		break;

	case PROP_FNAME:
		if (as->fname)
			BONOBO_ARG_SET_STRING (arg, as->fname);
		else
			BONOBO_ARG_SET_STRING (arg, "");
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static BonoboObject *
bonobo_animator_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboPropertyBag *pb;
	BonoboControl     *control;
	AnimationState    *as;

	as = g_new0 (AnimationState, 1);
	as->fname = NULL;

	/* Create the control. */
	as->drawing_area = gtk_drawing_area_new ();
	gtk_signal_connect (GTK_OBJECT (as->drawing_area), "expose_event",
			    GTK_SIGNAL_FUNC (animation_area_exposed), as);

	gtk_widget_set_usize (as->drawing_area, 10, 10);

	gtk_widget_show (as->drawing_area);

	g_warning ("New animator created");

	control = bonobo_control_new (as->drawing_area);
	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    GTK_SIGNAL_FUNC (animation_destroy), as);

	/* Create the properties. */
	pb = bonobo_property_bag_new (get_prop, set_prop, as);
	bonobo_control_set_property_bag (control, pb);

	bonobo_property_bag_add (pb, "running", PROP_RUNNING,
				 BONOBO_ARG_BOOLEAN, NULL,
				 "Whether or not we are animating", 0);

	bonobo_property_bag_add (pb, "filename", PROP_FNAME,
				 BONOBO_ARG_STRING, NULL,
				 "The filename of the animation", 0);

	return BONOBO_OBJECT (control);
}

static void
init_bonobo_animator_control_factory (void)
{
	if (animator_factory != NULL)
		return;

#if USING_OAF
	animator_factory = bonobo_generic_factory_new ("OAFIID:eog_animator_factory:8d2380ad-6e60-4986-ae27-152501839f57",
						       bonobo_animator_factory, NULL);
#else
	animator_factory = bonobo_generic_factory_new ("control-factory:animator",
						       bonobo_animator_factory, NULL);
#endif

	if (animator_factory == NULL)
		g_error ("I could not register an animator factory.");
}

int
main (int argc, char *argv [])
{
	init_server_factory (argc, argv);

	init_bonobo_image_generic_factory ();

	init_bonobo_animator_control_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}
