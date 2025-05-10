Eye of GNOME: an image viewing and cataloging program
-----------------------------------------------------

Perfect vision soup:

- 1 cauldron of snake broth
- 2 vampire ears
- 4 legs of tarantula
- 1 eye of gnome


Description
-----------

This is the Eye of GNOME, an image viewer program.  It is meant to be
a fast and functional image viewer.

Requirements
------------

This package requires the following modules to be installed: glib,
GTK, dconf, gnome-desktop, gio, gdk-pixbuf, shared-mime-info.

You can get these packages from https://download.gnome.org/sources/eog/
or from other sources where GNOME packages are distributed.
shared-mime-info can be downloaded from the freedesktop.org website.


Optional Libraries
------------------

'Eye of GNOME' supports the reading of EXIF information stored in
images by digital cameras. To get this working, you need the optional
libexif library. It is available at https://libexif.github.io/.
If you also want to preserve your EXIF data on save make sure you have
libjpeg installed, including the development files.

In order to make 'Eye of GNOME' work as a single instance application you'll
need D-Bus installed in your system. You can get it from
https://www.freedesktop.org/wiki/Software/dbus/ .

Other optional dependencies include Little cms for color management
and Exempi for XMP metadata reading.


Availability
------------

The bleeding-edge version of this package is always available from the GNOME
GIT repository (instructions at https://gitlab.gnome.org/GNOME/eog).
Released versions are available at https://download.gnome.org/sources/eog.

Webpage
-------

https://gitlab.gnome.org/GNOME/eog/


Reporting bugs
--------------

Please use the GNOME bug tracking system to report bugs.  You can
reach it at https://gitlab.gnome.org/GNOME/eog/issues.


License
-------

This program is released under the terms of the GNU General Public
License.  Please see the file COPYING for details.


Authors
-------

Maintainer: Lucas Rocha (lucasr@gnome.org)

- Felix Riemann (friemann@svn.gnome.org)
- Claudio Saaevedra (csaavedra@igalia.com)
- Tim Gerla (tim+gnomebugs@gerla.net)
- Arik Devens (arik@gnome.org)
- Federico Mena-Quintero (federico@gnome.org)
- Jens Finke (jens@gnome.org)
- Lutz Müller (urc8@rz.uni-karlsruhe.de)
- Martin Baulig (martin@gnome.org)
- Michael Meeks (michael@ximian.com)
