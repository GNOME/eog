@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

DESKTOP_IN_FILES= eog.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

schemas_in_files = eog.schemas.in
schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

gladedir = $(pkgdatadir)
glade_DATA = eog.glade

uidir = $(datadir)/eog
UI_FILES = eog-gtk-ui.xml 
ui_DATA = $(UI_FILES)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $(schemas_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p >&1 > /dev/null; \
	done \
	fi
endif

EXTRA_DIST =			\
        $(glade_DATA)		\
        $(DESKTOP_IN_FILES)	\
        $(schemas_in_files)	\
	$(UI_FILES)

DISTCLEANFILES = 		\
        $(DESKTOP_FILES)        \
        $(schemas_DATA)	
