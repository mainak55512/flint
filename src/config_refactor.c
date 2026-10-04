#include <arena.h>
#include <cmap.h>
#include <container.h>
#include <cstring.h>
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
  Vector *sync_keys = map_keys(conf->sync);
  for (int i = 0; i < length(sync_keys); i++) {
    sync_config_free(
        (Sync_config *)map_get(conf->sync, at(char *, sync_keys, i)));
  }
  vector_free(&sync_keys);
  map_free(conf->sync);
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
  conf->project_name =
      string_from(conf->arena,
                  (char *)yyjson_get_str(yyjson_obj_get(root, "project_name")));
  conf->project_language = string_from(
      conf->arena,
      (char *)yyjson_get_str(yyjson_obj_get(root, "project_language")));
  conf->compiler_path = string_from(
      conf->arena,
      (char *)yyjson_get_str(yyjson_obj_get(root, "compiler_path")));
  conf->version = string_from(
      conf->arena, (char *)yyjson_get_str(yyjson_obj_get(root, "version")));
  conf->executable = yyjson_get_bool(yyjson_obj_get(root, "executable"));

  size_t idx, max;
  yyjson_val *key, *val;

  yyjson_val *flag_arr = yyjson_obj_get(root, "flags");
  conf->flags = vector_init(String *);

  yyjson_arr_foreach(flag_arr, idx, max, val) {
    append(String *, conf->flags,
           string_from(conf->arena, (char *)yyjson_get_str(val)));
  }

  yyjson_val *lib_link_arr = yyjson_obj_get(root, "lib_links");
  conf->lib_links = vector_init(String *);

  yyjson_arr_foreach(lib_link_arr, idx, max, val) {
    append(String *, conf->lib_links,
           string_from(conf->arena, (char *)yyjson_get_str(val)));
  }

  yyjson_val *excludes_arr = yyjson_obj_get(root, "excludes");
  conf->excludes = vector_init(String *);

  yyjson_arr_foreach(excludes_arr, idx, max, val) {
    append(String *, conf->excludes,
           string_from(conf->arena, (char *)yyjson_get_str(val)));
  }

  yyjson_val *exclude_dir_arr = yyjson_obj_get(root, "exclude_dirs");
  conf->exclude_dirs = vector_init(String *);

  yyjson_arr_foreach(exclude_dir_arr, idx, max, val) {
    append(String *, conf->exclude_dirs,
           string_from(conf->arena, (char *)yyjson_get_str(val)));
  }

  yyjson_val *exclude_exception_arr = yyjson_obj_get(root, "exclude_exception");
  conf->exclude_exception = vector_init(String *);

  yyjson_arr_foreach(exclude_exception_arr, idx, max, val) {
    append(String *, conf->exclude_exception,
           string_from(conf->arena, (char *)yyjson_get_str(val)));
  }

  yyjson_val *dependencies = yyjson_obj_get(root, "dependencies");
  conf->dependencies = map_init();
  yyjson_obj_foreach(dependencies, idx, max, key, val) {
    Dependency *dep = arena_alloc(conf->arena, sizeof(Dependency));
    dep->version = string_from(
        conf->arena, (char *)yyjson_get_str(yyjson_obj_get(val, "version")));
    dep->remote = string_from(
        conf->arena, (char *)yyjson_get_str(yyjson_obj_get(val, "remote")));
    dep->hash = string_from(
        conf->arena, (char *)yyjson_get_str(yyjson_obj_get(val, "hash")));
    map_add(conf->dependencies, yyjson_get_str(key), dep);
  }

  yyjson_val *tmpl = yyjson_obj_get(root, "tmpl");
  conf->tmpl = map_init();
  yyjson_obj_foreach(tmpl, idx, max, key, val) {
    map_add(conf->tmpl, yyjson_get_str(key),
            string_from(conf->arena, (char *)yyjson_get_str(val)));
  }

  yyjson_val *sync = yyjson_obj_get(root, "sync");
  conf->sync = map_init();
  yyjson_obj_foreach(sync, idx, max, key, val) {
    Sync_config *sync_config = arena_alloc(conf->arena, sizeof(Sync_config));
    sync_config->version = string_from(
        conf->arena, (char *)yyjson_get_str(yyjson_obj_get(val, "version")));
    sync_config->remote = string_from(
        conf->arena, (char *)yyjson_get_str(yyjson_obj_get(val, "remote")));

    size_t idx_sync, max_sync;
    yyjson_val *key_sync, *val_sync;

    yyjson_val *flag_arr = yyjson_obj_get(val, "flags");
    sync_config->flags = vector_init(String *);

    yyjson_arr_foreach(flag_arr, idx_sync, max_sync, val_sync) {
      append(String *, sync_config->flags,
             string_from(conf->arena, (char *)yyjson_get_str(val_sync)));
    }

    yyjson_val *lib_link_arr = yyjson_obj_get(val, "lib_links");
    sync_config->lib_links = vector_init(String *);

    yyjson_arr_foreach(lib_link_arr, idx_sync, max_sync, val_sync) {
      append(String *, sync_config->lib_links,
             string_from(conf->arena, (char *)yyjson_get_str(val_sync)));
    }
    yyjson_val *excludes = yyjson_obj_get(val, "excludes");
    sync_config->excludes = vector_init(String *);

    yyjson_arr_foreach(excludes, idx_sync, max_sync, val_sync) {
      append(String *, sync_config->excludes,
             string_from(conf->arena, (char *)yyjson_get_str(val_sync)));
    }
    yyjson_val *exclude_dirs = yyjson_obj_get(val, "exclude_dirs");
    sync_config->exclude_dirs = vector_init(String *);

    yyjson_arr_foreach(exclude_dirs, idx_sync, max_sync, val_sync) {
      append(String *, sync_config->exclude_dirs,
             string_from(conf->arena, (char *)yyjson_get_str(val_sync)));
    }
    yyjson_val *exclude_exception = yyjson_obj_get(val, "exclude_exception");
    sync_config->exclude_exception = vector_init(String *);

    yyjson_arr_foreach(exclude_exception, idx_sync, max_sync, val_sync) {
      append(String *, sync_config->exclude_exception,
             string_from(conf->arena, (char *)yyjson_get_str(val_sync)));
    }
    yyjson_val *tmpl = yyjson_obj_get(val, "tmpl");
    sync_config->tmpl = map_init();
    yyjson_obj_foreach(tmpl, idx_sync, max_sync, key_sync, val_sync) {
      map_add(sync_config->tmpl, yyjson_get_str(key_sync),
              string_from(conf->arena, (char *)yyjson_get_str(val_sync)));
    }
    map_add(conf->sync, yyjson_get_str(key), sync_config);
  }

  yyjson_doc_free(doc);
}

