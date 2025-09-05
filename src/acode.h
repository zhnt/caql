/*
** $Id: acode.h $
** Code generator for AQL
** See Copyright Notice in aql.h
*/

#ifndef acode_h
#define acode_h

#include "alex.h"
#include "aobject.h"
#include "aopcodes.h"

/* Forward declarations */
typedef enum UnOpr UnOpr;
typedef enum BinOpr BinOpr;

/*
** Maximum number of registers in an AQL function
** (must fit in an unsigned byte)
*/
#define MAXREGS         254

/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)

/*
** Prototypes for code generation
*/
AQL_API int aqlK_code(FuncState *fs, Instruction i);
AQL_API int aqlK_codeABC(FuncState *fs, OpCode o, int a, int b, int c);
AQL_API int aqlK_codeABx(FuncState *fs, OpCode o, int a, unsigned int bx);
AQL_API int aqlK_codeAsBx(FuncState *fs, OpCode o, int a, int bx);
AQL_API int aqlK_codeAx(FuncState *fs, OpCode o, unsigned int ax);
AQL_API int aqlK_codek(FuncState *fs, int reg, int k);
AQL_API void aqlK_fixline(FuncState *fs, int line);
AQL_API void aqlK_nil(FuncState *fs, int from, int n);
AQL_API int aqlK_jump(FuncState *fs);
AQL_API int aqlK_ret(FuncState *fs, int first, int nret);
AQL_API void aqlK_patchlist(FuncState *fs, int list, int target);
AQL_API void aqlK_patchtohere(FuncState *fs, int list);
AQL_API void aqlK_concat(FuncState *fs, int *l1, int l2);
AQL_API int aqlK_getlabel(FuncState *fs);
AQL_API void aqlK_patchclose(FuncState *fs, int list, int level);
AQL_API void aqlK_finish(FuncState *fs);

