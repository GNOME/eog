SUBDIRS = intl macros po src idl libeog viewer doc shell collection

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = eog.schemas

EXTRA_DIST =		\
	DEPENDS		\
	HACKING		\
	MAINTAINERS	\
	TODO		\
	eog.desktop.in	\
	eog.spec	\
	eog.spec.in	\
	gnome-eog.png	\
	$(schema_DATA)	\
	xml-i18n-extract.in	\
	xml-i18n-update.in	\
	xml-i18n-merge.in

Graphicsdir = $(datadir)/gnome/apps/Graphics
Graphics_in_files = eog.desktop.in
Graphics_DATA = $(Graphics_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

icondir = $(datadir)/pixmaps
icon_DATA = gnome-eog.png

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_CONFIG_SOURCE) gconftool --makefile-install-rule $(top_srcdir)/$(schema_DATA)
