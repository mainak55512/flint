#include "flint_err.h"
#include <flint.h>

typedef struct {
	const char *name;
	const char *description;
} CommandHelp;

static const CommandHelp commands[] = {
	{"init", "Initialize a new project"},
	{"add", "Add a dependency"},
	{"remove", "Remove a dependency"},
	{"add-lib", "Add a library"},
	{"add-flag", "Add a compiler flag"},
	{"build", "Build the project"},
	{"run", "Build and run the project"},
	{"gen", "Generate compile_commands.json"},
	{"sync", "Synchronize dependencies"},
	{"deps", "List dependencies"},
	{"version", "Print current version of flint"},
	{"help", "List all available flint commands"},
	{"cache", "clear cache of the project"},
};

void print_version_details() {
	printf("\nVersion: %s\n", STR(PROJECT_VERSION));
}

void print_help_message() {
	printf("\nUsage: flint <command> [args]\n\n");
	printf("Commands:\n");

	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		printf("  %-10s %s\n", commands[i].name, commands[i].description);
	}
}

flint_err_t cli(int argc, char *argv[], Arena *global_str_arena) {
	if (argc < 2) {
		print_help_message();
		return FLINT_ERR_UNKNOWN;
	}
	char *opt = argv[1];
	if (STR_CMP(opt, "help") == 0) {
		print_help_message();
		return FLINT_OK;
	} else if (STR_CMP(opt, "init") == 0) {
		return init_project();
	} else if (STR_CMP(opt, "add") == 0) {
		add_lib(argv[2]);
		return FLINT_OK;
	} else if (STR_CMP(opt, "remove") == 0) {
		remove_library(argv[2]);
		return FLINT_OK;
	} else if (STR_CMP(opt, "add-lib") == 0) {
		add_local_lib(argc - 2, argv + 2);
		return FLINT_OK;
	} else if (STR_CMP(opt, "add-flag") == 0) {
		add_flag(argc - 2, argv + 2);
		return FLINT_OK;
	} else if (STR_CMP(opt, "build") == 0) {
		return build_project(global_str_arena).err;
	} else if (STR_CMP(opt, "run") == 0) {
		return run_project(global_str_arena);
	} else if (STR_CMP(opt, "gen") == 0) {
		flint_res_config_t conf_wraper = config_init();
		if (conf_wraper.err != FLINT_OK) {
			flint_error("Generate compile_commands", conf_wraper.err);
			return conf_wraper.err;
		}
		Config *conf = conf_wraper.value;
		flint_err_t conf_read_status =
			read_flint_composition("composition.json", conf);
		if (conf_read_status != FLINT_OK) {
			flint_error("Generate compile_commands", conf_read_status);
			return conf_read_status;
		}
		generate_compile_commands(conf);
		config_free(conf);
		return FLINT_OK;
	} else if (STR_CMP(opt, "sync") == 0) {
		sync_lib(argv[2]);
		return FLINT_OK;
	} else if (STR_CMP(opt, "deps") == 0) {
		list_deps();
		return FLINT_OK;
	} else if (STR_CMP(opt, "version") == 0) {
		print_version_details();
		return FLINT_OK;
	} else if (STR_CMP(opt, "cache") == 0) {
		clear_cache();
		return FLINT_OK;
	} else {
		printf("Unknown command: %s\n", opt);
		print_help_message();
		return FLINT_ERR_UNKNOWN;
	}
}
