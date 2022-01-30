#ifndef _WB_LAYERS_H
#define _WB_LAYERS_H
#include <wlr/types/wlr_layer_shell_v1.h>

struct wb_server;

struct wb_layer_surface {
	struct wlr_layer_surface_v1 *layer_surface;
	struct wb_server *server;
	struct wl_list link;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;

	struct wlr_box geo;
};

void init_layer_shell(struct wb_server *server);

#endif
