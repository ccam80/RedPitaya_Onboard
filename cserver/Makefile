CFLAGS = -march=armv7-a -mtune=cortex-a9 -D_GNU_SOURCE

all: feedback_server

debug: feedback_server.c
	gcc -g $(CFLAGS) -o $@ $^ -lm
	
feedback_server: feedback_server.c
	gcc $(CFLAGS) -O3 -o $@ $^ -lm

clean:
	rm -f feedback_server debug


