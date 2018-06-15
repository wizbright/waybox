#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_matrix.h>
//#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_primary_selection.h>


struct wb_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct wlr_backend *backend;
	struct wlr_compositor *compositor;

	struct wl_listener new_output;
	struct wl_list outputs; // wb_output::link
};

struct wb_output {
	struct wlr_output *wlr_output;
	struct wb_server *server;
	struct timespec last_frame;

	struct wl_listener destroy;
	struct wl_listener frame;

	struct wl_list link;
};

static void output_frame_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, frame);
//	struct wlr_backend *backend = output->server->backend;
	struct wlr_output *wlr_output = data;
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
								wlr_output->backend);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	wlr_output_make_current(wlr_output, NULL);
	wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

	float color[4] = {0.4f, 0.4f, 0.4f, 1.0f};
	wlr_renderer_clear(renderer, color);

	struct wl_resource *_surface;
	wl_resource_for_each(_surface, &output->server->compositor->surface_resources) {
		struct wlr_surface *surface = wlr_surface_from_resource(_surface);
		if (!wlr_surface_has_buffer(surface)) {
			continue;
		}
		struct wlr_box render_box = {
			.x = 20, .y = 20,
			.width = surface->current->width,
			.height = surface->current->height
		};
		float matrix[16];
        wlr_matrix_project_box(matrix, &render_box, surface->current->transform,
			    0, wlr_output->transform_matrix);
	    wlr_render_texture_with_matrix(renderer, surface->texture, matrix, 1.0f);
	    wlr_surface_send_frame_done(surface, &now);
	}

	wlr_output_swap_buffers(wlr_output, NULL, NULL);
	wlr_renderer_end(renderer);
	output->last_frame = now;
}

static void output_destroy_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, destroy);
	wl_list_remove(&output->link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->frame.link);
	free(output);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
	struct wb_server *server = wl_container_of(
			listener, server, new_output
			);
	struct wlr_output *wlr_output = data;

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);

		wlr_output_create_global(wlr_output);
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
}

int main(int argc, char **argv){
	struct wb_server server;

	server.wl_display = wl_display_create();
	assert(server.wl_display);
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
	assert(server.wl_event_loop);

	server.backend = wlr_backend_autocreate(server.wl_display, NULL);
	assert(server.backend);

	wl_list_init(&server.outputs);

	server.new_output.notify = new_output_notify;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	assert(socket);

	if (!wlr_backend_start(server.backend)) {
		fprintf(stderr, "Failed to start backend\n");
		wl_display_destroy(server.wl_display);
		return 1;
	}

	printf("Running compositer on wayland display '%s'\n", socket);
	setenv("WAYLAND_DISPLAY", socket, true);

	wl_display_init_shm(server.wl_display);
	wlr_gamma_control_manager_create(server.wl_display);
	wlr_screenshooter_create(server.wl_display);
	wlr_primary_selection_device_manager_create(server.wl_display);
	wlr_idle_create(server.wl_display);

	server.compositor = wlr_compositor_create(server.wl_display,
			wlr_backend_get_renderer(server.backend));

//	wlr_xdg_shell_v6_create(server.wl_display);
    wlr_idle_create(server.wl_display);

	wl_display_run(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
