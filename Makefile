CFLAGS=-g -Wall -O0 -fPIC
LDFLAGS=-lcurl -lyajl -lpthread
prefix=/usr/local

all: libcetcd.so
clean:
	rm -f libcetcd.so *.o sds/*.o
install:
	install -S -d  libcetcd.so $(prefix)/lib

libcetcd.so: cetcd_array.o sds/sds.o cetcd.o
	gcc $(LDFLAGS) -shared -o libcetcd.so cetcd_array.o sds/sds.o cetcd.o
sds/sds.o:sds/sds.c sds/sds.h
cetcd_array.o:cetcd_array.c cetcd_array.h
cetcd.o:cetcd.c cetcd.h cetcd_json_parser.h
