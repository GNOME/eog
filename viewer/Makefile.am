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
	$(EXTRA_VFS_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)				\
	$(EXTRA_WARNING_CFLAGS)				\
	$(GNOME_CFLAGS)

noinst_PROGRAMS = sample-container
sample_container_SOURCE = sample-container.c
sample_container_LDADD = $(GNOME_LIBS) $(EXTRA_BONOBO_LIBS)

bin_PROGRAMS = eog-image-viewer

if ENABLE_EVOLUTION
EVOLUTION_GENERATED =			\
	Evolution-Composer.h		\
	Evolution-Composer-common.c	\
	Evolution-Composer-skels.c	\
	Evolution-Composer-stubs.c
endif

eog_image_viewer_SOURCES =	\
	$(EVOLUTION_GENERATED)	\
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
	eog-full-screen.c	\
	eog-full-screen.h	\
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
	$(GCONF_LIBS)				\
	$(OAF_LIBS)				\
	$(INTLLIBS)				\
	$(EXTRA_BONOBO_LIBS)			\
	$(EXTRA_GNOME_LIBS)			\
	$(EXTRA_VFS_LIBS)			\
	$(LIBPNG) $(LIBJPEG)			\
	../libeog/libeog.la

$(EVOLUTION_GENERATED):
	$(ORBIT_IDL) `$(GNOME_CONFIG) --datadir`/idl/Evolution-Composer.idl

uidir = $(datadir)/gnome/ui
ui_DATA = eog-image-view-ui.xml

BUILT_SOURCES = $(EVOLUTION_GENERATED)
CLEANFILES += $(BUILT_SOURCES)
EXTRA_DIST = $(ui_DATA)

