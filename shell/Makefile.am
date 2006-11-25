if ENABLE_JPEG
jpeg_LIB = -L$(top_builddir)/jpegutils -leog-jpegutils
jpeg_DEPS = $(top_builddir)/jpegutils/libeog-jpegutils.a
endif

INCLUDES =							\
	-DDATADIR=\"$(pkgdatadir)\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	$(WARN_CFLAGS)						\
	$(EOG_CFLAGS)						\
	-DHAVE_COLLECTION					\
	-DEOG_PREFIX=\""${prefix}"\"				\
	-DEOG_DATADIR=\""$(datadir)"\"				\
	-DEOG_ICONDIR=\""$(datadir)/pixmaps"\"

bin_PROGRAMS = eog

eog_SOURCES =				\
	eog-application.h		\
	eog-application.c		\
	eog-session.h			\
	eog-session.c			\
	eog-util.h			\
	eog-util.c			\
	eog-window.h			\
	eog-window.c			\
	eog-preferences.h		\
	eog-preferences.c		\
	eog-file-chooser.h		\
	eog-file-chooser.c		\
	eog-save-dialog-helper.h	\
	eog-save-dialog-helper.c	\
	eog-save-as-dialog-helper.h	\
	eog-save-as-dialog-helper.c	\
	eog-statusbar.h			\
	eog-statusbar.c			\
	eog-thumb-nav.h			\
	eog-thumb-nav.c			\
	eog-config-keys.h		\
	main.c	

eog_DEPENDENCIES = 				\
	$(top_builddir)/libeog/libeog.a 	\
	$(jpeg_DEPS)

eog_LDADD = 				\
	-L$(top_builddir)/libeog 	\
	-leog				\
	$(LIBJPEG)			\
	$(jpeg_LIB)			\
	$(X_LIBS)			\
	$(EOG_LIBS)			\
	$(LIBJPEG)
