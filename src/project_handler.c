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
	Arena *str_arena;
	String *dep_dir, *project_name, *project_dir, *project_lang, *dep_incl,
		*dep_lib, *compiler_path, *project_type;
	int ret, dir_create_status = 0, file_create_status = 0;
	bool isExec;

	isExec = true;
	ret = 0;

	if (file_exists("./composition.json")) {
		printf("[!] Flint Chert already initiated!");
		return ret;
	}

	str_arena = arena_init(1024);
	printf("[+] Bootstrapping New Project...\n");
	printf("[?] Project Name: ");
	project_name = string_trim(str_arena, string_get(str_arena));

	project_dir = string_from(str_arena, ".");

	printf("[?] Language (c/c++) [default: c]: ");
	project_lang = string_trim(str_arena, string_get(str_arena));

	printf("[?] Project type (exec/lib) [default: exec]: ");
	project_type = string_trim(str_arena, string_get(str_arena));

	printf("[?] Compiler path [default: gcc]: ");
	compiler_path = string_trim(str_arena, string_get(str_arena));

	if (STR_CMP(string(compiler_path), "") == 0) {
		compiler_path = string_from(str_arena, "gcc");
	}
	if (!check_available_tool(string(compiler_path))) {
		fprintf(stderr, "[!] Compiler '%s' was not found in PATH.\n",
				string(compiler_path));
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

	dep_dir = string_concat_cstr(str_arena, 2, string(project_dir), "/deps");

	if (!directory_exists(string(dep_dir))) {
		dir_create_status = MAKE_DIR(string(dep_dir));
		if (dir_create_status != 0) {
			ret = 1;
			goto CLEANUP;
		}
	}

	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);
	yyjson_mut_val *initial_package = yyjson_mut_arr(doc);
	yyjson_mut_obj_add_val(doc, root, "packages", initial_package);

	yyjson_write_err werr;
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
	if (!yyjson_mut_write_file("./deps/.package", doc, flg, NULL, &werr)) {
		fprintf(stderr, "Write error: %s\n", werr.msg);
	}

	yyjson_mut_doc_free(doc);

	isExec = STR_CMP(string(project_type), "lib") == 0 ? false : true;
	if (isExec) {
		String *src_dir =
			string_concat_cstr(str_arena, 2, string(project_dir), "/src");
		if (!directory_exists(string(src_dir))) {
			dir_create_status = MAKE_DIR(string(src_dir));
		}
	} else {
		String *lib_dir =
			string_concat_cstr(str_arena, 2, string(project_dir), "/lib");
		if (!directory_exists(string(lib_dir))) {
			dir_create_status = MAKE_DIR(string(lib_dir));
		}
	}

	if (dir_create_status != 0) {
		ret = 1;
		goto CLEANUP;
	}

	char *init_incl_dir = string(
		string_concat_cstr(str_arena, 2, string(project_dir), "/include"));
	char *init_static_dir = string(
		string_concat_cstr(str_arena, 2, string(project_dir), "/static"));
	char *init_shared_dir = string(
		string_concat_cstr(str_arena, 2, string(project_dir), "/shared"));

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

	switch (check_project_lang(string(project_lang))) {
	case 0: {
		char *file_name;
		file_name =
			isExec ? "./src/main.cpp"
				   : string(string_concat_cstr(str_arena, 3, "./lib/",
											   string(project_name), ".cpp"));
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
			str_arena, 3, "./include/", string(project_name), ".hpp"));

		if (!file_exists(header_file)) {
			file_create_status = create_append_file(header_file, "");
		}

		if (file_create_status == 1) {
			ret = 1;
			goto CLEANUP;
		}
		create_my_build_config("./composition.json", string(project_name),
							   "cpp", string(compiler_path), isExec);
		break;
	}
	default: {
		char *file_name =
			isExec ? "./src/main.c"
				   : string(string_concat_cstr(str_arena, 3, "./lib/",
											   string(project_name), ".c"));
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
			str_arena, 3, "./include/", string(project_name), ".h"));

		if (!file_exists(header_file)) {
			file_create_status = create_append_file(header_file, "");
		}

		if (file_create_status == 1) {
			ret = 1;
			goto CLEANUP;
		}
		create_my_build_config("./composition.json", string(project_name), "c",
							   string(compiler_path), isExec);
		break;
	}
	}

	printf("[✓] Flint project '%s' successfully initiated\n",
		   string(project_name));
	generate_compile_commands();

CLEANUP:
	arena_free(&str_arena);
	return ret;
}

