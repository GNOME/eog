INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-DICONDIR=\""$(datadir)/pixmaps/eog"\"		\
	-I. 						\
	-I$(srcdir) 					\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libeog				\
	-I$(top_builddir)/libeog			\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
        $(GNOME_INCLUDEDIR)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	$(EXTRA_WARNING_CFLAGS)				\
	$(GNOME_CFLAGS)

bin_PROGRAMS = eog-image-viewer

noinst_PROGRAMS = sample-container

sample_container_SOURCES =	\
	sample-container.c	\
	eog-util.h		\
	eog-util.c

sample_container_LDADD = 	\
	$(GNOME_LIBS)		\
	$(GNOME_LIBDIR)		\
	$(GNOME_LIBS)		\
	$(GCONF_LIBS)		\
	$(OAF_LIBS)		\
	$(INTLLIBS)		\
	$(EXTRA_BONOBO_LIBS)	\
	../libeog/libeog.la

eog_image_viewer_SOURCES =	\
	eog-image.c		\
	eog-image.h		\
	eog-image-io.c		\
	eog-image-io.h		\
	eog-image-view.c	\
	eog-image-view.h	\
	eog-control.c		\
	eog-control.h		\
	eog-embeddable.c	\
	eog-embeddable.h	\
	eog-embeddable-view.c	\
	eog-embeddable-view.h	\
	eog-preview.c		\
	eog-preview.h		\
	eog-preview-page.c	\
	eog-preview-page.h	\
	eog-print-setup.c	\
	eog-print-setup.h	\
	eog-util.h		\
	eog-util.c		\
	preferences.h		\
	preferences.c		\
	main.c

eog_image_viewer_LDADD = 			\
	$(GNOME_LIBS)				\
	$(GNOME_LIBDIR)				\
	$(GNOME_LIBS)				\
	$(GCONF_LIBS)				\
	$(OAF_LIBS)				\
	$(INTLLIBS)				\
	$(EXTRA_BONOBO_LIBS)			\
	$(LIBPNG) $(LIBJPEG)			\
	../libeog/libeog.la

uidir = $(datadir)/gnome/ui
ui_DATA = eog-image-view-ui.xml

EXTRA_DIST = $(ui_DATA)

