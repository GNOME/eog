dnl Configure script for the EOG Image Viewer

AC_PREREQ(2.57)
AC_INIT([eog], [2.19.2], [http://bugzilla.gnome.org/enter_bug.cgi?product=eog], [eog])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE(eog, 2.19.2)

GNOME_COMMON_INIT
GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS
GNOME_DEBUG_CHECK

AC_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

AC_CHECK_FUNCS(strptime)

# ***********
# Translation 
# ***********

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package.])

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)

# ***************
# gnome-doc-utils
# ***************

PKG_PROG_PKG_CONFIG
GNOME_DOC_INIT

# **********
# GNOME Libs 
# **********

GTK_REQUIRED=2.10.0
GLIB_REQUIRED=2.12.0
GNOME_VFS_REQUIRED=2.10.0
LIBGNOMEUI_REQUIRED=2.6.0
LIBGLADE_REQUIRED=2.3.6
GDKPIXBUF_REQUIRED=2.4.0
GCONF_REQUIRED=2.5.90
LIBART_REQUIRED=2.3.16
GNOME_DESKTOP_REQUIRED=2.10.0
GTK_PRINT_REQUIRED=2.10.0
GNOME_ICON_THEME_REQUIRED=2.17.1

EOG_MODULES="gtk+-2.0 >= $GTK_REQUIRED \
             glib-2.0 >= $GLIB_REQUIRED \
             gnome-vfs-2.0 >= $GNOME_VFS_REQUIRED \
             libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED \
             libglade-2.0 >= $LIBGLADE_REQUIRED \
             gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED \
             gconf-2.0 >= $GCONF_REQUIRED \
             libart-2.0 >= $LIBART_REQUIRED \
             gnome-desktop-2.0 >= $GNOME_DESKTOP_REQUIRED \
             gtk+-unix-print-2.0 >= $GTK_PRINT_REQUIRED \
	     gnome-icon-theme >= $GNOME_ICON_THEME_REQUIRED"

# ***************
# EXIF (optional)
# ***************

LIBEXIF_REQUIRED=0.6.12

AC_ARG_WITH([libexif], AC_HELP_STRING([--without-libexif], [disable EXIF support]))
have_exif=no
if test x$with_libexif != xno; then
    PKG_CHECK_MODULES(EXIF, libexif >= $LIBEXIF_REQUIRED, have_exif=yes, have_exif=no)
fi 
if test "x$have_exif" = "xyes"; then
  AC_DEFINE(HAVE_EXIF, 1, [EXIF Support.])
  EOG_MODULES="$EOG_MODULES libexif >= $LIBEXIF_REQUIRED"
fi

AM_CONDITIONAL([HAVE_EXIF], [test "x$have_exif" = "xyes"])

# *********************
# Little CMS (optional)
# *********************

AC_ARG_WITH([cms], AC_HELP_STRING([--without-cms], [disable colour management support]))
have_lcms=no
if test x$with_cms != xno; then
    PKG_CHECK_MODULES(LCMS, lcms, have_lcms=yes, have_lcms=no)
fi 
if test "x$have_lcms" = "xyes"; then
  AC_DEFINE(HAVE_LCMS, 1, [Little CMS Support.])
  EOG_MODULES="$EOG_MODULES lcms"
fi

# ********************
# Jpeg (semi-optional)
# ********************

