#ifndef _WB_OUTPUT_H
#define _WB_OUTPUT_H

#include <time.h>

#include "waybox/server.h"
#include <wlr/types/wlr_output_management_v1.h>

struct wb_output {
	struct wlr_output *wlr_output;
	struct wb_server *server;

	struct {
		struct wlr_scene_tree *shell_background;
		struct wlr_scene_tree *shell_bottom;
		struct wlr_scene_tree *shell_fullscreen;
		struct wlr_scene_tree *shell_overlay;
		struct wlr_scene_tree *shell_top;
	} layers;

#if ! WLR_CHECK_VERSION(0, 18, 0)
	/* DEPRECATED: Use a tool like swaybg or hyprpaper instead */
	struct wlr_scene_rect *background;
#endif
	bool gamma_lut_changed;
	struct wlr_box geometry;

	struct wl_listener destroy;
	struct wl_listener frame;
	struct wl_listener request_state;

	struct wl_list link;
};

void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data);
void output_frame_notify(struct wl_listener *listener, void *data);
void output_destroy_notify(struct wl_listener *listener, void *data);
void init_output(struct wb_server *server);

#endif /* output.h */
