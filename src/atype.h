/*
** $Id: atype.h $
** Type inference engine for AQL
** See Copyright Notice in aql.h
*/

#ifndef atype_h
#define atype_h

#include "aconf.h"
#include "aobject.h"

/*
** Type inference system
** Based on 95%+ accuracy target with smart fallback to dynamic typing
*/

/* Type categories for inference */
typedef enum {
    TYPENONE = 0,      /* Unknown type */
    TYPEINT,           /* Integer type */
    TYPEFLOAT,         /* Floating point type */
    TYPESTRING,        /* String type */
    TYPEBOOLEAN,       /* Boolean type */
    TYPEARRAY,         /* Array type */
    TYPESLICE,         /* Slice type */
    TYPEDICT,          /* Dictionary type */
    TYPEVECTOR,        /* Vector type */
    TYPEFUNCTION,      /* Function type */
    TYPEANY            /* Any type (fallback) */
} TypeCategory;

/* Type information structure */
typedef struct TypeInfo {
    TypeCategory category;
    int confidence;      /* 0-100 confidence level */
    int inferred;        /* 1 if inferred, 0 if explicit */
    union {
        struct {
            TypeCategory element_type;
        } array_info;
        struct {
            TypeCategory key_type;
            TypeCategory value_type;
        } dict_info;
        struct {
            TypeCategory element_type;
            int dimensions;
        } vector_info;
        struct {
            TypeCategory return_type;
            int param_count;
            TypeCategory *param_types;
        } function_info;
    } info;
} TypeInfo;

/* Variable type tracking */
typedef struct VarType {
    TString *name;       /* Variable name */
    TypeInfo type;       /* Type information */
    int scope_level;     /* Lexical scope level */
    int assignment_count;/* Number of assignments */
    struct VarType *next; /* Next variable in scope */
} VarType;

/* Scope-based type tracking */
typedef struct TypeScope {
    int level;
    VarType *variables;
    struct TypeScope *parent;
} TypeScope;

/* Expression type analysis */
typedef struct ExprType {
    TypeInfo result_type;
    int side_effects;
    int complexity;
} ExprType;

/* Type inference context */
typedef struct TypeInfer {
    TypeScope *current_scope;
    int inference_level;
    int strict_mode;
    int debug_mode;
} TypeInfer;

/*
** Type inference API
*/

/* Initialize type inference system */
void aqlT_init(TypeInfer *ti);

/* Create new type scope */
TypeScope *aqlT_new_scope(TypeInfer *ti, TypeScope *parent);

/* Destroy type scope */
void aqlT_close_scope(TypeInfer *ti, TypeScope *scope);

/* Add variable to current scope */
void aqlT_add_variable(TypeInfer *ti, const char *name, TypeInfo type);

/* Get variable type from scope chain */
TypeInfo aqlT_get_variable_type(TypeInfer *ti, const char *name);

/* Infer expression type */
ExprType aqlT_infer_expr(TypeInfer *ti, const TValue *expr);

/* Update variable type based on assignment */
void aqlT_update_variable_type(TypeInfer *ti, const char *name, TypeInfo new_type);

/* Type compatibility checking */
int aqlT_types_compatible(TypeInfo t1, TypeInfo t2);

/* Type conversion checking */
int aqlT_can_convert(TypeInfo from, TypeInfo to);

/* Generate type error/warning */
void aqlT_type_error(TypeInfer *ti, const char *msg, int line, int col);

/* Debug type information */
void aqlT_print_type_info(TypeInfo type);

/*
** Utility functions
*/

/* Convert TValue to TypeInfo */
TypeInfo aqlT_value_to_type(const TValue *val);

/* Convert TypeInfo to string representation */
const char *aqlT_type_to_string(TypeInfo type);

/* Calculate type compatibility score (0-100) */
int aqlT_type_score(TypeInfo actual, TypeInfo expected);

/*
** Smart type inference macros
*/

#define TYPE_INFER_ENABLED(ti) ((ti) != NULL && (ti)->inference_level > 0)
#define TYPE_STRICT_MODE(ti) ((ti) != NULL && (ti)->strict_mode)
#define TYPE_DEBUG_MODE(ti) ((ti) != NULL && (ti)->debug_mode)

/*
** Type inference configuration
*/
void aqlT_set_inference_level(TypeInfer *ti, int level);
void aqlT_set_strict_mode(TypeInfer *ti, int strict);
void aqlT_set_debug_mode(TypeInfer *ti, int debug);

#endif /* atype_h */