String *build_project(Arena *global_str_arena) {
	printf("[+] Compilation started\n");
	String *command;
	Arena *str_arena = arena_init(1024);

	int mkdir_err = 0, cmd_err = 0, create_append_err = 0, copy_err = 0;

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

	String *cwd = get_current_working_dir(str_arena);
	yyjson_read_err err;
	yyjson_doc *doc = yyjson_read_file("./composition.json", 0, NULL, &err);

	if (!doc) {
		fprintf(stderr, "Read error: %s\n", err.msg);
		goto CLEANUP;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);

	String *project_name = string_from(
		str_arena,
		(char *)yyjson_get_str(yyjson_obj_get(root, "project_name")));

	yyjson_val *header_arr = yyjson_obj_get(root, "include_paths");
	yyjson_val *src_arr = yyjson_obj_get(root, "src");
	yyjson_val *dep_arr = yyjson_obj_get(root, "dependencies");
	yyjson_val *compiler_path = yyjson_obj_get(root, "compiler_path");
	yyjson_val *executable = yyjson_obj_get(root, "executable");
	yyjson_val *version = yyjson_obj_get(root, "version");

	char *version_str = arena_strdup(str_arena, yyjson_get_str(version));

	bool isExec = yyjson_get_bool(executable);

	String *header_list = string_from(str_arena, "");
	String *src_list = string_from(str_arena, "");
	String *compiler = string_concat_cstr(
		str_arena, 2, (char *)yyjson_get_str(compiler_path), " ");

	size_t idx = 0, max = 0;
	yyjson_val *val, *key;

	yyjson_arr_foreach(header_arr, idx, max, val) {
		header_list =
			string_concat_cstr(str_arena, 4, string(header_list), "\n\"-I./",
							   (char *)yyjson_get_str(val), "\"");
	}

	idx = 0, max = 0;

	Vector *header_vec = vector_init(char *);
	Vector *src_file_arr = vector_init(char *);
	Vector *stat_file_arr = vector_init(char *);
	Vector *shared_file_arr = vector_init(char *);
	get_header_vec(str_arena, header_vec, root, dep_arr, cwd);
	get_src_vec(str_arena, src_file_arr, root, dep_arr,
				get_current_working_dir(str_arena));
	get_stat_lib_vec(str_arena, stat_file_arr, root, dep_arr,
					 get_current_working_dir(str_arena));
	get_shared_lib_vec(str_arena, shared_file_arr, root, dep_arr,
					   get_current_working_dir(str_arena));

	String *stat_lib = string_from(str_arena, "");
	String *shared_lib = string_from(str_arena, "");

	if (length(stat_file_arr) == 0) {
		if (directory_exists("./static")) {
			String *static_libs = collect_files(
				str_arena,
				string_concat_cstr(str_arena, 2, string(cwd), "/static"),
				string_from(str_arena, "static"));
			stat_lib = string_trim(str_arena, static_libs);
			if (string_len(stat_lib) > 0) {
				cmd_err = system(string(string_concat_cstr(
					str_arena, 3, "for f in ", string(stat_lib),
					"; do (cd ./build/.cache && ar x \"$f\"); done")));
				if (cmd_err) {
					fprintf(stderr,
							"Error encountered while adding static libs\n");
					vector_free(header_vec);
					vector_free(src_file_arr);
					vector_free(stat_file_arr);
					vector_free(shared_file_arr);
					goto CLEANUP;
				}
			}
		}
	} else {
		for (int i = 0; i < length(stat_file_arr); i++) {
			cmd_err = system(string(string_concat_cstr(
				str_arena, 5, "(cd ./build/.cache && ar x \"", string(cwd), "/",
				at(char *, stat_file_arr, i), "\")")));
			if (cmd_err) {
				fprintf(stderr, "Error encountered while adding static libs\n");
				vector_free(header_vec);
				vector_free(src_file_arr);
				vector_free(stat_file_arr);
				vector_free(shared_file_arr);
				goto CLEANUP;
			}
		}
	}

	if (length(shared_file_arr) == 0) {
		if (directory_exists("./shared")) {
			String *shared_libs = collect_files(
				str_arena,
				string_concat_cstr(str_arena, 2, string(cwd), "/shared"),
				string_from(str_arena, "dyn"));
			shared_lib = string_trim(str_arena, shared_libs);
		}
	} else {
		for (int i = 0; i < length(shared_file_arr); i++) {
			if (string_len(shared_lib) == 0) {
				shared_lib =
					string_from(str_arena, at(char *, shared_file_arr, i));
			} else {
				shared_lib =
					string_concat_cstr(str_arena, 3, string(shared_lib), " ",
									   at(char *, shared_file_arr, i));
			}
		}
	}

	idx = 0, max = 0;

	if (dep_arr) {
		yyjson_obj_foreach(dep_arr, idx, max, key, val) {
			yyjson_val *dep_src_arr, *dep_include_arr, *elem;
			int index, max_val;
			char *key_str = (char *)yyjson_get_str(key);

			dep_include_arr = yyjson_obj_get(val, "include_paths");

			index = 0, max_val = 0;
			yyjson_arr_foreach(dep_include_arr, index, max_val, elem) {
				header_list = string_concat_cstr(
					str_arena, 7, string(header_list), "\n\"-I", "./deps/",
					key_str, "/", (char *)yyjson_get_str(elem), "\"");
			}
		}
	}

	String *headers, *src;
	headers = string_trim(str_arena, header_list);

	String *response_content = string_concat_cstr(
		str_arena, 4, "-c\n-fPIC\n-MMD\n-MP\n", string(headers), "\n",
		string(get_flags(str_arena, root, string_from(str_arena, "build"))));

	String *lib_links =
		get_flags(str_arena, root, string_from(str_arena, "lib"));

	// yyjson_doc_free(doc);

	create_append_err = create_append_file("./build/.cache/compile.rsp",
										   string(response_content));
	create_append_err =
		create_append_file("./build/.cache/lib_links.rsp", string(lib_links));
	// create_append_file("./build/.cache/compile.rsp", string(static_libs));

	if (create_append_err) {
		fprintf(stderr, "Error encountered while generating `compile.rsp`\n");
		vector_free(header_vec);
		vector_free(src_file_arr);
		vector_free(stat_file_arr);
		vector_free(shared_file_arr);
		goto CLEANUP;
	}

	bool require_version_update = false;

	for (int i = 0; i < length(src_file_arr); i++) {
		const char *base_name =
			get_filename_without_path(at(char *, src_file_arr, i));
		String *obj_file = string_concat_cstr(str_arena, 3, "./build/.cache/",
											  base_name, ".o");

		String *d_file = string_concat_cstr(str_arena, 3, "./build/.cache/",
											base_name, ".d");

		long long src_time =
			get_file_modified_time(at(char *, src_file_arr, i));
		long long obj_time = get_file_modified_time(string(obj_file));

		bool need_recompile = false;

		char *current_version = read_current_version_from_file(str_arena);

		if (obj_time == 0 || src_time > obj_time) {
			need_recompile = true;
		} else if (are_headers_newer(string(d_file), obj_time)) {
			need_recompile = true;
		} else if (STR_CMP(version_str, current_version) != 0) {
			need_recompile = true;
			require_version_update = true;
		}

		if (need_recompile) {
			char *compilation_command = string(string_concat_cstr(
				str_arena, 7, string(compiler), " -DVERSION=\\\"", version_str,
				"\\\" @./build/.cache/compile.rsp ",
				at(char *, src_file_arr, i), " -o ", string(obj_file)));
			cmd_err = system(compilation_command);
			if (cmd_err) {
				fprintf(stderr, "Error encountered at compilation\n");
				vector_free(header_vec);
				vector_free(src_file_arr);
				vector_free(stat_file_arr);
				vector_free(shared_file_arr);
				goto CLEANUP;
			}

			printf("[✓] Compiled '%s'\n", base_name);
		}
	}

	vector_free(src_file_arr);
	vector_free(stat_file_arr);
	vector_free(shared_file_arr);

	String *output = string_concat_cstr(global_str_arena, 2, "./build/",
										string(project_name));

	if (isExec) {
		cmd_err = system(string(string_concat_cstr(
			str_arena, 6, string(compiler), " ./build/.cache/*.o ",
			string(shared_lib), " -o ", string(output),
			" @./build/.cache/lib_links.rsp")));

		if (cmd_err) {
			fprintf(stderr, "Error encountered while generating executable\n");
			vector_free(header_vec);
			goto CLEANUP;
		}
		printf("[✓] Executable ganerated\n");
	} else {
		String *archiever = get_archiever(str_arena);
		if (STR_CMP(string(archiever), "") == 0) {
			printf("[x] No archiever found!\n");
			vector_free(header_vec);
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
				fprintf(
					stderr,
					"Error encountered while generating library directories\n");
				vector_free(header_vec);
				goto CLEANUP;
			}
		}

		cmd_err = system(string(string_concat_cstr(
			str_arena, 5, string(compiler),
			" -shared ./build/.cache/*.o -o ./build/shared/lib/lib",
			string(project_name), ".so", " @./build/.cache/lib_links.rsp")));

		if (cmd_err) {
			fprintf(stderr,
					"Error encountered while generating shared library\n");
			vector_free(header_vec);
			goto CLEANUP;
		}
		cmd_err = system(string(string_concat_cstr(
			str_arena, 4, string(archiever), " rcs ./build/static/lib/lib",
			string(project_name), ".a ./build/.cache/*.o")));
		if (cmd_err) {
			fprintf(stderr,
					"Error encountered while generating static library\n");
			vector_free(header_vec);
			goto CLEANUP;
		}

		for (int i = 0; i < length(header_vec); i++) {
			char *src_path = at(char *, header_vec, i);
			const char *file_name = get_filename_without_path(src_path);
			char *dest_path_1 = string(string_concat_cstr(
				str_arena, 2, "./build/static/include/", file_name));
			char *dest_path_2 = string(string_concat_cstr(
				str_arena, 2, "./build/shared/include/", file_name));

			copy_err = copy_file(src_path, dest_path_1);
			copy_err = copy_file(src_path, dest_path_2);
			if (copy_err) {
				fprintf(stderr, "Error encountered while copying headers\n");
				vector_free(header_vec);
				goto CLEANUP;
			}
		}

		printf("[✓] Libraries ganerated\n");
		vector_free(header_vec);
	}
	if (require_version_update) {
		update_version_file(version_str);
	}
	// generate_compile_commands();

CLEANUP:
	if (doc) {
		yyjson_doc_free(doc);
	}
	arena_free(&str_arena);
	return output;
}

void run_project(Arena *global_str_arena) {
	char *command = string(build_project(global_str_arena));
	system(command);
}
