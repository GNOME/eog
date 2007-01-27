if ENABLE_JPEG
jpeg_LIB = -L$(top_builddir)/jpegutils -leog-jpegutils
jpeg_DEPS = $(top_builddir)/jpegutils/libeog-jpegutils.a
endif

INCLUDES =							\
	$(EOG_CFLAGS)						\
	$(WARN_CFLAGS)						\
	-I$(top_srcdir)/jpegutils				\
	-DHAVE_COLLECTION					\
	-DG_LOG_DOMAIN=\"Eog\"					\
	-DEOG_PREFIX=\""${prefix}"\"				\
	-DEOG_DATADIR=\""$(datadir)"\"				\
	-DEOG_ICONDIR=\""$(datadir)/pixmaps"\"			\
	-DDATADIR=\"$(pkgdatadir)\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

eog-marshal.h: eog-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=eog_marshal > $@

eog-marshal.c: eog-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --header --prefix=eog_marshal > $@

MARSHAL_OUTPUT = 	\
	eog-marshal.h	\
	eog-marshal.c

bin_PROGRAMS = eog

eog_SOURCES =				\
	$(MARSHAL_OUTPUT)		\
	eog-application.h		\
	eog-application.c		\
	eog-session.h			\
	eog-session.c			\
	eog-debug.h			\
	eog-debug.c			\
	eog-util.h			\
	eog-util.c			\
	eog-pixbuf-util.h		\
	eog-pixbuf-util.c		\
	eog-window.h			\
	eog-window.c			\
	eog-dialog.h			\
	eog-dialog.c			\
	eog-preferences-dialog.h	\
	eog-preferences-dialog.c	\
	eog-properties-dialog.h		\
	eog-properties-dialog.c		\
	eog-file-chooser.h		\
	eog-file-chooser.c		\
	eog-statusbar.h			\
	eog-statusbar.c			\
	eog-thumb-nav.h			\
	eog-thumb-nav.c			\
	eog-config-keys.h		\
	eog-transform.h			\
	eog-transform.c			\
	eog-image.c			\
	eog-image.h			\
	eog-image-private.h		\
	eog-image-jpeg.c		\
	eog-image-jpeg.h		\
	eog-image-save-info.c		\
	eog-image-save-info.h		\
	eog-scroll-view.c		\
	eog-scroll-view.h		\
	eog-thumb-view.c		\
	eog-thumb-view.h		\
	eog-list-store.c		\
	eog-list-store.h		\
	eog-thumbnail.h			\
	eog-thumbnail.c			\
	eog-thumb-shadow.c		\
	eog-thumb-shadow.h		\
	eog-job-queue.c			\
	eog-job-queue.h			\
	eog-jobs.c			\
	eog-jobs.h			\
	eog-uri-converter.h		\
	eog-uri-converter.c		\
	eog-metadata-reader.h		\
	eog-metadata-reader.c		\
	eog-save-dialog-helper.h	\
	eog-save-dialog-helper.c	\
	eog-save-as-dialog-helper.h	\
	eog-save-as-dialog-helper.c	\
	eog-print-image-setup.c         \
	eog-print-image-setup.h         \
	eog-print-preview.h             \
	eog-print-preview.c           	\
	uta.c				\
	uta.h				\
	zoom.c				\
	zoom.h				\
	main.c	

if HAVE_EXIF
eog_SOURCES +=				\
	eog-exif-details.h		\
	eog-exif-details.c
endif

eog_DEPENDENCIES = 	\
	$(jpeg_DEPS)

eog_LDADD = 				\
	$(EOG_LIBS)			\
	$(LIBJPEG)			\
	$(jpeg_LIB)			\
	$(X_LIBS)

BUILT_SOURCES = 			\
	$(MARSHAL_OUTPUT)
	
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = eog-marshal.list 

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
