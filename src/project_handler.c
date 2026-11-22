#include <flint.h>

flint_res_bool_t check_available_tool(const char *cmd) {
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
		if (_access(cmd, 0) == 0)
			return FLINT_OK_VAL(flint_res_bool_t, true);
		char test_path[4096];
		snprintf(test_path, sizeof(test_path), "%s.exe", cmd);
		if (_access(test_path, 0) == 0)
			return FLINT_OK_VAL(flint_res_bool_t, true);
		else
			return FLINT_ERR_VAL(flint_res_bool_t, FLINT_ERR_TOOL_UNAVAILABLE);
#else
		if (access(cmd, X_OK) == 0)
			return FLINT_OK_VAL(flint_res_bool_t, true);
		else
			return FLINT_ERR_VAL(flint_res_bool_t, FLINT_ERR_TOOL_UNAVAILABLE);
#endif
	}

	if (!path || cmd_len == 0)
		return FLINT_ERR_VAL(flint_res_bool_t, FLINT_ERR_TOOL_UNAVAILABLE);

	while (*path) {
		separator = strchr(path,
#if defined(_WIN32)
						   ';'
#else
						   ':'
#endif
		);
		size_t dir_len = separator ? (size_t)(separator - path) : strlen(path);

		if (dir_len + cmd_len + 5 < sizeof(candidate)) {
			if (dir_len == 0)
				snprintf(candidate, sizeof(candidate), "./%s", cmd);
			else
				snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)dir_len,
						 path, cmd);

#if defined(_WIN32)
			if (_access(candidate, 0) == 0)
				return FLINT_OK_VAL(flint_res_bool_t, true);

			char test_path[4096];
			snprintf(test_path, sizeof(test_path), "%s.exe", candidate);
			if (_access(test_path, 0) == 0)
				return FLINT_OK_VAL(flint_res_bool_t, true);
#else
			if (access(candidate, X_OK) == 0)
				return FLINT_OK_VAL(flint_res_bool_t, true);
#endif
		}
		if (!separator)
			break;
		path = separator + 1;
	}
	return FLINT_ERR_VAL(flint_res_bool_t, FLINT_ERR_TOOL_UNAVAILABLE);
}

flint_res_string_t get_archiever(Arena *arena) {
	flint_res_bool_t llvm_ar = check_available_tool("llvm-ar");
	if (llvm_ar.err == FLINT_OK && llvm_ar.value == true) {
		return FLINT_OK_VAL(flint_res_string_t, string_from(arena, "llvm-ar"));
	}

	flint_res_bool_t ar = check_available_tool("ar");
	if (ar.err == FLINT_OK && ar.value == true) {
		return FLINT_OK_VAL(flint_res_string_t, string_from(arena, "ar"));
	}
#if defined(_WIN32)
	flint_res_bool_t llvm_lib = check_available_tool("llvm-lib");
	if (llvm_lib.err == FLINT_OK && llvm_lib.value == true) {
		return FLINT_OK_VAL(flint_res_string_t, string_from(arena, "llvm-lib"));
	}
	flint_res_bool_t lib = check_available_tool("lib");
	if (lib.err == FLINT_OK && lib.value == true) {
		return FLINT_OK_VAL(flint_res_string_t, string_from(arena, "lib"));
	}
#endif
	return FLINT_ERR_VAL(flint_res_string_t, FLINT_ERR_ARCHIVER_UNAVAILABLE);
}

