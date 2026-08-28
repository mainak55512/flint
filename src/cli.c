#include <flint.h>

void print_version_details() {
	printf("Version: 0.3.0\n\n");
	printf("Usage: flint <command> [args]\n");
	printf("Commands: init, add, build, add-lib, add-flag, run, gen, "
		   "sync\n");
}

int cli(int argc, char *argv[], Arena *global_str_arena) {
	if (argc < 2) {
		print_version_details();
		return 1;
	}
	char *opt = argv[1];
	if (STR_CMP(opt, "init") == 0) {
		init_project();
		return 0;
	} else if (STR_CMP(opt, "add") == 0) {
		add_library(argv[2]);
		return 0;
	} else if (STR_CMP(opt, "add-lib") == 0) {
		add_local_lib(argc - 2, argv + 2);
		return 0;
	} else if (STR_CMP(opt, "add-flag") == 0) {
		add_flag(argc - 2, argv + 2);
		return 0;
	} else if (STR_CMP(opt, "build") == 0) {
		build_project(global_str_arena);
		return 0;
	} else if (STR_CMP(opt, "run") == 0) {
		run_project(global_str_arena);
		return 0;
	} else if (STR_CMP(opt, "gen") == 0) {
		generate_compile_commands();
		return 0;
	} else if (STR_CMP(opt, "sync") == 0) {
		sync_dependency();
		return 0;
	} else {
		printf("Unknown command: %s\n", opt);
		print_version_details();
		return 1;
	}
}
