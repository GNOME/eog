tests_exec_dir = join_paths(eog_pkglibexecdir, 'installed-tests')

tests_files = files(
  'actions.feature',
  'common_steps.py',
  'environment.py',
  'gnome-logo.png',
  'screenshot_tour.feature'
)

install_data(
  tests_files,
  install_dir: tests_exec_dir
)

install_subdir(
  'steps',
  install_dir: tests_exec_dir
)

tests = [
  'about',
  'undo',
  'sidepane',
  'fullscreen',
  'wallpaper',
  'screenshot_tour1',
  'screenshot_tour2'
]

foreach test: tests
  tests_conf = configuration_data()
  tests_conf.set('TESTS_PATH', tests_exec_dir)
  tests_conf.set('TEST_NAME', test)
  tests_conf.set('TEST_TYPE', 'session-exclusive')

  configure_file(
    input: 'tests.in',
    output: test + '.test',
    install: true,
    install_dir: join_paths(eog_datadir, 'installed-tests', meson.project_name()),
    configuration: tests_conf
  )
endforeach
