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
		struct wlr_gamma_control_v1 *gamma_control =
			wlr_gamma_control_manager_v1_get_control(output->server->gamma_control_manager,
					output->wlr_output);
		if (!wlr_gamma_control_v1_apply(gamma_control, &output->wlr_output->pending)) {
			return;
		}

		if (!wlr_output_test(output->wlr_output)) {
			wlr_output_rollback(output->wlr_output);
			wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
		}
	}

	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output, NULL);

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void output_configuration_applied(struct wl_listener *listener, void *data) {
	struct wb_server *server = wl_container_of(listener, server, wlr_output_manager);
	struct wlr_output_configuration_v1 *configuration = data;
	wlr_output_configuration_v1_send_succeeded(configuration);
}

void output_request_state_notify(struct wl_listener *listener, void *data) {
	struct wb_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;

	struct wlr_output_configuration_v1 *configuration = wlr_output_configuration_v1_create();
	wlr_output_manager_v1_set_configuration(output->server->wlr_output_manager, configuration);

	wlr_output_commit_state(output->wlr_output, event->state);
}

void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data) {
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	struct wb_output *output = event->output->data;
	output->gamma_lut_changed = true;
	wlr_output_schedule_frame(output->wlr_output);
}

void output_destroy_notify(struct wl_listener *listener, void *data) {
       	struct wb_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);

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

	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

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
	output->request_state.notify = output_request_state_notify;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	struct wlr_output_layout_output *l_output =
		wlr_output_layout_add_auto(server->output_layout, wlr_output);
	if (!l_output) {
		wlr_log(WLR_ERROR, "%s", _("Could not add an output layout."));
		return;
	}

	struct wlr_output_configuration_v1 *configuration = wlr_output_configuration_v1_create();
	wlr_output_configuration_head_v1_create(configuration, wlr_output);
	wlr_output_manager_v1_set_configuration(server->wlr_output_manager, configuration);

	struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);
}

void init_output(struct wb_server *server) {
	wl_list_init(&server->outputs);

	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	server->wlr_output_manager = wlr_output_manager_v1_create(server->wl_display);
	server->output_configuration_applied.notify = output_configuration_applied;
	wl_signal_add(&server->wlr_output_manager->events.apply, &server->output_configuration_applied);
	wl_signal_add(&server->wlr_output_manager->events.test, &server->output_configuration_applied);
}
