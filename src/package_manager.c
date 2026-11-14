#include <flint.h>

Vector(String *)
	remove_arr_entry(Arena *arena, Vector(String *) arr, char *search_str) {
	if (!arr)
		return NULL;
	if (!length(arr) || !search_str)
		return arr;

	Vector(String *) new_vec = vector_init(arena);
	for (size_t i = 0; i < (size_t)length(arr); i++) {
		String *str = at(arr, i);

		if (str && !string_contains(str, search_str)) {
			append(new_vec, str);
		}
	}
	// vector_free(&arr);
	return new_vec;
}

Dependency *clone_lib_hashed(Arena *arena, char *libURL, const char *ref_hash) {
	if (!directory_exists("./deps")) {
		if (MAKE_DIR("./deps") != 0 && errno != EEXIST) {
			return NULL;
		}
	}

	ArenaTemp temp = arena_temp_begin(arena);
	char *temp_repo = get_repo_name(arena, libURL);
	char *target_dir = string(string_format(arena, "./deps/%s", temp_repo));

	if (!directory_exists(target_dir)) {
		if (MAKE_DIR(target_dir) != 0 && errno != EEXIST) {
			arena_temp_end(temp);
			return NULL;
		}
	}

#if defined(_WIN32)
	const char *sink_path = ">NUL 2>&1";
#else
	const char *sink_path = ">/dev/null 2>&1";
#endif

	char *command = string(
		string_format(arena,
					  "git -C \"%s\" init %s && "
					  "git -C \"%s\" remote add origin \"%s\" %s && "
					  "git -C \"%s\" fetch --depth 1 --tags origin %s %s && "
					  "git -C \"%s\" checkout FETCH_HEAD %s",
					  target_dir, sink_path, target_dir, libURL, sink_path,
					  target_dir, ref_hash, sink_path, target_dir, sink_path));

	if (system(command) != 0) {
		if (directory_exists(target_dir)) {
			remove_directory(arena, target_dir);
		}
		arena_temp_end(temp);
		return NULL;
	}

	char *raw_tag = get_tag_from_hash(arena, target_dir, ref_hash);
	char tag_buf[256];
	snprintf(tag_buf, sizeof(tag_buf), "%s", raw_tag ? raw_tag : "unknown");

	remove_directory(arena,
					 string(string_format(arena, "%s/.git", target_dir)));

	arena_temp_end(temp);

	Dependency *lib_details = arena_push(arena, Dependency, 1);
	lib_details->repo_name = get_repo_name(arena, libURL);
	lib_details->version = string_from(arena, tag_buf);
	lib_details->hash = string_from(arena, (char *)ref_hash);
	lib_details->remote = string_from(arena, libURL);

	printf("[✓] Installed %s (version: %s, hash: %s)\n", lib_details->repo_name,
		   string(lib_details->version), string(lib_details->hash));

	return lib_details;
}

