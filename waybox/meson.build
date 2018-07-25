wb_src = files(
  'main.c',
  'output.c',
  'server.c',
	'desktop.c',
	'cursor.c',
  )

wb_dep = [
  wayland_server,
  wlroots,
  pixman,
  ]

executable(
  'waybox',
  wb_src,
  include_directories: [inc_dir],
  dependencies: [wb_dep, wlr_protos],
  install: true,
  )
