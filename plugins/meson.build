plugins = [
  'fullscreen',
  'reload',
]

if enable_libexif
  plugins += 'statusbar-date'
endif

foreach plugin: plugins
  shared_module(
    plugin,
    sources: f'@plugin@/eog-@plugin@-plugin.c',
    include_directories: top_inc,
    dependencies: libeog_dep,
    install: true,
    install_dir: eog_pluginsdir,
  )


  # FIXME: Remove `args` parameter when gettext acquires plugin support.
  #        http://lists.gnu.org/archive/html/bug-gettext/2017-06/msg00001.html
  # NOTE:  We need to keep the .desktop suffix on the source file to be able
  #        to extract translatable strings. .plugin is not recognized.
  i18n.merge_file(
    input: f'@plugin@/@plugin@.plugin.desktop.in',
    output: f'@plugin@.plugin',
    po_dir: po_dir,
    type: 'desktop',
    args: ['--keyword=Name', '--keyword=Description'],
    install: true,
    install_dir: eog_pluginsdir,
  )
endforeach
