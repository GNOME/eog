
gladedir = $(datadir)/eog/glade

INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-I. 						\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
	$(EXTRA_GNOME_CFLAGS)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	-DEOG_GLADEDIR=\""$(gladedir)"\"

CORBA_SOURCE =			\
	Eog.h			\
	Eog-common.c		\
	Eog-stubs.c		\
	Eog-skels.c

bin_PROGRAMS = eog-shell

eog_shell_SOURCES =	\
	$(CORBA_SOURCE)		\
	e-dialog-widgets.h	\
	e-dialog-widgets.c	\
	eog-window.h		\
	eog-window.c		\
	main.c			\
	preferences.h		\
	preferences.c		\
	util.h			\
	util.c

eog_shell_LDADD = 	\
	$(GCONF_LIBS)		\
	$(EXTRA_GNOME_LIBS)	\
	$(EXTRA_BONOBO_LIBS)	\
	$(INTLLIBS)

glade_DATA =				\
	preferences-dialog.glade

glade_messages =			\
	preferences-dialog.glade.h


idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags = `$(GNOME_CONFIG) --cflags idl` -D__EOG_COMPILATION

$(CORBA_SOURCE): $(idls)
	$(ORBIT_IDL) $(top_srcdir)/idl/Eog.idl -I$(top_srcdir)/idl $(idl_flags)

Eog.h: $(idls)

BUILT_SOURCES=$(CORBA_SOURCE)

CLEANFILES += $(BUILT_SOURCES)

uidir = $(datadir)/gnome/ui
UI_FILES = eog-shell-ui.xml 
ui_DATA = $(UI_FILES)

EXTRA_DIST = \
	$(UI_FILES)		\
	$(glade_DATA)		\
	$(glade_messages)	
