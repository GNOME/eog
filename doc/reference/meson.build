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
  'eog-print-preview.h'
]

if not have_exif
  private_headers += 'eog-exif-util.h'
endif

version_conf = configuration_data()
version_conf.set('VERSION', eog_version)

version_xml = 'version.xml'

configure_file(
  input: version_xml + '.in',
  output: version_xml,
  configuration: version_conf
)

# FIXME: gtkdoc is not able to get a configure_object as main_xml file.
#        https://github.com/mesonbuild/meson/issues/1875
#        https://github.com/mesonbuild/meson/issues/550
'''
# Build EXIF-related documentation if we really have support for it
doc_conf = configuration_data()
if have_exif or have_exempi
  doc_conf.set('EOG_DOC_EXIF_START', '')
  doc_conf.set('EOG_DOC_EXIF_STOP', '')
else
  doc_conf.set('EOG_DOC_EXIF_START', '<!--')
  doc_conf.set('EOG_DOC_EXIF_STOP', '-->')
endif

docs_xml = meson.project_name() + '-docs.xml'

main_xml = configure_file(
  input: docs_xml + '.in',
  output: docs_xml,
  configuration: doc_conf
)
'''

doc_path = join_paths(eog_datadir, 'gtk-doc', 'html', meson.project_name())

gnome.gtkdoc(
  meson.project_name(),
  main_xml: meson.project_name() + '-docs.xml',
  src_dir: src_inc,
  dependencies: libeog_dep,
  scan_args: [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  mkdb_args: [
    '--xml-mode',
    '--output-format=xml'
  ],
  gobject_typesfile: meson.project_name() + '.types',
  install: true,
  install_dir: doc_path
)