Dependency *clone_lib(Arena *arena, char *libURL, const char *hash) {
	ArenaTemp temp = arena_temp_begin(arena);

	char *url = get_modified_url(arena, libURL);
	if (url == NULL) {
		printf("[x] Invalid URL\n");
		arena_temp_end(temp);
		return NULL;
	}

	char url_buf[1024];
	snprintf(url_buf, sizeof(url_buf), "%s", url);

	if (hash && hash[0] != '\0') {
		arena_temp_end(temp);
		return clone_lib_hashed(arena, url_buf, hash);
	}

	char *temp_repo = get_repo_name(arena, url_buf);
	if (temp_repo == NULL) {
		printf("[x] Invalid URL\n");
		arena_temp_end(temp);
		return NULL;
	}

	char *version_number = get_version_number(arena, libURL);
	if (version_number == NULL) {
		printf("[x] Version details missing\n");
		arena_temp_end(temp);
		return NULL;
	}

	if (!directory_exists("./deps")) {
		if (MAKE_DIR("./deps") != 0 && errno != EEXIST) {
			arena_temp_end(temp);
			return NULL;
		}
	}

	char *target_dir = string(string_format(arena, "./deps/%s", temp_repo));

#if defined(_WIN32)
	const char *sink_path = ">NUL 2>&1";
#else
	const char *sink_path = ">/dev/null 2>&1";
#endif

	String *command;
	if (STR_CMP(version_number, "unknown") == 0) {
		command =
			string_format(arena, "git clone --depth 1 --quiet \"%s\" \"%s\" %s",
						  url_buf, target_dir, sink_path);
	} else {
		command = string_format(
			arena,
			"git clone --depth 1 --quiet --branch \"%s\" \"%s\" \"%s\" %s",
			version_number, url_buf, target_dir, sink_path);
	}

	if (system(string(command)) != 0) {
		if (directory_exists(target_dir)) {
			remove_directory(arena, target_dir);
		}
		arena_temp_end(temp);
		return NULL;
	}

	char *ref_hash = get_lib_hash(arena, target_dir);
	char *fetched_version = get_tag_from_hash(arena, target_dir, ref_hash);

	char hash_buf[128];
	char version_buf[256];
	snprintf(hash_buf, sizeof(hash_buf), "%s", ref_hash ? ref_hash : "");
	snprintf(version_buf, sizeof(version_buf), "%s",
			 fetched_version ? fetched_version : "unknown");

	remove_directory(arena,
					 string(string_format(arena, "%s/.git", target_dir)));

	arena_temp_end(temp);

	Dependency *lib_details = arena_push(arena, Dependency, 1);
	lib_details->repo_name = get_repo_name(arena, url_buf);
	lib_details->version = string_from(arena, version_buf);
	lib_details->hash = string_from(arena, hash_buf);
	lib_details->remote = string_from(arena, url_buf);

	printf("[✓] Installed %s (version: %s, hash: %s)\n", lib_details->repo_name,
		   string(lib_details->version), string(lib_details->hash));

	return lib_details;
}

void sync_flags(Config *conf, Vector(String *) flags) {
	for (int j = 0; j < length(flags); j++) {
		String *flag = at(flags, j);
		if (!set_contains_str(conf->flags, flag)) {
			append(conf->flags, string_clone(conf->arena, flag));
		}
	}
}

void sync_lib_links(Config *conf, Vector(String *) lib_links) {
	for (int j = 0; j < length(lib_links); j++) {
		String *link = at(lib_links, j);
		if (!set_contains_str(conf->lib_links, link)) {
			append(conf->lib_links, string_clone(conf->arena, link));
		}
	}
}

static void sync_repo_paths(Config *conf, Vector(String *) dest,
							Vector(String *) src, char *repo_name,
							bool allow_empty_subpath) {
	for (int j = 0; j < length(src); j++) {
		char *raw_str = string(at(src, j));
		char *colon = strchr(raw_str, ':');

		ArenaTemp temp = arena_temp_begin(conf->arena);
		String *entry;

		if (colon) {
			int prefix_len = (int)(colon - raw_str) + 1;
			const char *path = colon + 1;

			if (allow_empty_subpath && path[0] == '\0') {
				entry = string_format(conf->arena, "%.*sdeps/%s", prefix_len,
									  raw_str, repo_name);
			} else {
				entry = string_format(conf->arena, "%.*sdeps/%s/%s", prefix_len,
									  raw_str, repo_name, path);
			}
		} else {
			if (allow_empty_subpath && raw_str[0] == '\0') {
				entry = string_format(conf->arena, "deps/%s", repo_name);
			} else {
				entry = string_format(conf->arena, "deps/%s/%s", repo_name,
									  raw_str);
			}
		}

		if (set_contains_str(dest, entry)) {
			arena_temp_end(temp);
		} else {
			append(dest, entry);
		}
	}
}

void sync_excludes(Config *conf, Vector(String *) excludes, char *repo_name) {
	sync_repo_paths(conf, conf->excludes, excludes, repo_name, false);
}

void sync_exclude_dirs(Config *conf, Vector(String *) exclude_dirs,
					   char *repo_name) {
	sync_repo_paths(conf, conf->exclude_dirs, exclude_dirs, repo_name, true);
}

void sync_exclude_exception(Config *conf, Vector(String *) exclude_exception,
							char *repo_name) {
	sync_repo_paths(conf, conf->exclude_exception, exclude_exception, repo_name,
					false);
}

void sync_tmpl(Config *conf, Cmap(String *) tmpl) {
	if (!tmpl) {
		return;
	}

	Vector(char *) tmpl_keys = map_keys(tmpl);
	for (int j = 0; j < length(tmpl_keys); j++) {
		char *key = at(tmpl_keys, j);
		String *val = map_get(tmpl, key);
		String *existing = map_get(conf->tmpl, key);

		if (existing && string_comp(existing, val)) {
			continue;
		}

		map_add(conf->tmpl, key, string_clone(conf->arena, val));
	}
}

