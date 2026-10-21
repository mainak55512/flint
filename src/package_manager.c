#include <flint.h>

Vector *remove_arr_entry(Arena *arena, Vector *arr, char *search_str) {
	if (!arr)
		return NULL;
	if (!length(arr) || !search_str)
		return arr;

	Vector *new_vec = vector_init(String *);
	for (size_t i = 0; i < length(arr); i++) {
		String *str = at(String *, arr, i);

		if (str && !starts_with(string(str), search_str)) {
			append(String *, new_vec, string_clone(arena, str));
		}
	}
	vector_free(&arr);
	return new_vec;
}

Dependency *clone_lib_hashed(Arena *arena, char *libURL, const char *ref_hash) {
	Dependency *lib_details =
		(Dependency *)arena_alloc(arena, sizeof(Dependency));
	char *repo_name = get_repo_name(arena, libURL);
	char *target_dir =
		string(string_concat_cstr(arena, 2, "./deps/", repo_name));

	if (!directory_exists("./deps")) {
		if (MAKE_DIR("./deps") != 0 && errno != EEXIST) {
			return NULL;
		}
	}
	if (!directory_exists(target_dir)) {
		if (MAKE_DIR(target_dir) != 0 && errno != EEXIST) {
			return NULL;
		}
	}

	// #if defined(_WIN32)
	// 	const char *sink_path = ">NUL 2>&1";
	// #else
	// 	const char *sink_path = ">/dev/null 2>&1";
	// #endif

	char *command = string(string_concat_cstr(
		arena, 20, "git -C \"", target_dir, "\" init ", NULL_DEVICE,
		" && git -C \"", target_dir, "\" remote add origin \"", libURL, "\" ",
		NULL_DEVICE, " && git -C \"", target_dir,
		"\" fetch --depth 1 --tags origin ", ref_hash, " ", NULL_DEVICE,
		" && git -C \"", target_dir, "\" checkout FETCH_HEAD ", NULL_DEVICE));

	if (system(command) != 0) {
		if (directory_exists(target_dir)) {
			remove_directory(arena, target_dir);
		}
		return NULL;
	}

	char *tag = get_tag_from_hash(arena, target_dir, ref_hash);

	lib_details->repo_name = repo_name;
	lib_details->version = string_from(arena, tag);
	lib_details->hash = string_from(arena, (char *)ref_hash);
	lib_details->remote = string_from(arena, libURL);

	printf("[*] Library: %s\n", lib_details->repo_name);
	printf("[*] Version: %s\n", string(lib_details->version));
	printf("[*] Hash: %s\n", string(lib_details->hash));
	printf("[✓] Done!\n\n");
	remove_directory(arena,
					 string(string_concat_cstr(arena, 2, target_dir, "/.git")));
	return lib_details;
}

Dependency *clone_lib(Arena *arena, char *libURL, const char *hash) {
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

	// #if defined(_WIN32)
	// 	const char *sink_path = ">NUL 2>&1";
	// #else
	// 	const char *sink_path = ">/dev/null 2>&1";
	// #endif

	printf("[+] Installing %s...\n", repo_name);
	String *command;

	if (!hash || STR_CMP(hash, "") == 0) {
		if (!directory_exists("./deps")) {
			if (MAKE_DIR("./deps") != 0 && errno != EEXIST) {
				return NULL;
			}
		}
		if (STR_CMP(version_number, "unknown") == 0) {
			command = string_concat_cstr(
				arena, 7, "git clone --depth 1 --quiet \"", url, "\" \"",
				target_dir, "\" ", NULL_DEVICE, "");
		} else {
			command = string_concat_cstr(
				arena, 8, "git clone --depth 1 --quiet --branch \"",
				version_number, "\" \"", url, "\" \"", target_dir, "\" ",
				NULL_DEVICE);
		}
	} else {
		return clone_lib_hashed(arena, url, hash);
	}

	if (system(string(command)) != 0) {
		if (directory_exists(target_dir)) {
			remove_directory(arena, target_dir);
		}
		return NULL;
	}

	char *ref_hash = get_lib_hash(arena, target_dir);
	char *fetched_version = get_tag_from_hash(arena, target_dir, ref_hash);
	Dependency *lib_details =
		(Dependency *)arena_alloc(arena, sizeof(Dependency));
	lib_details->repo_name = repo_name;
	lib_details->version = string_from(arena, fetched_version);
	lib_details->hash = string_from(arena, ref_hash);
	lib_details->remote = string_from(arena, url);

	printf("[*] Library: %s\n", lib_details->repo_name);
	printf("[*] Version: %s\n", string(lib_details->version));
	printf("[*] Hash: %s\n", string(lib_details->hash));
	printf("[✓] Done!\n\n");

	remove_directory(arena,
					 string(string_concat_cstr(arena, 2, target_dir, "/.git")));
	return lib_details;
}

