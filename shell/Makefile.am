INCLUDES =							\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	$(GCONF_CFLAGS)						\
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

eog_DEPENDENCIES = $(top_builddir)/libeog/libeog.a

eog_LDADD = 				\
	-L$(top_builddir)/libeog 	\
	-leog				\
	$(POPT_LIBS)			\
	$(EOG_LIBS)

uidir = $(datadir)/gnome-2.0/ui
UI_FILES = eog-shell-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = $(UI_FILES)
