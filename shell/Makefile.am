
gladedir = $(datadir)/eog/glade

INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-I. 						\
	-I$(top_srcdir)/libeog				\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)				\
	$(EXTRA_GNOME_CFLAGS)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	-DEOG_GLADEDIR=\""$(gladedir)"\"

bin_PROGRAMS = eog-shell

eog_shell_SOURCES =	\
	e-dialog-widgets.h	\
	e-dialog-widgets.c	\
	eog-window.h		\
	eog-window.c		\
	eog-preferences.h	\
	eog-preferences.c	\
	main.c			\
	preferences.h		\
	preferences.c		\
	util.h			\
	util.c

eog_shell_LDADD = 	\
	$(GCONF_LIBS)		\
	$(EXTRA_GNOME_LIBS)	\
	$(EXTRA_BONOBO_LIBS)	\
	$(INTLLIBS)		\
	../libeog/libeog.la

glade_DATA =				\
	preferences-dialog.glade

glade_messages =			\
	preferences-dialog.glade.h

idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -D__EOG_COMPILATION

uidir = $(datadir)/gnome/ui
UI_FILES = eog-shell-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = \
	$(UI_FILES)		\
	$(glade_DATA)

#	$(glade_messages)	
