/* Eye of Gnome image viewer - miscellaneous utility functions
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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
#include <gtk/gtkmessagedialog.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "util.h"



/**
 * open_failure_dialog:
 * @parent: Parent window for the dialog.
 * @filename: URI that could not be loaded.
 * 
 * Displays a dialog to indicate failure when loading a file.
 **/
void
open_failure_dialog (GtkWindow *parent, const char *text_uri)
{
	GtkWidget *msg;
	char *unescaped;

	g_return_if_fail (text_uri != NULL);
	g_return_if_fail (!parent || GTK_IS_WINDOW (parent));

	unescaped = gnome_vfs_unescape_string_for_display (text_uri);

	msg = gtk_message_dialog_new (parent,
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_ERROR,
				      GTK_BUTTONS_OK,
				      _("Could not open `%s'"),
				      unescaped);
	g_free (unescaped);

	gtk_dialog_run (GTK_DIALOG (msg));

	gtk_widget_destroy (msg);
}
