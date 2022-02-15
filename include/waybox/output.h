#ifndef _WB_OUTPUT_H
#define _WB_OUTPUT_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdlib.h>
#include <time.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#include "waybox/server.h"

struct wb_output {
	struct wlr_output *wlr_output;
	struct wb_server *server;

	struct wlr_xdg_output_manager_v1 *manager;

	struct wl_list layers[4];

	struct wl_listener destroy;
	struct wl_listener frame;

	struct wl_list link;
};

struct wb_view {
	struct wl_list link;
	struct wb_server *server;
	struct wlr_xdg_surface *xdg_surface;

	struct wlr_xdg_toplevel_decoration_v1 *decoration;

	struct wl_listener ack_configure;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	bool configured, mapped;
	int x, y;
};

void output_frame_notify(struct wl_listener* listener, void *data);
void output_destroy_notify(struct wl_listener* listener, void *data);
void new_output_notify(struct wl_listener* listener, void *data);

#endif /* output.h */
