libjpegutils = static_library(
  'eog-jpegutils',
  sources: 'transupp-@0@.c'.format(have_jpeg_80 ? '8a' : '6b'),
  include_directories: top_inc,
  dependencies: libjpeg_dep,
)

libjpegutils_dep = declare_dependency(
  include_directories: '.',
  dependencies: libjpeg_dep,
  link_with: libjpegutils,
)
