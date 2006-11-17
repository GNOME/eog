#
# FIXME:
#  currently scrollkeeper-update does not seem to
#  remove its database when there is no useful
#  information contained there. Therefore, with
#  "make uninstall" you're left with loads of
#  stuff in /var/scrollkeeper and distcheck
#  fails.
#
#  http://mail.gnome.org/archives/gnome-doc-list/2003-May/msg00008.html
#
#  This overrides the uninstall check so distcheck
#  passes
#
# distuninstallcheck_listfiles = find . -type f -print | grep -v 'scrollkeeper' | grep -v 'omf' | grep -v 'figures'
distuninstallcheck:
        @:

distcleancheck_listfiles = find . -type f -print | grep -v 'omf\.out' | grep -v 'legal.xml'

if ENABLE_JPEG
jpeg_DIRS = jpegutils
endif

SUBDIRS = $(jpeg_DIRS) libeog shell po help
DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = 			\
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
	intltool-merge.in	\
	gnome-doc-utils.make

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
# Work around MSYS feature
install-data-local:
	if test "$$MSYSBIN" ; then \
		(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE); \
		 echo $(GCONFTOOL) --makefile-install-rule $(top_builddir)/eog.schemas) > _temp.bat; \
		cmd /c _temp.bat; \
		rm _temp.bat; \
	else \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/eog.schemas; \
	fi
else
install-data-local:
endif

DISTCLEANFILES =                \
        intltool-extract        \
        intltool-merge          \
        intltool-update         \
        eog.desktop             \
        eog.schemas		\
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
