INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-I. 						\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/libeog				\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
        $(GNOME_INCLUDEDIR)				\
	$(EXTRA_BONOBO_CFLAGS)				\
	$(EXTRA_WARNING_CFLAGS)				\
	$(GNOME_CFLAGS)

bin_PROGRAMS = eog-collection

eog_collection_SOURCES = \
	eog-image-list-model.c        	\
	eog-image-list-model.h        	\
	eog-image-selection-model.c  	\
	eog-image-selection-model.h  	\
	eog-collection-control.c        \
	eog-collection-control.h        \
	eog-collection-view.h		\
	eog-collection-view.c		\
	cimage.c			\
	cimage.h			\
	eog-image-loader.c		\
	eog-image-loader.h		\
	gnome-icon-item-factory.c    	\
	gnome-icon-item-factory.h    	\
	gnome-icon-list-model.c      	\
	gnome-icon-list-model.h      	\
	gnome-list-item-factory.c	\
	gnome-list-item-factory.h	\
	gnome-list-model.c		\
	gnome-list-model.h		\
	gnome-list-selection-model.c	\
	gnome-list-selection-model.h	\
	gnome-list-view.c		\
	gnome-list-view.h		\
	gnome-position-list-model.c	\
	gnome-position-list-model.h	\
	gnome-wrap-list.c		\
	gnome-wrap-list.h		\
	main.c

eog_collection_LDADD = 			\
	$(GNOME_LIBDIR)				\
	$(GNOME_LIBS)				\
	$(OAF_LIBS)				\
	$(INTLLIBS)				\
	$(EXTRA_BONOBO_LIBS)			\
	$(LIBPNG) $(LIBJPEG)			\
	../libeog/libeog.la

oafdir = $(datadir)/oaf
OAF_FILES = GNOME_EOG_Collection.oafinfo
oaf_DATA = $(OAF_FILES)

EXTRA_DIST = $(OAF_FILES)
