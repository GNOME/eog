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
	eog-wrap-list.c			\
	eog-wrap-list.h			\
	eog-collection-control.c        \
	eog-collection-control.h        \
	eog-collection-view.h		\
	eog-collection-view.c		\
	cimage.c			\
	cimage.h			\
	eog-image-loader.c		\
	eog-image-loader.h		\
	eog-collection-model.h		\
	eog-collection-model.c		\
	gnome-icon-item-factory.c    	\
	gnome-icon-item-factory.h    	\
	gnome-list-item-factory.c	\
	gnome-list-item-factory.h	\
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

oaf_in_files = GNOME_EOG_Collection.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)
