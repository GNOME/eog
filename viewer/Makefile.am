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
	$(EOG_CFLAGS)						\
	$(EXTRA_WARNING_CFLAGS)

libexec_PROGRAMS = eog-image-viewer

eog_image_viewer_SOURCES =	\
	eog-image-view.c	\
	eog-image-view.h	\
	main.c
#	 eog-print-setup.c	\
#	 eog-print-setup.h	\
#	 eog-embeddable.c	\
#	 eog-embeddable.h	\
#	 eog-embeddable-view.c	\
#	 eog-embeddable-view.h	\
#	eog-control.c		\
#	eog-control.h		\
#	eog-full-screen.c	\
#	eog-full-screen.h	\
#	eog-preview.c		\
#	eog-preview.h		\
#	eog-preview-page.c	\
#	eog-preview-page.h	\
#	eog-util.h		\
#	eog-util.c		\
#	eog-image.c		\
#	eog-image.h		\
#	eog-image-io.c		\
#	eog-image-io.h		

eog_image_viewer_DEPENDENCIES = $(top_builddir)/libeog/libeog.a

eog_image_viewer_LDADD = 			\
	-L$(top_builddir)/libeog		\
	-leog					\
	$(EOG_LIBS)				\
	$(LIBPNG) $(LIBJPEG)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = eog-image-view-ui.xml \
	  eog-image-view-ctrl-ui.xml

EXTRA_DIST = $(ui_DATA)
