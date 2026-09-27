#include "yyjson.h"
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

			yyjson_mut_val *flags = yyjson_mut_obj_get(dep_obj, "flags");
			yyjson_mut_val *lib_links =
				yyjson_mut_obj_get(dep_obj, "lib_links");
			yyjson_mut_val *tmpl = yyjson_mut_obj_get(dep_obj, "tmpl");
			yyjson_mut_val *excludes = yyjson_mut_obj_get(dep_obj, "excludes");
			yyjson_mut_val *exclude_dirs =
				yyjson_mut_obj_get(dep_obj, "exclude_dirs");
			yyjson_mut_val *exclude_exception_dirs =
				yyjson_mut_obj_get(dep_obj, "exclude_exception");
			if (!set_contains(installed,
							  (char *)yyjson_mut_get_str(dep_remote))) {

				set_add(installed, (char *)yyjson_mut_get_str(dep_remote));

				char *modified_url = string(string_concat_cstr(
					local_arena, 3, (char *)yyjson_mut_get_str(dep_remote), "@",
					(char *)yyjson_mut_get_str(dep_version)));
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
		fetch_library(set, libURL, /*NULL, NULL, NULL, NULL, */ NULL, NULL, "",
					  NULL, NULL, NULL, NULL, false);
	}
	yyjson_doc *package = yyjson_read_file("./deps/.package", 0, NULL, &err);
	yyjson_mut_doc *package_mut = yyjson_doc_mut_copy(package, NULL);
	yyjson_doc_free(package);
	yyjson_mut_val *root = yyjson_mut_doc_get_root(package_mut);
	yyjson_mut_val *package_arr = yyjson_mut_arr(package_mut);

	for (int i = 0; i < length(set); i++) {
		yyjson_mut_val *val = yyjson_mut_str(package_mut, at(char *, set, i));
		yyjson_mut_arr_append(package_arr, val);
	}
	yyjson_mut_obj_put(root, yyjson_mut_str(package_mut, "packages"),
					   package_arr);

	generate_compile_commands();
	update_package_file(package_mut);
	yyjson_mut_doc_free(package_mut);
	yyjson_doc_free(current_doc);
	vector_free(set);
	arena_free(&local_arena);
}

