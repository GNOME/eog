SUBDIRS = cursors

INCLUDES =							\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DHAVE_COLLECTION					\
	-DDATADIR=\""$(datadir)"\"				\
	-I$(srcdir) -I$(top_srcdir)				\
	-I$(top_srcdir)/src					\
	-I$(top_srcdir)/jpegutils				\
	-I$(top_builddir)					\
	$(EOG_CFLAGS)						\
	$(WARN_CFLAGS)

libeog-marshal.h: libeog-marshal.list $(GLIB_GENMARSHAL)
	echo "genmarshal h" && \
        $(GLIB_GENMARSHAL) $< --header --prefix=libeog_marshal > $@

libeog-marshal.c: libeog-marshal.list $(GLIB_GENMARSHAL)
	echo "genmarshal c" && \
        $(GLIB_GENMARSHAL) $< --body --prefix=libeog_marshal > $@

MARSHAL_SOURCES = 		\
	libeog-marshal.h	\
	libeog-marshal.c

CORBA_SOURCES =			\
	Eog.h			\
	Eog-common.c		\
	Eog-stubs.c		\
	Eog-skels.c

noinst_LIBRARIES = libeog.a

libeog_a_SOURCES =			\
	$(CORBA_SOURCES)		\
	$(MARSHAL_SOURCES)		\
	access.c			\
	access.h			\
        accessible-image-view-factory.h	\
        accessible-image-view-factory.c	\
        accessible-image-view.h		\
        accessible-image-view.c		\
	eog-file-selection.h		\
	eog-file-selection.c		\
	eog-full-screen.h		\
	eog-full-screen.c		\
	eog-pixbuf-util.h		\
	eog-pixbuf-util.c		\
	eog-transform.h			\
	eog-transform.c			\
	eog-image.c			\
	eog-image.h			\
	eog-image-private.h		\
	eog-image-jpeg.c		\
	eog-image-jpeg.h		\
	eog-image-cache.h		\
	eog-image-cache.c		\
	eog-image-list.c		\
	eog-image-list.h		\
	eog-scroll-view.c		\
	eog-scroll-view.h		\
	eog-info-view.c			\
	eog-info-view.h			\
	eog-info-view-detail.h		\
	eog-info-view-detail.c		\
	eog-info-view-file.h		\
	eog-info-view-file.c		\
	eog-info-view-exif.h		\
	eog-info-view-exif.c		\
	eog-metdata-reader.h		\
	eog-metadata-reader.c		\
	eog-hig-dialog.h		\
	eog-hig-dialog.c		\
	eog-horizontal-splitter.c	\
	eog-horizontal-splitter.h	\
	eog-vertical-splitter.c		\
	eog-vertical-splitter.h		\
	eog-config-keys.h		\
	eog-wrap-list.h			\
	eog-wrap-list.c			\
	eog-collection-item.h		\
	eog-collection-item.c		\
	eog-canvas-pixbuf.h		\
	eog-canvas-pixbuf.c		\
	uta.c				\
	uta.h				\
	cursors.c			\
	cursors.h			\
	zoom.c				\
	zoom.h

idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags =  -I$(BONOBO_IDLDIR) -I$(BONOBO_ACT_IDLDIR) -I$(top_srcdir)/idl -D__EOG_COMPILATION

$(CORBA_SOURCES): $(idls)
	$(ORBIT_IDL)   $(top_srcdir)/idl/Eog.idl $(idl_flags)

Eog.h: $(idls)


server_in_files = GNOME_EOG.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = 			\
	$(CORBA_SOURCES)		\
	$(MARSHAL_SOURCES)

CLEANFILES = $(BUILT_SOURCES) $(server_DATA)

EXTRA_DIST = $(server_in_files) libeog-marshal.list 

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
