SUBDIRS = art idl libgpi libeog viewer collection shell po help

ACLOCAL_AMFLAGS = -I /usr/share/aclocal/gnome2-macros

EXTRA_DIST = 			\
	DEPENDS			\
	HACKING			\
	MAINTAINERS		\
	TODO			\
	$(schemas_in_files)	\
	$(Graphics_in_files)	\
	eog.spec		\
	eog.gladep		\
	eog.glade		\
	eog.spec.in		\
	gnome-eog.png		\
	intltool-extract.in	\
	intltool-update.in	\
	intltool-merge.in	

Graphicsdir = $(datadir)/applications
Graphics_in_files = eog.desktop.in
Graphics_DATA = $(Graphics_in_files:.desktop.in=.desktop)

Gladedir = $(datadir)/eog/glade
Glade_DATA = eog.glade

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
