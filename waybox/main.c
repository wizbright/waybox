#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>

#include "waybox/server.h"

int main(int argc, char **argv) {
	textdomain(GETTEXT_PACKAGE);
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	setlocale(LC_ALL, NULL);

	char *startup_cmd = NULL;
	bool debug = false;
	if (argc > 0) {
		int i;
		for (i = 0; i < argc; i++) {
			if (!strcmp("--debug", argv[i]) || !strcmp("-v", argv[i])) {
				debug = true;
			} else if (!strcmp("--exit", argv[i])) {
				fprintf(stderr, _("Warning: option %s is currently unimplemented\n"), argv[i]);
			} else if ((!strcmp("--startup", argv[i]) || !strcmp("-s", argv[i]))) {
				if (i < argc - 1) {
					startup_cmd = argv[i + 1];
				} else {
					fprintf(stderr, _("%s requires an argument\n"), argv[i]);
				}
			} else if (!strcmp("--version", argv[i]) || !strcmp("-V", argv[i])) {
				printf(PACKAGE_NAME " " PACKAGE_VERSION "\n");
				return 0;
			} else if (argv[i][0] == '-') {
				printf(_("Usage: %s [--debug] [--exit] [--help] [--startup CMD] [--version]\n"), argv[0]);
				return strcmp("--help", argv[i]) != 0 && strcmp("-h", argv[i]) != 0;
			}
		}
	}

	wlr_log_init(debug ? WLR_DEBUG : WLR_ERROR, NULL);
	struct wb_server server = {0};

	if (!wb_create_backend(&server)) {
		wlr_log(WLR_ERROR, "%s\n", _("Failed to create backend"));
		exit(EXIT_FAILURE);
	}

	if (!wb_start_server(&server)) {
		wlr_log(WLR_ERROR, "%s\n", _("Failed to start server"));
		wb_terminate(&server);
		exit(EXIT_FAILURE);
	}

	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (char *) NULL);
		}
	}

	wl_display_run(server.wl_display);

	wb_terminate(&server);

	return 0;
}
