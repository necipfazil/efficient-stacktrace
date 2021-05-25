CC=clang
all: wrap.o main.o
	$(CC) -Wl,--wrap=malloc wrap.o main.o
clean:
	rm -f *.o a.out