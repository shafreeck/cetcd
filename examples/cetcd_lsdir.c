#include "../cetcd.h"

int main(int argc, char *argv[]) {
    cetcd_client cli;
    cetcd_response *resp;
    cetcd_array addrs;

    cetcd_array_init(&addrs, 3);
    cetcd_array_append(&addrs, "http://127.0.0.1:2379");

    cetcd_client_init(&cli, &addrs);

    resp = cetcd_lsdir(&cli, "/radar/test", 1, 1);
    if(resp->err) {
        printf("error :%d, %s (%s)\n", resp->err->ecode, resp->err->message, resp->err->cause);
    }
    cetcd_response_print(resp);
    cetcd_response_release(resp);

    cetcd_array_destroy(&addrs);
    cetcd_client_destroy(&cli);
    return 0;
}
