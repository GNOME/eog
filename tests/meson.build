tests_execdir = join_paths(eog_pkglibexecdir, 'installed-tests')
tests_metadir = join_paths(eog_datadir, 'installed-tests', meson.project_name())

tests_data = files(
  'actions.feature',
  'common_steps.py',
  'environment.py',
  'gnome-logo.png',
  'screenshot_tour.feature'
)

install_data(
  tests_data,
  install_dir: tests_execdir
)

install_subdir(
  'steps',
  install_dir: tests_execdir
)

test_names = [
  'about',
  'undo',
  'sidepane',
  'fullscreen',
  'wallpaper',
  'screenshot_tour1',
  'screenshot_tour2'
]

foreach test_name: test_names
  tests_conf = configuration_data()
  tests_conf.set('TESTS_PATH', tests_execdir)
  tests_conf.set('TEST_NAME', test_name)

  configure_file(
    input: 'template.test.in',
    output: test_name + '.test',
    install: true,
    install_dir: tests_metadir,
    configuration: tests_conf
  )
endforeach