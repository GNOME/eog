SUBDIRS = recent-files

if ENABLE_JPEG
jpeg_LIB = -L$(top_builddir)/jpegutils -leog-jpegutils
jpeg_DEPS = $(top_builddir)/jpegutils/libeog-jpegutils.a
endif

INCLUDES =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	-I$(top_srcdir)/shell/recent-files			\
	$(WARN_CFLAGS)						\
	$(EOG_CFLAGS)						\
	-DHAVE_COLLECTION					\
	-DEOG_PREFIX=\""${prefix}"\"				\
	-DEOG_DATADIR=\""$(datadir)"\"				\
	-DEOG_ICONDIR=\""$(datadir)/pixmaps"\"

bin_PROGRAMS = eog

eog_SOURCES =				\
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
	main.c				\
	session.c			\
	session.h			\
	util.h				\
	util.c

eog_DEPENDENCIES = 				\
	$(top_builddir)/libeog/libeog.a 	\
	$(top_builddir)/shell/recent-files/librecent.a \
	$(jpeg_DEPS)

eog_LDADD = 				\
	-L$(top_builddir)/libeog 	\
	-L$(top_builddir)/shell/recent-files \
	-leog				\
	-lrecent			\
	$(LIBJPEG)			\
	$(jpeg_LIB)			\
	$(X_LIBS)			\
	$(EOG_LIBS)			\
	$(LIBJPEG)

uidir = $(datadir)/eog
UI_FILES = eog-gtk-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = $(UI_FILES)
