/*
** $Id: atype.c $
** Type inference engine implementation for AQL
** See Copyright Notice in aql.h
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "atype.h"

/*
** Initialize type inference system
*/
void aqlT_init(TypeInfer *ti) {
    if (ti == NULL) return;
    
    ti->current_scope = NULL;
    ti->inference_level = 1;    /* Default: basic inference */
    ti->strict_mode = 0;        /* Default: permissive mode */
    ti->debug_mode = 0;         /* Default: debug off */
}

/*
** Create new type scope
*/
TypeScope *aqlT_new_scope(TypeInfer *ti, TypeScope *parent) {
    TypeScope *scope = (TypeScope *)malloc(sizeof(TypeScope));
    if (scope == NULL) return NULL;
    
    scope->level = parent ? parent->level + 1 : 0;
    scope->variables = NULL;
    scope->parent = parent;
    
    if (ti != NULL) {
        ti->current_scope = scope;
    }
    
    return scope;
}

/*
** Destroy type scope and free variables
*/
void aqlT_close_scope(TypeInfer *ti, TypeScope *scope) {
    if (scope == NULL) return;
    
    /* Free all variables in this scope */
    VarType *current = scope->variables;
    while (current != NULL) {
        VarType *next = current->next;
        free(current);
        current = next;
    }
    
    if (ti != NULL && ti->current_scope == scope) {
        ti->current_scope = scope->parent;
    }
    
    free(scope);
}

/*
** Add variable to current scope
*/
void aqlT_add_variable(TypeInfer *ti, const char *name, TypeInfo type) {
    if (ti == NULL || ti->current_scope == NULL || name == NULL) return;
    
    VarType *var = (VarType *)malloc(sizeof(VarType));
    if (var == NULL) return;
    
    /* Note: In real implementation, we would use proper string allocation */
    var->name = (TString *)name; /* Simplified for now */
    var->type = type;
    var->scope_level = ti->current_scope->level;
    var->assignment_count = 0;
    var->next = ti->current_scope->variables;
    
    ti->current_scope->variables = var;
}

/*
** Get variable type from scope chain
*/
TypeInfo aqlT_get_variable_type(TypeInfer *ti, const char *name) {
    TypeInfo unknown = {TYPENONE, 0, 0, {0}};
    
    if (ti == NULL || name == NULL) return unknown;
    
    TypeScope *scope = ti->current_scope;
    while (scope != NULL) {
        VarType *var = scope->variables;
        while (var != NULL) {
            /* Simplified string comparison */
            if (var->name != NULL && strcmp((const char *)var->name, name) == 0) {
                return var->type;
            }
            var = var->next;
        }
        scope = scope->parent;
    }
    
    return unknown;
}

/*
** Convert TValue to TypeInfo
*/
TypeInfo aqlT_value_to_type(const TValue *val) {
    TypeInfo type = {TYPENONE, 100, 1, {0}};
    
    if (val == NULL) {
        type.category = TYPEANY;
        type.confidence = 50;
        return type;
    }
    
    switch (ttype(val)) {
        case AQL_VNUMINT:
            type.category = TYPEINT;
            break;
        case AQL_VNUMFLT:
            type.category = TYPEFLOAT;
            break;
        case AQL_VSHRSTR:
        case AQL_VLNGSTR:
            type.category = TYPESTRING;
            break;
        case AQL_VTRUE:
        case AQL_VFALSE:
            type.category = TYPEBOOLEAN;
            break;
        case AQL_TARRAY:
            type.category = TYPEARRAY;
            type.info.array_info.element_type = TYPEANY;
            break;
        case AQL_TSLICE:
            type.category = TYPESLICE;
            /* slice_info not defined - use array_info for now */
            break;
        case AQL_TDICT:
            type.category = TYPEDICT;
            type.info.dict_info.key_type = TYPESTRING;
            type.info.dict_info.value_type = TYPEANY;
            break;
        case AQL_TVECTOR:
            type.category = TYPEVECTOR;
            type.info.vector_info.element_type = TYPEANY;
            type.info.vector_info.dimensions = 1;
            break;
        case AQL_TFUNCTION:
            type.category = TYPEFUNCTION;
            break;
        default:
            type.category = TYPEANY;
            type.confidence = 30;
            break;
    }
    
    return type;
}

