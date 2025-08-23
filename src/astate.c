/*
** $Id: astate.c $
** Global State
** See Copyright Notice in aql.h
*/

#define astate_c
#define AQL_CORE

#include "aconf.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "aql.h"
#include "astate.h"
#include "aobject.h"
#include "amem.h"
#include "afunc.h"
#include "astring.h"

/*
** thread state + extra space
*/
typedef struct AX {
    aql_byte extra_[AQL_EXTRASPACE];
    aql_State l;
} AX;

/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
    AX l;
    global_State g;
} LG;

#define fromstate(L)	(cast(AX *, cast(aql_byte *, (L)) - offsetof(AX, l)))

/*
** Additional type definitions
*/
typedef int (*Pfunc) (aql_State *L, void *ud);

/*
** Forward declarations for placeholder functions
*/
void aqlD_errerr(aql_State *L);
int aqlD_closeprotected(aql_State *L, ptrdiff_t level, int status);
void aqlD_seterrorobj(aql_State *L, int errcode, StkId oldtop);
void aqlD_reallocstack(aql_State *L, int newsize, int raiseerror);
/* aqlF_closeupval moved to afunc.c */
void aqlC_freeallobjects(aql_State *L);
void aqlai_userstateclose(aql_State *L);
void aqlai_userstatethread(aql_State *L, aql_State *L1);
void aqlai_userstatefree(aql_State *L, aql_State *L1);
int aqlD_rawrunprotected(aql_State *L, Pfunc f, void *ud);

/*
** Missing lock/unlock macros
*/
#define aql_lock(L) ((void)0)
#define aql_unlock(L) ((void)0)

/*
** Missing extra space function
*/
#define aql_getextraspace(L) ((void *)((char *)(L) - AQL_EXTRASPACE))

/*
** Missing GC functions
*/
#define aqlC_checkGC(L) ((void)0)
#define aqlC_white(g) (0)
#define aqlC_newobjdt(L,t,sz,offset) ((GCObject *)aqlM_malloc_tagged(L, sz, t))

/*
** Helper function macros
*/
#define getCcalls(L) ((L)->nCcalls)
#define incnny(L) /* no-op for now */
#define resethookcount(L) (L->hookcount = L->basehookcount)
#define gettotalbytes(g) cast(lu_mem, (g)->totalbytes + (g)->GCdebt)
#define completestate(g) ttisnil(&(g)->nilvalue)
#define setgcparam(p,v) ((p) = (v))
#define api_incr_top(L) {L->top++;}

/*
** Constants
*/
#define BASIC_STACK_SIZE (2*AQL_MINSTACK)
#define EXTRA_STACK 5
#define MAX_LMEM ((lu_mem)(~(lu_mem)0) - 2)
#define WHITE0BIT 0
#define GCSTPGC 1
#define GCSpause 0
#define KGC_INC 0
#define AQL_RIDX_MAINTHREAD 1
#define AQL_RIDX_GLOBALS 2
#define AQL_RIDX_LAST AQL_RIDX_GLOBALS

/*
** A macro to create a "random" seed when a state is created;
** the seed is used to randomize string hashes.
*/
static unsigned int aqlai_makeseed (aql_State *L) {
    UNUSED(L);
    return (unsigned int)time(NULL);
}

/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void aqlE_setdebt (global_State *g, l_mem debt) {
    l_mem tb = gettotalbytes(g);
    aql_assert(tb > 0);
    if (debt < (l_mem)(tb - MAX_LMEM))
        debt = (l_mem)(tb - MAX_LMEM);  /* will make 'totalbytes == MAX_LMEM' */
    g->totalbytes = tb - debt;
    g->GCdebt = debt;
}

AQL_API int aql_setcstacklimit (aql_State *L, unsigned int limit) {
    UNUSED(L); UNUSED(limit);
    return AQLAI_MAXCCALLS;  /* warning?? */
}

CallInfo *aqlE_extendCI (aql_State *L) {
    CallInfo *ci;
    aql_assert(L->ci->next == NULL);
    ci = aqlM_new(L, CallInfo);
    aql_assert(L->ci->next == NULL);
    L->ci->next = ci;
    ci->previous = L->ci;
    ci->next = NULL;
    ci->u.l.trap = 0;
    L->nci++;
    return ci;
}

