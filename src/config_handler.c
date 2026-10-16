#include <flint.h>

Config *config_init() {
	Arena *arena = arena_init(1024 * 1024);
	Config *conf = arena_alloc(arena, sizeof(Config));
	conf->project_name = NULL;
	conf->project_language = NULL;
	conf->compiler_path = NULL;
	conf->version = NULL;
	conf->executable = true;
	conf->flags = NULL;
	conf->lib_links = NULL;
	conf->excludes = NULL;
	conf->exclude_dirs = NULL;
	conf->exclude_exception = NULL;
	conf->tmpl = NULL;
	conf->sync = NULL;
	conf->dependencies = NULL;
	conf->arena = arena;

	return conf;
}

void sync_config_free(Sync_config *conf) {
	if (conf->flags != NULL) {
		vector_free(&conf->flags);
	}
	if (conf->lib_links != NULL) {
		vector_free(&conf->lib_links);
	}
	if (conf->exclude_dirs != NULL) {
		vector_free(&conf->exclude_dirs);
	}
	if (conf->excludes != NULL) {
		vector_free(&conf->excludes);
	}
	if (conf->exclude_exception != NULL) {
		vector_free(&conf->exclude_exception);
	}
	if (conf->tmpl != NULL) {
		map_free(conf->tmpl);
	}
}

void config_free(Config *conf) {
	if (conf->flags != NULL) {
		vector_free(&conf->flags);
	}
	if (conf->lib_links != NULL) {
		vector_free(&conf->lib_links);
	}
	if (conf->exclude_dirs != NULL) {
		vector_free(&conf->exclude_dirs);
	}
	if (conf->excludes != NULL) {
		vector_free(&conf->excludes);
	}
	if (conf->exclude_exception != NULL) {
		vector_free(&conf->exclude_exception);
	}
	if (conf->tmpl != NULL) {
		map_free(conf->tmpl);
	}
	if (conf->dependencies != NULL) {
		map_free(conf->dependencies);
	}

	if (conf->sync != NULL) {
		Vector *sync_keys = map_keys(conf->sync);
		for (int i = 0; i < length(sync_keys); i++) {
			sync_config_free(
				(Sync_config *)map_get(conf->sync, at(char *, sync_keys, i)));
		}
		vector_free(&sync_keys);
		map_free(conf->sync);
	}
	Arena *arena_to_free = conf->arena;
	arena_free(&arena_to_free);
}

