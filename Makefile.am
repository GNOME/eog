SUBDIRS = art idl libeog viewer doc shell collection po

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
	eog.schemas		\
	xml-i18n-extract.in	\
	xml-i18n-update.in	\
	xml-i18n-merge.in

Graphicsdir = $(datadir)/gnome/apps/Graphics
Graphics_in_files = eog.desktop.in
Graphics_DATA = $(Graphics_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

icondir = $(datadir)/pixmaps
icon_DATA = gnome-eog.png

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_CONFIG_SOURCE) gconftool --makefile-install-rule $(top_srcdir)/eog.schemas
