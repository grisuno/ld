CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2

all: ld

ld: ld.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	bash test/run_tests.sh

clean:
	rm -f ld

.PHONY: all test clean
