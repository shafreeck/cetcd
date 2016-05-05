CFLAGS=-g -O0 -Wall -I ../third-party/build/include
LDFLAGS=-lcurl -lpthread
all:cetcd_get sync cetcd_lsdir multi_watch
cetcd_get:cetcd_get.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
cetcd_lsdir:cetcd_lsdir.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
multi_watch:multi_watch.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
sync:sync.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o

clean:
	rm -f cetcd_get cetcd_lsdir sync
