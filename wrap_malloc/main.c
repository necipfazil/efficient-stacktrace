#include <stdio.h>
#include <stdlib.h>

int main() {
  int* ints = (int*)malloc(4*sizeof(int));

  free(ints);
  return 0;
}