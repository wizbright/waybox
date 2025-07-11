/*
 * More or less taken verbatim from wio <https://git.sr.ht/~sircmpwn/wio>.
 * Additional material taken from sway <https://github.com/swaywm/sway>.
 *
 * Copyright 2019 Drew DeVault
 * Copyright 2022 Sway Developers
 */
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>

#include "waybox/xdg_shell.h"

static void descriptor_destroy(struct wb_scene_descriptor *desc) {
	if (!desc) {
		return;
	}

	wl_list_remove(&desc->destroy.link);

	free(desc);
}

static void handle_descriptor_destroy(struct wl_listener *listener, void *data) {
	struct wb_scene_descriptor *desc =
		wl_container_of(listener, desc, destroy);

	descriptor_destroy(desc);
}

void assign_scene_descriptor(struct wlr_scene_node *node,
		enum wb_scene_descriptor_type type, void *data) {
	struct wb_scene_descriptor *desc =
		calloc(1, sizeof(struct wb_scene_descriptor));

	if (!desc) {
		return;
	}

	desc->type = type;
	desc->data = data;

	desc->destroy.notify = handle_descriptor_destroy;
	wl_signal_add(&node->events.destroy, &desc->destroy);

	node->data = desc;
}

static void arrange_surface(struct wb_output *output, struct wlr_box *full_area,
		struct wlr_box *usable_area, struct wlr_scene_tree *scene_tree) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &scene_tree->children, link) {
		struct wb_scene_descriptor *desc = node->data;

		if (desc->type == WB_SCENE_DESC_LAYER_SHELL) {
			struct wb_layer_surface *surface = desc->data;
			surface->scene->layer_surface->initialized = true;
			wlr_scene_layer_surface_v1_configure(surface->scene,
					full_area, usable_area);
		}
	}
}

void arrange_layers(struct wb_output *output) {
	struct wlr_box usable_area = { 0 };
	struct wlr_box full_area;
	wlr_output_effective_resolution(output->wlr_output,
			&usable_area.width, &usable_area.height);

	memcpy(&full_area, &usable_area, sizeof(struct wlr_box));

	arrange_surface(output, &full_area, &usable_area, output->layers.shell_background);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_bottom);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_top);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_overlay);
}

static struct wlr_scene_tree *wb_layer_get_scene(struct wb_output *output,
		enum zwlr_layer_shell_v1_layer type) {
	switch (type) {
		case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
			return output->layers.shell_background;
		case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
			return output->layers.shell_bottom;
		case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
			return output->layers.shell_top;
		case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
			return output->layers.shell_overlay;
	}

	/* Unreachable */
	return NULL;
}

static struct wb_layer_surface *wb_layer_surface_create(
		struct wlr_scene_layer_surface_v1 *scene) {
	struct wb_layer_surface *surface = calloc(1, sizeof(struct wb_layer_surface));
	if (!surface) {
		return NULL;
	}

	surface->scene = scene;

	return surface;
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct wb_layer_surface *surface =
		wl_container_of(listener, surface, surface_commit);
	struct wb_toplevel *current_toplevel =
		wl_container_of(surface->server->toplevels.next, current_toplevel, link);

	if (!surface->output || current_toplevel->xdg_toplevel->current.fullscreen) {
		return;
	}
	wlr_fractional_scale_v1_notify_scale(surface->scene->layer_surface->surface, surface->output->wlr_output->scale);

	struct wlr_layer_surface_v1 *layer_surface = surface->scene->layer_surface;
	uint32_t committed = layer_surface->current.committed;

	enum zwlr_layer_shell_v1_layer layer_type = layer_surface->current.layer;
	struct wlr_scene_tree *output_layer = wb_layer_get_scene(
		surface->output, layer_type);

	if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		wlr_scene_node_reparent(&surface->scene->tree->node, output_layer);
	}

	if (committed || layer_surface->surface->mapped != surface->mapped) {
		surface->mapped = layer_surface->surface->mapped;
		arrange_layers(surface->output);

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		wlr_surface_send_frame_done(layer_surface->surface, &now);
	}

	if (layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
		wlr_scene_node_raise_to_top(&output_layer->node);
	}

	if (layer_surface == surface->server->seat->focused_layer) {
		seat_focus_surface(surface->server->seat, layer_surface->surface);
	}
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct wb_layer_surface *surface = wl_container_of(listener,
			surface, map);

	struct wlr_layer_surface_v1 *layer_surface =
				surface->scene->layer_surface;

	/* focus on new surface */
	if (layer_surface->current.keyboard_interactive &&
			(layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
			layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {

		struct wb_seat *seat = surface->server->seat;
		/* but only if the currently focused layer has a lower precedence */
		if (!seat->focused_layer ||
				seat->focused_layer->current.layer >= layer_surface->current.layer) {
			seat_set_focus_layer(seat, layer_surface);
		}
		arrange_layers(surface->output);
	}
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct wb_layer_surface *surface = wl_container_of(
			listener, surface, unmap);

	struct wb_seat *seat = surface->server->seat;
	if (seat->focused_layer == surface->scene->layer_surface) {
		seat_set_focus_layer(seat, NULL);
	}

	if (!wl_list_empty(&surface->server->toplevels)) {
		struct wb_toplevel *toplevel =
			wl_container_of(surface->server->toplevels.next, toplevel, link);
		if (toplevel && toplevel->scene_tree && toplevel->scene_tree->node.enabled) {
			focus_toplevel(toplevel);
		}
	}
}

static void wb_layer_surface_destroy(struct wb_layer_surface *surface) {
	if (surface == NULL) {
		return;
	}

	if (surface->scene->layer_surface->surface != NULL)
		wlr_fractional_scale_v1_notify_scale(surface->scene->layer_surface->surface,
				surface->output->wlr_output->scale);

	wl_list_remove(&surface->map.link);
	wl_list_remove(&surface->unmap.link);
	wl_list_remove(&surface->surface_commit.link);
	wl_list_remove(&surface->destroy.link);

	free(surface);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct wb_layer_surface *surface =
		wl_container_of(listener, surface, destroy);

	if (surface->output) {
		arrange_layers(surface->output);
	}

	wl_list_remove(&surface->new_popup.link);
	wb_layer_surface_destroy(surface);
}

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
	struct wb_layer_popup *popup =
		wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	free(popup);
}

