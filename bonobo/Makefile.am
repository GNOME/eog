bin_PROGRAMS = bonobo-image-generic

bonobo_image_generic_SOURCES =	\
	bonobo-image-generic.c

INCLUDES =	\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)				\
	$(GTK_CFLAGS)

bonobo_image_generic_LDADD = 			\
	$(top_builddir)/bonobo/libbonobo.la	\
	$(GTK_LIBS)				\
	$(GNOME_LIBDIR)				\
	$(GNOMEGNORBA_LIBS)			\
	$(INTLLIBS)				\
	-lgdk_pixbuf

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = image-generic.gnorba

EXTRA_DIST = $(gnorba_DATA)

