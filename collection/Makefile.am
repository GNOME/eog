INCLUDES =							\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DDATADIR=\""$(datadir)"\"				\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	$(EOG_CFLAGS)						\
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

eog_collection_view_SOURCES =		\
	$(EVOLUTION_GENERATED)		\
	eog-collection-marshal.h	\
	eog-collection-marshal.c	\
	eog-wrap-list.c			\
	eog-wrap-list.h			\
	eog-collection-view.h		\
	eog-collection-view.c		\
	eog-collection-item.h		\
	eog-collection-item.c		\
	eog-collection-model.h		\
	eog-collection-model.c		\
	main.c

#	eog-collection-preferences.h	\
#	eog-collection-preferences.c	\
#	eog-slide-show.h		\
#	eog-slide-show.c

eog_collection_view_DEPENDENCIES = $(top_builddir)/libeog/libeog.a

eog_collection_view_LDADD = 			\
	-L$(top_builddir)/libeog		\
	-leog					\
	$(EOG_LIBS)				\
	$(INTLLIBS)				\
	$(LIBPNG) $(LIBJPEG)

server_in_files = GNOME_EOG_Collection.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = eog-collection-view-ui.xml

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = $(ui_DATA) $(server_in_files) eog-collection-marshal.list
