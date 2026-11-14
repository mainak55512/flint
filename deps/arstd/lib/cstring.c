#include <cstring.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct String {
  size_t length;
  char str[];
};

String *string_from(Arena *arena, const char *str) {
  size_t len;

  if (arena == NULL || str == NULL) {
    return NULL;
  }

  len = strlen(str);
  return string_from_len(arena, len, str);
}

String *string_from_len(Arena *arena, size_t len, const char *str) {
  String *st;

  if (arena == NULL || (str == NULL && len > 0)) {
    return NULL;
  }

  st = (String *)arena_alloc(arena, sizeof(String) + len + 1);
  if (st == NULL) {
    return NULL;
  }

  if (len > 0) {
    memcpy(st->str, str, len);
  }
  st->str[len] = '\0';
  st->length = len;

  return st;
}

String *string_format(Arena *arena, const char *fmt, ...) {
  va_list args, args_copy;
  int needed;
  String *st;

  if (arena == NULL || fmt == NULL) {
    return NULL;
  }

  va_start(args, fmt);
  va_copy(args_copy, args);
  needed = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (needed < 0) {
    va_end(args_copy);
    return NULL;
  }

  st = (String *)arena_alloc(arena, sizeof(String) + (size_t)needed + 1);
  if (st != NULL) {
    vsnprintf(st->str, (size_t)needed + 1, fmt, args_copy);
    st->length = (size_t)needed;
  }
  va_end(args_copy);

  return st;
}

String *string_clone(Arena *arena, String *str) {
  if (arena == NULL || str == NULL) {
    return NULL;
  }
  return string_from_len(arena, str->length, str->str);
}

String *string_concat(Arena *arena, int n, ...) {
  int i;
  va_list args;
  size_t length;
  String *s_out;
  char *p;

  if (arena == NULL) {
    return NULL;
  }
  if (n <= 0) {
    return string_from_len(arena, 0, "");
  }

  length = 0;
  va_start(args, n);
  for (i = 0; i < n; i++) {
    String *s = va_arg(args, String *);
    if (s != NULL) {
      length += s->length;
    }
  }
  va_end(args);

  s_out = (String *)arena_alloc(arena, sizeof(String) + length + 1);
  if (s_out == NULL) {
    return NULL;
  }

  p = s_out->str;
  va_start(args, n);
  for (i = 0; i < n; i++) {
    String *s = va_arg(args, String *);
    if (s != NULL && s->length > 0) {
      memcpy(p, s->str, s->length);
      p += s->length;
    }
  }
  va_end(args);
  *p = '\0';
  s_out->length = length;

  return s_out;
}

String *string_concat_cstr(Arena *arena, int n, ...) {
  int i;
  va_list args;
  size_t length;
  String *s_out;
  char *p;

  if (arena == NULL) {
    return NULL;
  }
  if (n <= 0) {
    return string_from_len(arena, 0, "");
  }

  length = 0;
  va_start(args, n);
  for (i = 0; i < n; i++) {
    const char *s = va_arg(args, const char *);
    if (s != NULL) {
      length += strlen(s);
    }
  }
  va_end(args);

  s_out = (String *)arena_alloc(arena, sizeof(String) + length + 1);
  if (s_out == NULL) {
    return NULL;
  }

  p = s_out->str;
  va_start(args, n);
  for (i = 0; i < n; i++) {
    const char *s = va_arg(args, const char *);
    if (s != NULL) {
      size_t len = strlen(s);
      memcpy(p, s, len);
      p += len;
    }
  }
  va_end(args);
  *p = '\0';
  s_out->length = length;

  return s_out;
}

int string_len(String *str) {
  if (str == NULL) {
    return 0;
  }
  return (int)str->length;
}

String *string_sub(Arena *arena, String *str, int begin, int end) {
  size_t substring_length;

  if (arena == NULL || str == NULL) {
    return NULL;
  }

  if (begin < end && begin >= 0 && (size_t)begin < str->length &&
      (size_t)end <= str->length) {
    substring_length = (size_t)(end - begin);
    return string_from_len(arena, substring_length, str->str + begin);
  }

  return string_from_len(arena, 0, "");
}

char *string(String *st) {
  if (st == NULL) {
    return NULL;
  }
  return st->str;
}

