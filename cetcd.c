#include "cetcd.h"
#include "cetcd_json_parser.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <curl/curl.h>
#include <yajl/yajl_parse.h>
#include <sys/select.h>
#include <pthread.h>
static const char *http_method[] = {
    "GET", 
    "POST",
    "PUT",
    "DELETE",
    "HEAD",
    "OPTION"
};
typedef struct cetcd_response_parser_t {
    int st;
    int http_status;
    cetcd_string buf;
    cetcd_response *resp;
    yajl_parser_context ctx;
    yajl_handle json;
}cetcd_response_parser;
static const char *cetcd_event_action[] = {
    "set",
    "get",
    "update",
    "create",
    "delete",
    "expire"
};

void cetcd_client_init(cetcd_client *cli, cetcd_array *addresses) {
    curl_global_init(CURL_GLOBAL_ALL);
    srand(time(0));

    cli->keys_space =   "v2/keys";
    cli->stat_space =   "v2/stat";
    cli->member_space = "v2/members";
    cli->curl = curl_easy_init();
    cli->addresses = cetcd_array_shuffle(addresses);
    cli->picked = rand() % (cetcd_array_size(cli->addresses));

    cli->settings.verbose = 0;
    cli->settings.connect_timeout = 1;
    cli->settings.read_timeout = 1;  /*not used now*/
    cli->settings.write_timeout = 1; /*not used now*/

    cetcd_array_init(&cli->watchers, 10);

    curl_easy_setopt(cli->curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(cli->curl, CURLOPT_TCP_KEEPINTVL, 1L); /*the same as go-etcd*/
    curl_easy_setopt(cli->curl, CURLOPT_USERAGENT, "cetcd");
    curl_easy_setopt(cli->curl, CURLOPT_POSTREDIR, 3L);     /*post after redirecting*/
}
cetcd_client *cetcd_client_create(cetcd_array *addresses){
    cetcd_client *cli;

    cli = calloc(1, sizeof(cetcd_client));
    cetcd_client_init(cli, addresses);
    return cli;
}
void cetcd_client_destroy(cetcd_client *cli) {
    curl_easy_cleanup(cli->curl);
    curl_global_cleanup();
    cetcd_array_destory(&cli->watchers);
}
void cetcd_client_release(cetcd_client *cli){
    if (cli) {
        cetcd_client_destroy(cli);
        free(cli);
    }
}

size_t cetcd_parse_response(char *ptr, size_t size, size_t nmemb, void *userdata);

cetcd_watcher *cetcd_watcher_create(cetcd_string key, uint64_t index,
        int recursive, int once, watcher_callback callback, void *userdata) {
    cetcd_watcher *watcher;

    watcher = calloc(1, sizeof(cetcd_watcher));
    watcher->key = sdsnew(key);
    watcher->index = index;
    watcher->recursive = recursive;
    watcher->once = once;
    watcher->callback = callback;
    watcher->userdata = userdata;
    watcher->curl = curl_easy_init();

    watcher->parser = calloc(1, sizeof(cetcd_response_parser));
    watcher->parser->st = 0;
    watcher->parser->buf = sdsempty();
    watcher->parser->resp = calloc(1, sizeof(cetcd_response));

    watcher->array_index = -1;

    return watcher;
}
void cetcd_watcher_release(cetcd_watcher *watcher) {
    if (watcher) {
        if (watcher->key) {
            sdsfree(watcher->key);
        }
        if (watcher->curl) {
            curl_easy_cleanup(watcher->curl);
        }
        if (watcher->parser) {
            sdsfree(watcher->parser->buf);
            cetcd_response_release(watcher->parser->resp);
            free(watcher->parser);
        }
        free(watcher);
    }
}
static cetcd_string cetcd_watcher_build_url(cetcd_client *cli, cetcd_watcher *watcher) {
    cetcd_string url;
    url = sdscatprintf(sdsempty(), "http://%s/%s%s?wait=true", (cetcd_string)cetcd_array_get(cli->addresses, cli->picked),
            cli->keys_space, watcher->key);
    if (watcher->index) {
        url = sdscatprintf(url, "&waitIndex=%lu", watcher->index);
    }
    if (watcher->recursive) {
        url = sdscatprintf(url, "&recursive=true");
    }
    return url;
}
int cetcd_add_watcher(cetcd_client *cli, cetcd_watcher *watcher) {
    cetcd_array *watchers;
    cetcd_watcher *w;
    watchers = &cli->watchers;
    cetcd_string url;

    url = cetcd_watcher_build_url(cli, watcher);
    curl_easy_setopt(watcher->curl,CURLOPT_URL, url);
    sdsfree(url);

    watcher->attempts = cetcd_array_size(cli->addresses);

    curl_easy_setopt(watcher->curl, CURLOPT_CONNECTTIMEOUT, cli->settings.connect_timeout);
    curl_easy_setopt(watcher->curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(watcher->curl, CURLOPT_TCP_KEEPINTVL, 1L); /*the same as go-etcd*/
    curl_easy_setopt(watcher->curl, CURLOPT_USERAGENT, "cetcd");
    curl_easy_setopt(watcher->curl, CURLOPT_POSTREDIR, 3L);     /*post after redirecting*/
    curl_easy_setopt(watcher->curl, CURLOPT_VERBOSE, cli->settings.verbose); 

    curl_easy_setopt(watcher->curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(watcher->curl, CURLOPT_WRITEFUNCTION, cetcd_parse_response);
    curl_easy_setopt(watcher->curl, CURLOPT_WRITEDATA, watcher->parser);
    curl_easy_setopt(watcher->curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(watcher->curl, CURLOPT_FOLLOWLOCATION, 1L);
    /* We use an array to store watchers. It will cause holes when remove some watchers.
     * watcher->array_index is used to reset to the original hole if the watcher was deleted before.
     * */
    if (watcher->array_index == -1) {
        cetcd_array_append(watchers, watcher);
        watcher->array_index = cetcd_array_size(watchers) - 1;
    } else {
        w = cetcd_array_get(watchers, watcher->array_index);
        if (w) {
            cetcd_watcher_release(w);
        }
        cetcd_array_set(watchers, watcher->array_index, watcher);
    }
    return 1;
}
int cetcd_del_watcher(cetcd_client *cli, cetcd_watcher *watcher) {
    size_t index;
    index = watcher->array_index;
    if (watcher && index > 0) {
        cetcd_array_set(&cli->watchers, index, NULL);
        cetcd_watcher_release(watcher);
    }
    return 1;
}
static int cetcd_reap_watchers(cetcd_client *cli, CURLM *mcurl) {
    int     added, ignore;
    CURLMsg *msg;
    CURL    *curl;
    cetcd_string url;
    cetcd_watcher *watcher;
    cetcd_response *resp;
    added = 0;
    while ((msg = curl_multi_info_read(mcurl, &ignore)) != NULL) {
        if (msg->msg == CURLMSG_DONE) {
            curl = msg->easy_handle;
            curl_easy_getinfo(curl, CURLINFO_PRIVATE, &watcher);

            resp = watcher->parser->resp;
            if (msg->data.result != CURLE_OK) {
                /*try next in round-robin ways*/
                /*FIXME There is a race condition if multiple watchers failed*/
                if (watcher->attempts) {
                    cli->picked = (cli->picked+1)%(cetcd_array_size(cli->addresses));
                    url = cetcd_watcher_build_url(cli, watcher);
                    curl_easy_setopt(watcher->curl, CURLOPT_URL, url);
                    sdsfree(url);
                    curl_multi_remove_handle(mcurl, curl);
                    curl_multi_add_handle(mcurl, curl);
                    /*++added;
                     *watcher->attempts --;
                     */
                    continue;
                } else {
                    resp->err = calloc(1, sizeof(cetcd_error));
                    resp->err->ecode = error_cluster_failed;
                    resp->err->message = sdsnew("cetcd_reap_watchers: all cluster servers failed.");
                }
            }
            if (watcher->callback) {
                watcher->callback(watcher->userdata, resp);
                if (resp->err) {
                    curl_multi_remove_handle(mcurl, curl);
                    cetcd_watcher_release(watcher);
                    break;
                }
                cetcd_response_release(resp);
                watcher->parser->resp = NULL; /*surpress it be freed again by cetcd_watcher_release*/
            }
            if (!watcher->once) {
                sdsclear(watcher->parser->buf);
                watcher->parser->st = 0;
                watcher->parser->resp = calloc(1, sizeof(cetcd_response));
                if (watcher->index) {
                    watcher->index ++;
                    url = cetcd_watcher_build_url(cli, watcher);
                    curl_easy_setopt(watcher->curl, CURLOPT_URL, url);
                    sdsfree(url);
                }
                curl_multi_remove_handle(mcurl, curl);
                curl_multi_add_handle(mcurl, curl);
                ++added;
                continue;
            }
            curl_multi_remove_handle(mcurl, curl);
            cetcd_watcher_release(watcher);
        }
    }
    return added;
}
int cetcd_multi_watch(cetcd_client *cli) {
    int           i, count;
    int           maxfd, left, added;
    long          timeout;
    long          backoff, backoff_max;
    fd_set        r, w, e;
    cetcd_array   *watchers;
    cetcd_watcher *watcher;
    CURLM         *mcurl;

    struct timeval tv;

    mcurl = curl_multi_init();
    watchers = &cli->watchers;
    count = cetcd_array_size(watchers);
    for (i = 0; i < count; ++i) {
        watcher = cetcd_array_get(watchers, i);
        curl_easy_setopt(watcher->curl, CURLOPT_PRIVATE, watcher);
        curl_multi_add_handle(mcurl, watcher->curl);
    }
    backoff = 100; /*100ms*/
    backoff_max = 1000; /*1 sec*/
    for(;;) {
        curl_multi_perform(mcurl, &left);
        if (left) {
            FD_ZERO(&r);
            FD_ZERO(&w);
            FD_ZERO(&e);
            curl_multi_fdset(mcurl, &r, &w, &e, &maxfd);
            curl_multi_timeout(mcurl, &timeout);
            if (timeout == -1) {
                timeout = 100; /*wait for 0.1 seconds*/
            }
            tv.tv_sec = timeout/1000;
            tv.tv_usec = (timeout%1000)*1000;

            /*TODO handle errors*/
            select(maxfd+1, &r, &w, &e, &tv);
        }
        added = cetcd_reap_watchers(cli, mcurl);
        if (added == 0 && left == 0) {
        /* It will call curl_multi_perform immediately if:
         * 1. left is 0
         * 2. a new attempt should be issued
         * It is expected to sleep a mount time between attempts.
         * So we fix this by increasing added counter only
         * when a new request should be issued.
         * When added is 0, maybe there are retring requests or nothing.
         * Either situations should wait before issuing the request.
         * */
            if (backoff < backoff_max) {
                backoff = 2 * backoff;
            } else {
                backoff = backoff_max;
            }
            tv.tv_sec = backoff/1000;
            tv.tv_usec = (backoff%1000) * 1000;
            select(1, 0, 0, 0, &tv);
        }
    }
    curl_multi_cleanup(mcurl);
    return count;
}
int cetcd_multi_watch_async(cetcd_client *cli) {
    pthread_t thread;
    return pthread_create(&thread, NULL, (void *(*)(void *))cetcd_multi_watch, cli);
}

cetcd_response *cetcd_cluster_request(cetcd_client *cli, cetcd_request *req);
cetcd_response *cetcd_get(cetcd_client *cli, cetcd_string key) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_GET;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}

cetcd_response *cetcd_get_recursive(cetcd_client *cli, cetcd_string key, int sort) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_GET;
    req.uri = sdscatprintf(sdsempty(), "%s%s?recursive=true", cli->keys_space, key);
    if (sort) {
        req.uri = sdscatprintf(req.uri, "&sort=true");
    }
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}


cetcd_response *cetcd_set(cetcd_client *cli, cetcd_string key,
        cetcd_string value, uint64_t ttl) {
    cetcd_request req;
    cetcd_response *resp;
    cetcd_string params;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_PUT;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    params = sdscatprintf(sdsempty(), "value=%s", value);
    if (ttl) {
        params = sdscatprintf(params, "&ttl=%lu", ttl);
    }
    req.data = params;
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    sdsfree(params);
    return resp;
}

cetcd_response *cetcd_mkdir(cetcd_client *cli, cetcd_string key, uint64_t ttl){
    cetcd_request req;
    cetcd_response *resp;
    cetcd_string params;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_PUT;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    params = sdscatprintf(sdsempty(), "dir=true&prevExist=false");
    if (ttl) {
        params = sdscatprintf(params, "&ttl=%lu", ttl);
    }
    req.data = params;
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    sdsfree(params);
    return resp;
}
cetcd_response *cetcd_setdir(cetcd_client *cli, cetcd_string key, uint64_t ttl){
    cetcd_request req;
    cetcd_response *resp;
    cetcd_string params;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_PUT;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    params = sdscatprintf(sdsempty(), "dir=true");
    if (ttl) {
        params = sdscatprintf(params, "&ttl=%lu", ttl);
    }
    req.data = params;
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    sdsfree(params);
    return resp;
}
/*
cetcd_response *cetcd_refresh(cetcd_client *cli, cetcd_string key, uint64_t ttl) {
}
*/

cetcd_response *cetcd_delete(cetcd_client *cli, cetcd_string key) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_DELETE;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}


cetcd_response *cetcd_watch(cetcd_client *cli, cetcd_string key, uint64_t index) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_GET;
    req.uri = sdscatprintf(sdsempty(), "%s%s?wait=true&waitIndex=%lu", cli->keys_space, key, index);
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}