void write_flint_composition(char *composition_path, Config *conf) {

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_str(doc, root, "project_name", string(conf->project_name));
  yyjson_mut_obj_add_str(doc, root, "project_language",
                         string(conf->project_language));
  yyjson_mut_obj_add_str(doc, root, "compiler_path",
                         string(conf->compiler_path));
  yyjson_mut_obj_add_str(doc, root, "version", string(conf->version));
  yyjson_mut_obj_add_bool(doc, root, "executable", conf->executable);

  yyjson_mut_val *flags = yyjson_mut_arr(doc);
  for (int i = 0; i < length(conf->flags); i++) {
    yyjson_mut_arr_add_str(doc, flags, string(at(String *, conf->flags, i)));
  }
  yyjson_mut_obj_add_val(doc, root, "flags", flags);

  yyjson_mut_val *lib_links = yyjson_mut_arr(doc);
  for (int i = 0; i < length(conf->lib_links); i++) {
    yyjson_mut_arr_add_str(doc, lib_links,
                           string(at(String *, conf->lib_links, i)));
  }
  yyjson_mut_obj_add_val(doc, root, "lib_links", lib_links);

  yyjson_mut_val *excludes = yyjson_mut_arr(doc);
  for (int i = 0; i < length(conf->excludes); i++) {
    yyjson_mut_arr_add_str(doc, excludes,
                           string(at(String *, conf->excludes, i)));
  }
  yyjson_mut_obj_add_val(doc, root, "excludes", excludes);

  yyjson_mut_val *exclude_dirs = yyjson_mut_arr(doc);
  for (int i = 0; i < length(conf->exclude_dirs); i++) {
    yyjson_mut_arr_add_str(doc, exclude_dirs,
                           string(at(String *, conf->exclude_dirs, i)));
  }
  yyjson_mut_obj_add_val(doc, root, "exclude_dirs", exclude_dirs);

  yyjson_mut_val *exclude_exception = yyjson_mut_arr(doc);
  for (int i = 0; i < length(conf->exclude_exception); i++) {
    yyjson_mut_arr_add_str(doc, exclude_exception,
                           string(at(String *, conf->exclude_exception, i)));
  }
  yyjson_mut_obj_add_val(doc, root, "exclude_exception", exclude_exception);

  yyjson_mut_val *tmpl = yyjson_mut_obj(doc);
  Vector *tmpl_keys = map_keys(conf->tmpl);
  for (int i = 0; i < length(tmpl_keys); i++) {
    yyjson_mut_obj_add_strcpy(
        doc, tmpl, at(char *, tmpl_keys, i),
        string((String *)map_get(conf->tmpl, at(char *, tmpl_keys, i))));
  }
  yyjson_mut_obj_add_val(doc, root, "tmpl", tmpl);
  vector_free(&tmpl_keys);

  yyjson_mut_val *dependencies = yyjson_mut_obj(doc);
  Vector *dependency_keys = map_keys(conf->dependencies);
  for (int i = 0; i < length(dependency_keys); i++) {
    char *key = at(char *, dependency_keys, i);
    Dependency *elem = (Dependency *)map_get(conf->dependencies, key);
    yyjson_mut_val *dep_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_strcpy(doc, dep_obj, "version", string(elem->version));
    yyjson_mut_obj_add_strcpy(doc, dep_obj, "remote", string(elem->remote));
    yyjson_mut_obj_add_strcpy(doc, dep_obj, "hash", string(elem->hash));
    yyjson_mut_obj_add_val(doc, dependencies, key, dep_obj);
  }
  yyjson_mut_obj_add_val(doc, root, "dependencies", dependencies);
  vector_free(&dependency_keys);

  yyjson_mut_val *sync = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, root, "sync", sync);

  yyjson_write_flag flg = YYJSON_WRITE_PRETTY;

  yyjson_write_err err;
  bool success = yyjson_mut_write_file(composition_path, doc, flg, NULL, &err);

  if (success) {
    printf("[✓] Dependencies synced\n");
  } else {
    printf("[x] Failed to write JSON: %s (code: %u)\n", err.msg, err.code);
  }
  yyjson_mut_doc_free(doc);
}

