/*
** $Id: atable.c $
** AQL tables (hash) - based on Lua 5.4.8 ltable.c
** See Copyright Notice in aql.h
*/

#define atable_c
#define AQL_CORE

#include "aql.h"
#include <stdio.h>

/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest 'n' such that
** more than half the slots between 1 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the 'original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <math.h>
#include <limits.h>
#include <string.h>

#include "atable.h"
#include "amem.h"
#include "agc.h"
#include "aobject.h"
#include "astate.h"
#include "astring.h"
#include "avm.h"


/*
** MAXABITS is the largest integer such that MAXASIZE fits in an
** unsigned int.
*/
#define MAXABITS	cast_int(sizeof(int) * CHAR_BIT - 1)


/*
** MAXASIZE is the maximum size of the array part. It is the minimum
** between 2^MAXABITS and the maximum size that, measured in bytes,
** fits in a 'size_t'.
*/
#define MAXASIZE	((1u << MAXABITS) > SIZE_MAX/sizeof(TValue) ? SIZE_MAX/sizeof(TValue) : (1u << MAXABITS))

/*
** MAXHBITS is the largest integer such that 2^MAXHBITS fits in a
** signed int.
*/
#define MAXHBITS	(MAXABITS - 1)


/*
** MAXHSIZE is the maximum size of the hash part. It is the minimum
** between 2^MAXHBITS and the maximum size such that, measured in bytes,
** it fits in a 'size_t'.
*/
#define MAXHSIZE	((1u << MAXHBITS) > SIZE_MAX/sizeof(Node) ? SIZE_MAX/sizeof(Node) : (1u << MAXHBITS))

/* Math macros */
#define l_mathop(op)	op

/* Memory functions */
#define aqlM_error(L)	exit(1)  /* Simplified error handling */

#include <stdlib.h>  /* for exit() */

/* Note: All required functions are already implemented in their proper modules:
 * - aqlV_flttointeger is in avm_core.c (VM module)
 * - aqlG_runerror is in aobject.c (debug/error module)  
 * - aqlS_hashlongstr is in astring.c (string module)
 */


/*
** When the original hash value is good, hashing by a power of 2
** avoids the cost of '%'.
*/
#define hashpow2(t,n)		(gnode(t, lmod((n), sizenode(t))))

/*
** for other types, it is better to avoid modulo by power of 2, as
** they can have many 2 factors.
*/
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))


#define hashstr(t,str)		hashpow2(t, (str)->hash)
#define hashboolean(t,p)	hashpow2(t, p)


#define hashpointer(t,p)	hashmod(t, point2uint(p))


#define dummynode		(&dummynode_)

static const Node dummynode_ = {
  {{NULL}, AQL_VEMPTY,  /* value's value and type */
   AQL_TDEADKEY, 0, {NULL}}  /* key type, next, and key value */
};


static const TValue absentkey = {ABSTKEYCONSTANT};


/*
** Hash for integers. To allow a good hash, use the remainder operator
** ('%'). If integer fits as a non-negative int, compute an int
** remainder, which is faster. Otherwise, use an unsigned-integer
** remainder, which uses all bits and ensures a non-negative result.
*/
static Node *hashint (const Table *t, aql_Integer i) {
  aql_Unsigned ui = l_castS2U(i);
  if (ui <= cast_uint(INT_MAX))
    return hashmod(t, cast_int(ui));
  else
    return hashmod(t, ui);
}


/*
** Hash for floating-point numbers.
** The main computation should be just
**     n = frexp(n, &i); return (n * INT_MAX) + i
** but there are some numerical subtleties.
** In a two-complement representation, INT_MAX does not has an exact
** representation as a float, but INT_MIN does; because the absolute
** value of 'frexp' is smaller than 1 (unless 'n' is inf/NaN), the
** absolute value of the product 'frexp * -INT_MIN' is smaller or equal
** to INT_MAX. Next, the use of 'unsigned int' avoids overflows when
** adding 'i'; the use of '~u' (instead of '-u') avoids problems with
** INT_MIN.
*/
#if !defined(l_hashfloat)
static int l_hashfloat (aql_Number n) {
  int i;
  aql_Integer ni;
  n = l_mathop(frexp)(n, &i) * -cast_num(INT_MIN);
  if (!aql_numbertointeger(n, &ni)) {  /* is 'n' inf/-inf/NaN? */
    aql_assert(aql_numisnan(n) || l_mathop(fabs)(n) == cast_num(HUGE_VAL));
    return 0;
  }
  else {  /* normal case */
    unsigned int u = cast_uint(i) + cast_uint(ni);
    return cast_int(u <= cast_uint(INT_MAX) ? u : ~u);
  }
}
#endif


