
INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-I. 						\
	-I$(top_srcdir)/libeog				\
	-I$(top_builddir)/libeog			\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)				\
	-DEOG_ICONDIR=\""$(datadir)/pixmaps"\"

bin_PROGRAMS = eog

eog_SOURCES =	\
	eog-window.h		\
	eog-window.c		\
	eog-preferences.h	\
	eog-preferences.c	\
	main.c			\
	util.h			\
	util.c

eog_LDADD = 	\
	$(GCONF_LIBS)		\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)		\
	../libeog/libeog.la

idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -D__EOG_COMPILATION

uidir = $(datadir)/gnome/ui
UI_FILES = eog-shell-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = $(UI_FILES)
