/* convert to host format as necessary */

#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

/* TODO map errors better, and generally better error handling */
#define _NETBSD_ENOENT 2
#define _NETBSD_EINVAL 22
#define _NETBSD_ENOSYS 78
#define _NETBSD_ENOTSUP	86

static jmp_buf buf;

extern char *_netbsd__progname;
extern char **_netbsd_environ;
int _netbsd_main(int argc, char **argv);
void _netbsd_exit(int status);

static int ret = 0;

/* this will be the entry point, original one renamed to _netbsd_main */
int
main(int argc, char **argv)
{
	int jret;

	_netbsd__progname = argv[0];

	if (! (jret = setjmp(buf))) {
		/* exit has not been called, so stdio may not be flushed etc */
        	_netbsd_exit(_netbsd_main(argc, argv));
		/* will call _exit so will not reach here */
	}
	return ret;
}

void
emul__exit(int status)
{

	ret = status;
	longjmp(buf, status);
}

/* this is the NetBSD initial environ array; when we fully just use host environ this can go away */
/* it is not quite clear why it is not being initialised properly, we should call the initialiser... */
static char *the_env[1] = { NULL } ;

void nullenv_init (void) __attribute__((constructor (102)));

void
nullenv_init()
{
	_netbsd_environ = the_env;
}

int *
emul__errno(void)
{
        return &errno;
}

typedef int64_t _netbsd_time_t;
typedef int _netbsd_suseconds_t;
typedef int64_t _netbsd_off_t;
typedef int _netbsd_clockid_t;

struct _netbsd_timeval {
	_netbsd_time_t tv_sec;
	_netbsd_suseconds_t tv_usec;
};

struct _netbsd_rusage {
  struct _netbsd_timeval ru_utime;
  struct _netbsd_timeval ru_stime;
  long    ru_maxrss;
  long    ru_ixrss;
  long    ru_idrss;
  long    ru_isrss;
  long    ru_minflt;
  long    ru_majflt;
  long    ru_nswap;
  long    ru_inblock;
  long    ru_oublock;
  long    ru_msgsnd;
  long    ru_msgrcv;
  long    ru_nsignals;
  long    ru_nvcsw;
  long    ru_nivcsw;
};

#define _NETBSD_MAP_SHARED       0x0001
#define _NETBSD_MAP_PRIVATE      0x0002
#define _NETBSD_MAP_FILE         0x0000
#define _NETBSD_MAP_FIXED        0x0010
#define _NETBSD_MAP_RENAME       0x0020
#define _NETBSD_MAP_NORESERVE    0x0040
#define _NETBSD_MAP_INHERIT      0x0080
#define _NETBSD_MAP_HASSEMAPHORE 0x0200
#define _NETBSD_MAP_TRYFIXED     0x0400
#define _NETBSD_MAP_WIRED        0x0800
#define _NETBSD_MAP_ANON         0x1000
#define _NETBSD_MAP_STACK        0x2000

void *
emul_mmap(void *addr, size_t length, int prot, int nflags, int fd, _netbsd_off_t offset)
{
	void *memp;

	if (! (fd == -1 && nflags & _NETBSD_MAP_ANON)) {
		errno = _NETBSD_ENOSYS;
		return (void *) -1;
	}

        memp = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	if (memp == MAP_FAILED) {
		errno = _NETBSD_EINVAL;
		return (void *) -1;
	}

	return memp;
}

int
emul_munmap(void *addr, size_t len)
{
	munmap(addr, len);
	return 0;
}

int
emul_madvise(void *addr, size_t length, int advice)
{
	/* thanks for the advice TODO can add */
	return 0;
}

int
emul_setpriority(int which, int who, int prio) {
	/* don't prioritise TODO can add */
	return 0;
}

static int rusage_map[2] = {
  RUSAGE_SELF,
  RUSAGE_CHILDREN,
};

int
emul__getrusage50(int who, struct _netbsd_rusage *nrusage)
{
	struct rusage rusage;
	int ok;
	if (who < 0 || who >= 2) {
		errno = _NETBSD_EINVAL;
		return -1;
	}
	who = rusage_map[who];
	ok = getrusage(who, &rusage);
	memset(nrusage, 0, sizeof(struct _netbsd_rusage));
	nrusage->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
	nrusage->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
	nrusage->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
	nrusage->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
	/* TODO add rest of fields */
	return ok;
}

/* use host environment */
char *
emul_getenv(const char *name)
{

	return getenv(name);
}

int
emul_putenv(char *string)
{

	return putenv(string);
}

int
emul_setenv(const char *name, const char *value, int overwrite)
{

	return setenv(name, value, overwrite);
}

int
emul_unsetenv(const char *name)
{

	return unsetenv(name);
}

/* TODO, just lie for now */
int
emul_getenv_r(const char *name, char *buf, size_t len)
{

	errno = _NETBSD_ENOENT;
	return -1;
}

#include <ucontext.h>

void
emul_swapcontext(ucontext_t *old, ucontext_t *new)
{

	swapcontext(old, new);
}

int
rumprun_ucontext(void *ucpvoid, size_t ucpsize,
	void (*start)(void *), void *arg, void *stack_base, size_t stack_size)
{
	ucontext_t *ucp = ucpvoid;

	if (ucpsize < sizeof(*ucp))
		return E2BIG; /* E2SMALL */

	getcontext(ucp);
	ucp->uc_stack.ss_sp = stack_base;
	ucp->uc_stack.ss_size = stack_size;
	ucp->uc_link = NULL;

	/* XXX: makecontext, per spec, only accepts int args ... */
	makecontext(ucp, (void *)start, 1, arg);

	return 0;
}


/*
 * BEGIN stubs
 */

#define STUB(name)                              \
  int name(void); int name(void) {              \
        static int done = 0;                    \
        errno = _NETBSD_ENOTSUP;                        \
        if (done) return errno; done = 1;     \
      /*printk("STUB ``%s'' called\n", #name);*/\
        return errno;}

#define STUB_ABORT(name) void name(void); void name(void) { abort(); }

STUB(emul___sigaction14);
STUB(emul___sigprocmask14);
STUB(emul___sigsuspend14);

STUB(emul___wait450);
STUB(emul_kill);
STUB(emul_uuidgen);

STUB_ABORT(emul_undelete);
