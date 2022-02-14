#include "decoration.h"

static void destroy_xdg_toplevel_decoration(struct wl_listener *listener, void *data)
{
	struct wb_decoration *decoration = wl_container_of(listener, decoration, toplevel_decoration_destroy);
	wl_list_remove(&decoration->toplevel_decoration_destroy.link);
	free(decoration);
}

static void free_xdg_decoration_mode(struct wl_listener *listener, void *data)
{
	struct wb_decoration *decoration = wl_container_of(listener, decoration, mode_destroy);
	wl_list_remove(&decoration->mode_destroy.link);
	wl_list_remove(&decoration->request_mode.link);
}

static void handle_xdg_decoration_mode(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *toplevel_decoration = data;
	struct wb_decoration *decoration = wl_container_of(listener, decoration, request_mode);
	wlr_xdg_toplevel_decoration_v1_set_mode(toplevel_decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
	if (decoration->server)
	{
		struct wb_view *view = wl_container_of(decoration->server->views.next, view, link);
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(toplevel_decoration->surface, &geo_box);
		/* TODO: Figure out how to get the actual height of the CSD
		 * titlebar, which may or may not be 30px.
		 * Logically, geo_box.height should have the height of the
		 * decoration, but for some reason, it's 0. */
		/* view->y += geo_box.height; */
		view->y = view->y > 0 ? view->y : (geo_box.height > 0 ? geo_box.height : 30);
		wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
		wlr_xdg_toplevel_set_size(view->xdg_surface, geo_box.width - view->x, geo_box.height - view->y);
	}
}

static void handle_new_xdg_toplevel_decoration(struct wl_listener *listener, void *data)
{
	struct wb_decoration *decoration = (struct wb_decoration *) calloc(1, sizeof(struct wb_decoration));
	handle_xdg_decoration_mode(&decoration->request_mode, data);
	struct wb_server *server = wl_container_of(listener, server, new_xdg_decoration);
	decoration->server = server;
	struct wlr_xdg_toplevel_decoration_v1 *toplevel_decoration = data;
	decoration->toplevel_decoration_destroy.notify = destroy_xdg_toplevel_decoration;
	wl_signal_add(&toplevel_decoration->manager->events.destroy, &decoration->toplevel_decoration_destroy);
	decoration->request_mode.notify = handle_xdg_decoration_mode;
	wl_signal_add(&toplevel_decoration->events.request_mode, &decoration->request_mode);
	decoration->mode_destroy.notify = free_xdg_decoration_mode;
	wl_signal_add(&toplevel_decoration->events.destroy, &decoration->mode_destroy);
	/* TODO: Only call when the user's settings specify to always apply
	 * decorations */
	handle_xdg_decoration_mode(&decoration->request_mode, toplevel_decoration);
}

void init_xdg_decoration(struct wb_server *server)
{
	struct wlr_xdg_decoration_manager_v1 *decoration = wlr_xdg_decoration_manager_v1_create(server->wl_display);
	server->new_xdg_decoration.notify = handle_new_xdg_toplevel_decoration;
	wl_signal_add(&decoration->events.new_toplevel_decoration, &server->new_xdg_decoration);
}
