#ifndef _WB_DECORATION_H
#define _WB_DECORATION_H

#include <wlr/types/wlr_xdg_decoration_v1.h>

#include "waybox/server.h"

struct wb_decoration {
	struct wb_server *server;
	struct wlr_xdg_decoration_manager_v1 *decoration_manager;
	struct wlr_xdg_toplevel_decoration_v1 *toplevel_decoration;

	struct wl_listener toplevel_decoration_destroy;
	struct wl_listener request_mode;
	struct wl_listener mode_destroy;
	int yes;
};

void init_xdg_decoration(struct wb_server *server);
#endif
