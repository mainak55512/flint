#include <flint.h>

int check_available_tool(const char *cmd) {
	const char *path = getenv("PATH");
	const char *separator;
	char candidate[4096];
	size_t cmd_len = strlen(cmd);

	if (strchr(cmd, '/')
#if defined(_WIN32)
		|| strchr(cmd, '\\') || (cmd_len > 1 && cmd[1] == ':')
#endif
	) {
#if defined(_WIN32)
		return _access(cmd, 0) == 0;
#else
		return access(cmd, X_OK) == 0;
#endif
	}
	if (!path || cmd_len == 0)
		return 0;
	while (*path) {
		separator = strchr(path,
#if defined(_WIN32)
						   ';'
#else
						   ':'
#endif
		);
		size_t dir_len = separator ? (size_t)(separator - path) : strlen(path);
		if (dir_len + cmd_len + 2 < sizeof(candidate)) {
			if (dir_len == 0)
				snprintf(candidate, sizeof(candidate), "./%s", cmd);
			else
				snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)dir_len,
						 path, cmd);
#if defined(_WIN32)
			if (_access(candidate, 0) == 0)
#else
			if (access(candidate, X_OK) == 0)
#endif
				return 1;
		}
		if (!separator)
			break;
		path = separator + 1;
	}
	return 0;
}

String *get_archiever(Arena *arena) {
	String *llvm_ar = string_from(arena, "llvm-ar");
	String *ar = string_from(arena, "ar");

	if (check_available_tool("llvm-ar")) {
		return llvm_ar;
	} else if (check_available_tool("ar")) {
		return ar;
	}
	return string_from(arena, "");
}

int init_project() {
	struct STAT info;
	int ret = 0, dir_create_status = 0, file_create_status = 0;
	bool isExec;
	if (file_exists("./composition.json")) {
		printf("[!] Flint Chert already initiated!");
		return ret;
	}
	Config *conf = config_init();

	isExec = true;

	printf("[+] Bootstrapping New Project...\n");
	printf("[?] Project Name: ");
	conf->project_name = string_trim(conf->arena, string_get(conf->arena));

	String *project_dir = string_from(conf->arena, ".");

	printf("[?] Language (c/c++) [default: c]: ");
	conf->project_language = string_trim(conf->arena, string_get(conf->arena));
	if (STR_CMP(string(conf->project_language), "") == 0) {
		conf->project_language = string_from(conf->arena, "c");
	}

	printf("[?] Project type (exec/lib) [default: exec]: ");
	String *project_type = string_trim(conf->arena, string_get(conf->arena));

	printf("[?] Compiler path [default: gcc]: ");
	conf->compiler_path = string_trim(conf->arena, string_get(conf->arena));

	if (STR_CMP(string(conf->compiler_path), "") == 0) {
		conf->compiler_path = string_from(conf->arena, "gcc");
	}
	if (!check_available_tool(string(conf->compiler_path))) {
		fprintf(stderr, "[!] Compiler '%s' was not found in PATH.\n",
				string(conf->compiler_path));
		ret = 1;
		goto CLEANUP;
	}
	if (!check_available_tool("git")) {
		fprintf(stderr, "[!] Required tool 'git' was not found in PATH.\n");
		ret = 1;
		goto CLEANUP;
	}
	if (!check_available_tool("ar") && !check_available_tool("llvm-ar")) {
		fprintf(stderr,
				"[!] Required tool 'ar' or 'llvm-ar' was not found in PATH.\n");
		ret = 1;
		goto CLEANUP;
	}

	String *dep_dir =
		string_concat_cstr(conf->arena, 2, string(project_dir), "/deps");

	if (!directory_exists(string(dep_dir))) {
		dir_create_status = MAKE_DIR(string(dep_dir));
		if (dir_create_status != 0) {
			ret = 1;
			goto CLEANUP;
		}
	}

	isExec = STR_CMP(string(project_type), "lib") == 0 ? false : true;
	if (isExec) {
		String *src_dir =
			string_concat_cstr(conf->arena, 2, string(project_dir), "/src");
		if (!directory_exists(string(src_dir))) {
			dir_create_status = MAKE_DIR(string(src_dir));
		}
	} else {
		String *lib_dir =
			string_concat_cstr(conf->arena, 2, string(project_dir), "/lib");
		if (!directory_exists(string(lib_dir))) {
			dir_create_status = MAKE_DIR(string(lib_dir));
		}
	}

	if (dir_create_status != 0) {
		ret = 1;
		goto CLEANUP;
	}

	char *init_incl_dir = string(
		string_concat_cstr(conf->arena, 2, string(project_dir), "/include"));
	char *init_static_dir = string(
		string_concat_cstr(conf->arena, 2, string(project_dir), "/static"));
	char *init_shared_dir = string(
		string_concat_cstr(conf->arena, 2, string(project_dir), "/shared"));

	if (!directory_exists(init_incl_dir)) {
		dir_create_status = MAKE_DIR(init_incl_dir);
	}
	if (!directory_exists(init_static_dir)) {
		dir_create_status = MAKE_DIR(init_static_dir);
	}
	if (!directory_exists(init_shared_dir)) {
		dir_create_status = MAKE_DIR(init_shared_dir);
	}

	if (dir_create_status != 0) {
		ret = 1;
		goto CLEANUP;
	}

	switch (check_project_lang(string(conf->project_language))) {
	case 0: {
		char *file_name;
		file_name = isExec
						? "./src/main.cpp"
						: string(string_concat_cstr(conf->arena, 3, "./lib/",
													string(conf->project_name),
													".cpp"));
		if (!file_exists(file_name)) {
			file_create_status = create_append_file(
				file_name,
				"#include <iostream>\n\nint main() "
				"{\n\tstd::cout << \"Hello, World!\" << std::endl;\n}");
		}

		if (file_create_status == 1) {
			ret = 1;
			goto CLEANUP;
		}
		char *header_file = string(string_concat_cstr(
			conf->arena, 3, "./include/", string(conf->project_name), ".hpp"));

		if (!file_exists(header_file)) {
			file_create_status = create_append_file(header_file, "");
		}

		if (file_create_status == 1) {
			ret = 1;
			goto CLEANUP;
		}
		break;
	}
	default: {
		char *file_name =
			isExec
				? "./src/main.c"
				: string(string_concat_cstr(conf->arena, 3, "./lib/",
											string(conf->project_name), ".c"));
		if (!file_exists(file_name)) {
			file_create_status = create_append_file(
				file_name, "#include <stdio.h>\n\nint main() "
						   "{\n\tprintf(\"Hello, World!\");\n}");
		}

		if (file_create_status == 1) {
			ret = 1;
			goto CLEANUP;
		}

		char *header_file = string(string_concat_cstr(
			conf->arena, 3, "./include/", string(conf->project_name), ".h"));

		if (!file_exists(header_file)) {
			file_create_status = create_append_file(header_file, "");
		}

		if (file_create_status == 1) {
			ret = 1;
			goto CLEANUP;
		}
		break;
	}
	}
	conf->version = string_from(conf->arena, "unknown");
	write_flint_composition("composition.json", conf);
	printf("[✓] Flint project '%s' successfully initiated\n",
		   string(conf->project_name));
	generate_compile_commands(conf);

CLEANUP:
	config_free(conf);
	return ret;
}