String *string_trim(Arena *arena, String *str) {
  size_t start;
  size_t end;
  size_t len;

  if (arena == NULL || str == NULL) {
    return NULL;
  }

  start = 0;
  while (start < str->length &&
         (str->str[start] == ' ' || str->str[start] == '\t' ||
          str->str[start] == '\n' || str->str[start] == '\r')) {
    start++;
  }

  if (start == str->length) {
    return string_from_len(arena, 0, "");
  }

  end = str->length - 1;
  while (end > start && (str->str[end] == ' ' || str->str[end] == '\t' ||
                         str->str[end] == '\n' || str->str[end] == '\r')) {
    end--;
  }

  len = (end - start) + 1;
  return string_from_len(arena, len, str->str + start);
}

char *read_line(FILE *stream) {
  size_t capacity;
  size_t length;
  char *buffer;
  char *temp;
  int c;

  capacity = 16;
  length = 0;
  buffer = NULL;
  temp = NULL;

  buffer = (char *)m_align_alloc(capacity * sizeof(char));
  if (buffer == NULL) {
    return NULL;
  }

  while ((c = fgetc(stream)) != EOF && c != '\n') {
    if (length + 1 >= capacity) {
      capacity *= 2;
      temp = (char *)m_align_realloc(buffer, (capacity / 2) * sizeof(char),
                                     capacity * sizeof(char));
      if (temp == NULL) {
        m_align_free(buffer);
        return NULL;
      }
      buffer = temp;
    }

    buffer[length++] = (char)c;
  }

  if (length == 0 && c == EOF) {
    m_align_free(buffer);
    return NULL;
  }

  buffer[length] = '\0';

  temp = (char *)m_align_realloc(buffer, capacity * sizeof(char),
                                 (length + 1) * sizeof(char));
  if (temp != NULL) {
    buffer = temp;
  }

  return buffer;
}

String *string_get(Arena *arena) {
  char *text;
  String *st;

  if (arena == NULL) {
    return NULL;
  }

  fflush(stdout);
  text = read_line(stdin);
  if (text == NULL) {
    return NULL;
  }

  st = string_from(arena, text);
  m_align_free(text);

  return st;
}

String *string_upper(Arena *arena, String *str) {
  size_t i;
  String *st;

  if (!arena || !str) {
    return NULL;
  }

  st = (String *)arena_alloc(arena, sizeof(String) + str->length + 1);
  if (!st) {
    return NULL;
  }

  for (i = 0; i < str->length; i++) {
    char c = str->str[i];
    if (c >= 'a' && c <= 'z') {
      c = c - ('a' - 'A');
    }
    st->str[i] = c;
  }
  st->str[str->length] = '\0';
  st->length = str->length;
  return st;
}

String *string_lower(Arena *arena, String *str) {
  size_t i;
  String *st;

  if (!arena || !str) {
    return NULL;
  }

  st = (String *)arena_alloc(arena, sizeof(String) + str->length + 1);
  if (!st) {
    return NULL;
  }

  for (i = 0; i < str->length; i++) {
    char c = str->str[i];
    if (c >= 'A' && c <= 'Z') {
      c = c + ('a' - 'A');
    }
    st->str[i] = c;
  }
  st->str[str->length] = '\0';
  st->length = str->length;
  return st;
}

String *string_reverse(Arena *arena, String *str) {
  size_t i;
  String *st;

  if (arena == NULL || str == NULL) {
    return NULL;
  }

  st = (String *)arena_alloc(arena, sizeof(String) + str->length + 1);
  if (st == NULL) {
    return NULL;
  }

  for (i = 0; i < str->length; i++) {
    st->str[i] = str->str[str->length - 1 - i];
  }
  st->str[str->length] = '\0';
  st->length = str->length;

  return st;
}

String *string_repeat(Arena *arena, String *str, int n) {
  int i;
  size_t total_len;
  String *st;
  char *p;

  if (arena == NULL || str == NULL) {
    return NULL;
  }
  if (n <= 0 || str->length == 0) {
    return string_from_len(arena, 0, "");
  }

  total_len = str->length * (size_t)n;
  st = (String *)arena_alloc(arena, sizeof(String) + total_len + 1);
  if (st == NULL) {
    return NULL;
  }

  p = st->str;
  for (i = 0; i < n; i++) {
    memcpy(p, str->str, str->length);
    p += str->length;
  }
  *p = '\0';
  st->length = total_len;

  return st;
}

bool string_comp(String *s1, String *s2) {
  size_t i;

  if (s1 == NULL || s2 == NULL) {
    return false;
  }
  if (s1->length != s2->length) {
    return false;
  }

  for (i = 0; i < s1->length; i++) {
    if (s1->str[i] == s2->str[i]) {
      continue;
    } else if (s1->str[i] >= 'A' && s1->str[i] <= 'Z' &&
               s2->str[i] == s1->str[i] + ('a' - 'A')) {
      continue;
    } else if (s1->str[i] >= 'a' && s1->str[i] <= 'z' &&
               s2->str[i] == s1->str[i] - ('a' - 'A')) {
      continue;
    } else {
      break;
    }
  }
  return (i == s1->length);
}

