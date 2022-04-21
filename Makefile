CFLAGS= -std=c99 -pedantic -Wall -Wextra

.PHONY: all clean

all: send recv
clean:
	rm -f send recv ${TESTFILES}

.for s in 32 256 512 1024

test-$s.in:
	> $@
	for i in $$(jot $$(9000 / $s)); do printf "%0$ss" $$i >> $@; done

. for n in 32 256 512 1024

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

test-all: test-$n-$s test-m-$n-$s
. endfor
.endfor
