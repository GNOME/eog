SUBDIRS = intl macros po src bonobo doc

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = eog.schemas

EXTRA_DIST =		\
	DEPENDS		\
	HACKING		\
	MAINTAINERS	\
	TODO		\
	eog.desktop	\
	eog.spec	\
	eog.spec.in	\
	gnome-eog.png	\
	$(schema_DATA)

Graphicsdir = $(datadir)/gnome/apps/Graphics
Graphics_DATA = eog.desktop

icondir = $(datadir)/pixmaps
icon_DATA = gnome-eog.png

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_CONFIG_SOURCE) gconftool --makefile-install-rule $(top_srcdir)/$(schema_DATA)