String *build_project(Arena *global_str_arena) {
	String *command;
	// Arena *str_arena = arena_init(1024);

	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	printf("[+] Gathering Resources\n");
	Vector *header_arr = vector_init(const char *);
	Vector *src_arr = vector_init(const char *);
	Vector *static_lib_arr = vector_init(const char *);
	Vector *shared_lib_arr = vector_init(const char *);

	traverse_dir(conf->arena, string_from(conf->arena, "."), src_arr,
				 header_arr, static_lib_arr, shared_lib_arr,
				 conf->exclude_dirs);

	for (int i = 0; i < length(conf->exclude_exception); i++) {
		set_add(src_arr, string(at(String *, conf->exclude_exception, i)));
		set_add(header_arr, string(at(String *, conf->exclude_exception, i)));
	}

	printf("[✓] done!\n");
	int mkdir_err = 0, cmd_err = 0, create_append_err = 0, copy_err = 0;

	printf("[+] Compilation started\n");

	mkdir_err = MAKE_DIR("build");

	if (mkdir_err) {
		if (errno != EEXIST) {
			fprintf(stderr, "Unable to create `build` directory\n");
			goto CLEANUP;
		}
	}

	mkdir_err = MAKE_DIR("./build/.cache");

	if (mkdir_err) {
		if (errno != EEXIST) {
			fprintf(stderr, "Unable to create `.cache` directory\n");
			goto CLEANUP;
		}
	}

	String *cwd = get_current_working_dir(conf->arena);

	String *header_list = string_from(conf->arena, "");
	String *src_list = string_from(conf->arena, "");

	for (int i = 0; i < length(header_arr); i++) {
		header_list =
			string_concat_cstr(conf->arena, 4, string(header_list), "\n\"-I./",
							   at(char *, header_arr, i), "\"");
	}

	Vector *header_vec = vector_init(char *);
	Vector *src_file_arr = vector_init(char *);

	Vector *stat_file_arr = vector_init(char *);
	Vector *shared_file_arr = vector_init(char *);

	get_header_vec(conf, header_arr, header_vec, cwd);
	get_src_vec(conf, src_arr, src_file_arr, cwd);

	get_stat_lib_vec(conf, static_lib_arr, stat_file_arr, cwd);
	get_shared_lib_vec(conf, shared_lib_arr, shared_file_arr, cwd);

	String *stat_lib = string_from(conf->arena, "");
	String *shared_lib = string_from(conf->arena, "");

	if (length(stat_file_arr) == 0) {
		if (directory_exists("./static")) {
			String *static_libs = collect_files(
				conf->arena,
				string_concat_cstr(conf->arena, 2, string(cwd), "/static"),
				string_from(conf->arena, "static"));
			stat_lib = string_trim(conf->arena, static_libs);
			if (string_len(stat_lib) > 0) {
				cmd_err = system(string(string_concat_cstr(
					conf->arena, 3, "for f in ", string(stat_lib),
					"; do (cd ./build/.cache && ar x \"$f\"); done")));
				if (cmd_err) {
					fprintf(stderr,
							"Error encountered while adding static libs\n");
					vector_free(&header_vec);
					vector_free(&src_file_arr);
					vector_free(&stat_file_arr);
					vector_free(&shared_file_arr);
					vector_free(&static_lib_arr);
					vector_free(&shared_lib_arr);
					goto CLEANUP;
				}
			}
		}
	} else {
		for (int i = 0; i < length(stat_file_arr); i++) {
			cmd_err = system(string(string_concat_cstr(
				conf->arena, 5, "(cd ./build/.cache && ar x \"", string(cwd),
				"/", at(char *, stat_file_arr, i), "\")")));
			if (cmd_err) {
				fprintf(stderr, "Error encountered while adding static libs\n");
				vector_free(&header_vec);
				vector_free(&src_file_arr);
				vector_free(&stat_file_arr);
				vector_free(&shared_file_arr);
				vector_free(&static_lib_arr);
				vector_free(&shared_lib_arr);
				goto CLEANUP;
			}
		}
	}

	if (length(shared_file_arr) == 0) {
		if (directory_exists("./shared")) {
			String *shared_libs = collect_files(
				conf->arena,
				string_concat_cstr(conf->arena, 2, string(cwd), "/shared"),
				string_from(conf->arena, "dyn"));
			shared_lib = string_trim(conf->arena, shared_libs);
		}
	} else {
		for (int i = 0; i < length(shared_file_arr); i++) {
			if (string_len(shared_lib) == 0) {
				shared_lib =
					string_from(conf->arena, at(char *, shared_file_arr, i));
			} else {
				shared_lib =
					string_concat_cstr(conf->arena, 3, string(shared_lib), " ",
									   at(char *, shared_file_arr, i));
			}
		}
	}

	String *headers, *src;
	headers = string_trim(conf->arena, header_list);

	String *response_content = string_concat_cstr(
		conf->arena, 4, "-c\n-fPIC\n-MMD\n-MP\n", string(headers), "\n",
		string(join_vec_to_str(conf->arena, conf->flags,
							   string_from(conf->arena, "\n"))));

	String *lib_links = join_vec_to_str(conf->arena, conf->lib_links,
										string_from(conf->arena, "\n"));

	create_append_err = create_append_file("./build/.cache/compile.rsp",
										   string(response_content));
	create_append_err =
		create_append_file("./build/.cache/lib_links.rsp", string(lib_links));

	if (create_append_err) {
		fprintf(stderr, "Error encountered while generating `compile.rsp`\n");
		vector_free(&header_vec);
		vector_free(&src_file_arr);
		vector_free(&stat_file_arr);
		vector_free(&shared_file_arr);
		vector_free(&static_lib_arr);
		vector_free(&shared_lib_arr);
		goto CLEANUP;
	}

	bool require_version_update = false;

	for (int i = 0; i < length(src_file_arr); i++) {
		const char *base_name =
			get_filename_without_path(at(char *, src_file_arr, i));
		char *full_path_src =
			arena_strdup(conf->arena, at(char *, src_file_arr, i));
		String *obj_file = string_concat_cstr(conf->arena, 3, "./build/.cache/",
											  base_name, ".o");

		String *d_file = string_concat_cstr(conf->arena, 3, "./build/.cache/",
											base_name, ".d");

		long long src_time =
			get_file_modified_time(at(char *, src_file_arr, i));
		long long obj_time = get_file_modified_time(string(obj_file));

		bool need_recompile = false;

		char *current_version = read_current_version_from_file(conf->arena);

		if (obj_time == 0 || src_time > obj_time) {
			need_recompile = true;
		} else if (are_headers_newer(string(d_file), obj_time)) {
			need_recompile = true;
		} else if (STR_CMP(string(conf->version), current_version) != 0) {
			need_recompile = true;
			require_version_update = true;
		}

		if (need_recompile) {
			char *compilation_command = string(string_concat_cstr(
				conf->arena, 7, string(conf->compiler_path),
				" -DPROJECT_VERSION=\\\"", string(conf->version),
				"\\\" @./build/.cache/compile.rsp ",
				at(char *, src_file_arr, i), " -o ", string(obj_file)));
			cmd_err = system(compilation_command);
			if (cmd_err) {
				fprintf(stderr, "Error encountered at compilation\n");
				vector_free(&header_vec);
				vector_free(&src_file_arr);
				vector_free(&stat_file_arr);
				vector_free(&shared_file_arr);
				vector_free(&static_lib_arr);
				vector_free(&shared_lib_arr);
				goto CLEANUP;
			}

			printf("[✓] Compiled '%s'\n", full_path_src);
		}
	}

	vector_free(&src_file_arr);
	vector_free(&stat_file_arr);
	vector_free(&shared_file_arr);
	vector_free(&static_lib_arr);
	vector_free(&shared_lib_arr);

	String *output = string_concat_cstr(global_str_arena, 2, "./build/",
										string(conf->project_name));

	if (conf->executable) {
		cmd_err = system(string(string_concat_cstr(
			conf->arena, 6, string(conf->compiler_path), " ./build/.cache/*.o ",
			string(shared_lib), " -o ", string(output),
			" @./build/.cache/lib_links.rsp")));

		if (cmd_err) {
			fprintf(stderr, "Error encountered while generating executable\n");
			vector_free(&header_vec);
			goto CLEANUP;
		}
		vector_free(&header_vec);
		printf("[✓] Executable ganerated\n");
	} else {
		String *archiever = get_archiever(conf->arena);
		if (STR_CMP(string(archiever), "") == 0) {
			printf("[x] No archiever found!\n");
			vector_free(&header_vec);
			goto CLEANUP;
		}

		mkdir_err = MAKE_DIR("./build/static");
		mkdir_err = MAKE_DIR("./build/shared");
		mkdir_err = MAKE_DIR("./build/static/lib");
		mkdir_err = MAKE_DIR("./build/shared/lib");
		mkdir_err = MAKE_DIR("./build/static/include");
		mkdir_err = MAKE_DIR("./build/shared/include");
		if (mkdir_err) {
			if (errno != EEXIST) {
				fprintf(stderr, "Error encountered while generating "
								"library directories\n");
				vector_free(&header_vec);
				goto CLEANUP;
			}
		}

		cmd_err = system(string(string_concat_cstr(
			conf->arena, 5, string(conf->compiler_path),
			" -shared ./build/.cache/*.o -o ./build/shared/lib/lib",
			string(conf->project_name), ".so",
			" @./build/.cache/lib_links.rsp")));

		if (cmd_err) {
			fprintf(stderr,
					"Error encountered while generating shared library\n");
			vector_free(&header_vec);
			goto CLEANUP;
		}
		cmd_err = system(string(string_concat_cstr(
			conf->arena, 4, string(archiever), " rcs ./build/static/lib/lib",
			string(conf->project_name), ".a ./build/.cache/*.o")));
		if (cmd_err) {
			fprintf(stderr,
					"Error encountered while generating static library\n");
			vector_free(&header_vec);
			goto CLEANUP;
		}

		for (int i = 0; i < length(header_vec); i++) {
			char *src_path = at(char *, header_vec, i);
			const char *file_name = get_filename_without_path(src_path);
			char *dest_path_1 = string(string_concat_cstr(
				conf->arena, 2, "./build/static/include/", file_name));
			char *dest_path_2 = string(string_concat_cstr(
				conf->arena, 2, "./build/shared/include/", file_name));

			copy_err = copy_file(src_path, dest_path_1);
			copy_err = copy_file(src_path, dest_path_2);
			if (copy_err) {
				fprintf(stderr, "Error encountered while copying headers\n");
				vector_free(&header_vec);
				goto CLEANUP;
			}
		}

		printf("[✓] Libraries ganerated\n");
		vector_free(&header_vec);
	}
	if (require_version_update) {
		update_version_file(string(conf->version));
	}

CLEANUP:
	vector_free(&src_arr);
	vector_free(&header_arr);
	config_free(conf);
	return output;
}

void run_project(Arena *global_str_arena) {
	char *command = string(build_project(global_str_arena));
	system(command);
}