void sync_flags(Config *conf, Vector *flags) {
	for (int j = 0; j < length(flags); j++) {
		set_add_str(conf->flags,
					string_clone(conf->arena, at(String *, flags, j)));
	}
}
void sync_lib_links(Config *conf, Vector *lib_links) {
	for (int j = 0; j < length(lib_links); j++) {
		set_add_str(conf->lib_links,
					string_clone(conf->arena, at(String *, lib_links, j)));
	}
}
void sync_excludes(Config *conf, Vector *excludes, char *repo_name) {
	for (int j = 0; j < length(excludes); j++) {

		char *raw_str = string(at(String *, excludes, j));
		char *colon = strchr(raw_str, ':');

		if (colon) {
			size_t prefix_len = (size_t)(colon - raw_str) + 1;
			char *prefix = arena_alloc(conf->arena, prefix_len + 1);
			memcpy(prefix, raw_str, prefix_len);
			prefix[prefix_len] = '\0';

			const char *path = colon + 1;

			set_add_str(conf->excludes,
						string_concat_cstr(conf->arena, 5, prefix, "deps/",
										   repo_name, "/", path));

		} else {
			set_add_str(conf->excludes,
						string_concat_cstr(conf->arena, 4, "deps/", repo_name,
										   "/", raw_str));
		}
	}
}
void sync_exclude_dirs(Config *conf, Vector *exclude_dirs, char *repo_name) {
	for (int j = 0; j < length(exclude_dirs); j++) {

		char *raw_str = string(at(String *, exclude_dirs, j));
		char *colon = strchr(raw_str, ':');

		if (colon) {
			size_t prefix_len = (size_t)(colon - raw_str) + 1;
			char *prefix = arena_alloc(conf->arena, prefix_len + 1);
			memcpy(prefix, raw_str, prefix_len);
			prefix[prefix_len] = '\0';

			const char *path = colon + 1;

			set_add_str(conf->exclude_dirs,
						string_concat_cstr(conf->arena, 5, prefix, "deps/",
										   repo_name, "/", path));

		} else {
			set_add_str(conf->exclude_dirs,
						string_concat_cstr(conf->arena, 4, "deps/", repo_name,
										   "/", raw_str));
		}
	}
}
void sync_exclude_exception(Config *conf, Vector *exclude_exception,
							char *repo_name) {
	for (int j = 0; j < length(exclude_exception); j++) {
		char *raw_str = string(at(String *, exclude_exception, j));
		char *colon = strchr(raw_str, ':');

		if (colon) {
			size_t prefix_len = (size_t)(colon - raw_str) + 1;
			char *prefix = arena_alloc(conf->arena, prefix_len + 1);
			memcpy(prefix, raw_str, prefix_len);
			prefix[prefix_len] = '\0';

			const char *path = colon + 1;

			set_add_str(conf->exclude_exception,
						string_concat_cstr(conf->arena, 5, prefix, "deps/",
										   repo_name, "/", path));

		} else {
			set_add_str(conf->exclude_exception,
						string_concat_cstr(conf->arena, 4, "deps/", repo_name,
										   "/", raw_str));
		}
	}
}
void sync_tmpl(Config *conf, Cmap *tmpl) {
	if (tmpl) {
		Vector *tmpl_keys = map_keys(tmpl);
		for (int j = 0; j < length(tmpl_keys); j++) {
			map_add(
				conf->tmpl, at(char *, tmpl_keys, j),
				(String *)map_get(
					tmpl, arena_strdup(conf->arena, at(char *, tmpl_keys, j))));
		}
		vector_free(&tmpl_keys);
	}
}

