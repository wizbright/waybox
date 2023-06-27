#include "waybox/output.h"

void output_frame_notify(struct wl_listener *listener, void *data) {
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct wb_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;
	struct wlr_scene_output *scene_output =
		wlr_scene_get_scene_output(scene, output->wlr_output);

	wlr_output_layout_get_box(output->server->output_layout,
			output->wlr_output, &output->geometry);
#if ! WLR_CHECK_VERSION(0, 18, 0)
	/* Update the background for the current output size. */
	wlr_scene_rect_set_size(output->background,
			output->geometry.width, output->geometry.height);
#endif

	if (output->gamma_lut_changed) {
		output->gamma_lut_changed = false;
#if WLR_CHECK_VERSION(0, 17, 0)
		struct wlr_gamma_control_v1 *gamma_control =
			wlr_gamma_control_manager_v1_get_control(output->server->gamma_control_manager,
					output->wlr_output);
		if (!wlr_gamma_control_v1_apply(gamma_control, &output->wlr_output->pending)) {
			return;
		}
#endif
		if (!wlr_output_test(output->wlr_output)) {
			wlr_output_rollback(output->wlr_output);
#if WLR_CHECK_VERSION(0, 17, 0)
			wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
#endif
		}
	}

	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output);

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

#if WLR_CHECK_VERSION(0, 17, 0)
void output_request_state_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
}

void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data) {
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	struct wb_output *output = event->output->data;
	output->gamma_lut_changed = true;
	wlr_output_schedule_frame(output->wlr_output);
}
#endif

void output_destroy_notify(struct wl_listener *listener, void *data) {
       	struct wb_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->frame.link);
#if WLR_CHECK_VERSION(0, 17, 0)
	wl_list_remove(&output->request_state.link);
#endif

	/* Frees the layers */
	size_t num_layers = sizeof(output->layers) / sizeof(struct wlr_scene_node *);
	for (size_t i = 0; i < num_layers; i++) {
		struct wlr_scene_node *node =
			((struct wlr_scene_node **) &output->layers)[i];
		wlr_scene_node_destroy(node);
	}

	wl_list_remove(&output->link);
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

#if WLR_CHECK_VERSION(0, 17, 0)
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	wlr_output_state_finish(&state);
	wlr_output_commit_state(wlr_output, &state);
#else
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);

		if (!wlr_output_commit(wlr_output)) {
			wlr_log_errno(WLR_ERROR, "%s", _("Couldn't commit pending frame to output"));
			return;
		}
	}
#endif

	struct wb_output *output = calloc(1, sizeof(struct wb_output));
	output->server = server;
	output->wlr_output = wlr_output;
	wlr_output->data = output;

#if ! WLR_CHECK_VERSION(0, 18, 0)
	/* Set the background color */
	float color[4] = {0.1875, 0.1875, 0.1875, 1.0};
	output->background = wlr_scene_rect_create(&server->scene->tree, 0, 0, color);
	wlr_scene_node_lower_to_bottom(&output->background->node);
#endif

	/* Initializes the layers */
	size_t num_layers = sizeof(output->layers) / sizeof(struct wlr_scene_node *);
	for (size_t i = 0; i < num_layers; i++) {
		((struct wlr_scene_node **) &output->layers)[i] =
			&wlr_scene_tree_create(&server->scene->tree)->node;
	}

	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
#if WLR_CHECK_VERSION(0, 17, 0)
	output->request_state.notify = output_request_state_notify;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);
#endif

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
#if WLR_CHECK_VERSION(0, 17, 0)
	if (!wlr_output_layout_add_auto(server->output_layout, wlr_output)) {
		wlr_log(WLR_ERROR, "%s", _("Could not add an output layout."));
		return;
	}
#else
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
#endif
}
