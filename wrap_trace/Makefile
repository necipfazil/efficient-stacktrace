CC=clang
CFLAGS+=-g -fno-omit-frame-pointer
#CFLAGS+=-rdynamic # use for backtrace_symbols

all: wrap2trace.o main.o
	$(CC) -Wl,--wrap=malloc,--wrap=free wrap2trace.o main.o

clean:
	rm -f *.o a.out