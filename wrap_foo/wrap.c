#include <stdio.h>

void __real_foo();

void __wrap_foo() {
  printf("wrapper foo calling real foo\n");
  __real_foo();
}