void update_dependency_map(Config *conf, Dependency *lib) {
	map_add(conf->dependencies, lib->repo_name, lib);
}

void update_lock(Cmap(Dependency *) lock, Dependency *lib) {
	map_add(lock, lib->repo_name, lib);
}

void fetch_library(Config *current_config, char *libURL, char *hash,
				   Cmap(Dependency *) lock, bool sync) {
	Dependency *lib_details = clone_lib(current_config->arena, libURL, hash);
	if (lib_details == NULL) {
		return;
	}

	update_lock(lock, lib_details);
	update_dependency_map(current_config, lib_details);

	ArenaTemp temp = arena_temp_begin(current_config->arena);
	String *config_path =
		string_format(current_config->arena, "./deps/%s/composition.json",
					  lib_details->repo_name);
	bool has_config = is_mybuild_config_present(string(config_path));

	if (!has_config) {
		arena_temp_end(temp);
		return;
	}

	Config *dep_config = config_init();
	read_flint_composition(string(config_path), dep_config);
	arena_temp_end(temp);

	sync_flags(current_config, dep_config->flags);
	sync_lib_links(current_config, dep_config->lib_links);
	sync_tmpl(current_config, dep_config->tmpl);
	sync_excludes(current_config, dep_config->excludes, lib_details->repo_name);
	sync_exclude_dirs(current_config, dep_config->exclude_dirs,
					  lib_details->repo_name);
	sync_exclude_exception(current_config, dep_config->exclude_exception,
						   lib_details->repo_name);

	if (map_len(dep_config->dependencies)) {
		Vector(char *) dep_dependencies = map_keys(dep_config->dependencies);
		for (int i = 0; i < length(dep_dependencies); i++) {
			Dependency *dep_fetch =
				map_get(dep_config->dependencies, at(dep_dependencies, i));
			if (!lock_contains(dep_config->arena, lock,
							   string(dep_fetch->remote))) {
				char *modified_url = string(string_format(
					dep_config->arena, "%s@%s", string(dep_fetch->remote),
					string(dep_fetch->version)));
				char *dep_hash = dep_fetch->hash ? string(dep_fetch->hash) : "";
				fetch_library(current_config, modified_url, dep_hash, lock,
							  false);
			}
		}
	}
	config_free(dep_config);
}

void sync_remote_composition(Config *conf, Cmap(Dependency *) lock,
							 char *fragment) {
	char *colon = strchr(fragment, ':');
	if (!colon)
		return;
	char *repo_name = colon + 1;

	Arena *scratch = arena_init(4096);
	Sync_config *remote_composition =
		get_remote_composition(scratch, repo_name);
	if (remote_composition == NULL) {
		printf("Unable to fetch remote composition\n");
		arena_free(&scratch);
		return;
	}

	if (!lock_contains(scratch, lock, string(remote_composition->remote))) {
		char *modified_url = string(
			string_format(scratch, "%s@%s", string(remote_composition->remote),
						  string(remote_composition->version)));
		char *hash =
			remote_composition->hash ? string(remote_composition->hash) : "";

		fetch_library(conf, modified_url, hash, lock, true);

		sync_flags(conf, remote_composition->flags);
		sync_lib_links(conf, remote_composition->lib_links);
		sync_excludes(conf, remote_composition->excludes, repo_name);
		sync_exclude_dirs(conf, remote_composition->exclude_dirs, repo_name);
		sync_exclude_exception(conf, remote_composition->exclude_exception,
							   repo_name);
		sync_tmpl(conf, remote_composition->tmpl);
	} else {
		printf("[*] Library '%s' already installed, hence skipping...\n",
			   repo_name);
	}

	arena_free(&scratch);
}

void add_lib(char *libURL) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);
	Cmap(Dependency *) lock = read_lock_file(conf->arena);

	if (strncmp(libURL, "cherts:", 7) == 0) {
		sync_remote_composition(conf, lock, libURL);
	} else {
		if (!lock_contains(conf->arena, lock,
						   get_modified_url(conf->arena, libURL))) {
			fetch_library(conf, libURL, "", lock, false);
		} else {
			printf("[*] Dependency already available or malformed URL, "
				   "installation skipped...\n");
		}
	}

	write_flint_composition("composition.json", conf);
	write_lock_file(lock);
	generate_compile_commands(conf);
	config_free(conf);
}

