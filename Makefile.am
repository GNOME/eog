if ENABLE_JPEG
jpeg_DIRS = jpegutils
endif

SUBDIRS = $(jpeg_DIRS) cut-n-paste bindings src plugins po help data

EXTRA_DIST = 			\
	HACKING			\
	MAINTAINERS		\
	ChangeLog-2.6		\
	ChangeLog-2.18		\
	intltool-extract.in	\
	intltool-merge.in 	\
	intltool-update.in	\
	gnome-doc-utils.make

DISTCLEANFILES =                \
        intltool-extract        \
        intltool-merge          \
        intltool-update         \
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --disable-scrollkeeper

# Ignore scrollkeeper issues for now.  @#*$& scrollkeeper
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper | grep -v /share/gnome/help/ | grep -v \.omf
