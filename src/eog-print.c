/* Eye of Gnome - Print Operations
 *
 * Copyright (C) 2005-2008 The Free Software Foundation
 *
 * Author: Claudio Saavedra <csaavedra@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "eog-print.h"
#include "eog-print-image-setup.h"
#include "eog-debug.h"

typedef struct {
	EogImage *image;
	gdouble left_margin;
	gdouble top_margin;
	gdouble scale_factor;
	GtkUnit unit;
} EogPrintData;

static void
eog_print_draw_page (GtkPrintOperation *operation,
		     GtkPrintContext   *context,
		     gint               page_nr,
		     gpointer           user_data) 
{
	cairo_t *cr;
	gdouble dpi_x, dpi_y;
	gdouble x0, y0;
	gdouble scale_factor;
	gdouble p_width, p_height;
	gint width, height;
	GdkPixbuf *pixbuf;
	EogPrintData *data;
	GtkPageSetup *page_setup;
	
	eog_debug (DEBUG_PRINTING);
	
	data = (EogPrintData *) user_data;

	scale_factor = data->scale_factor/100;
	pixbuf = eog_image_get_pixbuf (data->image);

	dpi_x = gtk_print_context_get_dpi_x (context);
	dpi_y = gtk_print_context_get_dpi_y (context);
	
	switch (data->unit) {
	case GTK_UNIT_INCH:
		x0 = data->left_margin * dpi_x;
		y0 = data->top_margin  * dpi_y;
		break;
	case GTK_UNIT_MM:
		x0 = data->left_margin * dpi_x/25.4;
		y0 = data->top_margin  * dpi_y/25.4;
		break;
	default:
		g_assert_not_reached ();
	}

	cr = gtk_print_context_get_cairo_context (context);

	cairo_translate (cr, x0, y0);

	page_setup = gtk_print_context_get_page_setup (context);
	p_width =  gtk_page_setup_get_page_width (page_setup, GTK_UNIT_POINTS);
	p_height = gtk_page_setup_get_page_height (page_setup, GTK_UNIT_POINTS);

	width  = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* this is both a workaround for a bug in cairo's PDF backend, and
	   a way to ensure we are not printing outside the page margins */
	cairo_rectangle (cr, 0, 0, MIN (width*scale_factor, p_width), MIN (height*scale_factor, p_height));
	cairo_clip (cr);

	cairo_scale (cr, scale_factor, scale_factor);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
 	cairo_paint (cr);

	g_object_unref (pixbuf);
}

static GObject *
eog_print_create_custom_widget (GtkPrintOperation *operation, 
				       gpointer user_data)
{
	GtkPageSetup *page_setup;
	EogPrintData *data;
	
	eog_debug (DEBUG_PRINTING);
	
	data = (EogPrintData *)user_data;
	
	page_setup = gtk_print_operation_get_default_page_setup (operation);
	
	g_assert (page_setup != NULL);
	
	return G_OBJECT (eog_print_image_setup_new (data->image, page_setup));
}

static void
eog_print_custom_widget_apply (GtkPrintOperation *operation,
			       GtkWidget         *widget,
			       gpointer           user_data)
{
	EogPrintData *data;
	gdouble left_margin, top_margin, scale_factor;
	GtkUnit unit;
	
	eog_debug (DEBUG_PRINTING);
	
	data = (EogPrintData *)user_data;
	
	eog_print_image_setup_get_options (EOG_PRINT_IMAGE_SETUP (widget), 
					   &left_margin, &top_margin, 
					   &scale_factor, &unit);
	
	data->left_margin = left_margin;
	data->top_margin = top_margin;
	data->scale_factor = scale_factor;
	data->unit = unit;
}

static void
eog_print_end_print (GtkPrintOperation *operation,
		     GtkPrintContext   *context,
		     gpointer           user_data)
{
	EogPrintData *data = (EogPrintData*) user_data;

	eog_debug (DEBUG_PRINTING);
	
	g_object_unref (data->image);
	g_free (data);
}

GtkPrintOperation *
eog_print_operation_new (EogImage *image,
			 GtkPrintSettings *print_settings,
			 GtkPageSetup *page_setup)
{
	GtkPrintOperation *print;
	EogPrintData *data;

	eog_debug (DEBUG_PRINTING);

	print = gtk_print_operation_new ();

	data = g_new0 (EogPrintData, 1);

	data->left_margin = 0;
	data->top_margin = 0;
	data->scale_factor = 100;
	data->image = g_object_ref (image);
	data->unit = GTK_UNIT_INCH;

	gtk_print_operation_set_print_settings (print, print_settings);
	gtk_print_operation_set_default_page_setup (print, 
						    page_setup);
	gtk_print_operation_set_n_pages (print, 1);
	gtk_print_operation_set_job_name (print,
					  eog_image_get_caption (image));

	g_signal_connect (print, "draw_page", 
			  G_CALLBACK (eog_print_draw_page), 
			  data);
	g_signal_connect (print, "create-custom-widget", 
			  G_CALLBACK (eog_print_create_custom_widget),
			  data);
	g_signal_connect (print, "custom-widget-apply", 
			  G_CALLBACK (eog_print_custom_widget_apply), 
			  data);
	g_signal_connect (print, "end-print", 
			  G_CALLBACK (eog_print_end_print),
			  data);

	gtk_print_operation_set_custom_tab_label (print, _("Image Settings"));

	return print;
}
