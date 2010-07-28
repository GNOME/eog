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

gladedir = $(pkgdatadir)
glade_DATA = eog-image-properties-dialog.ui \
	     eog-multiple-save-as-dialog.ui \
	     eog-preferences-dialog.ui

uidir = $(datadir)/eog
UI_FILES = \
	eog-ui.xml \
	eog-toolbar.xml

ui_DATA = $(UI_FILES)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = eog.pc

gtkrcdir = $(pkgdatadir)
gtkrc_DATA = gtkrc

EXTRA_DIST =			\
        $(gtkrc_DATA)		\
        $(glade_DATA)		\
        $(DESKTOP_IN_FILES)	\
	eog.convert		\
	$(UI_FILES)

DISTCLEANFILES = 		\
        $(DESKTOP_FILES)        \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
