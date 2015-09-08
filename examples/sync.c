#include "../cetcd.h"
/* Test about the sync member feature
 * */

int main(int argc, char *argv[]) {
    cetcd_client *cli;
    cetcd_array addrs;

    cetcd_array_init(&addrs, 2);
    cetcd_array_append(&addrs, "127.0.0.1:2379");

    cli = cetcd_client_create(&addrs);

    cetcd_client_sync_cluster(cli);

    cetcd_array_destroy(&addrs);
    cetcd_client_release(cli);
    return 0;
}
