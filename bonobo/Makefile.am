if BONOBO
bin_PROGRAMS = bonobo-image-generic

bonobo_image_generic_SOURCES =	\
	bonobo-image-generic.c

INCLUDES =	\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-I$(srcdir) -I$(top_srcdir) 			\
	-I. 						\
        -I$(includedir)                                 \
        $(GNOME_INCLUDEDIR)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	$(GNOME_CFLAGS)

bonobo_image_generic_LDADD = 			\
	$(GNOME_LIBS)				\
	$(GNOME_LIBDIR)				\
	$(GNOMEGNORBA_LIBS)			\
	$(INTLLIBS)				\
	$(EXTRA_BONOBO_LIBS)

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = image-generic.gnorba
endif

EXTRA_DIST = $(gnorba_DATA)

