#ifndef CETCD_CETCD_H
#define CETCD_CETCD_H
#include <curl/curl.h>
#include <stdint.h>
#include "sds/sds.h"
#include "cetcd_array.h"
typedef sds cetcd_string;

enum HTTP_METHOD {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_HEAD,
    HTTP_OPTION
};
enum ETCD_EVENT_ACTION {
    ETCD_SET,
    ETCD_GET,
    ETCD_UPDATE,
    ETCD_CREATE,
    ETCD_DELETE,
    ETCD_EXPIRE,
    ETCD_ACTION_MAX
};
/* etcd error codes range is [100, 500]
 * We use 1000+ as cetcd error codes;
 * */
#define error_response_parsed_failed 1000
#define error_send_request_failed    1001
#define error_cluster_failed         1002
typedef struct cetcd_error_t {
    int ecode;
    cetcd_string message;
    cetcd_string cause;
    uint64_t index;
}cetcd_error;
typedef struct cetcd_client_t {
    CURL  *curl;
    cetcd_error *err;
    cetcd_array watchers; /*curl watch handlers*/
    cetcd_array addresses;/*cluster addresses*/
    const char *keys_space;
    const char *stat_space;
    const char *member_space;
    int picked;
    struct {
        int      verbose;
        uint64_t ttl;
        uint64_t connect_timeout;
        uint64_t read_timeout;
        uint64_t write_timeout;
        cetcd_string user;
        cetcd_string password;
    } settings;

} cetcd_client;

typedef struct cetcd_request_t {
    int method;
    cetcd_string uri;
    cetcd_string url;
    cetcd_string data;
} cetcd_request;

typedef struct cetcd_response_node_t {
    cetcd_array *nodes; //struct etcd_response_node_t 
    cetcd_string key;
    cetcd_string value;
    int dir; /* 1 for true, and 0 for false */
    uint64_t expiration;
    int64_t  ttl;
    uint64_t modified_index;
    uint64_t created_index;

} cetcd_response_node;

typedef struct cetcd_reponse_t {
    cetcd_error *err;
    int action;
    struct cetcd_response_node_t *node;
    struct cetcd_response_node_t *prev_node;
    uint64_t etcd_index;
    uint64_t raft_index;
    uint64_t raft_term;
} cetcd_response;

struct cetcd_response_parser_t ;

typedef int (*watcher_callback) (void *userdata, cetcd_response *resp);
typedef struct cetcd_watcher_t {
    watcher_callback callback;
    struct cetcd_response_parser_t *parser;
    int attempts;
    int array_index; /*the index in array cli->wachers*/

    CURL         *curl;
    int          once;
    int          recursive;
    uint64_t     index;
    cetcd_string key;
    void         *userdata;
} cetcd_watcher;

cetcd_client* cetcd_clietn_create(cetcd_array addresses);
void          cetcd_client_init(cetcd_client *cli, cetcd_array addresses);
void          cetcd_client_destroy(cetcd_client *cli);
void          cetcd_client_free(cetcd_client *cli);

cetcd_response *cetcd_get(cetcd_client *cli, cetcd_string key);
cetcd_response *cetcd_get_recursive(cetcd_client *cli, cetcd_string key, int sort);

cetcd_response *cetcd_set(cetcd_client *cli, cetcd_string key,
        cetcd_string value, uint64_t ttl);
cetcd_response *cetcd_mkdir(cetcd_client *cli, cetcd_string key, uint64_t ttl);
cetcd_response *cetcd_refresh(cetcd_client *cli, cetcd_string key, uint64_t ttl);

cetcd_response *cetcd_delete(cetcd_client *cli, cetcd_string key);

cetcd_response *cetcd_watch(cetcd_client *cli, cetcd_string key, uint64_t index);
cetcd_response *cetcd_watch_recursive(cetcd_client *cli, cetcd_string key, uint64_t index);

cetcd_response *cetcd_cmp_and_swap(cetcd_client *cli, cetcd_string key, cetcd_string value,
        cetcd_string prev, uint64_t ttl);
cetcd_response *cetcd_cmp_and_swap_by_index(cetcd_client *cli, cetcd_string key, cetcd_string value,
        uint64_t prev, uint64_t ttl);
cetcd_response *cetcd_cmp_and_delete(cetcd_client *cli, cetcd_string key, cetcd_string prev);
cetcd_response *cetcd_cmp_and_delete_by_index(cetcd_client *cli, cetcd_string key, uint64_t prev);

cetcd_watcher *cetcd_watcher_create(cetcd_string key, uint64_t index,
        int recursive, int once, watcher_callback callback, void *userdata);
int cetcd_add_watcher(cetcd_client *cli, cetcd_watcher *watcher);
int cetcd_del_watcher(cetcd_client *cli, cetcd_watcher *watcher);
int cetcd_multi_watch(cetcd_client *cli);


void cetcd_response_print(cetcd_response *resp);
void cetcd_response_free(cetcd_response *resp);
void cetcd_error_free(cetcd_error *err);
#endif
