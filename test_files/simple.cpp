#include <stdio.h>

class Test {
  private:
    int a_;

  public:
    void setA(int a) { a_ = a;}
    int getA() { return a_; }
};

int main(int argc, char *argv[]) {
  // this doesn't get cleaned up by my scalarrepl or the builtin one
  // -O3 turns it into printf("%d\n", 100) though. pretty cool.
  Test t;
  t.setA(100);
  printf("%d\n", t.getA());
  return 0;
}
