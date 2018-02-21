#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render.h>


struct wb_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct wlr_backend *backend;

	struct wl_listener new_output;
	struct wl_list outputs; // wb_output::link
};

struct wb_output {
	struct wlr_output *wlr_output;
	struct wb_server *server;
	struct timespec last_frame;
	float color[4];
	int dec;

	struct wl_listener destroy;
	struct wl_listener frame;

	struct wl_list link;
};

static void output_frame_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, frame);
	struct wlr_output *wlr_output = data;
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
								wlr_output->backend);

	wlr_output_make_current(wlr_output, NULL);
	wlr_renderer_begin(renderer, wlr_output);

	float color[4] = {1.0, 0, 0, 1.0}; // Color red
	wlr_renderer_clear(renderer, &output->color);

	wlr_output_swap_buffers(wlr_output, NULL, NULL);
	wlr_renderer_end(renderer);
}

static void output_remove_notify(struct wl_listener *listener, void *data) {
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

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
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

	server.backend = wlr_backend_autocreate(server.wl_display);
	assert(server.backend);

	wl_list_init(&server.outputs);

	server.new_output.notify = new_output_notify;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	if (!wlr_backend_start(server.backend)) {
		fprintf(stderr, "Failed to start backend\n");
		wl_display_destroy(server.wl_display);
		return 1;
	}

	wl_display_run(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
