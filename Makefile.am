SUBDIRS = art idl libeog viewer doc shell po

EXTRA_DIST =			\
	DEPENDS			\
	HACKING			\
	MAINTAINERS		\
	TODO			\
	eog.schemas		\
	eog.desktop.in		\
	eog.spec		\
	eog.spec.in		\
	gnome-eog.png		\
	intltool-extract.in	\
	intltool-update.in	\
	intltool-merge.in

Graphicsdir = $(datadir)/gnome/apps/Graphics
Graphics_in_files = eog.desktop.in
Graphics_DATA = $(Graphics_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

icondir = $(datadir)/pixmaps
icon_DATA = gnome-eog.png

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		mkdir -p $(GCONF_SCHEMA_FILE_DIR); \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) gconftool --makefile-install-rule $(top_srcdir)/eog.schemas; \
	fi
