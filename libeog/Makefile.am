SUBDIRS = stock cursors

INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-I. 						\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(top_srcdir)/src				\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
	$(EOG_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)


CORBA_SOURCES =			\
	Eog.h			\
	Eog-common.c		\
	Eog-stubs.c		\
	Eog-skels.c

noinst_LIBRARIES = libeog.a

libeog_a_SOURCES =			\
	$(CORBA_SOURCES)		\
	image-view.c			\
	image-view.h			\
	uta.c				\
	uta.h				\
	cursors.c			\
	cursors.h			\
	ui-image.c			\
	ui-image.h			\
	zoom.c				\
	zoom.h

idls =							\
	$(top_srcdir)/idl/Eog.idl

idl_flags =  -I$(BONOBO_IDLDIR) -I$(BONOBO_ACT_IDLDIR) -I$(top_srcdir)/idl -D__EOG_COMPILATION

$(CORBA_SOURCES): $(idls)
	$(ORBIT_IDL)   $(top_srcdir)/idl/Eog.idl $(idl_flags)

Eog.h: $(idls)

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_EOG.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

BUILT_SOURCES=$(CORBA_SOURCES)
CLEANFILES += $(BUILT_SOURCES) $(server_DATA)

EXTRA_DIST = $(server_in_files)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
