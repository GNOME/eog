#include <gdk-pixbuf/gdk-pixbuf.h>

void       eog_pixbuf_flip_horizontal (GdkPixbuf *pixbuf);

void       eog_pixbuf_flip_vertical   (GdkPixbuf *pixbuf);

GdkPixbuf* eog_pixbuf_rotate_90_cw    (GdkPixbuf *pixbuf);
GdkPixbuf* eog_pixbuf_rotate_90_ccw   (GdkPixbuf *pixbuf);
void       eog_pixbuf_rotate_180      (GdkPixbuf *pixbuf);