/*
** returns the 'main' position of an element in a table (that is,
** the index of its hash value).
*/
static Node *mainpositionTV (const Table *t, const TValue *key) {
  switch (ttypetag(key)) {
    case AQL_VNUMINT: {
      aql_Integer i = ivalue(key);
      return hashint(t, i);
    }
    case AQL_VNUMFLT: {
      aql_Number n = fltvalue(key);
      return hashmod(t, l_hashfloat(n));
    }
    case AQL_VSHRSTR: {
      TString *ts = tsvalue(key);
      return hashstr(t, ts);
    }
    case AQL_VLNGSTR: {
      TString *ts = tsvalue(key);
      return hashpow2(t, aqlS_hashlongstr(ts));
    }
    case AQL_VFALSE:
      return hashboolean(t, 0);
    case AQL_VTRUE:
      return hashboolean(t, 1);
    case AQL_VLIGHTUSERDATA: {
      void *p = pvalue(key);
      return hashpointer(t, p);
    }
    case AQL_VLCF: {
      aql_CFunction f = fvalue(key);
      return hashpointer(t, f);
    }
    default: {
      GCObject *o = gcvalue(key);
      return hashpointer(t, o);
    }
  }
}


l_sinline Node *mainpositionfromnode (const Table *t, Node *nd) {
  TValue key;
  getnodekey(cast(aql_State *, NULL), &key, nd);
  return mainpositionTV(t, &key);
}


/*
** Check whether key 'k1' is equal to the key in node 'n2'. This
** equality is raw, so there are no metamethods. Floats with integer
** values have been normalized, so integers cannot be equal to
** floats. It is assumed that 'eqshrstr' is simply pointer equality, so
** that short strings are handled in the default case.
** A true 'deadok' means to accept dead keys as equal to their original
** values. All dead keys are compared in the default case, by pointer
** identity. (Only collectable objects can produce dead keys.) Note that
** dead long strings are also compared by identity.
** Once a key is dead, its corresponding value may be collected, and
** then another value can be created with the same address. If this
** other value is given to 'next', 'equalkey' will signal a false
** positive. In a regular traversal, this situation should never happen,
** as all keys given to 'next' came from the table itself, and therefore
** could not have been collected. Outside a regular traversal, we
** have garbage in, garbage out. What is relevant is that this false
** positive does not break anything.  (In particular, 'next' will return
** some other valid item on the table or nil.)
*/
static int equalkey (const TValue *k1, const Node *n2, int deadok) {
  if ((rawtt(k1) != keytt(n2)) &&  /* not the same variants? */
       !(deadok && keyisdead(n2) && iscollectable(k1)))
   return 0;  /* cannot be same key */
  switch (keytt(n2)) {
    case AQL_VNIL: case AQL_VFALSE: case AQL_VTRUE:
      return 1;
    case AQL_VNUMINT:
      return (ivalue(k1) == keyival(n2));
    case AQL_VNUMFLT:
      return aql_numeq(fltvalue(k1), fltvalueraw(keyval(n2)));
    case AQL_VLIGHTUSERDATA:
      return pvalue(k1) == pvalueraw(keyval(n2));
    case AQL_VLCF:
      return fvalue(k1) == fvalueraw(keyval(n2));
    case ctb(AQL_VLNGSTR):
      return aqlS_eqlngstr(tsvalue(k1), keystrval(n2));
    default:
      return gcvalue(k1) == gcvalueraw(keyval(n2));
  }
}


/*
** True if value of 'alimit' is equal to the real size of the array
** part of table 't'. (Otherwise, the array part must be larger than
** 'alimit'.)
*/
#define limitequalsasize(t)	(isrealasize(t) || ispow2((t)->alimit))


/*
** Returns the real size of the 'array' array
*/
AQL_API unsigned int aqlH_realasize (const Table *t) {
  if (limitequalsasize(t))
    return t->alimit;  /* this is the size */
  else {
    unsigned int size = t->alimit;
    /* compute the smallest power of 2 not smaller than 'size' */
    size |= (size >> 1);
    size |= (size >> 2);
    size |= (size >> 4);
    size |= (size >> 8);
#if (UINT_MAX >> 14) > 3  /* unsigned int has more than 16 bits */
    size |= (size >> 16);
#if (UINT_MAX >> 30) > 3
    size |= (size >> 32);  /* unsigned int has more than 32 bits */
#endif
#endif
    size++;
    aql_assert(ispow2(size) && size/2 < t->alimit && t->alimit < size);
    return size;
  }
}


/*
** Check whether real size of the array is a power of 2.
** (If it is not, 'alimit' cannot be changed to any other value
** without changing the real size.)
*/
static int ispow2realasize (const Table *t) {
  return (!isrealasize(t) || ispow2(t->alimit));
}


static unsigned int setlimittosize (Table *t) {
  t->alimit = aqlH_realasize(t);
  setrealasize(t);
  return t->alimit;
}