/*
** Infer expression type
*/
ExprType aqlT_infer_expr(TypeInfer *ti, const TValue *expr) {
    ExprType result;
    result.result_type = aqlT_value_to_type(expr);
    result.side_effects = 0;
    result.complexity = 1;
    
    return result;
}

/*
** Update variable type based on assignment
*/
void aqlT_update_variable_type(TypeInfer *ti, const char *name, TypeInfo new_type) {
    if (ti == NULL || name == NULL) return;
    
    TypeScope *scope = ti->current_scope;
    while (scope != NULL) {
        VarType *var = scope->variables;
        while (var != NULL) {
            if (var->name != NULL && strcmp((const char *)var->name, name) == 0) {
                /* Update type with increased confidence */
                var->assignment_count++;
                if (var->assignment_count > 1) {
                    var->type.confidence = 95; /* High confidence after multiple assignments */
                }
                return;
            }
            var = var->next;
        }
        scope = scope->parent;
    }
}

/*
** Type compatibility checking
*/
int aqlT_types_compatible(TypeInfo t1, TypeInfo t2) {
    if (t1.category == t2.category) return 1;
    
    /* Allow numeric type compatibility */
    if ((t1.category == TYPEINT && t2.category == TYPEFLOAT) ||
        (t1.category == TYPEFLOAT && t2.category == TYPEINT)) {
        return 1;
    }
    
    /* Allow any type compatibility */
    if (t1.category == TYPEANY || t2.category == TYPEANY) {
        return 1;
    }
    
    return 0;
}

/*
** Type conversion checking
*/
int aqlT_can_convert(TypeInfo from, TypeInfo to) {
    return aqlT_types_compatible(from, to); /* Simplified for now */
}

/*
** Convert TypeInfo to string representation
*/
const char *aqlT_type_to_string(TypeInfo type) {
    switch (type.category) {
        case TYPENONE: return "none";
        case TYPEINT: return "int";
        case TYPEFLOAT: return "float";
        case TYPESTRING: return "string";
        case TYPEBOOLEAN: return "boolean";
        case TYPEARRAY: return "array";
        case TYPESLICE: return "slice";
        case TYPEDICT: return "dict";
        case TYPEVECTOR: return "vector";
        case TYPEFUNCTION: return "function";
        case TYPEANY: return "any";
        default: return "unknown";
    }
}

/*
** Generate type error/warning
*/
void aqlT_type_error(TypeInfer *ti, const char *msg, int line, int col) {
    if (ti == NULL || msg == NULL) return;
    
    fprintf(stderr, "Type error at line %d, col %d: %s\n", line, col, msg);
}

/*
** Debug type information
*/
void aqlT_print_type_info(TypeInfo type) {
    printf("Type: %s, Confidence: %d%%", 
           aqlT_type_to_string(type), type.confidence);
    if (type.inferred) printf(" (inferred)");
    else printf(" (explicit)");
    printf("\n");
}

/*
** Calculate type compatibility score (0-100)
*/
int aqlT_type_score(TypeInfo actual, TypeInfo expected) {
    if (actual.category == expected.category) return 100;
    
    if (aqlT_types_compatible(actual, expected)) {
        return 75; /* Compatible but not exact match */
    }
    
    return 0; /* Incompatible */
}

/*
** Configuration functions
*/
void aqlT_set_inference_level(TypeInfer *ti, int level) {
    if (ti != NULL) ti->inference_level = level;
}

void aqlT_set_strict_mode(TypeInfer *ti, int strict) {
    if (ti != NULL) ti->strict_mode = strict;
}

void aqlT_set_debug_mode(TypeInfer *ti, int debug) {
    if (ti != NULL) ti->debug_mode = debug;
}