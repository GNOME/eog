if BONOBO

if GPRINT
PRINT_LIBS = -lbonobo-print
else
PRINT_LIBS =
endif

if USING_OAF
OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)
else
OBJECT_DIRECTORY_LIBS = $(GNOMEGNORBA_LIBS)
endif

bin_PROGRAMS = bonobo-image-generic

bonobo_image_generic_SOURCES =	\
	bonobo-image-generic.c	\
	io-png.c		\
	io-png.h

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
	$(OBJECT_DIRECTORY_LIBS)		\
	$(INTLLIBS)				\
	$(PRINT_LIBS)				\
	$(EXTRA_BONOBO_LIBS)
endif

gnorbadir = $(sysconfdir)/CORBA/servers
oafdir = $(datadir)/oaf


GOAD_FILES = image-generic.gnorba
OAF_FILES = image-generic.oafinfo

if BONOBO
if USING_OAF
oaf_DATA = $(OAF_FILES)
else
gnorba_DATA = $(GOAD_FILES)
endif
endif

EXTRA_DIST = $(GOAD_FILES) $(OAF_FILES)
