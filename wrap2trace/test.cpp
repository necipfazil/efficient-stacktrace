// bunch of malloc/free calls, which are wrapped for stack trace collection

#include <stdio.h>
#include <stdlib.h>

// void odd(char* m) {
//   printf("debug: odd()\n");
//   free(m);

// }

// void even(char* m) {
//   printf("debug: even()\n");
//   free(m);
// }

void r1(int k) {
  //printf("debug: r1(%d)\n", k);
  if (k < 1) return;
  char* m = (char*)malloc(k);
  r1(k-1);
  free(m);
  // if (k % 2) {
  //   odd(m);
  //   r1(k-1);
  // } else {
  //   r1(k-1);
  //   even(m);
  // }
}

int main() {
  int k = 5;
  //printf("debug: main (k=%d)\n", k);
  r1(k);
  return 0;
}