project(
  'eog', 'c',
  version: '3.27.90',
  license: 'GPL2+',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.43.0'
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
  ['PACKAGE_URL', 'https://wiki.gnome.org/Apps/EyeOfGnome'],
  ['VERSION', eog_version],
  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# support for nl_langinfo (_NL_MEASUREMENT_MEASUREMENT) (optional)
langinfo_measurement_src = '''
  #include <langinfo.h>
  int main() {
    char c;
    c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
  };
'''
config_h.set('HAVE__NL_MEASUREMENT_MEASUREMENT', cc.compiles(langinfo_measurement_src),
             description: 'Define if _NL_MEASUREMENT_MEASUREMENT is available')

# support for strptime
config_h.set('HAVE_STRPTIME', cc.has_function('strptime'))

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

compiler_flags = []
if eog_debug
  test_cflags = [
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

  compiler_flags += cc.get_supported_arguments(test_cflags)
endif

add_project_arguments(common_flags + compiler_flags, language: 'c')

ldflag = '-Wl,--version-script'
have_version_script = host_machine.system().contains('linux') and cc.has_argument(ldflag)

glib_req_version = '>= 2.53.4'
peas_req_version = '>= 0.7.4'

eog_deps = [
  dependency('gtk+-3.0', version: '>= 3.22.0'),
  dependency('glib-2.0', version: glib_req_version),
  dependency('gio-2.0', version: glib_req_version),
  dependency('gio-unix-2.0', version: glib_req_version),
  dependency('gnome-desktop-3.0', version: '>= 2.91.2'),
  dependency('gdk-pixbuf-2.0', version: '>= 2.36.5'),
  dependency('gtk+-unix-print-3.0', version: '>= 3.5.4'),
  dependency('shared-mime-info', version: '>= 0.20'),
  dependency('gsettings-desktop-schemas', version: '>= 2.91.92'),
  dependency('libpeas-1.0', version: peas_req_version),
  dependency('libpeas-gtk-1.0', version: peas_req_version),
  cc.find_library('m')
]

# ZLIB support (required)
have_zlib = false
if cc.has_header('zlib.h')
  zlib_dep = cc.find_library('z', required: false)
  have_zlib =  zlib_dep.found() and (cc.has_function('inflate', dependencies: zlib_dep) or cc.has_function('crc32', dependencies: zlib_dep))

  if have_zlib
    eog_deps += zlib_dep
  endif
endif
assert(have_zlib, 'No sufficient zlib library found on your system')

# EXIF (optional)
have_exif = false
if get_option('libexif')
  libexif_dep = dependency('libexif', version: '>= 0.6.14', required: false)
  have_exif = libexif_dep.found() and cc.has_header('libexif/exif-data.h', dependencies: libexif_dep)

  if have_exif
    eog_deps += libexif_dep
    config_h.set10('HAVE_EXIF', true,
                   description: 'EXIF Support.')
  endif
endif

# Little CMS (optional)
have_lcms = false
if get_option('cms')
  libcms_dep = dependency('lcms2', required: false)
  have_lcms = libcms_dep.found()

  if have_lcms
    eog_deps += libcms_dep
    config_h.set10('HAVE_LCMS', true,
                   description: 'Little CMS Support.')
  endif
endif

# Exempi (optional)
have_exempi = false
if get_option('xmp')
  libexempi_dep = dependency('exempi-2.0', version: '>= 1.99.5', required: false)
  have_exempi = libexempi_dep.found()

  if have_exempi
    eog_deps += libexempi_dep
    config_h.set10('HAVE_EXEMPI', true,
                   description: 'XMP Support.')
  endif
endif

# Jpeg (semi-optional)
jpeg_deps = []

have_jpeg = false
have_jpeg_80 = false

if get_option('libjpeg')
  libjpeg_dep = dependency('libjpeg', required: false)
  if libjpeg_dep.found()
    have_jpeg = cc.has_function('jpeg_destroy_decompress', dependencies: libjpeg_dep) and cc.has_header('jpeglib.h', dependencies: libjpeg_dep)

    if have_jpeg
      jpeg_deps += libjpeg_dep

      have_progressive = cc.has_function('jpeg_simple_progression', dependencies: libjpeg_dep)
      if not have_progressive
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
    error_msg += '*** You can build without it by passing -Dlibjpeg=false to\n'
    error_msg += '*** meson but some programs using GTK+ may not work properly\n'
    error(error_msg)
  endif

  config_h.set10('HAVE_JPEG', true,
                 description: 'Jpeg Support.')
endif
config_h.set('HAVE_LIBJPEG', have_jpeg,
             description: 'libjpeg is Present.')

# introspection support
have_gir = false
if get_option('introspection')
  gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.6.7', required: false)
  have_gir = gir_dep.found()

  if have_gir
    eog_deps += gir_dep
  endif
endif
config_h.set('HAVE_INTROSPECTION', have_gir)

# RSVG (optional for scaling svg image)
have_rsvg = false
if get_option('librsvg')
  librsvg_dep = dependency('librsvg-2.0', version: '>= 2.36.2', required: false)
  have_rsvg = librsvg_dep.found()

  if have_rsvg
    eog_deps += librsvg_dep
  endif
endif
config_h.set('HAVE_RSVG', have_rsvg,
             description: 'RSVG Support.')

# libX11 (required for TotemScrSaver and Color Profiling)
gdk_dep = dependency('gdk-3.0', required: false)

if have_lcms or (gdk_dep.found() and gdk_dep.get_pkgconfig_variable('targets').contains('x11'))
  libx11_dep = dependency('x11', required: false)
  assert(libx11_dep.found(), 'X development libraries (libX11) not found')
  eog_deps += libx11_dep
endif

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

if get_option('gtk_doc')
  subdir('doc/reference')
endif

subdir('po')

if get_option('installed_tests')
  subdir('tests')
endif

configure_file(
  output: 'config.h',
  configuration: config_h
)

meson.add_install_script(
  'meson_post_install.py',
  eog_datadir
)

output = 'Configure summary:\n\n'
output += '  Source code location .......:  ' + meson.source_root() + '\n'
output += '  Compiler ...................:  ' + cc.get_id() + '\n'
output += '  Extra Compiler Warnings ....:  ' + ' '.join(compiler_flags) + '\n'
output += '  EXIF support ...............:  ' + have_exif.to_string() + '\n'
output += '  XMP support ................:  ' + have_exempi.to_string() + '\n'
output += '  JPEG support ...............:  ' + have_jpeg.to_string() + '\n'
output += '  Colour management support ..:  ' + have_lcms.to_string() + '\n'
output += '  GObject Introspection.......:  ' + have_gir.to_string() + '\n'
message(output)
