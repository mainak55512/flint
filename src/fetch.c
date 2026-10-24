#include <flint.h>

struct http_request_ctx {
	const char *url;
	bool done;
	int status_code;
	char *response_body;
	size_t response_len;
	Arena *arena;
};

static void fn(struct mg_connection *c, int ev, void *ev_data) {
	struct http_request_ctx *ctx = (struct http_request_ctx *)c->fn_data;
	if (ev == MG_EV_CONNECT) {
		const char *url = (const char *)ctx->url;
		struct mg_str host = mg_url_host(url);

		if (mg_url_is_ssl(url)) {
			struct mg_tls_opts opts = {.name = host, .skip_verification = true};
			mg_tls_init(c, &opts);
		}

		mg_printf(c,
				  "GET %s HTTP/1.1\r\n"
				  "Host: %.*s\r\n"
				  "User-Agent: mongoose\r\n"
				  "Accept: */*\r\n"
				  "Connection: close\r\n\r\n",
				  mg_url_uri(url), (int)host.len, host.buf);

	} else if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;

		ctx->status_code = mg_http_status(hm);
		ctx->response_len = hm->body.len;
		ctx->response_body = (char *)arena_alloc(ctx->arena, hm->body.len + 1);

		if (ctx->response_body != NULL) {
			memcpy(ctx->response_body, hm->body.buf, hm->body.len);
			ctx->response_body[hm->body.len] = '\0';
		}

		c->is_closing = 1;
		ctx->done = true;
	} else if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
		ctx->done = true;
	}
}

Sync_config *get_remote_composition(Arena *arena, char *repo_name) {
	mg_log_set(MG_LL_NONE);
	mg_boot_timestamp_ms = (uint64_t)time(NULL) * 1000 - mg_millis();
	struct mg_mgr mgr;

	char *url = string(string_concat_cstr(
		arena, 3, "https://mainak55512.github.io/flint-cherts/packages/",
		repo_name, "/"));

	// int url_len = strlen(url);

	mg_mgr_init(&mgr);

	struct http_request_ctx ctx = {.url = url,
								   .done = false,
								   .status_code = 0,
								   .response_body = NULL,
								   .response_len = 0,
								   .arena = arena};

	if (mg_http_connect(&mgr, ctx.url, fn, &ctx) == NULL) {
		fprintf(stderr, "Failed to initiate connection\n");
		mg_mgr_free(&mgr);
		return NULL;
	}

	while (!ctx.done) {
		mg_mgr_poll(&mgr, 50);
	}

	if (ctx.response_body != NULL) {
		yyjson_doc *doc = yyjson_read(ctx.response_body, ctx.response_len, 0);

		if (!doc) {
			fprintf(stderr, "Failed to parse JSON\n");
			mg_mgr_free(&mgr);
			return NULL;
		}
		yyjson_val *root = yyjson_doc_get_root(doc);
		yyjson_val *dependency = yyjson_obj_get(root, repo_name);

		Sync_config *sync = read_sync_config(arena, dependency);

		yyjson_doc_free(doc);
		mg_mgr_free(&mgr);
		return sync;

	} else {
		printf("Request failed or no body received. Status: %d\n",
			   ctx.status_code);
	}

	mg_mgr_free(&mgr);
	return NULL;
}