LibDetails *clone_lib(Arena *arena, char *libURL, const char *hash) {
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

void collect_resource_arr(Vector *collect_arr, yyjson_mut_val *current,
						  yyjson_val *dep, yyjson_mut_val *sync_elems,
						  bool sync) {

	int idx = 0, max = 0;
	yyjson_val *val, *key;
	yyjson_mut_val *val_mut, *key_mut;

	yyjson_mut_arr_foreach(current, idx, max, val_mut) {
		set_add(collect_arr, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep, idx, max, val) {
		set_add(collect_arr, (char *)yyjson_get_str(val));
	}

	if (sync && yyjson_mut_is_arr(sync_elems)) {
		yyjson_mut_arr_foreach(sync_elems, idx, max, val_mut) {
			set_add(collect_arr, (char *)yyjson_mut_get_str(val_mut));
		}
	}
}

void collect_path_arr(Arena *str_arena, char *repo_name, Vector *collect_arr,
					  yyjson_mut_val *current, yyjson_val *dep,
					  yyjson_mut_val *sync_elems, bool sync) {

	int idx = 0, max = 0;
	yyjson_val *val, *key;
	yyjson_mut_val *val_mut, *key_mut;

	yyjson_mut_arr_foreach(current, idx, max, val_mut) {
		set_add(collect_arr, (char *)yyjson_mut_get_str(val_mut));
	}
	yyjson_arr_foreach(dep, idx, max, val) {
		if (!check_if_dep_path((char *)yyjson_get_str(val))) {
			char *src_path;
			if (STR_CMP(yyjson_get_str(val), "") == 0) {
				src_path = string(
					string_concat_cstr(str_arena, 2, "deps/", repo_name));
			} else {
				src_path = string(
					string_concat_cstr(str_arena, 4, "deps/", repo_name, "/",
									   (char *)yyjson_get_str(val)));
			}
			set_add(collect_arr, src_path);
		} else {
			set_add(collect_arr, (char *)yyjson_get_str(val));
		}
	}

	if (sync && yyjson_mut_is_arr(sync_elems)) {
		yyjson_mut_arr_foreach(sync_elems, idx, max, val_mut) {
			char *src_path;
			if (STR_CMP(yyjson_mut_get_str(val_mut), "") == 0) {
				src_path = string(
					string_concat_cstr(str_arena, 2, "deps/", repo_name));
			} else {
				src_path = string(
					string_concat_cstr(str_arena, 4, "deps/", repo_name, "/",
									   (char *)yyjson_mut_get_str(val_mut)));
			}
			set_add(collect_arr, src_path);
		}
	}
}

void update_doc_arr_json(yyjson_mut_doc *current_mut_doc,
						 yyjson_mut_val *current_root, yyjson_mut_val *current,
						 Vector *collect_vec, const char *elem) {
	if (current != NULL) {
		yyjson_mut_arr_clear(current);
		for (int i = 0; i < length(collect_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, current,
								   at(char *, collect_vec, i));
		}
	} else {
		yyjson_mut_val *temp_flag_arr = yyjson_mut_arr(current_mut_doc);
		for (int i = 0; i < length(collect_vec); i++) {
			yyjson_mut_arr_add_str(current_mut_doc, temp_flag_arr,
								   at(char *, collect_vec, i));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, elem,
							   temp_flag_arr);
	}
}

void fetch_library(Vector *v, char *libURL, yyjson_mut_val *sync_flags,
				   yyjson_mut_val *sync_lib_links, const char *hash,
				   yyjson_mut_val *sync_excludes,
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

	yyjson_doc *dep_doc =
		yyjson_read_file(string(dep_mybuild_path), 0, NULL, &err);

	yyjson_val *dep_root = yyjson_doc_get_root(dep_doc);

	yyjson_val *dep_flags = yyjson_obj_get(dep_root, "flags");
	yyjson_val *dep_lib_links = yyjson_obj_get(dep_root, "lib_links");
	yyjson_mut_val *current_flags = yyjson_mut_obj_get(current_root, "flags");
	yyjson_mut_val *current_lib_links =
		yyjson_mut_obj_get(current_root, "lib_links");
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

	Vector *flag_vec = vector_init(char *);
	Vector *lib_link_vec = vector_init(char *);
	Vector *exclude_vec = vector_init(char *);
	Vector *exclude_dir_vec = vector_init(char *);
	Vector *exclude_exception_dir_vec = vector_init(char *);
	Cmap *tmpl_map = map_init();

	int idx = 0, max = 0;
	yyjson_val *val, *key;
	yyjson_mut_val *val_mut, *key_mut;

	collect_resource_arr(flag_vec, current_flags, dep_flags, sync_flags, sync);

	collect_resource_arr(lib_link_vec, current_lib_links, dep_lib_links,
						 sync_lib_links, sync);

	idx = 0, max = 0;
	yyjson_mut_obj_foreach(current_tmpl, idx, max, key_mut, val_mut) {
		if (!map_get(tmpl_map, (char *)yyjson_mut_get_str(key_mut))) {
			map_add(tmpl_map, yyjson_mut_get_str(key_mut),
					(char *)yyjson_mut_get_str(val_mut));
		}
	}
	yyjson_obj_foreach(dep_tmpl, idx, max, key, val) {
		if (!map_get(tmpl_map, (char *)yyjson_get_str(key))) {
			map_add(tmpl_map, yyjson_get_str(key), (char *)yyjson_get_str(val));
		}
	}
	if (sync && yyjson_mut_is_obj(sync_tmpl)) {
		yyjson_mut_obj_foreach(sync_tmpl, idx, max, key_mut, val_mut) {
			if (!map_get(tmpl_map, (char *)yyjson_mut_get_str(key_mut))) {
				map_add(tmpl_map, yyjson_mut_get_str(key_mut),
						(char *)yyjson_mut_get_str(val_mut));
			}
		}
	}

	collect_path_arr(str_arena, lib_details->repo_name, exclude_vec,
					 current_excludes, dep_excludes, sync_excludes, sync);

	collect_path_arr(str_arena, lib_details->repo_name, exclude_dir_vec,
					 current_exclude_dirs, dep_exclude_dirs, sync_exclude_dirs,
					 sync);

	collect_path_arr(str_arena, lib_details->repo_name,
					 exclude_exception_dir_vec, current_exclude_exception_dirs,
					 dep_exclude_exception_dirs, sync_exclude_exception_dirs,
					 sync);

	update_doc_arr_json(current_mut_doc, current_root, current_flags, flag_vec,
						"flags");

	update_doc_arr_json(current_mut_doc, current_root, current_lib_links,
						lib_link_vec, "lib_links");

	Vector *keys = map_keys(tmpl_map);
	if (current_tmpl != NULL) {
		yyjson_mut_obj_clear(current_tmpl);
		for (int i = 0; i < length(keys); i++) {
			yyjson_mut_obj_add_str(
				current_mut_doc, current_tmpl, at(char *, keys, i),
				(char *)map_get(tmpl_map, at(char *, keys, i)));
		}
	} else {
		yyjson_mut_val *temp_tmpl_arr = yyjson_mut_obj(current_mut_doc);
		for (int i = 0; i < length(keys); i++) {
			yyjson_mut_obj_add_str(
				current_mut_doc, temp_tmpl_arr, at(char *, keys, i),
				(char *)map_get(tmpl_map, at(char *, keys, i)));
		}
		yyjson_mut_obj_add_val(current_mut_doc, current_root, "tmpl",
							   temp_tmpl_arr);
	}

	update_doc_arr_json(current_mut_doc, current_root, current_excludes,
						exclude_vec, "excludes");

	update_doc_arr_json(current_mut_doc, current_root, current_exclude_dirs,
						exclude_dir_vec, "exclude_dirs");

	update_doc_arr_json(current_mut_doc, current_root,
						current_exclude_exception_dirs,
						exclude_exception_dir_vec, "exclude_exception");

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

			fetch_library(v, modified_url, /*NULL, NULL, NULL, NULL, */ NULL,
						  NULL, hash, NULL, NULL, NULL, NULL, false);
		}
	}
	generate_compile_commands();
	vector_free(flag_vec);
	vector_free(lib_link_vec);
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

	yyjson_read_err err;
	yyjson_doc *package = yyjson_read_file("./deps/.package", 0, NULL, &err);
	yyjson_mut_doc *package_mut = yyjson_doc_mut_copy(package, NULL);
	yyjson_doc_free(package);
	yyjson_mut_val *root = yyjson_mut_doc_get_root(package_mut);
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
	yyjson_mut_val *exclude_exception_dir_arr =
		yyjson_mut_obj_get(root, "exclude_exception");

	char *search_str =
		string(string_concat_cstr(local_arena, 2, "deps/", repo_name));

	remove_arr_entry(include_arr, search_str);
	remove_arr_entry(src_arr, search_str);
	remove_arr_entry(static_lib_arr, search_str);
	remove_arr_entry(shared_lib_arr, search_str);
	remove_arr_entry(excludes_arr, search_str);
	remove_arr_entry(exclude_dir_arr, search_str);
	remove_arr_entry(exclude_exception_dir_arr, search_str);

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
