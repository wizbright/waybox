#include "waybox/xdg_shell.h"

struct wb_toplevel *get_toplevel_at(
		struct wb_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * we only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a wb_toplevel. */
	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the wb_toplevel at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree->node.data;
}

void focus_toplevel(struct wb_toplevel *toplevel, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (toplevel == NULL || toplevel->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
	if (xdg_surface)
		wlr_log(WLR_INFO, "%s: %s", _("Keyboard focus is now on surface"),
				xdg_surface->toplevel->app_id);

	struct wb_server *server = toplevel->server;
	struct wlr_seat *seat = server->seat->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel != NULL) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
	}
	/* Move the toplevel to the front */
	if (!server->seat->focused_layer) {
		wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	}
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	/* Activate the new surface */
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	seat_focus_surface(server->seat, toplevel->xdg_toplevel->base->surface);
}

struct wlr_output *get_active_output(struct wb_toplevel *toplevel) {
	double closest_x, closest_y;
	struct wlr_output *output = NULL;
	wlr_output_layout_closest_point(toplevel->server->output_layout, output,
			toplevel->geometry.x + toplevel->geometry.width / 2,
			toplevel->geometry.y + toplevel->geometry.height / 2,
			&closest_x, &closest_y);
	return wlr_output_layout_output_at(toplevel->server->output_layout, closest_x, closest_y);
}

static struct wlr_box get_usable_area(struct wb_toplevel *toplevel) {
	struct wlr_output *output = get_active_output(toplevel);
	struct wlr_box usable_area = {0};
	wlr_output_effective_resolution(output, &usable_area.width, &usable_area.height);
	return usable_area;
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, map);
	if (toplevel->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return;

	struct wb_config *config = toplevel->server->config;
	struct wlr_box geo_box = {0};
	struct wlr_box usable_area = get_usable_area(toplevel);
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);

	if (config) {
		toplevel->geometry.height = MIN(geo_box.height,
				usable_area.height - config->margins.top - config->margins.bottom);
		toplevel->geometry.width = MIN(geo_box.width,
				usable_area.width - config->margins.left - config->margins.right);
		toplevel->geometry.x = config->margins.left;
		toplevel->geometry.y = config->margins.top;
	} else {
		toplevel->geometry.height = MIN(geo_box.height, usable_area.height);
		toplevel->geometry.width = MIN(geo_box.width, usable_area.width);
		toplevel->geometry.x = 0;
		toplevel->geometry.y = 0;
	}

	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
			toplevel->geometry.width, toplevel->geometry.height);
	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);

	wlr_scene_node_set_position(&toplevel->scene_tree->node,
			toplevel->geometry.x, toplevel->geometry.y);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
	if (toplevel->xdg_toplevel->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return;
	reset_cursor_mode(toplevel->server);

	/* Focus the next toplevel, if any. */
	if (wl_list_length(&toplevel->link) > 1) {
		struct wb_toplevel *next_toplevel = wl_container_of(toplevel->link.next, next_toplevel, link);
		if (next_toplevel && next_toplevel->xdg_toplevel && next_toplevel->scene_tree && next_toplevel->scene_tree->node.enabled) {
			wlr_log(WLR_INFO, "%s: %s", _("Focusing next toplevel"),
					next_toplevel->xdg_toplevel->app_id);
			focus_toplevel(next_toplevel, next_toplevel->xdg_toplevel->base->surface);
		}
	}
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed and should never be shown again. */
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->new_popup.link);

	wl_list_remove(&toplevel->request_fullscreen.link);
	wl_list_remove(&toplevel->request_minimize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);

	wl_list_remove(&toplevel->link);
	free(toplevel);
}

static void xdg_toplevel_request_fullscreen(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to set itself to
	 * fullscreen. waybox currently doesn't support fullscreen, but to
	 * conform to xdg-shell protocol we still must send a configure.
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. However, if the
	 * request was sent before an initial commit, we don't do anything and let the client finish
	 * the initial surface setup.
	 */
	struct wb_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations.
	 */
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, request_maximize);
	struct wlr_box usable_area = get_usable_area(toplevel);

	bool is_maximized = toplevel->xdg_toplevel->current.maximized;
	if (!is_maximized) {
		struct wb_config *config = toplevel->server->config;
		toplevel->previous_geometry = toplevel->geometry;
		if (config) {
			toplevel->geometry.x = config->margins.left;
			toplevel->geometry.y = config->margins.top;
			usable_area.height -= config->margins.top + config->margins.bottom;
			usable_area.width -= config->margins.left + config->margins.right;
		} else {
			toplevel->geometry.x = 0;
			toplevel->geometry.y = 0;
		}
	} else {
		usable_area = toplevel->previous_geometry;
		toplevel->geometry.x = toplevel->previous_geometry.x;
		toplevel->geometry.y = toplevel->previous_geometry.y;
	}
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, usable_area.width, usable_area.height);
	wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, !is_maximized);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
			toplevel->geometry.x, toplevel->geometry.y);
}