static struct wb_layer_surface *popup_get_layer(
		struct wb_layer_popup *popup) {
	struct wlr_scene_node *current = &popup->scene->node;
	while (current) {
		if (current->data) {
			struct wb_scene_descriptor *desc = current->data;
			if (desc->type == WB_SCENE_DESC_LAYER_SHELL) {
				return desc->data;
			}
		}

		current = &current->parent->node;
	}

	return NULL;
}

static void popup_unconstrain(struct wb_layer_popup *popup) {
	struct wb_layer_surface *surface = popup_get_layer(popup);
	if (!surface) {
		return;
	}

	struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
	struct wb_output *output = surface->output;

	int lx, ly;
	wlr_scene_node_coords(&popup->scene->node, &lx, &ly);

	/* The output box expressed in the coordinate system of the toplevel
	 * parent of the popup. */
	struct wlr_box output_toplevel_sx_box = {
		.x = output->geometry.x - MIN(lx, 0),
		.y = output->geometry.y - MAX(ly, 0),
		.width = output->geometry.width,
		.height = output->geometry.height,
	};

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data);

static struct wb_layer_popup *create_popup(struct wlr_xdg_popup *wlr_popup,
			struct wlr_scene_tree *parent) {
	struct wb_layer_popup *popup =
		calloc(1, sizeof(struct wb_layer_popup));
	if (popup == NULL) {
		return NULL;
	}

	popup->wlr_popup = wlr_popup;

	popup->scene = wlr_scene_xdg_surface_create(parent,
			wlr_popup->base);

	if (!popup->scene) {
		free(popup);
		return NULL;
	}

	assign_scene_descriptor(&popup->scene->node, WB_SCENE_DESC_LAYER_SHELL_POPUP,
			popup);

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);

	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

	popup_unconstrain(popup);

	return popup;
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data) {
	struct wb_layer_popup *layer_popup =
		wl_container_of(listener, layer_popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	create_popup(wlr_popup, layer_popup->scene);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct wb_layer_surface *wb_layer_surface =
		wl_container_of(listener, wb_layer_surface, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;

	create_popup(wlr_popup, wb_layer_surface->scene->tree);
}

void handle_layer_shell_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;

	if (layer_surface->output == NULL) {
		struct wb_server *server =
			wl_container_of(listener, server, new_layer_surface);
		if (wl_list_length(&server->toplevels) == 0) return;
		struct wb_toplevel *toplevel =
			wl_container_of(server->toplevels.next, toplevel, link);
		layer_surface->output = get_active_output(toplevel);
	}
	struct wb_output *output = layer_surface->output->data;

	if (!layer_surface->output) {
		/* Assign last active output */
		layer_surface->output = output->wlr_output;
	}


	enum zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;
	struct wlr_scene_tree *output_layer = wb_layer_get_scene(
			output, layer_type);
	struct wlr_scene_layer_surface_v1 *scene_surface =
		wlr_scene_layer_surface_v1_create(output_layer, layer_surface);
	if (!scene_surface) {
		return;
	}

	struct wb_layer_surface *surface =
		wb_layer_surface_create(scene_surface);

	assign_scene_descriptor(&scene_surface->tree->node,
		WB_SCENE_DESC_LAYER_SHELL, surface);
	if (!scene_surface->tree->node.data) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	surface->output = output;
	surface->server = output->server;

	surface->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&surface->surface_commit);
	surface->map.notify = handle_map;
	wl_signal_add(&layer_surface->surface->events.map, &surface->map);
	surface->unmap.notify = handle_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &surface->unmap);
	surface->destroy.notify = handle_destroy;
	wl_signal_add(&layer_surface->events.destroy, &surface->destroy);
	surface->new_popup.notify = handle_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

	/* Temporarily set the layer's current state to pending
	 * So that we can easily arrange it */
	struct wlr_layer_surface_v1_state old_state = layer_surface->current;
	layer_surface->current = layer_surface->pending;
	arrange_layers(output);
	layer_surface->current = old_state;
}

void init_layer_shell(struct wb_server *server) {
	server->layer_shell = wlr_layer_shell_v1_create(server->wl_display, 4);
	server->new_layer_surface.notify = handle_layer_shell_surface;
	wl_signal_add(&server->layer_shell->events.new_surface,
			&server->new_layer_surface);
}
