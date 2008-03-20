/* Eye Of Gnome - EXIF Utilities 
 *
 * Copyright (C) 2006-2007 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 * Author: Claudio Saavedra <csaavedra@alumnos.utalca.cl>
 * Author: Felix Riemann <felix@hsgheli.de>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STRPTIME
#define _XOPEN_SOURCE
#endif
#include <time.h>

#include "eog-exif-util.h"

#include <string.h>
#include <glib/gi18n.h>

#ifdef HAVE_STRPTIME
static gchar *  
eog_exif_util_format_date_with_strptime (const gchar *date)
{
	gchar *new_date = NULL;
	gchar tmp_date[100];
	gchar *p;
	gsize dlen;
	struct tm tm;
	
	memset (&tm, '\0', sizeof (tm));
	p = strptime (date, "%Y:%m:%d %T", &tm);

	if (p == date + strlen (date)) {
		dlen = strftime (tmp_date, 100 * sizeof(gchar), "%x %X", &tm);
		new_date = g_strndup (tmp_date, dlen);
	}

	return new_date;
}
#else
static gchar *
eog_exif_util_format_date_by_hand (const gchar *date)
{
	int year, month, day, hour, minutes, seconds;
	int result;
	gchar *new_date = NULL;
	
	result = sscanf (date, "%d:%d:%d %d:%d:%d",
			 &year, &month, &day, &hour, &minutes, &seconds);

	if (result < 3 || !g_date_valid_dmy (day, month, year)) {
		return NULL;
	} else {
		gchar tmp_date[100];
		gsize dlen;
		time_t secs;
		struct tm tm;
		
		memset (&tm, '\0', sizeof (tm));
		tm.tm_mday = day;
		tm.tm_mon = month-1;
		tm.tm_year = year-1900;
		
		if (result < 5) {
			dlen = strftime (tmp_date, 100 * sizeof(gchar), "%x", &tm);
		} else {
			tm.tm_sec = result < 6 ? 0 : seconds;
			tm.tm_min = minutes;
			tm.tm_hour = hour;
			dlen = strftime (tmp_date, 100 * sizeof(gchar), "%x %X", &tm);
		}

		if (dlen == 0) 
			return NULL;
		else
			new_date = g_strndup (tmp_date, dlen);
	}
	return new_date;
}
#endif /* HAVE_STRPTIME */

gchar *  
eog_exif_util_format_date (const gchar *date)
{
	gchar *new_date;
#ifdef HAVE_STRPTIME
	new_date = eog_exif_util_format_date_with_strptime (date);
#else
	new_date = eog_exif_util_format_date_by_hand (date);
#endif /* HAVE_STRPTIME */
	return new_date;
}

const gchar * 
eog_exif_util_get_value (ExifData *exif_data, gint tag_id, gchar *buffer, guint buf_size)
{
	ExifEntry *exif_entry;
	const gchar *exif_value;

        exif_entry = exif_data_get_entry (exif_data, tag_id);

	buffer[0] = 0;

	exif_value = exif_entry_get_value (exif_entry, buffer, buf_size);

	return exif_value;
}
