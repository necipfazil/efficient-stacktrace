void bar();

void foo() {
  bar();
  bar();
  bar();
}

void bar() {
  bar();
}

void baz() {
}

void qux() {
}

void indirection(void (*f)() ) {
  (*f)();
}

void indirection2(void (*f)(int, int, int)) {
  f(1,2,3);
}

int main() {
  int k;

  // conditional
  if (k) {
    foo();
  } else {
    baz();
  }

  // indirection
  indirection(&foo);
  return 0;
}
