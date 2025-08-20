CC = gcc

all: injector fileadder httpd_sim

clean:
	rm *.o

injector: injector.c
	gcc $^ -o $@

fileadder: fileadder.c
	gcc $^ -o $@

httpd_sim: httpd_sim.c httpd_sim.h
	gcc $< -o $@ -I/usr/include/json-c -ljson-c
