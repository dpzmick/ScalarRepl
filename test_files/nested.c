#include <stdlib.h>
#include <stdio.h>

struct c {
  int c;
};

struct b {
  struct c nest;
  int b;
};

struct a {
  struct b nest;
  int a;
};

int main(int argc, char *argv[]) {
  struct a mean;
  mean.a = rand();
  mean.nest.b = rand();
  mean.nest.nest.c = rand();
  mean.a = mean.a + mean.nest.b + mean.nest.nest.c;

  printf("mean.a: %d\n", mean.a);
  return 0;
}
