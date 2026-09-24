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
};

void print_version_details() { printf("\nVersion: %s\n", STR(VERSION)); }

void print_help_message() {
	/*
	const char *art =
		"                                           \n"
		"                                       *:                 "
		"                      \n"
		"                                     :%%%=                "
		"                      \n"
		"                                    =%%%%%*.              "
		"                      \n"
		"                                   *%%%%%%%%*             "
		"                      \n"
		"                                  %%%%%%%%%%%%            "
		"                      \n"
		"                                -%%%%%%%%%%%%%%-          "
		"                      \n"
		"                               :%%%%%%%%%%%%%%%%*         "
		"                      \n"
		"                              #%%%%%%%%%%%%%%%%%%%=       "
		"                      \n"
		"                             =%%%%%%%%%%%%%%%%%%%%%.      "
		"                      \n"
		"                              #%%%%%%%%#.*%%%%%%%#.       "
		"                      \n"
		"                               -%%%%%:  +%%%%%%%=         "
		"                      \n"
		"                                -%=    -%%%%%%%           "
		"                      \n"
		"                                      .%%%%%%-            "
		"                      \n"
		"                                     =%%%%%%.             "
		"                      \n"
		"                                    .%%%%%*               "
		"                      \n"
		"                                    .#%%%.                "
		"                      \n"
		"                                      #+                  "
		"                      \n"
		"                                                          "
		"                       \n"
		"                                                          "
		"                       \n"
		"                                                          "
		"                       \n"
		"        -%%***+        -%.            %%:        :%%:  =% "
		"       :**#%%**-      \n"
		"        -%%            -%.            %%-        :%:%= =% "
		"          =%*         \n"
		"        -%%:::.        -%.            %%:        :% .#%%% "
		"          =%*         \n"
		"        -%%            -%#***=        %%-        :%   =%% "
		"          =%*\n";
	printf("%s\n", art);
*/
	printf("\nUsage: flint <command> [args]\n\n");
	printf("Commands:\n");

	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		printf("  %-10s %s\n", commands[i].name, commands[i].description);
	}
}

int cli(int argc, char *argv[], Arena *global_str_arena) {
	if (argc < 2) {
		print_help_message();
		return 1;
	}
	char *opt = argv[1];
	if (STR_CMP(opt, "help") == 0) {
		print_help_message();
		return 0;
	} else if (STR_CMP(opt, "init") == 0) {
		return init_project();
	} else if (STR_CMP(opt, "add") == 0) {
		add_library(argv[2]);
		return 0;
	} else if (STR_CMP(opt, "remove") == 0) {
		remove_library(argv[2]);
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
	} else if (STR_CMP(opt, "deps") == 0) {
		list_deps();
		return 0;
	} else if (STR_CMP(opt, "version") == 0) {
		print_version_details();
		return 0;
	} else {
		printf("Unknown command: %s\n", opt);
		print_help_message();
		return 1;
	}
}
