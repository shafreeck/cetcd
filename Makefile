CFLAGS= -Wall -O3 -fPIC
LDFLAGS=-lcurl -lyajl -lpthread
prefix=/usr/local

release:all

debug:CFLAGS= -g -O0 -Wall -Werror -fPIC
debug:all

all: libcetcd.so
clean:
	rm -f libcetcd.so *.o sds/*.o
install:all
	install -D libcetcd.so $(prefix)/lib/libcetcd.so
	install -D cetcd.h $(prefix)/include/cetcd.h
	install -D cetcd_array.h $(prefix)/include/cetcd_array.h
	install -D cetcd_json_parser.h $(prefix)/include/cetcd_json_parser.h
	install -D sds/sds.h $(prefix)/include/sds/sds.h

libcetcd.so: cetcd_array.o sds/sds.o cetcd.o
	gcc $(LDFLAGS) -shared -o libcetcd.so cetcd_array.o sds/sds.o cetcd.o
sds/sds.o:sds/sds.c sds/sds.h
cetcd_array.o:cetcd_array.c cetcd_array.h
cetcd.o:cetcd.c cetcd.h cetcd_json_parser.h
