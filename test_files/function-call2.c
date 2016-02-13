struct forced {
  int a;
  int b;
  char c;
};

// declared "elsewhere" but actually nowhere
void foo(struct forced);

int main(int argc, char *argv[]) {
  struct forced a;
  a.a = 10;
  a.b = 100;
  a.c = 'c';

  // this function call should prevent promotion of the struct
  foo(a);

  return 0;
}
