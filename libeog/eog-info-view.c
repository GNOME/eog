#include <config.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "eog-info-view.h"
#include "eog-info-view-file.h"
#include "eog-info-view-exif.h"

enum {
	SIGNAL_ID_SIZE_PREPARED,
	SIGNAL_ID_IMAGE_FINISHED,
	SIGNAL_ID_CHANGED,
	SIGNAL_ID_LAST
};


struct _EogInfoViewPrivate
{
	EogImage *image;
	int image_signal_ids [SIGNAL_ID_LAST];

	GtkWidget *file_view;
	GtkWidget *exif_view;
#if 0
	GtkWidget *iptc_view;
#endif
	
};

GNOME_CLASS_BOILERPLATE (EogInfoView, eog_info_view,
			 GtkNotebook, GTK_TYPE_NOTEBOOK);


static void
eog_info_view_dispose (GObject *object)
{
	EogInfoView *view;
	EogInfoViewPrivate *priv;

	view = EOG_INFO_VIEW (object);
	priv = view->priv;

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}
}

static void
eog_info_view_finalize (GObject *object)
{
	EogInfoView *view;

	view = EOG_INFO_VIEW (object);

	g_free (view->priv);

	view->priv = NULL;
}

static void
eog_info_view_class_init (EogInfoViewClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->dispose = eog_info_view_dispose;
	object_class->finalize = eog_info_view_finalize;
}

static void
eog_info_view_instance_init (EogInfoView *view)
{
	EogInfoViewPrivate *priv;
	GtkWidget *scrwnd;
	int s;

	priv = g_new0 (EogInfoViewPrivate, 1);

	view->priv = priv;
	priv->image = NULL;
	for (s = 0; s < SIGNAL_ID_LAST; s++) {
		priv->image_signal_ids [s] = 0;
	} 

	priv->file_view = g_object_new (EOG_TYPE_INFO_VIEW_FILE, NULL);
	scrwnd = g_object_new (GTK_TYPE_SCROLLED_WINDOW, "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
			       "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
	gtk_container_add (GTK_CONTAINER (scrwnd), priv->file_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (view), scrwnd, gtk_label_new (_("File")));

#if HAVE_EXIF
	priv->exif_view = g_object_new (EOG_TYPE_INFO_VIEW_EXIF, NULL);
	scrwnd = g_object_new (GTK_TYPE_SCROLLED_WINDOW, 
			       "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
			       "vscrollbar-policy", GTK_POLICY_AUTOMATIC, 
			       NULL);
	gtk_container_add (GTK_CONTAINER (scrwnd), priv->exif_view);
	gtk_notebook_insert_page (GTK_NOTEBOOK (view), scrwnd, gtk_label_new (_("EXIF")), 1);
#else
	priv->exif_view = NULL;
#endif

#if 0
	priv->iptc_view = g_object_new (EOG_TYPE_INFO_VIEW_IPTC, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (view), priv->iptc_view, gtk_label_new (_("IPTC")));
#endif
}

/* loading_size_prepared_cb
 *
 * This function is always called, when the image dimension is known. Therefore
 * we add the image size and width to the info view list here.
 */
static void
loading_size_prepared_cb (EogImage *image, gint width, gint height, gpointer data)
{
	EogInfoView *view;

	g_return_if_fail (EOG_IS_IMAGE (image));

	view = EOG_INFO_VIEW (data);

	eog_info_view_file_show_data (EOG_INFO_VIEW_FILE (view->priv->file_view), image);
}

/* loading_finished_cb
 * 
 * This function is called if EXIF data has been read by EogImage.
 */
static void
loading_finished_cb (EogImage *image, gpointer data)
{
	EogInfoView *view;

	g_return_if_fail (EOG_IS_IMAGE (image));

	view = EOG_INFO_VIEW (data);

	eog_info_view_file_show_data (EOG_INFO_VIEW_FILE (view->priv->file_view), image);
#if HAVE_EXIF
	{
		ExifData *ed;

		ed = eog_image_get_exif_information (image);
		eog_info_view_exif_show_data (EOG_INFO_VIEW_EXIF (view->priv->exif_view), ed);
		if (ed != NULL) {
			gtk_widget_show_all (gtk_widget_get_parent (GTK_WIDGET (view->priv->exif_view)));
			exif_data_unref (ed);
		}
		else {
			gtk_widget_hide_all (gtk_widget_get_parent (GTK_WIDGET (view->priv->exif_view)));
		}
	}
#endif
}


/* image_changed_cb
 *
 * This function is called every time the image changed somehow. We
 * will clear everything and rewrite all attributes.
 */
static void
image_changed_cb (EogImage *image, gpointer data)
{
	EogInfoView *view;

	g_return_if_fail (EOG_IS_IMAGE (image));

	view = EOG_INFO_VIEW (data);

	eog_info_view_file_show_data (EOG_INFO_VIEW_FILE (view->priv->file_view), image);
#if HAVE_EXIF
	{
		ExifData *ed;

		ed = (ExifData*) eog_image_get_exif_information (image);
		eog_info_view_exif_show_data (EOG_INFO_VIEW_EXIF (view->priv->exif_view), ed);
		if (ed != NULL) {
			exif_data_unref (ed);
		}
	}
#endif

}

void
eog_info_view_set_image (EogInfoView *view, EogImage *image)
{
	EogInfoViewPrivate *priv;
	int s;

	g_return_if_fail (EOG_IS_INFO_VIEW (view));

	priv = view->priv;

	if (priv->image == image)
		return;

	if (priv->image != NULL) {
		for (s = 0; s < SIGNAL_ID_LAST; s++) {
			if (priv->image_signal_ids [s] > 0) {
				g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_signal_ids [s]);
				priv->image_signal_ids [s] = 0;
			}
		}

		g_object_unref (priv->image);
		priv->image = NULL;
	}

	eog_info_view_file_show_data (EOG_INFO_VIEW_FILE (priv->file_view), image);

	if (image != NULL) {
	g_object_ref (image);
	priv->image = image;

	/* prepare additional image information callbacks */
		priv->image_signal_ids [SIGNAL_ID_IMAGE_FINISHED] = 
			g_signal_connect (G_OBJECT (priv->image), "loading_finished", 
					  G_CALLBACK (loading_finished_cb), view);

	priv->image_signal_ids [SIGNAL_ID_SIZE_PREPARED] = 
		g_signal_connect (G_OBJECT (priv->image), "loading_size_prepared", 
				  G_CALLBACK (loading_size_prepared_cb), view);

	priv->image_signal_ids [SIGNAL_ID_CHANGED] = 
		g_signal_connect (G_OBJECT (priv->image), "image_changed", 
				  G_CALLBACK (image_changed_cb), view);

	/* start loading */
	eog_image_load (priv->image, EOG_IMAGE_LOAD_DEFAULT);
	}
}
