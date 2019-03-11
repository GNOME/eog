project(
  'eog', 'c',
  version: '3.32.0',
  license: 'GPL2+',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.44.0'
)

eog_version = meson.project_version()
version_array = eog_version.split('.')
eog_major_version = version_array[0].to_int()

eog_api_version = '@0@.0'.format(eog_major_version)
eog_api_name = '@0@-@1@'.format(meson.project_name(), eog_api_version)

eog_gir_ns = 'Eog'
eog_gir_version = '1.0'

eog_prefix = get_option('prefix')
eog_datadir = join_paths(eog_prefix, get_option('datadir'))
eog_includedir = join_paths(eog_prefix, get_option('includedir'))
eog_libdir = join_paths(eog_prefix, get_option('libdir'))
eog_localedir = join_paths(eog_prefix, get_option('localedir'))

eog_pkgdatadir = join_paths(eog_datadir, meson.project_name())
eog_pkglibdir = join_paths(eog_libdir, meson.project_name())
eog_pkglibexecdir = join_paths(eog_prefix, get_option('libexecdir'), meson.project_name())

eog_pluginsdir = join_paths(eog_pkglibdir, 'plugins')
eog_schemadir = join_paths(eog_datadir, 'glib-2.0', 'schemas')

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
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
if get_option('buildtype').contains('debug')
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

gio_unix_dep = dependency('gio-unix-2.0', version: glib_req_version)
gnome_desktop_dep = dependency('gnome-desktop-3.0', version: '>= 2.91.2')
libpeas_gtk_dep = dependency('libpeas-gtk-1.0', version: peas_req_version)

# runtime dependencies
dependency('gsettings-desktop-schemas', version: '>= 2.91.92')
dependency('shared-mime-info', version: '>= 0.20')

common_deps = [
  dependency('gdk-pixbuf-2.0', version: '>= 2.36.5'),
  dependency('gio-2.0', version: glib_req_version),
  dependency('glib-2.0', version: glib_req_version),
  dependency('gtk+-3.0', version: '>= 3.22.0'),
  dependency('libpeas-1.0', version: peas_req_version)
]

# ZLIB support (required)
if not cc.has_header('zlib.h') or not (cc.has_function('inflate') and cc.has_function('crc32'))
  zlib_dep = cc.find_library('z', required: false)
  assert(zlib_dep.found() and cc.has_function('inflate', dependencies: zlib_dep) and cc.has_function('crc32', dependencies: zlib_dep),
         'No sufficient zlib library found on your system')
endif

# EXIF (optional)
enable_libexif = get_option('libexif')
if enable_libexif
  libexif_dep = dependency('libexif', version: '>= 0.6.14', required: false)
  assert(libexif_dep.found() and cc.has_header('libexif/exif-data.h', dependencies: libexif_dep),
         'libexif support requested but library not found. Please use -Dlibexif=false')

  common_deps += libexif_dep
  config_h.set10('HAVE_EXIF', true)
endif

# Little CMS (optional)
enable_cms = get_option('cms')
if enable_cms
  common_deps += dependency('lcms2')
  config_h.set10('HAVE_LCMS', true)
endif

# Exempi (optional)
enable_xmp = get_option('xmp')
if enable_xmp
  common_deps += dependency('exempi-2.0', version: '>= 1.99.5')
  config_h.set10('HAVE_EXEMPI', true)
endif

# Jpeg (semi-optional)
enable_libjpeg = get_option('libjpeg')
if enable_libjpeg
  libjpeg_dep = dependency('libjpeg', required: false)
  assert(cc.has_function('jpeg_destroy_decompress', dependencies: libjpeg_dep) and cc.has_header('jpeglib.h', dependencies: libjpeg_dep),
         'libjpeg support requested but library not found. Please use -Dlibjpeg=false (some programs using GTK+ may not work properly)')

  if not cc.has_function('jpeg_simple_progression', dependencies: libjpeg_dep)
    message('JPEG library does not support progressive saving.')
  endif

  jpeg_80_check_src = '''
    #include <stdio.h>
    #include <jpeglib.h>
    #if JPEG_LIB_VERSION < 80
    #error "wrong version"
    #endif
  '''
  have_jpeg_80 = cc.compiles(jpeg_80_check_src, dependencies: libjpeg_dep, name: 'libjpeg version is 8 or greater')

  config_h.set10('HAVE_JPEG', true)
endif
config_h.set('HAVE_LIBJPEG', enable_libjpeg)

# introspection support
enable_introspection = get_option('introspection')
if enable_introspection
  gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.6.7')
endif
config_h.set('HAVE_INTROSPECTION', enable_introspection)

# RSVG (optional for scaling svg image)
enable_librsvg = get_option('librsvg')
if enable_librsvg
  common_deps += dependency('librsvg-2.0', version: '>= 2.36.2')
endif
config_h.set('HAVE_RSVG', enable_librsvg)

# libX11 (required for TotemScrSaver and Color Profiling)
gdk_dep = dependency('gdk-3.0', required: false)

enable_x11 = enable_cms or (gdk_dep.found() and gdk_dep.get_pkgconfig_variable('targets').contains('x11'))
if enable_x11
  x11_dep = dependency('x11')
endif

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

data_dir = join_paths(meson.source_root(), 'data')
po_dir = join_paths(meson.source_root(), 'po')

top_inc = include_directories('.')

subdir('data')

if enable_libjpeg
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
output += '  EXIF support ...............:  ' + enable_libexif.to_string() + '\n'
output += '  XMP support ................:  ' + enable_xmp.to_string() + '\n'
output += '  JPEG support ...............:  ' + enable_libjpeg.to_string() + '\n'
output += '  Colour management support ..:  ' + enable_cms.to_string() + '\n'
output += '  GObject Introspection.......:  ' + enable_introspection.to_string() + '\n'
message(output)
