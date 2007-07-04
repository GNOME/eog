if ENABLE_JPEG
jpeg_LIB = -L$(top_builddir)/jpegutils -leog-jpegutils
jpeg_DEPS = $(top_builddir)/jpegutils/libeog-jpegutils.a
endif

toolbar_LIB = -L$(top_builddir)/cut-n-paste/toolbar-editor -ltoolbareditor
toolbar_DEPS = $(top_builddir)/cut-n-paste/toolbar-editor/libtoolbareditor.la

noinst_LTLIBRARIES = libeog.la

bin_PROGRAMS = eog

headerdir = $(prefix)/include/eog-@EOG_API_VERSION@/eog
header_DATA = $(INST_H_FILES)	

MARSHAL_OUTPUT = 	\
	eog-marshal.h	\
	eog-marshal.c

NOINST_H_FILES =			\
	eog-session.h			\
	eog-util.h			\
	eog-pixbuf-util.h		\
	eog-preferences-dialog.h	\
	eog-config-keys.h		\
	eog-image-jpeg.h		\
	eog-image-private.h		\
	eog-uri-converter.h		\
	eog-metadata-reader.h		\
	eog-save-as-dialog-helper.h	\
	eog-print-image-setup.h         \
	eog-print-preview.h             \
	eog-module.h             	\
	eog-plugin-manager.h            \
	eog-plugin-engine.h             \
	uta.h				\
	zoom.h	

if ENABLE_PYTHON
NOINST_H_FILES += 			\
	eog-python-module.h		\
	eog-python-plugin.h 
endif

INST_H_FILES =				\
	eog-application.h		\
	eog-debug.h			\
	eog-window.h			\
	eog-dialog.h			\
	eog-properties-dialog.h		\
	eog-message-area.h		\
	eog-error-message-area.h	\
	eog-file-chooser.h		\
	eog-statusbar.h			\
	eog-thumb-nav.h			\
	eog-transform.h			\
	eog-image.h			\
	eog-image-save-info.h		\
	eog-scroll-view.h		\
	eog-thumb-view.h		\
	eog-list-store.h		\
	eog-thumbnail.h			\
	eog-job-queue.h			\
	eog-jobs.h			\
	eog-plugin.h 

libeog_la_SOURCES =			\
	eog-application.c		\
	eog-session.c			\
	eog-debug.c			\
	eog-util.c			\
	eog-pixbuf-util.c		\
	eog-window.c			\
	eog-dialog.c			\
	eog-preferences-dialog.c	\
	eog-properties-dialog.c		\
	eog-message-area.c		\
	eog-error-message-area.c	\
	eog-file-chooser.c		\
	eog-statusbar.c			\
	eog-thumb-nav.c			\
	eog-transform.c			\
	eog-image.c			\
	eog-image-jpeg.c		\
	eog-image-save-info.c		\
	eog-scroll-view.c		\
	eog-thumb-view.c		\
	eog-list-store.c		\
	eog-thumbnail.c			\
	eog-job-queue.c			\
	eog-jobs.c			\
	eog-uri-converter.c		\
	eog-metadata-reader.c		\
	eog-save-as-dialog-helper.c	\
	eog-print-image-setup.c         \
	eog-print-preview.c           	\
	eog-module.c            	\
	eog-plugin.c            	\
	eog-plugin-manager.c            \
	eog-plugin-engine.c            	\
	uta.c				\
	zoom.c				\
	$(BUILT_SOURCES)		\
	$(NOINST_H_FILES)		\
	$(INST_H_FILES)

if HAVE_EXIF
libeog_la_SOURCES +=			\
	eog-exif-util.h			\
	eog-exif-util.c			\
	eog-exif-details.h		\
	eog-exif-details.c
endif

if ENABLE_PYTHON
libeog_la_SOURCES += 			\
	eog-python-module.c		\
	eog-python-module.h		\
	eog-python-plugin.c		\
	eog-python-plugin.h	
endif

libeog_la_CFLAGS =						\
	$(EOG_CFLAGS)						\
	$(WARN_CFLAGS)						\
	-I$(top_srcdir)/jpegutils				\
 	-I$(top_srcdir)/cut-n-paste/toolbar-editor		\
	-DG_LOG_DOMAIN=\"EOG\"					\
	-DEOG_PREFIX=\""${prefix}"\"				\
	-DEOG_DATA_DIR=\""$(pkgdatadir)"\"			\
	-DEOG_LOCALE_DIR=\""$(datadir)/locale"\"		\
	-DEOG_PIXMAPS_DIR=\""$(datadir)/pixmaps/eog"\"		\
	-DEOG_PLUGIN_DIR=\""$(libdir)/eog/plugins"\"

libeog_la_LIBADD = \
	$(EOG_LIBS)

if ENABLE_PYTHON
libeog_la_CFLAGS += 			\
	$(NO_STRICT_ALIASING_CFLAGS)	\
	$(PYGTK_CFLAGS)			\
	$(PYTHON_CFLAGS)		\
	$(AM_CFLAGS)

libeog_la_LIBADD += \
	$(top_builddir)/bindings/python/eog.la
endif

libeog_la_LDFLAGS = -export-dynamic -no-undefined -export-symbols-regex "^[[^_]].*"

eog_SOURCES = main.c

eog_DEPENDENCIES = 	\
	libeog.la	\
	$(jpeg_DEPS)	\
	$(toolbar_DEPS)

eog_CFLAGS = 							\
	$(EOG_CFLAGS)						\
 	-I$(top_srcdir)/cut-n-paste/toolbar-editor		\
	-DEOG_DATADIR=\""$(pkgdatadir)"\"			\
	-DEOG_LOCALE_DIR=\""$(datadir)/locale"\"

eog_LDADD = 				\
	libeog.la			\
	$(EOG_LIBS)			\
	$(X_LIBS)			\
	$(LIBJPEG)			\
 	$(toolbar_LIB)			\
	$(jpeg_LIB)

eog_LDFLAGS = -export-dynamic -no-undefined -export-symbols-regex "^[[^_]].*"

BUILT_SOURCES = 			\
	eog-enum-types.c		\
	eog-enum-types.h		\
	$(MARSHAL_OUTPUT)

eog-enum-types.h: eog-enum-types.h.template $(INST_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template eog-enum-types.h.template $(INST_H_FILES)) > $@
	
eog-enum-types.c: eog-enum-types.c.template $(INST_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template eog-enum-types.c.template $(INST_H_FILES)) > $@

eog-marshal.h: eog-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=eog_marshal > $@

eog-marshal.c: eog-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --header --prefix=eog_marshal > $@

EXTRA_DIST = 				\
	eog-enum-types.h.template	\
	eog-enum-types.c.template	\
	eog-marshal.list 

if HAVE_DBUS 

BUILT_SOURCES += eog-application-service.h

EXTRA_DIST += eog-application-service.xml

eog-application-service.h: eog-application-service.xml
	dbus-binding-tool --prefix=eog_application --mode=glib-server --output=eog-application-service.h $<

endif

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
