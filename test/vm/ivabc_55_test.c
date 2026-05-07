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

  ok &= assert_int("parse GETTABUP with constant key",
                   aql_parse_instruction("GETTABUP", "R2", "0", "K4",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("GETTABUP opcode", GET_OPCODE(inst), OP_GETTABUP);
  ok &= assert_int("GETTABUP A", GETARG_A(inst), 2);
  ok &= assert_int("GETTABUP B", GETARG_B(inst), 0);
  ok &= assert_int("GETTABUP C", GETARG_C(inst), 4);
  ok &= assert_int("GETTABUP k", GETARG_k(inst), 0);

  ok &= assert_int("parse GETFIELD with constant key",
                   aql_parse_instruction("GETFIELD", "R2", "R3", "K5",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("GETFIELD opcode", GET_OPCODE(inst), OP_GETFIELD);
  ok &= assert_int("GETFIELD A", GETARG_A(inst), 2);
  ok &= assert_int("GETFIELD B", GETARG_B(inst), 3);
  ok &= assert_int("GETFIELD C", GETARG_C(inst), 5);
  ok &= assert_int("GETFIELD k", GETARG_k(inst), 0);

  ok &= assert_int("parse SELF with constant key",
                   aql_parse_instruction("SELF", "R2", "R3", "K5",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("SELF opcode", GET_OPCODE(inst), OP_SELF);
  ok &= assert_int("SELF A", GETARG_A(inst), 2);
  ok &= assert_int("SELF B", GETARG_B(inst), 3);
  ok &= assert_int("SELF C", GETARG_C(inst), 5);
  ok &= assert_int("SELF k", GETARG_k(inst), 0);

  ok &= assert_int("parse SETTABUP with constant value",
                   aql_parse_instruction("SETTABUP", "R2", "K4", "K7",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("SETTABUP opcode", GET_OPCODE(inst), OP_SETTABUP);
  ok &= assert_int("SETTABUP A", GETARG_A(inst), 2);
  ok &= assert_int("SETTABUP B", GETARG_B(inst), 4);
  ok &= assert_int("SETTABUP C", GETARG_C(inst), 7);
  ok &= assert_int("SETTABUP k", GETARG_k(inst), 1);

  ok &= assert_int("parse SETTABLE with constant value",
                   aql_parse_instruction("SETTABLE", "R2", "R3", "K4",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("SETTABLE opcode", GET_OPCODE(inst), OP_SETTABLE);
  ok &= assert_int("SETTABLE A", GETARG_A(inst), 2);
  ok &= assert_int("SETTABLE B", GETARG_B(inst), 3);
  ok &= assert_int("SETTABLE C", GETARG_C(inst), 4);
  ok &= assert_int("SETTABLE k", GETARG_k(inst), 1);

  ok &= assert_int("parse SETI with constant value",
                   aql_parse_instruction("SETI", "R2", "5", "K6",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("SETI opcode", GET_OPCODE(inst), OP_SETI);
  ok &= assert_int("SETI A", GETARG_A(inst), 2);
  ok &= assert_int("SETI B", GETARG_B(inst), 5);
  ok &= assert_int("SETI C", GETARG_C(inst), 6);
  ok &= assert_int("SETI k", GETARG_k(inst), 1);

  ok &= assert_int("parse SETFIELD with constant value",
                   aql_parse_instruction("SETFIELD", "R2", "K5", "K6",
                                         NULL, 4, &inst), 1);
  ok &= assert_int("SETFIELD opcode", GET_OPCODE(inst), OP_SETFIELD);
  ok &= assert_int("SETFIELD A", GETARG_A(inst), 2);
  ok &= assert_int("SETFIELD B", GETARG_B(inst), 5);
  ok &= assert_int("SETFIELD C", GETARG_C(inst), 6);
  ok &= assert_int("SETFIELD k", GETARG_k(inst), 1);

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
