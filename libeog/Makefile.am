SUBDIRS = stock cursors

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
	$(EXTRA_WARNING_CFLAGS)				\
	$(GNOME_CFLAGS)					\
	$(EXTRA_GNOME_CFLAGS)

CORBA_SOURCES =			\
	Eog.h			\
	Eog-common.c		\
	Eog-stubs.c		\
	Eog-skels.c

noinst_LTLIBRARIES = libeog.la

libeog_la_SOURCES =			\
	$(CORBA_SOURCES)		\
	gtkscrollframe.c		\
	gtkscrollframe.h		\
	image-view.c			\
	image-view.h			\
	uta.c				\
	uta.h				\
	cursors.c			\
	cursors.h			\
	ui-image.c			\
	ui-image.h			\
	zoom.c				\
	zoom.h


libeog_la_LIBADD = 			\
	$(GNOME_LIBS)			\
	$(GNOME_LIBDIR)			\
	$(EXTRA_GNOME_LIBS)		\
	$(OAF_LIBS)			\
	$(INTLLIBS)			\
	$(LIBPNG) $(LIBJPEG)

idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -D__EOG_COMPILATION

$(CORBA_SOURCES): $(idls)
	$(ORBIT_IDL) $(top_srcdir)/idl/Eog.idl -I$(top_srcdir)/idl $(idl_flags)

Eog.h: $(idls)

BUILT_SOURCES=$(CORBA_SOURCES)
CLEANFILES += $(BUILT_SOURCES)

oafdir = $(datadir)/oaf
oaf_in_files = GNOME_EOG.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)
@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