bool string_comp_case(String *s1, String *s2) {
  if (s1 == NULL || s2 == NULL) {
    return false;
  }
  if (s1->length != s2->length) {
    return false;
  }

  return memcmp(s1->str, s2->str, s1->length) == 0;
}

bool string_starts_with(String *str, const char *prefix) {
  size_t prefix_len;

  if (str == NULL || prefix == NULL) {
    return false;
  }

  prefix_len = strlen(prefix);
  if (prefix_len > str->length) {
    return false;
  }

  return memcmp(str->str, prefix, prefix_len) == 0;
}

bool string_ends_with(String *str, const char *suffix) {
  size_t suffix_len;

  if (str == NULL || suffix == NULL) {
    return false;
  }

  suffix_len = strlen(suffix);
  if (suffix_len > str->length) {
    return false;
  }

  return memcmp(str->str + (str->length - suffix_len), suffix, suffix_len) == 0;
}

int string_find(String *str, const char *substr) {
  char *match;

  if (str == NULL || substr == NULL) {
    return -1;
  }

  match = strstr(str->str, substr);
  if (match == NULL) {
    return -1;
  }

  return (int)(match - str->str);
}

bool string_contains(String *str, const char *substr) {
  return string_find(str, substr) != -1;
}

String *string_replace(Arena *arena, String *str, const char *old_sub,
                       const char *new_sub) {
  size_t old_len;
  size_t new_len;
  size_t count;
  size_t final_len;
  const char *curr;
  const char *match;
  String *st;
  char *dest;

  if (arena == NULL || str == NULL || old_sub == NULL || new_sub == NULL) {
    return NULL;
  }

  old_len = strlen(old_sub);
  if (old_len == 0) {
    return string_clone(arena, str);
  }
  new_len = strlen(new_sub);

  count = 0;
  curr = str->str;
  while ((match = strstr(curr, old_sub)) != NULL) {
    count++;
    curr = match + old_len;
  }

  if (count == 0) {
    return string_clone(arena, str);
  }

  final_len = str->length + count * (new_len - old_len);
  st = (String *)arena_alloc(arena, sizeof(String) + final_len + 1);
  if (st == NULL) {
    return NULL;
  }

  dest = st->str;
  curr = str->str;
  while ((match = strstr(curr, old_sub)) != NULL) {
    size_t seg_len = (size_t)(match - curr);
    memcpy(dest, curr, seg_len);
    dest += seg_len;

    memcpy(dest, new_sub, new_len);
    dest += new_len;

    curr = match + old_len;
  }

  strcpy(dest, curr);
  st->length = final_len;

  return st;
}

Vector(String *) string_split(Arena *arena, String *str, const char *delim) {
  Vector(String *) parts = NULL;
  size_t delim_len, i;
  const char *curr;
  const char *match;
  String *part;

  if (arena == NULL || str == NULL || delim == NULL) {
    return NULL;
  }

  parts = vector_init(arena);
  delim_len = strlen(delim);

  if (delim_len == 0) {
    for (i = 0; i < str->length; i++) {
      part = string_from_len(arena, 1, &str->str[i]);
      append(parts, part);
    }
    return parts;
  }

  curr = str->str;
  while ((match = strstr(curr, delim)) != NULL) {
    part = string_from_len(arena, (size_t)(match - curr), curr);
    append(parts, part);
    curr = match + delim_len;
  }

  part = string_from(arena, curr);
  append(parts, part);

  return parts;
}

String *string_join(Arena *arena, Vector(String *) parts, const char *delim) {
  int n;
  int i;
  size_t delim_len;
  size_t total_len;
  String *st;
  char *p;

  if (arena == NULL || parts == NULL) {
    return NULL;
  }

  n = length(parts);
  if (n <= 0) {
    return string_from_len(arena, 0, "");
  }

  delim_len = (delim != NULL) ? strlen(delim) : 0;
  total_len = 0;

  for (i = 0; i < n; i++) {
    String *part = at(parts, i);
    if (part != NULL) {
      total_len += part->length;
    }
    if (i + 1 < n) {
      total_len += delim_len;
    }
  }

  st = (String *)arena_alloc(arena, sizeof(String) + total_len + 1);
  if (st == NULL) {
    return NULL;
  }

  p = st->str;
  for (i = 0; i < n; i++) {
    String *part = at(parts, i);
    if (part != NULL && part->length > 0) {
      memcpy(p, part->str, part->length);
      p += part->length;
    }
    if (i + 1 < n && delim_len > 0) {
      memcpy(p, delim, delim_len);
      p += delim_len;
    }
  }
  *p = '\0';
  st->length = total_len;

  return st;
}

