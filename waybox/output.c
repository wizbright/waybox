#include "waybox/output.h"

#if !WLR_CHECK_VERSION(0, 16, 0)
static void render_layer_surface(struct wlr_surface *surface,
		int sx, int sy, void *data) {
	struct wb_layer_surface *layer_surface = data;
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL) {
		return;
	}
	struct wlr_output *output = layer_surface->layer_surface->output;
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			layer_surface->server->output_layout, output, &ox, &oy);
	ox += layer_surface->geo.x + sx, oy += layer_surface->geo.y + sy;
	float matrix[9];
	struct wlr_box box;
	memcpy(&box, &layer_surface->geo, sizeof(struct wlr_box));
	wlr_render_texture_with_matrix(layer_surface->server->renderer,
			texture, matrix, 1);
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_surface_send_frame_done(surface, &now);
}

static void render_layer(
		struct wb_output *output, struct wl_list *layer_surfaces) {
	struct wb_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface_v1 =
			layer_surface->layer_surface;
		wlr_surface_for_each_surface(wlr_layer_surface_v1->surface,
			render_layer_surface, layer_surface);
	}
}
#endif

void output_frame_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
		scene, output->wlr_output);

	wlr_scene_rect_set_size(output->scene_rect, output->wlr_output->width, output->wlr_output->height);

#if !WLR_CHECK_VERSION(0, 16, 0)
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);
#endif

	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output);

#if !WLR_CHECK_VERSION(0, 16, 0)
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
#endif

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void output_destroy_notify(struct wl_listener *listener, void *data) {
       	struct wb_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->frame.link);

	free(output);
}

void new_output_notify(struct wl_listener *listener, void *data) {
	struct wb_server *server = wl_container_of(
			listener, server, new_output
			);
	struct wlr_output *wlr_output = data;
	wlr_log(WLR_INFO, "%s: %s", _("New output device detected"), wlr_output->name);

	/* Configures the output created by the backend to use our allocator
         * and our renderer */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);

		if (!wlr_output_commit(wlr_output)) {
			wlr_log_errno(WLR_ERROR, "%s", _("Couldn't commit pending frame to output"));
			return;
		}
	}

	struct wb_output *output = calloc(1, sizeof(struct wb_output));
	output->server = server;
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	wl_list_insert(&server->outputs, &output->link);

	float color[4] = {0.3, 0.3, 0.3, 1.0};
	output->scene_rect = wlr_scene_rect_create(&server->scene->node, 0, 0, color);

	wl_list_init(&output->layers[0]);
	wl_list_init(&output->layers[1]);
	wl_list_init(&output->layers[2]);
	wl_list_init(&output->layers[3]);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}
