#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-rename-dialog.h"

#include <string.h>

#define DEFAULT_ENTRY_SIZE 25

static glong
get_selected_size (const gchar *str)
{
        glong  size;
        gchar *substr;

        size = g_utf8_strlen (str, -1);
        substr = g_utf8_strrchr (str, size, '.');

        if (substr == NULL)
            return -1;

        return size - g_utf8_strlen (substr, -1);
}

static void
press_enter_to_rename (GtkEntry *entry, gpointer data)
{
        (void) entry;
        gtk_button_clicked(GTK_BUTTON (data));
}

GtkWidget *
eog_rename_dialog_new (GtkWindow *main, const gchar *prev_name)
{
	GtkBuilder *xml;
	GtkWidget  *dlg;
        GtkWidget  *name_entry;
        GtkWidget  *rename_button;

        xml = gtk_builder_new_from_resource ("/org/gnome/eog/ui/eog-rename-dialog.ui");

	gtk_builder_set_translation_domain (xml, GETTEXT_PACKAGE);
	dlg = GTK_WIDGET (g_object_ref (gtk_builder_get_object (xml, "eog_rename_dialog")));
	gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main));
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER_ON_PARENT);

        name_entry = GTK_WIDGET (gtk_builder_get_object (xml, "name_entry"));

        if (prev_name != NULL) {

                size_t text_size = strlen (prev_name);
                gint entry_size = text_size > DEFAULT_ENTRY_SIZE ? text_size : DEFAULT_ENTRY_SIZE;

                gtk_entry_set_text (GTK_ENTRY(name_entry), prev_name);
                gtk_entry_set_width_chars (GTK_ENTRY(name_entry), entry_size);

                gtk_widget_grab_focus (name_entry);
                gint max = get_selected_size (prev_name);
                gtk_editable_select_region (GTK_EDITABLE(name_entry), 0, max);
        }

        rename_button = GTK_WIDGET (gtk_builder_get_object (xml, "rename_button"));
        g_signal_connect (G_OBJECT (name_entry), "activate",
                          (GCallback) press_enter_to_rename, rename_button);

        g_object_set_data (G_OBJECT (dlg), "name_entry", name_entry);

        g_object_unref (xml);

        return dlg;
}

const gchar *
eog_rename_dialog_get_new_name (GtkWidget *dlg)
{
        GtkWidget *name_entry;

	name_entry = g_object_get_data (G_OBJECT (dlg), "name_entry");
	g_assert (name_entry != NULL);

        const gchar *name = gtk_entry_get_text (GTK_ENTRY (name_entry));

        return name;
}
