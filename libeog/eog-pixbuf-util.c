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


