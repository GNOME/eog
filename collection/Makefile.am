if ENABLE_LIBPREVIEW
  LIBPREVIEW_FLAGS=-DHAVE_LIBPREVIEW
endif

INCLUDES =							\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DDATADIR=\""$(datadir)"\"				\
	$(LIBPREVIEW_FLAGS)					\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	$(EOG_CFLAGS)						\
	$(LIBPREVIEW_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)

libexec_PROGRAMS = eog-collection-view

$(eog_collection-view_OBJECTS): eog-collection-marshal.c eog-collection-marshal.h

eog-collection-marshal.h: eog-collection-marshal.list $(GLIB_GENMARSHAL)
	echo "genmarshal h" && \
        $(GLIB_GENMARSHAL) $< --header --prefix=eog_collection_marshal > $@

eog-collection-marshal.c: eog-collection-marshal.list $(GLIB_GENMARSHAL)
	echo "genmarshal c" && \
        $(GLIB_GENMARSHAL) $< --body --prefix=eog_collection_marshal > $@

BUILT_SOURCES = \
	eog-collection-marshal.c	\
	eog-collection-marshal.h

if ENABLE_EVOLUTION
EVOLUTION_GENERATED =			\
	Evolution-Composer.h		\
	Evolution-Composer-common.c	\
	Evolution-Composer-skels.c	\
	Evolution-Composer-stubs.c
endif

eog_collection_view_SOURCES =		\
	$(EVOLUTION_GENERATED)		\
	eog-collection-marshal.h	\
	eog-collection-marshal.c	\
	eog-wrap-list.c			\
	eog-wrap-list.h			\
	eog-collection-control.c        \
	eog-collection-control.h        \
	eog-collection-view.h		\
	eog-collection-view.c		\
	cimage.c			\
	cimage.h			\
	eog-image-loader.c		\
	eog-image-loader.h		\
	eog-image-loader-simple.c	\
	eog-image-loader-simple.h	\
	eog-image-loader-preview.c	\
	eog-image-loader-preview.h	\
	eog-collection-model.h		\
	eog-collection-model.c		\
	eog-slide-show.h		\
	eog-slide-show.c		\
	eog-item-factory.h		\
	eog-item-factory.c		\
	eog-item-factory-simple.c	\
	eog-item-factory-simple.h	\
	eog-item-factory-clean.h	\
	eog-item-factory-clean.c	\
	eog-collection-preferences.h	\
	eog-collection-preferences.c	\
	main.c

eog_collection_view_DEPENDENCIES = $(top_builddir)/libeog/libeog.a

eog_collection_view_LDADD = 			\
	-L$(top_builddir)/libeog		\
	-leog					\
	$(EOG_LIBS)				\
	$(LIBPREVIEW_LIBS)			\
	$(INTLLIBS)				\
	$(LIBPNG) $(LIBJPEG)

server_in_files = GNOME_EOG_Collection.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

if ENABLE_EVOLUTION
$(EVOLUTION_GENERATED):
	$(ORBIT_IDL) `$(GNOME_CONFIG) --datadir`/idl/Evolution-Composer.idl
endif

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = eog-collection-view-ui.xml

CLEANFILES = $(EVOLUTION_GENERATED) $(BUILT_SOURCES)
EXTRA_DIST = $(ui_DATA) $(server_in_files) eog-collection-marshal.list
