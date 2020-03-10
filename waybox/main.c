#include <stdio.h>
#include <unistd.h>
#include <wayland-server.h>

#include "waybox/server.h"

bool show_help(char *name)
{
	printf(_("Syntax: %s [options]\n"), name);
	printf(_("\nOptions:\n"));
	printf(_("  --help              Display this help and exit\n"));
	printf(_("  --version           Display the version and exit\n"));
	/* TRANSLATORS: If you translate FILE, be sure the text remains aligned. */
	printf(_("  --config-file FILE  Specify the path to the config file to use\n"));
	printf(_("  --sm-disable        Disable connection to the session manager\n"));
	printf(_("  --startup CMD       Run CMD after starting\n"));
	printf(_("  --debug             Display debugging output\n"));
	printf(_("\nOther Openbox options aren't accepted, "
				"mostly due to them being nonsensical on Wayland.\n"));

	return true;
}

int main(int argc, char **argv) {
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	textdomain(GETTEXT_PACKAGE);

	char *startup_cmd = NULL;
	bool debug = false;
	if (argc > 1) {
		int i;
		for (i = 0; i < argc; i++) {
			if (!strcmp("--debug", argv[i]) || !strcmp("-v", argv[i])) {
				debug = true;
			} else if ((!strcmp("--startup", argv[i]) || !strcmp("-s", argv[i]))) {
				if (i < argc - 1) {
					startup_cmd = argv[i + 1];
				} else {
					fprintf(stderr, _("%s requires an argument\n"), argv[i]);
				}
			} else if (!strcmp("--version", argv[i]) || !strcmp("-V", argv[i])) {
				printf(PACKAGE_NAME " " PACKAGE_VERSION "\n");
				return 0;
			} else if (!strcmp("--help", argv[i]) || !strcmp("-h", argv[i])) {
				show_help(argv[0]);
				return 0;
			} else if (!strcmp("--config-file", argv[i]) ||
					!strcmp("--sm-disable", argv[i])) {
				fprintf(stderr, _("%s hasn't been implemented yet.\n"), argv[i]);
				if (i == argc - 1) {
					fprintf(stderr, _("%s requires an argument\n"), argv[i]);
				}
			} else if (argv[i][0] == '-') {
				show_help(argv[0]);
				return 1;
			}
		}
	}

	wlr_log_init(debug ? WLR_DEBUG : WLR_ERROR, NULL);
	struct wb_server server = {0};

	if (wb_create_backend(&server)) {
		wlr_log(WLR_INFO, "%s", _("Successfully created backend"));
	} else {
		wlr_log(WLR_ERROR, "%s", _("Failed to create backend"));
		exit(EXIT_FAILURE);
	}

	if (wb_start_server(&server)) {
		wlr_log(WLR_INFO, "%s", _("Successfully started server"));
	} else {
		wlr_log(WLR_ERROR, "%s", _("Failed to start server"));
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
