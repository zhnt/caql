/*
** $Id: agc.h $
** Garbage Collector
** See Copyright Notice in aql.h
*/

#ifndef agc_h
#define agc_h

#include "aobject.h"

/*
** Collectable objects may have one of three colors: white, which means
** the object is not marked; gray, which means the object is marked, but
** its references may be not marked; and black, which means that the
** object and all its references are marked.  The main invariant of the
** garbage collector, while marking objects, is that a black object can
** never point to a white object. Moreover, any gray object must be in a
** "gray list" (gray, grayagain, weak, allweak, ephemeron) so that it
** can be visited again before finishing the collection cycle. These lists
** have no meaning when the invariant is not being enforced (e.g., sweep
** phase).
*/

/* how much to allocate before next GC step */
#if !defined(GCSTEPSIZE)
/* ~100 small strings */
#define GCSTEPSIZE	(cast_int(100 * sizeof(TString)))
#endif

/*
** Possible states of the Garbage Collector
*/
#define GCSpropagate	0
#define GCSatomic	1
#define GCSswpallgc	2
#define GCSswpfinobj	3
#define GCSswptobefnz	4
#define GCSswpend	5
#define GCScallfin	6
#define GCSpause	7

#define issweepphase(g)  \
	(GCSswpallgc <= (g)->gcstate && (g)->gcstate <= GCSswpend)

/*
** macro to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep
** phase may break the invariant, as objects turned white may point to
** still-black objects. The invariant is restored when sweep ends and
** all objects are white again.
*/

#define keepinvariant(g)	((g)->gcstate <= GCSatomic)

/*
** some useful bit tricks
*/
#define resetbits(x,m)		((x) &= cast_byte(~(m)))
#define setbits(x,m)		((x) |= (m))
#define testbits(x,m)		((x) & (m))
#define bitmask(b)		(1<<(b))
#define bit2mask(b1,b2)		(bitmask(b1) | bitmask(b2))
#define l_setbit(x,b)		setbits(x, bitmask(b))
#define resetbit(x,b)		resetbits(x, bitmask(b))
#define testbit(x,b)		testbits(x, bitmask(b))

/*
** Layout for bit use in 'marked' field. First three bits are
** used for object "age" in generational mode. Last bit is used
** by tests.
*/
#define WHITE0BIT	3  /* object is white (type 0) */
#define WHITE1BIT	4  /* object is white (type 1) */
#define BLACKBIT	5  /* object is black */
#define FINALIZEDBIT	6  /* object has been marked for finalization */

#define TESTBIT		7

#define WHITEBITS	bit2mask(WHITE0BIT, WHITE1BIT)

#define iswhite(x)      testbits((x)->marked, WHITEBITS)
#define isblack(x)      testbit((x)->marked, BLACKBIT)
#define isgray(x)  /* neither white nor black */  \
	(!testbits((x)->marked, WHITEBITS | bitmask(BLACKBIT)))

#define tofinalize(x)	testbit((x)->marked, FINALIZEDBIT)

#define otherwhite(g)	((g)->currentwhite ^ WHITEBITS)
#define isdeadm(ow,m)	(!((m) & bitmask(BLACK BIT)) && ((m) & (ow)))
#define isdead(g,v)	isdeadm(otherwhite(g), (v)->marked)

#define changewhite(x)	((x)->marked ^= WHITEBITS)
#define nw2black(x)  \
	check_exp(!iswhite(x), l_setbit((x)->marked, BLACKBIT))

#define aqlC_white(g)	cast_byte((g)->currentwhite & WHITEBITS)

/*
** Does one step of collection when debt becomes positive. 'pre'/'pos'
** allows some adjustments to be done only when needed. macro
** 'condchangemem' is used only for heavy tests (forcing a full
** GC cycle on every opportunity)
*/
#define aqlC_condGC(L,pre,pos) \
	{ if (G(L)->GCdebt > 0) { pre; aqlC_step(L); pos;}; \
	  condchangemem(L,pre,pos); }

/* more often than not, 'pre'/'pos' are empty */
#define aqlC_checkGC(L)		aqlC_condGC(L,(void)0,(void)0)

#define aqlC_objbarrier(L,p,o) (  \
	(iscollectable(o) && isblack(p) && iswhite(gcvalue(o))) ? \
	aqlC_barrier_(L,obj2gco(p),gcvalue(o)) : cast_void(0))

#define aqlC_barrierback(L,p,v) (  \
	(iscollectable(v) && isblack(p) && iswhite(gcvalue(v))) ? \
	aqlC_barrierback_(L,p) : cast_void(0))

#define aqlC_objbarrierback(L,p,o) (  \
	(iscollectable(o) && isblack(p) && iswhite(gcvalue(o))) ? \
	aqlC_barrierback_(L,obj2gco(p)) : cast_void(0))

AQL_API void aqlC_fix(aql_State *L, GCObject *o);
AQL_API void aqlC_freeallobjects(aql_State *L);
AQL_API void aqlC_step(aql_State *L);
AQL_API void aqlC_runtilstate(aql_State *L, int statesmask);
AQL_API void aqlC_fullgc(aql_State *L, int isemergency);
AQL_API GCObject *aqlC_newobj(aql_State *L, int tt, size_t sz);
AQL_API void aqlC_barrier_(aql_State *L, GCObject *o, GCObject *v);
AQL_API void aqlC_barrierback_(aql_State *L, GCObject *o);
AQL_API void aqlC_upvalbarrier(aql_State *L, UpVal *uv);
AQL_API void aqlC_checkfinalizer(aql_State *L, GCObject *o, Table *mt);
AQL_API void aqlC_upvdeccount(aql_State *L, UpVal *uv);

#endif /* agc_h */ 