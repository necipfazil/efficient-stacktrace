# Efficient stack trace collection/reconstruction

This is an ongoing project.  We are looking for ways to improve the correctness and the performance.

Steps are as follows:
1. Collecting stack traces
2. Computing and storing conservative call graph in the binary
3. Restoring call graph from the binary
4. Stack trace reconstruction

Originally, the aim is to collect compressed stack traces at the first stage and decompress it at the fourth.
However, for easier testing/evaluation of the whole pipeline, first stage will print the full stack trace and fourth stage will compress then decompress.

For testing, SPEC benchmarks can be used as described below.

### 1. Collecting stack traces
For stack trace collection, the program should be instrumented such that the target functions are instrumented for printing the stack traces when called.
In the efficient scenario, only the compressed stack trace will be printed.
However, following describes to get the full stack trace.

#### Printing stack trace

To get and print the stack traces, `backtrace()` can be used (from `execinfo.h`).  See TODO for implementation of `GetCurrentStackTrace()` and `PrintStackTrace()`.
Implementation depends on frame pointers; thus, use `-fno-omit-frame-pointer`.

Notice: Due to ASLR/DSO, memory addresses do not always map to binary addresses.
A temporary solution to this is to map the addresses at runtime using `dladdr1` (from `dlfcn.h`, link with `-ldl`).
This requires knowledge of the DSOs loaded for later reconstructing the call graph.
However, for some cases (e.g., SPEC benchmarks), it is a _functional_ work around for ASLR.
Later solution will involve logging information on loaded modules.

#### Wrapping functions

To achieve this, functions to instrument are wrapped by using `--wrap` option of the linker.

For example, to print stack traces when malloc is called, following code piece is sufficient (together with definition for `PrintStackTrace`):

```
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size) {
  PrintStackTrace("__wrap_malloc");
  return __real_malloc(size);
}
```

Compile an object by implementing __wrap_FUNCNAME for functions to be instrumented, and link it with `--wrap=FUNCNAME`.

A full example can be found at TODO.

**As a result,** the program will output full stack traces to `stderr`, in "`FUNCNAME STADDR_TOP, .., STADDR_BOTTOM\n`" format per stack trace.

### 2. Computing and storing conservative call graph in the binary

A new feature is implemented in LLVM for this.
Find it at https://github.com/necipfazil/llvm-project/tree/necip-callgraph.
As it is implemented in `clang`, build and install `llvm` including `clang` (i.e., pass `-DLLVM_ENABLE_PROJECTS=clang` to `CMake`).

To enable storing the call graph information in the binary, compile code with `clang` using `-fcall-graph-section`.

More documentation on how it works will follow.

### 3. Restoring call graph from the binary

A new feature is implemented in `llvm-objdump` for this.
Find it at https://github.com/necipfazil/llvm-project/tree/necip-callgraph (same as step 2).

To dump call graph information from a binary (e.g., `a.out`), use: 

```llvm-objdump --call-graph-info a.out > call_graph_info.txt```

The output will include several pieces of information regarding the call graph, which can be used at step 4.
The indirect call and target types will be included only if the binary is compiled with `clang` using `-fcall-graph-section` as described in step 2.
It can still extract information otherwise; yet, it will be less precise with no restrictions on indirect call and target types; which reduces the stack trace reconstruction performance.

### 4. Stack trace reconstruction

A stand-alone tool is implemented for stack trace reconstruction.
Originally, the aim is to input compressed stack trace and call graph, and output the decompressed stack trace.
However, the tool takes full stack traces and call graph, internally compresses the stack traces, decompresses, and evaluates the correctness and the performance of the decompression.

The tool implementation is provided at TODO.

The command line arguments to the tool is as follows, respectively:
1. Call graph info: path to text file containing the output from `llvm-objdump --call-graph-info`.
2. Stack traces: path to the text file containing the stack trace output from the instrumented program.
3. Max depth: number representing the maximum depth to be explored in the call graph while decompression.

Stack traces (2nd arg) that are longer than max depth (3rd arg) are cut off to the maximum depth.

Currently, the tool has little optimizations for faster decompression.
Decompressing long stack traces for large call graphs might take long.
To test in shorter time, limit max depth (e.g., 5-10).

#### Output
For each function stack traces are given, a DFS is done on the call graph and a summary is reported.

Example summary is as follows:
```
Func: __wrap_malloc
Nodes visited: 200
Pruning count: 1
Num stack traces             : 5
Num found correctly          : 5
Num had incorrect collisions : 0
Num incorrect collisions     : 0
```

