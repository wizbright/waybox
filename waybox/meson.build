wb_src = files(
  'config.c',
  'cursor.c',
  'decoration.c',
  'idle.c',
  'layer_shell.c',
  'main.c',
  'output.c',
  'seat.c',
  'server.c',
  'xdg_shell.c',
  )

wb_dep = [
  libevdev,
  libinput,
  libxml2,
  wayland_server,
  wlroots,
  xkbcommon,
  ]

executable(
  'waybox',
  wb_src,
  include_directories: [inc_dir],
  dependencies: [wb_dep, wlr_protos],
  install: true,
  install_dir: get_option('prefix') / get_option('libexecdir'),
  link_args: ['-Wl,-lm'],
  )
