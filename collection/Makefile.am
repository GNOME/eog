INCLUDES =                                              \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DG_LOG_DOMAIN=\"Eog\"				\
	-DDATADIR=\""$(datadir)"\"			\
	-I. 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libeog				\
	-I$(top_builddir)/libeog			\
        -I$(includedir)                                 \
	$(GCONF_CFLAGS)					\
        $(GNOME_INCLUDEDIR)				\
	$(EXTRA_GNOME_CFLAGS)				\
	$(EXTRA_WARNING_CFLAGS)				\
	$(GNOME_CFLAGS)

bin_PROGRAMS = eog-collection

if ENABLE_EVOLUTION
EVOLUTION_GENERATED =			\
	Evolution-Composer.h		\
	Evolution-Composer-common.c	\
	Evolution-Composer-skels.c	\
	Evolution-Composer-stubs.c
endif

eog_collection_SOURCES =		\
	$(EVOLUTION_GENERATED)		\
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
	eog-item-factory.h		\
	eog-item-factory.c		\
	eog-item-factory-simple.h	\
	eog-item-factory-simple.c	\
	eog-collection-preferences.h	\
	eog-collection-preferences.c	\
	main.c

eog_collection_LDADD = 			\
	$(GNOME_LIBS)				\
	$(INTLLIBS)				\
	$(EXTRA_GNOME_LIBS)			\
	$(LIBPNG) $(LIBJPEG)			\
	../libeog/libeog.la

oafdir = $(datadir)/oaf

oaf_in_files = GNOME_EOG_Collection.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

if ENABLE_EVOLUTION
$(EVOLUTION_GENERATED):
	$(ORBIT_IDL) `$(GNOME_CONFIG) --datadir`/idl/Evolution-Composer.idl
endif

CLEANFILES += $(EVOLUTION_GENERATED)
EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)