Interpretation is as follows:
1. **Func:** function name.
2. **Nodes visited:** number of nodes visited in the call graph during DFS.
3. **Pruning count:** times pruning is done (simple pruning is done in the current implementation at depth 4).
4. **Num stack traces:** number of unique stack traces compressed/decompressed.
5. **Num found correctly:** number of unique stack trace decompressed correctly.
6. **Num had incorrect collisions:** number of unique stack traces that are decompressed incorrectly for at least one (notice that there can be multiple decompressions).
7. **Num incorrect collisions:** number of incorrect decompressions.

## Testing with SPEC benchmarks

Following describes how to set up SPEC CPU2006 436.cactusADM benchmark for testing the whole pipeline for malloc/free calls.

1. Install llvm/clang from https://github.com/necipfazil/llvm-project/tree/necip-callgraph (notice: enable clang, i.e., `-DLLVM_ENABLE_PROJECTS=clang`).

2. Create the shared object to be linked for collecting stack traces:
    1. Decide which functions you would like to instrument (e.g., `malloc`, `free`).
    2. Create a source file for wrappers as described in `### 1. Collecting stack traces` section. (or, directly use the already implemented ones for `malloc` and `free` at TODO).
    3. Compile a shared unit: `clang++ -msse4.2 -fno-omit-frame-pointer -fcall-graph-section -fPIC -shared -o wrap2trace.o wrap2trace.cpp`
        * Assume the compiled binary is at `$ST_TOOLS/wrap2trace.o`

3. Compile the stack trace reconstruction tool.  Assume the tool is at `$ST_TOOLS/st_reconst.out`.

4. SPEC benchmark setup:
    1. Install the SPEC CPU2006 benchmarks, let's say, at `$SPEC_CPU06/`.
    2. Make a copy of your favorite spec config file, e.g., `cp $SPEC_CPU06/config/Example-linux64-amd64-gcc43+.cfg $SPEC_CPU06/config/st_cg_instr.cfg`.
    3. Do the following changes on the config:
       1. Set CC: `CC = /usr/local/bin/clang -std=gnu89 -fcall-graph-section`
       2. Set CXX: `CC = /usr/local/bin/clang++ -std=c++98 -fcall-graph-section`
       3. Set COPTIMIZE/CXXOPTIMIZE/FOPTIMIZE (`-O0` is for faster building, can be changed):
             * `COPTIMIZE   = -O0 -fno-strict-aliasing -fno-omit-frame-pointer -fPIE`
             * `CXXOPTIMIZE = -O0 -fno-strict-aliasing -fno-omit-frame-pointer -fPIE`
             * `FOPTIMIZE   = -O0 -fno-strict-aliasing -fno-omit-frame-pointer`
       4. Set EXTRA_LIBS: `EXTRA_LIBS = -Wl,--wrap=malloc,--wrap=free $$ST_TOOLS/wrap2trace.o -ldl`
        5. Set ext: `ext = efficient-st-test`


5. Build and run the benchmark (`-size` can be changed to `train`/`ref`):
    1. `cd $SPEC_CPU06/`
    2. `runspec --config st_cg_instr.cfg -size test -I -l 436.cactusADM`
6. CD to: `cd $SPEC_CPU06/benchspec/CPU2006/436.cactusADM/run/run_base_test_efficient-st-test.0000/`
    1. `benchADM.err` contains the stack traces
7. Restore the call graph: `llvm-objdump --call-graph-info cactusADM_base.efficient-st-test > cg.txt`
8. Run the reconstruction tool: `$ST_TOOLS/st_reconst.out cg.txt benchADM.err 7`
    * Max depth is set to 7 to get quick results.  This can be set differently for avoiding cutting off the stack traces.

And, enjoy the output:
```
WARNING: 33166 stack traces were clipped as they exceeded the depth limit.
WARNING: Hash collisions occured for 27118 stack traces.
Func: __wrap_malloc
Nodes visited: 99484755
Pruning count: 17061262
Num stack traces             : 5158
Num found correctly          : 5151
Num had incorrect collisions : 0
Num incorrect collisions     : 0

Func: __wrap_free
Nodes visited: 102369243
Pruning count: 52174778
Num stack traces             : 3104
Num found correctly          : 3097
Num had incorrect collisions : 0
Num incorrect collisions     : 0
```