#define limitasasize(t)	check_exp(isrealasize(t), t->alimit)



/*
** "Generic" get version. (Not that generic: not valid for integers,
** which may be in array part, nor for floats with integral values.)
** See explanation about 'deadok' in function 'equalkey'.
*/
static const TValue *getgeneric (Table *t, const TValue *key, int deadok) {
  Node *n = mainpositionTV(t, key);
  for (;;) {  /* check whether 'key' is somewhere in the chain */
    if (equalkey(key, n, deadok))
      return gval(n);  /* that's it */
    else {
      int nx = gnext(n);
      if (nx == 0)
        return &absentkey;  /* not found */
      n += nx;
    }
  }
}


/*
** returns the index for 'k' if 'k' is an appropriate key to live in
** the array part of a table, 0 otherwise.
*/
static unsigned int arrayindex (aql_Integer k) {
  if (l_castS2U(k) - 1u < MAXASIZE)  /* 'k' in [1, MAXASIZE]? */
    return cast_uint(k);  /* 'key' is an appropriate array index */
  else
    return 0;
}


/*
** returns the index of a 'key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signaled by 0.
*/
static unsigned int findindex (aql_State *L, Table *t, TValue *key,
                               unsigned int asize) {
  unsigned int i;
  if (ttisnil(key)) return 0;  /* first iteration */
  i = ttisinteger(key) ? arrayindex(ivalue(key)) : 0;
  if (i - 1u < asize)  /* is 'key' inside array part? */
    return i;  /* yes; that's the index */
  else {
    const TValue *n = getgeneric(t, key, 1);
    if (l_unlikely(isabstkey(n)))
      aqlG_runerror(L, "invalid key to 'next'");  /* key not found */
    i = cast_int(nodefromval(n) - gnode(t, 0));  /* key index in hash table */
    /* hash elements are numbered after array ones */
    return (i + 1) + asize;
  }
}


