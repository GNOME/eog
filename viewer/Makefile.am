INCLUDES =							\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DDATADIR=\""$(datadir)"\"				\
	-DICONDIR=\""$(datadir)/pixmaps/eog"\"			\
	-I$(srcdir)						\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	-I$(top_srcdir)/libgpi					\
	$(EOG_CFLAGS)						\
	$(WARN_CFLAGS)

libexec_PROGRAMS = eog-image-viewer

eog_image_viewer_SOURCES =	\
	eog-image-view.c	\
	eog-image-view.h	\
	main.c

eog_image_viewer_DEPENDENCIES =		\
	$(top_builddir)/libeog/libeog.a	\
	$(top_builddir)/libgpi/libgpi.a

eog_image_viewer_LDADD = 			\
	-L$(top_builddir)/libeog -leog		\
	-L$(top_builddir)/libgpi -lgpi		\
	$(EOG_LIBS)				\
	$(LIBJPEG)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = eog-image-view-ui.xml \
	  eog-image-view-ctrl-ui.xml

EXTRA_DIST = $(ui_DATA)
