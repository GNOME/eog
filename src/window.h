#ifndef WINDOW_H
#define WINDOW_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-app.h>

BEGIN_GNOME_DECLS



#define TYPE_WINDOW            (window_get_type ())
#define WINDOW(obj)            (GTK_CHECK_CAST ((obj), TYPE_WINDOW, Window))
#define WINDOW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_WINDOW, WindowClass))
#define IS_WINDOW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_WINDOW))
#define IS_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_WINDOW))


typedef struct _Window Window;
typedef struct _WindowClass WindowClass;


struct _Window {
	GnomeApp app;

	/* Private data */
	gpointer priv;
};

struct _WindowClass {
	GnomeAppClass parent_class;
};


GtkType window_get_type (void);



END_GNOME_DECLS

#endif