String *string_read_file(Arena *arena, const char *filepath) {
  FILE *fp;
  long file_size;
  size_t read_bytes;
  String *st;

  if (arena == NULL || filepath == NULL) {
    return NULL;
  }

  fp = fopen(filepath, "rb");
  if (fp == NULL) {
    return NULL;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }

  file_size = ftell(fp);
  if (file_size < 0) {
    fclose(fp);
    return NULL;
  }
  rewind(fp);

  st = (String *)arena_alloc(arena, sizeof(String) + (size_t)file_size + 1);
  if (st == NULL) {
    fclose(fp);
    return NULL;
  }

  read_bytes = fread(st->str, 1, (size_t)file_size, fp);
  fclose(fp);

  st->str[read_bytes] = '\0';
  st->length = read_bytes;

  return st;
}

bool string_write_file(String *str, const char *filepath) {
  FILE *fp;
  size_t written;

  if (str == NULL || filepath == NULL) {
    return false;
  }

  fp = fopen(filepath, "wb");
  if (fp == NULL) {
    return false;
  }

  written = fwrite(str->str, 1, str->length, fp);
  fclose(fp);

  return (written == str->length);
}

StringView sv_from(const char *cstr) {
  StringView sv;
  if (cstr == NULL) {
    sv.data = "";
    sv.length = 0;
    return sv;
  }
  sv.data = cstr;
  sv.length = strlen(cstr);
  return sv;
}

StringView sv_from_len(const char *data, size_t len) {
  StringView sv;
  sv.data = data;
  sv.length = (data != NULL) ? len : 0;
  return sv;
}

StringView sv_from_string(String *str) {
  StringView sv;
  if (str == NULL) {
    sv.data = "";
    sv.length = 0;
    return sv;
  }
  sv.data = str->str;
  sv.length = str->length;
  return sv;
}

String *sv_to_string(Arena *arena, StringView sv) {
  return string_from_len(arena, sv.length, sv.data);
}

StringView sv_sub(StringView sv, size_t begin, size_t end) {
  if (begin < end && begin < sv.length && end <= sv.length) {
    return sv_from_len(sv.data + begin, end - begin);
  }
  return sv_from_len("", 0);
}

StringView sv_trim(StringView sv) {
  size_t start = 0;
  size_t end;

  while (start < sv.length &&
         (sv.data[start] == ' ' || sv.data[start] == '\t' ||
          sv.data[start] == '\n' || sv.data[start] == '\r')) {
    start++;
  }

  if (start == sv.length) {
    return sv_from_len("", 0);
  }

  end = sv.length - 1;
  while (end > start && (sv.data[end] == ' ' || sv.data[end] == '\t' ||
                         sv.data[end] == '\n' || sv.data[end] == '\r')) {
    end--;
  }

  return sv_from_len(sv.data + start, (end - start) + 1);
}

StringView sv_chop_by_delim(StringView *sv, char delim) {
  size_t i = 0;
  StringView result;

  if (sv == NULL || sv->length == 0) {
    return sv_from_len("", 0);
  }

  while (i < sv->length && sv->data[i] != delim) {
    i++;
  }

  result = sv_from_len(sv->data, i);

  if (i < sv->length) {
    sv->data += (i + 1);
    sv->length -= (i + 1);
  } else {
    sv->data += i;
    sv->length = 0;
  }

  return result;
}

bool sv_eq(StringView a, StringView b) {
  if (a.length != b.length) {
    return false;
  }
  if (a.length == 0) {
    return true;
  }
  return memcmp(a.data, b.data, a.length) == 0;
}

bool sv_eq_cstr(StringView sv, const char *cstr) {
  if (cstr == NULL) {
    return false;
  }
  return sv_eq(sv, sv_from(cstr));
}

bool sv_starts_with(StringView sv, const char *prefix) {
  size_t prefix_len;
  if (prefix == NULL) {
    return false;
  }
  prefix_len = strlen(prefix);
  if (prefix_len > sv.length) {
    return false;
  }
  return memcmp(sv.data, prefix, prefix_len) == 0;
}

bool sv_ends_with(StringView sv, const char *suffix) {
  size_t suffix_len;
  if (suffix == NULL) {
    return false;
  }
  suffix_len = strlen(suffix);
  if (suffix_len > sv.length) {
    return false;
  }
  return memcmp(sv.data + (sv.length - suffix_len), suffix, suffix_len) == 0;
}
