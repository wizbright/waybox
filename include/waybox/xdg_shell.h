#ifndef _WB_XDG_SHELL_H
#define _WB_XDG_SHELL_H

#include <wlr/types/wlr_fractional_scale_v1.h>

#include "waybox/server.h"

struct wb_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct wb_toplevel {
	struct wb_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;

	struct wlr_xdg_toplevel_decoration_v1 *decoration;

	struct wlr_ext_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wlr_ext_foreign_toplevel_handle_v1_state foreign_toplevel_state;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener new_popup;
	struct wl_listener request_fullscreen;
	struct wl_listener request_maximize;
	struct wl_listener request_minimize;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener set_app_id;
	struct wl_listener set_title;

	struct wlr_box geometry;
	struct wlr_box previous_geometry;

	struct wl_list link;
};

void init_xdg_shell(struct wb_server *server);
void focus_toplevel(struct wb_toplevel *toplevel);
struct wlr_output *get_active_output(struct wb_toplevel *toplevel);
struct wb_toplevel *get_toplevel_at(
		struct wb_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);
#endif
