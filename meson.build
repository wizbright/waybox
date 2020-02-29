project(
  'waybox',
  'c',
  version: '0.0.2.1',
  license: 'MIT',
  meson_version: '>=0.43.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

add_project_arguments(
  '-Wno-unused-parameter',
  '-DWLR_USE_UNSTABLE',
  '-DVERSION="' + meson.project_version() + '"',
  language: 'c')

cc = meson.get_compiler('c')

# Adding include directory
inc_dir = include_directories('include')

pixman = dependency('pixman-1')
wlroots = dependency('wlroots', version: '>=0.9.0')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
wayland_protos  = dependency('wayland-protocols', version: '>=1.17')
xkbcommon  = dependency('xkbcommon')

subdir('protocol')
subdir('waybox')
