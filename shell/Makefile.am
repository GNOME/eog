SUBDIRS = recent-files

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
	$(top_builddir)/shell/recent-files/librecent.a

eog_LDADD = 				\
	-L$(top_builddir)/libeog 	\
	-L$(top_builddir)/shell/recent-files \
	-leog				\
	-lrecent			\
	$(POPT_LIBS)			\
	$(X_LIBS)			\
	$(EOG_LIBS)

uidir = $(datadir)/gnome-2.0/ui
UI_FILES = eog-shell-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = $(UI_FILES)
