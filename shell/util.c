/* Eye of Gnome image viewer - miscellaneous utility functions
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
#include "util.h"



void
open_failure_dialog (GtkWindow *parent, const char *filename)
{
	GtkWidget *msg;
	char *text;

	if (parent)
		g_return_if_fail (GTK_IS_WINDOW (parent));

	g_return_if_fail (filename != NULL);

	text = g_strdup_printf (_("Could not open `%s'"), filename);
	msg = gnome_message_box_new (text, GNOME_MESSAGE_BOX_ERROR,
				     GNOME_STOCK_BUTTON_OK,
				     NULL);
	g_free (text);

	if (parent)
		gnome_dialog_set_parent (GNOME_DIALOG (msg), parent);

	gnome_dialog_run (GNOME_DIALOG (msg));
}
