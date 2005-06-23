#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "eog-pixbuf-util.h"

GSList*
eog_pixbuf_get_savable_formats (void)
{
	GSList *list;
	GSList *write_list = NULL;
	GSList *it;

	list = gdk_pixbuf_get_formats ();
	
	for (it = list; it != NULL; it = it->next) {
		GdkPixbufFormat *format;

		format = (GdkPixbufFormat*) it->data;
		if (gdk_pixbuf_format_is_writable (format)) {
			write_list = g_slist_prepend (write_list, format);
		}
	}

	g_slist_free (list);
	write_list = g_slist_reverse (write_list);

	return write_list;
}

GdkPixbufFormat* 
eog_pixbuf_get_format_by_suffix (const char *suffix)
{
	GSList *list;
	GSList *it;
	GdkPixbufFormat *result = NULL;

	g_return_val_if_fail (suffix != NULL, NULL);

	list = gdk_pixbuf_get_formats ();
	
	for (it = list; (it != NULL) && (result == NULL); it = it->next) {
		GdkPixbufFormat *format;
		gchar **extensions;
		int i;

		format = (GdkPixbufFormat*) it->data;
		
		extensions = gdk_pixbuf_format_get_extensions (format);
		for (i = 0; extensions[i] != NULL; i++) {
			/* g_print ("check extension: %s against %s\n", extensions[i], suffix); */
			if (g_ascii_strcasecmp (suffix, extensions[i]) == 0) {
				result = format;
				break;
			}
		}

		g_strfreev (extensions);
	}

	g_slist_free (list);

	return result;	
}

char*
eog_pixbuf_get_common_suffix (GdkPixbufFormat *format)
{
	char **extensions;
	int i;
	char *result = NULL;

	if (format == NULL) return NULL;

	extensions = gdk_pixbuf_format_get_extensions (format);
	if (extensions[0] == NULL) return NULL;

	/* try to find 3-char suffix first, use the last occurence */
	for (i = 0; extensions [i] != NULL; i++) {
		if (strlen (extensions[i]) <= 3) {
			if (result != NULL) 
				g_free (result);
			result = g_ascii_strdown (extensions[i], -1);
		}
	}

	/* otherwise take the first one */
	if (result == NULL) {
		result = g_ascii_strdown (extensions[0], -1);
	}

	g_strfreev (extensions);
	      
	return result;
}

GdkPixbufFormat* 
eog_pixbuf_get_format_by_vfs_uri (GnomeVFSURI *uri)
{
	char *suffix;
	char *short_name;
	char *suffix_start;
	guint len;
	GdkPixbufFormat *format;

	g_return_val_if_fail (uri != NULL, NULL);

	/* get unescaped string */
	short_name = gnome_vfs_uri_extract_short_name (uri); 
	
	/* FIXME: does this work for all locales? */
	suffix_start = g_utf8_strrchr (short_name, -1, '.'); 
	
	if (suffix_start == NULL) 
		return NULL;
	
	len = MAX (0, strlen (suffix_start) - 1);
	suffix = g_strndup (suffix_start+1, len);
	
	format = eog_pixbuf_get_format_by_suffix (suffix);
	
	g_free (short_name);
	g_free (suffix);

	return format;

}

GdkPixbufFormat* 
eog_pixbuf_get_format_by_uri (const char *txt_uri)
{
	GnomeVFSURI *uri;
	GdkPixbufFormat *format;

	g_return_val_if_fail (txt_uri != NULL, NULL);

	uri = gnome_vfs_uri_new (txt_uri);

	format = eog_pixbuf_get_format_by_vfs_uri (uri);

	gnome_vfs_uri_unref (uri);

	return format;
}