static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, request_minimize);
	bool minimize_requested = toplevel->xdg_toplevel->requested.minimized;
	if (minimize_requested) {
		toplevel->previous_geometry = toplevel->geometry;
		toplevel->geometry.y = -toplevel->geometry.height;

		struct wb_toplevel *next_toplevel = wl_container_of(toplevel->link.next, next_toplevel, link);
		if (wl_list_length(&toplevel->link) > 1)
			focus_toplevel(next_toplevel, next_toplevel->xdg_toplevel->base->surface);
		else
			focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
	} else {
		toplevel->geometry = toplevel->previous_geometry;
	}

	wlr_scene_node_set_position(&toplevel->scene_tree->node,
			toplevel->geometry.x, toplevel->geometry.y);
}

static void begin_interactive(struct wb_toplevel *toplevel,
		enum wb_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propagating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct wb_server *server = toplevel->server;
	struct wlr_surface *focused_surface =
		server->seat->seat->pointer_state.focused_surface;
	if (toplevel->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grabbed_toplevel = toplevel;
	server->cursor->cursor_mode = mode;

	if (mode == WB_CURSOR_MOVE) {
		server->grab_x = server->cursor->cursor->x - toplevel->geometry.x;
		server->grab_y = server->cursor->cursor->y - toplevel->geometry.y;
	} else if (mode == WB_CURSOR_RESIZE) {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);

		double border_x = (toplevel->geometry.x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (toplevel->geometry.y + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->cursor->x - border_x;
		server->grab_y = server->cursor->cursor->y - border_y;

		server->grab_geo_box = geo_box;
		server->grab_geo_box.x += toplevel->geometry.x;
		server->grab_geo_box.y += toplevel->geometry.y;

		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_move(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. */
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, WB_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
	begin_interactive(toplevel, WB_CURSOR_RESIZE, event->edges);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *popup = data;
	struct wb_toplevel *toplevel = wl_container_of(listener, toplevel, new_popup);

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
			toplevel->server->output_layout,
			toplevel->geometry.x + popup->current.geometry.x,
			toplevel->geometry.y + popup->current.geometry.y);

	if (!wlr_output) {
		return;
	}
	struct wb_output *output = wlr_output->data;

	int top_margin = (toplevel->server->config) ?
		toplevel->server->config->margins.top : 0;
	struct wlr_box output_toplevel_box = {
		.x = output->geometry.x - toplevel->geometry.x,
		.y = output->geometry.y - toplevel->geometry.y,
		.width = output->geometry.width,
		.height = output->geometry.height - top_margin,
	};
	wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);
}

static void handle_new_xdg_popup(struct wl_listener *listener, void *data) {
	/* We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable this,
	 * we always set the user data field of xdg_surfaces to the corresponding
	 * scene node. */
	struct wlr_xdg_popup *xdg_popup = data;
	struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(
		xdg_popup->parent);
	if (parent != NULL) {
		struct wlr_scene_tree *parent_tree = parent->data;
		xdg_popup->base->data = wlr_scene_xdg_surface_create(
			parent_tree, xdg_popup->base);
	}
}

static void handle_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	struct wb_server *server =
#if WLR_CHECK_VERSION (0, 18,0)
		wl_container_of(listener, server, new_xdg_toplevel);
#else
		wl_container_of(listener, server, new_xdg_surface);
#endif
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	/* Allocate a wb_toplevel for this toplevel */
	struct wb_toplevel *toplevel =
		calloc(1, sizeof(struct wb_toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;

	/* Listen to the various events it can emit */
	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->destroy.notify = xdg_toplevel_destroy;
#if WLR_CHECK_VERSION (0, 18, 0)
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);
#else
	wl_signal_add(&xdg_toplevel->base->events.destroy, &toplevel->destroy);
#endif
	toplevel->new_popup.notify = handle_new_popup;
	wl_signal_add(&xdg_toplevel->base->events.new_popup, &toplevel->new_popup);

	toplevel->scene_tree = wlr_scene_xdg_surface_create(
		&toplevel->server->scene->tree, xdg_toplevel->base);
	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;

	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
	toplevel->request_minimize.notify = xdg_toplevel_request_minimize;
	wl_signal_add(&xdg_toplevel->events.request_minimize, &toplevel->request_minimize);
	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);

	wl_list_insert(&toplevel->server->toplevels, &toplevel->link);
}

#if !WLR_CHECK_VERSION(0, 18, 0)
static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct wlr_xdg_surface *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		handle_new_xdg_popup(listener, xdg_surface->popup);
	}
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		handle_new_xdg_toplevel(listener, xdg_surface->toplevel);
	}
}
#endif

void init_xdg_shell(struct wb_server *server) {
	/* xdg-shell version 3 */
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
#if WLR_CHECK_VERSION (0, 18, 0)
	server->new_xdg_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&server->xdg_shell->events.new_popup, &server->new_xdg_popup);
	server->new_xdg_toplevel.notify = handle_new_xdg_toplevel;
	wl_signal_add(&server->xdg_shell->events.new_toplevel, &server->new_xdg_toplevel);
#else
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);
#endif
}
