#include <config.h>
#include <math.h>
#include <gnome.h>
#include "image.h"
#include "image-item.h"
#include "gtkscrollframe.h"


static GnomeCanvas *canvas;


static void
realize (GtkWidget *widget, gpointer data)
{
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window, NULL, FALSE);
}

static void
zoom_changed (GtkAdjustment *adj, gpointer data)
{
	gnome_canvas_set_pixels_per_unit (canvas, adj->value);
}

static void
sb_spacing_changed (GtkAdjustment *adj, gpointer data)
{
	gtk_scroll_frame_set_scrollbar_spacing (GTK_SCROLL_FRAME (data), (int) adj->value);
}

static GtkWidget *
create_sb_spacing (GtkWidget *sf)
{
	GtkWidget *hbox;
	GtkWidget *w;
	GtkAdjustment *adj;

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

	w = gtk_label_new ("Scrollbar spacing");
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (3.0, 0.0, 50.0, 1.0, 1.0, 1.0));
	w = gtk_spin_button_new (adj, 0.0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);

	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (sb_spacing_changed),
			    sf);

	return hbox;
}

static void
shadow_type_changed (GtkAdjustment *adj, gpointer data)
{
	gtk_scroll_frame_set_shadow_type (GTK_SCROLL_FRAME (data), (int) adj->value);
}

static GtkWidget *
create_shadow_type (GtkWidget *sf)
{
	GtkWidget *hbox;
	GtkWidget *w;
	GtkAdjustment *adj;

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

	w = gtk_label_new ("Shadow type");
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 4.0, 1.0, 1.0, 1.0));
	w = gtk_spin_button_new (adj, 0.0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);

	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (shadow_type_changed),
			    sf);

	return hbox;
}

int
main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *sf;
	GtkWidget *w;
	GtkAdjustment *adj;
	Image *image;
	int i;
	GnomeCanvasItem *item;
	poptContext ctx;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table (PACKAGE, VERSION, argc, argv,
				    NULL, 0, &ctx);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	sf = gtk_scroll_frame_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), sf, TRUE, TRUE, 0);
	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (sf),
				     GTK_POLICY_AUTOMATIC,
				     GTK_POLICY_AUTOMATIC);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	canvas = GNOME_CANVAS (gnome_canvas_new ());
	gtk_container_add (GTK_CONTAINER (sf), GTK_WIDGET (canvas));
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_signal_connect (GTK_OBJECT (canvas), "realize",
			    GTK_SIGNAL_FUNC (realize),
			    NULL);

	image = g_new (Image, 1);
	image->buf = gdk_pixbuf_load_image (poptGetArg (ctx));
	g_assert (image->buf != NULL);
	poptFreeContext (ctx);

	image->r_lut = g_new (guchar, 256);
	image->g_lut = g_new (guchar, 256);
	image->b_lut = g_new (guchar, 256);

	for (i = 0; i < 256; i++) {
		image->r_lut[i] = i;
		image->g_lut[i] = i;
		image->b_lut[i] = i;
	}

	item = gnome_canvas_item_new (gnome_canvas_root (canvas),
				      image_item_get_type (),
				      "image", image,
				      "zoom", 1.0,
				      NULL);

	gnome_canvas_set_scroll_region (canvas,
					0.0,
					0.0,
					image->buf->art_pixbuf->width,
					image->buf->art_pixbuf->height);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1.0, 0.01, 5.0, 0.01, 0.01, 0.01));
	w = gtk_hscale_new (adj);
	gtk_scale_set_digits (GTK_SCALE (w), 2);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (zoom_changed),
			    item);

	w = create_sb_spacing (sf);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	w = create_shadow_type (sf);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	gtk_widget_show_all (window);

	gtk_main ();
	return 0;
}
