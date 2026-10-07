#include <arena.h>
#include <cmap.h>
#include <container.h>
#include <cstring.h>
#include <flint.h>
#include <stdio.h>
#include <yyjson.h>

typedef struct {
	char *repo_name;
	String *version;
	String *remote;
	String *hash;
} Dependency;

typedef struct {
	String *version;
	String *hash;
	String *remote;
	Vector *flags;
	Vector *lib_links;
	Vector *excludes;
	Vector *exclude_dirs;
	Vector *exclude_exception;
	Cmap *tmpl;
} Sync_config;

typedef struct {
	String *project_name;
	String *project_language;
	String *compiler_path;
	String *version;
	bool executable;
	Vector *flags;
	Vector *lib_links;
	Vector *excludes;
	Vector *exclude_dirs;
	Cmap *tmpl;
	Cmap *dependencies;
	Vector *exclude_exception;
	Cmap *sync;
	Arena *arena;
} Config;

#define STR_CMP strcasecmp

Dependency *clone_lib_hashed(Arena *arena, char *libURL, const char *ref_hash) {
	Dependency *lib_details =
		(Dependency *)arena_alloc(arena, sizeof(Dependency));
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
	char *sink_path = ">/dev/null 2>&1";
	printf("[+] Installing %s...\n", repo_name);
	String *command;
	// if (hash != NULL) {
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
	// }

	if (system(string(command)) >> 8 == 128) {
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

bool set_contains_str(Vector *v, String *elem) {
	for (int i = 0; i < length(v); i++) {
		if (STR_CMP(string(at(String *, v, i)), string(elem)) == 0) {
			return true;
		}
	}
	return false;
}

void set_add_str(Vector *v, String *elem) {
	if (!set_contains_str(v, elem)) {
		append(String *, v, elem);
	}
}

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

static String *safe_get_string(Arena *arena, yyjson_val *obj, const char *key) {
	yyjson_val *val = yyjson_obj_get(obj, key);
	const char *str = yyjson_get_str(val);
	return str ? string_from(arena, (char *)str) : NULL;
}

Vector *parse_string_array(Arena *arena, yyjson_val *obj, char *key) {
	yyjson_val *arr = yyjson_obj_get(obj, key);
	Vector *target_vec = vector_init(String *);
	size_t idx, max;
	yyjson_val *val;
	if (yyjson_is_arr(arr)) {
		yyjson_arr_foreach(arr, idx, max, val) {
			const char *s = yyjson_get_str(val);
			if (s)
				append(String *, target_vec, string_from(arena, (char *)s));
		}
	}
	return target_vec;
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

char *safe_str(String *s) { return s ? string(s) : ""; }

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

	// Excludes
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

	// Exclude Dirs
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

	// Exclude Exceptions
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

	// Tmpl — replaced raw string() with safe_str()
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
		printf("[✓] Dependencies synced\n");
	} else {
		printf("[x] Failed to write JSON: %s (code: %u)\n", err.msg, err.code);
	}
	yyjson_mut_doc_free(doc);
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
		set_add_str(conf->excludes,
					string_concat_cstr(conf->arena, 4, "deps/", repo_name, "/",
									   string(at(String *, excludes, j))));
	}
}
void sync_exclude_dirs(Config *conf, Vector *exclude_dirs, char *repo_name) {
	for (int j = 0; j < length(exclude_dirs); j++) {
		set_add_str(conf->exclude_dirs,
					string_concat_cstr(conf->arena, 4, "deps/", repo_name, "/",
									   string(at(String *, exclude_dirs, j))));
	}
}
void sync_exclude_exception(Config *conf, Vector *exclude_exception,
							char *repo_name) {
	for (int j = 0; j < length(exclude_exception); j++) {
		set_add_str(
			conf->exclude_exception,
			string_concat_cstr(conf->arena, 4, "deps/", repo_name, "/",
							   string(at(String *, exclude_exception, j))));
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

void fetch_library(Config *current_config, char *libURL, char *hash, Vector *v,
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

		if (dep_config->dependencies != NULL) {
			Vector *dep_dependencies = map_keys(dep_config->dependencies);
			for (int i = 0; i < length(dep_dependencies); i++) {
				Dependency *dep_fetch = (Dependency *)map_get(
					dep_config->dependencies, at(char *, dep_dependencies, i));
				if (!set_contains_str(v, dep_fetch->remote)) {
					char *modified_url = string(string_concat_cstr(
						current_config->arena, 3, string(dep_fetch->remote),
						"@", string(dep_fetch->version)));
					char *hash = "";
					if (dep_fetch->hash) {
						hash = string(dep_fetch->hash);
					}
					set_add_str(v, dep_fetch->remote);
					fetch_library(current_config, modified_url, hash, v, false);
				}
			}

			vector_free(&dep_dependencies);
		}
		config_free(dep_config);
	}
}

void add_lib(char *libURL) {
	Config *conf = config_init();
	read_flint_composition("demo.json", conf);

	Vector *v = vector_init(String *);
	Vector *dep_keys = map_keys(conf->dependencies);
	for (int i = 0; i < length(dep_keys); i++) {
		Dependency *elem =
			(Dependency *)map_get(conf->dependencies, at(char *, dep_keys, i));
		set_add_str(v, string_clone(conf->arena, elem->remote));
	}
	vector_free(&dep_keys);

	if (!set_contains_str(v, string_from(conf->arena, libURL))) {
		fetch_library(conf, libURL, "", v, false);
	} else {
		printf("[x] Dependency already available, installation skipped\n");
	}

	write_flint_composition("demo.json", conf);
	vector_free(&v);
	config_free(conf);
}

void sync_lib() {
	Config *conf = config_init();
	read_flint_composition("demo.json", conf);
	Vector *v = vector_init(String *);
	Vector *dep_keys = map_keys(conf->dependencies);
	for (int i = 0; i < length(dep_keys); i++) {
		Dependency *elem =
			(Dependency *)map_get(conf->dependencies, at(char *, dep_keys, i));
		set_add_str(v, string_clone(conf->arena, elem->remote));
	}
	Vector *sync_keys = map_keys(conf->sync);

	if (length(sync_keys)) {
		for (int i = 0; i < length(sync_keys); i++) {
			char *repo_name = at(char *, sync_keys, i);
			Sync_config *dependency =
				(Sync_config *)map_get(conf->sync, at(char *, sync_keys, i));
			if (!set_contains_str(v, dependency->remote)) {
				printf("Contains url: %s\n", string(dependency->remote));
				char *modified_url = string(string_concat_cstr(
					conf->arena, 3, string(dependency->remote), "@",
					string(dependency->version)));
				char *hash = "";
				if (dependency->hash) {
					hash = string(dependency->hash);
				}
				set_add_str(v, dependency->remote);
				fetch_library(conf, modified_url, hash, v, true);

				sync_flags(conf, dependency->flags);
				sync_lib_links(conf, dependency->lib_links);
				sync_excludes(conf, dependency->excludes, repo_name);
				sync_exclude_dirs(conf, dependency->exclude_dirs, repo_name);
				sync_exclude_exception(conf, dependency->exclude_exception,
									   repo_name);
				sync_tmpl(conf, dependency->tmpl);
			}
		}
	} else {
		printf("[x] Dependency already available, installation skipped\n");
	}
	write_flint_composition("demo.json", conf);
	vector_free(&sync_keys);
	vector_free(&v);
	vector_free(&dep_keys);
	config_free(conf);
}

int main() {

	// Config *conf = config_init();
	// read_flint_composition("./deps/Cmap/composition.json", conf);

	// config_free(conf);
	// add_lib("https://github.com/mainak55512/Cmap@v0.1.1");
	sync_lib();
}
