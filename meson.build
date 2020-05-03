project(
  'waybox',
  'c',
  version: '0.0.1',
  license: 'MIT',
  meson_version: '>=0.43.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

cairo          = dependency('cairo')
pango          = dependency('pango')
pangocairo     = dependency('pangocairo')
glesv2         = dependency('glesv2')

add_project_arguments(
  '-Wno-unused-parameter',
  '-DWLR_USE_UNSTABLE',
  '-DPACKAGE_NAME="' + meson.project_name() + '"',
  '-DPACKAGE_VERSION="' + meson.project_version() + '"',
  language: 'c')

cc = meson.get_compiler('c')

# Adding include directory
inc_dir = include_directories('include')

wlroots = dependency('wlroots', version: '>=0.9.0')
wayland_server = dependency('wayland-server', version: '>=1.15')
wayland_protos  = dependency('wayland-protocols', version: '>=1.17')
xkbcommon  = dependency('xkbcommon')

subdir('protocol')
subdir('waybox')