void update_dependency_map(Config *conf, Dependency *lib) {
	map_add(conf->dependencies, lib->repo_name, lib);
}
void update_lock(Cmap *lock, Dependency *lib) {
	map_add(lock, lib->repo_name, lib);
}

void fetch_library(Config *current_config, char *libURL, char *hash, Cmap *lock,
				   bool sync) {
	Dependency *lib_details = clone_lib(current_config->arena, libURL, hash);

	if (lib_details == NULL) {
		return;
	}
	Dependency *entry = arena_alloc(current_config->arena, sizeof(Dependency));
	entry->repo_name = lib_details->repo_name;
	entry->remote = lib_details->remote;
	entry->version = lib_details->version;
	entry->hash = lib_details->hash;

	update_lock(lock, entry);
	update_dependency_map(current_config, entry);

	String *config_path =
		string_concat_cstr(current_config->arena, 3, "./deps/",
						   lib_details->repo_name, "/composition.json");
	if (!sync && !is_mybuild_config_present(string(config_path))) {
		return;
	}

	if (is_mybuild_config_present(string(config_path))) {
		Config *dep_config = config_init();
		read_flint_composition(string(config_path), dep_config);

		sync_flags(current_config, dep_config->flags);
		sync_lib_links(current_config, dep_config->lib_links);

		sync_tmpl(current_config, dep_config->tmpl);
		sync_excludes(current_config, dep_config->excludes,
					  lib_details->repo_name);
		sync_exclude_dirs(current_config, dep_config->exclude_dirs,
						  lib_details->repo_name);
		sync_exclude_exception(current_config, dep_config->exclude_exception,
							   lib_details->repo_name);

		if (map_len(dep_config->dependencies)) {
			Vector *dep_dependencies = map_keys(dep_config->dependencies);
			for (int i = 0; i < length(dep_dependencies); i++) {
				Dependency *dep_fetch = (Dependency *)map_get(
					dep_config->dependencies, at(char *, dep_dependencies, i));
				if (!lock_contains(lock, string(dep_fetch->remote))) {
					char *modified_url = string(string_concat_cstr(
						current_config->arena, 3, string(dep_fetch->remote),
						"@", string(dep_fetch->version)));
					char *hash = "";
					if (dep_fetch->hash) {
						hash = string(dep_fetch->hash);
					}
					fetch_library(current_config, modified_url, hash, lock,
								  false);
				}
			}

			vector_free(&dep_dependencies);
		}
		config_free(dep_config);
	}
}

void add_lib(char *libURL) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	Cmap *lock = read_lock_file();

	if (!lock_contains(lock, get_modified_url(conf->arena, libURL))) {
		fetch_library(conf, libURL, "", lock, false);
	} else {
		printf("[*] Dependency already available, installation skipped...\n");
	}

	write_flint_composition("composition.json", conf);
	write_lock_file(lock);
	generate_compile_commands(conf);
	config_free(conf);
}

void sync_lib(char *elem) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);
	Vector *sync_keys = map_keys(conf->sync);
	Vector *dep_keys = map_keys(conf->dependencies);

	if (elem && STR_CMP(elem, "all") == 0) {
		remove("flint.lock");
		remove_directory(conf->arena, "deps");
	}

	Cmap *lock = read_lock_file();

	if (length(sync_keys)) {
		for (int i = 0; i < length(sync_keys); i++) {
			char *repo_name = at(char *, sync_keys, i);
			Sync_config *dependency =
				(Sync_config *)map_get(conf->sync, at(char *, sync_keys, i));
			if (!lock_contains(lock, string(dependency->remote))) {
				char *modified_url = string(string_concat_cstr(
					conf->arena, 3, string(dependency->remote), "@",
					string(dependency->version)));
				char *hash = "";
				if (dependency->hash) {
					hash = string(dependency->hash);
				}
				fetch_library(conf, modified_url, hash, lock, true);

				sync_flags(conf, dependency->flags);
				sync_lib_links(conf, dependency->lib_links);
				sync_excludes(conf, dependency->excludes, repo_name);
				sync_exclude_dirs(conf, dependency->exclude_dirs, repo_name);
				sync_exclude_exception(conf, dependency->exclude_exception,
									   repo_name);
				sync_tmpl(conf, dependency->tmpl);
			} else {
				printf(
					"[*] Library '%s' already installed, hence skipping...\n",
					repo_name);
			}
		}
	}

	if (length(dep_keys)) {
		for (int i = 0; i < length(dep_keys); i++) {
			char *repo_name = at(char *, dep_keys, i);
			Dependency *dependency = (Dependency *)map_get(
				conf->dependencies, at(char *, dep_keys, i));
			if (!lock_contains(lock, string(dependency->remote))) {
				char *modified_url = string(string_concat_cstr(
					conf->arena, 3, string(dependency->remote), "@",
					string(dependency->version)));
				char *hash = "";
				if (dependency->hash) {
					hash = string(dependency->hash);
				}
				fetch_library(conf, modified_url, hash, lock, true);
			}
		}
	}

	write_flint_composition("composition.json", conf);
	write_lock_file(lock);
	vector_free(&sync_keys);
	vector_free(&dep_keys);
	generate_compile_commands(conf);
	config_free(conf);
}

