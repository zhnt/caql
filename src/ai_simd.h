/*
** $Id: ai_simd.h $
** SIMD Optimization Layer for AQL
** See Copyright Notice in aql.h
*/

#ifndef ai_simd_h
#define ai_simd_h

#include "aconf.h"
#include "aobject.h"

#if AQL_USE_SIMD

/* Platform-specific SIMD headers */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <immintrin.h>  /* AVX, AVX2, AVX-512, SSE */
#define AQL_HAS_X86_SIMD 1
#endif

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>   /* ARM NEON */
#define AQL_HAS_ARM_NEON 1
#endif

/*
** SIMD Vector Types
*/
typedef union {
  struct { float x, y, z, w; } f32x4;
  struct { double x, y; } f64x2;
  struct { int32_t x, y, z, w; } i32x4;
  struct { int64_t x, y; } i64x2;
  
#if AQL_HAS_X86_SIMD
  __m128 sse_f32;
  __m128d sse_f64;
  __m128i sse_i32;
  __m256 avx_f32;
  __m256d avx_f64;
  __m256i avx_i32;
#if defined(__AVX512F__)
  __m512 avx512_f32;
  __m512d avx512_f64;
  __m512i avx512_i32;
#endif
#endif

#if AQL_HAS_ARM_NEON
  float32x4_t neon_f32;
  float64x2_t neon_f64;
  int32x4_t neon_i32;
  int64x2_t neon_i64;
#endif
  
  aql_Byte bytes[64];  /* For maximum SIMD width */
} SIMD_Vector;

/*
** SIMD Capability Detection
*/
typedef struct {
  aql_Byte has_sse;
  aql_Byte has_sse2;
  aql_Byte has_sse3;
  aql_Byte has_ssse3;
  aql_Byte has_sse41;
  aql_Byte has_sse42;
  aql_Byte has_avx;
  aql_Byte has_avx2;
  aql_Byte has_avx512f;
  aql_Byte has_avx512bw;
  aql_Byte has_avx512dq;
  aql_Byte has_fma;
  aql_Byte has_neon;
  aql_Byte has_sve;
  int max_vector_size;
  int preferred_alignment;
} SIMD_Caps;

AQL_API void aqlSIMD_detect_capabilities(SIMD_Caps *caps);
AQL_API int aqlSIMD_get_optimal_width(DataType dtype);
AQL_API const char *aqlSIMD_get_instruction_set_name(void);

/*
** SIMD Memory Operations
*/
AQL_API void *aqlSIMD_aligned_alloc(size_t size, size_t alignment);
AQL_API void aqlSIMD_aligned_free(void *ptr);
AQL_API void aqlSIMD_prefetch(const void *addr, int level);
AQL_API int aqlSIMD_is_aligned(const void *ptr, size_t alignment);

/*
** SIMD Arithmetic Operations
*/

/* Single precision floating point */
AQL_API void aqlSIMD_add_f32(const float *a, const float *b, float *result, size_t count);
AQL_API void aqlSIMD_sub_f32(const float *a, const float *b, float *result, size_t count);
AQL_API void aqlSIMD_mul_f32(const float *a, const float *b, float *result, size_t count);
AQL_API void aqlSIMD_div_f32(const float *a, const float *b, float *result, size_t count);
AQL_API void aqlSIMD_scale_f32(const float *a, float scalar, float *result, size_t count);

/* Double precision floating point */
AQL_API void aqlSIMD_add_f64(const double *a, const double *b, double *result, size_t count);
AQL_API void aqlSIMD_sub_f64(const double *a, const double *b, double *result, size_t count);
AQL_API void aqlSIMD_mul_f64(const double *a, const double *b, double *result, size_t count);
AQL_API void aqlSIMD_div_f64(const double *a, const double *b, double *result, size_t count);
AQL_API void aqlSIMD_scale_f64(const double *a, double scalar, double *result, size_t count);

/* Integer operations */
AQL_API void aqlSIMD_add_i32(const int32_t *a, const int32_t *b, int32_t *result, size_t count);
AQL_API void aqlSIMD_sub_i32(const int32_t *a, const int32_t *b, int32_t *result, size_t count);
AQL_API void aqlSIMD_mul_i32(const int32_t *a, const int32_t *b, int32_t *result, size_t count);

AQL_API void aqlSIMD_add_i64(const int64_t *a, const int64_t *b, int64_t *result, size_t count);
AQL_API void aqlSIMD_sub_i64(const int64_t *a, const int64_t *b, int64_t *result, size_t count);
AQL_API void aqlSIMD_mul_i64(const int64_t *a, const int64_t *b, int64_t *result, size_t count);

/*
** SIMD Reduction Operations
*/
AQL_API float aqlSIMD_sum_f32(const float *data, size_t count);
AQL_API double aqlSIMD_sum_f64(const double *data, size_t count);
AQL_API int32_t aqlSIMD_sum_i32(const int32_t *data, size_t count);
AQL_API int64_t aqlSIMD_sum_i64(const int64_t *data, size_t count);

