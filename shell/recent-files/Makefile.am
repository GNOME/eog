NULL=

noinst_LIBRARIES = librecent.a

INCLUDES = $(EOG_CFLAGS)

EGG_RECENT_FILES =		\
	egg-recent.h		\
	egg-recent-model.c	\
	egg-recent-model.h	\
	egg-recent-item.c	\
	egg-recent-item.h       \
	egg-recent-util.c	\
	egg-recent-util.h	\
	egg-recent-view.c	\
	egg-recent-view.h	\
	egg-recent-view-gtk.c	\
	egg-recent-view-gtk.h	\
        egg-recent-view-bonobo.c\
        egg-recent-view-bonobo.h\
	$(NULL)

librecent_a_SOURCES =		\
	$(EGG_RECENT_FILES)	\
	$(NULL)

EXTRA_DIST =			\
	update-from-egg.sh	\
	$(NULL)

EGG_RECENT_DIR = $(srcdir)/../../../libegg/libegg/recent-files

regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGG_RECENT_DIR)" $(srcdir)/update-from-egg.sh
