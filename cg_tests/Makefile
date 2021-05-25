OPTPASS_STDERR=optpass_stderr.txt

all: a.out main_wpass.ll

a.out: main_wpass.o
	clang -O0 -g main_wpass.o

main_wpass.ll: main_wpass.bc
	llvm-dis main_wpass.bc

main_wpass.o: main_wpass.bc
	llc -filetype=obj -O0 main_wpass.bc

# use LLVMFuncInstr pass
main_wpass.bc: main.bc
	opt -load ${LLVMLIB}/LLVMFuncInstr.so --func-instr -enable-new-pm=0 -f main.bc > main_wpass.bc 2> $(OPTPASS_STDERR)

main.bc: main.c
	clang -O0 -g -emit-llvm main.c -c -o main.bc

od: a.out main_wpass.o
	llvm-objdump -D main_wpass.o > main_wpass.dmp
	llvm-objdump -D a.out > a_out.dmp

clean:
	rm -f *.o *.bc *.ll $(OPTPASS_STDERR) *.out *.dmp *.elf
