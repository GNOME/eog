INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libeog				\
	-I$(top_builddir)/libeog			\
	$(EOG_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)

bin_PROGRAMS = eog-collection

$(eog_collection_OBJECTS): eog-collection-marshal.c eog-collection-marshal.h

eog-collection-marshal.h: eog-collection-marshal.list $(GLIB_GENMARSHAL)
	echo "genmarshal h" && \
        $(GLIB_GENMARSHAL) $< --header --prefix=eog_collection_marshal > $@

eog-collection-marshal.c: eog-collection-marshal.list $(GLIB_GENMARSHAL)
	echo "genmarshal c" && \
        $(GLIB_GENMARSHAL) $< --body --prefix=eog_collection_marshal > $@


if ENABLE_EVOLUTION
EVOLUTION_GENERATED =			\
	Evolution-Composer.h		\
	Evolution-Composer-common.c	\
	Evolution-Composer-skels.c	\
	Evolution-Composer-stubs.c
endif

eog_collection_SOURCES =		\
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
	eog-collection-model.h		\
	eog-collection-model.c		\
	eog-item-factory.h		\
	eog-item-factory.c		\
	eog-item-factory-simple.h	\
	eog-item-factory-simple.c	\
	eog-collection-preferences.h	\
	eog-collection-preferences.c	\
	main.c

eog_collection_DEPENDENCIES = $(top_builddir)/libeog/libeog.a

eog_collection_LDADD = 			\
	-L$(top_builddir)/libeog		\
	-leog					\
	$(EOG_LIBS)				\
	$(INTLLIBS)				\
	$(LIBPNG) $(LIBJPEG)

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_EOG_Collection.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

if ENABLE_EVOLUTION
$(EVOLUTION_GENERATED):
	$(ORBIT_IDL) `$(GNOME_CONFIG) --datadir`/idl/Evolution-Composer.idl
endif

CLEANFILES += $(EVOLUTION_GENERATED)
EXTRA_DIST = $(server_in_files)
