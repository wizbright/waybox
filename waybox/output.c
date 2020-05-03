#include "waybox/output.h"
#include "waybox/util.h"

#include <wlr/render/gles2.h>
#include <wlr/render/wlr_renderer.h>
#include <GLES2/gl2.h>
#include <cairo.h>

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct wb_view *view;
	struct timespec *when;
};

struct wlr_texture *textCache[16] = {
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL
};

static int lastCacheHash = 0;

enum {
	tx_window_title_focus,
	tx_window_title_unfocus,
	tx_window_label_focus,
	tx_window_label_unfocus,
	tx_window_handle_focus,
	tx_window_handle_unfocus,
	tx_window_grip_focus,
	tx_window_grip_unfocus,
};

static void generate_texture(struct wlr_renderer *renderer, int idx, int flags, int w, int h, float color[static 4], float colorTo[static 4]) {
	// printf("generate texture %d\n", idx);

	if (flags == 0) {
		return;

	}
	if (textCache[idx]) {
		wlr_texture_destroy(textCache[idx]);
		textCache[idx] = NULL;
	}

	cairo_surface_t *surf = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, w, h); // numbers pulled from ass
	cairo_t *cx = cairo_create(surf);

	draw_gradient_rect(cx, flags, w, h, color, colorTo);

	unsigned char *data = cairo_image_surface_get_data(surf);
	textCache[idx] = wlr_texture_from_pixels(renderer,
			WL_SHM_FORMAT_ARGB8888,
			cairo_image_surface_get_stride(surf),
			w, h, data);

	// char fname[255] = "";
	// sprintf(fname, "/tmp/text_%d.png", idx);
	// cairo_surface_write_to_png(surf, fname);

	cairo_destroy(cx);
	cairo_surface_destroy(surf);
};

static void generate_textures(struct wlr_renderer *renderer, bool forced) {

	if (textCache[0] != NULL && !(forced || lastCacheHash != server.style.hash)) {
		return;
	}

	lastCacheHash = server.style.hash;

	float color[4];
	float colorTo[4];
	int flags;

	// titlebar
	color_to_rgba(color, server.style.window_title_focus_color);
	color_to_rgba(colorTo, server.style.window_title_focus_colorTo);
	flags = server.style.window_title_focus;
	generate_texture(renderer, tx_window_title_focus, flags, 512, 16, color, colorTo);

	color_to_rgba(color, server.style.window_title_unfocus_color);
	color_to_rgba(colorTo, server.style.window_title_unfocus_colorTo);
	flags = server.style.window_title_unfocus;
	generate_texture(renderer, tx_window_title_unfocus, flags, 512, 16, color, colorTo);

	// titlebar/label
	color_to_rgba(color, server.style.window_label_focus_color);
	color_to_rgba(colorTo, server.style.window_label_focus_colorTo);
	flags = server.style.window_label_focus;
	generate_texture(renderer, tx_window_label_focus, flags, 512, 16, color, colorTo);

	color_to_rgba(color, server.style.window_label_unfocus_color);
	color_to_rgba(colorTo, server.style.window_label_unfocus_colorTo);
	flags = server.style.window_label_unfocus;
	generate_texture(renderer, tx_window_label_unfocus, flags, 512, 16, color, colorTo);

	// handle
	color_to_rgba(color, server.style.window_handle_focus_color);
	color_to_rgba(colorTo, server.style.window_handle_focus_colorTo);
	flags = server.style.window_handle_focus;
	generate_texture(renderer, tx_window_handle_focus, flags, 512, 16, color, colorTo);

	color_to_rgba(color, server.style.window_handle_unfocus_color);
	color_to_rgba(colorTo, server.style.window_handle_unfocus_colorTo);
	flags = server.style.window_handle_unfocus;
	generate_texture(renderer, tx_window_handle_unfocus, flags, 512, 16, color, colorTo);

	// grip
	color_to_rgba(color, server.style.window_grip_focus_color);
	color_to_rgba(colorTo, server.style.window_grip_focus_colorTo);
	flags = server.style.window_grip_focus;
	generate_texture(renderer, tx_window_grip_focus, flags, 30, 16, color, colorTo);

	color_to_rgba(color, server.style.window_grip_unfocus_color);
	color_to_rgba(colorTo, server.style.window_grip_unfocus_colorTo);
	flags = server.style.window_grip_unfocus;
	generate_texture(renderer, tx_window_grip_unfocus, flags, 30, 16, color, colorTo);
}

