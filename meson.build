project(
  'Waybox',
  'c',
  version: '0.2.3',
  license: 'MIT',
  meson_version: '>=0.60.0',
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

if get_option('wlroots-version') != ''
  wlroots_version = get_option('wlroots-version')
else
  wlroots_version = ['wlroots-0.20', 'wlroots-0.19', 'wlroots-0.18']
endif

libevdev = dependency('libevdev')
libinput = dependency('libinput', version: '>=1.21.0', required: false)
libxml2 = dependency('libxml-2.0')
wayland_protos = dependency('wayland-protocols', version: '>=1.37')
wayland_server = dependency('wayland-server', version: '>=1.15')
wlroots = dependency(wlroots_version, version: '>=0.17.0')
xkbcommon = dependency('xkbcommon')

if libinput.found()
  add_project_arguments('-DHAS_LIBINPUT', language: 'c')
endif

msgfmt = find_program('msgfmt', required: false)
if msgfmt.found()
  source_root = meson.current_source_dir()
  subdir('po')
endif

subdir('data')
subdir('protocol')
subdir('waybox')
