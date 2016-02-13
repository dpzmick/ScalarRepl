#include <stdio.h>

struct x {
	int a, b, c;
};

int f(int b)
{
	struct x x; 
	if (b) {
		x.a = 1;
		x.b = 2;
		x.c = 3; 
	} else {
		x.a = 0;
		x.b = 1;
		x.c = 2; 
	}
	return x.a + x.b + x.c;
}

int main()
{
	printf("%d\n", f(1)); // 6
	printf("%d\n", f(0)); // 3
}
