#include "ui-image.h"


static void ui_image_class_init (UIImageClass *class);
static void ui_image_init (UIImage *ui);
static void ui_image_destroy (GtkObject *object);

static GtkTableClass *parent_class;

/**
 * ui_image_get_type:
 * @void:
 *
 * Registers the &UIImage class if necessary, and returns the type ID associated
 * to it.
 *
 * Return value: the type ID of the &UIImage class.
 **/
GtkType
ui_image_get_type (void)
{
	static GtkType ui_image_type = 0;

	if (!ui_image_type) {
		static const ui_image_info = {
			"UIImage",
			sizeof (UIImage),
			sizeof (UIImageClass),
			(GtkClassInitFunc) ui_image_class_init,
			(GtkObjectInitfunc) ui_image_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ui_image_type = gtk_type_unique (gtk_table_get_type (), &ui_image_info);
	}

	return ui_image_type;
}

/* Class initialization function for an image's user interface */
static void
ui_image_class_init (UIImageClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gtk_table_get_type ());

	object_class->destroy = ui_image_destroy;
}

/* Object initialization function for an image's user interface */
static void
ui_image_init (UIImage *ui)
{
	GtkTable *table;

	table = GTK_TABLE (ui);
	gtk_table_resize (table, 2, 2);

	/* Create the image area */
	ui->view = view_new ();
	gtk_table_attach (table, ui->view,
			  0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (ui->view);

	/* Create the adjustments and scrollbars */

	ui->vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	ui->hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	ui->vsb = gtk_vscrollbar_new (ui->vadj);
	ui->hsb = gtk_hscrollbar_new (ui->hadj);

	gtk_table_attach (table, ui->vsb,
			  1, 2, 0, 1,
			  0,
			  GTK_EXPAND | GTK_FILL,
			  0, 0);
	gtk_table_attach (table, ui->hsb,
			  0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL,
			  0,
			  0, 0);
}

/* Destroy handler for an image's user interface */
static void
ui_image_destroy (GtkObject *object)
{
	UIImage *ui;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_WINDOW (object));

	ui = UI_IMAGE (object);

	/* FIXME */

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
