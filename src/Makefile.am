SUBDIRS = cursors stock

gladedir = $(datadir)/eog/glade

INCLUDES =						\
	-I$(includedir)					\
	$(EXTRA_GNOME_CFLAGS)				\
	$(EXTRA_WARNING_CFLAGS)				\
	$(GCONF_CFLAGS)					\
	-DEOG_GLADEDIR=\""$(gladedir)"\"		\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"eog\"

noinst_LTLIBRARIES = libeog.la

libeog_la_SOURCES =			\
	gtkscrollframe.c		\
	gtkscrollframe.h		\
	image-view.c			\
	image-view.h			\
	image.h				\
	image.c				\
	uta.c				\
	uta.h				\
	cursors.c			\
	cursors.h			\
	ui-image.c			\
	ui-image.h			\
	zoom.c				\
	zoom.h

libeog_la_LIBADD =			\
	$(EXTRA_GNOME_LIBS)		\
	$(INTLLIBS)

eog_SOURCES =				\
	commands.c			\
	commands.h			\
	e-dialog-widgets.c		\
	e-dialog-widgets.h		\
	full-screen.c			\
	full-screen.h			\
	main.c				\
	preferences.c			\
	preferences.h			\
	stock.c				\
	stock.h				\
	tb-image.c			\
	tb-image.h			\
	util.c				\
	util.h				\
	gconf.c				\
	window.c			\
	window.h

testicon_SOURCES =			\
	gnome-icon-item-factory.c	\
	gnome-icon-item-factory.h	\
	gnome-icon-list-model.c		\
	gnome-icon-list-model.h		\
	gnome-icon-view.c		\
	gnome-icon-view.h		\
	gnome-list-item-factory.c	\
	gnome-list-item-factory.h	\
	gnome-list-model.c		\
	gnome-list-model.h		\
	gnome-list-selection-model.c	\
	gnome-list-selection-model.h	\
	gnome-list-view.c		\
	gnome-list-view.h		\
	gnome-position-list-model.c	\
	gnome-position-list-model.h	\
	gnome-wrap-list.c		\
	gnome-wrap-list.h		\
	testicon.c

eog_LDADD =			\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)		\
	$(GCONF_LIBS)		\
	libeog.la

testicon_LDADD =		\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)

bin_PROGRAMS = eog

noinst_PROGRAMS = testicon

glade_DATA =				\
	preferences-dialog.glade

glade_messages =			\
	preferences-dialog.glade.h

EXTRA_DIST =				\
	$(glade_DATA)			\
	$(glade_messages)
