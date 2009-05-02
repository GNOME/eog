# Fullscreen plugin
plugindir = $(libdir)/eog/plugins

INCLUDES = \
	-I$(top_srcdir)/src 				\
	$(EOG_CFLAGS) 					\
	$(WARN_CFLAGS)					\
	-DEOG_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

plugin_LTLIBRARIES = libfullscreen.la

libfullscreen_la_SOURCES = \
	eog-fullscreen-plugin.h				\
	eog-fullscreen-plugin.c

libfullscreen_la_LDFLAGS = \
	-avoid-version -module

libfullscreen_la_LIBADD = $(EOG_LIBS)

# Plugin Info

plugin_in_files = fullscreen.eog-plugin.desktop.in

%.eog-plugin: %.eog-plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.eog-plugin.desktop.in=.eog-plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
