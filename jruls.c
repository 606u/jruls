#include <sys/types.h>
#include <curses.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/rctl.h>
#include <sys/sysctl.h>
#include <sysexits.h>
#include <jail.h>
#include <unistd.h>

/* column widths */
static int cw_jid = 3, cw_name = 20, cw_pct = 4, cw_mem = 6;
static int cw_iops = 5, cw_iovol = 6, cw_cnt = 5;

static int smart_terminal = 1;

static void print_n(uint64_t v, int colwidth);
static void print_nmp(uint64_t v, int colwidth);

struct metric
{
	const char *label, *name;
	const int *colwidth;
	void (*print)(uint64_t v, int colwidth);
};
static const struct metric all_metrics[] = {
	{ "cpu%", "pcpu", &cw_pct, &print_n },
	{ "mem", "memoryuse", &cw_mem, &print_nmp },
	{ "memlck", "memorylocked", &cw_mem, &print_nmp },
	{ "proc", "maxproc", &cw_cnt, &print_n },
	{ "fds", "openfiles", &cw_cnt, &print_nmp },
	{ "vmem", "vmemoryuse", &cw_mem, &print_nmp },
	{ "ptys", "pseudoterminals", &cw_cnt, &print_n },
	{ "swap", "swapuse", &cw_mem, &print_nmp },
	{ "thread", "nthr", &cw_cnt, &print_n },
	{ "r/s", "readiops", &cw_iops, &print_nmp },
	{ "read", "readbps", &cw_iovol, &print_nmp },
	{ "w/s", "writeiops", &cw_iops, &print_nmp },
	{ "writtn", "writebps", &cw_iovol, &print_nmp },
};
static const size_t tot_metrics =
	sizeof (all_metrics) / sizeof (all_metrics[0]);
static const struct metric *metrics[30];

/* Rudimentary I/O abstraction to support smart and dump terminals */
struct iofuncs {
	/* x-prefix is a work-around as clear, attron... are macros */
	int (*xclear)(void);
	int (*xattron)(int attrs);
	int (*xattroff)(int attrs);
	int (*xprint)(const char *fmt, ...);
	int (*xrefresh)(void);
} io = { 0 };

static const struct metric*
find_metric(const char *name)
{
	const struct metric *it = all_metrics, *endp = it + tot_metrics;
	for (; it != endp; ++it)
		if (!strcmp(it->label, name) ||
		    !strcmp(it->name, name))
			return it;
	return 0;
}


static uint64_t
findval(const char *list,
	const char *name)
{
	char *p = strstr(list, name);
	if (p) {
		while (*p && *p != '=')
			++p;
		if (*p == '=') {
			++p;
			return (uint64_t)strtoull(p, 0, 10);
		}
	}
	return 0;
}


static void
print_n(uint64_t v, int colwidth)
{
	io.xprint("%*"PRIu64, colwidth, v);
}

static void
print_nmp(uint64_t v, int colwidth)
{
	char buf[20];
	if (v < 9999)
		snprintf(buf, sizeof buf, "%"PRIu64, v);
	else {
		if (v > 9999 * 1024 * 1024ll)
			snprintf(buf, sizeof buf, "%.2fG",
				 v / (1024 * 1024 * 1024.0));
		else if (v > 9999 * 1024)
			snprintf(buf, sizeof buf, "%.1fM",
				 v / (1024 * 1024.0));
		else /* if (v > 9999) */
			snprintf(buf, sizeof buf, "%.0fK",
				 v / 1024.0);
	}
	io.xprint("%*s", colwidth, buf);
}


static void
print_headers(void)
{
	io.xclear();
	io.xattron(A_BOLD);
	io.xprint("%*s  %-*s",
		  cw_jid, "jid",
		  cw_name, "name");
	const struct metric **it = metrics;
	for (; *it; ++it) {
		io.xprint("  %*s", *(*it)->colwidth, (*it)->label);
	}
	io.xprint("\n");
	io.xattroff(A_BOLD);
}


static void
print_jail(int jid,
	   const char *name)
{
	char q[MAXHOSTNAMELEN + 20], buf[4096];
	io.xprint("%*d  %-*s",
		  cw_jid, jid,
		  cw_name, name);

	snprintf(q, sizeof q, "jail:%s:cputime", name);
	int rv = rctl_get_racct(q, strlen(q) + 1,
				buf, sizeof buf);
	if (rv != -1) {
		/* buf is comma-delimited sequence of key=value
		 * pairs: cputime=##,datasize=##,... */
		const struct metric **it = metrics;
		for (; *it; ++it) {
 			io.xprint("  ");
			(*it)->print(findval(buf, (*it)->name),
				     *(*it)->colwidth);
		}
	} else {
		if (errno == ENOSYS)
			err(EX_OSERR, "rctl_get_racct");
		io.xprint(": %s", strerror(errno));
	}
	io.xprint("\n");
}


