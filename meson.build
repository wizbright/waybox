project(
  'Waybox',
  'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>=0.52.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

add_project_arguments(
  '-Wno-unused-parameter',
  '-DWLR_USE_UNSTABLE',
  '-DPACKAGE_NAME="' + meson.project_name() + '"',
  '-DPACKAGE_VERSION="' + meson.project_version() + '"',
  language: 'c')

cc = meson.get_compiler('c')

# Adding include directory
inc_dir = include_directories('include')

# Due to the planned refactor of xdg_shell in wlroots 0.16.0, I doubt this will
# build when it's released
libxml2 = dependency('libxml-2.0')
wlroots = dependency('wlroots', version: ['>=0.15.0', '<0.16.0'])
wayland_server = dependency('wayland-server', version: '>=1.15')
wayland_protos  = dependency('wayland-protocols', version: '>=1.17')
xkbcommon  = dependency('xkbcommon')

subdir('po')
subdir('protocol')
subdir('waybox')
