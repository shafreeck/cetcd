#include "../cetcd.h"
#include <signal.h>
#include <unistd.h>

int quit = 0;
void sighandler(int sig) {
    quit = 1;
}

int watch(void *userdata, cetcd_response *resp) {
    cetcd_response_print(resp);
    return 0;
}
int main(int argc, char *argv[]) {
    cetcd_client cli;
    cetcd_array addrs;

    cetcd_array watchers;
    cetcd_watch_id wid;

    /*initialize the address array*/
    cetcd_array_init(&addrs, 3);
    /*append the address of etcd*/
    cetcd_array_append(&addrs, "http://127.0.0.1:2379");

    /*initialize the cetcd_client*/
    cetcd_client_init(&cli, &addrs);
    //cli.settings.verbose = 1;
    
    signal(SIGINT, sighandler);

    cetcd_array_init(&watchers, 3);
    cetcd_add_watcher(&watchers, cetcd_watcher_create(&cli, "/test/watch", 0, 1, 1, watch, NULL));
    cetcd_add_watcher(&watchers, cetcd_watcher_create(&cli, "/test/watch", 0, 1, 1, watch, NULL));
    cetcd_add_watcher(&watchers, cetcd_watcher_create(&cli, "/test/watch", 0, 1, 1, watch, NULL));
    wid = cetcd_multi_watch_async(&cli, &watchers);
    while(!quit) {
        usleep(10000);
    }
    cetcd_multi_watch_async_stop(&cli, wid);
    cetcd_client_destroy(&cli);
    cetcd_array_destroy(&addrs);
    cetcd_array_destroy(&watchers);

    return 0;
}
