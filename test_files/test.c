#include <stdio.h>
#include <stdlib.h>

struct test {
  int a;
  int b;
  int c;
};

int main () {
  int r = rand();

  struct test t;
  t.a = 100 + r;
  t.b = 200;
  t.c = 12;

  printf("rand is: %d\n", r);
  printf("%d\n", t.a + t.b);
}
