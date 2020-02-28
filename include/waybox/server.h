#ifndef SERVER_H
#define SERVER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "waybox/output.h"
#include "waybox/cursor.h"
#include "waybox/seat.h"

struct wb_server {
	struct wl_display *wl_display;

	struct wlr_backend *backend;
	struct wlr_compositor *compositor;
	struct wlr_renderer *renderer;

	struct wlr_output_layout *layout;
	struct wb_cursor *cursor;
	struct wb_seat *seat;

	struct wb_view *grabbed_view;
	double grab_x, grab_y;
	int grab_width, grab_height;
	uint32_t resize_edges;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_list views;

	struct wl_listener new_output;
	struct wl_listener new_input;
	struct wl_list outputs; // wb_output::link
};

bool init_wb(struct wb_server* server);
bool start_wb(struct wb_server* server);
bool terminate_wb(struct wb_server* server);

#endif // server.h
