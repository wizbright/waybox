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
  '-D_DEFAULT_SOURCE',
  '-D_POSIX_C_SOURCE=200112L',
  '-DWL_HIDE_DEPRECATED', # Hide the deprecated parts of the Wayland API
  '-DWLR_USE_UNSTABLE',
  '-DPACKAGE_NAME="' + meson.project_name() + '"',
  '-DPACKAGE_VERSION="' + meson.project_version() + '"',
  language: 'c')

cc = meson.get_compiler('c')

# Adding include directory
inc_dir = include_directories('include')

libxml2 = dependency('libxml-2.0')
wlroots = dependency('wlroots', version: '>=0.16.0')
wayland_server = dependency('wayland-server', version: '>=1.15')
wayland_protos  = dependency('wayland-protocols', version: '>=1.17')
xkbcommon  = dependency('xkbcommon')

msgfmt = find_program('msgfmt', required: false)
if msgfmt.found()
  source_root = meson.current_source_dir()
  add_project_arguments('-DUSE_NLS=1', language: 'c')
  subdir('po')
endif

subdir('protocol')
subdir('waybox')
