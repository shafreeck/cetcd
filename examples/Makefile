CFLAGS=-g -O0 -Wall -I ../third-party/build/include
LDFLAGS=-lcurl -lpthread
all:cetcdget sync
cetcdget:cetcdget.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o
sync:sync.c ../cetcd.c ../cetcd_array.c ../sds/sds.c ../third-party/build/*.o

clean:
	rm -f cetcdget sync
