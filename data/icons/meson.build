icon_sizes = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '256x256'
]

foreach icon_size: icon_sizes
  install_subdir(
    join_paths(icon_size, 'apps'),
    install_dir: join_paths(eog_datadir, 'icons', 'hicolor', icon_size)
  )
endforeach

install_data(
  'scalable/apps/eog-symbolic.svg',
  install_dir: join_paths(eog_datadir, 'icons', 'hicolor', 'scalable', 'apps')
)

icon_sizes = [
  ['16x16', ['slideshow-play.png', 'eog-plugin.png']],
  ['22x22', ['slideshow-play.png', 'eog-plugin.png']],
  ['24x24', ['slideshow-play.png']],
  ['32x32', ['slideshow-play.png', 'eog-plugin.png']],
  ['48x48', ['slideshow-play.png']]
]

foreach icon_size: icon_sizes
  actions_icons = []
  foreach file: icon_size[1]
    actions_icons += files(join_paths(icon_size[0], 'actions', file))
  endforeach

  install_data(
    actions_icons,
    install_dir: join_paths(eog_pkgdatadir, 'icons', 'hicolor', icon_size[0], 'actions')
  )
endforeach

install_subdir(
  'scalable/actions',
  install_dir: join_paths(eog_pkgdatadir, 'icons', 'hicolor', 'scalable')
)
