INCLUDES = -I$(top_srcdir) $(GPI_CFLAGS)	\
	-DTOP_SRCDIR=\"$(top_srcdir)\"		\
	-DGLADE_DIR=\"$(datadir)/$(PACKAGE)\"

noinst_LIBRARIES = libgpi.a
libgpi_a_SOURCES = 							\
	gpi-mgr.c 			gpi-mgr.h			\
	gpi-mgr-pixbuf.c		gpi-mgr-pixbuf.h		\
	gpi-dialog.c			gpi-dialog.h			\
	gpi-dialog-pixbuf.c		gpi-dialog-pixbuf.h		\
	gpi-dialog-pixbuf-page.c	gpi-dialog-pixbuf-page.h

gladedir = $(datadir)/$(PACKAGE)
glade_DATA = gpi-dialog.glade

EXTRA_DIST = $(glade_DATA)