/*
** free all CallInfo structures not in use by a thread
*/
static void freeCI (aql_State *L) {
    CallInfo *ci = L->ci;
    CallInfo *next = ci->next;
    ci->next = NULL;
    while ((ci = next) != NULL) {
        next = ci->next;
        aqlM_free(L, ci, sizeof(CallInfo));
        L->nci--;
    }
}

/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void aqlE_shrinkCI (aql_State *L) {
    CallInfo *ci = L->ci->next;  /* first free CallInfo */
    CallInfo *next;
    if (ci == NULL)
        return;  /* no extra elements */
    while ((next = ci->next) != NULL) {  /* two extra elements? */
        CallInfo *next2 = next->next;  /* next's next */
        ci->next = next2;  /* remove next from the list */
        L->nci--;
        aqlM_free(L, next, sizeof(CallInfo));  /* free next */
        if (next2 == NULL)
            break;  /* no more elements */
        else {
            next2->previous = ci;
            ci = next2;  /* continue */
        }
    }
}

/*
** Called when 'getCcalls(L)' larger or equal to AQLAI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** AQLAI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void aqlE_checkcstack (aql_State *L) {
    if (getCcalls(L) == AQLAI_MAXCCALLS)
        aqlG_runerror(L, "C stack overflow");
    else if (getCcalls(L) >= (AQLAI_MAXCCALLS / 10 * 11))
        aqlD_errerr(L);  /* error while handling stack error */
}

static void aqlE_incCstack (aql_State *L) {
    L->nCcalls++;
    if (l_unlikely(getCcalls(L) >= AQLAI_MAXCCALLS))
        aqlE_checkcstack(L);
}

static void stack_init (aql_State *L1, aql_State *L) {
    int i; CallInfo *ci;
    /* initialize stack array */
    L1->stack = aqlM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
    for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
        setnilvalue(s2v(L1->stack + i));  /* erase new stack */
    L1->top = L1->stack;
    L1->stack_last = L1->stack + BASIC_STACK_SIZE;
    /* initialize first ci */
    ci = &L1->base_ci;
    ci->next = ci->previous = NULL;
    ci->callstatus = CIST_C;
    ci->func = L1->top;
    ci->u.c.k = NULL;
    ci->nresults = 0;
    setnilvalue(s2v(L1->top));  /* 'function' entry for this 'ci' */
    L1->top++;
    ci->top = L1->top + AQL_MINSTACK;
    L1->ci = ci;
}

static void freestack (aql_State *L) {
    if (L->stack == NULL)
        return;  /* stack not completely built yet */
    L->ci = &L->base_ci;  /* free the entire 'ci' list */
    freeCI(L);
    aql_assert(L->nci == 0);
    aqlM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}

/*
** Create registry table and its predefined values
*/
static void init_registry (aql_State *L, global_State *g) {
    /* For MVP, just set registry to nil */
    setnilvalue(&g->l_registry);
}

/*
** open parts of the state that may cause memory-allocation errors.
*/
static int f_aqlopen (aql_State *L, void *ud) {
    global_State *g = G(L);
    UNUSED(ud);
    stack_init(L, L);  /* init stack */
    init_registry(L, g);
    aqlStr_init(L);  /* init string system */
    /* Skip subsystem initialization for MVP */
    g->gcemergency = 0;  /* allow gc */
    setnilvalue(&g->nilvalue);  /* now state is complete */
    return AQL_OK;
}

/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (aql_State *L, global_State *g) {
    G(L) = g;
    L->stack = NULL;
    L->ci = NULL;
    L->nci = 0;
    L->twups = L;  /* thread has no upvalues */
    L->nCcalls = 0;
    L->errorJmp = NULL;
    L->hook = NULL;
    L->hookmask = 0;
    L->basehookcount = 0;
    L->allowhook = 1;
    resethookcount(L);
    L->openupval = NULL;
    L->status = AQL_OK;
    L->errfunc = 0;
    L->oldpc = 0;
}

