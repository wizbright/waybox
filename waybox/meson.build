wb_src = files(
  'cursor.c',
  'main.c',
  'output.c',
  'seat.c',
  'server.c',
  )

wb_dep = [
  wayland_server,
  wlroots,
  pixman,
  xkbcommon,
  ]

executable(
  'waybox',
  wb_src,
  include_directories: [inc_dir],
  dependencies: [wb_dep, wlr_protos],
  install: true,
  )
