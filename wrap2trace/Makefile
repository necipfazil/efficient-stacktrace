CXX = clang++
CXXFLAGS = -fPIC -fno-omit-frame-pointer
LDFLAGS = -Wl,--wrap=malloc,--wrap=free -ldl

all: wrap2trace.o

test: a.out
	./a.out

a.out: wrap2trace.o test.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) wrap2trace.o test.cpp

wrap2trace.o: wrap2trace.cpp
	$(CXX) $(CXXFLAGS) -c wrap2trace.cpp -o wrap2trace.o

clean:
	rm -f a.out wrap2trace.o
