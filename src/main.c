/* Eye of Gnome image viewer - main module, startup code
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include "preferences.h"
#include "stock.h"
#include "util.h"
#include "window.h"


int
main (int argc, char **argv)
{
	poptContext ctx;
	GtkWidget *window;
	const char **args;
	gboolean opened;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table (PACKAGE, VERSION, argc, argv, NULL, 0, &ctx);
	gdk_rgb_init ();
	glade_gnome_init ();

	stock_init ();
	prefs_init ();

	args = poptGetArgs (ctx);

	opened = FALSE;

	if (args)
		for (; *args; args++) {
			window = window_new ();
			if (window_open_image (WINDOW (window), *args)) {
				gtk_widget_show_now (window);
				opened = TRUE;
			} else {
				open_failure_dialog (GTK_WINDOW (window), *args);
				gtk_widget_destroy (window);
			}
		}
	else {
		window = window_new ();
		gtk_widget_show (window);
		opened = TRUE;
	}

	poptFreeContext (ctx);

	if (opened) {
		gtk_main ();
		return 0;
	} else
		return 1;
}
