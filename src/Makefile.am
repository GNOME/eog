INCLUDES =						\
	-I$(includedir)					\
	$(EXTRA_GNOME_CFLAGS)				\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"eog\"

eog_SOURCES =			\
	gtkscrollframe.c	\
	gtkscrollframe.h	\
	image.h			\
	image.c			\
	image-item.c		\
	image-item.h		\
	main.c			\
	render.c		\
	render.h		\
	ui-image.c		\
	ui-image.h		\
	window.c		\
	window.h

eog_LDADD =			\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)

bin_PROGRAMS = eog
