INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-I. 						\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(top_srcdir)/src				\
	-I$(top_builddir)				\
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
	$(OAF_LIBS)		\
	$(INTLLIBS)		\
	$(EXTRA_BONOBO_LIBS)	\
	../src/libeog.la

CORBA_SOURCE =			\
	Eog.h			\
	Eog-common.c		\
	Eog-stubs.c		\
	Eog-skels.c

eog_image_viewer_SOURCES =	\
	$(CORBA_SOURCE)		\
	eog-image-data.c	\
	eog-image-data.h	\
	eog-image.c		\
	eog-image.h		\
	eog-image-view.c	\
	eog-image-view.h	\
	eog-control.c		\
	eog-control.h		\
	eog-embeddable.c	\
	eog-embeddable.h	\
	eog-embeddable-view.c	\
	eog-embeddable-view.h	\
	eog-util.h		\
	eog-util.c		\
	main.c

eog_image_viewer_LDADD = 			\
	$(GNOME_LIBS)				\
	$(GNOME_LIBDIR)				\
	$(GNOME_LIBS)				\
	$(OAF_LIBS)				\
	$(INTLLIBS)				\
	$(EXTRA_BONOBO_LIBS)			\
	../src/libeog.la

idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -D__EOG_COMPILATION

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) $(top_srcdir)/idl/Eog.idl -I$(top_srcdir)/idl $(idl_flags)

Eog.h: $(idls)

BUILT_SOURCES=$(CORBA_SOURCE)

CLEANFILES += $(BUILT_SOURCES)

oafdir = $(datadir)/oaf
OAF_FILES = eog-image-viewer.oafinfo
oaf_DATA = $(OAF_FILES)

uidir = $(datadir)/gnome/ui
ui_DATA = eog-image-view-ui.xml

EXTRA_DIST = $(OAF_FILES)

