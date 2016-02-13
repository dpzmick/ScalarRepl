struct y {
	int b, c;
};

struct x {
	int a;
	struct y y;
};

int f()
{
	struct x x; 
	x.a = 1;
	x.y.b = 2;
	x.y.c = 3; 
	return x.a + x.y.b + x.y.c;
}

int main()
{
	printf("%d\n", f()); // 6
}
