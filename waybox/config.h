#ifndef _WB_CONFIG_H
#define _WB_CONFIG_H

#include "waybox/server.h"

enum action_type {
	ACTION_EXIT = 1 << 0,
	ACTION_NEXT_WINDOW = 1 << 1,
	ACTION_EXECUTE = 1 << 2,
	ACTION_PREVIOUS_WINDOW = 1 << 3,
	ACTION_CLOSE = 1 << 4,
	ACTION_RECONFIGURE = 1 << 5,
	ACTION_TOGGLE_MAXIMIZE = 1 << 6,
	ACTION_ICONIFY = 1 << 7,
	ACTION_SHADE = 1 << 8,
	ACTION_UNSHADE = 1 << 9,
};

struct wb_config {
	struct wb_server *server;
	struct {
		char *layout;
		char *model;
		char *options;
		char *rules;
		char *variant;

		bool use_config;
	} keyboard_layout;
	struct {
		char *accel_profile;
		char *accel_speed;
		char *calibration_matrix;
		char *click_method;
		char *dwt;
		char *dwtp;
		char *left_handed;
		char *middle_emulation;
		char *natural_scroll;
		char *scroll_button;
		char *scroll_button_lock;
		char *scroll_method;
		char *tap;
		char *tap_button_map;
		char *tap_drag;
		char *tap_drag_lock;

		bool use_config;
	} libinput_config;
	struct {
		int bottom;
		int left;
		int right;
		int top;
	} margins;

	struct wl_list applications;
	struct wl_list key_bindings;
};

struct wb_key_binding {
	xkb_keysym_t sym;
	uint32_t modifiers;
	enum action_type action;
	char *cmd;
	struct wl_list link;
};

bool init_config(struct wb_server *server);
void deinit_config(struct wb_config *config);
#endif
