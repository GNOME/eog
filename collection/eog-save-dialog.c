#include <string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include "eog-save-dialog.h"
#include "eog-image.h"

struct _EogSaveDialogPrivate {
	GtkWidget *label;
	GtkWidget *progress;
};

GNOME_CLASS_BOILERPLATE (EogSaveDialog, 
			 eog_save_dialog,
			 GtkDialog,
			 GTK_TYPE_DIALOG);

void
eog_save_dialog_class_init (EogSaveDialogClass *klass)
{
}

void 
eog_save_dialog_instance_init (EogSaveDialog *dlg)
{
	EogSaveDialogPrivate *priv;
	
	priv = g_new0 (EogSaveDialogPrivate, 1);
	
	dlg->priv = priv;
}


GtkWidget*  
eog_save_dialog_new (void)
{
	EogSaveDialogPrivate *priv;
	GtkWidget *dlg;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *vbox;
	
	dlg = gtk_widget_new (EOG_TYPE_SAVE_DIALOG, 
			      "border-width", 6, 
			      "resizable", FALSE,
			      "has-separator", FALSE,
			      "modal", FALSE,
			      NULL);
	priv = EOG_SAVE_DIALOG (dlg)->priv;
			      
	/* arrange containee elements */
	hbox = gtk_widget_new (GTK_TYPE_HBOX, 
			       "homogeneous", FALSE,
			       "spacing", 12,
			       "border-width", 6,
			       NULL);

	image = gtk_widget_new (GTK_TYPE_IMAGE,
				"stock", GTK_STOCK_SAVE_AS,
				"icon-size", GTK_ICON_SIZE_DIALOG,
				"yalign", 0.0,
				NULL);
	gtk_container_add (GTK_CONTAINER (hbox), image);

	vbox = gtk_widget_new (GTK_TYPE_VBOX,
			       "homogeneous", FALSE,
			       "spacing", 12,
			       "border-width", 6,
			       NULL);

	priv->label = gtk_widget_new (GTK_TYPE_LABEL,
				      "use-markup", TRUE, 
				      "wrap", TRUE,
				      "yalign", 0.0,
				      NULL); 
	gtk_container_add (GTK_CONTAINER (vbox), priv->label);

	priv->progress = gtk_widget_new (GTK_TYPE_PROGRESS_BAR, NULL);
	gtk_container_add (GTK_CONTAINER (vbox), priv->progress);		       
	
	gtk_container_add (GTK_CONTAINER (hbox), vbox);
	gtk_widget_show_all (hbox);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), hbox);
	g_object_set (G_OBJECT (GTK_DIALOG (dlg)->vbox), "spacing", 12);

	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	return dlg;
}

void
eog_save_dialog_update (EogSaveDialog *dlg, double fraction, const gchar *caption)
{
	EogSaveDialogPrivate *priv;
	int header_len;
	int message_len;
	int caption_len;
	char *header = _("Saving image");
	char *message;

	g_print ("update save dialog: %.2f, %s\n", fraction, caption);

	priv = dlg->priv;

	header_len = strlen (header);
	caption_len = strlen (caption);
	message_len = header_len + caption_len + 70;

	message = g_new0 (char, message_len);
	g_snprintf (message, message_len,
		    "<span weight=\"bold\" size=\"larger\">%s: %s...</span>", header, caption);
	
	gtk_label_set_markup (GTK_LABEL (priv->label), message);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress), fraction);

	g_free (message);
}