void sync_lib(char *elem) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	if (elem && STR_CMP(elem, "all") == 0) {
		remove("flint.lock");
		ArenaTemp temp = arena_temp_begin(conf->arena);
		remove_directory(conf->arena, "deps");
		arena_temp_end(temp);
	}

	Cmap(Dependency *) lock = read_lock_file(conf->arena);

	if (map_len(conf->sync) > 0) {
		Vector(char *) sync_keys = map_keys(conf->sync);
		for (int i = 0; i < length(sync_keys); i++) {
			char *repo_name = at(sync_keys, i);
			Sync_config *dependency = map_get(conf->sync, repo_name);

			ArenaTemp temp = arena_temp_begin(conf->arena);
			bool is_locked =
				lock_contains(conf->arena, lock, string(dependency->remote));
			arena_temp_end(temp);

			if (!is_locked) {
				char modified_url[1024];
				snprintf(modified_url, sizeof(modified_url), "%s@%s",
						 string(dependency->remote),
						 string(dependency->version));
				char *hash = dependency->hash ? string(dependency->hash) : "";

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
		// vector_free(&sync_keys);
	}

	if (map_len(conf->dependencies) > 0) {
		Vector(char *) dep_keys = map_keys(conf->dependencies);
		for (int i = 0; i < length(dep_keys); i++) {
			char *repo_name = at(dep_keys, i);
			Dependency *dependency = map_get(conf->dependencies, repo_name);

			ArenaTemp temp = arena_temp_begin(conf->arena);
			bool is_locked =
				lock_contains(conf->arena, lock, string(dependency->remote));
			arena_temp_end(temp);

			if (!is_locked) {
				char modified_url[1024];
				snprintf(modified_url, sizeof(modified_url), "%s@%s",
						 string(dependency->remote),
						 string(dependency->version));
				char *hash = dependency->hash ? string(dependency->hash) : "";

				fetch_library(conf, modified_url, hash, lock, true);
			}
		}
		// vector_free(&dep_keys);
	}

	write_flint_composition("composition.json", conf);
	write_lock_file(lock);
	generate_compile_commands(conf);
	config_free(conf);
}

void remove_library_partial(Arena *arena, char *repo_name) {
	Cmap(Dependency *) lock = read_lock_file(arena);

	remove_directory(
		arena, string(string_concat_cstr(arena, 2, "./deps/", repo_name)));

	map_remove(lock, repo_name);
	write_lock_file(lock);
}

void remove_library(char *repo_name) {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	if (!map_has(conf->dependencies, repo_name)) {
		config_free(conf);
		printf("[x] No dependency named '%s' found\n", repo_name);
		return;
	}

	// printf("[+] Removing '%s'...\n", repo_name);

	ArenaTemp temp = arena_temp_begin(conf->arena);
	remove_library_partial(conf->arena, repo_name);
	arena_temp_end(temp);

	char search_str[512];
	snprintf(search_str, sizeof(search_str), "deps/%s", repo_name);

	conf->excludes = remove_arr_entry(conf->arena, conf->excludes, search_str);
	conf->exclude_dirs =
		remove_arr_entry(conf->arena, conf->exclude_dirs, search_str);
	conf->exclude_exception =
		remove_arr_entry(conf->arena, conf->exclude_exception, search_str);

	map_remove(conf->dependencies, repo_name);

	printf("[✓] Removed %s!\n", repo_name);
	write_flint_composition("composition.json", conf);
	generate_compile_commands(conf);
	config_free(conf);
}

void list_deps() {
	Config *conf = config_init();
	read_flint_composition("composition.json", conf);

	if (map_len(conf->dependencies) > 0) {
		Vector(char *) dep_list = map_keys(conf->dependencies);
		for (int i = 0; i < length(dep_list); i++) {
			char *key = at(dep_list, i);
			Dependency *dep = map_get(conf->dependencies, key);
			printf("[*] %s: %s\n", key, string(dep->version));
		}
	} else {
		printf("[x] No dependencies found.\n");
	}
	config_free(conf);
}
