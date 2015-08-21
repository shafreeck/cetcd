# Cetcd is a C client for etcd

## Status
 cetcd is on active development. It aims to be used in production environment and to supply full features of etcd.
 **Any issues or pull requests are welcome!**

## Deps 
 cetcd use [sds](https://github.com/antirez/sds) as a dynamic string utility.  It is licensed in sds/LICENSE.
 sds is interaged in cetcd's source code, so you don't have to install it before.

 [yajl](https://github.com/lloyd/yajl) is a powerful json stream parsing libaray. We use the stream apis to 
 parse response from cetcd. It is already integrated as a third-party dependency, so you are not necessary to
 install it before.

 [curl](http://curl.haxx.se/download.html) is required to issue HTTP requests in cetcd

## Install

Install curl if needed
on Ubuntu
```
apt-get install libcurl4-openssl-dev
```
or on CentOS
```
yum install libcurl-devel
```
then
 ```
 make 
 make install
 ```
 It default installs to /usr/local.

 Use 
 ```
 make install prefix=/path
 ```
 to specify your custom path.

## Link
 Use `-lcetcd` to link the library

## Usage
cetcd_array is an expandable dynamic array. It is used to pass etcd cluster addresses, and return cetcd response nodes

### Create an array to store the etcd addresses
```
    cetcd_array addrs;

    cetcd_array_init(&addrs, 3);
    cetcd_array_append(&addrs, "127.0.0.1:2379");
    cetcd_array_append(&addrs, "127.0.0.1:2378");
    cetcd_array_append(&addrs, "127.0.0.1:2377");
```

cetcd_client is a context cetcd uses to issue requests, you should init it with etcd addresses
### Init the cetcd_client
```
    cetcd_client cli;
    cetcd_client_init(&cli, &addrs);
```

Then you can issue an cetcd request which reply with an cetcd response
### List a directory
```
    cetcd_response *resp;
    resp = cetcd_lsdir(&cli, "/radar/service", 1, 1);
    if(resp->err) {
        printf("error :%d, %s (%s)\n", resp->err->ecode, resp->err->message, resp->err->cause);
    }
    cetcd_response_print(resp);
    cetcd_response_release(resp);
```

### Clean all resources
```
    cetcd_array_destory(&addrs);
    cetcd_client_destroy(&cli);
```
See [examples/cetcdget.c](https://github.com/shafreeck/cetcd/blob/master/examples/cetcdget.c) for more detailes
