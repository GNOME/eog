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
