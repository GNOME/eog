#ifndef _EOG_PRINT_SETUP_H_
#define _EOG_PRINT_SETUP_H_

#include <gnome.h>
#include <eog-image-view.h>

BEGIN_GNOME_DECLS

#define EOG_TYPE_PRINT_SETUP		(eog_print_setup_get_type ())
#define EOG_PRINT_SETUP(obj)		(GTK_CHECK_CAST ((obj), EOG_TYPE_PRINT_SETUP, EogPrintSetup))
#define EOG_PRINT_SETUP_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_PRINT_SETUP, EogPrintSetupClass))
#define EOG_IS_PRINT_SETUP(obj)		(GTK_CHECK_TYPE ((obj), EogPrintSetup))
#define EOG_IS_PRINT_SETUP_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_PRINT_SETUP))

typedef struct _EogPrintSetup		EogPrintSetup;
typedef struct _EogPrintSetupPrivate	EogPrintSetupPrivate;
typedef struct _EogPrintSetupClass	EogPrintSetupClass;

struct _EogPrintSetup 
{
	GnomeDialog		 dialog;

	EogPrintSetupPrivate	*priv;
};

struct _EogPrintSetupClass
{
	GnomeDialogClass	 parent_class;
};

GtkType		eog_print_setup_get_type (void);

GtkWidget*	eog_print_setup_new	 (EogImageView *image_view);

END_GNOME_DECLS

#endif /* _EOG_PRINT_SETUP_H_ */