void read_flint_composition(char *composition_path, Config *conf) {

	yyjson_doc *doc = yyjson_read_file(composition_path, 0, NULL, NULL);
	if (!doc) {
		fprintf(stderr, "Failed to read or parse JSON file.\n");
		return;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	conf->project_name = safe_get_string(conf->arena, root, "project_name");
	conf->project_language =
		safe_get_string(conf->arena, root, "project_language");
	conf->compiler_path = safe_get_string(conf->arena, root, "compiler_path");
	conf->version = safe_get_string(conf->arena, root, "version");
	conf->executable = yyjson_get_bool(yyjson_obj_get(root, "executable"));

	conf->flags = parse_string_array(conf->arena, root, "flags");
	conf->lib_links = parse_string_array(conf->arena, root, "lib_links");
	conf->excludes = parse_string_array(conf->arena, root, "excludes");
	conf->exclude_dirs = parse_string_array(conf->arena, root, "exclude_dirs");
	conf->exclude_exception =
		parse_string_array(conf->arena, root, "exclude_exception");

	size_t idx, max;
	yyjson_val *val, *key;

	yyjson_val *dependencies = yyjson_obj_get(root, "dependencies");
	conf->dependencies = map_init();

	if (yyjson_is_obj(dependencies)) {
		yyjson_obj_foreach(dependencies, idx, max, key, val) {
			Dependency *dep = arena_alloc(conf->arena, sizeof(Dependency));
			dep->version = safe_get_string(conf->arena, val, "version");
			dep->remote = safe_get_string(conf->arena, val, "remote");
			dep->hash = safe_get_string(conf->arena, val, "hash");
			map_add(conf->dependencies, yyjson_get_str(key), dep);
		}
	}

	yyjson_val *tmpl = yyjson_obj_get(root, "tmpl");
	conf->tmpl = map_init();
	if (yyjson_is_obj(tmpl)) {
		yyjson_obj_foreach(tmpl, idx, max, key, val) {
			map_add(conf->tmpl, yyjson_get_str(key),
					string_from(conf->arena, (char *)yyjson_get_str(val)));
		}
	}

	yyjson_val *sync = yyjson_obj_get(root, "sync");
	conf->sync = map_init();
	if (yyjson_is_obj(sync)) {
		yyjson_obj_foreach(sync, idx, max, key, val) {
			Sync_config *sync_config =
				arena_alloc(conf->arena, sizeof(Sync_config));
			sync_config->version = safe_get_string(conf->arena, val, "version");
			sync_config->remote = safe_get_string(conf->arena, val, "remote");
			sync_config->hash = safe_get_string(conf->arena, val, "hash");

			size_t idx_sync, max_sync;
			yyjson_val *key_sync, *val_sync;

			sync_config->flags = parse_string_array(conf->arena, val, "flags");
			sync_config->lib_links =
				parse_string_array(conf->arena, val, "lib_links");
			sync_config->excludes =
				parse_string_array(conf->arena, val, "excludes");
			sync_config->exclude_dirs =
				parse_string_array(conf->arena, val, "exclude_dirs");
			sync_config->exclude_exception =
				parse_string_array(conf->arena, val, "exclude_exception");
			yyjson_val *tmpl = yyjson_obj_get(val, "tmpl");
			if (tmpl) {
				sync_config->tmpl = map_init();
				yyjson_obj_foreach(tmpl, idx_sync, max_sync, key_sync,
								   val_sync) {
					map_add(sync_config->tmpl, yyjson_get_str(key_sync),
							string_from(conf->arena,
										(char *)yyjson_get_str(val_sync)));
				}
			}
			map_add(conf->sync, yyjson_get_str(key), sync_config);
		}
	}

	yyjson_doc_free(doc);
}

void write_flint_composition(char *composition_path, Config *conf) {
	if (!conf || !composition_path)
		return;

	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);

	yyjson_mut_obj_add_str(doc, root, "project_name",
						   safe_str(conf->project_name));
	yyjson_mut_obj_add_str(doc, root, "project_language",
						   safe_str(conf->project_language));
	yyjson_mut_obj_add_str(doc, root, "compiler_path",
						   safe_str(conf->compiler_path));
	yyjson_mut_obj_add_str(doc, root, "version", safe_str(conf->version));
	yyjson_mut_obj_add_bool(doc, root, "executable", conf->executable);

	yyjson_mut_val *flags = yyjson_mut_arr(doc);
	if (conf->flags) {
		for (int i = 0; i < length(conf->flags); i++) {
			String *s = at(String *, conf->flags, i);
			if (s) {
				yyjson_mut_arr_add_str(doc, flags, safe_str(s));
			}
		}
	}
	yyjson_mut_obj_add_val(doc, root, "flags", flags);

	yyjson_mut_val *lib_links = yyjson_mut_arr(doc);
	if (conf->lib_links) {
		for (int i = 0; i < length(conf->lib_links); i++) {
			String *s = at(String *, conf->lib_links, i);
			if (s) {
				yyjson_mut_arr_add_str(doc, lib_links, safe_str(s));
			}
		}
	}
	yyjson_mut_obj_add_val(doc, root, "lib_links", lib_links);

	yyjson_mut_val *excludes = yyjson_mut_arr(doc);
	if (conf->excludes) {
		for (int i = 0; i < length(conf->excludes); i++) {
			String *s = at(String *, conf->excludes, i);
			if (s) {
				yyjson_mut_arr_add_str(doc, excludes, safe_str(s));
			}
		}
	}
	yyjson_mut_obj_add_val(doc, root, "excludes", excludes);

	yyjson_mut_val *exclude_dirs = yyjson_mut_arr(doc);
	if (conf->exclude_dirs) {
		for (int i = 0; i < length(conf->exclude_dirs); i++) {
			String *s = at(String *, conf->exclude_dirs, i);
			if (s) {
				yyjson_mut_arr_add_str(doc, exclude_dirs, safe_str(s));
			}
		}
	}
	yyjson_mut_obj_add_val(doc, root, "exclude_dirs", exclude_dirs);

	yyjson_mut_val *exclude_exception = yyjson_mut_arr(doc);
	if (conf->exclude_exception) {
		for (int i = 0; i < length(conf->exclude_exception); i++) {
			String *s = at(String *, conf->exclude_exception, i);
			if (s) {
				yyjson_mut_arr_add_str(doc, exclude_exception, safe_str(s));
			}
		}
	}
	yyjson_mut_obj_add_val(doc, root, "exclude_exception", exclude_exception);

	yyjson_mut_val *tmpl = yyjson_mut_obj(doc);
	if (conf->tmpl) {
		Vector *tmpl_keys = map_keys(conf->tmpl);
		if (tmpl_keys) {
			for (int i = 0; i < length(tmpl_keys); i++) {
				char *k = at(char *, tmpl_keys, i);
				if (!k)
					continue;
				String *v = (String *)map_get(conf->tmpl, k);
				yyjson_mut_obj_add_strcpy(doc, tmpl, k, safe_str(v));
			}
			vector_free(&tmpl_keys);
		}
	}
	yyjson_mut_obj_add_val(doc, root, "tmpl", tmpl);

	yyjson_mut_val *dependencies = yyjson_mut_obj(doc);
	if (conf->dependencies) {
		Vector *dependency_keys = map_keys(conf->dependencies);
		if (dependency_keys) {
			for (int i = 0; i < length(dependency_keys); i++) {
				char *key = at(char *, dependency_keys, i);
				if (!key)
					continue;

				Dependency *elem =
					(Dependency *)map_get(conf->dependencies, key);
				if (!elem)
					continue;

				yyjson_mut_val *dep_obj = yyjson_mut_obj(doc);
				yyjson_mut_obj_add_strcpy(doc, dep_obj, "version",
										  safe_str(elem->version));
				yyjson_mut_obj_add_strcpy(doc, dep_obj, "remote",
										  safe_str(elem->remote));
				yyjson_mut_obj_add_strcpy(doc, dep_obj, "hash",
										  safe_str(elem->hash));

				yyjson_mut_obj_add_val(doc, dependencies, key, dep_obj);
			}
			vector_free(&dependency_keys);
		}
	}
	yyjson_mut_obj_add_val(doc, root, "dependencies", dependencies);

	yyjson_mut_val *sync = yyjson_mut_obj(doc);
	yyjson_mut_obj_add_val(doc, root, "sync", sync);

	yyjson_write_flag flg = YYJSON_WRITE_PRETTY;
	yyjson_write_err err;
	bool success =
		yyjson_mut_write_file(composition_path, doc, flg, NULL, &err);

	if (success) {
		printf("[✓] Composition created/updated...\n");
	} else {
		printf("[x] Failed to write JSON: %s (code: %u)\n", err.msg, err.code);
	}
	yyjson_mut_doc_free(doc);
}

