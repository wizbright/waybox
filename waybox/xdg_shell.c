#include "waybox/xdg_shell.h"

void focus_view(struct wb_view *view, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (view == NULL || surface == NULL || !wlr_surface_is_xdg_surface(surface)) {
		return;
	}
	struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
	if (xdg_surface)
		wlr_log(WLR_INFO, "%s: %s", _("Keyboard focus is now on surface"),
				xdg_surface->toplevel->app_id);
	struct wb_server *server = view->server;
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
		struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
					seat->keyboard_state.focused_surface);
#if WLR_CHECK_VERSION(0, 16, 0)
		wlr_xdg_toplevel_set_activated(previous->toplevel, false);
#else
		wlr_xdg_toplevel_set_activated(previous, false);
#endif
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	/* Move the view to the front */
	wl_list_remove(&view->link);
	wl_list_insert(&server->views, &view->link);
	/* Activate the new surface */
#if WLR_CHECK_VERSION(0, 16, 0)
	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
#else
	wlr_xdg_toplevel_set_activated(view->xdg_surface, true);
#endif
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	wlr_seat_keyboard_notify_enter(seat, view->xdg_toplevel->base->surface,
		keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

static void xdg_surface_commit(struct wl_listener *listener, void *data) {
	/* Called after the surface is committed */
	struct wb_view *view = wl_container_of(listener, view, surface_commit);
	struct wlr_xdg_surface *xdg_surface = view->xdg_toplevel->base;
	struct wlr_box geo_box = {0};
	wlr_xdg_surface_get_geometry(xdg_surface, &geo_box);
	if (geo_box.x < 0 && view->x < 1)
		view->x += -geo_box.x;
	if (geo_box.y < 0 && view->y < 1)
		view->y += -geo_box.y;
}

static void xdg_surface_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct wb_view *view = wl_container_of(listener, view, map);
	view->mapped = true;
	struct wlr_box geo_box = {0};
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
#if WLR_CHECK_VERSION(0, 16, 0)
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, geo_box.width, geo_box.height);
#else
	wlr_xdg_toplevel_set_size(view->xdg_surface, geo_box.width, geo_box.height);
#endif
	focus_view(view, view->xdg_toplevel->base->surface);
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct wb_view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;

	struct wb_view *current_view = wl_container_of(view->server->views.next, current_view, link);
	struct wb_view *next_view = wl_container_of(current_view->link.next, next_view, link);

	/* If the current view is mapped, focus it. */
	if (current_view->mapped) {
		wlr_log(WLR_INFO, "%s: %s", _("Focusing current view"),
				current_view->xdg_toplevel->app_id);
		focus_view(current_view, current_view->xdg_toplevel->base->surface);
	}
	/* Otherwise, focus the next view, if any. */
	else if (next_view->xdg_toplevel->base->surface &&
			wlr_surface_is_xdg_surface(next_view->xdg_toplevel->base->surface)) {
		wlr_log(WLR_INFO, "%s: %s", _("Focusing next view"),
				next_view->xdg_toplevel->app_id);
		focus_view(next_view, next_view->xdg_toplevel->base->surface);
	}
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data) {
	/* Called when the surface is destroyed and should never be shown again. */
	struct wb_view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->link);
	free(view);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	struct wlr_xdg_surface *surface = data;
	struct wb_view *view = wl_container_of(listener, view, request_maximize);

	double closest_x, closest_y;
	struct wlr_box geo_box;
	struct wlr_output *output = NULL;
	wlr_xdg_surface_get_geometry(surface, &geo_box);
	wlr_output_layout_closest_point(view->server->output_layout, output, view->x + geo_box.width / 2, view->y + geo_box.height / 2, &closest_x, &closest_y);
	output = wlr_output_layout_output_at(view->server->output_layout, closest_x, closest_y);

	bool is_maximized = surface->toplevel->current.maximized;
	struct wlr_box usable_area = {0};
	if (!is_maximized) {
		wlr_output_effective_resolution(output, &usable_area.width, &usable_area.height);
		view->origdim.height = geo_box.height;
		view->origdim.width = geo_box.width;
		view->origdim.x = view->x;
		view->origdim.y = view->y;
		view->x = 0;
		view->y = 0;
	} else {
		usable_area = view->origdim;
		view->x = view->origdim.x;
		view->y = view->origdim.y;
	}
#if WLR_CHECK_VERSION(0, 16, 0)
	wlr_xdg_toplevel_set_size(surface->toplevel, usable_area.width, usable_area.height);
	wlr_xdg_toplevel_set_maximized(surface->toplevel, !is_maximized);
#else
	wlr_xdg_toplevel_set_size(surface, usable_area.width, usable_area.height);
	wlr_xdg_toplevel_set_maximized(surface, !is_maximized);
#endif
}

static void begin_interactive(struct wb_view *view,
		enum wb_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propagating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct wb_server *server = view->server;
	struct wlr_surface *focused_surface =
		server->seat->seat->pointer_state.focused_surface;
	if (view->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grabbed_view = view;
	server->cursor->cursor_mode = mode;

	if (mode == WB_CURSOR_MOVE) {
		server->grab_x = server->cursor->cursor->x - view->x;
		server->grab_y = server->cursor->cursor->y - view->y;
	} else if (mode == WB_CURSOR_RESIZE) {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);

		double border_x = (view->x + geo_box.x) + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (view->y + geo_box.y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->cursor->x - border_x;
		server->grab_y = server->cursor->cursor->y - border_y;

		server->grab_geo_box = geo_box;
		server->grab_geo_box.x += view->x;
		server->grab_geo_box.y += view->y;

		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_move(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. */
	struct wb_view *view = wl_container_of(listener, view, request_move);
	begin_interactive(view, WB_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct wb_view *view = wl_container_of(listener, view, request_resize);
	begin_interactive(view, WB_CURSOR_RESIZE, event->edges);
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	struct wb_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	/* Allocate a wb_view for this surface */
	struct wb_view *view =
		calloc(1, sizeof(struct wb_view));
	view->server = server;
	view->xdg_toplevel = xdg_surface->toplevel;
#if !WLR_CHECK_VERSION(0, 16, 0)
	view->xdg_surface = xdg_surface;
#endif

	/* Listen to the various events it can emit */
	view->surface_commit.notify = xdg_surface_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &view->surface_commit);

	view->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	struct wlr_xdg_toplevel *toplevel = view->xdg_toplevel;
	view->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

	/* Add it to the list of views. */
	wl_list_insert(&server->views, &view->link);
}

bool view_at(struct wb_view *view,
		double lx, double ly, struct wlr_surface **surface,
		double *sx, double *sy) {
	/*
	 * XDG toplevels may have nested surfaces, such as popup windows for context
	 * menus or tooltips. This function tests if any of those are underneath the
	 * coordinates lx and ly (in output Layout Coordinates). If so, it sets the
	 * surface pointer to that wlr_surface and the sx and sy coordinates to the
	 * coordinates relative to that surface's top-left corner.
	 */
	double view_sx = lx - view->x;
	double view_sy = ly - view->y;

	double _sx, _sy;
	struct wlr_surface *_surface = NULL;
	_surface = wlr_xdg_surface_surface_at(
			view->xdg_toplevel->base, view_sx, view_sy, &_sx, &_sy);

	if (_surface != NULL) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}

	return false;
}

struct wb_view *desktop_view_at(
		struct wb_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This iterates over all of our surfaces and attempts to find one under the
	 * cursor. This relies on server->views being ordered from top-to-bottom. */
	struct wb_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
	}
	return NULL;
}

void init_xdg_shell(struct wb_server *server) {
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display);
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);
}
