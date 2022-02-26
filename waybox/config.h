#ifndef _WB_CONFIG_H
#define _WB_CONFIG_H

#include "waybox/server.h"

enum action_type {
	ACTION_CLOSE = 1,
	ACTION_EXECUTE = 2,
	ACTION_EXIT = 4,
	ACTION_ICONIFY = 8,
	ACTION_NEXT_WINDOW = 16,
	ACTION_PREVIOUS_WINDOW = 32,
	ACTION_RECONFIGURE = 64,
	ACTION_SHADE = 128,
	ACTION_TOGGLE_MAXIMIZE = 256,
	ACTION_UNSHADE = 512,
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