void sync_flags(Config *conf, Sync_config *dependency) {
  printf("[+] Syncing flags...\n");
  for (int j = 0; j < length(dependency->flags); j++) {
    set_add_str(conf->flags, at(String *, dependency->flags, j));
  }
}
void sync_lib_links(Config *conf, Sync_config *dependency) {
  printf("[+] Syncing library links...\n");
  for (int j = 0; j < length(dependency->lib_links); j++) {
    set_add_str(conf->lib_links, at(String *, dependency->lib_links, j));
  }
}
void sync_excludes(Config *conf, Sync_config *dependency, char *repo_name) {
  printf("[+] Syncing exclude files...\n");
  for (int j = 0; j < length(dependency->excludes); j++) {
    set_add_str(
        conf->excludes,
        string_concat_cstr(conf->arena, 4, "deps/", repo_name, "/",
                           string(at(String *, dependency->excludes, j))));
  }
}
void sync_exclude_dirs(Config *conf, Sync_config *dependency, char *repo_name) {
  printf("[+] Syncing exclude directories...\n");
  for (int j = 0; j < length(dependency->exclude_dirs); j++) {
    set_add_str(
        conf->exclude_dirs,
        string_concat_cstr(conf->arena, 4, "deps/", repo_name, "/",
                           string(at(String *, dependency->exclude_dirs, j))));
  }
}
void sync_exclude_exception(Config *conf, Sync_config *dependency,
                            char *repo_name) {
  printf("[+] Syncing exception directories...\n");
  for (int j = 0; j < length(dependency->exclude_exception); j++) {
    set_add_str(conf->exclude_exception,
                string_concat_cstr(
                    conf->arena, 4, "deps/", repo_name, "/",
                    string(at(String *, dependency->exclude_exception, j))));
  }
}
void sync_tmpl(Config *conf, Sync_config *dependency) {
  printf("[+] Syncing template variables...\n");
  Vector *tmpl_keys = map_keys(dependency->tmpl);
  for (int j = 0; j < length(tmpl_keys); j++) {
    map_add(conf->tmpl, at(char *, tmpl_keys, j),
            (String *)map_get(dependency->tmpl, at(char *, tmpl_keys, j)));
  }
  vector_free(&tmpl_keys);
}

void update_dependency_map(Config *conf, Dependency *lib) {
  map_add(conf->dependencies, lib->repo_name, lib);
}

void sync_config(char *composition_path, Config *conf) {
  read_flint_composition(composition_path, conf);

  Vector *sync_keys = map_keys(conf->sync);

  if (length(sync_keys)) {
    for (int i = 0; i < length(sync_keys); i++) {
      char *repo_name = at(char *, sync_keys, i);
      Sync_config *dependency =
          (Sync_config *)map_get(conf->sync, at(char *, sync_keys, i));
      sync_flags(conf, dependency);
      sync_lib_links(conf, dependency);
      sync_excludes(conf, dependency, repo_name);
      sync_exclude_dirs(conf, dependency, repo_name);
      sync_exclude_exception(conf, dependency, repo_name);
      sync_tmpl(conf, dependency);

      Dependency *entry = arena_alloc(conf->arena, sizeof(Dependency));
      entry->repo_name = repo_name;
      entry->remote = dependency->remote;
      entry->version = dependency->version;
      entry->hash = string_from(conf->arena, "kjabhsvkjabdjkahvbkdjhv138796sb");

      update_dependency_map(conf, entry);
    }
  }
  write_flint_composition(composition_path, conf);
  vector_free(&sync_keys);
}

void fetch_lib_demo(Config *conf) {}

int main() {

  Config *conf = config_init();
  sync_config("demo.json", conf);

  config_free(conf);
}
