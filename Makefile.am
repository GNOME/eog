if ENABLE_COLLECTION
collection_DIRS = collection
endif

SUBDIRS = art idl libeog viewer $(collection_DIRS) shell po help omf-install
DIST_SUBDIRS = $(SUBDIRS) collection

EXTRA_DIST =			\
	DEPENDS			\
	HACKING			\
	MAINTAINERS		\
	TODO			\
	$(schema_DATA)		\
	eog.desktop.in		\
	eog.spec		\
	eog.gladep		\
	eog.glade		\
	eog-glade.h		\
	eog.spec.in		\
	gnome-eog.png		\
	intltool-extract.in	\
	intltool-update.in	\
	intltool-merge.in	\
	xmldocs.make

Graphicsdir = $(datadir)/applications
Graphics_in_files = eog.desktop.in
Graphics_DATA = $(Graphics_in_files:.desktop.in=.desktop)

Gladedir = $(datadir)/eog/glade
Glade_DATA = eog.glade

@INTLTOOL_DESKTOP_RULE@

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = eog.schemas

icondir = $(datadir)/pixmaps
icon_DATA = gnome-eog.png

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/eog.schemas
