#define	setcontext(u)	_qt_setmctxt(&(u)->mc)
#define	getcontext(u)	_qt_getmctxt(&(u)->mc)
typedef struct mcontext mcontext_t;
typedef struct ucontext ucontext_t;
struct mcontext
{
	unsigned long	pc;		/* lr */
	unsigned long	cr;		/* condition register (mfcr) */
	unsigned long	ctr;		/* count register, for branching (mfcr) */
	unsigned long	xer;		/* xer register, an optional-condition register (mfcr) */
	unsigned long	sp;		/* callee saved: r1 */
	unsigned long	toc;		/* callee saved: r2 */
	unsigned long	r3;		/* first arg to function, return register: r3 */
	unsigned long	gpr[19];	/* callee saved: r13-r31 */
	/* double-aligned for performance */
	unsigned long pad;
	double fpregs[18];		/* callee saved: r14-r31 */
/*
// XXX: currently do not save vector registers
//	unsigned long	vr[4*12];	/ * callee saved: v20-v31, 256-bits each * /
//      unsigned long   vrsave;         / * which v regs should be saved? * /
*/
};

struct ucontext
{
	struct {
		void *ss_sp;
		uint ss_size;
	} uc_stack;
	sigset_t uc_sigmask;
	mcontext_t mc;
	struct ucontext * uc_link; /* unused */
};

void qt_makectxt(ucontext_t*, void(*)(void), int, ...);
int qt_swapctxt(ucontext_t*, ucontext_t*);
int _qt_getmctxt(mcontext_t*);
void _qt_setmctxt(mcontext_t*);