cetcd_response *cetcd_watch_recursive(cetcd_client *cli, cetcd_string key, uint64_t index) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_GET;
    req.uri = sdscatprintf(sdsempty(), "%s%s?wait=true&recursive=true&waitIndex=%lu", cli->keys_space, key, index);
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}

cetcd_response *cetcd_cmp_and_swap(cetcd_client *cli, cetcd_string key, cetcd_string value, cetcd_string prev, uint64_t ttl) {
    cetcd_request req;
    cetcd_response *resp;
    cetcd_string params;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_PUT;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    params = sdscatprintf(sdsempty(), "value=%s&prevValue=%s", value, prev);
    if (ttl) {
        params = sdscatprintf(params, "&ttl=%lu", ttl);
    }
    req.data = params;
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    sdsfree(params);
    return resp;
}
cetcd_response *cetcd_cmp_and_swap_by_index(cetcd_client *cli, cetcd_string key, cetcd_string value, uint64_t prev, uint64_t ttl) {
    cetcd_request req;
    cetcd_response *resp;
    cetcd_string params;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_PUT;
    req.uri = sdscatprintf(sdsempty(), "%s%s", cli->keys_space, key);
    params = sdscatprintf(sdsempty(), "value=%s&prevIndex=%lu", value, prev);
    if (ttl) {
        params = sdscatprintf(params, "&ttl=%lu", ttl);
    }
    req.data = params;
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    sdsfree(params);
    return resp;
}
cetcd_response *cetcd_cmp_and_delete(cetcd_client *cli, cetcd_string key, cetcd_string prev) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_DELETE;
    req.uri = sdscatprintf(sdsempty(), "%s%s?prevValue=%s", cli->keys_space, key, prev);
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}
cetcd_response *cetcd_cmp_and_delete_by_index(cetcd_client *cli, cetcd_string key, uint64_t prev) {
    cetcd_request req;
    cetcd_response *resp;

    memset(&req, 0, sizeof(cetcd_request));
    req.method = HTTP_DELETE;
    req.uri = sdscatprintf(sdsempty(), "%s%s?prevIndex=%lu", cli->keys_space, key, prev);
    resp = cetcd_cluster_request(cli, &req);
    sdsfree(req.uri);
    return resp;
}
void cetcd_node_release(cetcd_response_node *node) {
    int i, count;
    cetcd_response_node *n;
    if (node->nodes) {
        count = cetcd_array_size(node->nodes);
        for (i = 0; i < count; ++i) {
            n = cetcd_array_get(node->nodes, i);
            cetcd_node_release(n);
        }
        cetcd_array_release(node->nodes);
    }
    if (node->key) {
        sdsfree(node->key);
    }
    if (node->value) {
        sdsfree(node->value);
    }
    free(node);
}
void cetcd_response_release(cetcd_response *resp) {
    if(resp) {
        if (resp->err) {
            cetcd_error_release(resp->err);
            resp->err = NULL;
        }
        if (resp->node) {
            cetcd_node_release(resp->node);
        }
        if (resp->prev_node) {
            cetcd_node_release(resp->prev_node);
        }
        free(resp);
    }
}
void cetcd_error_release(cetcd_error *err) {
    if (err) {
        if (err->message) {
            sdsfree(err->message);
        }
        if (err->cause) {
            sdsfree(err->cause);
        }
        free(err);
    }
}
static void cetcd_node_print(cetcd_response_node *node) {
    int i, count;
    cetcd_response_node *n;
    if (node) {
        printf("Node TTL: %lu\n", node->ttl);
        printf("Node ModifiedIndex: %lu\n", node->modified_index);
        printf("Node CreatedIndex: %lu\n", node->created_index);
        printf("Node Key: %s\n", node->key);
        printf("Node Value: %s\n", node->value);
        printf("Node Dir: %d\n", node->dir);
        printf("\n");
        if (node->nodes) {
            count = cetcd_array_size(node->nodes);
            for (i = 0; i < count; ++i) {
                n = cetcd_array_get(node->nodes, i);
                cetcd_node_print(n);
            }
        }
    }
}
void cetcd_response_print(cetcd_response *resp) {
    if (resp->err) {
        printf("Error Code:%d\n", resp->err->ecode);
        printf("Error Message:%s\n", resp->err->message);
        printf("Error Cause:%s\n", resp->err->cause);
        return;
    }
    printf("Etcd Action:%s\n", cetcd_event_action[resp->action]);
    printf("Etcd Index:%lu\n", resp->etcd_index);
    printf("Raft Index:%lu\n", resp->raft_index);
    printf("Raft Term:%lu\n", resp->raft_term);
    if (resp->node) {
        printf("-------------Node------------\n");
        cetcd_node_print(resp->node);
    }
    if (resp->prev_node) {
        printf("-----------prevNode------------\n");
        cetcd_node_print(resp->prev_node);
    }
}

