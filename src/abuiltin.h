/*
** Builtin function identifiers shared by parser, VM, and bootstrap code.
** These are temporary VM builtin values; Lua-compatible libraries should
** eventually expose ordinary functions in _ENV.
*/

#ifndef abuiltin_h
#define abuiltin_h

typedef enum {
  AQL_BUILTIN_PRINT = 0,
  AQL_BUILTIN_TYPE = 1,
  AQL_BUILTIN_LEN = 2,
  AQL_BUILTIN_TOSTRING = 3,
  AQL_BUILTIN_TONUMBER = 4,
  AQL_BUILTIN_RANGE = 5,
  AQL_BUILTIN_SELECT = 6,
  AQL_BUILTIN_STRING_LEN = 7,
  AQL_BUILTIN_PRINT2 = 99
} aql_BuiltinId;

#endif
