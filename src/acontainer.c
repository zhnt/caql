/*
** $Id: acontainer_new.c $
** AQL 零开销统一容器系统实现
** 匹配新版 acontainer.h 设计
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "acontainer.h"
#include "amem.h"
#include "aobject.h"
#include "adatatype.h"

/* ============================================================================
 * 容器创建函数
 * ============================================================================ */

AQL_API AQL_ContainerBase *acontainer_new(aql_State *L, ContainerType type, 
                                         DataType dtype, size_t capacity) {
    AQL_ContainerBase *c = (AQL_ContainerBase*)aqlM_malloc_tagged(L, sizeof(AQL_ContainerBase), 0);
    if (!c) return NULL;
    
    /* 初始化基本字段 */
    c->dtype = dtype;
    c->type = type;
    c->length = 0;
    c->capacity = capacity;
    c->flags = 0;
    
    /* 分配数据存储 */
    if (capacity > 0) {
        size_t elem_size = acontainer_elem_size(c);
        c->data = aqlM_malloc_tagged(L, capacity * elem_size, 0);
        if (!c->data) {
            aqlM_freemem(L, c, sizeof(AQL_ContainerBase));
            return NULL;
        }
        memset(c->data, 0, capacity * elem_size);
    } else {
        c->data = NULL;
    }
    
    /* 容器特定初始化 */
    switch (type) {
        case CONTAINER_ARRAY:
            /* 数组无需特殊初始化 */
            break;
        case CONTAINER_SLICE:
            c->u.slice.offset = 0;
            c->u.slice.source = NULL;
            break;
        case CONTAINER_VECTOR:
            c->u.vector.alignment = sizeof(void*);
            c->u.vector.simd_width = 1;
            break;
        case CONTAINER_DICT:
            c->u.dict.bucket_count = capacity;
            c->u.dict.hash_mask = capacity - 1;
            c->u.dict.load_factor = 0.75;
            break;
    }
    
    return c;
}

/* ============================================================================
 * 容器销毁函数
 * ============================================================================ */

AQL_API void acontainer_destroy(aql_State *L, AQL_ContainerBase *c) {
    if (!c) return;
    
    /* 释放数据内存 */
    if (c->data) {
        size_t elem_size = acontainer_elem_size(c);
        aqlM_freemem(L, c->data, c->capacity * elem_size);
    }
    
    /* 释放容器结构 */
    aqlM_freemem(L, c, sizeof(AQL_ContainerBase));
}

/* ============================================================================
 * 通用数组操作
 * ============================================================================ */

AQL_API int acontainer_array_get(aql_State *L, AQL_ContainerBase *c, 
                                size_t idx, TValue *result) {
    (void)L;  /* 避免未使用警告 */
    
    if (!acontainer_check_bounds(c, idx)) {
        return -1;  /* 越界 */
    }
    
    TValue *data = (TValue*)c->data;
    *result = data[idx];
    return 0;
}

AQL_API int acontainer_array_set(aql_State *L, AQL_ContainerBase *c, 
                                size_t idx, const TValue *value) {
    (void)L;  /* 避免未使用警告 */
    
    if (!acontainer_check_bounds(c, idx)) {
        return -1;  /* 越界 */
    }
    
    if (acontainer_is_readonly(c)) {
        return -2;  /* 只读 */
    }
    
    TValue *data = (TValue*)c->data;
    data[idx] = *value;
    return 0;
}

AQL_API int acontainer_array_resize(aql_State *L, AQL_ContainerBase *c, 
                                   size_t new_size) {
    if (acontainer_is_fixed(c)) {
        return -1;  /* 固定大小容器不能调整 */
    }
    
    if (new_size <= c->capacity) {
        c->length = new_size;
        return 0;
    }
    
    /* 需要扩容 */
    size_t new_capacity = acontainer_new_capacity(c->capacity, new_size);
    size_t elem_size = acontainer_elem_size(c);
    
    void *new_data = aqlM_realloc(L, c->data, 
                                  c->capacity * elem_size,
                                  new_capacity * elem_size);
    if (!new_data) {
        return -2;  /* 内存不足 */
    }
    
    /* 初始化新元素为零 */
    char *new_area = (char*)new_data + (c->capacity * elem_size);
    memset(new_area, 0, (new_capacity - c->capacity) * elem_size);
    
    c->data = new_data;
    c->capacity = new_capacity;
    c->length = new_size;
    
    return 0;
}

AQL_API int acontainer_array_append(aql_State *L, AQL_ContainerBase *c, 
                                   const TValue *value) {
    if (acontainer_is_readonly(c)) {
        return -1;  /* 只读 */
    }
    
    /* 检查是否需要扩容 */
    if (c->length >= c->capacity) {
        size_t new_capacity = acontainer_new_capacity(c->capacity, c->length + 1);
        if (acontainer_array_resize(L, c, new_capacity) != 0) {
            return -2;  /* 扩容失败 */
        }
    }
    
    /* 添加元素 */
    TValue *data = (TValue*)c->data;
    data[c->length] = *value;
    c->length++;
    
    return 0;
}

/* ============================================================================
 * 通用切片操作
 * ============================================================================ */

AQL_API int acontainer_slice_get(aql_State *L, AQL_ContainerBase *c, 
                                size_t idx, TValue *result) {
    return acontainer_array_get(L, c, idx, result);  /* 切片访问与数组相同 */
}

AQL_API int acontainer_slice_set(aql_State *L, AQL_ContainerBase *c, 
                                size_t idx, const TValue *value) {
    return acontainer_array_set(L, c, idx, value);  /* 切片设置与数组相同 */
}

AQL_API AQL_ContainerBase *acontainer_slice_view(aql_State *L, 
                                                AQL_ContainerBase *source,
                                                size_t start, size_t end) {
    if (start >= source->length || end > source->length || start >= end) {
        return NULL;  /* 无效范围 */
    }
    
    AQL_ContainerBase *slice = acontainer_new(L, CONTAINER_SLICE, source->dtype, 0);
    if (!slice) return NULL;
    
    /* 设置切片视图 */
    slice->data = (char*)source->data + (start * acontainer_elem_size(source));
    slice->length = end - start;
    slice->capacity = end - start;
    slice->u.slice.offset = start;
    slice->u.slice.source = source;
    slice->flags |= CONTAINER_FLAG_EXTERNAL;  /* 标记为外部数据 */
    
    return slice;
}

/* ============================================================================
 * 通用向量操作
 * ============================================================================ */

AQL_API int acontainer_vector_get(aql_State *L, AQL_ContainerBase *c, 
                                 size_t idx, TValue *result) {
    return acontainer_array_get(L, c, idx, result);  /* 向量访问与数组相同 */
}

AQL_API int acontainer_vector_set(aql_State *L, AQL_ContainerBase *c, 
                                 size_t idx, const TValue *value) {
    return acontainer_array_set(L, c, idx, value);  /* 向量设置与数组相同 */
}

/* ============================================================================
 * 通用字典操作 (简化实现)
 * ============================================================================ */

AQL_API int acontainer_dict_get(aql_State *L, AQL_ContainerBase *c, 
                               const TValue *key, TValue *result) {
    (void)L; (void)c; (void)key; (void)result;
    /* TODO: 实现字典查找 */
    return -1;  /* 暂未实现 */
}

AQL_API int acontainer_dict_set(aql_State *L, AQL_ContainerBase *c, 
                               const TValue *key, const TValue *value) {
    (void)L; (void)c; (void)key; (void)value;
    /* TODO: 实现字典设置 */
    return -1;  /* 暂未实现 */
}