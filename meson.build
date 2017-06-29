project(
  'eog', 'c',
  version: '3.25.2',
  license: 'GPL2',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=1'
  ],
  meson_version: '>= 0.42.0'
)

eog_version = meson.project_version()
version_array = eog_version.split('.')
eog_major_version = version_array[0].to_int()
eog_minor_version = version_array[1].to_int()
eog_micro_version = version_array[2].to_int()

eog_api_version = '@0@.0'.format(eog_major_version)
eog_api_name = '@0@-@1@'.format(meson.project_name(), eog_api_version)

eog_gir_ns = 'Eog'
eog_gir_version = '1.0'

eog_prefix = get_option('prefix')
eog_bindir = join_paths(eog_prefix, get_option('bindir'))
eog_datadir = join_paths(eog_prefix, get_option('datadir'))
eog_includedir = join_paths(eog_prefix, get_option('includedir'))
eog_libdir = join_paths(eog_prefix, get_option('libdir'))
eog_libexecdir = join_paths(eog_prefix, get_option('libexecdir'))
eog_localedir = join_paths(eog_prefix, get_option('localedir'))

eog_pkgdatadir = join_paths(eog_datadir, meson.project_name())
eog_pkglibdir = join_paths(eog_libdir, meson.project_name())
eog_pkglibexecdir = join_paths(eog_libexecdir, meson.project_name())

eog_pluginsdir = join_paths(eog_pkglibdir, 'plugins')
eog_schemadir = join_paths(eog_datadir, 'glib-2.0', 'schemas')

eog_debug = get_option('buildtype').contains('debug')

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_BUGREPORT', 'http://bugzilla.gnome.org/enter_bug.cgi?product=' + meson.project_name()],
  ['PACKAGE_NAME', meson.project_name()],
  ['PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), eog_version)],
  ['PACKAGE_TARNAME', meson.project_name()],
  ['PACKAGE_URL', 'https://wiki.gnome.org/Apps/EyeOfGnome'],
  ['PACKAGE_VERSION', eog_version],
  ['VERSION', eog_version],
  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# version values
config_h.set('EOG_MAJOR_VERSION', eog_major_version)
config_h.set('EOG_MINOR_VERSION', eog_minor_version)
config_h.set('EOG_MICRO_VERSION', eog_micro_version)

# debug options
config_h.set('GNOME_ENABLE_DEBUG', eog_debug,
             description: 'Define if debugging is enabled')
config_h.set('NDEBUG', not eog_debug)


# Support for nl_langinfo (_NL_MEASUREMENT_MEASUREMENT) (optional)
langinfo_measurement_src = '''
  #include <langinfo.h>
  int main() {
    char c;
    c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
  };
'''
config_h.set('HAVE__NL_MEASUREMENT_MEASUREMENT', cc.compiles(langinfo_measurement_src),
             description: 'Define if _NL_MEASUREMENT_MEASUREMENT is available')

# headers
check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_UNISTD_H', 'unistd.h']
]

foreach header: check_headers
  config_h.set(header[0], cc.has_header(header[1]))
endforeach

# functions
# functions
check_functions = [
  ['HAVE_STRPTIME', 'strptime'],
  # i18n
  ['HAVE_DCGETTEXT', 'dcgettext'],
  ['HAVE_GETTEXT', 'gettext'],
  ['HAVE_ICONV', 'iconv']
]

if host_machine.system().contains('darwin')
  check_functions += [
    ['HAVE_CFLOCALECOPYCURRENT', 'CFLocaleCopyCurrent'],
    ['HAVE_CFPREFERENCESCOPYAPPVALUE', 'CFPreferencesCopyAppValue']
  ]
endif

foreach func: check_functions
  config_h.set(func[0], cc.has_function(func[1]))
endforeach

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

test_cflags = []
if eog_debug
  test_cflags += [
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Werror=return-type',
    '-Wnested-externs',
    '-Wstrict-prototypes'
  ]

  foreach cflag: test_cflags
    if cc.has_argument(cflag)
      common_flags += [cflag]
    endif
  endforeach
endif

add_project_arguments(common_flags, language: 'c')

glib_req_version = '>= 2.42.0'
peas_req_version = '>= 0.7.4'

eog_deps = [
  dependency('gtk+-3.0', version: '>= 3.22.0'),
  dependency('glib-2.0', version: glib_req_version),
  dependency('gio-2.0', version: glib_req_version),
  dependency('gio-unix-2.0', version: glib_req_version),
  dependency('gnome-desktop-3.0', version: '>= 2.91.2'),
  dependency('gdk-pixbuf-2.0', version: '>= 2.30.0'),
  dependency('gtk+-unix-print-3.0', version: '>= 3.5.4'),
  dependency('shared-mime-info', version: '>= 0.20'),
  dependency('gsettings-desktop-schemas', version: '>= 2.91.92'),
  dependency('libpeas-1.0', version: peas_req_version),
  dependency('libpeas-gtk-1.0', version: peas_req_version)
]

# ZLIB support (required)
have_zlib = false
if cc.has_header('zlib.h')
  config_h.set('HAVE_ZLIB_H', true)
  zlib_dep = cc.find_library('z', required: false)
  if zlib_dep.found() and (cc.has_function('inflate', dependencies: zlib_dep) or cc.has_function('crc32', dependencies: zlib_dep))
    have_zlib = true
    config_h.set('HAVE_LIBZ', true)
    eog_deps += zlib_dep
  endif
