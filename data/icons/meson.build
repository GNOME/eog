install_subdir(
  'scalable/apps',
  install_dir: eog_datadir / 'icons/hicolor/scalable',
)

install_subdir(
  'symbolic',
  install_dir: eog_datadir / 'icons/hicolor',
)

foreach icon_size: ['16x16', '22x22', '24x24', '32x32', '48x48']
  install_subdir(
    icon_size / 'actions',
    install_dir: eog_pkgdatadir / 'icons/hicolor' / icon_size,
    exclude_files: ['eog-plugin.svg', 'slideshow-play.svg', 'eog-image-gallery.svg'],
  )
endforeach

install_subdir(
  'scalable/actions',
  install_dir: eog_pkgdatadir / 'icons/hicolor/scalable',
)