AQL_API float aqlSIMD_min_f32(const float *data, size_t count);
AQL_API float aqlSIMD_max_f32(const float *data, size_t count);
AQL_API double aqlSIMD_min_f64(const double *data, size_t count);
AQL_API double aqlSIMD_max_f64(const double *data, size_t count);

AQL_API int32_t aqlSIMD_min_i32(const int32_t *data, size_t count);
AQL_API int32_t aqlSIMD_max_i32(const int32_t *data, size_t count);
AQL_API int64_t aqlSIMD_min_i64(const int64_t *data, size_t count);
AQL_API int64_t aqlSIMD_max_i64(const int64_t *data, size_t count);

/*
** SIMD Mathematical Functions
*/
AQL_API void aqlSIMD_sqrt_f32(const float *input, float *output, size_t count);
AQL_API void aqlSIMD_sqrt_f64(const double *input, double *output, size_t count);
AQL_API void aqlSIMD_rsqrt_f32(const float *input, float *output, size_t count);

AQL_API void aqlSIMD_exp_f32(const float *input, float *output, size_t count);
AQL_API void aqlSIMD_exp_f64(const double *input, double *output, size_t count);
AQL_API void aqlSIMD_log_f32(const float *input, float *output, size_t count);
AQL_API void aqlSIMD_log_f64(const double *input, double *output, size_t count);

AQL_API void aqlSIMD_sin_f32(const float *input, float *output, size_t count);
AQL_API void aqlSIMD_cos_f32(const float *input, float *output, size_t count);
AQL_API void aqlSIMD_tan_f32(const float *input, float *output, size_t count);

AQL_API void aqlSIMD_abs_f32(const float *input, float *output, size_t count);
AQL_API void aqlSIMD_abs_f64(const double *input, double *output, size_t count);

/*
** SIMD Comparison Operations
*/
AQL_API void aqlSIMD_compare_eq_f32(const float *a, const float *b, int *result, size_t count);
AQL_API void aqlSIMD_compare_lt_f32(const float *a, const float *b, int *result, size_t count);
AQL_API void aqlSIMD_compare_le_f32(const float *a, const float *b, int *result, size_t count);
AQL_API void aqlSIMD_compare_gt_f32(const float *a, const float *b, int *result, size_t count);
AQL_API void aqlSIMD_compare_ge_f32(const float *a, const float *b, int *result, size_t count);

/*
** SIMD Bitwise Operations
*/
AQL_API void aqlSIMD_and_i32(const int32_t *a, const int32_t *b, int32_t *result, size_t count);
AQL_API void aqlSIMD_or_i32(const int32_t *a, const int32_t *b, int32_t *result, size_t count);
AQL_API void aqlSIMD_xor_i32(const int32_t *a, const int32_t *b, int32_t *result, size_t count);
AQL_API void aqlSIMD_not_i32(const int32_t *a, int32_t *result, size_t count);

/*
** SIMD Window/Rolling Operations (for time series)
*/
AQL_API void aqlSIMD_rolling_sum_f32(const float *input, float *output, size_t count, size_t window);
AQL_API void aqlSIMD_rolling_mean_f32(const float *input, float *output, size_t count, size_t window);
AQL_API void aqlSIMD_rolling_min_f32(const float *input, float *output, size_t count, size_t window);
AQL_API void aqlSIMD_rolling_max_f32(const float *input, float *output, size_t count, size_t window);

/*
** SIMD Memory Copy and Set Operations
*/
AQL_API void aqlSIMD_memcpy(void *dest, const void *src, size_t size);
AQL_API void aqlSIMD_memset_f32(float *dest, float value, size_t count);
AQL_API void aqlSIMD_memset_f64(double *dest, double value, size_t count);
AQL_API void aqlSIMD_memset_i32(int32_t *dest, int32_t value, size_t count);

/*
** SIMD Type Conversion
*/
AQL_API void aqlSIMD_convert_f32_to_f64(const float *input, double *output, size_t count);
AQL_API void aqlSIMD_convert_f64_to_f32(const double *input, float *output, size_t count);
AQL_API void aqlSIMD_convert_i32_to_f32(const int32_t *input, float *output, size_t count);
AQL_API void aqlSIMD_convert_f32_to_i32(const float *input, int32_t *output, size_t count);

/*
** SIMD Gather/Scatter Operations
*/
AQL_API void aqlSIMD_gather_f32(const float *base, const int32_t *indices, float *output, size_t count);
AQL_API void aqlSIMD_scatter_f32(const float *input, float *base, const int32_t *indices, size_t count);

/*
** SIMD Optimized String Operations
*/
AQL_API int aqlSIMD_strlen(const char *str);
AQL_API int aqlSIMD_strcmp(const char *s1, const char *s2);
AQL_API char *aqlSIMD_strchr(const char *str, int c);
AQL_API void *aqlSIMD_memchr(const void *ptr, int value, size_t num);

