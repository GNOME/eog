SUBDIRS = pixmaps icons

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES= eog.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

gsettings_ENUM_NAMESPACE = org.gnome.eog
gsettings_ENUM_FILES = $(top_srcdir)/src/eog-scroll-view.h	\
                       $(top_srcdir)/src/eog-window.h
gsettings_SCHEMAS = org.gnome.eog.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

gsettingsconvertdir = $(datadir)/GConf/gsettings
gsettingsconvert_DATA = eog.convert

uidir = $(pkgdatadir)
ui_DATA = eog-image-properties-dialog.ui \
	eog-multiple-save-as-dialog.ui \
	eog-preferences-dialog.ui \
	eog-app-menu.xml \
	eog-ui.xml \
	eog-toolbar.xml

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = eog.pc

styledir = $(pkgdatadir)
style_DATA = eog.css

EXTRA_DIST =			\
        $(style_DATA)		\
        $(ui_DATA)		\
        $(DESKTOP_IN_FILES)	\
	eog.convert

DISTCLEANFILES = 		\
        $(DESKTOP_FILES)        \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
