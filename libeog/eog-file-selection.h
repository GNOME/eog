#ifndef _EOG_FILE_SELECTION_H_
#define _EOG_FILE_SELECTION_H_

#include <gtk/gtkfilesel.h>

G_BEGIN_DECLS
 
#define EOG_TYPE_FILE_SELECTION          (eog_file_selection_get_type ())
#define EOG_FILE_SELECTION(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_FILE_SELECTION, EogFileSelection))
#define EOG_FILE_SELECTION_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_FILE_SELECTION, EogFileSelectionClass))

#define EOG_IS_FILE_SELECTION(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_FILE_SELECTION))
#define EOG_IS_FILE_SELECTION_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_FILE_SELECTION))
#define EOG_FILE_SELECTION_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_FILE_SELECTION, EogFileSelectionClass))

typedef struct _EogFileSelection         EogFileSelection;
typedef struct _EogFileSelectionClass    EogFileSelectionClass;
typedef struct _EogFileSelectionPrivate  EogFileSelectionPrivate;

typedef enum {
	EOG_FILE_SELECTION_LOAD,
	EOG_FILE_SELECTION_SAVE
} EogFileSelectionType;

struct _EogFileSelection {
	GtkFileSelection  parent_object;

	EogFileSelectionPrivate *priv;
};


struct _EogFileSelectionClass {
	GtkFileSelectionClass  parent_object;
};


GtkType    eog_file_selection_get_type            (void) G_GNUC_CONST;

GtkWidget* eog_file_selection_new (EogFileSelectionType type);

G_END_DECLS

#endif /* _EOG_FILE_SELECTION_H_ */
