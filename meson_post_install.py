#!/usr/bin/env python3

import os
import subprocess
import sys

if not os.environ.get('DESTDIR'):
  icondir = os.path.join(sys.argv[1], 'icons', 'hicolor')
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

  print('Compiling gsettings schemas...')
  subprocess.call(['glib-compile-schemas', sys.argv[2]])
