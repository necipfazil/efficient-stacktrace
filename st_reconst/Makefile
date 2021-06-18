OUT = st_reconst

all: $(OUT)

$(OUT): cg.cpp cg.hpp cg_reconst.cpp
	clang++ -O3 -msse4.2 cg.cpp cg_reconst.cpp -o $(OUT)

run: $(OUT)
	./$(OUT) cgdump.txt st_sample.txt 100 4

clean:
	rm -f $(OUT)
