CC = gcc
CCFLAGS = -Wall -o

all: injector fileadder httpd_sim

clean:
	rm *.o

injector: injector.c
	gcc $^ $(CCFLAGS) $@

fileadder: fileadder.c
	gcc $^ $(CCFLAGS) $@

httpd_sim: httpd_sim.c httpd_sim.h
	gcc $< $(CCFLAGS) $@ -I/usr/include/json-c -ljson-c
