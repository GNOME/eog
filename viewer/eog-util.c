/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-util.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000 SuSE GmbH.
 */

#include <eog-util.h>

GSList *
eog_util_split_string (const gchar *string, const gchar *delimiter)
{
	const gchar *ptr = string;
	int pos = 0, escape = 0;
	char buffer [BUFSIZ];
	GSList *list = NULL;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);

	while (*ptr) {
		char c = *ptr++;
		const gchar *d;
		int found = 0;

		if (pos >= BUFSIZ) {
			g_warning ("string too long, aborting");
			return list;
		}

		if (escape) {
			buffer [pos++] = c;
			escape = 0;
			continue;
		}

		if (c == '\\') {
			escape = 1;
			continue;
		}

		for (d = delimiter; *d; d++) {
			if (c == *d) {
				buffer [pos++] = 0;
				list = g_slist_prepend (list, g_strdup (buffer));
				pos = 0; found = 1;
				break;
			}
		}

		if (!found)
			buffer [pos++] = c;
	}

	buffer [pos++] = 0;
	list = g_slist_prepend (list, g_strdup (buffer));

	return list;
}

void
eog_util_load_print_settings (GConfClient *client,
			      gchar **paper_size, gdouble *top, 
			      gdouble *bottom, gdouble *left, gdouble *right, 
			      gboolean *landscape, gboolean *cut, 
			      gboolean *horizontally, gboolean *vertically, 
			      gboolean *down_right, gboolean *fit_to_page, 
			      gint *adjust_to, gint *unit, gdouble *overlap_x,
			      gdouble *overlap_y, gboolean *overlap)
{

	*paper_size = gconf_client_get_string (client, 
				"/apps/eog/viewer/paper_size", NULL);
	*top = gconf_client_get_float (client, "/apps/eog/viewer/top", NULL); 
	*bottom = gconf_client_get_float (client, 
				"/apps/eog/viewer/bottom", NULL); 
	*left = gconf_client_get_float (client, "/apps/eog/viewer/left", NULL); 
	*right = gconf_client_get_float (client, 
				"/apps/eog/viewer/right", NULL); 
	*overlap_x = gconf_client_get_float (client,
				"/apps/eog/viewer/overlap_x", NULL);
	*overlap_y = gconf_client_get_float (client,
				"/apps/eog/viewer/overlap_y", NULL);
	*overlap = gconf_client_get_bool (client,
				"/apps/eog/viewer/overlap", NULL);
	*landscape = gconf_client_get_bool (client, 
				"/apps/eog/viewer/landscape", NULL);
	*cut = gconf_client_get_bool (client, "/apps/eog/viewer/cut", NULL); 
	*horizontally = gconf_client_get_bool (client, 
				"/apps/eog/viewer/horizontally", NULL);
	*vertically = gconf_client_get_bool (client, 
				"/apps/eog/viewer/vertically", NULL); 
	*down_right = gconf_client_get_bool (client, 
				"/apps/eog/viewer/down_right", NULL); 
	*fit_to_page = gconf_client_get_bool (client, 
				"/apps/eog/viewer/fit_to_page", NULL); 
	*adjust_to = gconf_client_get_int (client, 
				"/apps/eog/viewer/adjust_to", NULL); 
	*unit = gconf_client_get_int (client, "/apps/eog/viewer/unit", NULL);

	/* First time users */
	if (*adjust_to == 0)
		*adjust_to = 100;
	if (*paper_size == NULL)
		*paper_size = g_strdup (gnome_paper_name_default ());
}

void
eog_util_paper_size (const gchar *paper_size, gboolean landscape, 
		     gdouble *width, gdouble *height)
{
	const GnomePaper *paper;

	paper = gnome_paper_with_name (paper_size);
	if (landscape) {
		*width = gnome_paper_psheight (paper); 
		*height = gnome_paper_pswidth (paper); 
	} else { 
		*width = gnome_paper_pswidth (paper); 
		*height = gnome_paper_psheight (paper);
	}
} 