/*
** Expression handling
*/
struct expdesc;  /* forward declaration */
AQL_API void aqlK_exp2nextreg(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_exp2anyreg(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_exp2anyregup(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_exp2val(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_setmultret(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_int(FuncState *fs, int reg, aql_Integer i);
AQL_API void aqlK_float(FuncState *fs, int reg, aql_Number f);
AQL_API int aqlK_numberK(FuncState *fs, aql_Number r);
AQL_API int aqlK_exp2RK(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_storevar(FuncState *fs, struct expdesc *var, struct expdesc *e);
AQL_API void aqlK_self(FuncState *fs, struct expdesc *e, struct expdesc *key);
AQL_API void aqlK_indexed(FuncState *fs, struct expdesc *t, struct expdesc *k);
AQL_API void aqlK_goiftrue(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_goiffalse(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_dischargevars(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_checkstack(FuncState *fs, int n);
AQL_API int aqlK_reserveregs(FuncState *fs, int n);
AQL_API void aqlK_freereg(FuncState *fs, int reg);
AQL_API void aqlK_freeexp(FuncState *fs, struct expdesc *e);
AQL_API int aqlK_addk(FuncState *fs, TValue *key, TValue *v);
AQL_API int aqlK_stringK(FuncState *fs, TString *s);
AQL_API int aqlK_intK(FuncState *fs, aql_Integer n);
AQL_API int aqlK_numberK(FuncState *fs, aql_Number r);

/*
** AQL-specific code generation
*/

/* Container operations */
AQL_API void aqlK_newarray(FuncState *fs, struct expdesc *e, int size, int type);
AQL_API void aqlK_newslice(FuncState *fs, struct expdesc *e, int type);
AQL_API void aqlK_newdict(FuncState *fs, struct expdesc *e, int keytype, int valtype);
AQL_API void aqlK_newvector(FuncState *fs, struct expdesc *e, int type, int size);

/* Type operations */
AQL_API void aqlK_checktype(FuncState *fs, struct expdesc *e, int expected_type);
AQL_API void aqlK_casttype(FuncState *fs, struct expdesc *e, int target_type);
AQL_API int aqlK_gettypeinfo(FuncState *fs, struct expdesc *e);

/* Generic operations */
AQL_API void aqlK_generic_call(FuncState *fs, struct expdesc *f, int nargs, int nrets);
AQL_API void aqlK_generic_invoke(FuncState *fs, struct expdesc *obj, struct expdesc *method, int nargs, int nrets);

/* Async/coroutine operations */
AQL_API void aqlK_async_call(FuncState *fs, struct expdesc *f, int nargs);
AQL_API void aqlK_await_expr(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_yield_expr(FuncState *fs, struct expdesc *e, int nargs);

/* Pipeline operations */
AQL_API void aqlK_pipe_expr(FuncState *fs, struct expdesc *left, struct expdesc *right);
AQL_API void aqlK_parallel_expr(FuncState *fs, struct expdesc *e, int nargs);

/* Assignment operators */
AQL_API void aqlK_compound_assign(FuncState *fs, struct expdesc *var, struct expdesc *val, OpCode op);

/* AI-specific operations (Phase 2) */
AQL_API void aqlK_ai_call(FuncState *fs, struct expdesc *e, int builtin_type);
AQL_API void aqlK_intent_expr(FuncState *fs, struct expdesc *e, TString *intent);
AQL_API void aqlK_workflow_block(FuncState *fs, int workflow_id);

/*
** Constant folding and optimization
*/
AQL_API int aqlK_constfolding(FuncState *fs, OpCode op, struct expdesc *e1, struct expdesc *e2);
AQL_API void aqlK_optimize_constants(FuncState *fs);
AQL_API int aqlK_can_fold(struct expdesc *e1, struct expdesc *e2);

/*
** Jump and control flow
*/
AQL_API int aqlK_cond_jump(FuncState *fs, OpCode op, int a, int b, int c);
AQL_API void aqlK_patch_jump(FuncState *fs, int jmp, int target);
AQL_API int aqlK_need_value(FuncState *fs, int list);
AQL_API void aqlK_drop_value(FuncState *fs, struct expdesc *e);

/*
** Expression code generation
*/
AQL_API void aqlK_prefix(FuncState *fs, UnOpr op, struct expdesc *e, int line);
AQL_API void aqlK_infix(FuncState *fs, BinOpr op, struct expdesc *v);
AQL_API void aqlK_posfix(FuncState *fs, BinOpr op, struct expdesc *e1, struct expdesc *e2, int line);

/*
** Function call optimization
*/
AQL_API int aqlK_is_tail_call(FuncState *fs, struct expdesc *e);
AQL_API void aqlK_tail_call(FuncState *fs, struct expdesc *e, int nargs);
AQL_API void aqlK_optimize_call(FuncState *fs, struct expdesc *e, int nargs, int nrets);

/*
** Register allocation
*/
typedef struct RegInfo {
  aql_byte used;     /* register is in use */
  aql_byte temp;     /* register is temporary */
  aql_byte local;    /* register holds a local variable */
  aql_byte close;    /* register needs to be closed */
} RegInfo;

AQL_API void aqlK_init_registers(FuncState *fs);
AQL_API int aqlK_alloc_reg(FuncState *fs, int hint);
AQL_API void aqlK_free_register(FuncState *fs, int reg);
AQL_API void aqlK_mark_temp(FuncState *fs, int reg);
AQL_API void aqlK_clear_temp(FuncState *fs, int reg);

/*
** Code patching and fixups
*/
typedef struct PatchInfo {
  int pc;        /* instruction address */
  int target;    /* target address */
  OpCode op;     /* original opcode */
} PatchInfo;

AQL_API void aqlK_add_patch(FuncState *fs, int pc, int target);
AQL_API void aqlK_apply_patches(FuncState *fs);
AQL_API void aqlK_resolve_labels(FuncState *fs);

/*
** Line number information
*/
AQL_API void aqlK_setlineinfo(FuncState *fs, int line);
AQL_API void aqlK_fixlineinfo(FuncState *fs);
AQL_API int aqlK_getlineinfo(const Proto *f, int pc);

/*
** Debug information
*/
AQL_API void aqlK_setlocvar(FuncState *fs, TString *name, int reg);
AQL_API void aqlK_removelocvar(FuncState *fs, int reg);
AQL_API void aqlK_setupval(FuncState *fs, TString *name, int idx);

/*
** Instruction utilities
*/
AQL_API Instruction *aqlK_getcode(FuncState *fs, struct expdesc *e);
AQL_API int aqlK_codeextraarg(FuncState *fs, int a);

/*
** Optimization analysis
*/
typedef struct OptInfo {
  int hotcount;        /* execution count for hot path detection */
  aql_byte can_inline; /* function can be inlined */
  aql_byte has_upvals; /* function has upvalues */
  aql_byte has_loops;  /* function has loops */
  aql_byte has_calls;  /* function has function calls */
} OptInfo;

AQL_API void aqlK_analyze_function(FuncState *fs, OptInfo *info);
AQL_API int aqlK_estimate_cost(FuncState *fs, int start, int end);
AQL_API void aqlK_mark_hotpath(FuncState *fs, int start, int end);

/*
** Error handling for code generation
*/
AQL_API l_noret aqlK_semerror(LexState *ls, const char *msg);
AQL_API void aqlK_warning(FuncState *fs, const char *msg);

/*
** Constants and limits
*/
#define MAXSTACK        MAXREGS
/* Use existing definitions from aopcodes.h */
#define RK_CONSTANT     (1 << (SIZE_B - 1))
#define RK_REGISTER     0

/*
** Debugging support
*/
#if defined(AQL_DEBUG_CODE)
AQL_API void aqlK_debug_code(FuncState *fs, const char *msg);
AQL_API void aqlK_dump_function(FuncState *fs);
AQL_API void aqlK_trace_instruction(FuncState *fs, int pc);
#else
#define aqlK_debug_code(fs, msg)     ((void)0)
#define aqlK_dump_function(fs)       ((void)0)
#define aqlK_trace_instruction(fs, pc) ((void)0)
#endif

#endif /* acode_h */ 