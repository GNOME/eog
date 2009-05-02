if ENABLE_JPEG
jpeg_DIRS = jpegutils
endif

SUBDIRS = $(jpeg_DIRS) cut-n-paste bindings src plugins po help data doc

EXTRA_DIST = 			\
	HACKING			\
	MAINTAINERS		\
	ChangeLog-2.6		\
	ChangeLog-2.18		\
	gnome-doc-utils.make

DISTCLEANFILES =                \
        intltool-extract        \
        intltool-merge          \
        intltool-update         \
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --disable-scrollkeeper --enable-gtk-doc

# Ignore scrollkeeper issues for now.  @#*$& scrollkeeper
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper | grep -v /share/gnome/help/ | grep -v \.omf

MAINTAINERCLEANFILES = \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make

-include $(top_srcdir)/git.mk
