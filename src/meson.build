src_inc = include_directories('.')

enum_headers = files(
  'eog-scroll-view.h',
  'eog-window.h',
)

gnome.mkenums(
  'org.gnome.eog.enums.xml',
  sources: enum_headers,
  comments: '<!-- @comment@ -->',
  fhead: '<schemalist>',
  vhead: '  <@type@ id="org.gnome.eog.@EnumName@">',
  vprod: '    <value nick="@valuenick@" value="@valuenum@"/>',
  vtail: '  </@type@>',
  ftail: '</schemalist>',
  install_header: true,
  install_dir: gio_schemasdir,
)

headers = enum_headers + files(
  'eog-application.h',
  'eog-application-activatable.h',
  'eog-clipboard-handler.h',
  'eog-debug.h',
  'eog-details-dialog.h',
  'eog-enums.h',
  'eog-file-chooser.h',
  'eog-image.h',
  'eog-image-save-info.h',
  'eog-job-scheduler.h',
  'eog-jobs.h',
  'eog-list-store.h',
  'eog-remote-presenter.h',
  'eog-sidebar.h',
  'eog-statusbar.h',
  'eog-thumbnail.h',
  'eog-thumb-nav.h',
  'eog-thumb-view.h',
  'eog-transform.h',
  'eog-uri-converter.h',
  'eog-window-activatable.h',
)

sources = files(
  'eog-application.c',
  'eog-application-activatable.c',
  'eog-clipboard-handler.c',
  'eog-close-confirmation-dialog.c',
  'eog-debug.c',
  'eog-details-dialog.c',
  'eog-error-message-area.c',
  'eog-file-chooser.c',
  'eog-image.c',
  'eog-image-jpeg.c',
  'eog-image-save-info.c',
  'eog-job-scheduler.c',
  'eog-jobs.c',
  'eog-list-store.c',
  'eog-metadata-sidebar.c',
  'eog-metadata-reader.c',
  'eog-metadata-reader-jpg.c',
  'eog-metadata-reader-png.c',
  'eog-pixbuf-util.c',
  'eog-plugin-engine.c',
  'eog-preferences-dialog.c',
  'eog-print.c',
  'eog-print-image-setup.c',
  'eog-print-preview.c',
  'eog-remote-presenter.c',
  'eog-save-as-dialog-helper.c',
  'eog-scroll-view.c',
  'eog-session.c',
  'eog-sidebar.c',
  'eog-statusbar.c',
  'eog-thumbnail.c',
  'eog-thumb-nav.c',
  'eog-thumb-view.c',
  'eog-transform.c',
  'eog-uri-converter.c',
  'eog-util.c',
  'eog-window-activatable.c',
  'eog-window.c',
  'eog-zoom-entry.c',
  'zoom.c',
)

if enable_libexif
  headers += files('eog-exif-util.h')
  sources += files('eog-exif-util.c')
endif

if enable_libexif or enable_xmp
  sources += files('eog-metadata-details.c')
endif

install_headers(
  headers,
  subdir: eog_api_name / eog_name,
)

sources += gnome.compile_resources(
  'eog-resources',
  'eog.gresource.xml',
  source_dir: data_dir,
  c_name: '_eog',
  dependencies: resource_data,
  export: true,
)

enum = 'eog-enum-types'

sources += gnome.mkenums(
  enum,
  sources: headers,
  c_template: f'@enum@.c.template',
  h_template: f'@enum@.h.template',
)

marshal = 'eog-marshal'

sources += gnome.genmarshal(
  marshal,
  sources: f'@marshal@.list',
  prefix: 'eog_marshal',
  internal: true,
)

deps = common_deps + [
  gio_unix_dep,
  gnome_desktop_dep,
  libpeas_gtk_dep,
  m_dep,
  zlib_dep,
]

cflags = [
  '-DG_LOG_DOMAIN="@0@"'.format(eog_name.to_upper()),
  '-DEOG_DATA_DIR="@0@"'.format(eog_prefix / eog_pkgdatadir),
  '-DEOG_PLUGIN_DIR="@0@"'.format(eog_prefix / eog_pluginsdir),
  '-DLIBDIR="@0@"'.format(eog_prefix / eog_libdir),
]

symbol_map = meson.current_source_dir() / 'symbol.map'
ldflags = cc.get_supported_link_arguments(f'-Wl,--version-script,@symbol_map@')

if enable_introspection
  deps += gir_dep
endif

if enable_libjpeg
  deps += libjpegutils_dep
endif

if enable_x11
  deps += x11_dep
endif

libeog = shared_library(
  eog_name,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: symbol_map,
  install: true,
  install_dir: eog_pkglibdir,
)

libeog_dep = declare_dependency(
  link_with: libeog,
  include_directories: src_inc,
  dependencies: common_deps,
)

pkg.generate(
  libraries: libeog,
  version: eog_version,
  name: 'Eye of GNOME',
  description: 'Image viewer for GNOME',
  filebase: eog_name,
  subdirs: eog_api_name,
  requires: ['gtk+-3.0', 'libhandy-1'],
  variables: [
    'exec_prefix=${prefix}',
    # the equivalent of eog_pluginsdir but using ${libdir} for variable replacement
    'pluginsdir=' + ('${libdir}' / eog_name / 'plugins'),
  ],
)

deps = [libeog_dep]

cflags = [
  '-DG_LOG_DOMAIN="@0@"'.format(eog_name.to_upper()),
  '-DEOG_LOCALE_DIR="@0@"'.format(eog_prefix / eog_localedir),
]

if enable_introspection
  deps += gir_dep
endif

eog = executable(
  eog_name,
  'main.c',
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_rpath: eog_prefix / eog_pkglibdir,
)

if enable_introspection
  eog_gir = gnome.generate_gir(
    libeog,
    sources: sources + headers,
    nsversion: eog_api_version,
    namespace: eog_gir_ns,
    includes: 'Gtk-3.0',
    install: true,
    install_dir_gir: eog_pkgdatadir / f'gir-@eog_gir_version@',
    install_dir_typelib: eog_pkglibdir / f'girepository-@eog_gir_version@',
  )
endif
