#include "../cetcd.h"

int watcher_cb (void *userdata, cetcd_response *resp) {
    cetcd_response_print(resp);
    return 1;
}
int main(int argc, char *argv[]) {
    cetcd_client cli;
    cetcd_response *resp;
    cetcd_array addrs;

    cetcd_array_init(&addrs, 3);
    cetcd_array_append(&addrs, "https://127.0.0.1:2379");
    cetcd_array_append(&addrs, "http://127.0.0.1:2378");
    cetcd_array_append(&addrs, "127.0.0.1:2377");

    cetcd_client_init(&cli, &addrs);

    resp = cetcd_lsdir(&cli, "/radar/service", 1, 1);
    if(resp->err) {
        printf("error :%d, %s (%s)\n", resp->err->ecode, resp->err->message, resp->err->cause);
    }
    cetcd_response_print(resp);
    cetcd_response_release(resp);

    cetcd_array_destroy(&addrs);
    cetcd_client_destroy(&cli);
    return 0;
}
