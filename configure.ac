dnl Configure script for the EOG Image Viewer

AC_PREREQ(2.57)
AC_INIT(eog, 2.17.1)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE(eog, 2.15.1)

GNOME_COMMON_INIT
GNOME_DOC_INIT
GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS
GNOME_DEBUG_CHECK

AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

PKG_PROG_PKG_CONFIG

# ***********
# Translation 
# ***********

ALL_LINGUAS="`grep -v '^#' "$srcdir/po/LINGUAS"`"
AC_SUBST([CONFIG_STATUS_DEPENDENCIES],['$(top_srcdir)/po/LINGUAS'])

AM_GLIB_GNU_GETTEXT
AC_PROG_INTLTOOL

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package.])

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)

# **********
# GNOME Libs 
# **********

GTK_REQUIRED=2.8.0
GLIB_REQUIRED=2.8.0
GNOME_VFS_REQUIRED=2.10.0
LIBGNOMEUI_REQUIRED=2.6.0
LIBGLADE_REQUIRED=2.3.6
GDKPIXBUF_REQUIRED=2.4.0
LIBGNOMECANVAS_REQUIRED=2.5.92
GCONF_REQUIRED=2.5.90
LIBART_REQUIRED=2.3.16
GNOME_DESKTOP_REQUIRED=2.10.0

EOG_MODULES="gtk+-2.0 >= $GTK_REQUIRED \
             glib-2.0 >= $GLIB_REQUIRED \
             gnome-vfs-2.0 >= $GNOME_VFS_REQUIRED \
             libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED \
             libglade-2.0 >= $LIBGLADE_REQUIRED \
             gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED \
             libgnomecanvas-2.0 >= $LIBGNOMECANVAS_REQUIRED \
             gconf-2.0 >= $GCONF_REQUIRED \
             libart-2.0 >= $LIBART_REQUIRED \
             gnome-desktop-2.0 >= $GNOME_DESKTOP_REQUIRED"

# ***************
# EXIF (optional)
# ***************

LIBEXIF_REQUIRED=0.6.12

AC_ARG_WITH(libexif, [  --without-libexif	  disable EXIF support])
have_exif=no
if test x$with_libexif != xno; then
    PKG_CHECK_MODULES(EXIF, libexif >= $LIBEXIF_REQUIRED, have_exif=yes, have_exif=no)
fi 
if test "x$have_exif" = "xyes"; then
  AC_DEFINE(HAVE_EXIF, 1, [EXIF Support.])
  EOG_MODULES="$EOG_MODULES libexif >= $LIBEXIF_REQUIRED"
fi

# *********************
# Little CMS (optional)
# *********************

AC_ARG_WITH(cms, [  --without-cms	          disable colour management support])
have_lcms=no
if test x$with_cms != xno; then
    PKG_CHECK_MODULES(LCMS, lcms, have_lcms=yes, have_lcms=no)
fi 
if test "x$have_lcms" = "xyes"; then
  AC_DEFINE(HAVE_LCMS, 1, [Little CMS Support.])
  EOG_MODULES="$EOG_MODULES lcms"
fi

# *******************
# Printing (optional)
# *******************

LIBGNOMEPRINTUI_REQUIRED=2.5.1

AC_ARG_WITH(gnome_print, [  --without-gnome-print	  disable print support])
have_gnome_print=no
if test x$with_gnome_print != xno; then
    PKG_CHECK_MODULES(GNOME_PRINT,
                      libgnomeprintui-2.2 >= $LIBGNOMEPRINTUI_REQUIRED,
                      have_gnome_print=yes, have_gnome_print=no)
fi 
if test "x$have_gnome_print" = "xyes"; then
  AC_DEFINE(HAVE_GNOME_PRINT, 1, [Printing Support.])
  EOG_MODULES="$EOG_MODULES libgnomeprintui-2.2"
fi

# ********************
# Jpeg (semi-optional)
# ********************

AC_ARG_WITH(libjpeg, [  --without-libjpeg	  disable special JPEG support])
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
data/Makefile
data/eog.desktop.in
art/Makefile
libeog/Makefile
libeog/cursors/Makefile
jpegutils/Makefile
help/Makefile
shell/Makefile
shell/recent-files/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
Configure summary:

	Source code location .......:  ${srcdir}
	Compiler ...................:  ${CC} 
	Extra Compiler Warnings ....:  ${WARN_CFLAGS}

	Printing support ...........:  ${have_gnome_print}
	EXIF support ...............:  ${have_exif}
	JPEG support ...............:  ${have_jpeg}
	Colour management support ..:  ${have_lcms}
"
