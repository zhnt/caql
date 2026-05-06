#include <stdio.h>

#include "../../src/aopcodes.h"

static int assert_int(const char *name, int actual, int expected) {
  if (actual != expected) {
    fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
    return 0;
  }
  return 1;
}

int main(void) {
  int ok = 1;
  Instruction inst;

  ok &= assert_int("parse NEWTABLE",
                   aql_parse_instruction("NEWTABLE", "R5", "63", "1023",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("NEWTABLE opcode", GET_OPCODE(inst), OP_NEWTABLE);
  ok &= assert_int("NEWTABLE A", GETARG_A(inst), 5);
  ok &= assert_int("NEWTABLE vB", GETARG_vB(inst), 63);
  ok &= assert_int("NEWTABLE vC", GETARG_vC(inst), 1023);
  ok &= assert_int("NEWTABLE k", GETARG_k(inst), 0);

  ok &= assert_int("parse SETLIST with k",
                   aql_parse_instruction("SETLIST", "R3", "1", "476",
                                         "1", 5, &inst), 1);
  ok &= assert_int("parsed SETLIST opcode", GET_OPCODE(inst), OP_SETLIST);
  ok &= assert_int("parsed SETLIST A", GETARG_A(inst), 3);
  ok &= assert_int("parsed SETLIST vB", GETARG_vB(inst), 1);
  ok &= assert_int("parsed SETLIST vC", GETARG_vC(inst), 476);
  ok &= assert_int("parsed SETLIST k", GETARG_k(inst), 1);

  inst = CREATE_vABCk(OP_SETLIST, 3, 1, 300, 1);
  ok &= assert_int("SETLIST opcode", GET_OPCODE(inst), OP_SETLIST);
  ok &= assert_int("SETLIST A", GETARG_A(inst), 3);
  ok &= assert_int("SETLIST vB", GETARG_vB(inst), 1);
  ok &= assert_int("SETLIST vC", GETARG_vC(inst), 300);
  ok &= assert_int("SETLIST k", GETARG_k(inst), 1);

  if (ok) {
    puts("ivabc_55_test passed");
    return 0;
  }
  return 1;
}
