wb_src = files(
  'cursor.c',
  'decoration.c',
  'main.c',
  'output.c',
  'seat.c',
  'server.c',
  'xdg_shell.c',
  )

wb_dep = [
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
  )
