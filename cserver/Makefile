CFLAGS = -O3 -march=armv7-a -mtune=cortex-a9 -D_GNU_SOURCE

all: feedback_server

feedback_server: feedback_server.c
	gcc $(CFLAGS) -o -g $@ $^ -lm

clean:
	rm -f feedback_server

