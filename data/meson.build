subdir('icons')

resource_data = files(
  'pixmaps/thumbnail-frame.png',
  'eog-gear-menu.ui',
  'eog-image-properties-dialog.ui',
  'eog-multiple-save-as-dialog.ui',
  'eog-preferences-dialog.ui',
  'eog-zoom-entry.ui',
  'eog.css',
  'fullscreen-toolbar.ui',
  'help-overlay.ui',
  'metadata-sidebar.ui',
  'popup-menus.ui'
)

desktop_conf = configuration_data()
desktop_conf.set('VERSION', eog_version)

desktop = 'eog.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(eog_datadir, 'applications')
)

appdata = 'eog.appdata.xml'

i18n.merge_file(
  appdata,
  input: appdata + '.in',
  output: appdata,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(eog_datadir, 'metainfo')
)

schema_conf = configuration_data()
schema_conf.set('GETTEXT_PACKAGE', meson.project_name())

schema = 'org.gnome.eog.gschema.xml'

configure_file(
  input: schema + '.in',
  output: schema,
  install: true,
  install_dir: eog_schemadir,
  configuration: schema_conf
)

install_data(
  'eog.convert',
  install_dir: join_paths(eog_datadir, 'GConf', 'gsettings')
)
