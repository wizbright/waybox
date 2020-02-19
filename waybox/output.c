#include <waybox/output.h>

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct wb_view *view;
	struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
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
		return;
	}

	/* The view has a position in layout coordinates. If you have two displays,
	 * one next to the other, both 1080p, a view on the rightmost display might
	 * have layout coordinates of 2000,100. We need to translate that to
	 * output-local coordinates, or (2000 - 1920). */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			view->server->layout, output, &ox, &oy);
	ox += view->x + sx, oy += view->y + sy;

	/* We also have to apply the scale factor for HiDPI outputs. This is only
	 * part of the puzzle, TinyWL does not fully support HiDPI. */
	struct wlr_box box = {
		.x = ox * output->scale,
		.y = oy * output->scale,
		.width = surface->current.width * output->scale,
		.height = surface->current.height * output->scale,
	};

	/*
	 * Those familiar with OpenGL are also familiar with the role of matricies
	 * in graphics programming. We need to prepare a matrix to render the view
	 * with. wlr_matrix_project_box is a helper which takes a box with a desired
	 * x, y coordinates, width and height, and an output geometry, then
	 * prepares an orthographic projection and multiplies the necessary
	 * transforms to produce a model-view-projection matrix.
	 *
	 * Naturally you can do this any way you like, for example to make a 3D
	 * compositor.
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



void output_frame_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, frame);
//	struct wlr_backend *backend = output->server->backend;
	struct wlr_output *wlr_output = data;
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
								wlr_output->backend);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!wlr_output_attach_render(wlr_output, NULL)) {
		return;
	}
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	wlr_renderer_begin(renderer, width, height);

	float color[4] = {0.4f, 0.4f, 0.4f, 1.0f};
	wlr_renderer_clear(renderer, color);

	struct wb_view *view;
	wl_list_for_each_reverse(view, &output->server->views, link)
	{
		if (!view->mapped)
			/* An unmapped view should not be rendered */
			continue;

		struct render_data rdata = {
			.output = output->wlr_output,
			.renderer = renderer,
			.view = view,
			.when = &now,
		};

		wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, &rdata);
	}

	/*struct wl_resource *_surface;
	wl_resource_for_each(_surface, &output->server->compositor->surface_resources) {
		struct wlr_surface *surface = wlr_surface_from_resource(_surface);
		if (!wlr_surface_has_buffer(surface)) {
			continue;
		}
		struct wlr_box render_box = {
			.x = 20, .y = 20,
			.width = surface->current.width,
			.height = surface->current.height
		};
		float matrix[16];
        wlr_matrix_project_box(matrix, &render_box, surface->current.transform,
			    0, wlr_output->transform_matrix);
		struct wlr_texture *texture = wlr_surface_get_texture(surface);
	    wlr_render_texture_with_matrix(renderer, texture, matrix, 1.0f);
	    wlr_surface_send_frame_done(surface, &now);
	}*/

	wlr_renderer_end(renderer);
	wlr_output_commit(wlr_output);
	output->last_frame = now;
}

void output_destroy_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, destroy);
	wlr_output_layout_remove(output->server->layout, output->wlr_output);
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

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);

		if (!wlr_output_commit(wlr_output)) {
			return;
		}
		//wlr_output_create_global(wlr_output);
	}

	struct wb_output *output = calloc(1, sizeof(struct wb_output));
	clock_gettime(CLOCK_MONOTONIC, &output->last_frame);
	output->server = server;
	output->wlr_output = wlr_output;
	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	wlr_output_layout_add_auto(server->layout, wlr_output);
	wlr_output_create_global(wlr_output);
	wlr_xcursor_manager_load(server->cursor->xcursor_manager, output->wlr_output->scale);
	wlr_xcursor_manager_set_cursor_image(server->cursor->xcursor_manager, "left_ptr", server->cursor->cursor);
}
