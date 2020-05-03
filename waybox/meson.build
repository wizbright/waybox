wb_src = files(
  'cursor.c',
  'main.c',
  'output.c',
  'seat.c',
  'server.c',
  'xdg_shell.c',
  'stringop.c',
  'util.c',
  'style.c'
  )

wb_dep = [
  wayland_server,
  wlroots,
  xkbcommon,
  cairo,
  pango,
  pangocairo,
  glesv2
  ]

executable(
  'waybox',
  wb_src,
  include_directories: [inc_dir],
  dependencies: [wb_dep, wlr_protos],
  install: true,
  )
