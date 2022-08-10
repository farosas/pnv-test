/* Copyright 2022 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdbool.h>

#include "console.h"

#define DEBUG
#include "print.h"

#define strfy(x) #x

#define RTAS_GET_TIME_OF_DAY 0x2001
#define RTAS_OS_TERM 0x201f
#define DECR 0x16
#define TBL 0x10c
#define VTB 0x351

/* ~160ms in TB units, enough for a local migration */
#define MIGRATION_DELAY 0x5000000

#define LARGE_DECR

struct rtas {
	int32_t token;
	int32_t nargs;
	int32_t nret;
	int32_t args[16];
	int32_t *rets;
} rtas_argbuf;

struct one_spr {
	const char *name;
	uint64_t dval;
};
extern void init_gprs(uint64_t *);
extern void save_gprs(uint64_t *);
extern void init_sprs(struct one_spr *);
extern void save_sprs(struct one_spr *);
extern void rtas_call(void);

uint64_t tb_freq;
uint64_t delay;
uint64_t saved_gprs[32];
uint64_t default_gprs[32];
struct one_spr saved_sprs[1024];
struct one_spr default_sprs[1024] = {
#ifdef LARGE_DECR
	[DECR] = { strfy(DECR), 0x007afafadeadbeef /* arbitrary */ },
#else
	[DECR] = { strfy(DECR), 0x7eadbeef /* arbitrary */ },
#endif
};

static inline unsigned long mfspr(int sprnum)
{
	long val;

	__asm__ volatile("mfspr %0,%1" : "=r" (val) : "i" (sprnum));
	return val;
}

static void print_time(void)
{
	print("[");
	print_hex(mfspr(TBL));
	print("] ");
}

static void print_spr_neq(int i)
{
	print("\n");
	print_time();
	print(default_sprs[i].name);
	print_regs(default_sprs[i].dval, saved_sprs[i].dval, " != ");
}

static void print_spr_eq(int i)
{
	print("\n");
	print_time();
	print(default_sprs[i].name);
	print_regs(default_sprs[i].dval, saved_sprs[i].dval, " == ");
}

static void tb_init(void) {
	/* P9 default */
	tb_freq = 512000000ULL;
}

static void rtas_init(void)
{
	rtas_argbuf.rets = rtas_argbuf.args;
}

static void rtas_load_retval(const char *str)
{
	int32_t strp = (int64_t)str;

	rtas_argbuf.token = __builtin_bswap32(RTAS_OS_TERM);
	rtas_argbuf.nargs = __builtin_bswap32(1);
	rtas_argbuf.nret = __builtin_bswap32(1);

	rtas_argbuf.args[0] = __builtin_bswap32(strp);
}

static void assert(bool cond)
{
	if (cond)
		return;

	rtas_load_retval("FAIL");
	rtas_call();
}

static uint64_t make_ts()
{
	volatile uint64_t ts;
	uint32_t hour, min, sec;

	rtas_argbuf.token = __builtin_bswap32(RTAS_GET_TIME_OF_DAY);
	rtas_argbuf.nargs = 0;
	rtas_argbuf.nret = __builtin_bswap32(8);

	rtas_call();

	hour = __builtin_bswap32(rtas_argbuf.rets[4]);
	min  = __builtin_bswap32(rtas_argbuf.rets[5]);
	sec  = __builtin_bswap32(rtas_argbuf.rets[6]);

	ts = hour << 12;
	ts |= min << 6;
	ts |= sec;

	return ts;
}

static void regs_init(void)
{
	int i;

	for (i = 0; i < 1023; i++)
		saved_sprs[i].name = default_sprs[i].name;
}

static bool spr_equal(int i)
{
	uint64_t x = default_sprs[i].dval;
	uint64_t y = saved_sprs[i].dval;

	switch (i) {
	case DECR:
	{
		uint64_t threshold = (tb_freq * delay) + MIGRATION_DELAY;
		uint64_t diff = (x > y) ? x - y : y - x;

		return diff < threshold;
	}
	default:
		return x == y;
	}
}

static int cmp_regs(void)
{
	int fail = 0;
	int i;

	print("Checking SPRs");
	for (i = 0; i < 1023; i++) {
		if (!default_sprs[i].name)
			continue;

		print(".");
		if (!spr_equal(i)) {
			print_spr_neq(i);
			fail = 1;
			break;
		} else {
			print_spr_eq(i);
		}
	}

	if (!fail)
		print("OK\n");

	return fail;
}

static void sleep(int seconds)
{
	uint64_t ts0, ts1, tb0, vtb0, count;

	count = tb_freq * seconds;
	ts0 = make_ts();

	/* some redundancy in case one of them gets corrupted */
	tb0 = mfspr(TBL);
	vtb0 = mfspr(VTB);
	while (mfspr(TBL) - tb0 < count || mfspr(VTB) - vtb0 < count);

	ts1 = make_ts();
	assert(ts1 > ts0);
	assert(((ts1 - ts0) & 0x3f) >= seconds);
}

__attribute__((optimize(0))) int main(void)
{
	uint64_t iter = 100;

	console_init();
	tb_init();
	rtas_init();
	regs_init();

	print_test_number(1);
	delay = 2;
	do {
		print("Dirtying SPRs (migrate now)\n");

		init_sprs(default_sprs);
		sleep(delay);
		save_sprs(saved_sprs);
	} while (cmp_regs() == 0 && --iter);

	if (iter)
		rtas_load_retval("FAIL");
	else
		rtas_load_retval("PASS");

	return 0;
}
