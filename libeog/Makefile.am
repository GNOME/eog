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

MARSHAL_OUTPUT = 		\
	libeog-marshal.h	\
	libeog-marshal.c

noinst_LIBRARIES = libeog.a

libeog_a_SOURCES =			\
	$(MARSHAL_OUTPUT)		\
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
	eog-image-save-info.c		\
	eog-image-save-info.h		\
	eog-save-dialog-helper.h	\
	eog-save-dialog-helper.c	\
	eog-save-as-dialog-helper.h	\
	eog-save-as-dialog-helper.c	\
	eog-image-cache.h		\
	eog-image-cache.c		\
	eog-image-list.c		\
	eog-image-list.h		\
	eog-uri-converter.h		\
	eog-uri-converter.c		\
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
	eog-metadata-reader.h		\
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
	eog-util.h			\
	eog-util.c			\
	uta.c				\
	uta.h				\
	cursors.c			\
	cursors.h			\
	zoom.c				\
	zoom.h

BUILT_SOURCES = 			\
	$(MARSHAL_OUTPUT)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = libeog-marshal.list 

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
