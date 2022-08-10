#ifdef DEBUG
#define print(str) puts(str)

// i < 100
static void print_test_number(int i)
{
	puts("Test ");
	putchar(48 + i/10);
	putchar(48 + i%10);
	puts(":\n");
}

static void print_hex(uint64_t n)
{
	int i;
	char *p = (char *) &n;
	const char dig[] = "0123456789abcdef";

	for (i = sizeof(n) - 1; i >= 0; --i) {
		putchar(dig[p[i] >> 4]);
		putchar(dig[p[i] & 0xf]);
	}
}

static void print_regs(uint64_t x, uint64_t y, char *str)
{
	print(" 0x");
	print_hex(x);
	print(str);
	print("0x");
	print_hex(y);
	print("\n");
}
#else
static void print_regs(uint64_t x, uint64_t y, char *str) {}
static void print_test_number(int i) {}
#define print(str)
#endif