AC_ARG_WITH(libjpeg, AC_HELP_STRING([--without-libjpeg], [disable special JPEG support]))
  have_jpeg=no
  if test x$with_libjpeg != xno && test -z "$LIBJPEG"; then
    AC_CHECK_LIB(jpeg, jpeg_destroy_decompress,
      have_jpeg=yes,
      have_jpeg=no
      AC_MSG_WARN(*** JPEG loader will not be built (JPEG library not found) ***))
    if test "$have_jpeg" = yes; then
      AC_MSG_CHECKING([for jpeglib.h])
      AC_TRY_CPP(
[#include <stdio.h>
#undef PACKAGE
#undef VERSION
#undef HAVE_STDLIB_H
#include <jpeglib.h>],
        have_jpeg=yes,
        have_jpeg=no)
      AC_MSG_RESULT($have_jpeg)
      if test "$have_jpeg" = yes; then
        LIBJPEG='-ljpeg'
	AC_DEFINE(HAVE_JPEG, 1, [Jpeg Support.])
        AC_DEFINE(HAVE_LIBJPEG, 1, [libjpeg is Present.])
        AC_CHECK_LIB(jpeg, jpeg_simple_progression,     
          AC_DEFINE(HAVE_PROGRESSIVE_JPEG, 1, [Jpeg Progressive Saving Support.]),
          AC_MSG_WARN(JPEG library does not support progressive saving.))
      else
          AC_MSG_WARN(*** JPEG loader will not be built (JPEG header file not found) ***)
      fi
    fi
  fi

  if test x$with_libjpeg != xno && test -z "$LIBJPEG"; then
     AC_MSG_ERROR([
*** Checks for JPEG loader failed. You can build without it by passing 
*** --without-libjpeg to configure but some programs using GTK+ may
*** not work properly])
  fi
AC_SUBST(LIBJPEG)
AM_CONDITIONAL(ENABLE_JPEG, test x$have_jpeg = xyes)

# ***************
# D-Bus activation
# ***************

DBUS_GLIB_REQUIRED=0.71

AC_ARG_WITH([dbus-glib-1], AC_HELP_STRING([--without-dbus], [disable dbus based activation]))
have_dbus=no
if test x$with_dbus != xno; then
    PKG_CHECK_MODULES([DBUS], [dbus-glib-1 >= $DBUS_GLIB_REQUIRED], have_dbus=yes, have_dbus=no)
fi
if test x$have_dbus = xyes; then
   AC_PATH_PROG([DBUS_BINDING_TOOL], [dbus-binding-tool], [no])

   if test x$DBUS_BINDING_TOOL = "xno" ; then
       AC_MSG_ERROR([dbus-binding-tool executable not found in your path - should be installed with dbus glib bindings])
   fi                                            

    AC_DEFINE(HAVE_DBUS, 1, [DBUS based activation.])
    EOG_MODULES="$EOG_MODULES dbus-glib-1 >= $DBUS_GLIB_REQUIRED"
fi

AM_CONDITIONAL([HAVE_DBUS], [test "x$have_dbus" = "xyes"])

# ******************
# GTK+ 2.12 Tooltips
# ******************

PKG_CHECK_MODULES(GTK_TOOLTIP, gtk+-2.0 >= 2.11.0, 
		  [HAVE_GTK_TOOLTIP=yes], [HAVE_GTK_TOOLTIP=no])
if test x$HAVE_GTK_TOOLTIP = xyes; then
   AC_DEFINE(HAVE_GTK_TOOLTIP, 1, [defined if GtkTooltip is available])
fi

# ****************
# CFLAGS/LIBS init
# ****************

PKG_CHECK_MODULES(EOG, $EOG_MODULES)
AC_SUBST(EOG_CFLAGS)
AC_SUBST(EOG_LIBS)

# *****
# GConf 
# *****

AM_GCONF_SOURCE_2

AC_PATH_PROG([GCONFTOOL], [gconftool-2], [no])

if test "$GCONFTOOL" = "no"; then
        AC_MSG_ERROR([gconftool-2 not found])
fi

# ************
# Sun Keyboard
# ************
AC_CHECK_HEADERS(X11/Sunkeysym.h, AC_DEFINE(HAVE_SUNKEYSYM_H, 1, [Sun Keyboard Support]))

AC_CONFIG_FILES([
Makefile
src/Makefile
jpegutils/Makefile
cut-n-paste/Makefile
cut-n-paste/toolbar-editor/Makefile
help/Makefile
help/eu/Makefile
help/it/Makefile
help/ja/Makefile
help/zh_CN/Makefile
help/zh_TW/Makefile
po/Makefile.in
data/Makefile
data/eog.pc
data/eog.desktop.in
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/apps/Makefile
data/icons/22x22/Makefile
data/icons/22x22/apps/Makefile
data/icons/24x24/Makefile
data/icons/24x24/apps/Makefile
data/icons/32x32/Makefile
data/icons/32x32/apps/Makefile
data/icons/scalable/Makefile
data/icons/scalable/apps/Makefile
])

AC_OUTPUT

echo "
Configure summary:

	Source code location .......:  ${srcdir}
	Compiler ...................:  ${CC} 
	Extra Compiler Warnings ....:  ${WARN_CFLAGS}

	EXIF support ...............:  ${have_exif}
	JPEG support ...............:  ${have_jpeg}
	Colour management support ..:  ${have_lcms}
	D-Bus activation............:  ${have_dbus}
"
