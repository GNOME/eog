INCLUDES =						\
	-I$(includedir)					\
	$(EXTRA_GNOME_CFLAGS)				\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"eog\"

eog_SOURCES =			\
	commands.c		\
	commands.h		\
	gtkscrollframe.c	\
	gtkscrollframe.h	\
	image.h			\
	image.c			\
	image-item.c		\
	image-item.h		\
	main.c			\
	render.c		\
	render.h		\
	tb-image.c		\
	tb-image.h		\
	ui-image.c		\
	ui-image.h		\
	util.c			\
	util.h			\
	window.c		\
	window.h		\
	zoom.c			\
	zoom.h

eog_LDADD =			\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)

bin_PROGRAMS = eog
