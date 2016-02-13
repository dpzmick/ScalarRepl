#include <stdlib.h>
#include <stdio.h>

struct a {
  int a;
  int b;
  char x;
};

int main(int argc, char *argv[]) {
  struct a *ptr = alloca(sizeof(struct a));
  ptr->a = 10 + rand();
  ptr->b = 10;
  ptr->x = 'c';

  if (ptr == NULL) {
    printf("NULL\n");
    return 1;
  }

  printf("not null, and value is: %d\n", ptr->a);
  return 0;
}
