#include <config.h>
#include <glib/gi18n.h>

#include "eog-info-view.h"
#include "eog-info-view-file.h"
#include "eog-info-view-exif.h"

struct _EogInfoViewPrivate
{
	EogImage *image;
	int image_changed_signal_id;

	GtkWidget *file_view;
	GtkWidget *exif_view;
#if 0
	GtkWidget *iptc_view;
#endif
	
};

G_DEFINE_TYPE (EogInfoView, eog_info_view, GTK_TYPE_NOTEBOOK)


static void
eog_info_view_dispose (GObject *object)
{
	EogInfoView *view;
	EogInfoViewPrivate *priv;

	view = EOG_INFO_VIEW (object);
	priv = view->priv;

	if (priv->image) {
		eog_image_data_unref (priv->image);
		priv->image = NULL;
	}

	G_OBJECT_CLASS (eog_info_view_parent_class)->dispose (object);
}

static void
eog_info_view_finalize (GObject *object)
{
	EogInfoView *view;

	view = EOG_INFO_VIEW (object);

	g_free (view->priv);

	view->priv = NULL;

	G_OBJECT_CLASS (eog_info_view_parent_class)->finalize (object);
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
eog_info_view_init (EogInfoView *view)
{
	EogInfoViewPrivate *priv;
	GtkWidget *scrwnd;
	GtkRequisition requisition;

	priv = g_new0 (EogInfoViewPrivate, 1);

	view->priv = priv;
	priv->image = NULL;
	priv->image_changed_signal_id = 0;

	priv->file_view = g_object_new (EOG_TYPE_INFO_VIEW_FILE, NULL);
	scrwnd = g_object_new (GTK_TYPE_SCROLLED_WINDOW, "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
			       "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
	gtk_container_add (GTK_CONTAINER (scrwnd), priv->file_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (view), scrwnd, gtk_label_new (_("File")));
	gtk_widget_size_request (priv->file_view, &requisition);
	gtk_widget_set_size_request (GTK_WIDGET (view), 
				     requisition.width * 3 / 2, -1);

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

static void
update_data_pages_for_image (EogInfoView *view, EogImage *image)
{
	g_return_if_fail (EOG_IS_INFO_VIEW (view));
	g_return_if_fail (EOG_IS_IMAGE (image));
	
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
	g_return_if_fail (EOG_IS_IMAGE (image));

	update_data_pages_for_image (EOG_INFO_VIEW (data), image);
}

void
eog_info_view_set_image (EogInfoView *view, EogImage *image)
{
	EogInfoViewPrivate *priv;

	g_return_if_fail (EOG_IS_INFO_VIEW (view));

	priv = view->priv;

	if (priv->image == image)
		return;

	/* clean up current data */
	if (priv->image != NULL) {
		if (priv->image_changed_signal_id > 0) {
			g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_changed_signal_id);
			priv->image_changed_signal_id = 0;
		}

		eog_image_data_unref (priv->image);
		priv->image = NULL;
	}

	if (image != NULL) {
		eog_image_data_ref (image);
		priv->image = image;

		priv->image_changed_signal_id = 
			g_signal_connect (G_OBJECT (priv->image), "image_changed", 
					  G_CALLBACK (image_changed_cb), view);

	}

	update_data_pages_for_image (view, image);
}
