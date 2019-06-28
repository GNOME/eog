private_headers = [
  'eog-application-service.h',
  'eog-close-confirmation-dialog.h',
  'eog-enum-types.h',
  'eog-image-private.h',
  'eog-image-jpeg.h',
  'eog-marshal.h',
  'eog-metadata-details.h',
  'eog-metadata-reader.h',
  'eog-metadata-reader-jpg.h',
  'eog-metadata-reader-png.h',
  'eog-metadata-sidebar.h',
  'eog-module.h',
  'eog-plugin-engine.h',
  'eog-preferences-dialog.h',
  'eog-print-image-setup.h',
  'eog-print-preview.h',
]

if not enable_libexif
  private_headers += 'eog-exif-util.h'
endif

version_xml = configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: {'VERSION': eog_version},
)

gnome.gtkdoc(
  meson.project_name(),
  main_xml: meson.project_name() + '-docs.xml',
  src_dir: src_inc,
  ignore_headers: private_headers,
  dependencies: libeog_dep,
  scan_args: '--rebuild-types',
  content_files: version_xml,
  install: true,
)