Cmap *read_lock_file() {
	Cmap *lock = map_init();

	yyjson_doc *doc = yyjson_read_file("flint.lock", 0, NULL, NULL);
	if (!doc) {
		return lock;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *key, *val;
	size_t idx, max;

	if (yyjson_is_obj(root)) {
		yyjson_obj_foreach(root, idx, max, key, val) {
			Dependency *dep = arena_alloc(lock->arena, sizeof(Dependency));
			dep->version = safe_get_string(lock->arena, val, "version");
			dep->remote = safe_get_string(lock->arena, val, "remote");
			dep->hash = safe_get_string(lock->arena, val, "hash");
			map_add(lock, yyjson_get_str(key), dep);
		}
	}

	yyjson_doc_free(doc);

	return lock;
}

bool lock_contains(Cmap *lock, char *libURL) {
	if (map_len(lock)) {
		char *repo_name = get_repo_name(lock->arena, libURL);
		Dependency *lock_entry = map_get(lock, repo_name);
		if (lock_entry) {
			if (STR_CMP(string(lock_entry->remote), libURL) == 0) {
				return true;
			}
		}
	}
	return false;
}

void write_lock_file(Cmap *lock) {
	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);
	if (lock) {
		Vector *dependency_keys = map_keys(lock);
		if (dependency_keys) {
			for (int i = 0; i < length(dependency_keys); i++) {
				char *key = at(char *, dependency_keys, i);
				if (!key)
					continue;

				Dependency *elem = (Dependency *)map_get(lock, key);
				if (!elem)
					continue;

				yyjson_mut_val *dep_obj = yyjson_mut_obj(doc);
				yyjson_mut_obj_add_strcpy(doc, dep_obj, "version",
										  safe_str(elem->version));
				yyjson_mut_obj_add_strcpy(doc, dep_obj, "remote",
										  safe_str(elem->remote));
				yyjson_mut_obj_add_strcpy(doc, dep_obj, "hash",
										  safe_str(elem->hash));

				yyjson_mut_obj_add_val(doc, root, key, dep_obj);
			}
			vector_free(&dependency_keys);
		}
	}
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY;
	yyjson_write_err err;
	bool success = yyjson_mut_write_file("flint.lock", doc, flg, NULL, &err);
	map_free(lock);
	yyjson_mut_doc_free(doc);
}