endif

assert(have_zlib, 'No sufficient zlib library found on your system')

# EXIF (optional)
have_exif = false
if get_option('enable-libexif')
  libexif_dep = dependency('libexif', version: '>= 0.6.14', required: false)
  if libexif_dep.found() and cc.has_header('libexif/exif-data.h', dependencies: libexif_dep)
    have_exif = true
    config_h.set('HAVE_EXIF', 1,
                 description: 'EXIF Support.')
    eog_deps += libexif_dep
  endif
endif

# Little CMS (optional)
have_lcms = false
if get_option('enable-cms')
  libcms_dep = dependency('lcms2', required: false)
  if libcms_dep.found()
    have_lcms = true
    config_h.set('HAVE_LCMS', 1,
                 description: 'Little CMS Support.')
    eog_deps += libcms_dep
  endif
endif

# Exempi (optional)
have_exempi = false
if get_option('enable-xmp')
  libexempi_dep = dependency('exempi-2.0', version: '>= 1.99.5', required: false)
  if libexempi_dep.found()
    have_exempi = true
    config_h.set('HAVE_EXEMPI', 1,
                 description: 'XMP Support.')
    eog_deps += libexempi_dep
  endif
endif

# Jpeg (semi-optional)
jpeg_deps = []

have_jpeg = false
have_jpeg_80 = false

libjpeg_option = get_option('enable-libjpeg')
if libjpeg_option
  libjpeg_dep = dependency('libjpeg', required: false)
  if libjpeg_dep.found()
    if cc.has_function('jpeg_destroy_decompress', dependencies: libjpeg_dep) and cc.has_header('jpeglib.h', dependencies: libjpeg_dep)
      have_jpeg = true
      config_h.set('HAVE_JPEG', 1,
                   description: 'Jpeg Support.')
      config_h.set('HAVE_LIBJPEG', true,
                   description: 'libjpeg is Present.')
      jpeg_deps += libjpeg_dep

      if cc.has_function('jpeg_simple_progression', dependencies: libjpeg_dep)
        config_h.set('HAVE_PROGRESSIVE_JPEG', true,
                     description: 'Jpeg Progressive Saving Support.')
      else
        message('JPEG library does not support progressive saving.')
      endif

      message('Checking libjpeg version is 8 or greater')
      jpeg_80_check_src = '''
        #include <stdio.h>
        #include <jpeglib.h>
        #if JPEG_LIB_VERSION < 80
        #error "wrong version"
        #endif
      '''
      have_jpeg_80 = cc.compiles(jpeg_80_check_src, dependencies: libjpeg_dep)
    else
      error_msg = '*** JPEG loader will not be built (JPEG header file not found) ***\n'
    endif
  else
    error_msg = '*** JPEG loader will not be built (JPEG library not found) ***\n'
  endif

  if not have_jpeg
    error_msg += '*** You can build without it by passing --without-libjpeg to\n'
    error_msg += '*** configure but some programs using GTK+ may not work properly\n'
    error(error_msg)
  endif
endif

# introspection support
have_gir = false
if get_option('enable-introspection')
  gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.6.7', required: false)
  if gir_dep.found()
    have_gir = true
    config_h.set('HAVE_INTROSPECTION', true)
    eog_deps += gir_dep
  endif
endif

# RSVG (optional for scaling svg image)
have_rsvg = false
if get_option('enable-librsvg')
  librsvg_dep = dependency('librsvg-2.0', version: '>= 2.36.2', required: false)
  if librsvg_dep.found()
    have_rsvg = true
    config_h.set('HAVE_RSVG', true,
                 description: 'RSVG Support.')
    eog_deps += librsvg_dep
  endif
endif

# libX11 (required for TotemScrSaver and Color Profiling)
gdk_dep = dependency('gdk-3.0', required: false)

have_x11 = false
if have_lcms or (gdk_dep.found() and gdk_dep.get_pkgconfig_variable('targets').contains('x11'))
  libx11_dep = dependency('x11', required: false)
  assert(libx11_dep.found(), 'X development libraries (libX11) not found')
  have_x11 = true
  eog_deps += libx11_dep
endif

configure_file(
  output: 'config.h',
  configuration: config_h
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

data_dir = join_paths(meson.source_root(), 'data')
po_dir = join_paths(meson.source_root(), 'po')

top_inc = include_directories('.')

subdir('data')

if have_jpeg
  subdir('jpegutils')
endif

subdir('src')
subdir('plugins')
subdir('help')

if get_option('enable-gtk-doc')
  subdir('doc/reference')
endif

subdir('po')

if get_option('enable-installed-tests')
  subdir('tests')
endif

meson.add_install_script('meson_post_install.py')

output = 'Configure summary:\n\n'
output += '  Source code location .......:  ' + meson.source_root() + '\n'
output += '  Compiler ...................:  ' + cc.get_id() + '\n'
output += '  Extra Compiler Warnings ....:  ' + ' '.join(test_cflags) + '\n'
output += '  EXIF support ...............:  ' + have_exif.to_string() + '\n'
output += '  XMP support ................:  ' + have_exempi.to_string() + '\n'
output += '  JPEG support ...............:  ' + have_jpeg.to_string() + '\n'
output += '  Colour management support ..:  ' + have_lcms.to_string() + '\n'
output += '  GObject Introspection.......:  ' + have_gir.to_string() + '\n'
message(output)
