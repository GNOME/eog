/*
 *  EogCanvasPixbuf is a descendant of the system GnomeCanvasPixbuf
 *  class. It's only purpose is to ignore the possible alpha values of
 *  an pixbuf when determining if a point is in the item or not. Then
 *  we can react also if the user clicks on transparent parts of the
 *  image.
 */

#ifndef _EOG_CANVAS_PIXBUF_H_
#define _EOG_CANVAS_PIXBUF_H_

#include <glib-object.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>

G_BEGIN_DECLS

#define EOG_TYPE_CANVAS_PIXBUF            (eog_canvas_pixbuf_get_type ())
#define EOG_CANVAS_PIXBUF(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_CANVAS_PIXBUF, EogCanvasPixbuf))
#define EOG_CANVAS_PIXBUF_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_CANVAS_PIXBUF, EogCanvasPixbufClass))
#define EOG_IS_CANVAS_PIXBUF(o)         G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_CANVAS_PIXBUF))
#define EOG_IS_CANVAS_PIXBUF_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_CANVAS_PIXBUF))
#define EOG_CANVAS_PIXBUF_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_CANVAS_PIXBUF, EogCanvasPixbufClass))

typedef struct _EogCanvasPixbuf EogCanvasPixbuf;
typedef struct _EogCanvasPixbufClass EogCanvasPixbufClass;

struct _EogCanvasPixbuf {
	GnomeCanvasPixbuf parent;
};

struct _EogCanvasPixbufClass {
	GnomeCanvasPixbufClass parent_klass;
};

GType               eog_canvas_pixbuf_get_type                       (void) G_GNUC_CONST;



G_END_DECLS

#endif /* _EOG_CANVAS_PIXBUF_H_ */