// void create_my_build_config(char *config_file_path, char *project_name,
// 							char *project_lang, char *compiler_path,
// 							bool isExec) {
// 	Arena *str_arena = arena_init(1024);
// 	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
// 	yyjson_mut_val *root = yyjson_mut_obj(doc);
// 	yyjson_mut_doc_set_root(doc, root);

// 	yyjson_mut_obj_add_str(doc, root, "project_name", project_name);
// 	yyjson_mut_obj_add_str(doc, root, "project_language", project_lang);
// 	yyjson_mut_obj_add_str(doc, root, "compiler_path", compiler_path);
// 	yyjson_mut_obj_add_str(doc, root, "version", "unknown");
// 	yyjson_mut_obj_add_bool(doc, root, "executable", isExec);

// 	// yyjson_mut_val *headers = yyjson_mut_arr(doc);
// 	// yyjson_mut_val *sources = yyjson_mut_arr(doc);
// 	yyjson_mut_val *flags = yyjson_mut_arr(doc);
// 	yyjson_mut_val *lib_links = yyjson_mut_arr(doc);
// 	yyjson_mut_val *static_lib = yyjson_mut_arr(doc);
// 	yyjson_mut_val *shared_lib = yyjson_mut_arr(doc);
// 	yyjson_mut_val *excludes = yyjson_mut_arr(doc);
// 	yyjson_mut_val *exclude_dirs = yyjson_mut_arr(doc);
// 	yyjson_mut_val *tmpl = yyjson_mut_obj(doc);

// 	// if (isExec) {
// 	// 	yyjson_mut_arr_add_str(doc, sources, "src");
// 	// } else {
// 	// 	yyjson_mut_arr_add_str(doc, sources, "lib");
// 	// }

// 	// yyjson_mut_arr_add_str(doc, static_lib, "static");
// 	// yyjson_mut_arr_add_str(doc, shared_lib, "shared");

// 	// yyjson_mut_obj_add_val(doc, root, "include_paths", headers);
// 	// yyjson_mut_obj_add_val(doc, root, "src", sources);
// 	yyjson_mut_obj_add_val(doc, root, "flags", flags);
// 	yyjson_mut_obj_add_val(doc, root, "lib_links", lib_links);
// 	// yyjson_mut_obj_add_val(doc, root, "static_lib", static_lib);
// 	// yyjson_mut_obj_add_val(doc, root, "shared_lib", shared_lib);
// 	yyjson_mut_obj_add_val(doc, root, "excludes", excludes);
// 	yyjson_mut_obj_add_val(doc, root, "exclude_dirs", exclude_dirs);
// 	yyjson_mut_obj_add_val(doc, root, "tmpl", tmpl);

// 	// yyjson_mut_arr_add_str(doc, headers, "include");
// 	// yyjson_mut_arr_add_str(doc, headers, "./deps/include");