size_t cetcd_parse_response(char *ptr, size_t size, size_t nmemb, void *userdata) {
    int len, i;
    char *key, *val;
    cetcd_response *resp;
    cetcd_response_parser *parser;
    yajl_status status;

    enum resp_parser_st {
        request_line_start_st,
        request_line_end_st,
        request_line_http_status_start_st,
        request_line_http_status_st,
        request_line_http_status_end_st,
        header_key_start_st,
        header_key_st,
        header_key_end_st,
        header_val_start_st,
        header_val_st,
        header_val_end_st,
        blank_line_st,
        json_start_st,
        json_end_st,
        response_discard_st
    };
    /* Headers we are interested in:
     * X-Etcd-Index: 14695
     * X-Raft-Index: 672930
     * X-Raft-Term: 12
     * */

    parser = userdata;
    resp = parser->resp;
    len = size * nmemb;
    for (i = 0; i < len; ++i) {
        if (parser->st == request_line_start_st) {
            if (ptr[i] == ' ') {
                parser->st = request_line_http_status_start_st;
            }
            continue;
        }
        if (parser->st == request_line_end_st) {
            if (ptr[i] == '\n') {
                parser->st = header_key_start_st;
            }
            continue;
        }
        if (parser->st == request_line_http_status_start_st) {
            parser->buf = sdscatlen(parser->buf, ptr+i, 1);
            parser->st = request_line_http_status_st;
            continue;
        }
        if (parser->st == request_line_http_status_st) {
            if (ptr[i] == ' ') {
                parser->st = request_line_http_status_end_st;
            } else {
                parser->buf = sdscatlen(parser->buf, ptr+i, 1);
                continue;
            }
        }
        if (parser->st == request_line_http_status_end_st) {
            val = parser->buf;
            parser->http_status = atoi(val);
            sdsclear(parser->buf);
            parser->st = request_line_end_st;
            continue;
        }
        if (parser->st == header_key_start_st) {
            if (ptr[i] == '\r') {
                ++i;
            }
            if (ptr[i] == '\n') {
                parser->st = blank_line_st;
                if (parser->http_status >= 300 && parser->http_status < 400) {
                    /*this is a redirection, restart the state machine*/
                    parser->st = request_line_start_st;
                    break;
                }
                continue;
            }
            parser->st = header_key_st;
        }
        if (parser->st == header_key_st) {
            parser->buf = sdscatlen(parser->buf, ptr+i, 1);
            if (ptr[i] == ':') {
                parser->st = header_key_end_st;
            } else {
                continue;
            }
        }
        if (parser->st == header_key_end_st) {
            parser->st = header_val_start_st;
            continue;
        }
        if (parser->st == header_val_start_st) {
            if (ptr[i] == ' ') {
                continue;
            }
            parser->st = header_val_st;
        }
        if (parser->st == header_val_st) {
            if (ptr[i] == '\r') {
                ++i;
            }
            if (ptr[i] == '\n') {
                parser->st = header_val_end_st;
            } else {
                parser->buf = sdscatlen(parser->buf, ptr+i, 1);
                continue;
            }
        }
        if (parser->st == header_val_end_st) {
            parser->st = header_key_start_st;
            int count = 0;
            sds *kvs = sdssplitlen(parser->buf, sdslen(parser->buf), ":", 1, &count);
            sdsclear(parser->buf);
            if (count < 2) {
                sdsfreesplitres(kvs, count);
                continue;
            }
            key = kvs[0];
            val = kvs[1];
            if (strncmp(key, "X-Etcd-Index", sizeof("X-Etcd-Index")-1) == 0) {
                resp->etcd_index = atoi(val);
            } else if (strncmp(key, "X-Raft-Index", sizeof("X-Raft-Index")-1) == 0) {
                resp->raft_index = atoi(val);
            } else if (strncmp(key, "X-Raft-Term", sizeof("X-Raft-Term")-1) == 0) {
                resp->raft_term = atoi(val);
            }
            sdsfreesplitres(kvs, count);
            continue;
        }
        if (parser->st == blank_line_st) {
            if (ptr[i] != '{') {
                /*not a json response, discard*/
                parser->st = response_discard_st;
                if (resp->err == NULL) {
                    resp->err = calloc(1, sizeof(cetcd_error));
                    resp->err->ecode = error_response_parsed_failed;
                    resp->err->message = sdsnew("not a json response");
                    resp->err->cause = sdsnewlen(ptr, len);
                }
                continue;
            }
            parser->st = json_start_st;
            cetcd_array_init(&parser->ctx.keystack, 10);
            cetcd_array_init(&parser->ctx.nodestack, 10);
            if (parser->http_status != 200 && parser->http_status != 201) {
                resp->err = calloc(1, sizeof(cetcd_error));
                parser->ctx.userdata = resp->err;
                parser->json = yajl_alloc(&error_callbacks, 0, &parser->ctx);
            } else {
                parser->ctx.userdata = resp;
                parser->json = yajl_alloc(&callbacks, 0, &parser->ctx);
            }
        }
        if (parser->st == json_start_st) {
            if (yajl_status_ok != yajl_parse(parser->json, (const unsigned char *)ptr + i, len - i)) {
                parser->st = json_end_st;
            } else {
                parser->st = response_discard_st;
                yajl_free(parser->json);
                cetcd_array_destory(&parser->ctx.keystack);
                cetcd_array_destory(&parser->ctx.nodestack);
            }
        }
        if (parser->st == json_end_st) {
            status = yajl_complete_parse(parser->json);
            yajl_free(parser->json);
            cetcd_array_destory(&parser->ctx.keystack);
            cetcd_array_destory(&parser->ctx.nodestack);
            /*parse failed, TODO set error message*/
            if (status != yajl_status_ok) {
                if (resp->err == NULL) {
                    resp->err = calloc(1, sizeof(cetcd_error));
                    resp->err->ecode = error_response_parsed_failed;
                    resp->err->message = sdsnew("http response is invalid json format");
                    resp->err->cause = sdsnewlen(ptr, len);
                }
                return 0;
            }
            break;
        }
        if (parser->st == response_discard_st) {
            return len;
        }
    }
    return len;
}
cetcd_response *cetcd_send_request(CURL *curl, cetcd_request *req) {
    CURLcode res;
    cetcd_response_parser parser;
    cetcd_response *resp;

    resp = calloc(1, sizeof(cetcd_response));
    parser.resp = resp;
    parser.st = 0; /*0 should be the start state of the state machine*/
    parser.buf = sdsempty();

    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, http_method[req->method]);
    if (req->method == HTTP_PUT) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->data);
    } else {
        /* We must clear post fields here:
         * We reuse the curl handle for all HTTP methods.
         * CURLOPT_POSTFIELDS would be set when issue a PUT request.
         * The field  pointed to the freed req->data. It would be 
         * reused by next request.
         * */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    }
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cetcd_parse_response);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, req->cli->settings.verbose); 
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, req->cli->settings.connect_timeout);

    res = curl_easy_perform(curl);

    sdsfree(parser.buf);
    if (res != CURLE_OK) {
        if (resp->err == NULL) {
            resp->err = calloc(1, sizeof(cetcd_error));
            resp->err->ecode = error_send_request_failed;
            resp->err->message = sdsnew(curl_easy_strerror(res));
            resp->err->cause = sdsdup(req->url);
        }
        return resp;
    }
    return resp;
}
/*
 * cetcd_cluster_request tries to request the whole cluster. It round-robin to next server if the request failed
 * */