static void render_rect(struct wlr_output *output, struct wlr_box *box, float color[4]) {
	struct wlr_renderer *renderer = wlr_backend_get_renderer(output->backend);
	wlr_render_rect(renderer, box, color, output->transform_matrix);
}


static void render_texture(struct wlr_output *output, struct wlr_box *box, struct wlr_texture *texture) {
	if (!texture) {
		return;
	}
	
	struct wlr_renderer *renderer = wlr_backend_get_renderer(output->backend);

	struct wlr_gles2_texture_attribs attribs;
	wlr_gles2_texture_get_attribs(texture, &attribs);
	glBindTexture(attribs.target, attribs.tex);
	glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	float matrix[9];
	wlr_matrix_project_box(matrix, box,
		WL_OUTPUT_TRANSFORM_NORMAL,
		0.0, output->transform_matrix);

	wlr_render_texture_with_matrix(renderer, texture, matrix, 1.0);
}

static void render_view_frame(struct wlr_surface *surface, int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	struct render_data *rdata = data;
	struct wb_view *view = rdata->view;
	struct wlr_output *output = rdata->output;
	// struct wlr_renderer *renderer = wlr_backend_get_renderer(output->backend);

	/* The view has a position in layout coordinates. If you have two displays,
	 * one next to the other, both 1080p, a view on the rightmost display might
	 * have layout coordinates of 2000,100. We need to translate that to
	 * output-local coordinates, or (2000 - 1920). */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			view->server->layout, output, &ox, &oy);
	ox += view->x + sx, oy += view->y + sy;

	// ox += 40;
	// oy += 40;

	int border = 2;

	/* We also have to apply the scale factor for HiDPI outputs. This is only
	 * part of the puzzle, Waybox does not fully support HiDPI. */
	struct wlr_box box = {
		.x = (ox - border) * output->scale,
		.y = (oy - border) * output->scale,
		.width = (surface->current.width + border * 2) * output->scale,
		.height = (surface->current.height + border * 2) * output->scale,
	};

	struct wb_view *active_view = server.active_view;
	int focusTextureOffset = (active_view == view) ? 0 : 1;

	float color[4];
	color_to_rgba(color, server.style.borderColor);
	color[3] = 1.0;

	// lazy borders
	render_rect(output, &box, color);

	// titlebar
	int border_thickness = 2;
	int title_bar_height = 28;
	box.y = box.y - title_bar_height;
	box.height = title_bar_height;
	render_rect(output, &box, color);

	box.x += border_thickness;
	box.y += border_thickness;
	box.width -= (border_thickness*2);
	box.height -= (border_thickness*2);
	render_texture(output, &box, textCache[tx_window_title_focus + focusTextureOffset]);

	// label
	box.x += border_thickness;
	box.y += border_thickness;
	box.width -= (border_thickness*2);
	box.height -= (border_thickness*2);
	render_texture(output, &box, textCache[tx_window_label_focus + focusTextureOffset]);

	// footer
	int footer_height = 8;
	box.x = (ox - border) * output->scale;
	box.y = (oy - border) * output->scale;
	box.width = (surface->current.width + border * 2) * output->scale;
	box.height = (surface->current.height + border * 2) * output->scale;

	box.y += box.height;
	box.height = footer_height + border_thickness;
	render_rect(output, &box, color);

	// handle
	box.x += border_thickness;
	box.y += border_thickness;
	box.width -= (border_thickness * 2);
	box.height -= (border_thickness * 2);
	render_texture(output, &box, textCache[tx_window_handle_focus + focusTextureOffset]);
}

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
	struct wlr_renderer *renderer = output->server->renderer;

	generate_textures(renderer, false);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!wlr_output_attach_render(output->wlr_output, NULL)) {
		return;
	}
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	wlr_renderer_begin(renderer, width, height);

	float color[4] = {0.4f, 0.4f, 0.4f, 1.0f};
	wlr_renderer_clear(renderer, color);

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

		wlr_xdg_surface_for_each_surface(view->xdg_surface, render_view_frame, &rdata);
		wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, &rdata);
	}

	wlr_output_render_software_cursors(output->wlr_output, NULL);
	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);
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
	}

	struct wb_output *output = calloc(1, sizeof(struct wb_output));
	output->server = server;
	output->wlr_output = wlr_output;
	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	wlr_output_layout_add_auto(server->layout, wlr_output);
	wlr_output_create_global(wlr_output);
}
