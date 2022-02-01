toml_conf = {
  'license': ','.join(meson.project_license()),
  'version': eog_version,
}

toml = configure_file(
  input: f'@eog_name@.toml.in',
  output: '@BASENAME@',
  configuration: toml_conf,
)

custom_target(
  f'@eog_name@-doc',
  input: [
    toml,
    eog_gir,
  ],
  output: eog_name,
  command: [
    find_program('gi-docgen'),
    'generate',
    '--quiet',
    '--no-namespace-dir',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: gnome.gtkdoc_html_dir(''),
)