// 	yyjson_mut_val *dependencies = yyjson_mut_obj(doc);
// 	yyjson_mut_obj_add_val(doc, root, "dependencies", dependencies);

// 	yyjson_write_err err;
// 	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
// 	if (!yyjson_mut_write_file(config_file_path, doc, flg, NULL, &err)) {
// 		fprintf(stderr, "Write error: %s\n", err.msg);
// 		goto CLEANUP;
// 	}
// CLEANUP:
// 	yyjson_mut_doc_free(doc);
// 	arena_free(&str_arena);
// }

// int generate_compile_commands(Config *conf) {
// 	char *input_file = "composition.json";
// 	char *output_file = "compile_commands.json";
// 	int success = 1;
// 	String *cwd = get_current_working_dir(conf->arena);

// 	Vector *include_paths = vector_init(char *);
// 	Vector *src_paths = vector_init(char *);
// 	Vector *static_libs = vector_init(char *);
// 	Vector *shared_libs = vector_init(char *);
// 	size_t idx = 0, max = 0;
// 	yyjson_val *val, *key;
// 	traverse_dir(conf->arena, string_from(conf->arena, "."), src_paths,
// 				 include_paths, static_libs, shared_libs, conf->excludes);

// 	for (int i = 0; i < length(conf->exclude_exception); i++) {
// 		set_add(src_paths, at(char *, conf->exclude_exception, i));
// 		set_add(include_paths, at(char *, conf->exclude_exception, i));
// 	}
// 	Vector *source_files = vector_init(char *);
// 	get_src_vec(conf, src_paths, source_files, cwd);

// 	yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
// 	yyjson_mut_val *out_root = yyjson_mut_arr(out_doc);
// 	yyjson_mut_doc_set_root(out_doc, out_root);

// 	String *includes = string_from(conf->arena, "");
// 	for (size_t i = 0; i < length(include_paths); i++) {
// 		includes = string_concat_cstr(conf->arena, 3, string(includes), " -I",
// 									  at(char *, include_paths, i));
// 	}

// 	for (size_t i = 0; i < length(source_files); i++) {
// 		yyjson_mut_val *entry = yyjson_mut_arr_add_obj(out_doc, out_root);

// 		success =
// 			yyjson_mut_obj_add_str(out_doc, entry, "directory", string(cwd));
// 		if (!success) {
// 			fprintf(stderr,
// 					"Error encountered while generating compile commands\n");
// 			success = 0;
// 			vector_free(&include_paths);
// 			vector_free(&src_paths);
// 			vector_free(&source_files);
// 			vector_free(&static_libs);
// 			vector_free(&shared_libs);
// 			yyjson_mut_doc_free(out_doc);
// 			goto CLEANUP;
// 		}
// 		success = yyjson_mut_obj_add_str(out_doc, entry, "file",
// 										 at(char *, source_files, i));
// 		if (!success) {
// 			fprintf(stderr,
// 					"Error encountered while generating compile commands\n");
// 			success = 0;
// 			vector_free(&include_paths);
// 			vector_free(&src_paths);
// 			vector_free(&source_files);
// 			vector_free(&static_libs);
// 			vector_free(&shared_libs);
// 			yyjson_mut_doc_free(out_doc);
// 			goto CLEANUP;
// 		}

// 		String *command = string_from(conf->arena, "");
// 		command = string_concat_cstr(
// 			conf->arena, 7, string(conf->compiler_path), " -c ",
// 			at(char *, source_files, i), string(includes), " -o ",
// 			at(char *, source_files, i), ".o");

// 		success =
// 			yyjson_mut_obj_add_str(out_doc, entry, "command", string(command));
// 		if (!success) {
// 			fprintf(stderr,
// 					"Error encountered while generating compile commands\n");
// 			success = 0;
// 			vector_free(&include_paths);
// 			vector_free(&src_paths);
// 			vector_free(&source_files);
// 			vector_free(&static_libs);
// 			vector_free(&shared_libs);
// 			yyjson_mut_doc_free(out_doc);
// 			goto CLEANUP;
// 		}
// 	}

