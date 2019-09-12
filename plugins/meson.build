msgfmt_plugin_cmd = [
  find_program('msgfmt'),
  '--desktop',
  '--keyword=Name',
  '--keyword=Description',
  '--template=@INPUT@',
  '-d', po_dir,
  '--output=@OUTPUT@',
]

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
    sources: '@0@/eog-@0@-plugin.c'.format(plugin),
    include_directories: top_inc,
    dependencies: libeog_dep,
    install: true,
    install_dir: eog_pluginsdir,
  )

  plugin_data = plugin + '.plugin'

  # FIXME: Move custom_target to i18n.merge_file when gettext acquires plugin support.
  #        http://lists.gnu.org/archive/html/bug-gettext/2017-06/msg00001.html
  custom_target(
    plugin_data,
    input: '@0@/@1@.desktop.in'.format(plugin, plugin_data),
    output: plugin_data,
    command: msgfmt_plugin_cmd,
    install: true,
    install_dir: eog_pluginsdir,
  )
endforeach
