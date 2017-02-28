CFLAGS= -Wall -Wextra -fPIC -I third-party/build/include
LDFLAGS=-lcurl -lpthread
prefix=/usr/local

release:CFLAGS += -O3
release:all

debug:CFLAGS += -g -O0
debug:all

all: libcetcd.so
clean:
	rm -rf libcetcd.so *.o sds/*.o third-party/build
distclean:
	rm -rf libcetcd.so *.o sds/*.o third-party/*
install:all
	install -D libcetcd.so $(prefix)/lib/libcetcd.so
	install -D cetcd.h $(prefix)/include/cetcd.h
	install -D cetcd_array.h $(prefix)/include/cetcd_array.h
	install -D sds/sds.h $(prefix)/include/sds/sds.h

libcetcd.so: cetcd_array.o sds/sds.o cetcd.o
	$(CC) -shared -o libcetcd.so cetcd_array.o sds/sds.o cetcd.o third-party/build/*.o $(LDFLAGS)
sds/sds.o:sds/sds.c sds/sds.h
cetcd_array.o:cetcd_array.c cetcd_array.h
cetcd.o:cetcd.c cetcd.h cetcd_json_parser.h third-party/build/include/yajl/*.h third-party/build/*.o

# Processing third-party projects
third-party:third-party/yajl-2.1.0

third-party/yajl-2.1.0.tar.gz:
	mkdir -p third-party
	curl -L https://github.com/lloyd/yajl/archive/2.1.0.tar.gz -o third-party/yajl-2.1.0.tar.gz
third-party/yajl-2.1.0:third-party/yajl-2.1.0.tar.gz
	tar -zxf third-party/yajl-2.1.0.tar.gz -C third-party
	cd third-party/yajl-2.1.0/ && ./configure
	cd third-party/yajl-2.1.0/ && make

third-party/build/*.o:third-party/yajl-2.1.0
	mkdir -p third-party/build
	cp third-party/yajl-2.1.0/build/src/CMakeFiles/yajl.dir/*.o third-party/build/
third-party/build/include/yajl/*.h:third-party/yajl-2.1.0
	mkdir -p third-party/build/include/yajl
	cp third-party/yajl-2.1.0/src/api/*.h third-party/build/include/yajl