// 	yyjson_write_err write_err;
// 	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
// 	success =
// 		yyjson_mut_write_file(output_file, out_doc, flg, NULL, &write_err);

// 	if (!success) {
// 		fprintf(stderr, "Failed to write %s: %s\n", output_file, write_err.msg);
// 	}
// 	vector_free(&include_paths);
// 	vector_free(&src_paths);
// 	vector_free(&source_files);
// 	vector_free(&static_libs);
// 	vector_free(&shared_libs);
// 	yyjson_mut_doc_free(out_doc);

// CLEANUP:
// 	return success ? 0 : -1;
// }

int generate_compile_commands(Config *conf) {
	// char *input_file = "composition.json";
	char *output_file = "compile_commands.json";
	int success = 1;
	String *cwd = get_current_working_dir(conf->arena);

	Vector *include_paths = vector_init(char *);
	Vector *src_paths = vector_init(char *);
	Vector *static_libs = vector_init(char *);
	Vector *shared_libs = vector_init(char *);
	size_t idx = 0, max = 0;
	yyjson_val *val, *key;
	traverse_dir(conf->arena, string_from(conf->arena, "."), src_paths,
				 include_paths, static_libs, shared_libs, conf->exclude_dirs);

	if (conf->exclude_exception) {
		for (int i = 0; i < length(conf->exclude_exception); i++) {
			set_add(src_paths, at(char *, conf->exclude_exception, i));
			set_add(include_paths, at(char *, conf->exclude_exception, i));
		}
	}
	Vector *source_files = vector_init(char *);
	get_src_vec(conf, src_paths, source_files, cwd);

	yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *out_root = yyjson_mut_arr(out_doc);
	yyjson_mut_doc_set_root(out_doc, out_root);

	String *includes = string_from(conf->arena, "");
	for (size_t i = 0; i < length(include_paths); i++) {
		includes = string_concat_cstr(conf->arena, 3, string(includes), " -I",
									  at(char *, include_paths, i));
	}

	for (size_t i = 0; i < length(source_files); i++) {
		yyjson_mut_val *entry = yyjson_mut_arr_add_obj(out_doc, out_root);

		success =
			yyjson_mut_obj_add_str(out_doc, entry, "directory", string(cwd));
		if (!success) {
			fprintf(stderr,
					"Error encountered while generating compile commands\n");
			success = 0;
			vector_free(&include_paths);
			vector_free(&src_paths);
			vector_free(&source_files);
			vector_free(&static_libs);
			vector_free(&shared_libs);
			yyjson_mut_doc_free(out_doc);
			goto CLEANUP;
		}
		success = yyjson_mut_obj_add_str(out_doc, entry, "file",
										 at(char *, source_files, i));
		if (!success) {
			fprintf(stderr,
					"Error encountered while generating compile commands\n");
			success = 0;
			vector_free(&include_paths);
			vector_free(&src_paths);
			vector_free(&source_files);
			vector_free(&static_libs);
			vector_free(&shared_libs);
			yyjson_mut_doc_free(out_doc);
			goto CLEANUP;
		}

		String *command = string_from(conf->arena, "");
		command = string_concat_cstr(
			conf->arena, 7, string(conf->compiler_path), " -c ",
			at(char *, source_files, i), string(includes), " -o ",
			at(char *, source_files, i), ".o");

		success =
			yyjson_mut_obj_add_str(out_doc, entry, "command", string(command));
		if (!success) {
			fprintf(stderr,
					"Error encountered while generating compile commands\n");
			success = 0;
			vector_free(&include_paths);
			vector_free(&src_paths);
			vector_free(&source_files);
			vector_free(&static_libs);
			vector_free(&shared_libs);
			yyjson_mut_doc_free(out_doc);
			goto CLEANUP;
		}
	}

	yyjson_write_err write_err;
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
	success =
		yyjson_mut_write_file(output_file, out_doc, flg, NULL, &write_err);

	if (!success) {
		fprintf(stderr, "Failed to write %s: %s\n", output_file, write_err.msg);
	}
	vector_free(&include_paths);
	vector_free(&src_paths);
	vector_free(&source_files);
	vector_free(&static_libs);
	vector_free(&shared_libs);
	yyjson_mut_doc_free(out_doc);

CLEANUP:
	return success ? 0 : -1;
}

