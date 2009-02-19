/* gcc -o  eog-thumb-view test-eog-tb.c eog-thumb-view.c `pkg-config --cflags --libs gtk+-2.0 gio-2.0` */

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "eog-thumbnail.h"
#include "eog-image.h"
#include "eog-thumb-view.h"
#include "eog-list-store.h"
#include "eog-job-queue.h"

static void
tb_on_selection_changed (GtkIconView *thumb_view,
			 gpointer data)
{
	GList *list, *item;
	EogImage *image;

	g_print ("selection changed: %i\n",
		 eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (thumb_view)));

	list = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (thumb_view));
	if (list) {
		g_print ("selected images at: ");
		for (item = list; item != NULL; item = item->next) {
			image = EOG_IMAGE (item->data);
			g_print ("%s ", eog_image_get_caption (image));
/*			g_free (item->data); */
		}
		g_print ("\n");

		g_list_free (list);
	}
	else {
		g_print ("no images selected\n");
	}
}

static void
on_button_sfi_clicked (GtkButton *button,
		       gpointer data)
{
	EogThumbView *tb = EOG_THUMB_VIEW (data);
	GtkTreeIter iter;
	EogImage *image;
	GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW (tb));
	gtk_tree_model_get_iter_first (model, &iter);

	gtk_tree_model_get (model, &iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);

	eog_thumb_view_set_current_image (tb, image, TRUE);

	g_object_unref (G_OBJECT (image));
}

static void
on_button_next_clicked (GtkButton *button,
		       gpointer data)
{
	EogThumbView *tb = EOG_THUMB_VIEW (data);

	eog_thumb_view_select_single (tb, EOG_THUMB_VIEW_SELECT_RIGHT);
}

static void
on_button_back_clicked (GtkButton *button,
			gpointer data)
{
	EogThumbView *tb = EOG_THUMB_VIEW (data);

	eog_thumb_view_select_single (tb, EOG_THUMB_VIEW_SELECT_LEFT);
}

static void
on_button_first_clicked (GtkButton *button,
			gpointer data)
{
	EogThumbView *tb = EOG_THUMB_VIEW (data);

	eog_thumb_view_select_single (tb, EOG_THUMB_VIEW_SELECT_FIRST);
}

static void
on_button_last_clicked (GtkButton *button,
			gpointer data)
{
	EogThumbView *tb = EOG_THUMB_VIEW (data);

	eog_thumb_view_select_single (tb, EOG_THUMB_VIEW_SELECT_LAST);
}


static GFile*
make_file (const char *path)
{
	GFile *file = NULL;

	file = g_file_new_for_commandline_arg (path);

	return file;
}

static GList*
string_array_to_list (gchar **files, gint n_files)
{
	gint i;
	GList *list = NULL;

	if (files == NULL || n_files == 0) {
		return list;
	}

	for (i = 0; i < n_files; i++) {
		list = g_list_prepend (list, make_file (files [i]));
		g_print ("%s\n", files[i]);
	}
	return g_list_reverse (list);
}

static void
do_something_with_test_1 (void)
{
	g_print ("do something with test 1\n");
}

static const gchar *ui = "<ui>\n"
	"  <popup name=\"ThumbnailPopup\" action=\"PopupAction\">\n"
	"      <menuitem name=\"ItemName\" action=\"ItemAction\" />\n"
	"  </popup>\n"
	"</ui>\n\0";

static const GtkActionEntry entries[] = {
	{ "PopupAction", NULL, ""},
	{ "ItemAction", GTK_STOCK_OPEN, "Test 1", NULL, NULL, G_CALLBACK (do_something_with_test_1) },
};

int
main (gint argc, gchar **argv)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkActionGroup *action_group;
	GList *list_uris;
	GtkUIManager *ui_manager;
	GtkWidget *popup;
	GError *error = NULL;

	GtkWidget *scrolled_window;
	GtkWidget *thumb_view;
	GtkListStore *model;

	gtk_init (&argc, &argv);
	eog_thumbnail_init ();
	eog_job_queue_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (window),
			  "delete-event",
			  G_CALLBACK (gtk_main_quit),
			  NULL);
	gtk_window_set_default_size (GTK_WINDOW (window),
				     640, 480);

	thumb_view = eog_thumb_view_new ();
	model = eog_list_store_new ();

	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), thumb_view);

	ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error)) {
		g_warning ("Couldn't load menu definition: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	popup = g_object_ref (gtk_ui_manager_get_widget (ui_manager, "/ThumbnailPopup"));

 	g_object_unref (ui_manager);
 	eog_thumb_view_set_thumbnail_popup (EOG_THUMB_VIEW (thumb_view), popup);

	g_signal_connect (G_OBJECT (thumb_view),
			  "selection-changed",
			  G_CALLBACK (tb_on_selection_changed),
			  window);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (thumb_view));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	vbox = gtk_vbox_new (FALSE, 0);
	hbox = gtk_hbox_new (FALSE, 0);

	button = gtk_button_new_with_label ("Select first image");
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (on_button_sfi_clicked), thumb_view);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock (GTK_STOCK_GOTO_FIRST);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (on_button_first_clicked), thumb_view);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (on_button_back_clicked), thumb_view);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (on_button_next_clicked), thumb_view);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock (GTK_STOCK_GOTO_LAST);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (on_button_last_clicked), thumb_view);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show_all (window);

	list_uris = string_array_to_list (argv + 1, argc - 1);

	eog_list_store_add_uris (EOG_LIST_STORE (model), list_uris);
	eog_thumb_view_set_model (EOG_THUMB_VIEW (thumb_view), EOG_LIST_STORE (model));

	gtk_main ();
	return 0;
}
