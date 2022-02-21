#ifndef _WB_CONFIG_H
#define _WB_CONFIG_H

#include "waybox/server.h"

enum action_type {
	ACTION_FIRST,
	ACTION_CLOSE,
	ACTION_EXECUTE,
	ACTION_EXIT,
	ACTION_NEXT_WINDOW,
	ACTION_PREVIOUS_WINDOW,
	ACTION_RECONFIGURE
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
