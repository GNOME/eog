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
	-DDATADIR=\""$(datadir)"\"				\
	-DEOG_ICONDIR=\""$(datadir)/pixmaps"\"

bin_PROGRAMS = eog

eog_SOURCES =			\
	eog-window.h		\
	eog-window.c		\
	eog-preferences.h	\
	eog-preferences.c	\
	main.c			\
	session.c		\
	session.h		\
	util.h			\
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
	$(jpeg_LIB)			\
	$(POPT_LIBS)			\
	$(X_LIBS)			\
	$(EOG_LIBS)

uidir = $(datadir)/eog
UI_FILES = eog-gtk-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = $(UI_FILES)
