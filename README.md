# cetcd is C client of etcd

## Deps 
 cetcd use [sds](https://github.com/antirez/sds) as a dynamic string utility.  It is licensed in sds/LICENSE.
 sds is interaged in cetcd's source code, so you don't have to install it before.

 [yajl](https://github.com/lloyd/yajl) is a powerfull json stream parsing libaray. We use the stream apis to 
 parse response from cetcd

## Install
 cetcd parses json stream base on [yajl](https://github.com/lloyd/yajl), so you should install yajl first.

 then 
 ```
 make 
 make install
 ```
 It default installs to /usr/local.

 Use 
 ```
 make intall prefix=/path
 ```
 to specify your custom path.

## Link
 Use `-lcetcd` to link the library
