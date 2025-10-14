/*
** $Id: atable.h $
** AQL tables (hash) - based on Lua 5.4.8 ltable.h
** See Copyright Notice in aql.h
*/

#ifndef atable_h
#define atable_h

#include "aobject.h"
#include "adebug.h"

/*
** {==================================================================
** Table and Node Structure Definitions
** ===================================================================
*/

/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries. The distribution
** of the key's fields ('key_tt' and 'key_val') not forming a proper
** 'TValue' allows for a smaller size for 'Node' both in 4-byte
** and 8-byte alignments.
*/
typedef union Node {
  struct NodeKey {
    TValuefields;  /* fields for value */
    aql_byte key_tt;  /* key type */
    int next;  /* for chaining */
    Value key_val;  /* key value */
  } u;
  TValue i_val;  /* direct access to node's value as a proper 'TValue' */
} Node;

/* copy a value into a key */
#define setnodekey(L,node,obj) \
	{ Node *n_=(node); const TValue *io_=(obj); \
	  n_->u.key_val = io_->value_; n_->u.key_tt = io_->tt_; \
	  checkliveness(L,io_); }

/* copy a value from a key */
#define getnodekey(L,obj,node) \
	{ TValue *io_=(obj); const Node *n_=(node); \
	  io_->value_ = n_->u.key_val; io_->tt_ = n_->u.key_tt; \
	  checkliveness(L,io_); }

/*
** About 'alimit': if 'isrealasize(t)' is true, then 'alimit' is the
** real size of 'array'. Otherwise, the real size of 'array' is the
** smallest power of two not smaller than 'alimit' (or zero iff 'alimit'
** is zero); 'alimit' is then used as a hint for #t.
*/

#define BITRAS		(1 << 7)
#define isrealasize(t)		(!((t)->flags & BITRAS))
#define setrealasize(t)		((t)->flags &= cast_byte(~BITRAS))
#define setnorealasize(t)	((t)->flags |= BITRAS)

typedef struct Table {
  CommonHeader;
  aql_byte flags;  /* 1<<p means tagmethod(p) is not present */
  aql_byte lsizenode;  /* log2 of size of 'node' array */
  unsigned int alimit;  /* "limit" of 'array' array */
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  struct Table *metatable;
  GCObject *gclist;
} Table;

/*
** Macros to manipulate keys inserted in nodes
*/
#define keytt(node)		((node)->u.key_tt)
#define keyval(node)		((node)->u.key_val)

#define keyisnil(node)		(keytt(node) == AQL_TNIL)
#define keyisinteger(node)	(keytt(node) == AQL_VNUMINT)
#define keyival(node)		(keyval(node).i)
#define keyisshrstr(node)	(keytt(node) == ctb(AQL_VSHRSTR))
#define keystrval(node)		(gco2ts(keyval(node).gc))

#define setnilkey(node)		(keytt(node) = AQL_TNIL)

#define keyiscollectable(node)	(keytt(node) & BIT_ISCOLLECTABLE)

#define gckey(n)	(keyval(n).gc)
#define gckeyN(n)	(keyiscollectable(n) ? gckey(n) : NULL)

/* Additional key type checks */
#define keyisdead(n)		(keytt(n) == AQL_TDEADKEY)
#define keyisempty(n)		(keytt(n) == AQL_TNIL)

/* }================================================================== */

/*
** {==================================================================
** Table Access Macros
** ===================================================================
*/

#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)

/* Size of hash part */
#define sizenode(t)	(twoto((t)->lsizenode))

/* Utility macros */
#define twoto(x)	(1<<(x))
#define ispow2(x)	(((x) & ((x) - 1)) == 0)

/* Table flags for metamethods */
#define maskflags	(~0u)  /* Simplified for AQL */

/* Array size checks - use Lua's BITRAS system */
/* Note: isrealasize, setrealasize, setnorealasize are defined above */

/* Modulo operations for hash */
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast_int((s) & ((size)-1)))))

/* Empty value checks - use AQL's definitions from aobject.h */
#define isempty(v)		ttisnil(v)
/* Note: setempty, ABSTKEYCONSTANT, isabstkey are defined in aobject.h */

/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= ~maskflags)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))


AQL_API const TValue *aqlH_getint (Table *t, aql_Integer key);
AQL_API void aqlH_setint (aql_State *L, Table *t, aql_Integer key,
                                                    TValue *value);
AQL_API const TValue *aqlH_getshortstr (Table *t, TString *key);
AQL_API const TValue *aqlH_getstr (Table *t, TString *key);
AQL_API const TValue *aqlH_get (Table *t, const TValue *key);
AQL_API void aqlH_set (aql_State *L, Table *t, const TValue *key,
                                                 TValue *value);
AQL_API void aqlH_finishset (aql_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
AQL_API void aqlH_newkey (aql_State *L, Table *t, const TValue *key,
                                                 TValue *value);
AQL_API Table *aqlH_new (aql_State *L);
AQL_API void aqlH_resize (aql_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
AQL_API void aqlH_resizearray (aql_State *L, Table *t, unsigned int nasize);
AQL_API void aqlH_free (aql_State *L, Table *t);
AQL_API int aqlH_next (aql_State *L, Table *t, StkId key);
AQL_API aql_Unsigned aqlH_getn (Table *t);
AQL_API unsigned int aqlH_realasize (const Table *t);


#if defined(AQL_DEBUG)
AQL_API Node *aqlH_mainposition (const Table *t, const TValue *key);
#endif


#endif