int aqlH_next (aql_State *L, Table *t, StkId key) {
  unsigned int asize = aqlH_realasize(t);
  unsigned int i = findindex(L, t, s2v(key), asize);  /* find original key */
  for (; i < asize; i++) {  /* try first array part */
    if (!isempty(&t->array[i])) {  /* a non-empty entry? */
      setivalue(s2v(key), i + 1);
      setobj2s(L, key + 1, &t->array[i]);
      return 1;
    }
  }
  for (i -= asize; cast_int(i) < sizenode(t); i++) {  /* hash part */
    if (!isempty(gval(gnode(t, i)))) {  /* a non-empty entry? */
      Node *n = gnode(t, i);
      getnodekey(L, s2v(key), n);
      setobj2s(L, key + 1, gval(n));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


static void freehash (aql_State *L, Table *t) {
  if (!isdummy(t))
    aqlM_freearray(L, t->node, cast_sizet(sizenode(t)));
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/

/*
** Compute the optimal size for the array part of table 't'. 'nums' is a
** "count array" where 'nums[i]' is the number of integers in the table
** between 2^(i - 1) + 1 and 2^i. 'pna' enters with the total number of
** integer keys in the table and leaves with the number of keys that
** will go to the array part; return the optimal size.  (The condition
** 'twotoi > 0' in the for loop stops the loop if 'twotoi' overflows.)
*/
static unsigned int computesizes (unsigned int nums[], unsigned int *pna) {
  int i;
  unsigned int twotoi;  /* 2^i (candidate for optimal size) */
  unsigned int a = 0;  /* number of elements smaller than 2^i */
  unsigned int na = 0;  /* number of elements to go to array part */
  unsigned int optimal = 0;  /* optimal size for array part */
  /* loop while keys can fill more than half of total size */
  for (i = 0, twotoi = 1;
       twotoi > 0 && *pna > twotoi / 2;
       i++, twotoi *= 2) {
    a += nums[i];
    if (a > twotoi/2) {  /* more than half elements present? */
      optimal = twotoi;  /* optimal size (till now) */
      na = a;  /* all elements up to 'optimal' will go to array part */
    }
  }
  aql_assert((optimal == 0 || optimal / 2 < na) && na <= optimal);
  *pna = na;
  return optimal;
}


static int countint (aql_Integer key, unsigned int *nums) {
  unsigned int k = arrayindex(key);
  if (k != 0) {  /* is 'key' an appropriate array index? */
    nums[aqlO_ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}


/*
** Count keys in array part of table 't': Fill 'nums[i]' with
** number of keys that will go into corresponding slice and return
** total number of non-nil keys.
*/
static unsigned int numusearray (const Table *t, unsigned int *nums) {
  int lg;
  unsigned int ttlg;  /* 2^lg */
  unsigned int ause = 0;  /* summation of 'nums' */
  unsigned int i = 1;  /* count to traverse all array keys */
  unsigned int asize = limitasasize(t);  /* real array size */
  /* traverse each slice */
  for (lg = 0, ttlg = 1; lg <= MAXABITS; lg++, ttlg *= 2) {
    unsigned int lc = 0;  /* counter */
    unsigned int lim = ttlg;
    if (lim > asize) {
      lim = asize;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg - 1), 2^lg] */
    for (; i <= lim; i++) {
      if (!isempty(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}


static int numusehash (const Table *t, unsigned int *nums, unsigned int *pna) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* elements added to 'nums' (can go to array part) */
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!isempty(gval(n))) {
      if (keyisinteger(n))
        ause += countint(keyival(n), nums);
      totaluse++;
    }
  }
  *pna += ause;
  return totaluse;
}


/*
** Creates an array for the hash part of a table with the given
** size, or reuses the dummy node if size is zero.
** The computation for size overflow is in two steps: the first
** comparison ensures that the shift in the second one does not
** overflow.
*/
static void setnodevector (aql_State *L, Table *t, unsigned int size) {
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(Node *, dummynode);  /* use common 'dummynode' */
    t->lsizenode = 0;
    t->lastfree = NULL;  /* signal that it is using dummy node */
  }
  else {
    int i;
    int lsize = aqlO_ceillog2(size);
    if (lsize > MAXHBITS || (1u << lsize) > MAXHSIZE)
      aqlG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = aqlM_newvector(L, size, Node);
    for (i = 0; i < cast_int(size); i++) {
      Node *n = gnode(t, i);
      gnext(n) = 0;
      setnilkey(n);
      setempty(gval(n));
    }
    t->lsizenode = cast_byte(lsize);
    t->lastfree = gnode(t, size);  /* all positions are free */
  }
}


/*
** (Re)insert all elements from the hash part of 'ot' into table 't'.
*/
static void reinsert (aql_State *L, Table *ot, Table *t) {
  int j;
  int size = sizenode(ot);
  for (j = 0; j < size; j++) {
    Node *old = gnode(ot, j);
    if (!isempty(gval(old))) {
      /* doesn't need barrier/invalidate cache, as entry was
         already present in the table */
      TValue k;
      getnodekey(L, &k, old);
      aqlH_set(L, t, &k, gval(old));
    }
  }
}


/*
** Exchange the hash part of 't1' and 't2'.
*/
static void exchangehashpart (Table *t1, Table *t2) {
  aql_byte lsizenode = t1->lsizenode;
  Node *node = t1->node;
  Node *lastfree = t1->lastfree;
  t1->lsizenode = t2->lsizenode;
  t1->node = t2->node;
  t1->lastfree = t2->lastfree;
  t2->lsizenode = lsizenode;
  t2->node = node;
  t2->lastfree = lastfree;
}


/*
** Resize table 't' for the new given sizes. Both allocations (for
** the hash part and for the array part) can fail, which creates some
** subtleties. If the first allocation, for the hash part, fails, an
** error is raised and that is it. Otherwise, it copies the elements from
** the shrinking part of the array (if it is shrinking) into the new
** hash. Then it reallocates the array part.  If that fails, the table
** is in its original state; the function frees the new hash part and then
** raises the allocation error. Otherwise, it sets the new hash part
** into the table, initializes the new part of the array (if any) with
** nils and reinserts the elements of the old hash back into the new
** parts of the table.
*/
void aqlH_resize (aql_State *L, Table *t, unsigned int newasize,
                                          unsigned int nhsize) {
  unsigned int i;
  Table newt;  /* to keep the new hash part */
  unsigned int oldasize = setlimittosize(t);
  TValue *newarray;
  /* create new hash part with appropriate size into 'newt' */
  setnodevector(L, &newt, nhsize);
  if (newasize < oldasize) {  /* will array shrink? */
    t->alimit = newasize;  /* pretend array has new size... */
    exchangehashpart(t, &newt);  /* and new hash */
    /* re-insert into the new hash the elements from vanishing slice */
    for (i = newasize; i < oldasize; i++) {
      if (!isempty(&t->array[i]))
        aqlH_setint(L, t, i + 1, &t->array[i]);
    }
    t->alimit = oldasize;  /* restore current size... */
    exchangehashpart(t, &newt);  /* and hash (in case of errors) */
  }
  /* allocate new array */
  newarray = aqlM_reallocvector(L, t->array, oldasize, newasize, TValue);
  if (l_unlikely(newarray == NULL && newasize > 0)) {  /* allocation failed? */
    freehash(L, &newt);  /* release new hash part */
    aqlM_error(L);  /* raise error (with array unchanged) */
  }
  /* allocation ok; initialize new part of the array */
  exchangehashpart(t, &newt);  /* 't' has the new hash ('newt' has the old) */
  t->array = newarray;  /* set new array part */
  t->alimit = newasize;
  for (i = oldasize; i < newasize; i++)  /* clear new slice of the array */
     setempty(&t->array[i]);
  /* re-insert elements from old hash part into new parts */
  reinsert(L, &newt, t);  /* 'newt' now has the old hash */
  freehash(L, &newt);  /* free old hash part */
}


void aqlH_resizearray (aql_State *L, Table *t, unsigned int nasize) {
  int nsize = allocsizenode(t);
  aqlH_resize(L, t, nasize, nsize);
}

/*
** nums[i] = number of keys 'k' where 2^(i - 1) < k <= 2^i
*/
static void rehash (aql_State *L, Table *t, const TValue *ek) {
  unsigned int asize;  /* optimal size for array part */
  unsigned int na;  /* number of keys in the array part */
  unsigned int nums[MAXABITS + 1];
  int i;
  int totaluse;
  aql_debug("[DEBUG] rehash: 开始rehash，t=%p\n", (void*)t);
  for (i = 0; i <= MAXABITS; i++) nums[i] = 0;  /* reset counts */
  setlimittosize(t);
  aql_debug("[DEBUG] rehash: 计算数组部分键数量\n");
  na = numusearray(t, nums);  /* count keys in array part */
  totaluse = na;  /* all those keys are integer keys */
  aql_debug("[DEBUG] rehash: 数组部分键数量=%u，计算哈希部分键数量\n", na);
  totaluse += numusehash(t, nums, &na);  /* count keys in hash part */
  aql_debug("[DEBUG] rehash: 总键数量=%d，处理额外键\n", totaluse);
  /* count extra key */
  if (ttisinteger(ek))
    na += countint(ivalue(ek), nums);
  totaluse++;
  aql_debug("[DEBUG] rehash: 最终总键数量=%d，计算新尺寸\n", totaluse);
  /* compute new size for array part */
  asize = computesizes(nums, &na);
  aql_debug("[DEBUG] rehash: 新数组尺寸=%u，新哈希尺寸=%d，调用aqlH_resize\n", asize, totaluse - na);
  /* resize the table to new computed sizes */
  aqlH_resize(L, t, asize, totaluse - na);
  aql_debug("[DEBUG] rehash: aqlH_resize完成\n");
}



/*
** }=============================================================
*/


Table *aqlH_new (aql_State *L) {
  GCObject *o = aqlC_newobj(L, AQL_VTABLE, sizeof(Table));
  Table *t = gco2t(o);
  t->metatable = NULL;
  t->flags = cast_byte(maskflags);  /* table has no metamethod fields */
  t->array = NULL;
  t->alimit = 0;
  setnodevector(L, t, 0);
  return t;
}


void aqlH_free (aql_State *L, Table *t) {
  freehash(L, t);
  aqlM_freearray(L, t->array, aqlH_realasize(t));
  aqlM_free(L, t, sizeof(Table));
}


static Node *getfreepos (Table *t) {
  if (!isdummy(t)) {
    while (t->lastfree > t->node) {
      t->lastfree--;
      if (keyisnil(t->lastfree))
        return t->lastfree;
    }
  }
  return NULL;  /* could not find a free place */
}



/*
** inserts a new key into a hash table; first, check whether key's main
** position is free. If not, check whether colliding node is in its main
** position or not: if it is not, move colliding node to an empty place and
** put new key in its main position; otherwise (colliding node is in its main
** position), new key goes to an empty position.
*/
void aqlH_newkey (aql_State *L, Table *t, const TValue *key,
                                                 TValue *value) {
  Node *mp;
  TValue aux;
  aql_debug("[DEBUG] aqlH_newkey: 开始插入新键\n");
  aql_debug("[DEBUG] aqlH_newkey: isdummy(t)=%d, t->lastfree=%p\n", isdummy(t), (void*)t->lastfree);
  aql_debug("[DEBUG] aqlH_newkey: key类型=%d, value类型=%d\n", ttype(key), ttype(value));
  
  if (l_unlikely(ttisnil(key)))
    aqlG_runerror(L, "table index is nil");
  else if (ttisfloat(key)) {
    aql_Number f = fltvalue(key);
    aql_Integer k;
    if (aqlV_flttointeger(f, &k, F2Ieq)) {  /* does key fit in an integer? */
      setivalue(&aux, k);
      key = &aux;  /* insert it as an integer */
    }
    else if (l_unlikely(aql_numisnan(f)))
      aqlG_runerror(L, "table index is NaN");
  }
  if (ttisnil(value)) {
    aql_debug("[DEBUG] aqlH_newkey: 值为nil，跳过插入\n");
    return;  /* do not insert nil values */
  }
  
  aql_debug("[DEBUG] aqlH_newkey: 开始计算主位置\n");
  mp = mainpositionTV(t, key);
  aql_debug("[DEBUG] aqlH_newkey: mp=%p, isempty(gval(mp))=%d\n", (void*)mp, isempty(gval(mp)));
  
  if (!isempty(gval(mp)) || isdummy(t)) {  /* main position is taken? */
    Node *othern;
    aql_debug("[DEBUG] aqlH_newkey: 主位置被占用或表为dummy，需要获取空闲位置\n");
    Node *f = getfreepos(t);  /* get a free place */
    aql_debug("[DEBUG] aqlH_newkey: 需要重新哈希或获取空闲位置，f=%p\n", (void*)f);
    
    if (f == NULL) {  /* cannot find a free place? */
      aql_debug("[DEBUG] aqlH_newkey: 无空闲位置，开始rehash\n");
      rehash(L, t, key);  /* grow table */
      aql_debug("[DEBUG] aqlH_newkey: rehash完成，递归调用aqlH_set\n");
      /* whatever called 'newkey' takes care of TM cache */
      aqlH_set(L, t, key, value);  /* insert key into grown table */
      aql_debug("[DEBUG] aqlH_newkey: rehash完成，递归插入\n");
      return;
    }
    aql_assert(!isdummy(t));
    othern = mainpositionfromnode(t, mp);
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (othern + gnext(othern) != mp)  /* find previous */
        othern += gnext(othern);
      gnext(othern) = cast_int(f - othern);  /* rechain to point to 'f' */
      *f = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      if (gnext(mp) != 0) {
        gnext(f) += cast_int(mp - f);  /* correct 'next' */
        gnext(mp) = 0;  /* now 'mp' is free */
      }
      setempty(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      if (gnext(mp) != 0)
        gnext(f) = cast_int((mp + gnext(mp)) - f);  /* chain new position */
      else aql_assert(gnext(f) == 0);
      gnext(mp) = cast_int(f - mp);
      mp = f;
    }
  }
  aql_debug("[DEBUG] aqlH_newkey: 准备设置节点键，key类型=%d\n", ttype(key));
  setnodekey(L, mp, key);
  gnext(mp) = 0;  /* 初始化next字段为0，表示链表末尾 */
  aql_debug("[DEBUG] aqlH_newkey: 节点键设置完成，keytt(mp)=%d, next=%d\n", keytt(mp), gnext(mp));
  aqlC_barrierback(L, obj2gco(t), key);
  aql_assert(isempty(gval(mp)));
  setobj2t(L, gval(mp), value);
  aql_debug("[DEBUG] aqlH_newkey: 节点值设置完成\n");
}


/*
** Search function for integers. If integer is inside 'alimit', get it
** directly from the array part. Otherwise, if 'alimit' is not
** the real size of the array, the key still can be in the array part.
** In this case, do the "Xmilia trick" to check whether 'key-1' is
** smaller than the real size.
** The trick works as follow: let 'p' be an integer such that
**   '2^(p+1) >= alimit > 2^p', or  '2^(p+1) > alimit-1 >= 2^p'.
** That is, 2^(p+1) is the real size of the array, and 'p' is the highest
** bit on in 'alimit-1'. What we have to check becomes 'key-1 < 2^(p+1)'.
** We compute '(key-1) & ~(alimit-1)', which we call 'res'; it will
** have the 'p' bit cleared. If the key is outside the array, that is,
** 'key-1 >= 2^(p+1)', then 'res' will have some bit on higher than 'p',
** therefore it will be larger or equal to 'alimit', and the check
** will fail. If 'key-1 < 2^(p+1)', then 'res' has no bit on higher than
** 'p', and as the bit 'p' itself was cleared, 'res' will be smaller
** than 2^p, therefore smaller than 'alimit', and the check succeeds.
** As special cases, when 'alimit' is 0 the condition is trivially false,
** and when 'alimit' is 1 the condition simplifies to 'key-1 < alimit'.
** If key is 0 or negative, 'res' will have its higher bit on, so that
** if cannot be smaller than alimit.
*/
const TValue *aqlH_getint (Table *t, aql_Integer key) {
  aql_Unsigned alimit = t->alimit;
  if (l_castS2U(key) - 1u < alimit)  /* 'key' in [1, t->alimit]? */
    return &t->array[key - 1];
  else if (!isrealasize(t) &&  /* key still may be in the array part? */
           (((l_castS2U(key) - 1u) & ~(alimit - 1u)) < alimit)) {
    t->alimit = cast_uint(key);  /* probably '#t' is here now */
    return &t->array[key - 1];
  }
  else {  /* key is not in the array part; check the hash */
    Node *n = hashint(t, key);
    for (;;) {  /* check whether 'key' is somewhere in the chain */
      if (keyisinteger(n) && keyival(n) == key)
        return gval(n);  /* that's it */
      else {
        int nx = gnext(n);
        if (nx == 0) break;
        n += nx;
      }
    }
    return &absentkey;
  }
}


/*
** search function for short strings
*/
const TValue *aqlH_getshortstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  aql_debug("[DEBUG] aqlH_getshortstr: 开始查找key='%s'\n", getstr(key));
  aql_debug("[DEBUG] aqlH_getshortstr: isdummy(t)=%d, n=%p\n", isdummy(t), (void*)n);
  
  /* 注意：TString结构体没有tt字段，类型信息在TValue中 */
  /* aql_assert(key->tt == AQL_VSHRSTR); */  /* 注释掉错误的断言 */
  for (;;) {  /* check whether 'key' is somewhere in the chain */
    aql_debug("[DEBUG] aqlH_getshortstr: 检查节点n=%p, keyisshrstr(n)=%d\n", (void*)n, keyisshrstr(n));
    if (keyisshrstr(n) && eqshrstr(keystrval(n), key)) {
      aql_debug("[DEBUG] aqlH_getshortstr: 找到匹配键，准备返回值\n");
      aql_debug("[DEBUG] aqlH_getshortstr: n=%p, gval(n)=%p\n", (void*)n, (void*)gval(n));
      aql_debug("[DEBUG] aqlH_getshortstr: 调用gval(n)前\n");
      const TValue *result = gval(n);
      aql_debug("[DEBUG] aqlH_getshortstr: 调用gval(n)后，result=%p\n", (void*)result);
      return result;  /* that's it */
    }
    else {
      int nx = gnext(n);
      aql_debug("[DEBUG] aqlH_getshortstr: 键不匹配或非字符串键，nx=%d\n", nx);
      if (nx == 0) {
        aql_debug("[DEBUG] aqlH_getshortstr: 链表结束，未找到\n");
        return &absentkey;  /* not found */
      }
      aql_debug("[DEBUG] aqlH_getshortstr: 更新n从%p到%p\n", (void*)n, (void*)(n + nx));
      n += nx;
    }
  }
}


const TValue *aqlH_getstr (Table *t, TString *key) {
  if (tsslen(key) <= 40)  /* Short string threshold */
    return aqlH_getshortstr(t, key);
  else {  /* for long strings, use generic case */
    TValue ko;
    setsvalue(cast(aql_State *, NULL), &ko, key);
    return getgeneric(t, &ko, 0);
  }
}


/*
** main search function
*/
const TValue *aqlH_get (Table *t, const TValue *key) {
  switch (ttypetag(key)) {
    case AQL_VSHRSTR: return aqlH_getshortstr(t, tsvalue(key));
    case AQL_VNUMINT: return aqlH_getint(t, ivalue(key));
    case AQL_VNIL: return &absentkey;
    case AQL_VNUMFLT: {
      aql_Integer k;
      if (aqlV_flttointeger(fltvalue(key), &k, F2Ieq)) /* integral index? */
        return aqlH_getint(t, k);  /* use specialized version */
      /* else... */
    }  /* FALLTHROUGH */
    default:
      return getgeneric(t, key, 0);
  }
}


/*
** Finish a raw "set table" operation, where 'slot' is where the value
** should have been (the result of a previous "get table").
** Beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
void aqlH_finishset (aql_State *L, Table *t, const TValue *key,
                                   const TValue *slot, TValue *value) {
  if (isabstkey(slot))
    aqlH_newkey(L, t, key, value);
  else
    setobj2t(L, cast(TValue *, slot), value);
}


/*
** beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
void aqlH_set (aql_State *L, Table *t, const TValue *key, TValue *value) {
  const TValue *slot = aqlH_get(t, key);
  aqlH_finishset(L, t, key, slot, value);
}


void aqlH_setint (aql_State *L, Table *t, aql_Integer key, TValue *value) {
  const TValue *p = aqlH_getint(t, key);
  if (isabstkey(p)) {
    TValue k;
    setivalue(&k, key);
    aqlH_newkey(L, t, &k, value);
  }
  else
    setobj2t(L, cast(TValue *, p), value);
}


/*
** Try to find a boundary in the hash part of table 't'. From the
** caller, we know that 'j' is zero or present and that 'j + 1' is
** present. We want to find a larger key that is absent from the
** table, so that we can do a binary search between the two keys to
** find a boundary. We keep doubling 'j' until we get an absent index.
** If the doubling would overflow, we try AQL_MAXINTEGER. If it is
** absent, we are ready for the binary search. ('j', being max integer,
** is larger or equal to 'i', but it cannot be equal because it is
** absent while 'i' is present; so 'j > i'.) Otherwise, 'j' is a
** boundary. ('j + 1' cannot be a present integer key because it is
** not a valid integer in AQL.)
*/
static aql_Unsigned hash_search (Table *t, aql_Unsigned j) {
  aql_Unsigned i;
  if (j == 0) j++;  /* the caller ensures 'j + 1' is present */
  do {
    i = j;  /* 'i' is a present index */
    if (j <= l_castS2U(AQL_MAXINTEGER) / 2)
      j *= 2;
    else {
      j = AQL_MAXINTEGER;
      if (isempty(aqlH_getint(t, j)))  /* t[j] not present? */
        break;  /* 'j' now is an absent index */
      else  /* weird case */
        return j;  /* well, max integer is a boundary... */
    }
  } while (!isempty(aqlH_getint(t, j)));  /* repeat until an absent t[j] */
  /* i < j  &&  t[i] present  &&  t[j] absent */
  while (j - i > 1u) {  /* do a binary search between them */
    aql_Unsigned m = (i + j) / 2;
    if (isempty(aqlH_getint(t, m))) j = m;
    else i = m;
  }
  return i;
}


static unsigned int binsearch (const TValue *array, unsigned int i,
                                                    unsigned int j) {
  while (j - i > 1u) {  /* binary search */
    unsigned int m = (i + j) / 2;
    if (isempty(&array[m - 1])) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table 't'. (A 'boundary' is an integer index
** such that t[i] is present and t[i+1] is absent, or 0 if t[1] is absent
** and 'maxinteger' if t[maxinteger] is present.)
** (In the next explanation, we use AQL indices, that is, with base 1.
** The code itself uses base 0 when indexing the array part of the table.)
** The code starts with 'limit = t->alimit', a position in the array
** part that may be a boundary.
**
** (1) If 't[limit]' is empty, there must be a boundary before it.
** As a common case (e.g., after 't[#t]=nil'), check whether 'limit-1'
** is present. If so, it is a boundary. Otherwise, do a binary search
** between 0 and limit to find a boundary. In both cases, try to
** use this boundary as the new 'alimit', as a hint for the next call.
**
** (2) If 't[limit]' is not empty and the array has more elements
** after 'limit', try to find a boundary there. Again, try first
** the special case (which should be quite frequent) where 'limit+1'
** is empty, so that 'limit' is a boundary. Otherwise, check the
** last element of the array part. If it is empty, there must be a
** boundary between the old limit (present) and the last element
** (absent), which is found with a binary search. (This boundary always
** can be a new limit.)
**
** (3) The last case is when there are no elements in the array part
** (limit == 0) or its last element (the new limit) is present.
** In this case, must check the hash part. If there is no hash part
** or 'limit+1' is absent, 'limit' is a boundary.  Otherwise, call
** 'hash_search' to find a boundary in the hash part of the table.
** (In those cases, the boundary is not inside the array part, and
** therefore cannot be used as a new limit.)
*/
aql_Unsigned aqlH_getn (Table *t) {
  unsigned int limit = t->alimit;
  if (limit > 0 && isempty(&t->array[limit - 1])) {  /* (1)? */
    /* there must be a boundary before 'limit' */
    if (limit >= 2 && !isempty(&t->array[limit - 2])) {
      /* 'limit - 1' is a boundary; can it be a new limit? */
      if (ispow2realasize(t) && !ispow2(limit - 1)) {
        t->alimit = limit - 1;
        setnorealasize(t);  /* now 'alimit' is not the real size */
      }
      return limit - 1;
    }
    else {  /* must search for a boundary in [0, limit] */
      unsigned int boundary = binsearch(t->array, 0, limit);
      /* can this boundary represent the real size of the array? */
      if (ispow2realasize(t) && boundary > aqlH_realasize(t) / 2) {
        t->alimit = boundary;  /* use it as the new limit */
        setnorealasize(t);
      }
      return boundary;
    }
  }
  /* 'limit' is zero or present in table */
  if (!limitequalsasize(t)) {  /* (2)? */
    /* 'limit' > 0 and array has more elements after 'limit' */
    if (isempty(&t->array[limit]))  /* 'limit + 1' is empty? */
      return limit;  /* this is the boundary */
    /* else, try last element in the array */
    limit = aqlH_realasize(t);
    if (isempty(&t->array[limit - 1])) {  /* empty? */
      /* there must be a boundary in the array after old limit,
         and it must be a valid new limit */
      unsigned int boundary = binsearch(t->array, t->alimit, limit);
      t->alimit = boundary;
      return boundary;
    }
    /* else, new limit is present in the table; check the hash part */
  }
  /* (3) 'limit' is the last element and either is zero or present in table */
  aql_assert(limit == aqlH_realasize(t) &&
             (limit == 0 || !isempty(&t->array[limit - 1])));
  if (isdummy(t) || isempty(aqlH_getint(t, cast(aql_Integer, limit + 1))))
    return limit;  /* 'limit + 1' is absent */
  else  /* 'limit + 1' is also present */
    return hash_search(t, limit);
}



#if defined(AQL_DEBUG)

/* export these functions for the test library */

Node *aqlH_mainposition (const Table *t, const TValue *key) {
  return mainpositionTV(t, key);
}

#endif
