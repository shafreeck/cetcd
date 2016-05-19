CFLAGS=-g -O0 -Wall -I ../third-party/build/include
LDFLAGS=-lcurl -lpthread
all:cetcd_get sync cetcd_lsdir multi_watch
cetcd_get:cetcd_get.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
	gcc -o $@ $^ $(LDFLAGS) $(CFLAGS)
cetcd_lsdir:cetcd_lsdir.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
	gcc -o $@ $^ $(LDFLAGS) $(CFLAGS)
multi_watch:multi_watch.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
	gcc -o $@ $^ $(LDFLAGS) $(CFLAGS)
sync:sync.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
	gcc -o $@ $^ $(LDFLAGS) $(CFLAGS)

clean:
	rm -f cetcd_get cetcd_lsdir sync multi_watch
