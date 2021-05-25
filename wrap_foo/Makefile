CC=clang
all: foo.o wrap.o main.o
	$(CC) -Wl,--wrap=foo foo.o wrap.o main.o
clean:
	rm -f *.o a.out