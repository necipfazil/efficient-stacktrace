// Wrappers for malloc/free calls for collecting stack traces, and the
// stack trace collectiong/printing implementation.

// Compile this to an object file with:
//   clang++ -fPIC -fno-omit-frame-pointer wrap2trace.cpp -c -o wrap2trace.o
// Link wrap2trace.o to the software to be instrumented with following flags:
//   -Wl,--wrap=malloc,--wrap=free wrap2trace.o -ldl

#include <cstdint>
#include <cstdio>
#include <execinfo.h> // backtrace()
#include <dlfcn.h> // dladdr1(), link with -ldl
#include <link.h>
extern "C" {

#define MAX_STACK_TRACE_SIZE 100

// static uintptr_t Hash(uintptr_t *StackTrace, size_t StackTraceSize) {
//   uintptr_t Res = 0;
//   for (size_t I = 0; I < StackTraceSize; I++)
//     Res = __builtin_ia32_crc32di(Res, StackTrace[I]);
//   return Res & 0xfffff;
// }

/////////////////////////////////////
/* Stack trace collection/printing */
/////////////////////////////////////

__attribute__((always_inline)) static size_t GetCurrentStackTrace(
    void **StackTrace, size_t kMaxStackTraceSize, bool translate) { 
  uintptr_t BackTraceSize = backtrace(StackTrace, kMaxStackTraceSize);
  
  // TODO: Translating memory addresses to object addresses can be
  // eliminated by using ASLR offset and DSO load addresses are known.
  // After the mapping, one still needs which DSOs are loaded to
  // reconstruct the call graph thus the full stack trace.
  // Solution is to keep discard below mapping and keep track of DSOs
  // loaded, and ASLR offset.
  if (translate) {
    for (int I = 0; I < BackTraceSize; I++) {
      Dl_info info;
      struct link_map *map;
      if ( dladdr1(StackTrace[I], &info, (void**)&map, RTLD_DL_LINKMAP) ) {
        if (map->l_addr) {
          // There is difference between the memory and the object addresses.
          uintptr_t realdiff;
          if (StackTrace[I] >= (void *) map->l_addr)
            realdiff = (uintptr_t)StackTrace[I] - (uintptr_t)map->l_addr;
          else
            realdiff = (uintptr_t)map->l_addr - (uintptr_t)StackTrace[I];
          StackTrace[I] = (void*)realdiff;
        }
      }
    }
  }

  return BackTraceSize;
}

__attribute__((always_inline))
static void PrintStackTrace(const char *At) {
  void* StackTrace[MAX_STACK_TRACE_SIZE];
  size_t StackTraceSize = GetCurrentStackTrace(StackTrace, MAX_STACK_TRACE_SIZE, true);

  // Notice: duplicate stack traces might get printed as we don't check for
  // duplicates right now. Holding a { Hash: IsSeen } mapping might help,
  // however, it would deduplicate the traces with the same hash -- not 
  // necessarily the same traces as two different trace might share the
  // trace (undesired). To evaluate the compression method, keep printing
  // all stack traces and do any deduplication during reconstruction.
  fprintf(stderr, "%s ", At);

  // Cutting off the last one and first two frames as: last one is for
  // backtrace() call, first two are (generally) before main(). This avoids
  // the need for finding addresses to glibc calls that happen before main.
  // This is not a solution to all DSO related issues -- intermediate frames
  // might still include calls from DSOs.
  for (size_t i = 1; i < StackTraceSize - 2; i++)
      fprintf(stderr, "%p ", StackTrace[i]);
  fprintf(stderr, "\n");
}

//////////////////////////////
/* Wrappers for malloc/free */
//////////////////////////////

void *__real_malloc(size_t);
void __real_free(void*);

void *__wrap_malloc(size_t size) {
  PrintStackTrace("__wrap_malloc");
  return __real_malloc(size);
}

void __wrap_free(void* ptr) {
  PrintStackTrace("__wrap_free");
  __real_free(ptr);
}

} // extern "C"
