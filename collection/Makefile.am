if ENABLE_JPEG
jpeg_LIB = -L$(top_builddir)/jpegutils -leog-jpegutils
jpeg_DEPS = $(top_builddir)/jpegutils/libeog-jpegutils.a
endif

INCLUDES =							\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DDATADIR=\""$(datadir)"\"				\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	$(EOG_CFLAGS)						\
	$(WARN_CFLAGS)

libexec_PROGRAMS = eog-collection-view

eog_collection_view_SOURCES =		\
	eog-collection-view.h		\
	eog-collection-view.c		\
	eog-collection-view-iface.h	\
	eog-collection-view-iface.c	\
	eog-save-dialog.h		\
	eog-save-dialog.c		\
	main.c

eog_collection_view_DEPENDENCIES = $(top_builddir)/libeog/libeog.a $(jpeg_DEPS)

eog_collection_view_LDADD = 			\
	-L$(top_builddir)/libeog		\
	-leog					\
	$(jpeg_LIB)				\
	$(EOG_LIBS)				\
	$(INTLLIBS)				\
	$(X_LIBS)				\
	$(LIBJPEG)

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

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
