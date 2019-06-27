if have_jpeg_80
  sources = files('transupp-8a.c')
else
  sources = files('transupp-6b.c')
endif

libjpegutils = static_library(
  'eog-jpegutils',
  sources: sources,
  include_directories: top_inc,
  dependencies: libjpeg_dep,
)

libjpegutils_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: libjpeg_dep,
  link_with: libjpegutils,
)
