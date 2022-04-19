CFLAGS= -std=c99 -pedantic -Wall -Wextra

.PHONY: all clean

all: send recv
clean:
	rm -f send recv

.for s in 0 32 256 512 2048 4096

test-$s.in:
	> $@
	for i in $$(jot 1000); do printf "%$ss" $$i >> $@; done

. for n in 0 32 256 512 2048 4096

test-$n-$s: send recv test-$s.in
	> $@.out
	./recv -m -n $n -s $s $@.out & \
	pid=$$!; \
	sleep .1; \
	./send -n $n -s $s test-$s.in; \
	kill -INT $$pid; \
	sleep 2

test-m-$n-$s: send recv test-$s.in
	> $@.out
	./recv -m -n $n -s $s $@.out & \
	pid=$$!; \
	sleep .1; \
	./send -m -n $n -s $s test-$s.in; \
	kill -INT $$pid; \
	sleep 2

test: test-$n-$s test-m-$n-$s
. endfor
.endfor
