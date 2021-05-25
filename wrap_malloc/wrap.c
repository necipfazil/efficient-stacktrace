#include <stdio.h>
#include <stdlib.h>

void *__real_malloc(size_t);

void *__wrap_malloc(size_t size) {
  printf("wrapper malloc\n");
  return __real_malloc(size);
}