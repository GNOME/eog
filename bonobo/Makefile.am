PRINT_LIBS = -lbonobo-print

OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)

bin_PROGRAMS = bonobo-image-generic

bonobo_image_generic_SOURCES =	\
	bonobo-image-generic.c

INCLUDES =	\
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DDATADIR=\""$(datadir)"\"			\
	-I$(srcdir) -I$(top_srcdir) 			\
	-I$(top_srcdir)/src				\
	-I. 						\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
        $(GNOME_INCLUDEDIR)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	$(GNOME_CFLAGS)

bonobo_image_generic_LDADD = 			\
	$(GNOME_LIBS)				\
	$(GNOME_LIBDIR)				\
	$(OBJECT_DIRECTORY_LIBS)		\
	$(INTLLIBS)				\
	$(PRINT_LIBS)				\
	$(EXTRA_BONOBO_LIBS)

oafdir = $(datadir)/oaf
OAF_FILES = image-generic.oafinfo
oaf_DATA = $(OAF_FILES)

EXTRA_DIST = $(OAF_FILES)