cetcd_response *cetcd_cluster_request(cetcd_client *cli, cetcd_request *req) {
    int i;
    size_t count;
    cetcd_string url;
    cetcd_error *err;
    cetcd_response *resp;

    err = NULL;
    resp = NULL;
    count = cetcd_array_size(cli->addresses);
    
    for(i = 0; i < count; ++i) {
        url = sdscatprintf(sdsempty(), "http://%s/%s", (cetcd_string)cetcd_array_get(cli->addresses, cli->picked), req->uri);
        req->url = url;
        req->cli = cli;
        resp = cetcd_send_request(cli->curl, req);
        sdsfree(url);

        if(resp && resp->err && resp->err->ecode == error_send_request_failed) {
            if (i == count-1) {
                break;
            }
            /*try next*/
            cli->picked = (cli->picked + 1) % count;
            cetcd_response_release(resp);
            resp = NULL;
        } else {
            /*got response, return*/
            return resp;
        }
    }
    /*the whole cluster failed*/
    if (resp) {
        if(resp->err) {
            err = resp->err; /*remember last error*/
        }
        resp->err = calloc(1, sizeof(cetcd_error));
        resp->err->ecode = error_cluster_failed;
        resp->err->message = sdsnew("etcd_do_request: all cluster servers failed.");
        if (err) {
           resp->err->message = sdscatprintf(resp->err->message, " last error: %s", err->message);
           cetcd_error_release(err);
        }
        resp->err->cause = sdsdup(req->uri);
    }
    return resp;
}
