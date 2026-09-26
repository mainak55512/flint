#include <flint.h>

bool starts_with(char *str, char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

void update_package_file(yyjson_mut_doc *package) {
	yyjson_write_err werr;
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
	if (!yyjson_mut_write_file("./deps/.package", package, flg, NULL, &werr)) {
		fprintf(stderr, "Write error: %s\n", werr.msg);
	}
}

void remove_arr_entry(yyjson_mut_val *arr, char *search_str) {
	/*
	if (yyjson_mut_is_arr(arr)) {
		size_t idx, max;
		yyjson_mut_val *val;

		yyjson_mut_arr_foreach(arr, idx, max, val) {
			const char *str = yyjson_mut_get_str(val);
			if (starts_with((char *)str, search_str)) {
				yyjson_mut_arr_remove(arr, idx);
			}
		}
	}
*/
	if (!yyjson_mut_is_arr(arr) || !search_str)
		return;

	size_t count = yyjson_mut_arr_size(arr);
	for (size_t i = count; i > 0; i--) {
		size_t idx = i - 1;
		yyjson_mut_val *val = yyjson_mut_arr_get(arr, idx);
		const char *str = yyjson_mut_get_str(val);

		if (str && starts_with((char *)str, search_str)) {
			yyjson_mut_arr_remove(arr, idx);
		}
	}
}

void sync_dependency() {
	char *myBuildConfigFile = "composition.json";
	char *packageFile = "deps/.package";
	yyjson_read_err err;
	yyjson_doc *buildConf = yyjson_read_file(myBuildConfigFile, 0, NULL, &err);
	if (!buildConf) {
		fprintf(stderr, "Failed to read %s: %s\n", myBuildConfigFile, err.msg);
		return;
	}
	yyjson_doc *packageConf = yyjson_read_file(packageFile, 0, NULL, &err);
	if (!packageConf) {
		fprintf(stderr, "Failed to read %s: %s\n", packageFile, err.msg);
		yyjson_doc_free(buildConf);
		return;
	}

	yyjson_mut_doc *buildConf_mut = yyjson_doc_mut_copy(buildConf, NULL);
	yyjson_mut_doc *packageConf_mut = yyjson_doc_mut_copy(packageConf, NULL);

	yyjson_mut_val *build_root = yyjson_mut_doc_get_root(buildConf_mut);
	yyjson_mut_val *package_root = yyjson_mut_doc_get_root(packageConf_mut);

	yyjson_mut_val *deps = yyjson_mut_obj_get(build_root, "dependencies");
	yyjson_mut_val *inst_pkg = yyjson_mut_obj_get(package_root, "packages");

	Vector *installed = vector_init(char *);
	// Vector *dep_arr = vector_init(char *);
	// Vector *not_installed = vector_init(char *);

	if (yyjson_mut_is_arr(inst_pkg)) {
		yyjson_mut_arr_iter iter;
		yyjson_mut_arr_iter_init(inst_pkg, &iter);
		yyjson_mut_val *val;
		while ((val = yyjson_mut_arr_iter_next(&iter))) {
			set_add(installed, (char *)yyjson_mut_get_str(val));
		}
	}

	if (yyjson_mut_is_obj(deps)) {
		Arena *local_arena = arena_init(1024);
		yyjson_mut_obj_iter iter;
		yyjson_mut_obj_iter_init(deps, &iter);
		yyjson_mut_val *key, *dep_obj;
		while ((key = yyjson_mut_obj_iter_next(&iter))) {
			dep_obj = yyjson_mut_obj_iter_get_val(key);
			const char *dep_name = yyjson_mut_get_str(key);

			char *hash = "";

			yyjson_mut_val *dep_remote = yyjson_mut_obj_get(dep_obj, "remote");
			yyjson_mut_val *dep_version =
				yyjson_mut_obj_get(dep_obj, "version");
			yyjson_mut_val *dep_hash = yyjson_mut_obj_get(dep_obj, "hash");

			if (dep_hash) {
				hash = (char *)yyjson_mut_get_str(dep_hash);
			}

			// yyjson_mut_val *src = yyjson_mut_obj_get(dep_obj, "src");
			// yyjson_mut_val *include_paths =
			// 	yyjson_mut_obj_get(dep_obj, "include_paths");
			yyjson_mut_val *flags = yyjson_mut_obj_get(dep_obj, "flags");
			yyjson_mut_val *lib_links =
				yyjson_mut_obj_get(dep_obj, "lib_links");
			yyjson_mut_val *tmpl = yyjson_mut_obj_get(dep_obj, "tmpl");
			// yyjson_mut_val *stat_lib =
			// 	yyjson_mut_obj_get(dep_obj, "static_lib");
			// yyjson_mut_val *shared_lib =
			// 	yyjson_mut_obj_get(dep_obj, "shared_lib");
			yyjson_mut_val *excludes = yyjson_mut_obj_get(dep_obj, "excludes");
			yyjson_mut_val *exclude_dirs =
				yyjson_mut_obj_get(dep_obj, "exclude_dirs");
			yyjson_mut_val *exclude_exception_dirs =
				yyjson_mut_obj_get(dep_obj, "exclude_exception");
			if (!set_contains(installed,
							  (char *)yyjson_mut_get_str(dep_remote))) {

				set_add(installed, (char *)yyjson_mut_get_str(dep_remote));

				// fetch_library(installed, (char
				// *)yyjson_mut_get_str(dep_remote), 			  true);
				char *modified_url = string(string_concat_cstr(
					local_arena, 3, (char *)yyjson_mut_get_str(dep_remote), "@",
					(char *)yyjson_mut_get_str(dep_version)));
				// fetch_library(installed, modified_url, src, include_paths,
				// 			  flags, lib_links, stat_lib, shared_lib, true,
				// 			  hash, excludes);
				fetch_library(installed, modified_url, /* src, include_paths,*/
							  flags, lib_links,		   /*stat_lib, shared_lib,*/
							  hash, excludes, exclude_dirs, tmpl,
							  exclude_exception_dirs, true);
			}
		}

		yyjson_mut_val *package_arr = yyjson_mut_arr(packageConf_mut);

		for (int i = 0; i < length(installed); i++) {
			yyjson_mut_val *val =
				yyjson_mut_str(packageConf_mut, at(char *, installed, i));
			yyjson_mut_arr_append(package_arr, val);
		}
		yyjson_mut_obj_put(package_root,
						   yyjson_mut_str(packageConf_mut, "packages"),
						   package_arr);
		update_package_file(packageConf_mut);
		arena_free(&local_arena);
	}

	generate_compile_commands();
	yyjson_mut_doc_free(buildConf_mut);
	yyjson_mut_doc_free(packageConf_mut);
	yyjson_doc_free(buildConf);
	yyjson_doc_free(packageConf);
	vector_free(installed);
	// vector_free(dep_arr);
	// vector_free(not_installed);
}

void add_library(char *libURL) {
	Arena *local_arena = arena_init(1024);
	char *url = get_modified_url(local_arena, libURL);
	if (url == NULL) {
		printf("[x] Invalid URL\n");
		return;
	}
	yyjson_read_err err;
	yyjson_doc *current_doc =
		yyjson_read_file("./composition.json", 0, NULL, &err);
	yyjson_val *current_root = yyjson_doc_get_root(current_doc);
	yyjson_val *dependencies = yyjson_obj_get(current_root, "dependencies");
	Vector *set = vector_init(char *);
	int idx = 0, max = 0;
	yyjson_val *val, *key;
	yyjson_obj_foreach(dependencies, idx, max, key, val) {
		yyjson_val *remote = yyjson_obj_get(val, "remote");
		set_add(set, (char *)yyjson_get_str(remote));
	}
	if (!set_contains(set, url)) {
		set_add(set, url);
		// fetch_library(set, libURL, NULL, NULL, NULL, NULL, NULL, NULL, false,
		// 			  "", NULL);
		fetch_library(set, libURL, /*NULL, NULL, NULL, NULL, */ NULL, NULL, "",
					  NULL, NULL, NULL, NULL, false);
	}
	yyjson_doc *package = yyjson_read_file("./deps/.package", 0, NULL, &err);
	yyjson_mut_doc *package_mut = yyjson_doc_mut_copy(package, NULL);
	yyjson_doc_free(package);
	yyjson_mut_val *root = yyjson_mut_doc_get_root(package_mut);
	yyjson_mut_val *package_arr = yyjson_mut_arr(package_mut);
	// yyjson_mut_val *package_arr = yyjson_mut_obj_get(root, "packages");

	for (int i = 0; i < length(set); i++) {
		yyjson_mut_val *val = yyjson_mut_str(package_mut, at(char *, set, i));
		yyjson_mut_arr_append(package_arr, val);
	}
	yyjson_mut_obj_put(root, yyjson_mut_str(package_mut, "packages"),
					   package_arr);
	// yyjson_mut_obj_add_val(package_mut, root, "packages", package_arr);

	generate_compile_commands();
	update_package_file(package_mut);
	yyjson_mut_doc_free(package_mut);
	yyjson_doc_free(current_doc);
	vector_free(set);
	arena_free(&local_arena);
}

/*
String *clone_lib(Arena *arena, char *libURL) {
	String *repo_name = string_from(arena, get_repo_name(arena, libURL));
	printf("Installing %s...\n", string(repo_name));
	String *command = string_concat_cstr(arena, 4, "git clone --quiet ", libURL,
										 " ./deps/", string(repo_name));
	system(string(command));
	printf("Done!\n");
	return repo_name;
}
*/

LibDetails *clone_lib(Arena *arena, char *libURL, const char *hash) {
	// @unknown will work for now, will change it later
	// char *modified_url_temp =
	// 	string(string_concat_cstr(arena, 2, libURL, "@unknown"));
	// char *version_number = get_version_number(arena, modified_url_temp);
	// char *url = get_modified_url(arena, modified_url_temp);
	char *version_number = get_version_number(arena, libURL);
	if (version_number == NULL) {
		printf("[x] Version details missing\n");
		return NULL;
	}
	char *url = get_modified_url(arena, libURL);
	char *repo_name = get_repo_name(arena, url);

	if (url == NULL || repo_name == NULL) {
		printf("[x] Invalid URL\n");
		return NULL;
	}

	char *target_dir =
		string(string_concat_cstr(arena, 2, "./deps/", repo_name));
	char *sink_path = ">/dev/null 2>&1";
	printf("[+] Installing %s...\n", repo_name);
	// printf("HASH: %s\n", hash);
	String *command;
	if (STR_CMP(hash, "") == 0) {
		if (STR_CMP(version_number, "unknown") == 0) {
			command = string_concat_cstr(
				arena, 4, "git clone --depth 1 --quiet ", url, " ", target_dir);
		} else {
			command = string_concat_cstr(
				arena, 8, "git clone --depth 1 --quiet --branch ",
				version_number, " ", url, " ", target_dir, " ", sink_path);
		}
	} else {
		return clone_lib_hashed(arena, url, hash);
	}

	if (system(string(command)) >> 8 == 128) {
		if (directory_exists(target_dir)) {
			remove_directory(arena, target_dir);
		}
		return NULL;
	}

	char *ref_hash = get_lib_hash(arena, target_dir);
	char *fetched_version = get_tag_from_hash(arena, target_dir, ref_hash);
	LibDetails *lib_details =
		(LibDetails *)arena_alloc(arena, sizeof(LibDetails));
	lib_details->repo_name = repo_name;
	lib_details->version = fetched_version;
	lib_details->hash = ref_hash;

	printf("[*] Library: %s\n", lib_details->repo_name);
	printf("[*] Version: %s\n", lib_details->version);
	printf("[*] Hash: %s\n", lib_details->hash);
	printf("[✓] Done!\n\n");

	remove_directory(arena,
					 string(string_concat_cstr(arena, 2, target_dir, "/.git")));
	return lib_details;
}

LibDetails *clone_lib_hashed(Arena *arena, const char *libURL,
							 const char *ref_hash) {
	LibDetails *lib_details =
		(LibDetails *)arena_alloc(arena, sizeof(LibDetails));
	char *repo_name = get_repo_name(arena, libURL);
	char *target_dir =
		string(string_concat_cstr(arena, 2, "./deps/", repo_name));
	char *sink_path = ">/dev/null 2>&1";
	char *command = string(string_concat_cstr(
		arena, 24, "mkdir -p ", target_dir, " ", sink_path, " && git -C ",
		target_dir, " init ", sink_path, " && git -C ", target_dir,
		" remote add origin ", libURL, " ", sink_path, " && git -C ",
		target_dir, " fetch --depth 1 --tags origin ", ref_hash, " ", sink_path,
		" && git -C ", target_dir, " checkout FETCH_HEAD ", sink_path));
	if (system(command) >> 8 == 128) {
		if (directory_exists(target_dir)) {
			remove_directory(arena, target_dir);
		}
		return NULL;
	}
	char *tag = get_tag_from_hash(arena, target_dir, ref_hash);

	// printf("Library: %s\n", repo_name);

	/*
	if (STR_CMP(tag, "") == 0) {
	  if (directory_exists(target_dir)) {
		remove_directory(arena, target_dir);
	  }
	  return NULL;
	}
	*/

	lib_details->repo_name = repo_name;
	lib_details->version = tag;
	lib_details->hash = (char *)ref_hash;

	printf("[*] Library: %s\n", lib_details->repo_name);
	printf("[*] Version: %s\n", lib_details->version);
	printf("[*] Hash: %s\n", lib_details->hash);
	printf("[✓] Done!\n\n");
	remove_directory(arena,
					 string(string_concat_cstr(arena, 2, target_dir, "/.git")));
	return lib_details;
}

void fetch_library(Vector *v, char *libURL, /*yyjson_mut_val *sync_src,
				   yyjson_mut_val *sync_include_paths,*/
				   yyjson_mut_val *sync_flags, yyjson_mut_val *sync_lib_links,
				   /*yyjson_mut_val *sync_stat, yyjson_mut_val *sync_shared,*/
				   const char *hash, yyjson_mut_val *sync_excludes,
				   yyjson_mut_val *sync_exclude_dirs, yyjson_mut_val *sync_tmpl,
				   yyjson_mut_val *sync_exclude_exception_dirs, bool sync) {
	String *command, *dep_mybuild_path;
	Arena *str_arena;
	yyjson_read_err err;

	str_arena = arena_init(2048);
	LibDetails *lib_details = clone_lib(str_arena, libURL, hash);

	if (lib_details == NULL) {
		arena_free(&str_arena);
		return;
	}

	String *config_path = string_concat_cstr(
		str_arena, 3, "./deps/", lib_details->repo_name, "/composition.json");
	if (!sync && !is_mybuild_config_present(string(config_path))) {
		arena_free(&str_arena);
		return;
	}
	yyjson_doc *current_doc =
		yyjson_read_file("./composition.json", 0, NULL, &err);
	yyjson_mut_doc *current_mut_doc = yyjson_doc_mut_copy(current_doc, NULL);

	yyjson_mut_val *current_root = yyjson_mut_doc_get_root(current_mut_doc);
	yyjson_mut_val *dependencies =
		yyjson_mut_obj_get(current_root, "dependencies");

	dep_mybuild_path = string_concat_cstr(
		str_arena, 3, "./deps/", lib_details->repo_name, "/composition.json");

	// printf("Dep build path: %s\n", string(dep_mybuild_path));
	yyjson_doc *dep_doc =
		yyjson_read_file(string(dep_mybuild_path), 0, NULL, &err);

	yyjson_val *dep_root = yyjson_doc_get_root(dep_doc);

	// yyjson_val *src = yyjson_obj_get(dep_root, "src");
	// yyjson_val *headers = yyjson_obj_get(dep_root, "include_paths");
	yyjson_val *dep_flags = yyjson_obj_get(dep_root, "flags");
	yyjson_val *dep_lib_links = yyjson_obj_get(dep_root, "lib_links");
	yyjson_mut_val *current_flags = yyjson_mut_obj_get(current_root, "flags");
	yyjson_mut_val *current_lib_links =
		yyjson_mut_obj_get(current_root, "lib_links");
	// yyjson_mut_val *current_src = yyjson_mut_obj_get(current_root, "src");
	// yyjson_mut_val *current_incl =
	// yyjson_mut_obj_get(current_root, "include_paths");
	// yyjson_mut_val *current_stat_lib =
	// 	yyjson_mut_obj_get(current_root, "static_lib");
	// yyjson_val *dep_stat_lib = yyjson_obj_get(dep_root, "static_lib");
	// yyjson_mut_val *current_shared_lib =
	// 	yyjson_mut_obj_get(current_root, "shared_lib");
	// yyjson_val *dep_shared_lib = yyjson_obj_get(dep_root, "shared_lib");
	yyjson_mut_val *current_excludes =
		yyjson_mut_obj_get(current_root, "excludes");
	yyjson_val *dep_excludes = yyjson_obj_get(dep_root, "excludes");
	yyjson_mut_val *current_exclude_dirs =
		yyjson_mut_obj_get(current_root, "exclude_dirs");
	yyjson_val *dep_exclude_dirs = yyjson_obj_get(dep_root, "exclude_dirs");
	yyjson_mut_val *current_exclude_exception_dirs =
		yyjson_mut_obj_get(current_root, "exclude_exception");
	yyjson_val *dep_exclude_exception_dirs =
		yyjson_obj_get(dep_root, "exclude_exception");
	yyjson_mut_val *current_tmpl = yyjson_mut_obj_get(current_root, "tmpl");
	yyjson_val *dep_tmpl = yyjson_obj_get(dep_root, "tmpl");
	char *version = (char *)yyjson_get_str(yyjson_obj_get(dep_root, "version"));

	// Vector *src_vec = vector_init(char *);
	// Vector *incl_vec = vector_init(char *);
	Vector *flag_vec = vector_init(char *);
	Vector *lib_link_vec = vector_init(char *);
	// Vector *stat_vec = vector_init(char *);
	// Vector *shared_vec = vector_init(char *);
	Vector *exclude_vec = vector_init(char *);
	Vector *exclude_dir_vec = vector_init(char *);
	Vector *exclude_exception_dir_vec = vector_init(char *);
	Cmap *tmpl_map = map_init();

	int idx = 0, max = 0;
	yyjson_val *val, *key;
	yyjson_mut_val *val_mut, *key_mut;

	yyjson_mut_arr_foreach(current_flags, idx, max, val_mut) {
		set_add(flag_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_flags, idx, max, val) {
		set_add(flag_vec, (char *)yyjson_get_str(val));
	}

	if (sync && yyjson_mut_is_arr(sync_flags)) {
		yyjson_mut_arr_foreach(sync_flags, idx, max, val_mut) {
			set_add(flag_vec, (char *)yyjson_mut_get_str(val_mut));
		}
	}

	idx = 0, max = 0;
	yyjson_mut_arr_foreach(current_lib_links, idx, max, val_mut) {
		set_add(lib_link_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_lib_links, idx, max, val) {
		set_add(lib_link_vec, (char *)yyjson_get_str(val));
	}
	if (sync && yyjson_mut_is_arr(sync_lib_links)) {
		yyjson_mut_arr_foreach(sync_lib_links, idx, max, val_mut) {
			set_add(lib_link_vec, (char *)yyjson_mut_get_str(val_mut));
		}
	}
	idx = 0, max = 0;
	yyjson_mut_obj_foreach(current_tmpl, idx, max, key_mut, val_mut) {
		// set_add(tmpl_vec, (char *)yyjson_mut_get_str(val_mut));
		// Vector *key_set = map_keys(tmpl_map);
		if (!map_get(tmpl_map, (char *)yyjson_mut_get_str(key_mut))) {
			map_add(tmpl_map, yyjson_mut_get_str(key_mut),
					(char *)yyjson_mut_get_str(val_mut));
		}
		// vector_free(key_set);
	}
	yyjson_obj_foreach(dep_tmpl, idx, max, key, val) {
		// set_add(tmpl_vec, (char *)yyjson_get_str(val));
		// Vector *key_set = map_keys(tmpl_map);
		if (!map_get(tmpl_map, (char *)yyjson_get_str(key))) {
			map_add(tmpl_map, yyjson_get_str(key), (char *)yyjson_get_str(val));
		}
		// vector_free(key_set);
	}
	if (sync && yyjson_mut_is_obj(sync_tmpl)) {
		yyjson_mut_obj_foreach(sync_tmpl, idx, max, key_mut, val_mut) {
			// set_add(tmpl_vec, (char *)yyjson_mut_get_str(val_mut));
			// Vector *key_set = map_keys(tmpl_map);
			if (!map_get(tmpl_map, (char *)yyjson_mut_get_str(key_mut))) {
				map_add(tmpl_map, yyjson_mut_get_str(key_mut),
						(char *)yyjson_mut_get_str(val_mut));
			}
			// vector_free(key_set);
		}
	}

	/*
	idx = 0, max = 0;
	yyjson_mut_arr_foreach(current_src, idx, max, val_mut) {
		set_add(src_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(src, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path;
			if (STR_CMP(yyjson_get_str(val), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_get_str(val)));
			}
			set_add(src_vec, src_path);
		} else {
			set_add(src_vec, (char *)yyjson_get_str(val));
		}
	}
	if (sync && yyjson_mut_is_arr(sync_src)) {
		yyjson_mut_arr_foreach(sync_src, idx, max, val_mut) {
			char *src_path;
			if (STR_CMP(yyjson_mut_get_str(val_mut), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_mut_get_str(val_mut)));
			}
			set_add(src_vec, src_path);
			// set_add(src_vec, (char *)yyjson_mut_get_str(val_mut));
		}
	}
	*/

	idx = 0, max = 0;

	yyjson_mut_arr_foreach(current_excludes, idx, max, val_mut) {
		set_add(exclude_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_excludes, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path;
			if (STR_CMP(yyjson_get_str(val), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_get_str(val)));
			}
			set_add(exclude_vec, src_path);
		} else {
			set_add(exclude_vec, (char *)yyjson_get_str(val));
		}
	}

	if (sync && yyjson_mut_is_arr(sync_excludes)) {
		yyjson_mut_arr_foreach(sync_excludes, idx, max, val_mut) {
			char *src_path;
			if (STR_CMP(yyjson_mut_get_str(val_mut), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_mut_get_str(val_mut)));
			}
			set_add(exclude_vec, src_path);
		}
	}

	idx = 0, max = 0;

	yyjson_mut_arr_foreach(current_exclude_dirs, idx, max, val_mut) {
		set_add(exclude_dir_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_exclude_dirs, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path;
			if (STR_CMP(yyjson_get_str(val), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_get_str(val)));
			}
			set_add(exclude_dir_vec, src_path);
		} else {
			set_add(exclude_dir_vec, (char *)yyjson_get_str(val));
		}
	}

	if (sync && yyjson_mut_is_arr(sync_exclude_dirs)) {
		yyjson_mut_arr_foreach(sync_exclude_dirs, idx, max, val_mut) {
			char *src_path;
			if (STR_CMP(yyjson_mut_get_str(val_mut), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_mut_get_str(val_mut)));
			}
			set_add(exclude_dir_vec, src_path);
		}
	}
	idx = 0, max = 0;

	yyjson_mut_arr_foreach(current_exclude_exception_dirs, idx, max, val_mut) {
		set_add(exclude_exception_dir_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_exclude_exception_dirs, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path;
			if (STR_CMP(yyjson_get_str(val), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_get_str(val)));
			}
			set_add(exclude_exception_dir_vec, src_path);
		} else {
			set_add(exclude_exception_dir_vec, (char *)yyjson_get_str(val));
		}
	}

	if (sync && yyjson_mut_is_arr(sync_exclude_exception_dirs)) {
		yyjson_mut_arr_foreach(sync_exclude_exception_dirs, idx, max, val_mut) {
			char *src_path;
			if (STR_CMP(yyjson_mut_get_str(val_mut), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_mut_get_str(val_mut)));
			}
			set_add(exclude_exception_dir_vec, src_path);
		}
	}

	/*
	idx = 0, max = 0;
	yyjson_mut_arr_foreach(current_incl, idx, max, val_mut) {
		set_add(incl_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(headers, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path;
			if (STR_CMP(yyjson_get_str(val), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_get_str(val)));
			}
			set_add(incl_vec, src_path);
		} else {
			set_add(incl_vec, (char *)yyjson_get_str(val));
		}
	}
	if (sync && yyjson_mut_is_arr(sync_include_paths)) {
		yyjson_mut_arr_foreach(sync_include_paths, idx, max, val_mut) {
			char *src_path;
			if (STR_CMP(yyjson_mut_get_str(val_mut), "") == 0) {
				src_path = string(string_concat_cstr(str_arena, 2, "deps/",
													 lib_details->repo_name));
			} else {
				src_path = string(string_concat_cstr(
					str_arena, 4, "deps/", lib_details->repo_name, "/",
					(char *)yyjson_mut_get_str(val_mut)));
			}
			set_add(incl_vec, src_path);
			// set_add(incl_vec, (char *)yyjson_mut_get_str(val_mut));
		}
	}
	*/

	/*
	idx = 0, max = 0;
	yyjson_mut_arr_foreach(current_stat_lib, idx, max, val_mut) {
		set_add(stat_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_stat_lib, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path = string(string_concat_cstr(
				str_arena, 4, "deps/", lib_details->repo_name, "/",
				(char *)yyjson_get_str(val)));
			set_add(stat_vec, src_path);
		} else {
			set_add(stat_vec, (char *)yyjson_get_str(val));
		}
	}
	if (sync && yyjson_mut_is_arr(sync_stat)) {
		yyjson_mut_arr_foreach(sync_stat, idx, max, val_mut) {
			char *src_path = string(string_concat_cstr(
				str_arena, 4, "deps/", lib_details->repo_name, "/",
				(char *)yyjson_mut_get_str(val_mut)));
			set_add(stat_vec, src_path);
			// set_add(incl_vec, (char *)yyjson_mut_get_str(val_mut));
		}
	}

	idx = 0, max = 0;
	yyjson_mut_arr_foreach(current_shared_lib, idx, max, val_mut) {
		set_add(shared_vec, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep_shared_lib, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path = string(string_concat_cstr(
				str_arena, 4, "deps/", lib_details->repo_name, "/",
				(char *)yyjson_get_str(val)));
			set_add(shared_vec, src_path);
		} else {
			set_add(shared_vec, (char *)yyjson_get_str(val));
		}
	}
	if (sync && yyjson_mut_is_arr(sync_shared)) {
		yyjson_mut_arr_foreach(sync_shared, idx, max, val_mut) {
			char *src_path = string(string_concat_cstr(
				str_arena, 4, "deps/", lib_details->repo_name, "/",
				(char *)yyjson_mut_get_str(val_mut)));
			set_add(shared_vec, src_path);
			// set_add(incl_vec, (char *)yyjson_mut_get_str(val_mut));
		}
	}
	*/

	if (current_flags != NULL) {
		yyjson_mut_arr_clear(current_flags);
		for (int i = 0; i < length(flag_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current_flags,
								   at(char *, flag_vec, i));
		}
	} else {
		yyjson_mut_val *temp_flag_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(flag_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_flag_arr,
								   at(char *, flag_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "flags",
							   temp_flag_arr);
	}
	if (current_lib_links != NULL) {
		yyjson_mut_arr_clear(current_lib_links);
		for (int i = 0; i < length(lib_link_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current_lib_links,
								   at(char *, lib_link_vec, i));
		}
	} else {
		yyjson_mut_val *temp_lib_link_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(lib_link_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_lib_link_arr,
								   at(char *, lib_link_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "lib_links",
							   temp_lib_link_arr);
	}

	Vector *keys = map_keys(tmpl_map);
	if (current_tmpl != NULL) {
		yyjson_mut_obj_clear(current_tmpl);
		for (int i = 0; i < length(keys); i++) {
			yyjson_mut_obj_add_str(
				current_mut_doc, current_tmpl, at(char *, keys, i),
				(char *)map_get(tmpl_map, at(char *, keys, i)));
		}
	} else {
		yyjson_mut_val *temp_tmpl_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(keys); i++) {
			yyjson_mut_obj_add_str(
				current_mut_doc, temp_tmpl_arr, at(char *, keys, i),
				(char *)map_get(tmpl_map, at(char *, keys, i)));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "tmpl",
							   temp_tmpl_arr);
	}
	// if (current_src != NULL) {
	/*
	yyjson_mut_arr_clear(current_src);
	for (int i = 0; i < length(src_vec); i++) {
		yyjson_mut_arr_add_str(current_mut_doc, current_src,
							   at(char *, src_vec, i));
	}
	// }
	yyjson_mut_arr_clear(current_incl);
	for (int i = 0; i < length(incl_vec); i++) {
		yyjson_mut_arr_add_str(current_mut_doc, current_incl,
							   at(char *, incl_vec, i));
	}
	*/
	/*
	if (current_stat_lib != NULL) {
		yyjson_mut_arr_clear(current_stat_lib);
		for (int i = 0; i < length(stat_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current_stat_lib,
								   at(char *, stat_vec, i));
		}
	} else {
		yyjson_mut_val *temp_stat_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(stat_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_stat_arr,
								   at(char *, stat_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "static_lib",
							   temp_stat_arr);
	}
	if (current_shared_lib != NULL) {
		yyjson_mut_arr_clear(current_shared_lib);
		for (int i = 0; i < length(shared_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current_shared_lib,
								   at(char *, shared_vec, i));
		}
	} else {
		yyjson_mut_val *temp_shared_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(shared_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_shared_arr,
								   at(char *, shared_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "shared_lib",
							   temp_shared_arr);
	}
	*/
	if (current_excludes != NULL) {
		yyjson_mut_arr_clear(current_excludes);
		for (int i = 0; i < length(exclude_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current_excludes,
								   at(char *, exclude_vec, i));
		}
	} else {
		yyjson_mut_val *temp_exclude_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(exclude_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_exclude_arr,
								   at(char *, exclude_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "excludes",
							   temp_exclude_arr);
	}
	if (current_exclude_dirs != NULL) {
		yyjson_mut_arr_clear(current_exclude_dirs);
		for (int i = 0; i < length(exclude_dir_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current_exclude_dirs,
								   at(char *, exclude_dir_vec, i));
		}
	} else {
		yyjson_mut_val *temp_exclude_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(exclude_dir_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_exclude_arr,
								   at(char *, exclude_dir_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "exclude_dirs",
							   temp_exclude_arr);
	}
	if (current_exclude_exception_dirs != NULL) {
		yyjson_mut_arr_clear(current_exclude_exception_dirs);
		for (int i = 0; i < length(exclude_exception_dir_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc,
								   current_exclude_exception_dirs,
								   at(char *, exclude_exception_dir_vec, i));
		}
	} else {
		yyjson_mut_val *temp_exclude_exception_arr =
			yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(exclude_exception_dir_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_exclude_exception_arr,
								   at(char *, exclude_exception_dir_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root,
							   "exclude_exception", temp_exclude_exception_arr);
	}

	/*
	if (!sync && !yyjson_mut_obj_get(dependencies, lib_details->repo_name)) {
		yyjson_mut_val *target_obj = yyjson_mut_obj(current_mut_doc);
		// yyjson_mut_obj_add_str(current_mut_doc, target_obj, "version",
		// 					   get_version_number(str_arena, libURL));

		yyjson_mut_obj_add_str(current_mut_doc, target_obj, "version",
							   lib_details->version);

		yyjson_mut_obj_add_str(current_mut_doc, target_obj, "remote",
							   get_modified_url(str_arena, libURL));

		yyjson_mut_obj_add_str(current_mut_doc, target_obj, "hash",
							   lib_details->hash);

		yyjson_mut_obj_add(
			dependencies,
			yyjson_mut_str(current_mut_doc, lib_details->repo_name),
			target_obj);
	}


	if (dependencies != NULL) {
		int d_idx = 0, d_max = 0;
		yyjson_mut_val *d_key, *d_val;
		yyjson_mut_obj_foreach(dependencies, d_idx, d_max, d_key, d_val) {
			if (yyjson_mut_is_obj(d_val)) {
				yyjson_mut_obj_remove_str(d_val, "flags");
				yyjson_mut_obj_remove_str(d_val, "lib_links");
				yyjson_mut_obj_remove_str(d_val, "src");
				yyjson_mut_obj_remove_str(d_val, "include_paths");
				yyjson_mut_obj_remove_str(d_val, "static_lib");
				yyjson_mut_obj_remove_str(d_val, "shared_lib");
			}
		}
	}
	*/

	if (dependencies != NULL && lib_details != NULL) {
		yyjson_mut_val *target_obj =
			yyjson_mut_obj_get(dependencies, lib_details->repo_name);

		if (!target_obj) {
			target_obj = yyjson_mut_obj(current_mut_doc);
			yyjson_mut_obj_add(
				dependencies,
				yyjson_mut_str(current_mut_doc, lib_details->repo_name),
				target_obj);
		}

		yyjson_mut_obj_remove_str(target_obj, "version");
		yyjson_mut_obj_remove_str(target_obj, "remote");
		yyjson_mut_obj_remove_str(target_obj, "hash");

		yyjson_mut_obj_add_str(current_mut_doc, target_obj, "version",
							   lib_details->version);
		yyjson_mut_obj_add_str(current_mut_doc, target_obj, "remote",
							   get_modified_url(str_arena, libURL));
		yyjson_mut_obj_add_str(current_mut_doc, target_obj, "hash",
							   lib_details->hash);

		// yyjson_mut_obj_put()

		int d_idx = 0, d_max = 0;
		yyjson_mut_val *d_key, *d_val;
		yyjson_mut_obj_foreach(dependencies, d_idx, d_max, d_key, d_val) {
			if (yyjson_mut_is_obj(d_val)) {
				yyjson_mut_obj_remove_str(d_val, "flags");
				yyjson_mut_obj_remove_str(d_val, "lib_links");
				yyjson_mut_obj_remove_str(d_val, "src");
				yyjson_mut_obj_remove_str(d_val, "include_paths");
				yyjson_mut_obj_remove_str(d_val, "static_lib");
				yyjson_mut_obj_remove_str(d_val, "shared_lib");
				yyjson_mut_obj_remove_str(d_val, "excludes");
				yyjson_mut_obj_remove_str(d_val, "exclude_dirs");
				yyjson_mut_obj_remove_str(d_val, "exclude_exception");
				yyjson_mut_obj_remove_str(d_val, "tmpl");
			}
		}
	}
	yyjson_write_err werr;
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
	if (!yyjson_mut_write_file("./composition.json", current_mut_doc, flg, NULL,
							   &werr)) {
		fprintf(stderr, "Write error: %s\n", werr.msg);
	}

	yyjson_val *dep_dependencies = yyjson_obj_get(dep_root, "dependencies");
	idx = 0, max = 0;
	yyjson_obj_foreach(dep_dependencies, idx, max, key, val) {
		yyjson_val *remote = yyjson_obj_get(val, "remote");
		yyjson_val *dep_version = yyjson_obj_get(val, "version");
		yyjson_val *dep_hash = yyjson_obj_get(val, "hash");
		char *hash = "";
		if (dep_hash) {
			hash = (char *)yyjson_get_str(dep_hash);
		}
		if (!set_contains(v, (char *)yyjson_get_str(remote))) {
			set_add(v, (char *)yyjson_get_str(remote));
			char *modified_url = string(
				string_concat_cstr(str_arena, 3, (char *)yyjson_get_str(remote),
								   "@", (char *)yyjson_get_str(dep_version)));

			// fetch_library(v, modified_url, NULL, NULL, NULL, NULL, NULL,
			// NULL, 			  false, hash, NULL);
			fetch_library(v, modified_url, /*NULL, NULL, NULL, NULL, */ NULL,
						  NULL, hash, NULL, NULL, NULL, NULL, false);
		}
	}
	generate_compile_commands();
	vector_free(flag_vec);
	vector_free(lib_link_vec);
	// vector_free(src_vec);
	// vector_free(incl_vec);
	// vector_free(stat_vec);
	// vector_free(shared_vec);
	vector_free(exclude_vec);
	vector_free(exclude_dir_vec);
	vector_free(exclude_exception_dir_vec);
	vector_free(keys);
	map_free(tmpl_map);
	yyjson_mut_doc_free(current_mut_doc);
	yyjson_doc_free(current_doc);
	yyjson_doc_free(dep_doc);
	arena_free(&str_arena);
	return;
}

void remove_library_partial(char *libURL) {
	Arena *arena = arena_init(1024);

	char *repo_name = get_repo_name(arena, libURL);
	// char *url = get_modified_url(arena, libURL);

	yyjson_read_err err;
	yyjson_doc *package = yyjson_read_file("./deps/.package", 0, NULL, &err);
	yyjson_mut_doc *package_mut = yyjson_doc_mut_copy(package, NULL);
	yyjson_doc_free(package);
	yyjson_mut_val *root = yyjson_mut_doc_get_root(package_mut);
	// yyjson_mut_val *package_arr = yyjson_mut_arr(package_mut);
	yyjson_mut_val *package_arr = yyjson_mut_obj_get(root, "packages");
	Vector *set = vector_init(char *);

	yyjson_mut_val *val;
	size_t idx, max;

	yyjson_mut_arr_foreach(package_arr, idx, max, val) {
		if (STR_CMP(yyjson_mut_get_str(val), libURL) != 0) {
			set_add(set, (char *)yyjson_mut_get_str(val));
		}
	}

	yyjson_mut_arr_clear(package_arr);

	for (int i = 0; i < length(set); i++) {
		char *item = at(char *, set, i);
		yyjson_mut_arr_add_str(package_mut, package_arr, item);
	}

	/*
	yyjson_write_err werr;
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
	if (!yyjson_mut_write_file("./deps/.package", package_mut, flg, NULL,
							   &werr)) {
		fprintf(stderr, "Write error: %s\n", werr.msg);
	}
	*/
	update_package_file(package_mut);

	remove_directory(
		arena, string(string_concat_cstr(arena, 2, "./deps/", repo_name)));

	yyjson_mut_doc_free(package_mut);
	arena_free(&arena);
}

void remove_library(char *repo_name) {
	Arena *local_arena = arena_init(1024);
	yyjson_read_err err;
	yyjson_doc *config = yyjson_read_file("./composition.json", 0, NULL, &err);
	yyjson_mut_doc *config_mut = yyjson_doc_mut_copy(config, NULL);
	yyjson_doc_free(config);
	yyjson_mut_val *root = yyjson_mut_doc_get_root(config_mut);

	yyjson_mut_val *dependencies = yyjson_mut_obj_get(root, "dependencies");

	yyjson_mut_val *element = yyjson_mut_obj_get(dependencies, repo_name);

	char *repo_url = NULL;

	if (element && yyjson_mut_is_obj(element)) {
		repo_url =
			(char *)yyjson_mut_get_str(yyjson_mut_obj_get(element, "remote"));
	} else {
		printf("[x] No dependency named '%s' found\n", repo_name);
		return;
	}

	printf("[+] Removing '%s'...\n", repo_name);

	remove_library_partial(repo_url);

	yyjson_mut_obj_remove_key(dependencies, repo_name);

	yyjson_mut_val *include_arr = yyjson_mut_obj_get(root, "include_paths");
	yyjson_mut_val *src_arr = yyjson_mut_obj_get(root, "src");
	yyjson_mut_val *static_lib_arr = yyjson_mut_obj_get(root, "static_lib");
	yyjson_mut_val *shared_lib_arr = yyjson_mut_obj_get(root, "shared_lib");
	yyjson_mut_val *excludes_arr = yyjson_mut_obj_get(root, "excludes");
	yyjson_mut_val *exclude_dir_arr = yyjson_mut_obj_get(root, "exclude_dirs");

	char *search_str =
		string(string_concat_cstr(local_arena, 2, "deps/", repo_name));

	remove_arr_entry(include_arr, search_str);
	remove_arr_entry(src_arr, search_str);
	remove_arr_entry(static_lib_arr, search_str);
	remove_arr_entry(shared_lib_arr, search_str);
	remove_arr_entry(excludes_arr, search_str);
	remove_arr_entry(exclude_dir_arr, search_str);

	yyjson_write_err werr;
	yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
	if (!yyjson_mut_write_file("./composition.json", config_mut, flg, NULL,
							   &werr)) {
		fprintf(stderr, "Write error: %s\n", werr.msg);
	}

	yyjson_mut_doc_free(config_mut);
	arena_free(&local_arena);
	printf("[✓] Done!\n");
}

void list_deps() {
	yyjson_read_err err;
	yyjson_doc *config = yyjson_read_file("./composition.json", 0, NULL, &err);
	if (!config) {
		fprintf(stderr, "Failed to read composition.json: %s\n", err.msg);
		return;
	}
	yyjson_val *root = yyjson_doc_get_root(config);
	yyjson_val *dependencies = yyjson_obj_get(root, "dependencies");

	if (!dependencies || yyjson_obj_size(dependencies) == 0) {
		printf("[x] No dependencies found\n");
		yyjson_doc_free(config);
		return;
	}

	int idx = 0, max = 0;
	yyjson_val *key, *val;
	yyjson_obj_foreach(dependencies, idx, max, key, val) {
		char *version = (char *)yyjson_get_str(yyjson_obj_get(val, "version"));
		printf("[*] %s: %s\n", yyjson_get_str(key), version);
	}

	yyjson_doc_free(config);
}