void remove_library_partial(char *repo_name) {
	Cmap *lock = read_lock_file();

	Arena *arena = arena_init(1024);
	remove_directory(
		arena, string(string_concat_cstr(arena, 2, "./deps/", repo_name)));
	arena_free(&arena);

	Cmap *new_lock = map_init();

	Vector *dep_list = map_keys(lock);

	for (int i = 0; i < length(dep_list); i++) {
		char *key = at(char *, dep_list, i);
		if (STR_CMP(key, repo_name) != 0) {
			Dependency *old_dep = (Dependency *)map_get(lock, key);
			Dependency *new_dep =
				arena_alloc(new_lock->arena, sizeof(Dependency));
			new_dep->version = string_clone(new_lock->arena, old_dep->version);
			new_dep->remote = string_clone(new_lock->arena, old_dep->remote);
			new_dep->hash = string_clone(new_lock->arena, old_dep->hash);
			map_add(new_lock, at(char *, dep_list, i), new_dep);
		}
	}

	write_lock_file(new_lock);
	vector_free(&dep_list);
	map_free(lock);
}

void remove_library(char *repo_name) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	Dependency *element = (Dependency *)map_get(conf->dependencies, repo_name);

	if (!element) {
		config_free(conf);
		printf("[x] No dependency named '%s' found\n", repo_name);
		return;
	}

	printf("[+] Removing '%s'...\n", repo_name);

	remove_library_partial(repo_name);

	char *search_str =
		string(string_concat_cstr(conf->arena, 2, "deps/", repo_name));

	conf->excludes = remove_arr_entry(conf->arena, conf->excludes, search_str);
	conf->exclude_dirs =
		remove_arr_entry(conf->arena, conf->exclude_dirs, search_str);
	conf->exclude_exception =
		remove_arr_entry(conf->arena, conf->exclude_exception, search_str);

	Vector *dep_list = map_keys(conf->dependencies);
	Cmap *new_dep_map = map_init();

	for (int i = 0; i < length(dep_list); i++) {
		char *key = at(char *, dep_list, i);
		if (STR_CMP(key, repo_name) != 0) {
			Dependency *old_dep =
				(Dependency *)map_get(conf->dependencies, key);
			Dependency *new_dep = arena_alloc(conf->arena, sizeof(Dependency));
			new_dep->version = string_clone(conf->arena, old_dep->version);
			new_dep->remote = string_clone(conf->arena, old_dep->remote);
			new_dep->hash = string_clone(conf->arena, old_dep->hash);
			map_add(new_dep_map, at(char *, dep_list, i), new_dep);
		}
	}

	map_free(conf->dependencies);
	conf->dependencies = new_dep_map;

	write_flint_composition("composition.json", conf);

	vector_free(&dep_list);
	generate_compile_commands(conf);
	config_free(conf);
	printf("[✓] Done!\n");
}

void list_deps() {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	Vector *dep_list = map_keys(conf->dependencies);
	for (int i = 0; i < length(dep_list); i++) {
		char *key = at(char *, dep_list, i);
		Dependency *dep = (Dependency *)map_get(conf->dependencies, key);
		printf("[*] %s: %s\n", key, string(dep->version));
	}
	vector_free(&dep_list);

	config_free(conf);
}
