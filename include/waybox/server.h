#ifndef SERVER_H
#define SERVER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_gtk_primary_selection.h>
#include <wlr/types/wlr_output_layout.h>

#include "waybox/output.h"
#include "waybox/cursor.h"

struct wb_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct wlr_backend *backend;
	struct wlr_compositor *compositor;

	struct wlr_output_layout *layout;
	struct wb_cursor *cursor;

	struct wl_listener new_output;
	struct wl_listener new_input;
	struct wl_list outputs; // wb_output::link
};

bool init_wb(struct wb_server* server);
bool start_wb(struct wb_server* server);
bool terminate_wb(struct wb_server* server);

#endif // server.h
