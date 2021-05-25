// wrap malloc and free calls to collect stack traces
// -Wl,--wrap=malloc,--wrap=free

// TODO(necip): thread safety?
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>

#define MAX_STACK_TRACE_SIZE 100

void *__real_malloc(size_t);
void __real_free(void*);

// GetCurrentStackTrace
__attribute__((always_inline)) static size_t GetCurrentStackTrace(
    void **StackTrace, size_t kMaxStackTraceSize) { 
  return backtrace(StackTrace, kMaxStackTraceSize);
}

// PrintStackTrace
__attribute__((always_inline))
static void PrintStackTrace(const char *At) {
  void* StackTrace[MAX_STACK_TRACE_SIZE];
  size_t StackTraceSize = GetCurrentStackTrace(StackTrace, MAX_STACK_TRACE_SIZE);

  // TODO(necip): hash the stack trace
  fprintf(stderr, "%s: ", At);
  for (size_t i = 0; i < StackTraceSize; i++)
      fprintf(stderr, "%p ", StackTrace[i]);
  fprintf(stderr, "\n");
}

// malloc wrapper
void *__wrap_malloc(size_t size) {
  printf("debug: wrapper: malloc\n");
  PrintStackTrace("malloc");
  return __real_malloc(size);
}

// free wrapper
void __wrap_free(void* ptr) {
  printf("debug: wrapper: free\n");
  PrintStackTrace("free");
  __real_free(ptr);
}