INCLUDES = -I$(top_srcdir) $(GPI_CFLAGS)	\
	-DTOP_SRCDIR=\"$(top_srcdir)\"		\
	-DGLADE_DIR=\"$(datadir)/$(PACKAGE)\"	\
	-DTOP_SRCDIR=\"$(top_srcdir)\"

noinst_LIBRARIES = libgpi.a
libgpi_a_SOURCES = 							\
	gpi-mgr.c 			gpi-mgr.h			\
	gpi-mgr-pixbuf.c		gpi-mgr-pixbuf.h		\
	gpi-dialog.c			gpi-dialog.h			\
	gpi-dialog-pixbuf.c		gpi-dialog-pixbuf.h		\
	gpi-dialog-pixbuf-page.c	gpi-dialog-pixbuf-page.h

noinst_PROGRAMS = test-gpi
test_gpi_SOURCES = test-gpi.c
test_gpi_LDADD = $(EOG_LIBS) libgpi.a

gladedir = $(datadir)/$(PACKAGE)
glade_DATA = gpi-dialog.glade

EXTRA_DIST = $(glade_DATA)
