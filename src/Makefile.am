SUBDIRS = cursors stock

INCLUDES =						\
	-I$(includedir)					\
	$(EXTRA_GNOME_CFLAGS)				\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"eog\"

eog_SOURCES =				\
	gnome-icon-item-factory.c	\
	gnome-icon-item-factory.h	\
	gnome-icon-list-model.c		\
	gnome-icon-list-model.h		\
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
	commands.c			\
	commands.h			\
	cursors.c			\
	cursors.h			\
	gtkscrollframe.c		\
	gtkscrollframe.h		\
	image.h				\
	image.c				\
	image-item.c			\
	image-item.h			\
	main.c				\
	render.c			\
	render.h			\
	stock.c				\
	stock.h				\
	tb-image.c			\
	tb-image.h			\
	ui-image.c			\
	ui-image.h			\
	util.c				\
	util.h				\
	window.c			\
	window.h			\
	zoom.c				\
	zoom.h

eog_LDADD =			\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)

bin_PROGRAMS = eog
