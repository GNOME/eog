if ENABLE_JPEG
jpeg_DIRS = jpegutils
endif

SUBDIRS = art $(jpeg_DIRS) libeog shell po help
DIST_SUBDIRS = $(SUBDIRS) jpegutils

EXTRA_DIST = 			\
	DEPENDS			\
	HACKING			\
	MAINTAINERS		\
	TODO			\
	$(schemas_in_files)	\
	$(desktop_in_files)	\
	eog.spec		\
	eog.gladep		\
	eog.glade		\
	eog.spec.in		\
	gnome-eog.png		\
	intltool-extract.in	\
	intltool-update.in	\
	intltool-merge.in	

desktopdir = $(datadir)/applications
desktop_in_files = eog.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

gladedir = $(datadir)/eog/glade
glade_DATA = eog.glade

@INTLTOOL_DESKTOP_RULE@

schemasdir = $(sysconfdir)/gconf/schemas
schemas_in_files = eog.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

icondir = $(datadir)/pixmaps
icon_DATA = gnome-eog.png

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/eog.schemas
else
install-data-local:
endif