flint_err_t init_project() {
	int dir_create_status = 0, file_create_status = 0;
	flint_err_t ret = FLINT_OK;
	if (file_exists("./composition.json")) {
		flint_warn("Flint Chert already initiated!");
		return FLINT_ERR_COMPOSITION_EXISTS;
	}

	flint_res_config_t conf_wrpper = config_init();
	if (conf_wrpper.err != FLINT_OK) {
		flint_error("Init project", conf_wrpper.err);
		return conf_wrpper.err;
	}
	Config *conf = conf_wrpper.value;

	flint_info("Bootstrapping New Project...");
	flint_prompt("Project Name: ");
	conf->project_name = string_trim(conf->arena, string_get(conf->arena));

	flint_prompt("Language (c/c++) [default: c]: ");
	conf->project_language = string_trim(conf->arena, string_get(conf->arena));
	if (string_len(conf->project_language) == 0) {
		conf->project_language = string_from(conf->arena, "c");
	}

	flint_prompt("Project type (exec/lib) [default: exec]: ");
	String *project_type = string_trim(conf->arena, string_get(conf->arena));

	flint_prompt("Compiler path [default: gcc]: ");
	conf->compiler_path = string_trim(conf->arena, string_get(conf->arena));
	if (string_len(conf->compiler_path) == 0) {
		conf->compiler_path = string_from(conf->arena, "gcc");
	}

	if (STR_CMP(string(conf->project_name), "") == 0) {
		flint_error("Init project", FLINT_ERR_MISSING_NAME);
		ret = FLINT_ERR_MISSING_NAME;
		goto CLEANUP;
	}

	flint_res_bool_t compiler =
		check_available_tool(string(conf->compiler_path));
	if (compiler.err != FLINT_OK) {
		flint_error("Init project", compiler.err);
		ret = compiler.err;
		goto CLEANUP;
	}
	flint_res_bool_t git = check_available_tool("git");
	if (git.err != FLINT_OK) {
		flint_error("Init project", git.err);
		ret = git.err;
		goto CLEANUP;
	}

	flint_res_string_t archiever = get_archiever(conf->arena);
	if (archiever.err != FLINT_OK) {
		flint_error("Init project", archiever.err);
		ret = archiever.err;
		goto CLEANUP;
	}

	if (!directory_exists("./deps")) {
		dir_create_status = MAKE_DIR("./deps");
		if (dir_create_status != 0) {
			ret = FLINT_ERR_MKDIR;
			goto CLEANUP;
		}
	}

	conf->executable = STR_CMP(string(project_type), "lib") != 0;
	const char *code_dir = conf->executable ? "./src" : "./lib";
	if (!directory_exists(code_dir)) {
		dir_create_status = MAKE_DIR(code_dir);
		if (dir_create_status != 0) {
			flint_error("Init project", FLINT_ERR_MKDIR);
			ret = FLINT_ERR_MKDIR;
			goto CLEANUP;
		}
	}

	const char *dirs[] = {"./include", "./static", "./shared"};
	for (size_t i = 0; i < 3; i++) {
		if (!directory_exists(dirs[i])) {
			dir_create_status = MAKE_DIR(dirs[i]);
			if (dir_create_status != 0) {
				flint_error("Init project", FLINT_ERR_MKDIR);
				ret = FLINT_ERR_MKDIR;
				goto CLEANUP;
			}
		}
	}

	bool is_cpp = (check_project_lang(string(conf->project_language)) == 0);
	const char *src_ext = is_cpp ? ".cpp" : ".c";
	const char *hdr_ext = is_cpp ? ".hpp" : ".h";
	const char *boilerplate =
		is_cpp ? "#include <iostream>\n\nint main() "
				 "{\n\tstd::cout << \"Hello, World!\" << std::endl;\n}"
			   : "#include <stdio.h>\n\nint main() "
				 "{\n\tprintf(\"Hello, World!\");\n}";

	char *file_name =
		conf->executable
			? (is_cpp ? "./src/main.cpp" : "./src/main.c")
			: string(string_concat_cstr(conf->arena, 3, "./lib/",
										string(conf->project_name), src_ext));

	if (!file_exists(file_name)) {
		if (create_append_file(file_name, (char *)boilerplate) == 1) {
			flint_error("Init project", FLINT_ERR_MKFILE);
			ret = FLINT_ERR_MKFILE;
			goto CLEANUP;
		}
	}

	char *header_file = string(string_concat_cstr(
		conf->arena, 3, "./include/", string(conf->project_name), hdr_ext));

	if (!file_exists(header_file)) {
		if (create_append_file(header_file, "") == 1) {
			flint_error("Init project", FLINT_ERR_MKFILE);
			ret = FLINT_ERR_MKFILE;
			goto CLEANUP;
		}
	}

	conf->version = string_from(conf->arena, "unknown");
	ret = write_flint_composition("composition.json", conf);
	if (ret != FLINT_OK) {
		flint_error("Init project", ret);
		goto CLEANUP;
	}
	ret = generate_compile_commands(conf);
	if (ret != FLINT_OK) {
		flint_error("Init project", ret);
		goto CLEANUP;
	}
	flint_success("Flint project '%s' successfully initiated",
				  string(conf->project_name));

CLEANUP:
	config_free(conf);
	return ret;
}

