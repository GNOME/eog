INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-I. 						\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(top_srcdir)/src				\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
        $(GNOME_INCLUDEDIR)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	$(EXTRA_WARNING_CFLAGS)				\
	$(GNOME_CFLAGS)

bin_PROGRAMS = eog-shell

eog_shell_SOURCES =	\
	main.c			\
	eog-window.h		\
	eog-window.c

eog_shell_LDADD = 	\
	$(GNOME_LIBS)		\
	$(GNOME_LIBDIR)		\
	$(GNOME_LIBS)		\
	$(OAF_LIBS)		\
	$(INTLLIBS)		\
	$(EXTRA_BONOBO_LIBS)	\
	../src/libeog.la

uidir = $(datadir)/gnome/ui
ui_DATA = eog-shell-ui.xml