static void close_state (aql_State *L) {
    global_State *g = G(L);
    if (!completestate(g))  /* closing a partially built state? */
        aqlC_freeallobjects(L);  /* just collect its objects */
    else {  /* closing a fully built state */
        L->ci = &L->base_ci;  /* unwind CallInfo list */
        L->errfunc = 0;   /* stack unwind can "throw away" the error function */
        aqlD_closeprotected(L, 1, AQL_OK);  /* close all upvalues */
        L->top = L->stack + 1;  /* empty the stack to run finalizers */
        aqlC_freeallobjects(L);  /* collect all objects */
        aqlai_userstateclose(L);
    }
    aqlM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
    freestack(L);
    aql_assert(gettotalbytes(g) == sizeof(LG));
    (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}

AQL_API aql_State *aql_newthread (aql_State *L) {
    global_State *g = G(L);
    GCObject *o;
    aql_State *L1;
    aql_lock(L);
    aqlC_checkGC(L);
    /* create new thread */
    o = aqlC_newobjdt(L, AQL_TTHREAD, sizeof(AX), offsetof(AX, l));
    L1 = gco2th(o);
    /* anchor it on L stack */
    setthvalue2s(L, L->top, L1);
    api_incr_top(L);
    preinit_thread(L1, g);
    L1->hookmask = L->hookmask;
    L1->basehookcount = L->basehookcount;
    L1->hook = L->hook;
    resethookcount(L1);
    /* initialize L1 extra space */
    memcpy(aql_getextraspace(L1), aql_getextraspace(g->mainthread),
           AQL_EXTRASPACE);
    aqlai_userstatethread(L, L1);
    stack_init(L1, L);  /* init stack */
    aql_unlock(L);
    return L1;
}

void aqlE_freethread (aql_State *L, aql_State *L1) {
    AX *l = fromstate(L1);
    aqlF_closeupval(L1, L1->stack);  /* close all upvalues */
    aql_assert(L1->openupval == NULL);
    aqlai_userstatefree(L, L1);
    freestack(L1);
    aqlM_free(L, l, sizeof(AX));
}

int aqlE_resetthread (aql_State *L, int status) {
    CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
    setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
    ci->func = L->stack;
    ci->callstatus = CIST_C;
    if (status == AQL_YIELD)
        status = AQL_OK;
    L->status = AQL_OK;  /* so it can run __close metamethods */
    L->errfunc = 0;   /* stack unwind can "throw away" the error function */
    status = aqlD_closeprotected(L, 1, status);
    if (status != AQL_OK)  /* errors? */
        aqlD_seterrorobj(L, status, L->stack + 1);
    else
        L->top = L->stack + 1;
    ci->top = L->top + AQL_MINSTACK;
    aqlD_reallocstack(L, cast_int(ci->top - L->stack), 0);
    return status;
}

AQL_API int aql_closethread (aql_State *L, aql_State *from) {
    int status;
    aql_lock(L);
    L->nCcalls = (from) ? getCcalls(from) : 0;
    status = aqlE_resetthread(L, L->status);
    aql_unlock(L);
    return status;
}

/*
** Deprecated! Use 'aql_closethread' instead.
*/
AQL_API int aql_resetthread (aql_State *L, aql_State *from) {
    return aql_closethread(L, from);
}

AQL_API aql_State *aql_newstate (aql_Alloc f, void *ud) {
    int i;
    aql_State *L;
    global_State *g;
    LG *l = cast(LG *, (*f)(ud, NULL, AQL_TTHREAD, sizeof(LG)));
    if (l == NULL) return NULL;
    L = &l->l.l;
    g = &l->g;
    L->marked = 0;  /* will be set by GC */
    g->currentwhite = (1<<WHITE0BIT);  /* bitmask(WHITE0BIT) */
    L->marked = aqlC_white(g);
    preinit_thread(L, g);
    g->allgc = obj2gco(L);  /* by now, only object is the main thread */
    L->next = NULL;
    incnny(L);  /* main thread is always non yieldable */
    g->frealloc = f;
    g->ud = ud;
    g->warnf = NULL;
    g->ud_warn = NULL;
    g->mainthread = L;
    g->seed = aqlai_makeseed(L);
    g->gcemergency = GCSTPGC;  /* no GC while building state */
    g->strt.size = g->strt.nuse = 0;
    g->strt.hash = NULL;
    setnilvalue(&g->l_registry);
    g->panic = NULL;
    g->gcstate = GCSpause;
    g->gckind = KGC_INC;
    g->gcemergency = 0;
    g->finobj = g->tobefnz = g->fixedgc = NULL;
    g->survival = g->old1 = g->reallyold = NULL;
    g->firstold1 = NULL;
    g->finoold = NULL;
    g->sweepgc = NULL;
    g->gray = g->grayagain = NULL;
    g->weak = g->ephemeron = g->allweak = NULL;
    g->twups = NULL;
    g->totalbytes = sizeof(LG);
    g->GCdebt = 0;
    g->lastatomic = 0;
    setivalue(&g->nilvalue, 0);  /* to signal that state is not yet built */
    setgcparam(g->gcpause, AQLAI_GCPAUSE);
    setgcparam(g->gcstepmul, AQLAI_GCMUL);
    g->gcstepsize = AQLAI_GCSTEPSIZE;
    setgcparam(g->genmajormul, 100);  /* AQLAI_GENMAJORMUL */
    g->genminormul = 20;  /* AQLAI_GENMINORMUL */
    for (i=0; i < AQL_NUMTYPES; i++) g->mt[i] = NULL;
    if (aqlD_rawrunprotected(L, f_aqlopen, NULL) != AQL_OK) {
        /* memory allocation error: free partial state */
        close_state(L);
        L = NULL;
    }
    return L;
}

AQL_API void aql_close (aql_State *L) {
    aql_lock(L);
    L = G(L)->mainthread;  /* only the main thread can be closed */
    close_state(L);
}

void aqlE_warning (aql_State *L, const char *msg, int tocont) {
    aql_WarnFunction wf = G(L)->warnf;
    if (wf != NULL)
        wf(G(L)->ud_warn, msg, tocont);
}

/*
** Generate a warning from an error message
*/
void aqlE_warnerror (aql_State *L, const char *where) {
    TValue *errobj = s2v(L->top - 1);  /* error object */
    const char *msg = (ttisstring(errobj))
                    ? getstr(tsvalue(errobj))
                    : "error object is not a string";
    /* produce warning "error in %s (%s)" (where, msg) */
    aqlE_warning(L, "error in ", 1);
    aqlE_warning(L, where, 1);
    aqlE_warning(L, " (", 1);
    aqlE_warning(L, msg, 1);
    aqlE_warning(L, ")", 0);
}

/*
** Placeholder implementations for missing functions
*/

/* Error handling */
void aqlD_errerr(aql_State *L) {
    aqlG_runerror(L, "error in error handling");
}

/* Protected close */
int aqlD_closeprotected(aql_State *L, ptrdiff_t level, int status) {
    UNUSED(L); UNUSED(level);
    return status;  /* Simplified - no actual closing */
}

/* Error object setting */
void aqlD_seterrorobj(aql_State *L, int errcode, StkId oldtop) {
    UNUSED(L); UNUSED(errcode); UNUSED(oldtop);
    /* Error object setting - placeholder */
}

/* Stack reallocation */
void aqlD_reallocstack(aql_State *L, int newsize, int raiseerror) {
    UNUSED(L); UNUSED(newsize); UNUSED(raiseerror);
    /* Stack reallocation - placeholder */
}

/* Upvalue closing - moved to afunc.c */

/* GC functions - implemented in agc.c */

/* User state functions */
void aqlai_userstateclose(aql_State *L) {
    UNUSED(L);
}

void aqlai_userstatethread(aql_State *L, aql_State *L1) {
    UNUSED(L); UNUSED(L1);
}

void aqlai_userstatefree(aql_State *L, aql_State *L1) {
    UNUSED(L); UNUSED(L1);
}

/* Protected call implementation */
int aqlD_rawrunprotected(aql_State *L, Pfunc f, void *ud) {
    return (*f)(L, ud);  /* Simplified - no actual protection */
}