flint_res_string_t build_project(Arena *global_str_arena) {
	String *output = NULL;
	flint_err_t build_project_status = FLINT_OK;

	flint_res_config_t conf_wrpper = config_init();
	if (conf_wrpper.err != FLINT_OK) {
		flint_error("Build project", conf_wrpper.err);
		return FLINT_ERR_VAL(flint_res_string_t, conf_wrpper.err);
	}
	Config *conf = conf_wrpper.value;
	flint_err_t config_read_status =
		read_flint_composition("composition.json", conf);
	if (config_read_status != FLINT_OK) {
		flint_error("Build project", config_read_status);
		build_project_status = config_read_status;
		goto CLEANUP;
	}

	flint_res_string_t archiever = get_archiever(conf->arena);
	if (archiever.err != FLINT_OK) {
		flint_error("Build project", FLINT_ERR_ARCHIVER_UNAVAILABLE);
		build_project_status = FLINT_ERR_ARCHIVER_UNAVAILABLE;
		goto CLEANUP;
	}

	flint_info("Gathering Resources");
	Vector(const char *) header_arr = vector_init(conf->arena);
	Vector(const char *) src_arr = vector_init(conf->arena);
	Vector(const char *) static_lib_arr = vector_init(conf->arena);
	Vector(const char *) shared_lib_arr = vector_init(conf->arena);

	Vector(char *) header_vec = vector_init(conf->arena);
	Vector(char *) src_file_arr = vector_init(conf->arena);
	Vector(char *) stat_file_arr = vector_init(conf->arena);
	Vector(char *) shared_file_arr = vector_init(conf->arena);

	build_project_status = traverse_dir(
		conf->arena, string_from(conf->arena, "."), src_arr, header_arr,
		static_lib_arr, shared_lib_arr, conf->exclude_dirs);

	if (build_project_status != FLINT_OK) {
		flint_error("Build project", build_project_status);
		goto CLEANUP;
	}

	for (int i = 0; i < length(conf->exclude_exception); i++) {
		char *raw_entry = string(at(conf->exclude_exception, i));
		char *excluded = NULL;

		if (!is_platform_element(raw_entry, &excluded) || excluded[0] == '\0') {
			continue;
		}

		set_add(src_arr, excluded);
		set_add(header_arr, excluded);
	}

	int mkdir_err = 0, cmd_err = 0, create_append_err = 0, copy_err = 0;

	flint_info("Compilation started");

	mkdir_err = MAKE_DIR("build");
	if (mkdir_err && errno != EEXIST) {
		flint_error("Build project", FLINT_ERR_MKDIR);
		build_project_status = FLINT_ERR_MKDIR;
		goto CLEANUP;
	}

	mkdir_err = MAKE_DIR("./build/.cache");
	if (mkdir_err && errno != EEXIST) {
		flint_error("Build project", FLINT_ERR_MKDIR);
		build_project_status = FLINT_ERR_MKDIR;
		goto CLEANUP;
	}

	String *cwd = get_current_working_dir(conf->arena);

	Vector(String *) header_flags = vector_init(conf->arena);
	for (int i = 0; i < length(header_arr); i++) {
		append(header_flags, string_concat_cstr(conf->arena, 3, "\"-I./",
												at(header_arr, i), "\""));
	}
	String *headers = length(header_flags) > 0
						  ? string_join(conf->arena, header_flags, "\n")
						  : string_from(conf->arena, "");

	get_header_vec(conf, header_arr, header_vec, cwd);
	get_src_vec(conf, src_arr, src_file_arr, cwd);
	get_stat_lib_vec(conf, static_lib_arr, stat_file_arr, cwd);
	get_shared_lib_vec(conf, shared_lib_arr, shared_file_arr, cwd);

	String *stat_lib = NULL;
	char *ar_tool = string(archiever.value);

	if (length(stat_file_arr) == 0) {
		if (directory_exists("./static")) {
			String *static_libs = collect_files(
				conf->arena,
				string_concat_cstr(conf->arena, 2, string(cwd), "/static"),
				string_from(conf->arena, "static"));
			stat_lib = string_trim(conf->arena, static_libs);
			if (string_len(stat_lib) > 0) {
#if defined(_WIN32)
				char *win_stat_lib =
					string(string_clone(conf->arena, stat_lib));
				for (char *p = win_stat_lib; *p; p++) {
					if (*p == '/')
						*p = '\\';
				}
				cmd_err = system(string(string_concat_cstr(
					conf->arena, 5, "cd /d \".\\build\\.cache\" && for %f in (",
					win_stat_lib, ") do ", ar_tool, " x \"%f\"")));
#else
				cmd_err = system(string(string_concat_cstr(
					conf->arena, 5, "for f in ", string(stat_lib),
					"; do (cd ./build/.cache && ", ar_tool,
					" x \"$f\"); done")));
#endif
				if (cmd_err) {
					flint_error("Build project", FLINT_ERR_CMD_EXECUTION);
					build_project_status = FLINT_ERR_CMD_EXECUTION;
					goto CLEANUP;
				}
			}
		}
	} else {
		for (int i = 0; i < length(stat_file_arr); i++) {
#if defined(_WIN32)
			cmd_err = system(string(string_concat_cstr(
				conf->arena, 7, "cd /d \"./build/.cache\" && ", ar_tool,
				" x \"", string(cwd), "/", at(stat_file_arr, i), "\"")));
#else
			cmd_err = system(string(string_concat_cstr(
				conf->arena, 7, "(cd ./build/.cache && ", ar_tool, " x \"",
				string(cwd), "/", at(stat_file_arr, i), "\")")));
#endif
			if (cmd_err) {
				flint_error("Build project", FLINT_ERR_CMD_EXECUTION);
				build_project_status = FLINT_ERR_CMD_EXECUTION;
				goto CLEANUP;
			}
		}
	}

	String *shared_lib = NULL;
	if (length(shared_file_arr) == 0) {
		if (directory_exists("./shared")) {
			String *shared_libs = collect_files(
				conf->arena,
				string_concat_cstr(conf->arena, 2, string(cwd), "/shared"),
				string_from(conf->arena, "dyn"));
			shared_lib = string_trim(conf->arena, shared_libs);
		}
	} else {
		Vector(String *) quoted_shared = vector_init(conf->arena);
		for (int i = 0; i < length(shared_file_arr); i++) {
			append(quoted_shared,
				   string_concat_cstr(conf->arena, 3, "\"",
									  at(shared_file_arr, i), "\""));
		}
		shared_lib = string_join(conf->arena, quoted_shared, " ");
	}
	const char *shared_lib_str = shared_lib ? string(shared_lib) : "";

#if defined(_WIN32)
	const char *base_compile_flags = "-c\n-MMD\n-MP\n";
#else
	const char *base_compile_flags = "-c\n-fPIC\n-MMD\n-MP\n";
#endif

	String *response_content = string_concat_cstr(
		conf->arena, 4, base_compile_flags, string(headers), "\n",
		string(join_vec_to_str(conf->arena, conf->flags, "\n")));

	String *lib_links = join_vec_to_str(conf->arena, conf->lib_links, "\n");

	create_append_err = create_append_file("./build/.cache/compile.rsp",
										   string(response_content));
	if (create_append_err) {
		flint_error("Build project", FLINT_ERR_MKFILE);
		build_project_status = FLINT_ERR_MKFILE;
		goto CLEANUP;
	}

	create_append_err =
		create_append_file("./build/.cache/lib_links.rsp", string(lib_links));
	if (create_append_err) {
		flint_error("Build project", FLINT_ERR_MKFILE);
		build_project_status = FLINT_ERR_MKFILE;
		goto CLEANUP;
	}

	bool require_version_update = false;
	Vector(String *) obj_files_vec = vector_init(conf->arena);
	char *current_version = read_current_version_from_file(conf->arena);

	for (int i = 0; i < length(src_file_arr); i++) {
		char *full_path_src = at(src_file_arr, i);
		const char *base_name = get_filename_without_path(full_path_src);
		String *obj_file = string_concat_cstr(conf->arena, 3, "./build/.cache/",
											  base_name, ".o");
		String *d_file = string_concat_cstr(conf->arena, 3, "./build/.cache/",
											base_name, ".d");

		append(obj_files_vec, string_concat_cstr(conf->arena, 3, "\"",
												 string(obj_file), "\""));

		long long src_time = get_file_modified_time(full_path_src);
		long long obj_time = get_file_modified_time(string(obj_file));

		bool need_recompile = false;

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
				conf->arena, 8, string(conf->compiler_path),
				" \"-DPROJECT_VERSION=\\\"", string(conf->version),
				"\\\"\" @./build/.cache/compile.rsp \"", full_path_src,
				"\" -o \"", string(obj_file), "\""));
			cmd_err = system(compilation_command);
			if (cmd_err) {
				flint_error("Build project", FLINT_ERR_CMD_EXECUTION);
				build_project_status = FLINT_ERR_CMD_EXECUTION;
				goto CLEANUP;
			}

			flint_info("Compiled '%s'", full_path_src);
		}
	}

	String *obj_files_list = length(obj_files_vec) > 0
								 ? string_join(conf->arena, obj_files_vec, " ")
								 : string_from(conf->arena, "");

