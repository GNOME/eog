tests_execdir = eog_pkglibexecdir / 'installed-tests'
tests_metadir = eog_datadir / 'installed-tests' / eog_name

tests_data = files(
  'actions.feature',
  'common_steps.py',
  'environment.py',
  'gnome-logo.png',
  'screenshot_tour.feature',
)

install_data(
  tests_data,
  install_dir: tests_execdir,
)

install_subdir(
  'steps',
  install_dir: tests_execdir,
)

test_names = [
  'about',
  'undo',
  'sidepane',
  'fullscreen',
  'wallpaper',
  'screenshot_tour1',
  'screenshot_tour2',
]

foreach test_name: test_names
  tests_conf = {
    'TESTS_PATH': eog_prefix / tests_execdir,
    'TEST_NAME': test_name,
  }

  configure_file(
    input: 'template.test.in',
    output: f'@test_name@.test',
    configuration: tests_conf,
    install: true,
    install_dir: tests_metadir,
  )
endforeach
