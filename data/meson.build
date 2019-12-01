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
  'popup-menus.ui',
)

desktop = 'org.gnome.eog.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: '@BASENAME@',
  configuration: {'VERSION': eog_version, 'icon': application_id },
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: eog_datadir / 'applications',
)

appdata = 'eog.appdata.xml'

i18n.merge_file(
  appdata,
  input: appdata + '.in',
  output: appdata,
  po_dir: po_dir,
  install: true,
  install_dir: eog_datadir / 'metainfo',
)


configure_file(
  input: 'org.gnome.eog.gschema.xml.in',
  output: '@BASENAME@',
  configuration: {'GETTEXT_PACKAGE': meson.project_name()},
  install: true,
  install_dir: gio_schemasdir,
)

install_data(
  'eog.convert',
  install_dir: eog_datadir / 'GConf/gsettings',
)
