SUBDIRS = pixmaps icons

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES= eog.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_in_files = eog.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

gsettings_ENUM_NAMESPACE = org.gnome.eog
gsettings_ENUM_FILES = $(top_srcdir)/src/eog-scroll-view.h	\
                       $(top_srcdir)/src/eog-window.h
gsettings_SCHEMAS = org.gnome.eog.gschema.xml

@GSETTINGS_RULES@

gsettingsconvertdir = $(datadir)/GConf/gsettings
gsettingsconvert_DATA = eog.convert

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = eog.pc

EXTRA_DIST =			\
	fullscreen-toolbar.ui	\
	eog.css			\
	eog-app-menu.xml	\
	eog-gear-menu.ui	\
	eog-image-properties-dialog.ui	\
	eog-multiple-save-as-dialog.ui	\
	eog-preferences-dialog.ui \
	help-overlay.ui		\
	metadata-sidebar.ui	\
	popup-menus.ui		\
	$(DESKTOP_IN_FILES)	\
	$(appdata_in_files)	\
	eog.convert

CLEANFILES = 			\
	$(appdata_DATA)

DISTCLEANFILES = 		\
        $(DESKTOP_FILES)        \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
