if have_jpeg_80
  sources = files('transupp-8a.c')
else
  sources = files('transupp-6b.c')
endif

libjpegutils = static_library(
  'eog-jpegutils',
  sources: sources,
  include_directories: top_inc
)

jpeg_deps += declare_dependency(
  link_with: libjpegutils,
  include_directories: include_directories('.')
)
