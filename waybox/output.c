#include "waybox/output.h"

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct wb_view *view;
	struct timespec *when;
};

static void render_surface(struct wlr_surface *surface,
				int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	struct render_data *rdata = data;
	struct wb_view *view = rdata->view;
	struct wlr_output *output = rdata->output;

	/* We first obtain a wlr_texture, which is a GPU resource. wlroots
	 * automatically handles negotiating these with the client. The underlying
	 * resource could be an opaque handle passed from the client, or the client
	 * could have sent a pixel buffer which we copied to the GPU, or a few other
	 * means. You don't have to worry about this, wlroots takes care of it. */
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL) {
		wlr_log(WLR_ERROR, "%s", _("Couldn't get a surface texture"));
		return;
	}

	/* The view has a position in layout coordinates. If you have two displays,
	 * one next to the other, both 1080p, a view on the rightmost display might
	 * have layout coordinates of 2000,100. We need to translate that to
	 * output-local coordinates, or (2000 - 1920). */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			view->server->output_layout, output, &ox, &oy);
	ox += view->x + sx, oy += view->y + sy;

	/* We also have to apply the scale factor for HiDPI outputs. This is only
	 * part of the puzzle, Waybox does not fully support HiDPI. */
	struct wlr_box box = {
		.x = ox * output->scale,
		.y = oy * output->scale,
		.width = surface->current.width * output->scale,
		.height = surface->current.height * output->scale,
	};

	/*
	 * Those familiar with OpenGL are also familiar with the role of matrices
	 * in graphics programming. We need to prepare a matrix to render the view
	 * with. wlr_matrix_project_box is a helper which takes a box with a desired
	 * x, y coordinates, width and height, and an output geometry, then
	 * prepares an orthographic projection and multiplies the necessary
	 * transforms to produce a model-view-projection matrix.
	 *
	 * Naturally you can do this any way you like.
	 */
	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	wlr_matrix_project_box(matrix, &box, transform, 0,
		output->transform_matrix);

	/* This takes our matrix, the texture, and an alpha, and performs the actual
	 * rendering on the GPU. */
	wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
	wlr_surface_send_frame_done(surface, rdata->when);
}

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
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	struct wlr_box box;
	memcpy(&box, &layer_surface->geo, sizeof(struct wlr_box));
	wlr_matrix_project_box(matrix, &box, transform, 0,
		output->transform_matrix);
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

void output_frame_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, frame);
	struct wlr_renderer *renderer = output->server->renderer;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!wlr_output_attach_render(output->wlr_output, NULL)) {
		wlr_log_errno(WLR_ERROR, "%s", _("Couldn't attach renderer to output"));
		return;
	}
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	wlr_renderer_begin(renderer, width, height);

	float color[4] = {0.4f, 0.4f, 0.4f, 1.0f};
	wlr_renderer_clear(renderer, color);

	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

	struct wb_view *view;
	wl_list_for_each_reverse(view, &output->server->views, link) {
		if (!view->mapped)
			/* An unmapped view should not be rendered */
			continue;

		struct render_data rdata = {
			.output = output->wlr_output,
			.renderer = renderer,
			.view = view,
			.when = &now,
		};

		if (view->xdg_toplevel->base->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
			wlr_xdg_surface_for_each_surface(view->xdg_toplevel->base,
							render_surface, &rdata);
	}

	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

	wlr_output_render_software_cursors(output->wlr_output, NULL);
	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);
}

void output_destroy_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, destroy);
	wlr_output_layout_remove(output->server->output_layout, output->wlr_output);
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
