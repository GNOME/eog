#include "gpi-dialog-pixbuf.h"

int
main (int argc, char **argv)
{
	GdkPixbuf *p;
	GPIDialogPixbuf *d;

	gtk_init (&argc, &argv);

	p = gdk_pixbuf_new_from_file (TOP_SRCDIR "/art/down-right.png", NULL);
	d = gpi_dialog_pixbuf_new (p);
	g_object_unref (p);
	gtk_widget_show (GTK_WIDGET (d));
	gtk_main ();

	return 0;
}
