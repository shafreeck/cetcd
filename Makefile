CFLAGS=-g -Wall -O0
LDFLAGS=-lcurl -lyajl -lpthread

all:cetcd_array.o sds/sds.o cetcd.o cetcdctrl
clean:
	rm -f cetcdctrl parse *.o sds/*.o

parse:parse.c sds/sds.o cetcd_array.o
cetcdctrl:cetcd_array.o sds/sds.o cetcd.o
sds/sds.o:sds/sds.c sds/sds.h
cetcd_array.o:cetcd_array.c cetcd_array.h
cetcd.o:cetcd.c cetcd.h cetcd_json_parser.h
