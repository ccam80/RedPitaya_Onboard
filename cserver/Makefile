CFLAGS = -O3 -march=armv7-a -mtune=cortex-a9 -D_GNU_SOURCE

all: adc-test-server gen struct_test

adc-test-server: adc-test-server.c
	gcc $(CFLAGS) -o $@ $^ -lm

struct_test: struct_test.c
	gcc $(CFLAGS) -o $@ $^ -lm
	
gen: gen.c
	gcc $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f adc-test-server gen struc_test

