#include <config.h>
#include <gnome.h>
#include "gnome-icon-item-factory.h"
#include "gnome-icon-view.h"

typedef struct {
	char *name;
	GdkPixbuf *pixbuf;
} Icon;

static Icon icons[] = {
	{ "apple-green.png", NULL },
	{ "apple-red.png", NULL },
	{ "emacs.png", NULL },
	{ "gnome-aorta.png", NULL },
	{ "gnome-audio2.png", NULL },
	{ "gnome-background.png", NULL },
	{ "gnome-balsa2.png", NULL },
	{ "gnome-battery.png", NULL },
	{ "gnome-calc2.png", NULL },
	{ "gnome-calendar.png", NULL },
	{ "gnome-cardgame.png", NULL },
	{ "gnome-ccbackground.png", NULL },
	{ "gnome-ccdesktop.png", NULL },
	{ "gnome-ccdialog.png", NULL },
	{ "gnome-cckeyboard-bell.png", NULL },
	{ "gnome-cckeyboard.png", NULL },
	{ "gnome-ccmime.png", NULL },
	{ "gnome-ccperiph.png", NULL },
	{ "gnome-ccscreensaver.png", NULL },
	{ "gnome-ccthemes.png", NULL },
	{ "gnome-ccwindowmanager.png", NULL },
	{ "gnome-clock.png", NULL },
	{ "gnome-color-browser.png", NULL },
	{ "gnome-color-xterm.png", NULL },
	{ "gnome-cpu-mem.png", NULL },
	{ "gnome-cpu.png", NULL },
	{ "gnome-cromagnon.png", NULL },
	{ "gnome-default.png", NULL },
	{ "gnome-die1.png", NULL },
	{ "gnome-die2.png", NULL },
	{ "gnome-die3.png", NULL },
	{ "gnome-die4.png", NULL },
	{ "gnome-die5.png", NULL },
	{ "gnome-die6.png", NULL },
	{ "gnome-ee.png", NULL },
	{ "gnome-eterm.png", NULL },
	{ "gnome-favorites.png", NULL },
	{ "gnome-fifteen.png", NULL },
	{ "gnome-file-c.png", NULL },
	{ "gnome-file-h.png", NULL },
	{ "gnome-fish.png", NULL },
	{ "gnome-folder.png", NULL },
	{ "gnome-gegl.png", NULL },
	{ "gnome-gemvt.png", NULL },
	{ "gnome-gimp.png", NULL },
	{ "gnome-globe.png", NULL },
	{ "gnome-gmenu.png", NULL },
	{ "gnome-gmush.png", NULL },
	{ "gnome-gnomine.png", NULL },
	{ "gnome-graphics.png", NULL },
	{ "gnome-gsame.png", NULL },
	{ "gnome-help.png", NULL },
	{ "gnome-html.png", NULL },
	{ "gnome-image-gif.png", NULL },
	{ "gnome-image-jpeg.png", NULL },
	{ "gnome-irc.png", NULL },
	{ "gnome-joystick.png", NULL },
	{ "gnome-laptop.png", NULL },
	{ "gnome-life.png", NULL },
	{ "gnome-lockscreen.png", NULL },
	{ "gnome-log.png", NULL },
	{ "gnome-logo-icon-transparent.png", NULL },
	{ "gnome-logo-icon.png", NULL },
	{ "gnome-logo-large.png", NULL },
	{ "gnome-mem.png", NULL },
	{ "gnome-mnemonic.png", NULL },
	{ "gnome-modem.png", NULL },
	{ "gnome-money.png", NULL },
	{ "gnome-mouse.png", NULL },
	{ "gnome-networktool.png", NULL },
	{ "gnome-note.png", NULL },
	{ "gnome-panel.png", NULL },
	{ "gnome-qeye.png", NULL },
	{ "gnome-talk.png", NULL },
	{ "gnome-term-linux.png", NULL },
	{ "gnome-term-linux2.png", NULL },
	{ "gnome-term-night.png", NULL },
	{ "gnome-term-tiger.png", NULL },
	{ "gnome-term.png", NULL },
	{ "gnome-terminal.png", NULL },
	{ "gnome-tigert.png", NULL },
	{ "gnome-unknown.png", NULL },
	{ "gnome-util.png", NULL },
	{ "gnome-word.png", NULL },
	{ "gnome-xterm.png", NULL }
};

static void
create_data (void)
{
	int i;
	char *name;

	for (i = 0; i < sizeof (icons) / sizeof (icons[0]); i++) {
		name = gnome_unconditional_pixmap_file (icons[i].name);
		g_print ("Loading `%s'... ", name);
		icons[i].pixbuf = gdk_pixbuf_new_from_file (name);
		g_print (icons[i].pixbuf ? "\n" : "failed\n");
		g_free (name);
	}
}

static guint
get_length (GnomeListModel *model, gpointer data)
{
	return sizeof (icons) / sizeof (icons[0]);
}

static void
get_icon (GnomeIconListModel *model, guint n, GdkPixbuf **pixbuf, const char **caption, gpointer data)
{
	g_assert (n < sizeof (icons) / sizeof (icons[0]));
	g_assert (pixbuf != NULL);
	g_assert (caption != NULL);

	*pixbuf = icons[n].pixbuf;
	*caption = icons[n].name;
}

static GnomeIconListModel *
create_model (void)
{
	GnomeIconListModel *model;

	model = gtk_type_new (GNOME_TYPE_ICON_LIST_MODEL);

	gtk_signal_connect (GTK_OBJECT (model), "get_length",
			    GTK_SIGNAL_FUNC (get_length),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "get_icon",
			    GTK_SIGNAL_FUNC (get_icon),
			    NULL);

	return model;
}

int
main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *view;
	GnomeIconListModel *model;
	GnomeIconItemFactory *factory;

	gnome_init ("testicon", "1.0", argc, argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			    GTK_SIGNAL_FUNC (gtk_main_quit),
			    NULL);

	create_data ();
	model = create_model ();

	view = gnome_icon_view_new ();
	gtk_container_add (GTK_CONTAINER (window), view);
	gnome_icon_view_set_model (GNOME_ICON_VIEW (view), model);
	gnome_wrap_list_set_item_size (GNOME_WRAP_LIST (view), 48, 48);
	gnome_wrap_list_set_row_spacing (GNOME_WRAP_LIST (view), 4);
	gnome_wrap_list_set_col_spacing (GNOME_WRAP_LIST (view), 4);
	gnome_wrap_list_set_item_size (GNOME_WRAP_LIST (view), 80, 80);
	gnome_wrap_list_set_shadow_type (GNOME_WRAP_LIST (view), GTK_SHADOW_IN);

	factory = gtk_type_new (GNOME_TYPE_ICON_ITEM_FACTORY);
	gnome_icon_item_factory_set_item_metrics (factory, 80, 80, 48, 48);
	gnome_list_view_set_list_item_factory (GNOME_LIST_VIEW (view),
					       GNOME_LIST_ITEM_FACTORY (factory));

	gtk_widget_show_all (window);
	gtk_main ();
	return 0;
}
