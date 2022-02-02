#ifndef _WB_CONFIG_H
#define _WB_CONFIG_H

#include "waybox/server.h"

struct wb_config {
	struct wb_server *server;
	struct {
		char *layout;
		char *model;
		char *options;
		char *rules;
		char *variant;
	} keyboard_layout;

	struct wl_list applications;
	struct wl_list key_bindings;
};

struct wb_key_binding {
	xkb_keysym_t sym;
	uint32_t modifiers;
	char *action;
	char *cmd;
	struct wl_list link;
};

bool init_config(struct wb_server *server);
void deinit_config(struct wb_config *config);
#endif
