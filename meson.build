project(
  'eog', 'c',
  version: '47.1',
  license: 'GPL2+',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.58.0',
)

eog_name = meson.project_name()
eog_version = meson.project_version()
version_array = eog_version.split('.')
eog_major_version = version_array[0].to_int()

eog_api_version = '3.0'
eog_api_name = f'@eog_name@-@eog_api_version@'

eog_gir_ns = 'Eog'
eog_gir_version = '1.0'

eog_prefix = get_option('prefix')
eog_datadir = get_option('datadir')
eog_libdir = get_option('libdir')
eog_localedir = get_option('localedir')

eog_pkgdatadir = eog_datadir / eog_name
eog_pkglibdir = eog_libdir / eog_name
eog_pkglibexecdir = get_option('libexecdir') / eog_name

eog_pluginsdir = eog_pkglibdir / 'plugins'

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

source_root = meson.current_source_dir()

data_dir = source_root / 'data'
po_dir = source_root / 'po'

top_inc = include_directories('.')

cc = meson.get_compiler('c')

config_h = configuration_data()

if get_option('profile') != 'default'
  profile = 'Devel'
  name_suffix = ' (Nightly)'
  application_id = f'org.gnome.eog.@profile@'
else
  profile = ''
  name_suffix = ''
  application_id = 'org.gnome.eog'
endif

# defines
set_defines = [
  # build profile
  ['APPLICATION_ID', application_id],
  ['PROFILE', profile],
  # package
  ['PACKAGE', eog_name],
  ['VERSION', eog_version],
  # i18n
  ['GETTEXT_PACKAGE', eog_name],
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
  compiler_flags += cc.get_supported_arguments([
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Werror=return-type',
    '-Wnested-externs',
    '-Wstrict-prototypes',
  ])
endif

add_project_arguments(common_flags + compiler_flags, language: 'c')

glib_req_version = '>= 2.73.2'
peas_req_version = '>= 0.7.4'

gio_dep = dependency('gio-2.0', version: glib_req_version)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_req_version)
gnome_desktop_dep = dependency('gnome-desktop-3.0', version: '>= 2.91.2')
libpeas_gtk_dep = dependency('libpeas-gtk-1.0', version: peas_req_version)

m_dep = cc.find_library('m')

# runtime dependencies
dependency('gsettings-desktop-schemas', version: '>= 42.beta')
dependency('shared-mime-info', version: '>= 0.20')

common_deps = [
  gio_dep,
  dependency('gdk-pixbuf-2.0', version: '>= 2.36.5'),
  dependency('glib-2.0', version: glib_req_version),
  dependency('gtk+-3.0', version: '>= 3.24.15'),
  dependency('libhandy-1', version: '>= 1.5.0'),
  dependency('libpeas-1.0', version: peas_req_version),
]

gio_schemasdir = gio_dep.get_variable(
  'schemasdir',
  pkgconfig_define: ['datadir', eog_prefix / eog_datadir],
  default_value: eog_datadir / 'glib-2.0/schemas',
)

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
endif
config_h.set('HAVE_EXIF', enable_libexif)

# Little CMS (optional)
enable_cms = get_option('cms')
if enable_cms
  common_deps += dependency('lcms2')
endif
config_h.set('HAVE_LCMS', enable_cms)

# Exempi (optional)
enable_xmp = get_option('xmp')
if enable_xmp
  common_deps += dependency('exempi-2.0', version: '>= 1.99.5')
endif
config_h.set('HAVE_EXEMPI', enable_xmp)

# xdg-desktop-portal support with libportal (optional)
enable_libportal = get_option('libportal')
if enable_libportal
  libportal_dep = dependency('libportal', version: '>= 0.5', required: false)
  assert(libportal_dep.found() and cc.has_header('libportal/portal.h', dependencies: libportal_dep),
         'xdg-desktop-portal support requested but library not found. Please use -Dlibportal=false')

  libportal_gtk3_dep = dependency('libportal-gtk3', version: '>= 0.5', required: false)
  assert(libportal_gtk3_dep.found() and cc.has_header('libportal-gtk3/portal-gtk3.h', dependencies: libportal_gtk3_dep),
         'xdg-desktop-portal support requested but library not found. Please use -Dlibportal=false')

  common_deps += [libportal_dep, libportal_gtk3_dep]
endif
config_h.set('HAVE_LIBPORTAL', enable_libportal)

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
endif
config_h.set('HAVE_JPEG', enable_libjpeg)
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
  common_deps += dependency('librsvg-2.0', version: '>= 2.44.0')
endif
config_h.set('HAVE_RSVG', enable_librsvg)

# libX11 (required for TotemScrSaver and Color Profiling)
gdk_dep = dependency('gdk-3.0', required: false)

enable_x11 = enable_cms or (gdk_dep.found() and gdk_dep.get_variable('targets').contains('x11'))
if enable_x11
  x11_dep = dependency('x11')
endif

subdir('data')

if enable_libjpeg
  subdir('jpegutils')
endif

subdir('src')
subdir('plugins')
subdir('help')

if get_option('gtk_doc')
  assert(enable_introspection, 'documentation generation support requested but introspection is disabled. Please use -Dgtk_doc=false.')
  subdir('doc/reference')
endif

subdir('po')

if get_option('installed_tests')
  subdir('tests')
endif

configure_file(
  output: 'config.h',
  configuration: config_h,
)

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
)

summary({
  'Source code location': source_root,
  'Compiler': cc.get_id(),
  'Extra compiler warnings': compiler_flags,
}, section: 'Build')

summary({
  'EXIF support': enable_libexif,
  'XMP support': enable_xmp,
  'JPEG support': enable_libjpeg,
  'Colour management support': enable_cms,
  'GObject introspection': enable_introspection,
  'xdg-desktop-portal support': enable_libportal,
}, section: 'Features')