bool is_mybuild_config_present(char *filename) {
	FILE *file = fopen(filename, "r");
	if (file) {
		fclose(file);
		return true;
	}
	return false;
}

// String *get_flags(Arena *str_arena, yyjson_val *root, String *type) {
// 	Arena *local_arena = arena_init(1024);
// 	String *flag_list = string_from(local_arena, "");

// 	yyjson_val *flag_arr;

// 	if (STR_CMP(string(type), "build") == 0) {
// 		flag_arr = yyjson_obj_get(root, "flags");
// 	} else if (STR_CMP(string(type), "lib") == 0) {
// 		flag_arr = yyjson_obj_get(root, "lib_links");
// 	}

// 	size_t idx = 0, max = 0;
// 	yyjson_val *val;

// 	yyjson_arr_foreach(flag_arr, idx, max, val) {
// 		if (string_len(flag_list) == 0) {
// 			flag_list = string_from(local_arena, (char *)yyjson_get_str(val));
// 		} else {
// 			flag_list = string_concat_cstr(local_arena, 3, string(flag_list),
// 										   "\n", (char *)yyjson_get_str(val));
// 		}
// 	}

// 	String *export_list = string_from(str_arena, string(flag_list));
// 	arena_free(&local_arena);
// 	return export_list;
// }

void _add_local(Config *conf, int lib_count, char **lib_link, char *element) {

	// Arena *arena = arena_init(1024);

	// char *input_file = "composition.json";

	char *prefix = "-";
	Vector *element_arr = conf->flags;
	if (STR_CMP(element, "lib_links") == 0) {
		prefix = "-l";
		element_arr = conf->lib_links;
	}

	// yyjson_read_err err;
	// yyjson_doc *doc = yyjson_read_file(input_file, 0, NULL, &err);

	// if (!doc) {
	// 	fprintf(stderr, "Failed to read %s: %s\n", input_file, err.msg);
	// 	return;
	// }
	// yyjson_mut_doc *mut_doc = yyjson_doc_mut_copy(doc, NULL);

	// yyjson_doc_free(doc);

	// yyjson_mut_val *root = yyjson_mut_doc_get_root(mut_doc);
	// yyjson_mut_val *local_elem = yyjson_mut_obj_get(root, element);

	// Vector *temp_vec = vector_init(char *);

	// int idx = 0, max = 0;
	// yyjson_mut_val *val_mut;
	// yyjson_mut_arr_foreach(local_elem, idx, max, val_mut) {
	// 	set_add(temp_vec, (char *)yyjson_mut_get_str(val_mut));
	// }

	for (int i = 0; i < lib_count; i++) {
		char *current_lib = lib_link[i];
		set_add_str(element_arr,
					string_concat_cstr(conf->arena, 2, prefix, current_lib));
	}

	// yyjson_mut_arr_clear(local_elem);
	// for (int i = 0; i < length(temp_vec); i++) {
	// 	yyjson_mut_arr_add_str(mut_doc, local_elem, at(char *, temp_vec, i));
	// }

	// yyjson_write_err werr;
	// yyjson_write_flag flg = YYJSON_WRITE_PRETTY |
	// YYJSON_WRITE_ESCAPE_UNICODE; if
	// (!yyjson_mut_write_file("./composition.json", mut_doc, flg, NULL,
	// &werr)) { 	fprintf(stderr, "Write error: %s\n", werr.msg);
	// }

	// vector_free(&temp_vec);
	// yyjson_mut_doc_free(mut_doc);
	// arena_free(&arena);
}

void add_local_lib(int lib_count, char **lib_link) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);
	_add_local(conf, lib_count, lib_link, "lib_links");
	write_flint_composition("composition.json", conf);
	config_free(conf);
}
void add_flag(int lib_count, char **lib_link) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);
	_add_local(conf, lib_count, lib_link, "flags");
	write_flint_composition("composition.json", conf);
	config_free(conf);
}