#if defined(_WIN32)
	const char *exe_ext = ".exe";
	const char *shared_prefix = "";
	const char *shared_ext = ".dll";
	const char *objs_arg = "./build/.cache/*.o";
#else
	const char *exe_ext = "";
	const char *shared_prefix = "lib";
#if defined(__APPLE__)
	const char *shared_ext = ".dylib";
#else
	const char *shared_ext = ".so";
#endif
	const char *objs_arg = "./build/.cache/*.o";
#endif

	bool has_stat_libs = (stat_lib != NULL && string_len(stat_lib) > 0) ||
						 length(stat_file_arr) > 0;
	const char *link_objs = has_stat_libs ? objs_arg : string(obj_files_list);

	if (conf->executable) {
		String *target_path = string_concat_cstr(
			conf->arena, 3, "./build/", string(conf->project_name), exe_ext);

		cmd_err = system(string(string_concat_cstr(
			conf->arena, 8, string(conf->compiler_path), " ", link_objs, " ",
			shared_lib_str, " -o \"", string(target_path),
			"\" @./build/.cache/lib_links.rsp")));

		if (cmd_err) {
			flint_error("Build project", FLINT_ERR_CMD_EXECUTION);
			build_project_status = FLINT_ERR_CMD_EXECUTION;
			goto CLEANUP;
		}
		output = string_clone(global_str_arena, target_path);
		flint_success("Executable generated");
	} else {
		const char *lib_dirs[] = {
			"./build/static",		  "./build/shared",
			"./build/static/lib",	  "./build/shared/lib",
			"./build/static/include", "./build/shared/include"};
		for (int i = 0; i < 6; i++) {
			mkdir_err = MAKE_DIR(lib_dirs[i]);
			if (mkdir_err && errno != EEXIST) {
				flint_error("Build project", FLINT_ERR_MKDIR);
				build_project_status = FLINT_ERR_MKDIR;
				goto CLEANUP;
			}
		}

		cmd_err = system(string(string_concat_cstr(
			conf->arena, 8, string(conf->compiler_path), " -shared ", link_objs,
			" -o \"./build/shared/lib/", shared_prefix,
			string(conf->project_name), shared_ext,
			"\" @./build/.cache/lib_links.rsp")));

		if (cmd_err) {
			flint_error("Build project", FLINT_ERR_CMD_EXECUTION);
			build_project_status = FLINT_ERR_CMD_EXECUTION;
			goto CLEANUP;
		}

#if defined(_WIN32)
		if (STR_CMP(ar_tool, "lib") == 0 || STR_CMP(ar_tool, "llvm-lib") == 0) {
			cmd_err = system(string(string_concat_cstr(
				conf->arena, 5, ar_tool, " /OUT:\"./build/static/lib/",
				string(conf->project_name), ".lib\" ", link_objs)));
		} else {
			cmd_err = system(string(string_concat_cstr(
				conf->arena, 5, ar_tool, " rcs \"./build/static/lib/lib",
				string(conf->project_name), ".a\" ", link_objs)));
		}
#else
		cmd_err = system(string(string_concat_cstr(
			conf->arena, 5, ar_tool, " rcs \"./build/static/lib/lib",
			string(conf->project_name), ".a\" ", link_objs)));
#endif
		if (cmd_err) {
			flint_error("Build project", FLINT_ERR_CMD_EXECUTION);
			build_project_status = FLINT_ERR_CMD_EXECUTION;
			goto CLEANUP;
		}

		for (int i = 0; i < length(header_vec); i++) {
			char *src_path = at(header_vec, i);
			const char *file_name = get_filename_without_path(src_path);
			char *dest_path_1 = string(string_concat_cstr(
				conf->arena, 2, "./build/static/include/", file_name));
			char *dest_path_2 = string(string_concat_cstr(
				conf->arena, 2, "./build/shared/include/", file_name));

			copy_err = copy_file(src_path, dest_path_1);
			if (!copy_err) {
				copy_err = copy_file(src_path, dest_path_2);
			}
			if (copy_err) {
				flint_error("Build project", FLINT_ERR_COPY_HEADERS);
				build_project_status = FLINT_ERR_COPY_HEADERS;
				goto CLEANUP;
			}
		}

		output = string_concat_cstr(global_str_arena, 2, "./build/",
									string(conf->project_name));
		flint_success("Libraries generated");
	}
	if (require_version_update) {
		update_version_file(string(conf->version));
	}

