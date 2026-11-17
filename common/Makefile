CC     ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O1

test: test_proto
	./test_proto

test_proto: test_proto.c proto.c proto.h
	$(CC) $(CFLAGS) -o $@ test_proto.c proto.c

clean:
	rm -f test_proto

.PHONY: test clean