/*
** SIMD Performance Measurement
*/
typedef struct {
  uint64_t cycles_start;
  uint64_t cycles_end;
  double throughput_gbps;
  double operations_per_second;
} SIMD_PerfInfo;

AQL_API void aqlSIMD_perf_start(SIMD_PerfInfo *info);
AQL_API void aqlSIMD_perf_end(SIMD_PerfInfo *info, size_t data_processed);
AQL_API void aqlSIMD_benchmark_operation(const char *name, void (*func)(void), size_t iterations);

/*
** SIMD Compiler Intrinsics Wrappers
*/
#if AQL_HAS_X86_SIMD

/* SSE Wrappers */
AQL_API __m128 aqlSIMD_load_ps(const float *p);
AQL_API void aqlSIMD_store_ps(float *p, __m128 a);
AQL_API __m128 aqlSIMD_add_ps(__m128 a, __m128 b);
AQL_API __m128 aqlSIMD_mul_ps(__m128 a, __m128 b);

/* AVX Wrappers */
AQL_API __m256 aqlSIMD_load_pd256(const double *p);
AQL_API void aqlSIMD_store_pd256(double *p, __m256d a);
AQL_API __m256d aqlSIMD_add_pd256(__m256d a, __m256d b);
AQL_API __m256d aqlSIMD_mul_pd256(__m256d a, __m256d b);

#endif /* AQL_HAS_X86_SIMD */

#if AQL_HAS_ARM_NEON

/* NEON Wrappers */
AQL_API float32x4_t aqlSIMD_load_f32x4(const float *p);
AQL_API void aqlSIMD_store_f32x4(float *p, float32x4_t a);
AQL_API float32x4_t aqlSIMD_add_f32x4(float32x4_t a, float32x4_t b);
AQL_API float32x4_t aqlSIMD_mul_f32x4(float32x4_t a, float32x4_t b);

#endif /* AQL_HAS_ARM_NEON */

/*
** SIMD Auto-Vectorization Hints
*/
typedef enum {
  SIMD_HINT_NONE = 0,
  SIMD_HINT_UNROLL = 1,
  SIMD_HINT_VECTORIZE = 2,
  SIMD_HINT_PREFER_ALIGNED = 4,
  SIMD_HINT_ASSUME_ALIGNED = 8,
  SIMD_HINT_NO_ALIAS = 16
} SIMD_Hints;

AQL_API void aqlSIMD_set_hints(SIMD_Hints hints);
AQL_API SIMD_Hints aqlSIMD_get_hints(void);

/*
** SIMD Debugging and Analysis
*/
#if defined(AQL_DEBUG_SIMD)
AQL_API void aqlSIMD_debug_vector_f32(const char *name, const float *data, size_t count);
AQL_API void aqlSIMD_debug_vector_f64(const char *name, const double *data, size_t count);
AQL_API void aqlSIMD_trace_operation(const char *op_name, size_t data_size, double elapsed_ms);
AQL_API void aqlSIMD_dump_capabilities(void);
#else
#define aqlSIMD_debug_vector_f32(name, data, count) ((void)0)
#define aqlSIMD_debug_vector_f64(name, data, count) ((void)0)
#define aqlSIMD_trace_operation(op, size, time) ((void)0)
#define aqlSIMD_dump_capabilities() ((void)0)
#endif

/*
** SIMD Constants
*/
#define SIMD_ALIGN_BYTES        64
#define SIMD_CACHELINE_SIZE     64
#define SIMD_MIN_VECTOR_SIZE    4
#define SIMD_PREFETCH_DISTANCE  64

/* SIMD operation thresholds */
#define SIMD_THRESHOLD_F32      16    /* Minimum elements for F32 SIMD */
#define SIMD_THRESHOLD_F64      8     /* Minimum elements for F64 SIMD */
#define SIMD_THRESHOLD_I32      16    /* Minimum elements for I32 SIMD */
#define SIMD_THRESHOLD_I64      8     /* Minimum elements for I64 SIMD */

/*
** Fallback implementations for non-SIMD platforms
*/
#else /* !AQL_USE_SIMD */

typedef struct { int dummy; } SIMD_Vector;
typedef struct { int dummy; } SIMD_Caps;
typedef struct { int dummy; } SIMD_PerfInfo;
typedef int SIMD_Hints;

/* Provide scalar fallbacks for all SIMD functions */
#define aqlSIMD_detect_capabilities(caps) ((void)0)
#define aqlSIMD_get_optimal_width(dtype) 1
#define aqlSIMD_get_instruction_set_name() "scalar"

/* All SIMD functions become regular scalar loops */
AQL_API void aqlSIMD_add_f32(const float *a, const float *b, float *result, size_t count);
AQL_API void aqlSIMD_sum_f32(const float *data, size_t count);
/* ... other scalar implementations ... */

#endif /* AQL_USE_SIMD */

#endif /* ai_simd_h */ 