#include <stdio.h>
#include <stdlib.h>

struct a {
  int el;
  const char * str;
};

void foo(struct a t) {
  printf("%d\n", t.el);
}

int main(int argc, char *argv[]) {
  struct a s;
  s.el = rand();

  // can't eliminate the struct now, we've made a function call
  // or so we would hope. Something seems to be expanding the function call into
  // a function taking n arguments, where n is the number of fields in the
  // struct
  foo(s);

  return 0;
}
