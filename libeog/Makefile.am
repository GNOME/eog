SUBDIRS = cursors

if ENABLE_JPEG
jpeg_LIB = -L$(top_builddir)/jpegutils -leog-jpegutils
jpeg_DEPS = $(top_builddir)/jpegutils/libeog-jpegutils.a
endif

INCLUDES =							\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DHAVE_COLLECTION					\
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

noinst_PROGRAMS = test-thumb-view

libeog_a_SOURCES =			\
	$(MARSHAL_OUTPUT)		\
        accessible-image-view-factory.h	\
        accessible-image-view-factory.c	\
        accessible-image-view.h		\
        accessible-image-view.c		\
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
	eog-thumbnail.h			\
	eog-thumbnail.c			\
	eog-thumb-view.c		\
	eog-thumb-view.h		\
	eog-thumb-shadow.c		\
	eog-thumb-shadow.h		\
	eog-list-store.c		\
	eog-list-store.h		\
	eog-image-cache.h		\
	eog-image-cache.c		\
	eog-job-queue.c			\
	eog-job-queue.h			\
	eog-jobs.c			\
	eog-jobs.h			\
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

test_thumb_view_DEPENDENCIES = libeog.a

test_thumb_view_SOURCES = test-eog-tb.c eog-thumb-view.c eog-list-store.c

test_thumb_view_LDADD = \
	-L$(top_builddir)/libeog 	\
	-leog				\
	$(jpeg_LIB)			\
	$(EOG_LIBS)			\
	$(LIBJPEG)			\
	$(X_LIBS)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = libeog-marshal.list 

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
