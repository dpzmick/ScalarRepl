#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct t {
  int arr[8];
  int a;
};

int main() {
  struct t tt; // this alloca should go away

  // set some fields
  // not using a loop because I want GEP to all have constant offsets (loop
  // introduces induction var)
  tt.arr[0] = 10;
  tt.arr[1] = 10;
  tt.arr[2] = 10;
  tt.arr[3] = 10;
  tt.arr[4] = 10;
  tt.arr[5] = 10;
  tt.arr[6] = 10;
  tt.arr[7] = 10;
  tt.a = tt.arr[7];

  // "loop" over the array
  printf("%d\n", tt.arr[0]);
  printf("%d\n", tt.arr[1]);
  printf("%d\n", tt.arr[2]);
  printf("%d\n", tt.arr[3]);
  printf("%d\n", tt.arr[4]);
  printf("%d\n", tt.arr[5]);
  printf("%d\n", tt.arr[6]);
  printf("%d\n", tt.arr[7]);

  // dont do anything with one of the elements, this element should get
  // eliminated with DCE
  // printf("%d\n", tt.a);

  return 0;
}
