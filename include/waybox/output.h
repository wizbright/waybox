#ifndef _WB_OUTPUT_H
#define _WB_OUTPUT_H

#include <stdlib.h>
#include <time.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#include "waybox/server.h"

struct wb_output {
	struct wlr_output *wlr_output;
	struct wb_server *server;

	struct wl_list layers[4];

	struct wl_listener destroy;
	struct wl_listener frame;

	struct wl_list link;
};

struct wb_view {
	struct wl_list link;
	struct wb_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
#if !WLR_CHECK_VERSION(0, 16, 0)
	struct wlr_xdg_surface *xdg_surface;
#endif

	struct wlr_xdg_toplevel_decoration_v1 *decoration;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_maximize;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener surface_commit;
	bool mapped;
	int x, y;
	struct wlr_box origdim;
};

void output_frame_notify(struct wl_listener* listener, void *data);
void output_destroy_notify(struct wl_listener* listener, void *data);
void new_output_notify(struct wl_listener* listener, void *data);

#endif /* output.h */
