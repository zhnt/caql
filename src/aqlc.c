/*
** AQL compiler CLI (aqlc)
** Emits the current text chunk format consumed by aqlvm and can list chunks.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aql.h"
#include "aapi.h"
#include "adebug.h"
#include "adebug_user.h"
#include "aobject.h"
#include "aopcodes.h"
#include "astate.h"

static void *aqlc_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static void print_usage(const char *progname) {
  fprintf(stderr,
          "Usage: %s [options] <file.aql>\n"
          "Options:\n"
          "  -o <file.by>   write aqlvm text chunk\n"
          "  -l             list/disassemble compiled chunk\n"
          "  -h, --help     show this help\n",
          progname);
}

static int compile_file(aql_State *L, const char *filename, LClosure **out) {
  TValue *slot;
  if (aql_loadfile(L, filename) != 0)
    return 0;
  if (L->top.p <= L->stack.p)
    return 0;
  slot = s2v(L->top.p - 1);
  if (!ttisLclosure(slot))
    return 0;
  *out = clLvalue(slot);
  return (*out != NULL && (*out)->p != NULL);
}

static int write_quoted(FILE *out, const char *s, size_t len) {
  fputc('"', out);
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    switch (c) {
      case '\\': fputs("\\\\", out); break;
      case '"': fputs("\\\"", out); break;
      case '\n': fputs("\\n", out); break;
      case '\r': fputs("\\r", out); break;
      case '\t': fputs("\\t", out); break;
      default:
        if (c < 0x20)
          return 0;
        fputc((int)c, out);
        break;
    }
  }
  fputc('"', out);
  return 1;
}

static void write_rk(FILE *out, int index, int k) {
  fprintf(out, "%c%d", k ? 'K' : 'R', index);
}

static void write_instruction(FILE *out, Instruction i) {
  OpCode op = GET_OPCODE(i);
  int a = GETARG_A(i);
  int b = GETARG_B(i);
  int c = GETARG_C(i);
  int k = GETARG_k(i);

  switch (op) {
    case OP_RETURN0:
      fprintf(out, "RETURN0");
      break;
    case OP_RETURN1:
      fprintf(out, "RETURN1 R%d", a);
      break;
    case OP_JMP:
      fprintf(out, "JMP %d", GETARG_sJ(i));
      break;
    case OP_LOADI:
    case OP_LOADF:
      fprintf(out, "%s R%d %d", aql_opnames[op], a, GETARG_sBx(i));
      break;
    case OP_LOADK:
      fprintf(out, "LOADK R%d K%d", a, GETARG_Bx(i));
      break;
    case OP_LOADKX:
      fprintf(out, "LOADKX R%d", a);
      break;
    case OP_EXTRAARG:
      fprintf(out, "EXTRAARG %d", GETARG_Ax(i));
      break;
    case OP_GETTABUP:
      fprintf(out, "GETTABUP R%d %d K%d", a, b, c);
      break;
    case OP_SETTABUP:
      fprintf(out, "SETTABUP %d K%d ", a, b);
      write_rk(out, c, k);
      break;
    case OP_GETFIELD:
      fprintf(out, "GETFIELD R%d R%d K%d", a, b, c);
      break;
    case OP_SETFIELD:
      fprintf(out, "SETFIELD R%d K%d ", a, b);
      write_rk(out, c, k);
      break;
    case OP_GETI:
      fprintf(out, "GETI R%d R%d %d", a, b, c);
      break;
    case OP_SETI:
      fprintf(out, "SETI R%d %d ", a, b);
      write_rk(out, c, k);
      break;
    case OP_GETTABLE:
      fprintf(out, "GETTABLE R%d R%d R%d", a, b, c);
      break;
    case OP_SETTABLE:
      fprintf(out, "SETTABLE R%d R%d ", a, b);
      write_rk(out, c, k);
      break;
    case OP_SELF:
      fprintf(out, "SELF R%d R%d K%d", a, b, c);
      break;
    case OP_ADDI:
    case OP_SUBI:
    case OP_MULI:
    case OP_DIVI:
    case OP_SHLI:
    case OP_SHRI:
      fprintf(out, "%s R%d R%d %d", aql_opnames[op], a, b, GETARG_sC(i));
      break;
    case OP_MMBIN:
      fprintf(out, "MMBIN R%d R%d %d", a, b, c);
      break;
    case OP_MMBINI:
      fprintf(out, "MMBINI R%d %d %d %d", a, GETARG_sB(i), c, k);
      break;
    case OP_MMBINK:
      fprintf(out, "MMBINK R%d %d %d %d", a, b, c, k);
      break;
    case OP_EQ:
    case OP_LT:
    case OP_LE:
      fprintf(out, "%s R%d R%d %d", aql_opnames[op], a, b, k);
      break;
    case OP_EQK:
      fprintf(out, "EQK R%d K%d %d", a, b, k);
      break;
    case OP_EQI:
    case OP_LTI:
    case OP_LEI:
    case OP_GTI:
    case OP_GEI:
      fprintf(out, "%s R%d %d %d", aql_opnames[op], a, GETARG_sB(i), k);
      break;
    case OP_TEST:
      fprintf(out, "TEST R%d %d", a, k);
      break;
    case OP_TESTSET:
      fprintf(out, "TESTSET R%d R%d %d", a, b, k);
      break;
    case OP_CALL:
      fprintf(out, "CALL R%d %d %d", a, b, c);
      break;
    case OP_TAILCALL:
      fprintf(out, "TAILCALL R%d %d %d %d", a, b, c, k);
      break;
    case OP_RETURN:
      fprintf(out, "RETURN R%d %d %d %d", a, b, c, k);
      break;
    case OP_FORLOOP:
    case OP_FORPREP:
    case OP_TFORPREP:
    case OP_TFORLOOP:
    case OP_CLOSURE:
    case OP_ERRNNIL:
      fprintf(out, "%s R%d %d", aql_opnames[op], a, GETARG_Bx(i));
      break;
    case OP_TFORCALL:
      fprintf(out, "TFORCALL R%d 0 %d", a, c);
      break;
    case OP_NEWTABLE:
    case OP_SETLIST:
      fprintf(out, "%s R%d %d %d %d", aql_opnames[op], a,
              GETARG_vB(i), GETARG_vC(i), k);
      break;
    case OP_VARARG:
      fprintf(out, "VARARG R%d %d %d %d", a, b, c, k);
      break;
    case OP_GETVARG:
      fprintf(out, "GETVARG R%d R%d R%d", a, b, c);
      break;
    case OP_VARARGPREP:
      fprintf(out, "VARARGPREP %d", a);
      break;
    default: {
      enum OpMode mode = getOpMode(op);
      switch (mode) {
        case iABC:
          fprintf(out, "%s R%d %d %d", aql_opnames[op], a, b, c);
          break;
        case ivABC:
          fprintf(out, "%s R%d %d %d %d", aql_opnames[op], a,
                  GETARG_vB(i), GETARG_vC(i), k);
          break;
        case iABx:
          fprintf(out, "%s R%d %d", aql_opnames[op], a, GETARG_Bx(i));
          break;
        case iAsBx:
          fprintf(out, "%s R%d %d", aql_opnames[op], a, GETARG_sBx(i));
          break;
        case iAx:
          fprintf(out, "%s %d", aql_opnames[op], GETARG_Ax(i));
          break;
        case isJ:
          fprintf(out, "%s %d", aql_opnames[op], GETARG_sJ(i));
          break;
      }
      break;
    }
  }
  fputc('\n', out);
}

static int write_constants(FILE *out, Proto *p) {
  for (int i = 0; i < p->sizek; i++) {
    TValue *v = &p->k[i];
    switch (ttypetag(v)) {
      case AQL_VNUMINT:
        fprintf(out, "K%d INTEGER %lld\n", i, (long long)ivalue(v));
        break;
      case AQL_VNUMFLT:
        fprintf(out, "K%d FLOAT %.17g\n", i, (double)fltvalue(v));
        break;
      case AQL_VSHRSTR:
      case AQL_VLNGSTR:
        fprintf(out, "K%d STRING ", i);
        if (!write_quoted(out, getstr(tsvalue(v)), tsslen(tsvalue(v))))
          return 0;
        fputc('\n', out);
        break;
      default:
        fprintf(stderr, "aqlc: unsupported constant type %d at K%d\n",
                ttypetag(v), i);
        return 0;
    }
  }
  return 1;
}

static int write_upvalues(FILE *out, Proto *p) {
  if (p->sizeupvalues <= 0)
    return 1;
  fprintf(out, ".upvalues %d\n", p->sizeupvalues);
  for (int i = 0; i < p->sizeupvalues; i++) {
    Upvaldesc *uv = &p->upvalues[i];
    fprintf(out, ".upvalue %d %s %d kind %d\n", i,
            uv->instack ? "instack" : "upval", uv->idx, uv->kind);
    if (uv->name != NULL) {
      fprintf(out, ".upvalue_name %d ", i);
      if (!write_quoted(out, getstr(uv->name), tsslen(uv->name)))
        return 0;
      fputc('\n', out);
    }
  }
  return 1;
}

static int write_proto(FILE *out, Proto *p, const char *name, int is_main) {
  if (is_main)
    fprintf(out, ".main %d%s\n", p->numparams, p->is_vararg ? " vararg" : "");
  else
    fprintf(out, ".function %s %d%s\n", name, p->numparams,
            p->is_vararg ? " vararg" : "");
  fprintf(out, ".stack %d\n", p->maxstacksize);
  if (!write_constants(out, p) || !write_upvalues(out, p))
    return 0;
  fprintf(out, ".code\n");
  for (int pc = 0; pc < p->sizecode; pc++)
    write_instruction(out, p->code[pc]);
  fprintf(out, ".end\n\n");
  return 1;
}

static int proto_child_count(Proto *p) {
  int max_child = -1;
  for (int pc = 0; pc < p->sizecode; pc++) {
    Instruction i = p->code[pc];
    if (GET_OPCODE(i) == OP_CLOSURE && GETARG_Bx(i) > max_child)
      max_child = GETARG_Bx(i);
  }
  return max_child + 1;
}

static int write_proto_tree(FILE *out, Proto *p, const char *name, int is_main) {
  char child_name[64];
  int child_count;
  if (!write_proto(out, p, name, is_main))
    return 0;
  child_count = proto_child_count(p);
  if (child_count > 0 && p->p == NULL) {
    fprintf(stderr, "aqlc: missing child prototype array in %s\n", name);
    return 0;
  }
  for (int i = 0; i < child_count; i++) {
    if (p->p[i] == NULL) {
      fprintf(stderr, "aqlc: missing child prototype %d in %s\n", i, name);
      return 0;
    }
    snprintf(child_name, sizeof(child_name), "%s_child%d", name, i);
    if (!write_proto_tree(out, p->p[i], child_name, 0))
      return 0;
  }
  return 1;
}

static void list_proto(FILE *out, Proto *p, const char *name) {
  int child_count;
  fprintf(out, "Function: %s\n", name);
  fprintf(out, "  instructions: %d, constants: %d, upvalues: %d, stack: %d\n",
          p->sizecode, p->sizek, p->sizeupvalues, p->maxstacksize);
  for (int pc = 0; pc < p->sizecode; pc++) {
    fprintf(out, "  %04d  ", pc);
    write_instruction(out, p->code[pc]);
  }
  child_count = proto_child_count(p);
  for (int i = 0; i < child_count; i++) {
    char child_name[128];
    if (p->p == NULL || p->p[i] == NULL)
      continue;
    snprintf(child_name, sizeof(child_name), "%s::child[%d]", name, i);
    list_proto(out, p->p[i], child_name);
  }
}

int main(int argc, char **argv) {
  const char *input = NULL;
  const char *output = NULL;
  int list = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    else if (strcmp(argv[i], "-l") == 0) {
      list = 1;
    }
    else if (strcmp(argv[i], "-o") == 0) {
      if (++i >= argc) {
        fprintf(stderr, "aqlc: -o requires a path\n");
        return 1;
      }
      output = argv[i];
    }
    else if (argv[i][0] == '-') {
      fprintf(stderr, "aqlc: unknown option '%s'\n", argv[i]);
      return 1;
    }
    else if (input == NULL) {
      input = argv[i];
    }
    else {
      fprintf(stderr, "aqlc: multiple input files are not supported yet\n");
      return 1;
    }
  }

  if (input == NULL) {
    print_usage(argv[0]);
    return 1;
  }

  aql_State *L = aql_newstate(aqlc_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "aqlc: failed to create AQL state\n");
    return 1;
  }

  aqlD_init_debug();
  aqlD_set_debug_flags(AQL_DBG_NONE);

  LClosure *cl = NULL;
  if (!compile_file(L, input, &cl)) {
    fprintf(stderr, "aqlc: failed to compile '%s'\n", input);
    aql_close(L);
    return 1;
  }

  if (list) {
    list_proto(stdout, cl->p, input);
  }

  if (output != NULL || !list) {
    FILE *out = stdout;
    int ok;
    if (output != NULL) {
      out = fopen(output, "w");
      if (out == NULL) {
        fprintf(stderr, "aqlc: cannot open output '%s'\n", output);
        aql_close(L);
        return 1;
      }
    }
    fprintf(out, "# AQL text chunk generated by aqlc\n");
    ok = write_proto_tree(out, cl->p, "main", 1);
    if (output != NULL)
      fclose(out);
    if (!ok) {
      aql_close(L);
      return 1;
    }
  }

  aql_close(L);
  return 0;
}
