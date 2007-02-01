/* Eye Of Gnome - General Utilities 
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 *	- Jens Finke <jens@gnome.org>
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

#ifndef __EOG_UTIL_H__
#define __EOG_UTIL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

void    eog_util_show_help                  (const gchar *section, 
					     GtkWindow   *parent);

char*   eog_util_make_valid_utf8            (const char  *name);

GSList* eog_util_string_list_to_uri_list    (GSList *string_list);

#ifdef HAVE_DBUS
GSList* eog_util_strings_to_uri_list        (gchar **strings);
#endif

GSList* eog_util_string_array_to_list       (const gchar **files,
					     gboolean create_uri);

gchar** eog_util_string_array_make_absolute (gchar **files);

G_END_DECLS

#endif /* __EOG_UTIL_H__ */
