SUBDIRS = pixmaps icons

# .desktop file
DESKTOP_IN_FILES= eog.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

$(desktop_DATA): %.desktop: %.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

# AppData
appdatadir = $(datadir)/appdata
appdata_in_files = eog.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

# GSettings
gsettings_ENUM_NAMESPACE = org.gnome.eog
gsettings_ENUM_FILES = $(top_srcdir)/src/eog-scroll-view.h	\
                       $(top_srcdir)/src/eog-window.h
gsettings_SCHEMAS = org.gnome.eog.gschema.xml

@GSETTINGS_RULES@

gsettingsconvertdir = $(datadir)/GConf/gsettings
gsettingsconvert_DATA = eog.convert

# pkg-config
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = eog.pc


EXTRA_DIST =			\
	fullscreen-toolbar.ui	\
	eog.css			\
	eog-app-menu.ui		\
	eog-gear-menu.ui	\
	eog-image-properties-dialog.ui	\
	eog-multiple-save-as-dialog.ui	\
	eog-preferences-dialog.ui \
	eog-zoom-entry.ui	\
	help-overlay.ui		\
	metadata-sidebar.ui	\
	popup-menus.ui		\
	$(DESKTOP_IN_FILES)	\
	$(appdata_in_files)	\
	eog.convert

CLEANFILES = 			\
        $(DESKTOP_FILES)        \
	$(appdata_DATA)

DISTCLEANFILES = 		\
        $(DESKTOP_FILES)        \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
