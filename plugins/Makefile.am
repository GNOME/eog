AUTOMAKE_OPTIONS = subdir-objects

pluginsdir = $(libdir)/eog/plugins

AM_CPPFLAGS = \
	-I$(top_srcdir)/src 				\
	$(EOG_CFLAGS) 					\
	$(WARN_CFLAGS)					\
	-DEOG_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

AM_LDFLAGS = -avoid-version -module

plugins_LTLIBRARIES = \
	fullscreen/libfullscreen.la  \
	reload/libreload.la

plugins_in_files = \
	fullscreen/fullscreen.plugin.desktop.in  \
	reload/reload.plugin.desktop.in

# Fullscreen plugin
fullscreen_libfullscreen_la_SOURCES = \
	fullscreen/eog-fullscreen-plugin.h  \
	fullscreen/eog-fullscreen-plugin.c

fullscreen_libfullscreen_la_LIBADD = \
	$(top_builddir)/src/libeog.la	\
	$(EOG_LIBS)

# Reload plugin
reload_libreload_la_SOURCES = \
	reload/eog-reload-plugin.h  \
	reload/eog-reload-plugin.c

reload_libreload_la_LIBADD = \
	$(top_builddir)/src/libeog.la	\
	$(EOG_LIBS)

# Statusbar Date Plugin
if HAVE_EXIF
plugins_LTLIBRARIES += statusbar-date/libstatusbar-date.la

statusbar_date_libstatusbar_date_la_SOURCES = \
	statusbar-date/eog-statusbar-date-plugin.h  \
	statusbar-date/eog-statusbar-date-plugin.c

statusbar_date_libstatusbar_date_la_LIBADD = \
	$(top_builddir)/src/libeog.la	\
	$(EOG_LIBS)

plugins_in_files += statusbar-date/statusbar-date.plugin.desktop.in
endif

# Generate plugin info files
plugins_DATA = $(plugins_in_files:.plugin.desktop.in=.plugin)

# msgfmt 0.19.8 has a bug that adding additional keywords for translation
# disables the default keywords, so we have to explicitly name them for now.
$(plugins_DATA): %.plugin: %.plugin.desktop.in $(wildcard $(top_srcdir)/po/*po)
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword=Name --keyword=Description --template $< -d $(top_srcdir)/po -o $@



DISTCLEANFILES = $(plugins_DATA)
CLEANFILES = $(plugins_DATA)

EXTRA_DIST = $(plugins_in_files)

-include $(top_srcdir)/git.mk
