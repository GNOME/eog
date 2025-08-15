subdir('icons')

resource_data = files(
  'pixmaps/thumbnail-frame.png',
  'eog-details-dialog.ui',
  'eog-gear-menu.ui',
  'eog-multiple-save-as-dialog.ui',
  'eog-preferences-dialog.ui',
  'eog-remote-presenter.ui',
  'eog-zoom-entry.ui',
  'eog.css',
  'fullscreen-toolbar.ui',
  'help-overlay.ui',
  'metadata-sidebar.ui',
  'popup-menus.ui',
)

i18n.merge_file(
  input: configure_file(
    input: 'org.gnome.eog.desktop.in.in',
    output: '@BASENAME@',
    configuration: {'icon': application_id},
  ),
  output: '@BASENAME@',
  po_dir: po_dir,
  type: 'desktop',
  install: true,
  install_dir: eog_datadir / 'applications',
)

i18n.merge_file(
  input: 'org.gnome.eog.metainfo.xml.in',
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: eog_datadir / 'metainfo',
)


configure_file(
  input: 'org.gnome.eog.gschema.xml.in',
  output: '@BASENAME@',
  configuration: {'GETTEXT_PACKAGE': eog_name},
  install: true,
  install_dir: gio_schemasdir,
)

install_data(
  'eog.convert',
  install_dir: eog_datadir / 'GConf/gsettings',
)