static int
no_clear(void)
{
	return 0;
}
static int
no_refresh(void)
{
	printf("\n");
	fflush(stdout);
	return 0;
}
static int
no_attrtoggle(int attrs)
{
	(void)attrs;
	return 0;
}

static void
init_io(void)
{
	smart_terminal = getenv("TERM") != 0;
	if (smart_terminal) {
		smart_terminal = isatty(1);
	}

	if (smart_terminal) {
		/* Initialize curses library */
		setlocale(LC_ALL, "");
		initscr();

		io.xclear = &clear;
		io.xattron = &attron;
		io.xattroff = &attroff;
		io.xprint = &printw;
		io.xrefresh = &refresh;
	} else { /* dump terminal */
		io.xclear = &no_clear;
		io.xattron = &no_attrtoggle;
		io.xattroff = &no_attrtoggle;
		io.xprint = &printf;
		io.xrefresh = &no_refresh;
	}
}


static int
usage(void)
{
	printf("Usage: jruls [-d count] [-s time]\n");
	return 0;
}


int
main(int argc, char *argv[])
{
	int ena;
	size_t ena_len = sizeof ena;
	int rv = sysctlbyname("kern.racct.enable", &ena, &ena_len, 0, 0);
	if (rv == -1 && errno == ENOENT)
		errx(EX_UNAVAILABLE,
		     "RACCT/RCTL support not compiled; see rctl(8)");
	if (!ena)
		errx(EX_UNAVAILABLE,
		     "RACCT/RCTL support not enabled; enable using kern.racct.enable=1 tunable");

	int count = INT_MAX, sleep_itv = 2;
	int opt;

	while ((opt = getopt(argc, argv, "s:d:h")) != -1) {
		char *endp = 0;
		switch (opt) {
		case 's':
			sleep_itv = strtol(optarg, &endp, 10);
			if (!endp || *endp || sleep_itv <= 0)
				errx(EX_USAGE, "illegal time -- '%s'", optarg);
			break;
		case 'd':
			count = strtol(optarg, &endp, 10);
			if (!endp || *endp || count < 0)
				errx(EX_USAGE, "illegal count -- '%s'", optarg);
			break;
		default:
			usage();
			return EX_USAGE;
		}
	}

	if (!metrics[0]) {
		/* include default metrics, unless customized */
		metrics[0] = find_metric("cpu%");
		metrics[1] = find_metric("mem");
		metrics[2] = find_metric("r/s");
		metrics[3] = find_metric("read");
		metrics[4] = find_metric("w/s");
		metrics[5] = find_metric("writtn");
	}

	int jid, lastjid;
	char name[MAXHOSTNAMELEN];
	struct jailparam param[3]; /* lastjid jid name */

	if (jailparam_init(&param[0], "lastjid") == -1 ||
	    jailparam_init(&param[1], "jid") == -1 ||
	    jailparam_init(&param[2], "name") == -1)
		errx(EX_OSERR, "jailparam_init: %s", jail_errmsg);

	if (jailparam_import_raw(&param[0], &lastjid, sizeof lastjid) == -1 ||
	    jailparam_import_raw(&param[1], &jid, sizeof jid) == -1 ||
	    jailparam_import_raw(&param[2], name, sizeof name) == -1)
		errx(EX_OSERR, "jailparam_import_raw: %s", jail_errmsg);

	init_io();

	if (count == INT_MAX && !smart_terminal) {
		count = 1;
	}

	do {
		int jflags = 0, rv, n;
		lastjid = n = 0;
		do {
			rv = jailparam_get(param, sizeof (param) / sizeof (param[0]), jflags);
			if (rv != -1) {
				if (++n == 1)
					print_headers();
				print_jail(*(int*)param[1].jp_value,
					   (char*)param[2].jp_value);
			}
			lastjid = rv;
		} while (rv >= 0);
		if (rv == -1 && errno != ENOENT)
			errx(EX_OSERR, "jailparam_get: %s", jail_errmsg);
		if (!n)
			errx(EX_UNAVAILABLE, "no jails found");

		io.xrefresh();
		if (count > 1) {
			sleep(sleep_itv);
			puts("");
		}
	} while (--count > 0);
	return 0;
}