CLEANUP:
	config_free(conf);
	return output ? FLINT_OK_VAL(flint_res_string_t, output)
				  : FLINT_ERR_VAL(flint_res_string_t, build_project_status);
}

flint_err_t run_project(Arena *global_str_arena) {
	flint_res_string_t target = build_project(global_str_arena);
	if (target.err != FLINT_OK) {
		return target.err;
	}
	if (!target.value || string_len(target.value) == 0) {
		flint_error("Run project", FLINT_ERR_UNKNOWN);
		return FLINT_ERR_UNKNOWN;
	}

	char *path_str = string(target.value);
	char command[4096];

#if defined(_WIN32)
	char abs_path[MAX_PATH];
	if (_fullpath(abs_path, path_str, sizeof(abs_path)) != NULL) {
		for (int i = 0; abs_path[i] != '\0'; i++) {
			if (abs_path[i] == '/')
				abs_path[i] = '\\';
		}
		snprintf(command, sizeof(command), "\"%s\"", abs_path);
	} else {
		snprintf(command, sizeof(command), "\"%s\"", path_str);
	}
#else
	snprintf(command, sizeof(command), "\"%s\"", path_str);
#endif

	int cmd_err = system(command);
	if (cmd_err) {
		return FLINT_ERR_CMD_EXECUTION;
	}
	return FLINT_OK;
}
