CC=clang
CFLAGS+=-g -fno-omit-frame-pointer
CFLAGS+=-rdynamic # use for backtrace_symbols
all: a.out

a.out: stacktrace.c
	$(CC) $(CFLAGS) stacktrace.c -o a.out
clean:
	rm -f a.out