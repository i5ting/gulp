#ifdef __cplusplus
extern "C" {
#endif

#include "moonbit.h"

#ifdef _MSC_VER
#define _Noreturn __declspec(noreturn)
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wshift-op-parentheses"
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif

MOONBIT_EXPORT _Noreturn void moonbit_panic(void);
MOONBIT_EXPORT void *moonbit_malloc_array(enum moonbit_block_kind kind,
                                          int elem_size_shift, int32_t len);
MOONBIT_EXPORT int moonbit_val_array_equal(const void *lhs, const void *rhs);
MOONBIT_EXPORT moonbit_string_t moonbit_add_string(moonbit_string_t s1,
                                                   moonbit_string_t s2);
MOONBIT_EXPORT void moonbit_unsafe_bytes_blit(moonbit_bytes_t dst,
                                              int32_t dst_start,
                                              moonbit_bytes_t src,
                                              int32_t src_offset, int32_t len);
MOONBIT_EXPORT moonbit_string_t moonbit_unsafe_bytes_sub_string(
    moonbit_bytes_t bytes, int32_t start, int32_t len);
MOONBIT_EXPORT void moonbit_println(moonbit_string_t str);
MOONBIT_EXPORT moonbit_bytes_t *moonbit_get_cli_args(void);
MOONBIT_EXPORT void moonbit_runtime_init(int argc, char **argv);
MOONBIT_EXPORT void moonbit_drop_object(void *);

#define Moonbit_make_regular_object_header(ptr_field_offset, ptr_field_count,  \
                                           tag)                                \
  (((uint32_t)moonbit_BLOCK_KIND_REGULAR << 30) |                              \
   (((uint32_t)(ptr_field_offset) & (((uint32_t)1 << 11) - 1)) << 19) |        \
   (((uint32_t)(ptr_field_count) & (((uint32_t)1 << 11) - 1)) << 8) |          \
   ((tag) & 0xFF))

// header manipulation macros
#define Moonbit_object_ptr_field_offset(obj)                                   \
  ((Moonbit_object_header(obj)->meta >> 19) & (((uint32_t)1 << 11) - 1))

#define Moonbit_object_ptr_field_count(obj)                                    \
  ((Moonbit_object_header(obj)->meta >> 8) & (((uint32_t)1 << 11) - 1))

#if !defined(_WIN64) && !defined(_WIN32)
void *malloc(size_t size);
void free(void *ptr);
#define libc_malloc malloc
#define libc_free free
#endif

// several important runtime functions are inlined
static void *moonbit_malloc_inlined(size_t size) {
  struct moonbit_object *ptr = (struct moonbit_object *)libc_malloc(
      sizeof(struct moonbit_object) + size);
  ptr->rc = 1;
  return ptr + 1;
}

#define moonbit_malloc(obj) moonbit_malloc_inlined(obj)
#define moonbit_free(obj) libc_free(Moonbit_object_header(obj))

static void moonbit_incref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const count = header->rc;
  if (count > 0) {
    header->rc = count + 1;
  }
}

#define moonbit_incref moonbit_incref_inlined

static void moonbit_decref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const count = header->rc;
  if (count > 1) {
    header->rc = count - 1;
  } else if (count == 1) {
    moonbit_drop_object(ptr);
  }
}

#define moonbit_decref moonbit_decref_inlined

#define moonbit_unsafe_make_string moonbit_make_string

// detect whether compiler builtins exist for advanced bitwise operations
#ifdef __has_builtin

#if __has_builtin(__builtin_clz)
#define HAS_BUILTIN_CLZ
#endif

#if __has_builtin(__builtin_ctz)
#define HAS_BUILTIN_CTZ
#endif

#if __has_builtin(__builtin_popcount)
#define HAS_BUILTIN_POPCNT
#endif

#if __has_builtin(__builtin_sqrt)
#define HAS_BUILTIN_SQRT
#endif

#if __has_builtin(__builtin_sqrtf)
#define HAS_BUILTIN_SQRTF
#endif

#if __has_builtin(__builtin_fabs)
#define HAS_BUILTIN_FABS
#endif

#if __has_builtin(__builtin_fabsf)
#define HAS_BUILTIN_FABSF
#endif

#endif

// if there is no builtin operators, use software implementation
#ifdef HAS_BUILTIN_CLZ
static inline int32_t moonbit_clz32(int32_t x) {
  return x == 0 ? 32 : __builtin_clz(x);
}

static inline int32_t moonbit_clz64(int64_t x) {
  return x == 0 ? 64 : __builtin_clzll(x);
}

#undef HAS_BUILTIN_CLZ
#else
// table for [clz] value of 4bit integer.
static const uint8_t moonbit_clz4[] = {4, 3, 2, 2, 1, 1, 1, 1,
                                       0, 0, 0, 0, 0, 0, 0, 0};

int32_t moonbit_clz32(uint32_t x) {
  /* The ideas is to:

     1. narrow down the 4bit block where the most signficant "1" bit lies,
        using binary search
     2. find the number of leading zeros in that 4bit block via table lookup

     Different time/space tradeoff can be made here by enlarging the table
     and do less binary search.
     One benefit of the 4bit lookup table is that it can fit into a single cache
     line.
  */
  int32_t result = 0;
  if (x > 0xffff) {
    x >>= 16;
  } else {
    result += 16;
  }
  if (x > 0xff) {
    x >>= 8;
  } else {
    result += 8;
  }
  if (x > 0xf) {
    x >>= 4;
  } else {
    result += 4;
  }
  return result + moonbit_clz4[x];
}

int32_t moonbit_clz64(uint64_t x) {
  int32_t result = 0;
  if (x > 0xffffffff) {
    x >>= 32;
  } else {
    result += 32;
  }
  return result + moonbit_clz32((uint32_t)x);
}
#endif

#ifdef HAS_BUILTIN_CTZ
static inline int32_t moonbit_ctz32(int32_t x) {
  return x == 0 ? 32 : __builtin_ctz(x);
}

static inline int32_t moonbit_ctz64(int64_t x) {
  return x == 0 ? 64 : __builtin_ctzll(x);
}

#undef HAS_BUILTIN_CTZ
#else
int32_t moonbit_ctz32(int32_t x) {
  /* The algorithm comes from:

       Leiserson, Charles E. et al. “Using de Bruijn Sequences to Index a 1 in a
     Computer Word.” (1998).

     The ideas is:

     1. leave only the least significant "1" bit in the input,
        set all other bits to "0". This is achieved via [x & -x]
     2. now we have [x * n == n << ctz(x)], if [n] is a de bruijn sequence
        (every 5bit pattern occurn exactly once when you cycle through the bit
     string), we can find [ctz(x)] from the most significant 5 bits of [x * n]
 */
  static const uint32_t de_bruijn_32 = 0x077CB531;
  static const uint8_t index32[] = {0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
                                    15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
                                    16, 7,  26, 12, 18, 6,  11, 5,  10, 9};
  return (x == 0) * 32 + index32[(de_bruijn_32 * (x & -x)) >> 27];
}

int32_t moonbit_ctz64(int64_t x) {
  static const uint64_t de_bruijn_64 = 0x0218A392CD3D5DBF;
  static const uint8_t index64[] = {
      0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  34, 20, 40,
      5,  17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
      63, 6,  12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56,
      62, 11, 23, 32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58};
  return (x == 0) * 64 + index64[(de_bruijn_64 * (x & -x)) >> 58];
}
#endif

#ifdef HAS_BUILTIN_POPCNT

#define moonbit_popcnt32 __builtin_popcount
#define moonbit_popcnt64 __builtin_popcountll
#undef HAS_BUILTIN_POPCNT

#else
int32_t moonbit_popcnt32(uint32_t x) {
  /* The classic SIMD Within A Register algorithm.
     ref: [https://nimrod.blog/posts/algorithms-behind-popcount/]
 */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F;
  return (x * 0x01010101) >> 24;
}

int32_t moonbit_popcnt64(uint64_t x) {
  x = x - ((x >> 1) & 0x5555555555555555);
  x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
  return (x * 0x0101010101010101) >> 56;
}
#endif

/* The following sqrt implementation comes from
   [musl](https://git.musl-libc.org/cgit/musl),
   with some helpers inlined to make it zero dependency.
 */
#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
const uint16_t __rsqrt_tab[128] = {
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43, 0xaa14,
    0xa8eb, 0xa7c8, 0xa6aa, 0xa592, 0xa480, 0xa373, 0xa26b, 0xa168, 0xa06a,
    0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1, 0x99f0, 0x9913, 0x983a,
    0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430, 0x936b, 0x92a9, 0x91ea, 0x912e,
    0x9075, 0x8fbe, 0x8f0a, 0x8e59, 0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07,
    0x8a64, 0x89c4, 0x8925, 0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599,
    0x8508, 0x8479, 0x83ec, 0x8361, 0x82d8, 0x8250, 0x81c9, 0x8145, 0x80c2,
    0x8040, 0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2, 0xe443,
    0xe2dc, 0xe17a, 0xe020, 0xdecb, 0xdd7d, 0xdc34, 0xdaf1, 0xd9b3, 0xd87b,
    0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192, 0xd07b, 0xcf69, 0xce5b,
    0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f, 0xc858, 0xc764, 0xc674, 0xc587,
    0xc49d, 0xc3b7, 0xc2d4, 0xc1f4, 0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe,
    0xbcef, 0xbc23, 0xbb59, 0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0,
    0xb617, 0xb560,
};

/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) {
  return (uint64_t)a * b >> 32;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float sqrtf(float x) {
  uint32_t ix, m, m1, m0, even, ey;

  ix = *(uint32_t *)&x;
  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000) {
    /* x < 0x1p-126 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7f800000)
      return x;
    if (ix > 0x7f800000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p23f;
    ix = *(uint32_t *)&x;
    ix -= 23 << 23;
  }

  /* x = 4^e m; with int e and m in [1, 4).  */
  even = ix & 0x00800000;
  m1 = (ix << 8) | 0x80000000;
  m0 = (ix << 7) & 0x7fffffff;
  m = even ? m0 : m1;

  /* 2^e is the exponent part of the return value.  */
  ey = ix >> 1;
  ey += 0x3f800000 >> 1;
  ey &= 0x7f800000;

  /* compute r ~ 1/sqrt(m), s ~ sqrt(m) with 2 goldschmidt iterations.  */
  static const uint32_t three = 0xc0000000;
  uint32_t r, s, d, u, i;
  i = (ix >> 17) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r*sqrt(m) - 1| < 0x1p-8 */
  s = mul32(m, r);
  /* |s/sqrt(m) - 1| < 0x1p-8 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r*sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  s = mul32(s, u);
  /* -0x1.03p-28 < s/sqrt(m) - 1 < 0x1.fp-31 */
  s = (s - 1) >> 6;
  /* s < sqrt(m) < s + 0x1.08p-23 */

  /* compute nearest rounded result.  */
  uint32_t d0, d1, d2;
  float y, t;
  d0 = (m << 16) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 31;
  s &= 0x007fffff;
  s |= ey;
  y = *(float *)&s;
  /* handle rounding and inexact exception. */
  uint32_t tiny = d2 == 0 ? 0 : 0x01000000;
  tiny |= (d1 ^ d2) & 0x80000000;
  t = *(float *)&tiny;
  y = y + t;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
  uint64_t ahi = a >> 32;
  uint64_t alo = a & 0xffffffff;
  uint64_t bhi = b >> 32;
  uint64_t blo = b & 0xffffffff;
  return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}

double sqrt(double x) {
  uint64_t ix, top, m;

  /* special case handling.  */
  ix = *(uint64_t *)&x;
  top = ix >> 52;
  if (top - 0x001 >= 0x7ff - 0x001) {
    /* x < 0x1p-1022 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7ff0000000000000)
      return x;
    if (ix > 0x7ff0000000000000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p52;
    ix = *(uint64_t *)&x;
    top = ix >> 52;
    top -= 52;
  }

  /* argument reduction:
     x = 4^e m; with integer e, and m in [1, 4)
     m: fixed point representation [2.62]
     2^e is the exponent part of the result.  */
  int even = top & 1;
  m = (ix << 11) | 0x8000000000000000;
  if (even)
    m >>= 1;
  top = (top + 0x3ff) >> 1;

  /* approximate r ~ 1/sqrt(m) and s ~ sqrt(m) when m in [1,4)

     initial estimate:
     7bit table lookup (1bit exponent and 6bit significand).

     iterative approximation:
     using 2 goldschmidt iterations with 32bit int arithmetics
     and a final iteration with 64bit int arithmetics.

     details:

     the relative error (e = r0 sqrt(m)-1) of a linear estimate
     (r0 = a m + b) is |e| < 0.085955 ~ 0x1.6p-4 at best,
     a table lookup is faster and needs one less iteration
     6 bit lookup table (128b) gives |e| < 0x1.f9p-8
     7 bit lookup table (256b) gives |e| < 0x1.fdp-9
     for single and double prec 6bit is enough but for quad
     prec 7bit is needed (or modified iterations). to avoid
     one more iteration >=13bit table would be needed (16k).

     a newton-raphson iteration for r is
       w = r*r
       u = 3 - m*w
       r = r*u/2
     can use a goldschmidt iteration for s at the end or
       s = m*r

     first goldschmidt iteration is
       s = m*r
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     next goldschmidt iteration is
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     and at the end r is not computed only s.

     they use the same amount of operations and converge at the
     same quadratic rate, i.e. if
       r1 sqrt(m) - 1 = e, then
       r2 sqrt(m) - 1 = -3/2 e^2 - 1/2 e^3
     the advantage of goldschmidt is that the mul for s and r
     are independent (computed in parallel), however it is not
     "self synchronizing": it only uses the input m in the
     first iteration so rounding errors accumulate. at the end
     or when switching to larger precision arithmetics rounding
     errors dominate so the first iteration should be used.

     the fixed point representations are
       m: 2.30 r: 0.32, s: 2.30, d: 2.30, u: 2.30, three: 2.30
     and after switching to 64 bit
       m: 2.62 r: 0.64, s: 2.62, d: 2.62, u: 2.62, three: 2.62  */

  static const uint64_t three = 0xc0000000;
  uint64_t r, s, d, u, i;

  i = (ix >> 46) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r sqrt(m) - 1| < 0x1.fdp-9 */
  s = mul32(m >> 32, r);
  /* |s/sqrt(m) - 1| < 0x1.fdp-9 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
  r = r << 32;
  s = mul64(m, r);
  d = mul64(s, r);
  u = (three << 32) - d;
  s = mul64(s, u); /* repr: 3.61 */
  /* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
  s = (s - 2) >> 9; /* repr: 12.52 */
  /* -0x1.09p-52 < s - sqrt(m) < -0x1.fffcp-63 */

  /* s < sqrt(m) < s + 0x1.09p-52,
     compute nearest rounded result:
     the nearest result to 52 bits is either s or s+0x1p-52,
     we can decide by comparing (2^52 s + 0.5)^2 to 2^104 m.  */
  uint64_t d0, d1, d2;
  double y, t;
  d0 = (m << 42) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 63;
  s &= 0x000fffffffffffff;
  s |= top << 52;
  y = *(double *)&s;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
double fabs(double x) {
  union {
    double f;
    uint64_t i;
  } u = {x};
  u.i &= 0x7fffffffffffffffULL;
  return u.f;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float fabsf(float x) {
  union {
    float f;
    uint32_t i;
  } u = {x};
  u.i &= 0x7fffffff;
  return u.f;
}
#endif

#ifdef _MSC_VER
/* MSVC treats syntactic division by zero as fatal error,
   even for float point numbers,
   so we have to use a constant variable to work around this */
static const int MOONBIT_ZERO = 0;
#else
#define MOONBIT_ZERO 0
#endif

#ifdef __cplusplus
}
#endif
struct _M0Y3Int;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTP36mulpjs4mulp4core9MulpError9GlobError;

struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__;

struct _M0DTPC16option6OptionGOWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorEE4Some;

struct _M0TP36mulpjs4mulp8through216FileThroughState;

struct _M0DTP36mulpjs4mulp4core9MulpError11ConfigError;

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0TPB6Logger;

struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE;

struct _M0TURPB6LoggerRPC16string10StringViewE;

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__;

struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__;

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TPB8MutLocalGOiE;

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOs;

struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__;

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE;

struct _M0TP36mulpjs4mulp6stream4File;

struct _M0TPB4Show;

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__;

struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE;

struct _M0KTPB4ShowS3Int;

struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145;

struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__;

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed;

struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TP36mulpjs4mulp6stream14ArrayFileState;

struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE;

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0TPB9ArrayViewGsE;

struct _M0TWEu;

struct _M0DTP36mulpjs4mulp4core9MulpError14ParallelFailed;

struct _M0DTP36mulpjs4mulp4core9MulpError10WatchError;

struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__;

struct _M0TP36mulpjs4mulp4core7Context;

struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE;

struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__;

struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0KTPB4ShowS4Bool;

struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink;

struct _M0Y4Bool;

struct _M0DTP36mulpjs4mulp4core9MulpError12TaskNotFound;

struct _M0TPB13StringBuilder;

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream;

struct _M0TUssE;

struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0BTPB6Logger;

struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__;

struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE;

struct _M0DTPC15error5Error118mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32stream__pipeline__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWEuQRPC15error5Error;

struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__;

struct _M0TP36mulpjs4mulp6stream9PipeState;

struct _M0KTPB4ShowTPB5ArrayGsE;

struct _M0TPB8MutLocalGiE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TP36mulpjs4mulp6stream10ByteStream;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__;

struct _M0TPB5ArrayGUssEE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32stream__pipeline__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__;

struct _M0TP36mulpjs4mulp4core17CancellationToken;

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__;

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0DTP36mulpjs4mulp6stream12FileContents6Buffer;

struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__;

struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE;

struct _M0BTPB4Show;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB8MutLocalGbE;

struct _M0KTPB4ShowS6String;

struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0TP36mulpjs4mulp6stream10FileStream;

struct _M0DTP36mulpjs4mulp6stream12FileContents4Text;

struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__;

struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030;

struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__;

struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__;

struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream;

struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0Y3Int {
  int32_t $0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TPB8MutLocalGbE* $0;
  
};

struct _M0TWssbEu {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  
};

struct _M0TUsiE {
  int32_t $1;
  moonbit_string_t $0;
  
};

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $5;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError9GlobError {
  moonbit_string_t $0;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError {
  moonbit_string_t $0;
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__ {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream10FileStream*
  );
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* $0;
  
};

struct _M0DTPC16option6OptionGOWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorEE4Some {
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $0;
  
};

struct _M0TP36mulpjs4mulp8through216FileThroughState {
  int32_t $4;
  int32_t $5;
  int32_t $6;
  struct _M0TP36mulpjs4mulp6stream10FileStream* $0;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $1;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $2;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* $3;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError11ConfigError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  
};

struct _M0TURPB6LoggerRPC16string10StringViewE {
  int32_t $1_1;
  int32_t $1_2;
  struct _M0BTPB6Logger* $0_0;
  void* $0_1;
  moonbit_string_t $1_0;
  
};

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp6stream9PipeState* $0;
  
};

struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__ {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $0;
  
};

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream10FileStream*
  );
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0TPB8MutLocalGOiE {
  int64_t $0;
  
};

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TP36mulpjs4mulp6stream9PipeState* $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__ {
  void*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* $0;
  
};

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE {
  int32_t $1;
  struct _M0TP36mulpjs4mulp6stream4File** $0;
  
};

struct _M0TP36mulpjs4mulp6stream4File {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  void* $3;
  struct _M0TPB5ArrayGUssEE* $4;
  moonbit_string_t $5;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TPB5ArrayGsE* $0;
  
};

struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE {
  void*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  
};

struct _M0KTPB4ShowS3Int {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File {
  struct _M0TP36mulpjs4mulp6stream4File*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok {
  struct _M0TP36mulpjs4mulp6stream4File* $0;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__ {
  void*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP36mulpjs4mulp6stream14ArrayFileState {
  int32_t $1;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* $0;
  
};

struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE {
  int32_t $1;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream** $0;
  
};

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError14ParallelFailed {
  struct _M0TPB5ArrayGUssEE* $0;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError10WatchError {
  moonbit_string_t $0;
  
};

struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TP36mulpjs4mulp4core7Context {
  int64_t $1;
  moonbit_string_t $0;
  struct _M0TP36mulpjs4mulp4core17CancellationToken* $2;
  
};

struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE {
  void*(* code)(
    struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
  );
  
};

struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__ {
  void*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* $0;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__ {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* $0;
  
};

struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes {
  struct _M0TP36mulpjs4mulp6stream10ByteStream* $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS4Bool {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink {
  moonbit_string_t $0;
  
};

struct _M0Y4Bool {
  int32_t $0;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError12TaskNotFound {
  moonbit_string_t $0;
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  uint16_t* $0;
  
};

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  
};

struct _M0TUssE {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGUsiEE {
  int32_t $1;
  struct _M0TUsiE** $0;
  
};

struct _M0TWRPC15error5ErrorEs {
  moonbit_string_t(* code)(struct _M0TWRPC15error5ErrorEs*, void*);
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok {
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* $0;
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  
};

struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* $0;
  
};

struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE {
  struct _M0TP36mulpjs4mulp6stream10FileStream* $0;
  
};

struct _M0DTPC15error5Error118mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32stream__pipeline__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp8through216FileThroughState* $0;
  
};

struct _M0TP36mulpjs4mulp6stream9PipeState {
  struct _M0TP36mulpjs4mulp6stream10FileStream* $0;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* $1;
  struct _M0TP36mulpjs4mulp6stream10FileStream* $2;
  
};

struct _M0KTPB4ShowTPB5ArrayGsE {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB8MutLocalGiE {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TPB5ArrayGsE* $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0TPB13SourceLocRepr {
  int32_t $0_1;
  int32_t $0_2;
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2_1;
  int32_t $2_2;
  int32_t $3_1;
  int32_t $3_2;
  int32_t $4_1;
  int32_t $4_2;
  moonbit_string_t $0_0;
  moonbit_string_t $1_0;
  moonbit_string_t $2_0;
  moonbit_string_t $3_0;
  moonbit_string_t $4_0;
  
};

struct _M0TP36mulpjs4mulp6stream10ByteStream {
  int32_t $2;
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* $0;
  struct _M0TWEu* $1;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPB8MutLocalGiE* $1;
  
};

struct _M0TPB5ArrayGUssEE {
  int32_t $1;
  struct _M0TUssE** $0;
  
};

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32stream__pipeline__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok {
  moonbit_bytes_t $0;
  
};

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__ {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  struct _M0TPB5ArrayGsE* $0;
  
};

struct _M0TP36mulpjs4mulp4core17CancellationToken {
  int32_t $0;
  
};

struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp6stream4File* $0;
  
};

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTP36mulpjs4mulp6stream12FileContents6Buffer {
  moonbit_bytes_t $0;
  
};

struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__ {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream10FileStream*
  );
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $0;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $1;
  
};

struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE {
  void*(* code)(
    struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*
  );
  
};

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0TPB8MutLocalGbE {
  int32_t $0;
  
};

struct _M0KTPB4ShowS6String {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE {
  int32_t $1;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream** $0;
  
};

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TP36mulpjs4mulp6stream10FileStream {
  int32_t $2;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* $0;
  struct _M0TWEu* $1;
  
};

struct _M0DTP36mulpjs4mulp6stream12FileContents4Text {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  void* $0;
  
};

struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030 {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream10FileStream*
  );
  struct _M0TPB5ArrayGsE* $0;
  
};

struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TP36mulpjs4mulp8through216FileThroughState* $0;
  
};

struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__ {
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*
  );
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* $0;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* $1;
  
};

struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream {
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*
  );
  
};

struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE {
  void*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0TUWEuQRPC15error5ErrorNsE {
  struct _M0TWEuQRPC15error5Error* $0;
  moonbit_string_t* $1;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

void* _M0FP36mulpjs4mulp8through228empty__file__flush_2edyncall(
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1154(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1145(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testC2603l433(
  struct _M0TWEu*
);

int32_t _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testC2599l434(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1078(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1073(
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1060(
  int32_t,
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp32stream__pipeline__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__6(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5(
  
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5C2435l155(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4(
  
);

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2399l112(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2396l129(
  struct _M0TWEu*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4N4holdS1030(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2360l134(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2368l136(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2365l137(
  struct _M0TWEu*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3(
  
);

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2353l86(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2347l89(
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2(
  
);

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2312l58(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2N9duplicateS1019(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2264l65(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1(
  
);

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1C2255l37(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0(
  
);

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0C2224l18(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TP36mulpjs4mulp4core7Context* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context(
  
);

struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through225file__transformer_2einner(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through225file__transformer_2einnerC2175l142(
  struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*
);

void* _M0FP36mulpjs4mulp8through225file__transformer_2einnerC2187l159(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

void* _M0FP36mulpjs4mulp8through225file__transformer_2einnerC2179l146(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through214through__files(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

void* _M0FP36mulpjs4mulp8through214through__filesC2168l126(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through220through__files__flat(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through220through__files__flatC2164l116(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flush(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2145l64(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

void* _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2156l67(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2150l106(
  struct _M0TWEu*
);

struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0FP36mulpjs4mulp8through211file__state(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream4File* _M0MP36mulpjs4mulp8through216FileThroughState13next__pending(
  struct _M0TP36mulpjs4mulp8through216FileThroughState*
);

void* _M0FP36mulpjs4mulp8through218empty__file__flush();

void* _M0FP36mulpjs4mulp16stream__pipeline4lead(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp4core7Context*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline7compose(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE*
);

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline11to__through(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline11to__throughC2124l15(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline8pipeline(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE*
);

moonbit_string_t _M0MP36mulpjs4mulp6stream4File8basename(
  struct _M0TP36mulpjs4mulp6stream4File*
);

int64_t _M0FP36mulpjs4mulp6stream15last__index__of(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0MP36mulpjs4mulp6stream10FileStream7collect(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp4core7Context*
);

struct _M0TP36mulpjs4mulp6stream4File* _M0MP36mulpjs4mulp6stream4File10with__path(
  struct _M0TP36mulpjs4mulp6stream4File*,
  moonbit_string_t
);

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream10map__files(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream10map__filesC2090l270(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0MP36mulpjs4mulp6stream10FileStream12pipe__stream(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0MP36mulpjs4mulp6stream10FileStream4pipe(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*
);

void* _M0MP36mulpjs4mulp6stream10FileStream4pipeC2079l193(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0MP36mulpjs4mulp6stream10FileStream4pipeC2075l215(struct _M0TWEu*);

void* _M0MP36mulpjs4mulp6stream10FileStream4next(
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

int32_t _M0MP36mulpjs4mulp6stream10FileStream5close(
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream13error__stream(
  void*
);

void* _M0FP36mulpjs4mulp6stream13error__streamC2067l68(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp6stream4file(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t,
  void*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream12file__stream(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*
);

void* _M0FP36mulpjs4mulp6stream12file__streamC2051l129(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream24file__stream__from__pull(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2048l55(
  struct _M0TWEu*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TWEu*
);

struct _M0TP36mulpjs4mulp4core7Context* _M0FP36mulpjs4mulp4core12new__context(
  moonbit_string_t,
  int64_t,
  struct _M0TP36mulpjs4mulp4core17CancellationToken*
);

struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0FP36mulpjs4mulp4core24new__cancellation__token(
  
);

moonbit_string_t _M0MP36mulpjs4mulp4core9MulpError7message(void*);

void* _M0FP36mulpjs4mulp4core13stream__error(moonbit_string_t);

int32_t _M0MP36mulpjs4mulp4core7Context13is__cancelled(
  struct _M0TP36mulpjs4mulp4core7Context*
);

int32_t _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(
  struct _M0TP36mulpjs4mulp4core17CancellationToken*
);

void* _M0FP36mulpjs4mulp4core12task__failed(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*,
  int32_t
);

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE,
  struct _M0TPC16string10StringView
);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Hash13hash__combine(int32_t, struct _M0TPB6Hasher*);

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t,
  struct _M0TPB6Hasher*
);

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher*,
  moonbit_string_t
);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TUWEuQRPC15error5ErrorNsE*
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TUWEuQRPC15error5ErrorNsE*,
  int32_t
);

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC16option6Option6unwrapGRP36mulpjs4mulp6stream4FileE(
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

void* _M0MPC16result6Result11unwrap__errGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(
  void*
);

void* _M0MPC16result6Result11unwrap__errGuRP36mulpjs4mulp4core9MulpErrorE(
  void*
);

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0MPC16result6Result6unwrapGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(
  void*
);

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC16result6Result6unwrapGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE(
  void*
);

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(struct _M0TPB5ArrayGsE*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1818l680(struct _M0TWEOs*);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t);

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array4pushGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*
);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*
);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

struct _M0TP36mulpjs4mulp6stream4File** _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(int32_t);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t,
  int32_t,
  int32_t,
  int64_t
);

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t,
  int32_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPC15array9ArrayView6lengthGsE(struct _M0TPB9ArrayViewGsE);

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t,
  int64_t,
  int64_t
);

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0IPC16string10StringViewPB2Eq5equal(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0IPC14byte4BytePB7Default7default();

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

#define _M0FPB19unsafe__sub__string moonbit_unsafe_bytes_sub_string

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t,
  int32_t,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPC14uint4UInt8to__byte(uint32_t);

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs*);

moonbit_string_t _M0MPC13int3Int18to__string_2einner(int32_t, int32_t);

int32_t _M0FPB14radix__count32(uint32_t, int32_t);

int32_t _M0FPB12hex__count32(uint32_t);

int32_t _M0FPB12dec__count32(uint32_t);

int32_t _M0FPB20int__to__string__dec(uint16_t*, uint32_t, int32_t, int32_t);

int32_t _M0FPB24int__to__string__generic(
  uint16_t*,
  uint32_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB20int__to__string__hex(uint16_t*, uint32_t, int32_t, int32_t);

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger,
  struct _M0TWEOs*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs*);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE*
);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void*
);

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t);

int32_t _M0IP016_24default__implPB4Hash4hashGsE(moonbit_string_t);

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t);

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t);

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher*);

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher*);

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

int32_t _M0MPB6Hasher12combine__int(struct _M0TPB6Hasher*, int32_t);

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show,
  moonbit_string_t,
  moonbit_string_t,
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

moonbit_string_t _M0MPB9SourceLoc16to__json__string(moonbit_string_t);

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr*
);

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t
);

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t*,
  int32_t,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(moonbit_string_t);

moonbit_string_t _M0MPC16string6String14escape_2einner(
  moonbit_string_t,
  int32_t
);

int32_t _M0MPC16string10StringView18escape__to_2einner(
  struct _M0TPC16string10StringView,
  struct _M0TPB6Logger,
  int32_t
);

int32_t _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(
  struct _M0TURPB6LoggerRPC16string10StringViewE*,
  int32_t,
  int32_t
);

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView,
  int32_t
);

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView,
  int32_t,
  int64_t
);

int32_t _M0MPC16string10StringView6length(struct _M0TPC16string10StringView);

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t);

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t);

int32_t _M0IPC14byte4BytePB3Sub3sub(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Mod3mod(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Div3div(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Add3add(int32_t, int32_t);

moonbit_string_t _M0FPB33base64__encode__string__codepoint(moonbit_string_t);

int32_t _M0MPC16string6String16unsafe__char__at(moonbit_string_t, int32_t);

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t);

int32_t _M0FPB32code__point__of__surrogate__pair(int32_t, int32_t);

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t);

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t);

uint32_t _M0MPC14char4Char8to__uint(int32_t);

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder*
);

int32_t _M0IPC16uint166UInt16PB7Default7default();

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(int32_t);

int32_t _M0MPC14byte4Byte8to__char(int32_t);

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE**,
  int32_t,
  struct _M0TUsiE**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp6stream4FileE(
  struct _M0TP36mulpjs4mulp6stream4File**,
  int32_t,
  struct _M0TP36mulpjs4mulp6stream4File**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t*,
  int32_t,
  uint16_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE**,
  int32_t,
  struct _M0TUsiE**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP36mulpjs4mulp6stream4FileEE(
  struct _M0TP36mulpjs4mulp6stream4File**,
  int32_t,
  struct _M0TP36mulpjs4mulp6stream4File**,
  int32_t,
  int32_t
);

int32_t _M0MPB6Hasher13combine__uint(struct _M0TPB6Hasher*, uint32_t);

int32_t _M0MPB6Hasher8consume4(struct _M0TPB6Hasher*, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0IPB7FailurePB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0FPC15abort5abortGRPB5ArrayGRP36mulpjs4mulp6stream4FileEE(
  moonbit_string_t
);

struct _M0TP36mulpjs4mulp6stream4File* _M0FPC15abort5abortGORP36mulpjs4mulp6stream4FileE(
  moonbit_string_t
);

void* _M0FPC15abort5abortGRP36mulpjs4mulp4core9MulpErrorE(moonbit_string_t);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IPC14bool4BoolPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  moonbit_string_t
);

moonbit_string_t _M0IPC13int3IntPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGRPB5ArrayGsEE(
  void*
);

int32_t _M0IPC15array5ArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGsE(
  void*,
  struct _M0TPB6Logger
);

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    116, 114, 97, 110, 115, 102, 111, 114, 109, 32, 99, 108, 111, 115, 
    101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 52, 50, 58, 51, 45, 49, 52, 50, 58, 
    55, 51, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    104, 101, 108, 108, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    98, 111, 111, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 55, 52, 58, 49, 49, 45, 49, 55, 52, 
    58, 50, 55, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    115, 111, 117, 114, 99, 101, 32, 99, 108, 111, 115, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_134 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 112, 114, 111, 112, 97, 103, 97, 116, 101, 115, 32, 116, 
    114, 97, 110, 115, 102, 111, 114, 109, 32, 101, 114, 114, 111, 114, 
    115, 32, 100, 111, 119, 110, 115, 116, 114, 101, 97, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[45]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 44), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 99, 111, 109, 112, 111, 115, 101, 115, 32, 116, 114, 97, 
    110, 115, 102, 111, 114, 109, 115, 32, 105, 110, 32, 111, 114, 100, 
    101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 26), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 97, 112, 112, 46, 111, 117, 116, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    114, 101, 110, 97, 109, 101, 100, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    112, 105, 112, 101, 108, 105, 110, 101, 32, 102, 97, 105, 108, 101, 
    100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_101 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_65 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    123, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    87, 97, 116, 99, 104, 32, 101, 114, 114, 111, 114, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 48, 52, 58, 52, 49, 45, 49, 48, 52, 
    58, 53, 48, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_124 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    46, 99, 111, 112, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 54, 50, 58, 51, 45, 49, 54, 53, 58, 
    52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 55, 52, 58, 51, 45, 49, 55, 52, 58, 
    52, 52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 54, 49, 58, 49, 49, 45, 49, 54, 49, 
    58, 50, 55, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_79 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 47, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    99, 97, 110, 99, 101, 108, 108, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    70, 105, 108, 101, 32, 115, 121, 115, 116, 101, 109, 32, 101, 114, 
    114, 111, 114, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 55, 52, 58, 51, 55, 45, 49, 55, 52, 
    58, 52, 51, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    83, 116, 114, 101, 97, 109, 32, 101, 114, 114, 111, 114, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    114, 101, 110, 97, 109, 101, 100, 46, 116, 120, 116, 46, 99, 111, 
    112, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 52, 58, 49, 49, 45, 55, 52, 58, 51, 
    49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 52, 58, 51, 54, 45, 50, 52, 58, 51, 
    57, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 51, 58, 51, 45, 55, 51, 58, 53, 53, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[110]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 109), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 115, 116, 
    114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 101, 95, 
    98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 
    84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 53, 58, 51, 45, 52, 53, 58, 52, 48, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 52, 52, 58, 49, 49, 45, 49, 52, 52, 
    58, 49, 55, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 55, 53, 58, 51, 45, 49, 55, 53, 58, 
    55, 49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    97, 112, 112, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 52, 52, 58, 51, 45, 49, 52, 52, 58, 
    55, 49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 53, 58, 49, 49, 45, 50, 53, 58, 51, 
    49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    101, 110, 100, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 48, 51, 58, 49, 49, 45, 49, 48, 51, 
    58, 50, 54, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 52, 58, 51, 45, 50, 52, 58, 52, 48, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    80, 97, 114, 97, 108, 108, 101, 108, 32, 102, 97, 105, 108, 101, 
    100, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 54, 58, 52, 49, 45, 52, 54, 58, 53, 
    52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 48, 51, 58, 51, 54, 45, 49, 48, 51, 
    58, 51, 57, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 53, 58, 49, 49, 45, 52, 53, 58, 50, 
    54, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_136 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 116, 46, 
    109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    47, 116, 109, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 55, 53, 58, 49, 49, 45, 49, 55, 53, 
    58, 52, 48, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 99, 108, 111, 115, 101, 32, 99, 108, 111, 115, 101, 115, 
    32, 115, 111, 117, 114, 99, 101, 32, 97, 110, 100, 32, 97, 99, 116, 
    105, 118, 101, 32, 116, 114, 97, 110, 115, 102, 111, 114, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 26), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 114, 101, 110, 97, 109, 101, 100, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    59, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 99, 111, 109, 112, 111, 115, 101, 115, 32, 116, 104, 114, 
    111, 117, 103, 104, 50, 32, 102, 105, 108, 101, 32, 116, 114, 97, 
    110, 115, 102, 111, 114, 109, 101, 114, 32, 119, 105, 116, 104, 32, 
    102, 108, 117, 115, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 52, 58, 51, 45, 55, 52, 58, 54, 48, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_42 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[44]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 43), 
    99, 97, 108, 108, 101, 100, 32, 96, 82, 101, 115, 117, 108, 116, 
    58, 58, 117, 110, 119, 114, 97, 112, 40, 41, 96, 32, 111, 110, 32, 
    97, 110, 32, 96, 69, 114, 114, 96, 32, 118, 97, 108, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 54, 58, 51, 45, 52, 54, 58, 53, 53, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 53, 58, 51, 54, 45, 52, 53, 58, 51, 
    57, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 97, 99, 99, 101, 112, 116, 115, 32, 116, 104, 114, 111, 
    117, 103, 104, 50, 32, 102, 105, 108, 101, 32, 116, 114, 97, 110, 
    115, 102, 111, 114, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 51, 58, 49, 49, 45, 55, 51, 58, 51, 
    49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 48, 51, 58, 51, 45, 49, 48, 51, 58, 
    52, 48, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 109, 117, 
    108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 115, 116, 114, 101, 
    97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 101, 34, 44, 32, 
    34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 53, 58, 52, 49, 45, 50, 53, 58, 53, 
    52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 48, 52, 58, 51, 45, 49, 48, 52, 58, 
    53, 49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 54, 49, 58, 51, 55, 45, 49, 54, 49, 
    58, 52, 51, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 50, 58, 51, 54, 45, 55, 50, 58, 51, 
    57, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[38]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 37), 
    91, 34, 116, 114, 97, 110, 115, 102, 111, 114, 109, 32, 99, 108, 
    111, 115, 101, 100, 34, 44, 32, 34, 115, 111, 117, 114, 99, 101, 
    32, 99, 108, 111, 115, 101, 100, 34, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 54, 49, 58, 51, 45, 49, 54, 49, 58, 
    52, 52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 52, 50, 58, 54, 51, 45, 49, 52, 50, 
    58, 55, 50, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 50, 58, 51, 45, 55, 50, 58, 52, 48, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 54, 51, 58, 53, 45, 49, 54, 51, 58, 
    51, 52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 48, 52, 58, 49, 49, 45, 49, 48, 52, 
    58, 51, 49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    97, 112, 112, 46, 111, 117, 116, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    101, 110, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    83, 116, 114, 101, 97, 109, 32, 101, 114, 114, 111, 114, 58, 32, 
    112, 105, 112, 101, 108, 105, 110, 101, 32, 102, 97, 105, 108, 101, 
    100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    44, 32, 34, 109, 101, 115, 115, 97, 103, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 54, 52, 58, 49, 51, 45, 49, 54, 52, 
    58, 52, 52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[108]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 107), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 115, 116, 
    114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 101, 95, 
    98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 
    114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_137 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    112, 105, 112, 101, 108, 105, 110, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    84, 97, 115, 107, 32, 110, 111, 116, 32, 102, 111, 117, 110, 100, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 52, 50, 58, 49, 49, 45, 49, 52, 50, 
    58, 53, 51, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 97, 112, 112, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 99, 111, 109, 112, 111, 115, 101, 115, 32, 115, 116, 114, 
    101, 97, 109, 32, 116, 114, 97, 110, 115, 102, 111, 114, 109, 115, 
    32, 97, 110, 100, 32, 116, 111, 45, 116, 104, 114, 111, 117, 103, 
    104, 32, 97, 100, 97, 112, 116, 101, 114, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 55, 53, 58, 53, 48, 45, 49, 55, 53, 
    58, 55, 48, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[47]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 46), 
    99, 97, 108, 108, 101, 100, 32, 96, 82, 101, 115, 117, 108, 116, 
    58, 58, 117, 110, 119, 114, 97, 112, 95, 101, 114, 114, 40, 41, 96, 
    32, 111, 110, 32, 97, 110, 32, 96, 79, 107, 96, 32, 118, 97, 108, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_135 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    115, 116, 114, 101, 97, 109, 32, 112, 105, 112, 101, 108, 105, 110, 
    101, 32, 108, 101, 97, 100, 32, 100, 114, 97, 105, 110, 115, 32, 
    115, 116, 114, 101, 97, 109, 115, 32, 97, 110, 100, 32, 114, 101, 
    116, 117, 114, 110, 115, 32, 101, 114, 114, 111, 114, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 53, 58, 51, 45, 50, 53, 58, 53, 53, 
    64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    83, 116, 114, 101, 97, 109, 32, 101, 114, 114, 111, 114, 58, 32, 
    98, 111, 111, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 52, 58, 52, 49, 45, 55, 52, 58, 53, 
    57, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    71, 108, 111, 98, 32, 101, 114, 114, 111, 114, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 50, 58, 49, 49, 45, 55, 50, 58, 50, 
    54, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 101, 110, 100, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 52, 54, 58, 49, 49, 45, 52, 54, 58, 51, 
    49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_95 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    67, 111, 110, 102, 105, 103, 32, 101, 114, 114, 111, 114, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 50, 52, 58, 49, 49, 45, 50, 52, 58, 50, 
    54, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 49, 52, 52, 58, 50, 55, 45, 49, 52, 52, 
    58, 55, 48, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_103 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    115, 116, 114, 101, 97, 109, 95, 112, 105, 112, 101, 108, 105, 110, 
    101, 47, 112, 105, 112, 101, 108, 105, 110, 101, 95, 116, 101, 115, 
    116, 46, 109, 98, 116, 58, 55, 51, 58, 52, 49, 45, 55, 51, 58, 53, 
    52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 116, 114, 101, 97, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    84, 97, 115, 107, 32, 102, 97, 105, 108, 101, 100, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[65]; 
} const moonbit_bytes_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 64), 
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 
    82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98, 99, 100, 101, 102, 103, 
    104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 
    57, 43, 47, 0
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2N9duplicateS1019$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2N9duplicateS1019
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2312l58$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2312l58
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__5_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__5_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0C2224l18$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0C2224l18
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__3_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1154$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1154
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__4_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__4_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__6_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__6_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE data;
  
} const _M0FP36mulpjs4mulp8through228empty__file__flush_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp8through228empty__file__flush_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1C2255l37$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1C2255l37
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2264l65$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2264l65
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__2_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2347l89$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2347l89
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEu data; 
} const _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2048l55$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2048l55
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5C2435l155$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5C2435l155
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE data;
  
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2353l86$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2353l86
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__4_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__4_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__5_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__5_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__6_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__6_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0FP36mulpjs4mulp8through224empty__file__flush_2eclo =
  (struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*)&_M0FP36mulpjs4mulp8through228empty__file__flush_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array5ArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGsE,
       .$method_1 = _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGRPB5ArrayGsEE}
  };

struct _M0BTPB4Show* _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC13int3IntPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6Logger data; 
} _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6Logger) >> 2, 0, 0),
    {.$method_0 = _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_1 = _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE,
       .$method_2 = _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_3 = _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger}
  };

struct _M0BTPB6Logger* _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id =
  &_M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC14bool4BoolPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1826 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test48moonbit__test__driver__internal__no__args__tests;

void* _M0FP36mulpjs4mulp8through228empty__file__flush_2edyncall(
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2641
) {
  return _M0FP36mulpjs4mulp8through218empty__file__flush();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2640
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2639
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__6();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2638
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2637
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2636
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2635
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test57____test__706970656c696e655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2634
) {
  return _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1();
}

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1175,
  moonbit_string_t _M0L8filenameS1150,
  int32_t _M0L5indexS1153
) {
  struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145* _closure_3108;
  struct _M0TWssbEu* _M0L14handle__resultS1145;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1154;
  void* _M0L11_2atry__errS1169;
  struct moonbit_result_0 _tmp_3110;
  int32_t _handle__error__result_3111;
  int32_t _M0L6_2atmpS2622;
  void* _M0L3errS1170;
  moonbit_string_t _M0L4nameS1172;
  struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1173;
  moonbit_string_t _M0L8_2afieldS2642;
  int32_t _M0L6_2acntS2895;
  moonbit_string_t _M0L7_2anameS1174;
  #line 532 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1150);
  _closure_3108
  = (struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145*)moonbit_malloc(sizeof(struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145));
  Moonbit_object_header(_closure_3108)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145, $1) >> 2, 1, 0);
  _closure_3108->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1145;
  _closure_3108->$0 = _M0L5indexS1153;
  _closure_3108->$1 = _M0L8filenameS1150;
  _M0L14handle__resultS1145 = (struct _M0TWssbEu*)_closure_3108;
  _M0L17error__to__stringS1154
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1154$closure.data;
  moonbit_incref(_M0L12async__testsS1175);
  moonbit_incref(_M0L17error__to__stringS1154);
  moonbit_incref(_M0L8filenameS1150);
  moonbit_incref(_M0L14handle__resultS1145);
  #line 566 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _tmp_3110
  = _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1175, _M0L8filenameS1150, _M0L5indexS1153, _M0L14handle__resultS1145, _M0L17error__to__stringS1154);
  if (_tmp_3110.tag) {
    int32_t const _M0L5_2aokS2631 = _tmp_3110.data.ok;
    _handle__error__result_3111 = _M0L5_2aokS2631;
  } else {
    void* const _M0L6_2aerrS2632 = _tmp_3110.data.err;
    moonbit_decref(_M0L12async__testsS1175);
    moonbit_decref(_M0L17error__to__stringS1154);
    moonbit_decref(_M0L8filenameS1150);
    _M0L11_2atry__errS1169 = _M0L6_2aerrS2632;
    goto join_1168;
  }
  if (_handle__error__result_3111) {
    moonbit_decref(_M0L12async__testsS1175);
    moonbit_decref(_M0L17error__to__stringS1154);
    moonbit_decref(_M0L8filenameS1150);
    _M0L6_2atmpS2622 = 1;
  } else {
    struct moonbit_result_0 _tmp_3112;
    int32_t _handle__error__result_3113;
    moonbit_incref(_M0L12async__testsS1175);
    moonbit_incref(_M0L17error__to__stringS1154);
    moonbit_incref(_M0L8filenameS1150);
    moonbit_incref(_M0L14handle__resultS1145);
    #line 569 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    _tmp_3112
    = _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1175, _M0L8filenameS1150, _M0L5indexS1153, _M0L14handle__resultS1145, _M0L17error__to__stringS1154);
    if (_tmp_3112.tag) {
      int32_t const _M0L5_2aokS2629 = _tmp_3112.data.ok;
      _handle__error__result_3113 = _M0L5_2aokS2629;
    } else {
      void* const _M0L6_2aerrS2630 = _tmp_3112.data.err;
      moonbit_decref(_M0L12async__testsS1175);
      moonbit_decref(_M0L17error__to__stringS1154);
      moonbit_decref(_M0L8filenameS1150);
      _M0L11_2atry__errS1169 = _M0L6_2aerrS2630;
      goto join_1168;
    }
    if (_handle__error__result_3113) {
      moonbit_decref(_M0L12async__testsS1175);
      moonbit_decref(_M0L17error__to__stringS1154);
      moonbit_decref(_M0L8filenameS1150);
      _M0L6_2atmpS2622 = 1;
    } else {
      struct moonbit_result_0 _tmp_3114;
      int32_t _handle__error__result_3115;
      moonbit_incref(_M0L12async__testsS1175);
      moonbit_incref(_M0L17error__to__stringS1154);
      moonbit_incref(_M0L8filenameS1150);
      moonbit_incref(_M0L14handle__resultS1145);
      #line 572 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _tmp_3114
      = _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1175, _M0L8filenameS1150, _M0L5indexS1153, _M0L14handle__resultS1145, _M0L17error__to__stringS1154);
      if (_tmp_3114.tag) {
        int32_t const _M0L5_2aokS2627 = _tmp_3114.data.ok;
        _handle__error__result_3115 = _M0L5_2aokS2627;
      } else {
        void* const _M0L6_2aerrS2628 = _tmp_3114.data.err;
        moonbit_decref(_M0L12async__testsS1175);
        moonbit_decref(_M0L17error__to__stringS1154);
        moonbit_decref(_M0L8filenameS1150);
        _M0L11_2atry__errS1169 = _M0L6_2aerrS2628;
        goto join_1168;
      }
      if (_handle__error__result_3115) {
        moonbit_decref(_M0L12async__testsS1175);
        moonbit_decref(_M0L17error__to__stringS1154);
        moonbit_decref(_M0L8filenameS1150);
        _M0L6_2atmpS2622 = 1;
      } else {
        struct moonbit_result_0 _tmp_3116;
        int32_t _handle__error__result_3117;
        moonbit_incref(_M0L12async__testsS1175);
        moonbit_incref(_M0L17error__to__stringS1154);
        moonbit_incref(_M0L8filenameS1150);
        moonbit_incref(_M0L14handle__resultS1145);
        #line 575 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        _tmp_3116
        = _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1175, _M0L8filenameS1150, _M0L5indexS1153, _M0L14handle__resultS1145, _M0L17error__to__stringS1154);
        if (_tmp_3116.tag) {
          int32_t const _M0L5_2aokS2625 = _tmp_3116.data.ok;
          _handle__error__result_3117 = _M0L5_2aokS2625;
        } else {
          void* const _M0L6_2aerrS2626 = _tmp_3116.data.err;
          moonbit_decref(_M0L12async__testsS1175);
          moonbit_decref(_M0L17error__to__stringS1154);
          moonbit_decref(_M0L8filenameS1150);
          _M0L11_2atry__errS1169 = _M0L6_2aerrS2626;
          goto join_1168;
        }
        if (_handle__error__result_3117) {
          moonbit_decref(_M0L12async__testsS1175);
          moonbit_decref(_M0L17error__to__stringS1154);
          moonbit_decref(_M0L8filenameS1150);
          _M0L6_2atmpS2622 = 1;
        } else {
          struct moonbit_result_0 _tmp_3118;
          moonbit_incref(_M0L14handle__resultS1145);
          #line 578 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
          _tmp_3118
          = _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1175, _M0L8filenameS1150, _M0L5indexS1153, _M0L14handle__resultS1145, _M0L17error__to__stringS1154);
          if (_tmp_3118.tag) {
            int32_t const _M0L5_2aokS2623 = _tmp_3118.data.ok;
            _M0L6_2atmpS2622 = _M0L5_2aokS2623;
          } else {
            void* const _M0L6_2aerrS2624 = _tmp_3118.data.err;
            _M0L11_2atry__errS1169 = _M0L6_2aerrS2624;
            goto join_1168;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2622) {
    void* _M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2633 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2633)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2633)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1169
    = _M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2633;
    goto join_1168;
  } else {
    moonbit_decref(_M0L14handle__resultS1145);
  }
  goto joinlet_3109;
  join_1168:;
  _M0L3errS1170 = _M0L11_2atry__errS1169;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1173
  = (struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1170;
  _M0L8_2afieldS2642 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1173->$0;
  _M0L6_2acntS2895
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1173)->rc;
  if (_M0L6_2acntS2895 > 1) {
    int32_t _M0L11_2anew__cntS2896 = _M0L6_2acntS2895 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1173)->rc
    = _M0L11_2anew__cntS2896;
    moonbit_incref(_M0L8_2afieldS2642);
  } else if (_M0L6_2acntS2895 == 1) {
    #line 585 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1173);
  }
  _M0L7_2anameS1174 = _M0L8_2afieldS2642;
  _M0L4nameS1172 = _M0L7_2anameS1174;
  goto join_1171;
  goto joinlet_3119;
  join_1171:;
  #line 586 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1145(_M0L14handle__resultS1145, _M0L4nameS1172, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3119:;
  joinlet_3109:;
  return 0;
}

moonbit_string_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1154(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2621,
  void* _M0L3errS1155
) {
  void* _M0L1eS1157;
  moonbit_string_t _M0L1eS1159;
  #line 555 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2621);
  switch (Moonbit_object_tag(_M0L3errS1155)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1160 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1155;
      moonbit_string_t _M0L8_2afieldS2643 = _M0L10_2aFailureS1160->$0;
      int32_t _M0L6_2acntS2897 =
        Moonbit_object_header(_M0L10_2aFailureS1160)->rc;
      moonbit_string_t _M0L4_2aeS1161;
      if (_M0L6_2acntS2897 > 1) {
        int32_t _M0L11_2anew__cntS2898 = _M0L6_2acntS2897 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1160)->rc
        = _M0L11_2anew__cntS2898;
        moonbit_incref(_M0L8_2afieldS2643);
      } else if (_M0L6_2acntS2897 == 1) {
        #line 556 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1160);
      }
      _M0L4_2aeS1161 = _M0L8_2afieldS2643;
      _M0L1eS1159 = _M0L4_2aeS1161;
      goto join_1158;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1162 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1155;
      moonbit_string_t _M0L8_2afieldS2644 = _M0L15_2aInspectErrorS1162->$0;
      int32_t _M0L6_2acntS2899 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1162)->rc;
      moonbit_string_t _M0L4_2aeS1163;
      if (_M0L6_2acntS2899 > 1) {
        int32_t _M0L11_2anew__cntS2900 = _M0L6_2acntS2899 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1162)->rc
        = _M0L11_2anew__cntS2900;
        moonbit_incref(_M0L8_2afieldS2644);
      } else if (_M0L6_2acntS2899 == 1) {
        #line 556 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1162);
      }
      _M0L4_2aeS1163 = _M0L8_2afieldS2644;
      _M0L1eS1159 = _M0L4_2aeS1163;
      goto join_1158;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1164 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1155;
      moonbit_string_t _M0L8_2afieldS2645 = _M0L16_2aSnapshotErrorS1164->$0;
      int32_t _M0L6_2acntS2901 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1164)->rc;
      moonbit_string_t _M0L4_2aeS1165;
      if (_M0L6_2acntS2901 > 1) {
        int32_t _M0L11_2anew__cntS2902 = _M0L6_2acntS2901 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1164)->rc
        = _M0L11_2anew__cntS2902;
        moonbit_incref(_M0L8_2afieldS2645);
      } else if (_M0L6_2acntS2901 == 1) {
        #line 556 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1164);
      }
      _M0L4_2aeS1165 = _M0L8_2afieldS2645;
      _M0L1eS1159 = _M0L4_2aeS1165;
      goto join_1158;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error118mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1166 =
        (struct _M0DTPC15error5Error118mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1155;
      moonbit_string_t _M0L8_2afieldS2646 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1166->$0;
      int32_t _M0L6_2acntS2903 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1166)->rc;
      moonbit_string_t _M0L4_2aeS1167;
      if (_M0L6_2acntS2903 > 1) {
        int32_t _M0L11_2anew__cntS2904 = _M0L6_2acntS2903 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1166)->rc
        = _M0L11_2anew__cntS2904;
        moonbit_incref(_M0L8_2afieldS2646);
      } else if (_M0L6_2acntS2903 == 1) {
        #line 556 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1166);
      }
      _M0L4_2aeS1167 = _M0L8_2afieldS2646;
      _M0L1eS1159 = _M0L4_2aeS1167;
      goto join_1158;
      break;
    }
    default: {
      _M0L1eS1157 = _M0L3errS1155;
      goto join_1156;
      break;
    }
  }
  join_1158:;
  return _M0L1eS1159;
  join_1156:;
  #line 561 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1157);
}

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1145(
  struct _M0TWssbEu* _M0L6_2aenvS2607,
  moonbit_string_t _M0L8testnameS1146,
  moonbit_string_t _M0L7messageS1147,
  int32_t _M0L7skippedS1148
) {
  struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145* _M0L14_2acasted__envS2608;
  moonbit_string_t _M0L8filenameS1150;
  int32_t _M0L5indexS1153;
  int32_t _M0L6_2acntS2905;
  int32_t _if__result_3122;
  moonbit_string_t _M0L10file__nameS1149;
  moonbit_string_t _M0L10test__nameS1151;
  moonbit_string_t _M0L7messageS1152;
  moonbit_string_t _M0L6_2atmpS2620;
  moonbit_string_t _M0L6_2atmpS2619;
  moonbit_string_t _M0L6_2atmpS2617;
  moonbit_string_t _M0L6_2atmpS2618;
  moonbit_string_t _M0L6_2atmpS2616;
  moonbit_string_t _M0L6_2atmpS2614;
  moonbit_string_t _M0L6_2atmpS2615;
  moonbit_string_t _M0L6_2atmpS2613;
  moonbit_string_t _M0L6_2atmpS2611;
  moonbit_string_t _M0L6_2atmpS2612;
  moonbit_string_t _M0L6_2atmpS2610;
  moonbit_string_t _M0L6_2atmpS2609;
  #line 539 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2608
  = (struct _M0R122_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1145*)_M0L6_2aenvS2607;
  _M0L8filenameS1150 = _M0L14_2acasted__envS2608->$1;
  _M0L5indexS1153 = _M0L14_2acasted__envS2608->$0;
  _M0L6_2acntS2905 = Moonbit_object_header(_M0L14_2acasted__envS2608)->rc;
  if (_M0L6_2acntS2905 > 1) {
    int32_t _M0L11_2anew__cntS2906 = _M0L6_2acntS2905 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2608)->rc
    = _M0L11_2anew__cntS2906;
    moonbit_incref(_M0L8filenameS1150);
  } else if (_M0L6_2acntS2905 == 1) {
    #line 539 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2608);
  }
  if (!_M0L7skippedS1148) {
    _if__result_3122 = 1;
  } else {
    _if__result_3122 = 0;
  }
  if (_if__result_3122) {
    
  }
  #line 545 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1149
  = _M0MPC16string6String14escape_2einner(_M0L8filenameS1150, 1);
  #line 546 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1151
  = _M0MPC16string6String14escape_2einner(_M0L8testnameS1146, 1);
  #line 547 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1152
  = _M0MPC16string6String14escape_2einner(_M0L7messageS1147, 1);
  #line 548 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 550 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2620
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1149);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2619
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2620);
  moonbit_decref(_M0L6_2atmpS2620);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2617
  = moonbit_add_string(_M0L6_2atmpS2619, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2619);
  #line 550 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2618 = _M0IPC13int3IntPB4Show10to__string(_M0L5indexS1153);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2616 = moonbit_add_string(_M0L6_2atmpS2617, _M0L6_2atmpS2618);
  moonbit_decref(_M0L6_2atmpS2618);
  moonbit_decref(_M0L6_2atmpS2617);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2614
  = moonbit_add_string(_M0L6_2atmpS2616, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2616);
  #line 550 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2615
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1151);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2613 = moonbit_add_string(_M0L6_2atmpS2614, _M0L6_2atmpS2615);
  moonbit_decref(_M0L6_2atmpS2615);
  moonbit_decref(_M0L6_2atmpS2614);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2611
  = moonbit_add_string(_M0L6_2atmpS2613, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2613);
  #line 550 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2612
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1152);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2610 = moonbit_add_string(_M0L6_2atmpS2611, _M0L6_2atmpS2612);
  moonbit_decref(_M0L6_2atmpS2612);
  moonbit_decref(_M0L6_2atmpS2611);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2609
  = moonbit_add_string(_M0L6_2atmpS2610, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2610);
  #line 549 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2609);
  #line 552 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1144,
  moonbit_string_t _M0L8filenameS1141,
  int32_t _M0L5indexS1135,
  struct _M0TWssbEu* _M0L14handle__resultS1131,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1133
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1111;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1140;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1113;
  moonbit_string_t* _M0L5attrsS1114;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1134;
  moonbit_string_t _M0L4nameS1117;
  moonbit_string_t _M0L4nameS1115;
  int32_t _M0L6_2atmpS2606;
  struct _M0TWEOs* _M0L5_2aitS1119;
  struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__* _closure_3131;
  struct _M0TWEu* _M0L6_2atmpS2597;
  struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__* _closure_3132;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2598;
  struct moonbit_result_0 _result_3133;
  #line 413 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1144);
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 420 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1140
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1141);
  if (_M0L7_2abindS1140 == 0) {
    struct moonbit_result_0 _result_3124;
    if (_M0L7_2abindS1140) {
      moonbit_decref(_M0L7_2abindS1140);
    }
    moonbit_decref(_M0L17error__to__stringS1133);
    moonbit_decref(_M0L14handle__resultS1131);
    _result_3124.tag = 1;
    _result_3124.data.ok = 0;
    return _result_3124;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1142 =
      _M0L7_2abindS1140;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1143 =
      _M0L7_2aSomeS1142;
    _M0L10index__mapS1111 = _M0L13_2aindex__mapS1143;
    goto join_1110;
  }
  join_1110:;
  #line 422 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1134
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1111, _M0L5indexS1135);
  if (_M0L7_2abindS1134 == 0) {
    struct moonbit_result_0 _result_3126;
    if (_M0L7_2abindS1134) {
      moonbit_decref(_M0L7_2abindS1134);
    }
    moonbit_decref(_M0L17error__to__stringS1133);
    moonbit_decref(_M0L14handle__resultS1131);
    _result_3126.tag = 1;
    _result_3126.data.ok = 0;
    return _result_3126;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1136 =
      _M0L7_2abindS1134;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1137 = _M0L7_2aSomeS1136;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1138 = _M0L4_2axS1137->$0;
    moonbit_string_t* _M0L8_2afieldS2649 = _M0L4_2axS1137->$1;
    int32_t _M0L6_2acntS2907 = Moonbit_object_header(_M0L4_2axS1137)->rc;
    moonbit_string_t* _M0L8_2aattrsS1139;
    if (_M0L6_2acntS2907 > 1) {
      int32_t _M0L11_2anew__cntS2908 = _M0L6_2acntS2907 - 1;
      Moonbit_object_header(_M0L4_2axS1137)->rc = _M0L11_2anew__cntS2908;
      moonbit_incref(_M0L8_2afieldS2649);
      moonbit_incref(_M0L4_2afS1138);
    } else if (_M0L6_2acntS2907 == 1) {
      #line 420 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1137);
    }
    _M0L8_2aattrsS1139 = _M0L8_2afieldS2649;
    _M0L1fS1113 = _M0L4_2afS1138;
    _M0L5attrsS1114 = _M0L8_2aattrsS1139;
    goto join_1112;
  }
  join_1112:;
  _M0L6_2atmpS2606 = Moonbit_array_length(_M0L5attrsS1114);
  if (_M0L6_2atmpS2606 >= 1) {
    moonbit_string_t _M0L7_2anameS1118 = (moonbit_string_t)_M0L5attrsS1114[0];
    moonbit_incref(_M0L7_2anameS1118);
    _M0L4nameS1117 = _M0L7_2anameS1118;
    goto join_1116;
  } else {
    _M0L4nameS1115 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3127;
  join_1116:;
  _M0L4nameS1115 = _M0L4nameS1117;
  joinlet_3127:;
  #line 423 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1119 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1114);
  while (1) {
    moonbit_string_t _M0L4attrS1121;
    moonbit_string_t _M0L7_2abindS1128;
    int32_t _M0L6_2atmpS2590;
    int64_t _M0L6_2atmpS2589;
    moonbit_incref(_M0L5_2aitS1119);
    #line 425 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1128 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1119);
    if (_M0L7_2abindS1128 == 0) {
      if (_M0L7_2abindS1128) {
        moonbit_decref(_M0L7_2abindS1128);
      }
      moonbit_decref(_M0L5_2aitS1119);
    } else {
      moonbit_string_t _M0L7_2aSomeS1129 = _M0L7_2abindS1128;
      moonbit_string_t _M0L7_2aattrS1130 = _M0L7_2aSomeS1129;
      _M0L4attrS1121 = _M0L7_2aattrS1130;
      goto join_1120;
    }
    goto joinlet_3129;
    join_1120:;
    _M0L6_2atmpS2590 = Moonbit_array_length(_M0L4attrS1121);
    _M0L6_2atmpS2589 = (int64_t)_M0L6_2atmpS2590;
    moonbit_incref(_M0L4attrS1121);
    #line 426 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1121, 5, 0, _M0L6_2atmpS2589)
    ) {
      int32_t _M0L6_2atmpS2596 = _M0L4attrS1121[0];
      int32_t _M0L4_2axS1122 = _M0L6_2atmpS2596;
      if (_M0L4_2axS1122 == 112) {
        int32_t _M0L6_2atmpS2595 = _M0L4attrS1121[1];
        int32_t _M0L4_2axS1123 = _M0L6_2atmpS2595;
        if (_M0L4_2axS1123 == 97) {
          int32_t _M0L6_2atmpS2594 = _M0L4attrS1121[2];
          int32_t _M0L4_2axS1124 = _M0L6_2atmpS2594;
          if (_M0L4_2axS1124 == 110) {
            int32_t _M0L6_2atmpS2593 = _M0L4attrS1121[3];
            int32_t _M0L4_2axS1125 = _M0L6_2atmpS2593;
            if (_M0L4_2axS1125 == 105) {
              int32_t _M0L6_2atmpS2592 = _M0L4attrS1121[4];
              int32_t _M0L4_2axS1126;
              moonbit_decref(_M0L4attrS1121);
              _M0L4_2axS1126 = _M0L6_2atmpS2592;
              if (_M0L4_2axS1126 == 99) {
                void* _M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2591;
                struct moonbit_result_0 _result_3130;
                moonbit_decref(_M0L17error__to__stringS1133);
                moonbit_decref(_M0L14handle__resultS1131);
                moonbit_decref(_M0L5_2aitS1119);
                moonbit_decref(_M0L1fS1113);
                _M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2591
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2591)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2591)->$0
                = _M0L4nameS1115;
                _result_3130.tag = 0;
                _result_3130.data.err
                = _M0L120mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2591;
                return _result_3130;
              }
            } else {
              moonbit_decref(_M0L4attrS1121);
            }
          } else {
            moonbit_decref(_M0L4attrS1121);
          }
        } else {
          moonbit_decref(_M0L4attrS1121);
        }
      } else {
        moonbit_decref(_M0L4attrS1121);
      }
    } else {
      moonbit_decref(_M0L4attrS1121);
    }
    continue;
    joinlet_3129:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1131);
  moonbit_incref(_M0L4nameS1115);
  _closure_3131
  = (struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__*)moonbit_malloc(sizeof(struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__));
  Moonbit_object_header(_closure_3131)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__, $0) >> 2, 2, 0);
  _closure_3131->code
  = &_M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testC2603l433;
  _closure_3131->$0 = _M0L14handle__resultS1131;
  _closure_3131->$1 = _M0L4nameS1115;
  _M0L6_2atmpS2597 = (struct _M0TWEu*)_closure_3131;
  _closure_3132
  = (struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__*)moonbit_malloc(sizeof(struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__));
  Moonbit_object_header(_closure_3132)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__, $0) >> 2, 3, 0);
  _closure_3132->code
  = &_M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testC2599l434;
  _closure_3132->$0 = _M0L17error__to__stringS1133;
  _closure_3132->$1 = _M0L14handle__resultS1131;
  _closure_3132->$2 = _M0L4nameS1115;
  _M0L6_2atmpS2598 = (struct _M0TWRPC15error5ErrorEu*)_closure_3132;
  #line 431 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1113, _M0L6_2atmpS2597, _M0L6_2atmpS2598);
  _result_3133.tag = 1;
  _result_3133.data.ok = 1;
  return _result_3133;
}

int32_t _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testC2603l433(
  struct _M0TWEu* _M0L6_2aenvS2604
) {
  struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__* _M0L14_2acasted__envS2605;
  moonbit_string_t _M0L4nameS1115;
  struct _M0TWssbEu* _M0L8_2afieldS2651;
  int32_t _M0L6_2acntS2909;
  struct _M0TWssbEu* _M0L14handle__resultS1131;
  #line 433 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2605
  = (struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2603__l433__*)_M0L6_2aenvS2604;
  _M0L4nameS1115 = _M0L14_2acasted__envS2605->$1;
  _M0L8_2afieldS2651 = _M0L14_2acasted__envS2605->$0;
  _M0L6_2acntS2909 = Moonbit_object_header(_M0L14_2acasted__envS2605)->rc;
  if (_M0L6_2acntS2909 > 1) {
    int32_t _M0L11_2anew__cntS2910 = _M0L6_2acntS2909 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2605)->rc
    = _M0L11_2anew__cntS2910;
    moonbit_incref(_M0L4nameS1115);
    moonbit_incref(_M0L8_2afieldS2651);
  } else if (_M0L6_2acntS2909 == 1) {
    #line 433 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2605);
  }
  _M0L14handle__resultS1131 = _M0L8_2afieldS2651;
  #line 433 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1131->code(_M0L14handle__resultS1131, _M0L4nameS1115, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP36mulpjs4mulp32stream__pipeline__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testC2599l434(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2600,
  void* _M0L3errS1132
) {
  struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__* _M0L14_2acasted__envS2601;
  moonbit_string_t _M0L4nameS1115;
  struct _M0TWssbEu* _M0L14handle__resultS1131;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2653;
  int32_t _M0L6_2acntS2911;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1133;
  moonbit_string_t _M0L6_2atmpS2602;
  #line 434 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2601
  = (struct _M0R213_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2599__l434__*)_M0L6_2aenvS2600;
  _M0L4nameS1115 = _M0L14_2acasted__envS2601->$2;
  _M0L14handle__resultS1131 = _M0L14_2acasted__envS2601->$1;
  _M0L8_2afieldS2653 = _M0L14_2acasted__envS2601->$0;
  _M0L6_2acntS2911 = Moonbit_object_header(_M0L14_2acasted__envS2601)->rc;
  if (_M0L6_2acntS2911 > 1) {
    int32_t _M0L11_2anew__cntS2912 = _M0L6_2acntS2911 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2601)->rc
    = _M0L11_2anew__cntS2912;
    moonbit_incref(_M0L4nameS1115);
    moonbit_incref(_M0L14handle__resultS1131);
    moonbit_incref(_M0L8_2afieldS2653);
  } else if (_M0L6_2acntS2911 == 1) {
    #line 434 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2601);
  }
  _M0L17error__to__stringS1133 = _M0L8_2afieldS2653;
  #line 434 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2602
  = _M0L17error__to__stringS1133->code(_M0L17error__to__stringS1133, _M0L3errS1132);
  #line 434 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1131->code(_M0L14handle__resultS1131, _M0L4nameS1115, _M0L6_2atmpS2602, 0);
  return 0;
}

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1105,
  struct _M0TWEu* _M0L6on__okS1106,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1103
) {
  void* _M0L11_2atry__errS1101;
  struct moonbit_result_0 _tmp_3135;
  void* _M0L3errS1102;
  #line 375 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  #line 382 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _tmp_3135 = _M0L1fS1105->code(_M0L1fS1105);
  if (_tmp_3135.tag) {
    int32_t const _M0L5_2aokS2587 = _tmp_3135.data.ok;
    moonbit_decref(_M0L7on__errS1103);
  } else {
    void* const _M0L6_2aerrS2588 = _tmp_3135.data.err;
    moonbit_decref(_M0L6on__okS1106);
    _M0L11_2atry__errS1101 = _M0L6_2aerrS2588;
    goto join_1100;
  }
  #line 382 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1106->code(_M0L6on__okS1106);
  goto joinlet_3134;
  join_1100:;
  _M0L3errS1102 = _M0L11_2atry__errS1101;
  #line 383 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1103->code(_M0L7on__errS1103, _M0L3errS1102);
  joinlet_3134:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1060;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1073;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1078;
  struct _M0TUsiE** _M0L6_2atmpS2586;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1085;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1086;
  moonbit_string_t _M0L6_2atmpS2585;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1087;
  int32_t _M0L7_2abindS1088;
  int32_t _M0L2__S1089;
  #line 193 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1060 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1073
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1078 = 0;
  _M0L6_2atmpS2586 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1085
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1085)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1085->$0 = _M0L6_2atmpS2586;
  _M0L16file__and__indexS1085->$1 = 0;
  #line 282 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1086
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1073(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1073);
  #line 284 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2585 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1086, 1);
  #line 283 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1087
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1078(_M0L51moonbit__test__driver__internal__split__mbt__stringS1078, _M0L6_2atmpS2585, 47);
  _M0L7_2abindS1088 = _M0L10test__argsS1087->$1;
  _M0L2__S1089 = 0;
  while (1) {
    if (_M0L2__S1089 < _M0L7_2abindS1088) {
      moonbit_string_t* _M0L3bufS2584 = _M0L10test__argsS1087->$0;
      moonbit_string_t _M0L3argS1090 =
        (moonbit_string_t)_M0L3bufS2584[_M0L2__S1089];
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1091;
      moonbit_string_t _M0L4fileS1092;
      moonbit_string_t _M0L5rangeS1093;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1094;
      moonbit_string_t _M0L6_2atmpS2582;
      int32_t _M0L5startS1095;
      moonbit_string_t _M0L6_2atmpS2581;
      int32_t _M0L3endS1096;
      int32_t _M0L1iS1097;
      int32_t _M0L6_2atmpS2583;
      moonbit_incref(_M0L3argS1090);
      #line 288 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1091
      = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1078(_M0L51moonbit__test__driver__internal__split__mbt__stringS1078, _M0L3argS1090, 58);
      moonbit_incref(_M0L16file__and__rangeS1091);
      #line 289 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1092
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1091, 0);
      #line 290 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1093
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1091, 1);
      #line 291 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1094
      = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1078(_M0L51moonbit__test__driver__internal__split__mbt__stringS1078, _M0L5rangeS1093, 45);
      moonbit_incref(_M0L15start__and__endS1094);
      #line 294 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2582
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1094, 0);
      #line 294 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1095
      = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1060(_M0L45moonbit__test__driver__internal__parse__int__S1060, _M0L6_2atmpS2582);
      #line 295 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2581
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1094, 1);
      #line 295 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1096
      = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1060(_M0L45moonbit__test__driver__internal__parse__int__S1060, _M0L6_2atmpS2581);
      _M0L1iS1097 = _M0L5startS1095;
      while (1) {
        if (_M0L1iS1097 < _M0L3endS1096) {
          struct _M0TUsiE* _M0L8_2atupleS2579;
          int32_t _M0L6_2atmpS2580;
          moonbit_incref(_M0L4fileS1092);
          _M0L8_2atupleS2579
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2579)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2579->$0 = _M0L4fileS1092;
          _M0L8_2atupleS2579->$1 = _M0L1iS1097;
          moonbit_incref(_M0L16file__and__indexS1085);
          #line 297 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1085, _M0L8_2atupleS2579);
          _M0L6_2atmpS2580 = _M0L1iS1097 + 1;
          _M0L1iS1097 = _M0L6_2atmpS2580;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1092);
        }
        break;
      }
      _M0L6_2atmpS2583 = _M0L2__S1089 + 1;
      _M0L2__S1089 = _M0L6_2atmpS2583;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1087);
    }
    break;
  }
  return _M0L16file__and__indexS1085;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1078(
  int32_t _M0L6_2aenvS2560,
  moonbit_string_t _M0L1sS1079,
  int32_t _M0L3sepS1080
) {
  moonbit_string_t* _M0L6_2atmpS2578;
  struct _M0TPB5ArrayGsE* _M0L3resS1081;
  struct _M0TPB8MutLocalGiE* _M0L1iS1082;
  struct _M0TPB8MutLocalGiE* _M0L5startS1083;
  int32_t _M0L3valS2573;
  int32_t _M0L6_2atmpS2574;
  #line 261 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2578 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1081
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1081)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1081->$0 = _M0L6_2atmpS2578;
  _M0L3resS1081->$1 = 0;
  _M0L1iS1082
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1082)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS1082->$0 = 0;
  _M0L5startS1083
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS1083)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5startS1083->$0 = 0;
  while (1) {
    int32_t _M0L3valS2561 = _M0L1iS1082->$0;
    int32_t _M0L6_2atmpS2562 = Moonbit_array_length(_M0L1sS1079);
    if (_M0L3valS2561 < _M0L6_2atmpS2562) {
      int32_t _M0L3valS2565 = _M0L1iS1082->$0;
      int32_t _M0L6_2atmpS2564;
      int32_t _M0L6_2atmpS2563;
      int32_t _M0L3valS2572;
      int32_t _M0L6_2atmpS2571;
      if (
        _M0L3valS2565 < 0
        || _M0L3valS2565 >= Moonbit_array_length(_M0L1sS1079)
      ) {
        #line 269 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2564 = _M0L1sS1079[_M0L3valS2565];
      _M0L6_2atmpS2563 = _M0L6_2atmpS2564;
      if (_M0L6_2atmpS2563 == _M0L3sepS1080) {
        int32_t _M0L3valS2567 = _M0L5startS1083->$0;
        int32_t _M0L3valS2568 = _M0L1iS1082->$0;
        moonbit_string_t _M0L6_2atmpS2566;
        int32_t _M0L3valS2570;
        int32_t _M0L6_2atmpS2569;
        moonbit_incref(_M0L1sS1079);
        #line 270 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS2566
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1079, _M0L3valS2567, _M0L3valS2568);
        moonbit_incref(_M0L3resS1081);
        #line 270 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1081, _M0L6_2atmpS2566);
        _M0L3valS2570 = _M0L1iS1082->$0;
        _M0L6_2atmpS2569 = _M0L3valS2570 + 1;
        _M0L5startS1083->$0 = _M0L6_2atmpS2569;
      }
      _M0L3valS2572 = _M0L1iS1082->$0;
      _M0L6_2atmpS2571 = _M0L3valS2572 + 1;
      _M0L1iS1082->$0 = _M0L6_2atmpS2571;
      continue;
    } else {
      moonbit_decref(_M0L1iS1082);
    }
    break;
  }
  _M0L3valS2573 = _M0L5startS1083->$0;
  _M0L6_2atmpS2574 = Moonbit_array_length(_M0L1sS1079);
  if (_M0L3valS2573 < _M0L6_2atmpS2574) {
    int32_t _M0L3valS2576 = _M0L5startS1083->$0;
    int32_t _M0L6_2atmpS2577;
    moonbit_string_t _M0L6_2atmpS2575;
    moonbit_decref(_M0L5startS1083);
    _M0L6_2atmpS2577 = Moonbit_array_length(_M0L1sS1079);
    #line 276 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS2575
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1079, _M0L3valS2576, _M0L6_2atmpS2577);
    moonbit_incref(_M0L3resS1081);
    #line 276 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1081, _M0L6_2atmpS2575);
  } else {
    moonbit_decref(_M0L5startS1083);
    moonbit_decref(_M0L1sS1079);
  }
  return _M0L3resS1081;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1073(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066
) {
  moonbit_bytes_t* _M0L3tmpS1074;
  int32_t _M0L6_2atmpS2559;
  struct _M0TPB5ArrayGsE* _M0L3resS1075;
  int32_t _M0L1iS1076;
  #line 250 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  #line 253 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1074
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2559 = Moonbit_array_length(_M0L3tmpS1074);
  #line 254 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1075 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2559);
  _M0L1iS1076 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2555 = Moonbit_array_length(_M0L3tmpS1074);
    if (_M0L1iS1076 < _M0L6_2atmpS2555) {
      moonbit_bytes_t _M0L6_2atmpS2557;
      moonbit_string_t _M0L6_2atmpS2556;
      int32_t _M0L6_2atmpS2558;
      if (
        _M0L1iS1076 < 0 || _M0L1iS1076 >= Moonbit_array_length(_M0L3tmpS1074)
      ) {
        #line 256 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2557 = (moonbit_bytes_t)_M0L3tmpS1074[_M0L1iS1076];
      moonbit_incref(_M0L6_2atmpS2557);
      #line 256 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2556
      = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066, _M0L6_2atmpS2557);
      moonbit_incref(_M0L3resS1075);
      #line 256 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1075, _M0L6_2atmpS2556);
      _M0L6_2atmpS2558 = _M0L1iS1076 + 1;
      _M0L1iS1076 = _M0L6_2atmpS2558;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1074);
    }
    break;
  }
  return _M0L3resS1075;
}

moonbit_string_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1066(
  int32_t _M0L6_2aenvS2469,
  moonbit_bytes_t _M0L5bytesS1067
) {
  struct _M0TPB13StringBuilder* _M0L3resS1068;
  int32_t _M0L3lenS1069;
  struct _M0TPB8MutLocalGiE* _M0L1iS1070;
  #line 206 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  #line 209 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1068 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1069 = Moonbit_array_length(_M0L5bytesS1067);
  _M0L1iS1070
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1070)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS1070->$0 = 0;
  while (1) {
    int32_t _M0L3valS2470 = _M0L1iS1070->$0;
    if (_M0L3valS2470 < _M0L3lenS1069) {
      int32_t _M0L3valS2554 = _M0L1iS1070->$0;
      int32_t _M0L6_2atmpS2553;
      int32_t _M0L6_2atmpS2552;
      struct _M0TPB8MutLocalGiE* _M0L1cS1071;
      int32_t _M0L3valS2471;
      if (
        _M0L3valS2554 < 0
        || _M0L3valS2554 >= Moonbit_array_length(_M0L5bytesS1067)
      ) {
        #line 213 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2553 = _M0L5bytesS1067[_M0L3valS2554];
      _M0L6_2atmpS2552 = (int32_t)_M0L6_2atmpS2553;
      _M0L1cS1071
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L1cS1071)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
      _M0L1cS1071->$0 = _M0L6_2atmpS2552;
      _M0L3valS2471 = _M0L1cS1071->$0;
      if (_M0L3valS2471 < 128) {
        int32_t _M0L3valS2473 = _M0L1cS1071->$0;
        int32_t _M0L6_2atmpS2472;
        int32_t _M0L3valS2475;
        int32_t _M0L6_2atmpS2474;
        moonbit_decref(_M0L1cS1071);
        _M0L6_2atmpS2472 = _M0L3valS2473;
        moonbit_incref(_M0L3resS1068);
        #line 215 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1068, _M0L6_2atmpS2472);
        _M0L3valS2475 = _M0L1iS1070->$0;
        _M0L6_2atmpS2474 = _M0L3valS2475 + 1;
        _M0L1iS1070->$0 = _M0L6_2atmpS2474;
      } else {
        int32_t _M0L3valS2476 = _M0L1cS1071->$0;
        if (_M0L3valS2476 < 224) {
          int32_t _M0L3valS2478 = _M0L1iS1070->$0;
          int32_t _M0L6_2atmpS2477 = _M0L3valS2478 + 1;
          int32_t _M0L3valS2487;
          int32_t _M0L6_2atmpS2486;
          int32_t _M0L6_2atmpS2480;
          int32_t _M0L3valS2485;
          int32_t _M0L6_2atmpS2484;
          int32_t _M0L6_2atmpS2483;
          int32_t _M0L6_2atmpS2482;
          int32_t _M0L6_2atmpS2481;
          int32_t _M0L6_2atmpS2479;
          int32_t _M0L3valS2489;
          int32_t _M0L6_2atmpS2488;
          int32_t _M0L3valS2491;
          int32_t _M0L6_2atmpS2490;
          if (_M0L6_2atmpS2477 >= _M0L3lenS1069) {
            moonbit_decref(_M0L1cS1071);
            moonbit_decref(_M0L1iS1070);
            moonbit_decref(_M0L5bytesS1067);
            break;
          }
          _M0L3valS2487 = _M0L1cS1071->$0;
          _M0L6_2atmpS2486 = _M0L3valS2487 & 31;
          _M0L6_2atmpS2480 = _M0L6_2atmpS2486 << 6;
          _M0L3valS2485 = _M0L1iS1070->$0;
          _M0L6_2atmpS2484 = _M0L3valS2485 + 1;
          if (
            _M0L6_2atmpS2484 < 0
            || _M0L6_2atmpS2484 >= Moonbit_array_length(_M0L5bytesS1067)
          ) {
            #line 221 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2483 = _M0L5bytesS1067[_M0L6_2atmpS2484];
          _M0L6_2atmpS2482 = (int32_t)_M0L6_2atmpS2483;
          _M0L6_2atmpS2481 = _M0L6_2atmpS2482 & 63;
          _M0L6_2atmpS2479 = _M0L6_2atmpS2480 | _M0L6_2atmpS2481;
          _M0L1cS1071->$0 = _M0L6_2atmpS2479;
          _M0L3valS2489 = _M0L1cS1071->$0;
          moonbit_decref(_M0L1cS1071);
          _M0L6_2atmpS2488 = _M0L3valS2489;
          moonbit_incref(_M0L3resS1068);
          #line 222 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1068, _M0L6_2atmpS2488);
          _M0L3valS2491 = _M0L1iS1070->$0;
          _M0L6_2atmpS2490 = _M0L3valS2491 + 2;
          _M0L1iS1070->$0 = _M0L6_2atmpS2490;
        } else {
          int32_t _M0L3valS2492 = _M0L1cS1071->$0;
          if (_M0L3valS2492 < 240) {
            int32_t _M0L3valS2494 = _M0L1iS1070->$0;
            int32_t _M0L6_2atmpS2493 = _M0L3valS2494 + 2;
            int32_t _M0L3valS2510;
            int32_t _M0L6_2atmpS2509;
            int32_t _M0L6_2atmpS2502;
            int32_t _M0L3valS2508;
            int32_t _M0L6_2atmpS2507;
            int32_t _M0L6_2atmpS2506;
            int32_t _M0L6_2atmpS2505;
            int32_t _M0L6_2atmpS2504;
            int32_t _M0L6_2atmpS2503;
            int32_t _M0L6_2atmpS2496;
            int32_t _M0L3valS2501;
            int32_t _M0L6_2atmpS2500;
            int32_t _M0L6_2atmpS2499;
            int32_t _M0L6_2atmpS2498;
            int32_t _M0L6_2atmpS2497;
            int32_t _M0L6_2atmpS2495;
            int32_t _M0L3valS2512;
            int32_t _M0L6_2atmpS2511;
            int32_t _M0L3valS2514;
            int32_t _M0L6_2atmpS2513;
            if (_M0L6_2atmpS2493 >= _M0L3lenS1069) {
              moonbit_decref(_M0L1cS1071);
              moonbit_decref(_M0L1iS1070);
              moonbit_decref(_M0L5bytesS1067);
              break;
            }
            _M0L3valS2510 = _M0L1cS1071->$0;
            _M0L6_2atmpS2509 = _M0L3valS2510 & 15;
            _M0L6_2atmpS2502 = _M0L6_2atmpS2509 << 12;
            _M0L3valS2508 = _M0L1iS1070->$0;
            _M0L6_2atmpS2507 = _M0L3valS2508 + 1;
            if (
              _M0L6_2atmpS2507 < 0
              || _M0L6_2atmpS2507 >= Moonbit_array_length(_M0L5bytesS1067)
            ) {
              #line 229 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2506 = _M0L5bytesS1067[_M0L6_2atmpS2507];
            _M0L6_2atmpS2505 = (int32_t)_M0L6_2atmpS2506;
            _M0L6_2atmpS2504 = _M0L6_2atmpS2505 & 63;
            _M0L6_2atmpS2503 = _M0L6_2atmpS2504 << 6;
            _M0L6_2atmpS2496 = _M0L6_2atmpS2502 | _M0L6_2atmpS2503;
            _M0L3valS2501 = _M0L1iS1070->$0;
            _M0L6_2atmpS2500 = _M0L3valS2501 + 2;
            if (
              _M0L6_2atmpS2500 < 0
              || _M0L6_2atmpS2500 >= Moonbit_array_length(_M0L5bytesS1067)
            ) {
              #line 230 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2499 = _M0L5bytesS1067[_M0L6_2atmpS2500];
            _M0L6_2atmpS2498 = (int32_t)_M0L6_2atmpS2499;
            _M0L6_2atmpS2497 = _M0L6_2atmpS2498 & 63;
            _M0L6_2atmpS2495 = _M0L6_2atmpS2496 | _M0L6_2atmpS2497;
            _M0L1cS1071->$0 = _M0L6_2atmpS2495;
            _M0L3valS2512 = _M0L1cS1071->$0;
            moonbit_decref(_M0L1cS1071);
            _M0L6_2atmpS2511 = _M0L3valS2512;
            moonbit_incref(_M0L3resS1068);
            #line 231 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1068, _M0L6_2atmpS2511);
            _M0L3valS2514 = _M0L1iS1070->$0;
            _M0L6_2atmpS2513 = _M0L3valS2514 + 3;
            _M0L1iS1070->$0 = _M0L6_2atmpS2513;
          } else {
            int32_t _M0L3valS2516 = _M0L1iS1070->$0;
            int32_t _M0L6_2atmpS2515 = _M0L3valS2516 + 3;
            int32_t _M0L3valS2539;
            int32_t _M0L6_2atmpS2538;
            int32_t _M0L6_2atmpS2531;
            int32_t _M0L3valS2537;
            int32_t _M0L6_2atmpS2536;
            int32_t _M0L6_2atmpS2535;
            int32_t _M0L6_2atmpS2534;
            int32_t _M0L6_2atmpS2533;
            int32_t _M0L6_2atmpS2532;
            int32_t _M0L6_2atmpS2524;
            int32_t _M0L3valS2530;
            int32_t _M0L6_2atmpS2529;
            int32_t _M0L6_2atmpS2528;
            int32_t _M0L6_2atmpS2527;
            int32_t _M0L6_2atmpS2526;
            int32_t _M0L6_2atmpS2525;
            int32_t _M0L6_2atmpS2518;
            int32_t _M0L3valS2523;
            int32_t _M0L6_2atmpS2522;
            int32_t _M0L6_2atmpS2521;
            int32_t _M0L6_2atmpS2520;
            int32_t _M0L6_2atmpS2519;
            int32_t _M0L6_2atmpS2517;
            int32_t _M0L3valS2541;
            int32_t _M0L6_2atmpS2540;
            int32_t _M0L3valS2545;
            int32_t _M0L6_2atmpS2544;
            int32_t _M0L6_2atmpS2543;
            int32_t _M0L6_2atmpS2542;
            int32_t _M0L3valS2549;
            int32_t _M0L6_2atmpS2548;
            int32_t _M0L6_2atmpS2547;
            int32_t _M0L6_2atmpS2546;
            int32_t _M0L3valS2551;
            int32_t _M0L6_2atmpS2550;
            if (_M0L6_2atmpS2515 >= _M0L3lenS1069) {
              moonbit_decref(_M0L1cS1071);
              moonbit_decref(_M0L1iS1070);
              moonbit_decref(_M0L5bytesS1067);
              break;
            }
            _M0L3valS2539 = _M0L1cS1071->$0;
            _M0L6_2atmpS2538 = _M0L3valS2539 & 7;
            _M0L6_2atmpS2531 = _M0L6_2atmpS2538 << 18;
            _M0L3valS2537 = _M0L1iS1070->$0;
            _M0L6_2atmpS2536 = _M0L3valS2537 + 1;
            if (
              _M0L6_2atmpS2536 < 0
              || _M0L6_2atmpS2536 >= Moonbit_array_length(_M0L5bytesS1067)
            ) {
              #line 238 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2535 = _M0L5bytesS1067[_M0L6_2atmpS2536];
            _M0L6_2atmpS2534 = (int32_t)_M0L6_2atmpS2535;
            _M0L6_2atmpS2533 = _M0L6_2atmpS2534 & 63;
            _M0L6_2atmpS2532 = _M0L6_2atmpS2533 << 12;
            _M0L6_2atmpS2524 = _M0L6_2atmpS2531 | _M0L6_2atmpS2532;
            _M0L3valS2530 = _M0L1iS1070->$0;
            _M0L6_2atmpS2529 = _M0L3valS2530 + 2;
            if (
              _M0L6_2atmpS2529 < 0
              || _M0L6_2atmpS2529 >= Moonbit_array_length(_M0L5bytesS1067)
            ) {
              #line 239 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2528 = _M0L5bytesS1067[_M0L6_2atmpS2529];
            _M0L6_2atmpS2527 = (int32_t)_M0L6_2atmpS2528;
            _M0L6_2atmpS2526 = _M0L6_2atmpS2527 & 63;
            _M0L6_2atmpS2525 = _M0L6_2atmpS2526 << 6;
            _M0L6_2atmpS2518 = _M0L6_2atmpS2524 | _M0L6_2atmpS2525;
            _M0L3valS2523 = _M0L1iS1070->$0;
            _M0L6_2atmpS2522 = _M0L3valS2523 + 3;
            if (
              _M0L6_2atmpS2522 < 0
              || _M0L6_2atmpS2522 >= Moonbit_array_length(_M0L5bytesS1067)
            ) {
              #line 240 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2521 = _M0L5bytesS1067[_M0L6_2atmpS2522];
            _M0L6_2atmpS2520 = (int32_t)_M0L6_2atmpS2521;
            _M0L6_2atmpS2519 = _M0L6_2atmpS2520 & 63;
            _M0L6_2atmpS2517 = _M0L6_2atmpS2518 | _M0L6_2atmpS2519;
            _M0L1cS1071->$0 = _M0L6_2atmpS2517;
            _M0L3valS2541 = _M0L1cS1071->$0;
            _M0L6_2atmpS2540 = _M0L3valS2541 - 65536;
            _M0L1cS1071->$0 = _M0L6_2atmpS2540;
            _M0L3valS2545 = _M0L1cS1071->$0;
            _M0L6_2atmpS2544 = _M0L3valS2545 >> 10;
            _M0L6_2atmpS2543 = _M0L6_2atmpS2544 + 55296;
            _M0L6_2atmpS2542 = _M0L6_2atmpS2543;
            moonbit_incref(_M0L3resS1068);
            #line 242 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1068, _M0L6_2atmpS2542);
            _M0L3valS2549 = _M0L1cS1071->$0;
            moonbit_decref(_M0L1cS1071);
            _M0L6_2atmpS2548 = _M0L3valS2549 & 1023;
            _M0L6_2atmpS2547 = _M0L6_2atmpS2548 + 56320;
            _M0L6_2atmpS2546 = _M0L6_2atmpS2547;
            moonbit_incref(_M0L3resS1068);
            #line 243 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1068, _M0L6_2atmpS2546);
            _M0L3valS2551 = _M0L1iS1070->$0;
            _M0L6_2atmpS2550 = _M0L3valS2551 + 4;
            _M0L1iS1070->$0 = _M0L6_2atmpS2550;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1070);
      moonbit_decref(_M0L5bytesS1067);
    }
    break;
  }
  #line 247 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1068);
}

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1060(
  int32_t _M0L6_2aenvS2462,
  moonbit_string_t _M0L1sS1061
) {
  struct _M0TPB8MutLocalGiE* _M0L3resS1062;
  int32_t _M0L3lenS1063;
  int32_t _M0L1iS1064;
  int32_t _result_3142;
  #line 197 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1062
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L3resS1062)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L3resS1062->$0 = 0;
  _M0L3lenS1063 = Moonbit_array_length(_M0L1sS1061);
  _M0L1iS1064 = 0;
  while (1) {
    if (_M0L1iS1064 < _M0L3lenS1063) {
      int32_t _M0L3valS2467 = _M0L3resS1062->$0;
      int32_t _M0L6_2atmpS2464 = _M0L3valS2467 * 10;
      int32_t _M0L6_2atmpS2466;
      int32_t _M0L6_2atmpS2465;
      int32_t _M0L6_2atmpS2463;
      int32_t _M0L6_2atmpS2468;
      if (
        _M0L1iS1064 < 0 || _M0L1iS1064 >= Moonbit_array_length(_M0L1sS1061)
      ) {
        #line 201 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2466 = _M0L1sS1061[_M0L1iS1064];
      _M0L6_2atmpS2465 = _M0L6_2atmpS2466 - 48;
      _M0L6_2atmpS2463 = _M0L6_2atmpS2464 + _M0L6_2atmpS2465;
      _M0L3resS1062->$0 = _M0L6_2atmpS2463;
      _M0L6_2atmpS2468 = _M0L1iS1064 + 1;
      _M0L1iS1064 = _M0L6_2atmpS2468;
      continue;
    } else {
      moonbit_decref(_M0L1sS1061);
    }
    break;
  }
  _result_3142 = _M0L3resS1062->$0;
  moonbit_decref(_M0L3resS1062);
  return _result_3142;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1040,
  moonbit_string_t _M0L12_2adiscard__S1041,
  int32_t _M0L12_2adiscard__S1042,
  struct _M0TWssbEu* _M0L12_2adiscard__S1043,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1044
) {
  struct moonbit_result_0 _result_3143;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1044);
  moonbit_decref(_M0L12_2adiscard__S1043);
  moonbit_decref(_M0L12_2adiscard__S1041);
  moonbit_decref(_M0L12_2adiscard__S1040);
  _result_3143.tag = 1;
  _result_3143.data.ok = 0;
  return _result_3143;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1045,
  moonbit_string_t _M0L12_2adiscard__S1046,
  int32_t _M0L12_2adiscard__S1047,
  struct _M0TWssbEu* _M0L12_2adiscard__S1048,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1049
) {
  struct moonbit_result_0 _result_3144;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1049);
  moonbit_decref(_M0L12_2adiscard__S1048);
  moonbit_decref(_M0L12_2adiscard__S1046);
  moonbit_decref(_M0L12_2adiscard__S1045);
  _result_3144.tag = 1;
  _result_3144.data.ok = 0;
  return _result_3144;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1050,
  moonbit_string_t _M0L12_2adiscard__S1051,
  int32_t _M0L12_2adiscard__S1052,
  struct _M0TWssbEu* _M0L12_2adiscard__S1053,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1054
) {
  struct moonbit_result_0 _result_3145;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1054);
  moonbit_decref(_M0L12_2adiscard__S1053);
  moonbit_decref(_M0L12_2adiscard__S1051);
  moonbit_decref(_M0L12_2adiscard__S1050);
  _result_3145.tag = 1;
  _result_3145.data.ok = 0;
  return _result_3145;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32stream__pipeline__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1055,
  moonbit_string_t _M0L12_2adiscard__S1056,
  int32_t _M0L12_2adiscard__S1057,
  struct _M0TWssbEu* _M0L12_2adiscard__S1058,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1059
) {
  struct moonbit_result_0 _result_3146;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1059);
  moonbit_decref(_M0L12_2adiscard__S1058);
  moonbit_decref(_M0L12_2adiscard__S1056);
  moonbit_decref(_M0L12_2adiscard__S1055);
  _result_3146.tag = 1;
  _result_3146.data.ok = 0;
  return _result_3146;
}

int32_t _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp32stream__pipeline__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1039
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1039);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__6(
  
) {
  void* _M0L6_2atmpS2461;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2459;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L6_2atmpS2460;
  void* _M0L6resultS1038;
  int32_t _M0L6_2atmpS2446;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2447;
  struct _M0TPB4Show _M0L6_2atmpS2439;
  moonbit_string_t _M0L6_2atmpS2442;
  moonbit_string_t _M0L6_2atmpS2443;
  moonbit_string_t _M0L6_2atmpS2444;
  moonbit_string_t _M0L6_2atmpS2445;
  moonbit_string_t* _M0L6_2atmpS2441;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2440;
  struct moonbit_result_0 _tmp_3147;
  void* _M0L6_2atmpS2458;
  moonbit_string_t _M0L6_2atmpS2457;
  struct _M0TPB4Show _M0L6_2atmpS2450;
  moonbit_string_t _M0L6_2atmpS2453;
  moonbit_string_t _M0L6_2atmpS2454;
  moonbit_string_t _M0L6_2atmpS2455;
  moonbit_string_t _M0L6_2atmpS2456;
  moonbit_string_t* _M0L6_2atmpS2452;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2451;
  #line 169 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  #line 171 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2461
  = _M0FP36mulpjs4mulp4core13stream__error((moonbit_string_t)moonbit_string_literal_9.data);
  #line 171 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2459
  = _M0FP36mulpjs4mulp6stream13error__stream(_M0L6_2atmpS2461);
  #line 172 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2460
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context();
  #line 170 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6resultS1038
  = _M0FP36mulpjs4mulp16stream__pipeline4lead(_M0L6_2atmpS2459, _M0L6_2atmpS2460);
  switch (Moonbit_object_tag(_M0L6resultS1038)) {
    case 0: {
      _M0L6_2atmpS2446 = 1;
      break;
    }
    default: {
      _M0L6_2atmpS2446 = 0;
      break;
    }
  }
  _M0L14_2aboxed__selfS2447
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2447)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2447->$0 = _M0L6_2atmpS2446;
  _M0L6_2atmpS2439
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2447
  };
  _M0L6_2atmpS2442 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS2443 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS2444 = 0;
  _M0L6_2atmpS2445 = 0;
  _M0L6_2atmpS2441 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2441[0] = _M0L6_2atmpS2442;
  _M0L6_2atmpS2441[1] = _M0L6_2atmpS2443;
  _M0L6_2atmpS2441[2] = _M0L6_2atmpS2444;
  _M0L6_2atmpS2441[3] = _M0L6_2atmpS2445;
  _M0L6_2atmpS2440
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2440)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2440->$0 = _M0L6_2atmpS2441;
  _M0L6_2atmpS2440->$1 = 4;
  #line 174 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3147
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2439, (moonbit_string_t)moonbit_string_literal_12.data, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2440);
  if (_tmp_3147.tag) {
    int32_t const _M0L5_2aokS2448 = _tmp_3147.data.ok;
  } else {
    void* const _M0L6_2aerrS2449 = _tmp_3147.data.err;
    struct moonbit_result_0 _result_3148;
    moonbit_decref(_M0L6resultS1038);
    _result_3148.tag = 0;
    _result_3148.data.err = _M0L6_2aerrS2449;
    return _result_3148;
  }
  #line 175 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2458
  = _M0MPC16result6Result11unwrap__errGuRP36mulpjs4mulp4core9MulpErrorE(_M0L6resultS1038);
  #line 175 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2457
  = _M0MP36mulpjs4mulp4core9MulpError7message(_M0L6_2atmpS2458);
  _M0L6_2atmpS2450
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2457
  };
  _M0L6_2atmpS2453 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS2454 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2455 = 0;
  _M0L6_2atmpS2456 = 0;
  _M0L6_2atmpS2452 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2452[0] = _M0L6_2atmpS2453;
  _M0L6_2atmpS2452[1] = _M0L6_2atmpS2454;
  _M0L6_2atmpS2452[2] = _M0L6_2atmpS2455;
  _M0L6_2atmpS2452[3] = _M0L6_2atmpS2456;
  _M0L6_2atmpS2451
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2451)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2451->$0 = _M0L6_2atmpS2452;
  _M0L6_2atmpS2451->$1 = 4;
  #line 175 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2450, (moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS2451);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5(
  
) {
  void* _M0L4TextS2438;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5inputS1034;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2434;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L4failS1035;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2433;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2432;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2429;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream** _M0L6_2atmpS2431;
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE* _M0L6_2atmpS2430;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2427;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L6_2atmpS2428;
  void* _M0L6resultS1037;
  int32_t _M0L6_2atmpS2414;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2415;
  struct _M0TPB4Show _M0L6_2atmpS2407;
  moonbit_string_t _M0L6_2atmpS2410;
  moonbit_string_t _M0L6_2atmpS2411;
  moonbit_string_t _M0L6_2atmpS2412;
  moonbit_string_t _M0L6_2atmpS2413;
  moonbit_string_t* _M0L6_2atmpS2409;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2408;
  struct moonbit_result_0 _tmp_3149;
  void* _M0L6_2atmpS2426;
  moonbit_string_t _M0L6_2atmpS2425;
  struct _M0TPB4Show _M0L6_2atmpS2418;
  moonbit_string_t _M0L6_2atmpS2421;
  moonbit_string_t _M0L6_2atmpS2422;
  moonbit_string_t _M0L6_2atmpS2423;
  moonbit_string_t _M0L6_2atmpS2424;
  moonbit_string_t* _M0L6_2atmpS2420;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2419;
  #line 148 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4TextS2438
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
  Moonbit_object_header(_M0L4TextS2438)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2438)->$0
  = (moonbit_string_t)moonbit_string_literal_18.data;
  #line 149 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L5inputS1034
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L4TextS2438);
  _M0L6_2atmpS2434
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5C2435l155$closure.data;
  #line 155 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4failS1035
  = _M0FP36mulpjs4mulp16stream__pipeline11to__through(_M0L6_2atmpS2434);
  _M0L6_2atmpS2433
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2433[0] = _M0L5inputS1034;
  _M0L6_2atmpS2432
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2432)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2432->$0 = _M0L6_2atmpS2433;
  _M0L6_2atmpS2432->$1 = 1;
  #line 158 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2429
  = _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2432);
  _M0L6_2atmpS2431
  = (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2431[0] = _M0L4failS1035;
  _M0L6_2atmpS2430
  = (struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L6_2atmpS2430)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2430->$0 = _M0L6_2atmpS2431;
  _M0L6_2atmpS2430->$1 = 1;
  #line 158 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2427
  = _M0FP36mulpjs4mulp16stream__pipeline7compose(_M0L6_2atmpS2429, _M0L6_2atmpS2430);
  #line 159 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2428
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context();
  #line 158 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6resultS1037
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS2427, _M0L6_2atmpS2428);
  switch (Moonbit_object_tag(_M0L6resultS1037)) {
    case 0: {
      _M0L6_2atmpS2414 = 1;
      break;
    }
    default: {
      _M0L6_2atmpS2414 = 0;
      break;
    }
  }
  _M0L14_2aboxed__selfS2415
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2415)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2415->$0 = _M0L6_2atmpS2414;
  _M0L6_2atmpS2407
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2415
  };
  _M0L6_2atmpS2410 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS2411 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS2412 = 0;
  _M0L6_2atmpS2413 = 0;
  _M0L6_2atmpS2409 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2409[0] = _M0L6_2atmpS2410;
  _M0L6_2atmpS2409[1] = _M0L6_2atmpS2411;
  _M0L6_2atmpS2409[2] = _M0L6_2atmpS2412;
  _M0L6_2atmpS2409[3] = _M0L6_2atmpS2413;
  _M0L6_2atmpS2408
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2408)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2408->$0 = _M0L6_2atmpS2409;
  _M0L6_2atmpS2408->$1 = 4;
  #line 161 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3149
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2407, (moonbit_string_t)moonbit_string_literal_12.data, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS2408);
  if (_tmp_3149.tag) {
    int32_t const _M0L5_2aokS2416 = _tmp_3149.data.ok;
  } else {
    void* const _M0L6_2aerrS2417 = _tmp_3149.data.err;
    struct moonbit_result_0 _result_3150;
    moonbit_decref(_M0L6resultS1037);
    _result_3150.tag = 0;
    _result_3150.data.err = _M0L6_2aerrS2417;
    return _result_3150;
  }
  #line 163 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2426
  = _M0MPC16result6Result11unwrap__errGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(_M0L6resultS1037);
  #line 163 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2425
  = _M0MP36mulpjs4mulp4core9MulpError7message(_M0L6_2atmpS2426);
  _M0L6_2atmpS2418
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2425
  };
  _M0L6_2atmpS2421 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS2422 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS2423 = 0;
  _M0L6_2atmpS2424 = 0;
  _M0L6_2atmpS2420 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2420[0] = _M0L6_2atmpS2421;
  _M0L6_2atmpS2420[1] = _M0L6_2atmpS2422;
  _M0L6_2atmpS2420[2] = _M0L6_2atmpS2423;
  _M0L6_2atmpS2420[3] = _M0L6_2atmpS2424;
  _M0L6_2atmpS2419
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2419)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2419->$0 = _M0L6_2atmpS2420;
  _M0L6_2atmpS2419->$1 = 4;
  #line 162 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2418, (moonbit_string_t)moonbit_string_literal_27.data, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS2419);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__5C2435l155(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2436,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6__fileS1036
) {
  void* _M0L6_2atmpS2437;
  #line 155 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6__fileS1036);
  moonbit_decref(_M0L6_2aenvS2436);
  #line 156 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2437
  = _M0FP36mulpjs4mulp4core13stream__error((moonbit_string_t)moonbit_string_literal_29.data);
  #line 156 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FP36mulpjs4mulp6stream13error__stream(_M0L6_2atmpS2437);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4(
  
) {
  moonbit_string_t* _M0L6_2atmpS2406;
  struct _M0TPB5ArrayGsE* _M0L6eventsS1027;
  struct _M0TPB8MutLocalGbE* _M0L7emittedS1028;
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__* _closure_3151;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2394;
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__* _closure_3152;
  struct _M0TWEu* _M0L6_2atmpS2395;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6sourceS1029;
  struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030* _closure_3153;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L4holdS1030;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream** _M0L6_2atmpS2393;
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE* _M0L6_2atmpS2392;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6outputS1033;
  void* _M0L6_2atmpS2382;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2381;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2380;
  moonbit_string_t _M0L6_2atmpS2379;
  struct _M0TPB4Show _M0L6_2atmpS2372;
  moonbit_string_t _M0L6_2atmpS2375;
  moonbit_string_t _M0L6_2atmpS2376;
  moonbit_string_t _M0L6_2atmpS2377;
  moonbit_string_t _M0L6_2atmpS2378;
  moonbit_string_t* _M0L6_2atmpS2374;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2373;
  struct moonbit_result_0 _tmp_3154;
  struct _M0TPB4Show _M0L6_2atmpS2385;
  moonbit_string_t _M0L6_2atmpS2388;
  moonbit_string_t _M0L6_2atmpS2389;
  moonbit_string_t _M0L6_2atmpS2390;
  moonbit_string_t _M0L6_2atmpS2391;
  moonbit_string_t* _M0L6_2atmpS2387;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2386;
  #line 108 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2406 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6eventsS1027
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6eventsS1027)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6eventsS1027->$0 = _M0L6_2atmpS2406;
  _M0L6eventsS1027->$1 = 0;
  _M0L7emittedS1028
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L7emittedS1028)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGbE) >> 2, 0, 0);
  _M0L7emittedS1028->$0 = 0;
  _closure_3151
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__*)moonbit_malloc(sizeof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__));
  Moonbit_object_header(_closure_3151)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__, $0) >> 2, 1, 0);
  _closure_3151->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2399l112;
  _closure_3151->$0 = _M0L7emittedS1028;
  _M0L6_2atmpS2394
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_3151;
  moonbit_incref(_M0L6eventsS1027);
  _closure_3152
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__*)moonbit_malloc(sizeof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__));
  Moonbit_object_header(_closure_3152)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__, $0) >> 2, 1, 0);
  _closure_3152->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2396l129;
  _closure_3152->$0 = _M0L6eventsS1027;
  _M0L6_2atmpS2395 = (struct _M0TWEu*)_closure_3152;
  #line 111 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6sourceS1029
  = _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L6_2atmpS2394, _M0L6_2atmpS2395);
  moonbit_incref(_M0L6eventsS1027);
  _closure_3153
  = (struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030*)moonbit_malloc(sizeof(struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030));
  Moonbit_object_header(_closure_3153)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030, $0) >> 2, 1, 0);
  _closure_3153->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4N4holdS1030;
  _closure_3153->$0 = _M0L6eventsS1027;
  _M0L4holdS1030
  = (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*)_closure_3153;
  _M0L6_2atmpS2393
  = (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2393[0] = _M0L4holdS1030;
  _M0L6_2atmpS2392
  = (struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L6_2atmpS2392)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2392->$0 = _M0L6_2atmpS2393;
  _M0L6_2atmpS2392->$1 = 1;
  #line 141 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6outputS1033
  = _M0FP36mulpjs4mulp16stream__pipeline7compose(_M0L6sourceS1029, _M0L6_2atmpS2392);
  moonbit_incref(_M0L6outputS1033);
  #line 142 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2382
  = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L6outputS1033);
  #line 142 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2381
  = _M0MPC16result6Result6unwrapGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS2382);
  #line 142 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2380
  = _M0MPC16option6Option6unwrapGRP36mulpjs4mulp6stream4FileE(_M0L6_2atmpS2381);
  #line 142 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2379
  = _M0MP36mulpjs4mulp6stream4File8basename(_M0L6_2atmpS2380);
  _M0L6_2atmpS2372
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2379
  };
  _M0L6_2atmpS2375 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS2376 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS2377 = 0;
  _M0L6_2atmpS2378 = 0;
  _M0L6_2atmpS2374 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2374[0] = _M0L6_2atmpS2375;
  _M0L6_2atmpS2374[1] = _M0L6_2atmpS2376;
  _M0L6_2atmpS2374[2] = _M0L6_2atmpS2377;
  _M0L6_2atmpS2374[3] = _M0L6_2atmpS2378;
  _M0L6_2atmpS2373
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2373)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2373->$0 = _M0L6_2atmpS2374;
  _M0L6_2atmpS2373->$1 = 4;
  #line 142 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3154
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2372, (moonbit_string_t)moonbit_string_literal_32.data, (moonbit_string_t)moonbit_string_literal_33.data, _M0L6_2atmpS2373);
  if (_tmp_3154.tag) {
    int32_t const _M0L5_2aokS2383 = _tmp_3154.data.ok;
  } else {
    void* const _M0L6_2aerrS2384 = _tmp_3154.data.err;
    struct moonbit_result_0 _result_3155;
    moonbit_decref(_M0L6outputS1033);
    moonbit_decref(_M0L6eventsS1027);
    _result_3155.tag = 0;
    _result_3155.data.err = _M0L6_2aerrS2384;
    return _result_3155;
  }
  #line 143 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L6outputS1033);
  _M0L6_2atmpS2385
  = (struct _M0TPB4Show){
    _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6eventsS1027
  };
  _M0L6_2atmpS2388 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS2389 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS2390 = 0;
  _M0L6_2atmpS2391 = 0;
  _M0L6_2atmpS2387 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2387[0] = _M0L6_2atmpS2388;
  _M0L6_2atmpS2387[1] = _M0L6_2atmpS2389;
  _M0L6_2atmpS2387[2] = _M0L6_2atmpS2390;
  _M0L6_2atmpS2387[3] = _M0L6_2atmpS2391;
  _M0L6_2atmpS2386
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2386)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2386->$0 = _M0L6_2atmpS2387;
  _M0L6_2atmpS2386->$1 = 4;
  #line 144 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2385, (moonbit_string_t)moonbit_string_literal_36.data, (moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS2386);
}

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2399l112(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2400
) {
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__* _M0L14_2acasted__envS2401;
  struct _M0TPB8MutLocalGbE* _M0L8_2afieldS2659;
  int32_t _M0L6_2acntS2913;
  struct _M0TPB8MutLocalGbE* _M0L7emittedS1028;
  #line 112 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L14_2acasted__envS2401
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2399__l112__*)_M0L6_2aenvS2400;
  _M0L8_2afieldS2659 = _M0L14_2acasted__envS2401->$0;
  _M0L6_2acntS2913 = Moonbit_object_header(_M0L14_2acasted__envS2401)->rc;
  if (_M0L6_2acntS2913 > 1) {
    int32_t _M0L11_2anew__cntS2914 = _M0L6_2acntS2913 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2401)->rc
    = _M0L11_2anew__cntS2914;
    moonbit_incref(_M0L8_2afieldS2659);
  } else if (_M0L6_2acntS2913 == 1) {
    #line 112 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2401);
  }
  _M0L7emittedS1028 = _M0L8_2afieldS2659;
  if (_M0L7emittedS1028->$0) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2402;
    void* _block_3156;
    moonbit_decref(_M0L7emittedS1028);
    _M0L6_2atmpS2402 = 0;
    _block_3156
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3156)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3156)->$0
    = _M0L6_2atmpS2402;
    return _block_3156;
  } else {
    void* _M0L4TextS2405;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2404;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2403;
    void* _block_3157;
    _M0L7emittedS1028->$0 = 1;
    moonbit_decref(_M0L7emittedS1028);
    _M0L4TextS2405
    = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
    Moonbit_object_header(_M0L4TextS2405)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
    ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2405)->$0
    = (moonbit_string_t)moonbit_string_literal_18.data;
    #line 119 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    _M0L6_2atmpS2404
    = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L4TextS2405);
    _M0L6_2atmpS2403 = _M0L6_2atmpS2404;
    _block_3157
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3157)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3157)->$0
    = _M0L6_2atmpS2403;
    return _block_3157;
  }
}

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2396l129(
  struct _M0TWEu* _M0L6_2aenvS2397
) {
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__* _M0L14_2acasted__envS2398;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS2660;
  int32_t _M0L6_2acntS2915;
  struct _M0TPB5ArrayGsE* _M0L6eventsS1027;
  #line 129 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L14_2acasted__envS2398
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2396__l129__*)_M0L6_2aenvS2397;
  _M0L8_2afieldS2660 = _M0L14_2acasted__envS2398->$0;
  _M0L6_2acntS2915 = Moonbit_object_header(_M0L14_2acasted__envS2398)->rc;
  if (_M0L6_2acntS2915 > 1) {
    int32_t _M0L11_2anew__cntS2916 = _M0L6_2acntS2915 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2398)->rc
    = _M0L11_2anew__cntS2916;
    moonbit_incref(_M0L8_2afieldS2660);
  } else if (_M0L6_2acntS2915 == 1) {
    #line 129 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2398);
  }
  _M0L6eventsS1027 = _M0L8_2afieldS2660;
  #line 129 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0MPC15array5Array4pushGsE(_M0L6eventsS1027, (moonbit_string_t)moonbit_string_literal_38.data);
  return 0;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4N4holdS1030(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2357,
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS1031
) {
  struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030* _M0L14_2acasted__envS2358;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS2661;
  int32_t _M0L6_2acntS2917;
  struct _M0TPB5ArrayGsE* _M0L6eventsS1027;
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__* _closure_3158;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2359;
  #line 131 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L14_2acasted__envS2358
  = (struct _M0R115_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2ehold_7c1030*)_M0L6_2aenvS2357;
  _M0L8_2afieldS2661 = _M0L14_2acasted__envS2358->$0;
  _M0L6_2acntS2917 = Moonbit_object_header(_M0L14_2acasted__envS2358)->rc;
  if (_M0L6_2acntS2917 > 1) {
    int32_t _M0L11_2anew__cntS2918 = _M0L6_2acntS2917 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2358)->rc
    = _M0L11_2anew__cntS2918;
    moonbit_incref(_M0L8_2afieldS2661);
  } else if (_M0L6_2acntS2917 == 1) {
    #line 131 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2358);
  }
  _M0L6eventsS1027 = _M0L8_2afieldS2661;
  _closure_3158
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__*)moonbit_malloc(sizeof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__));
  Moonbit_object_header(_closure_3158)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__, $0) >> 2, 1, 0);
  _closure_3158->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2360l134;
  _closure_3158->$0 = _M0L6eventsS1027;
  _M0L6_2atmpS2359
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)_closure_3158;
  #line 134 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0MP36mulpjs4mulp6stream10FileStream4pipe(_M0L8upstreamS1031, _M0L6_2atmpS2359);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2360l134(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2361,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1032
) {
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__* _M0L14_2acasted__envS2362;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS2662;
  int32_t _M0L6_2acntS2919;
  struct _M0TPB5ArrayGsE* _M0L6eventsS1027;
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__* _closure_3159;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2363;
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__* _closure_3160;
  struct _M0TWEu* _M0L6_2atmpS2364;
  #line 134 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L14_2acasted__envS2362
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2360__l134__*)_M0L6_2aenvS2361;
  _M0L8_2afieldS2662 = _M0L14_2acasted__envS2362->$0;
  _M0L6_2acntS2919 = Moonbit_object_header(_M0L14_2acasted__envS2362)->rc;
  if (_M0L6_2acntS2919 > 1) {
    int32_t _M0L11_2anew__cntS2920 = _M0L6_2acntS2919 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2362)->rc
    = _M0L11_2anew__cntS2920;
    moonbit_incref(_M0L8_2afieldS2662);
  } else if (_M0L6_2acntS2919 == 1) {
    #line 134 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2362);
  }
  _M0L6eventsS1027 = _M0L8_2afieldS2662;
  _closure_3159
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__*)moonbit_malloc(sizeof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__));
  Moonbit_object_header(_closure_3159)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__, $0) >> 2, 1, 0);
  _closure_3159->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2368l136;
  _closure_3159->$0 = _M0L4fileS1032;
  _M0L6_2atmpS2363
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_3159;
  _closure_3160
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__*)moonbit_malloc(sizeof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__));
  Moonbit_object_header(_closure_3160)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__, $0) >> 2, 1, 0);
  _closure_3160->code
  = &_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2365l137;
  _closure_3160->$0 = _M0L6eventsS1027;
  _M0L6_2atmpS2364 = (struct _M0TWEu*)_closure_3160;
  #line 135 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L6_2atmpS2363, _M0L6_2atmpS2364);
}

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2368l136(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2369
) {
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__* _M0L14_2acasted__envS2370;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2663;
  int32_t _M0L6_2acntS2921;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1032;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2371;
  void* _block_3161;
  #line 136 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L14_2acasted__envS2370
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2368__l136__*)_M0L6_2aenvS2369;
  _M0L8_2afieldS2663 = _M0L14_2acasted__envS2370->$0;
  _M0L6_2acntS2921 = Moonbit_object_header(_M0L14_2acasted__envS2370)->rc;
  if (_M0L6_2acntS2921 > 1) {
    int32_t _M0L11_2anew__cntS2922 = _M0L6_2acntS2921 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2370)->rc
    = _M0L11_2anew__cntS2922;
    moonbit_incref(_M0L8_2afieldS2663);
  } else if (_M0L6_2acntS2921 == 1) {
    #line 136 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2370);
  }
  _M0L4fileS1032 = _M0L8_2afieldS2663;
  _M0L6_2atmpS2371 = _M0L4fileS1032;
  _block_3161
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3161)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3161)->$0
  = _M0L6_2atmpS2371;
  return _block_3161;
}

int32_t _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__4C2365l137(
  struct _M0TWEu* _M0L6_2aenvS2366
) {
  struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__* _M0L14_2acasted__envS2367;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS2664;
  int32_t _M0L6_2acntS2923;
  struct _M0TPB5ArrayGsE* _M0L6eventsS1027;
  #line 137 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L14_2acasted__envS2367
  = (struct _M0R123_24mulpjs_2fmulp_2fstream__pipeline__blackbox__test_2e____test__706970656c696e655f746573742e6d6274__4_2eanon__u2365__l137__*)_M0L6_2aenvS2366;
  _M0L8_2afieldS2664 = _M0L14_2acasted__envS2367->$0;
  _M0L6_2acntS2923 = Moonbit_object_header(_M0L14_2acasted__envS2367)->rc;
  if (_M0L6_2acntS2923 > 1) {
    int32_t _M0L11_2anew__cntS2924 = _M0L6_2acntS2923 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2367)->rc
    = _M0L11_2anew__cntS2924;
    moonbit_incref(_M0L8_2afieldS2664);
  } else if (_M0L6_2acntS2923 == 1) {
    #line 137 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2367);
  }
  _M0L6eventsS1027 = _M0L8_2afieldS2664;
  #line 137 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0MPC15array5Array4pushGsE(_M0L6eventsS1027, (moonbit_string_t)moonbit_string_literal_39.data);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3(
  
) {
  void* _M0L4TextS2356;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5inputS1023;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2344;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2346;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2345;
  struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L4makeS1024;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2343;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2342;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2338;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2341;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream** _M0L6_2atmpS2340;
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE* _M0L6_2atmpS2339;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2336;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L6_2atmpS2337;
  void* _M0L6_2atmpS2335;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6outputS1026;
  int32_t _M0L6_2atmpS2322;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2323;
  struct _M0TPB4Show _M0L6_2atmpS2315;
  moonbit_string_t _M0L6_2atmpS2318;
  moonbit_string_t _M0L6_2atmpS2319;
  moonbit_string_t _M0L6_2atmpS2320;
  moonbit_string_t _M0L6_2atmpS2321;
  moonbit_string_t* _M0L6_2atmpS2317;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2316;
  struct moonbit_result_0 _tmp_3162;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2334;
  moonbit_string_t _M0L6_2atmpS2333;
  struct _M0TPB4Show _M0L6_2atmpS2326;
  moonbit_string_t _M0L6_2atmpS2329;
  moonbit_string_t _M0L6_2atmpS2330;
  moonbit_string_t _M0L6_2atmpS2331;
  moonbit_string_t _M0L6_2atmpS2332;
  moonbit_string_t* _M0L6_2atmpS2328;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2327;
  #line 78 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4TextS2356
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
  Moonbit_object_header(_M0L4TextS2356)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2356)->$0
  = (moonbit_string_t)moonbit_string_literal_18.data;
  #line 79 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L5inputS1023
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L4TextS2356);
  _M0L6_2atmpS2344
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2353l86$closure.data;
  _M0L6_2atmpS2346
  = (struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2347l89$closure.data;
  _M0L6_2atmpS2345 = _M0L6_2atmpS2346;
  #line 85 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4makeS1024
  = _M0FP36mulpjs4mulp8through225file__transformer_2einner(_M0L6_2atmpS2344, _M0L6_2atmpS2345);
  _M0L6_2atmpS2343
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2343[0] = _M0L5inputS1023;
  _M0L6_2atmpS2342
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2342)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2342->$0 = _M0L6_2atmpS2343;
  _M0L6_2atmpS2342->$1 = 1;
  #line 100 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2338
  = _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2342);
  #line 100 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2341 = _M0L4makeS1024->code(_M0L4makeS1024);
  _M0L6_2atmpS2340
  = (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2340[0] = _M0L6_2atmpS2341;
  _M0L6_2atmpS2339
  = (struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L6_2atmpS2339)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2339->$0 = _M0L6_2atmpS2340;
  _M0L6_2atmpS2339->$1 = 1;
  #line 100 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2336
  = _M0FP36mulpjs4mulp16stream__pipeline7compose(_M0L6_2atmpS2338, _M0L6_2atmpS2339);
  #line 101 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2337
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context();
  #line 100 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2335
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS2336, _M0L6_2atmpS2337);
  #line 100 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6outputS1026
  = _M0MPC16result6Result6unwrapGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS2335);
  moonbit_incref(_M0L6outputS1026);
  #line 103 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2322
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1026);
  _M0L14_2aboxed__selfS2323
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2323)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2323->$0 = _M0L6_2atmpS2322;
  _M0L6_2atmpS2315
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2323
  };
  _M0L6_2atmpS2318 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS2319 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS2320 = 0;
  _M0L6_2atmpS2321 = 0;
  _M0L6_2atmpS2317 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2317[0] = _M0L6_2atmpS2318;
  _M0L6_2atmpS2317[1] = _M0L6_2atmpS2319;
  _M0L6_2atmpS2317[2] = _M0L6_2atmpS2320;
  _M0L6_2atmpS2317[3] = _M0L6_2atmpS2321;
  _M0L6_2atmpS2316
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2316)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2316->$0 = _M0L6_2atmpS2317;
  _M0L6_2atmpS2316->$1 = 4;
  #line 103 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3162
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2315, (moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_43.data, _M0L6_2atmpS2316);
  if (_tmp_3162.tag) {
    int32_t const _M0L5_2aokS2324 = _tmp_3162.data.ok;
  } else {
    void* const _M0L6_2aerrS2325 = _tmp_3162.data.err;
    struct moonbit_result_0 _result_3163;
    moonbit_decref(_M0L6outputS1026);
    _result_3163.tag = 0;
    _result_3163.data.err = _M0L6_2aerrS2325;
    return _result_3163;
  }
  #line 104 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2334
  = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1026, 1);
  #line 104 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2333
  = _M0MP36mulpjs4mulp6stream4File8basename(_M0L6_2atmpS2334);
  _M0L6_2atmpS2326
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2333
  };
  _M0L6_2atmpS2329 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS2330 = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS2331 = 0;
  _M0L6_2atmpS2332 = 0;
  _M0L6_2atmpS2328 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2328[0] = _M0L6_2atmpS2329;
  _M0L6_2atmpS2328[1] = _M0L6_2atmpS2330;
  _M0L6_2atmpS2328[2] = _M0L6_2atmpS2331;
  _M0L6_2atmpS2328[3] = _M0L6_2atmpS2332;
  _M0L6_2atmpS2327
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2327)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2327->$0 = _M0L6_2atmpS2328;
  _M0L6_2atmpS2327->$1 = 4;
  #line 104 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2326, (moonbit_string_t)moonbit_string_literal_46.data, (moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS2327);
}

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2353l86(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2354,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1025
) {
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2355;
  void* _block_3164;
  #line 86 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2354);
  _M0L6_2atmpS2355 = _M0L4fileS1025;
  _block_3164
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3164)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3164)->$0
  = _M0L6_2atmpS2355;
  return _block_3164;
}

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__3C2347l89(
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2348
) {
  void* _M0L4TextS2352;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2351;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2350;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2349;
  void* _block_3165;
  #line 89 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2348);
  _M0L4TextS2352
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
  Moonbit_object_header(_M0L4TextS2352)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2352)->$0
  = (moonbit_string_t)moonbit_string_literal_48.data;
  #line 91 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2351
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_49.data, _M0L4TextS2352);
  _M0L6_2atmpS2350
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2350[0] = _M0L6_2atmpS2351;
  _M0L6_2atmpS2349
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2349)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2349->$0 = _M0L6_2atmpS2350;
  _M0L6_2atmpS2349->$1 = 1;
  _block_3165
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3165)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3165)->$0
  = _M0L6_2atmpS2349;
  return _block_3165;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2(
  
) {
  void* _M0L4TextS2314;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5inputS1016;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L6_2atmpS2311;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2310;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6renameS1017;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L9duplicateS1019;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2309;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2308;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2305;
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream** _M0L6_2atmpS2307;
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE* _M0L6_2atmpS2306;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2303;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L6_2atmpS2304;
  void* _M0L6_2atmpS2302;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6outputS1022;
  int32_t _M0L6_2atmpS2278;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2279;
  struct _M0TPB4Show _M0L6_2atmpS2271;
  moonbit_string_t _M0L6_2atmpS2274;
  moonbit_string_t _M0L6_2atmpS2275;
  moonbit_string_t _M0L6_2atmpS2276;
  moonbit_string_t _M0L6_2atmpS2277;
  moonbit_string_t* _M0L6_2atmpS2273;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2272;
  struct moonbit_result_0 _tmp_3166;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2290;
  moonbit_string_t _M0L6_2atmpS2289;
  struct _M0TPB4Show _M0L6_2atmpS2282;
  moonbit_string_t _M0L6_2atmpS2285;
  moonbit_string_t _M0L6_2atmpS2286;
  moonbit_string_t _M0L6_2atmpS2287;
  moonbit_string_t _M0L6_2atmpS2288;
  moonbit_string_t* _M0L6_2atmpS2284;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2283;
  struct moonbit_result_0 _tmp_3168;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2301;
  moonbit_string_t _M0L6_2atmpS2300;
  struct _M0TPB4Show _M0L6_2atmpS2293;
  moonbit_string_t _M0L6_2atmpS2296;
  moonbit_string_t _M0L6_2atmpS2297;
  moonbit_string_t _M0L6_2atmpS2298;
  moonbit_string_t _M0L6_2atmpS2299;
  moonbit_string_t* _M0L6_2atmpS2295;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2294;
  #line 50 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4TextS2314
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
  Moonbit_object_header(_M0L4TextS2314)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2314)->$0
  = (moonbit_string_t)moonbit_string_literal_18.data;
  #line 51 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L5inputS1016
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L4TextS2314);
  _M0L6_2atmpS2311
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2312l58$closure.data;
  #line 58 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2310 = _M0FP36mulpjs4mulp6stream10map__files(_M0L6_2atmpS2311);
  #line 57 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6renameS1017
  = _M0FP36mulpjs4mulp16stream__pipeline11to__through(_M0L6_2atmpS2310);
  _M0L9duplicateS1019
  = (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2N9duplicateS1019$closure.data;
  _M0L6_2atmpS2309
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2309[0] = _M0L5inputS1016;
  _M0L6_2atmpS2308
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2308)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2308->$0 = _M0L6_2atmpS2309;
  _M0L6_2atmpS2308->$1 = 1;
  #line 69 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2305
  = _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2308);
  _M0L6_2atmpS2307
  = (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2307[0] = _M0L6renameS1017;
  _M0L6_2atmpS2307[1] = _M0L9duplicateS1019;
  _M0L6_2atmpS2306
  = (struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L6_2atmpS2306)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2306->$0 = _M0L6_2atmpS2307;
  _M0L6_2atmpS2306->$1 = 2;
  #line 69 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2303
  = _M0FP36mulpjs4mulp16stream__pipeline7compose(_M0L6_2atmpS2305, _M0L6_2atmpS2306);
  #line 70 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2304
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context();
  #line 69 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2302
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS2303, _M0L6_2atmpS2304);
  #line 69 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6outputS1022
  = _M0MPC16result6Result6unwrapGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS2302);
  moonbit_incref(_M0L6outputS1022);
  #line 72 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2278
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1022);
  _M0L14_2aboxed__selfS2279
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2279)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2279->$0 = _M0L6_2atmpS2278;
  _M0L6_2atmpS2271
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2279
  };
  _M0L6_2atmpS2274 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS2275 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS2276 = 0;
  _M0L6_2atmpS2277 = 0;
  _M0L6_2atmpS2273 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2273[0] = _M0L6_2atmpS2274;
  _M0L6_2atmpS2273[1] = _M0L6_2atmpS2275;
  _M0L6_2atmpS2273[2] = _M0L6_2atmpS2276;
  _M0L6_2atmpS2273[3] = _M0L6_2atmpS2277;
  _M0L6_2atmpS2272
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2272)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2272->$0 = _M0L6_2atmpS2273;
  _M0L6_2atmpS2272->$1 = 4;
  #line 72 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3166
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2271, (moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_52.data, _M0L6_2atmpS2272);
  if (_tmp_3166.tag) {
    int32_t const _M0L5_2aokS2280 = _tmp_3166.data.ok;
  } else {
    void* const _M0L6_2aerrS2281 = _tmp_3166.data.err;
    struct moonbit_result_0 _result_3167;
    moonbit_decref(_M0L6outputS1022);
    _result_3167.tag = 0;
    _result_3167.data.err = _M0L6_2aerrS2281;
    return _result_3167;
  }
  moonbit_incref(_M0L6outputS1022);
  #line 73 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2290
  = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1022, 0);
  #line 73 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2289
  = _M0MP36mulpjs4mulp6stream4File8basename(_M0L6_2atmpS2290);
  _M0L6_2atmpS2282
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2289
  };
  _M0L6_2atmpS2285 = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS2286 = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS2287 = 0;
  _M0L6_2atmpS2288 = 0;
  _M0L6_2atmpS2284 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2284[0] = _M0L6_2atmpS2285;
  _M0L6_2atmpS2284[1] = _M0L6_2atmpS2286;
  _M0L6_2atmpS2284[2] = _M0L6_2atmpS2287;
  _M0L6_2atmpS2284[3] = _M0L6_2atmpS2288;
  _M0L6_2atmpS2283
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2283)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2283->$0 = _M0L6_2atmpS2284;
  _M0L6_2atmpS2283->$1 = 4;
  #line 73 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3168
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2282, (moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_56.data, _M0L6_2atmpS2283);
  if (_tmp_3168.tag) {
    int32_t const _M0L5_2aokS2291 = _tmp_3168.data.ok;
  } else {
    void* const _M0L6_2aerrS2292 = _tmp_3168.data.err;
    struct moonbit_result_0 _result_3169;
    moonbit_decref(_M0L6outputS1022);
    _result_3169.tag = 0;
    _result_3169.data.err = _M0L6_2aerrS2292;
    return _result_3169;
  }
  #line 74 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2301
  = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1022, 1);
  #line 74 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2300
  = _M0MP36mulpjs4mulp6stream4File8basename(_M0L6_2atmpS2301);
  _M0L6_2atmpS2293
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2300
  };
  _M0L6_2atmpS2296 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS2297 = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS2298 = 0;
  _M0L6_2atmpS2299 = 0;
  _M0L6_2atmpS2295 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2295[0] = _M0L6_2atmpS2296;
  _M0L6_2atmpS2295[1] = _M0L6_2atmpS2297;
  _M0L6_2atmpS2295[2] = _M0L6_2atmpS2298;
  _M0L6_2atmpS2295[3] = _M0L6_2atmpS2299;
  _M0L6_2atmpS2294
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2294)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2294->$0 = _M0L6_2atmpS2295;
  _M0L6_2atmpS2294->$1 = 4;
  #line 74 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2293, (moonbit_string_t)moonbit_string_literal_59.data, (moonbit_string_t)moonbit_string_literal_60.data, _M0L6_2atmpS2294);
}

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2312l58(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L6_2aenvS2313,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1018
) {
  #line 58 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2313);
  #line 59 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0MP36mulpjs4mulp6stream4File10with__path(_M0L4fileS1018, (moonbit_string_t)moonbit_string_literal_61.data);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2N9duplicateS1019(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2262,
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6sourceS1020
) {
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2263;
  #line 62 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2262);
  _M0L6_2atmpS2263
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2264l65$closure.data;
  #line 65 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0MP36mulpjs4mulp6stream10FileStream4pipe(_M0L6sourceS1020, _M0L6_2atmpS2263);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__2C2264l65(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2265,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1021
) {
  moonbit_string_t _M0L4pathS2270;
  moonbit_string_t _M0L6_2atmpS2269;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2268;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2267;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2266;
  #line 65 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2265);
  _M0L4pathS2270 = _M0L4fileS1021->$2;
  #line 66 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2269
  = moonbit_add_string(_M0L4pathS2270, (moonbit_string_t)moonbit_string_literal_62.data);
  moonbit_incref(_M0L4fileS1021);
  #line 66 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2268
  = _M0MP36mulpjs4mulp6stream4File10with__path(_M0L4fileS1021, _M0L6_2atmpS2269);
  _M0L6_2atmpS2267
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2267[0] = _M0L4fileS1021;
  _M0L6_2atmpS2267[1] = _M0L6_2atmpS2268;
  _M0L6_2atmpS2266
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2266)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2266->$0 = _M0L6_2atmpS2267;
  _M0L6_2atmpS2266->$1 = 2;
  #line 66 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2266);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1(
  
) {
  void* _M0L4TextS2261;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5inputS1013;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2260;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2259;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2250;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2254;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2253;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream** _M0L6_2atmpS2252;
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE* _M0L6_2atmpS2251;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2248;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L6_2atmpS2249;
  void* _M0L6_2atmpS2247;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6outputS1014;
  int32_t _M0L6_2atmpS2234;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2235;
  struct _M0TPB4Show _M0L6_2atmpS2227;
  moonbit_string_t _M0L6_2atmpS2230;
  moonbit_string_t _M0L6_2atmpS2231;
  moonbit_string_t _M0L6_2atmpS2232;
  moonbit_string_t _M0L6_2atmpS2233;
  moonbit_string_t* _M0L6_2atmpS2229;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2228;
  struct moonbit_result_0 _tmp_3170;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2246;
  moonbit_string_t _M0L6_2atmpS2245;
  struct _M0TPB4Show _M0L6_2atmpS2238;
  moonbit_string_t _M0L6_2atmpS2241;
  moonbit_string_t _M0L6_2atmpS2242;
  moonbit_string_t _M0L6_2atmpS2243;
  moonbit_string_t _M0L6_2atmpS2244;
  moonbit_string_t* _M0L6_2atmpS2240;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2239;
  #line 29 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4TextS2261
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
  Moonbit_object_header(_M0L4TextS2261)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2261)->$0
  = (moonbit_string_t)moonbit_string_literal_18.data;
  #line 30 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L5inputS1013
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L4TextS2261);
  _M0L6_2atmpS2260
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2260[0] = _M0L5inputS1013;
  _M0L6_2atmpS2259
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2259)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2259->$0 = _M0L6_2atmpS2260;
  _M0L6_2atmpS2259->$1 = 1;
  #line 36 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2250
  = _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2259);
  _M0L6_2atmpS2254
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1C2255l37$closure.data;
  #line 37 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2253
  = _M0FP36mulpjs4mulp8through214through__files(_M0L6_2atmpS2254);
  _M0L6_2atmpS2252
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2252[0] = _M0L6_2atmpS2253;
  _M0L6_2atmpS2251
  = (struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L6_2atmpS2251)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2251->$0 = _M0L6_2atmpS2252;
  _M0L6_2atmpS2251->$1 = 1;
  #line 36 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2248
  = _M0FP36mulpjs4mulp16stream__pipeline8pipeline(_M0L6_2atmpS2250, _M0L6_2atmpS2251);
  #line 43 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2249
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context();
  #line 36 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2247
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS2248, _M0L6_2atmpS2249);
  #line 36 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6outputS1014
  = _M0MPC16result6Result6unwrapGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS2247);
  moonbit_incref(_M0L6outputS1014);
  #line 45 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2234
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1014);
  _M0L14_2aboxed__selfS2235
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2235)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2235->$0 = _M0L6_2atmpS2234;
  _M0L6_2atmpS2227
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2235
  };
  _M0L6_2atmpS2230 = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS2231 = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS2232 = 0;
  _M0L6_2atmpS2233 = 0;
  _M0L6_2atmpS2229 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2229[0] = _M0L6_2atmpS2230;
  _M0L6_2atmpS2229[1] = _M0L6_2atmpS2231;
  _M0L6_2atmpS2229[2] = _M0L6_2atmpS2232;
  _M0L6_2atmpS2229[3] = _M0L6_2atmpS2233;
  _M0L6_2atmpS2228
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2228)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2228->$0 = _M0L6_2atmpS2229;
  _M0L6_2atmpS2228->$1 = 4;
  #line 45 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3170
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2227, (moonbit_string_t)moonbit_string_literal_65.data, (moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS2228);
  if (_tmp_3170.tag) {
    int32_t const _M0L5_2aokS2236 = _tmp_3170.data.ok;
  } else {
    void* const _M0L6_2aerrS2237 = _tmp_3170.data.err;
    struct moonbit_result_0 _result_3171;
    moonbit_decref(_M0L6outputS1014);
    _result_3171.tag = 0;
    _result_3171.data.err = _M0L6_2aerrS2237;
    return _result_3171;
  }
  #line 46 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2246
  = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1014, 0);
  #line 46 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2245
  = _M0MP36mulpjs4mulp6stream4File8basename(_M0L6_2atmpS2246);
  _M0L6_2atmpS2238
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2245
  };
  _M0L6_2atmpS2241 = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS2242 = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS2243 = 0;
  _M0L6_2atmpS2244 = 0;
  _M0L6_2atmpS2240 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2240[0] = _M0L6_2atmpS2241;
  _M0L6_2atmpS2240[1] = _M0L6_2atmpS2242;
  _M0L6_2atmpS2240[2] = _M0L6_2atmpS2243;
  _M0L6_2atmpS2240[3] = _M0L6_2atmpS2244;
  _M0L6_2atmpS2239
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2239)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2239->$0 = _M0L6_2atmpS2240;
  _M0L6_2atmpS2239->$1 = 4;
  #line 46 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2238, (moonbit_string_t)moonbit_string_literal_69.data, (moonbit_string_t)moonbit_string_literal_70.data, _M0L6_2atmpS2239);
}

void* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__1C2255l37(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2256,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1015
) {
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2258;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2257;
  void* _block_3172;
  #line 37 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2256);
  #line 40 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2258
  = _M0MP36mulpjs4mulp6stream4File10with__path(_M0L4fileS1015, (moonbit_string_t)moonbit_string_literal_71.data);
  _M0L6_2atmpS2257 = _M0L6_2atmpS2258;
  _block_3172
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3172)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3172)->$0
  = _M0L6_2atmpS2257;
  return _block_3172;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0(
  
) {
  void* _M0L4TextS2226;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5inputS1009;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L6_2atmpS2223;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6renameS1010;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2222;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2221;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2218;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream** _M0L6_2atmpS2220;
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE* _M0L6_2atmpS2219;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2216;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L6_2atmpS2217;
  void* _M0L6_2atmpS2215;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6outputS1012;
  int32_t _M0L6_2atmpS2202;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2203;
  struct _M0TPB4Show _M0L6_2atmpS2195;
  moonbit_string_t _M0L6_2atmpS2198;
  moonbit_string_t _M0L6_2atmpS2199;
  moonbit_string_t _M0L6_2atmpS2200;
  moonbit_string_t _M0L6_2atmpS2201;
  moonbit_string_t* _M0L6_2atmpS2197;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2196;
  struct moonbit_result_0 _tmp_3173;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2214;
  moonbit_string_t _M0L6_2atmpS2213;
  struct _M0TPB4Show _M0L6_2atmpS2206;
  moonbit_string_t _M0L6_2atmpS2209;
  moonbit_string_t _M0L6_2atmpS2210;
  moonbit_string_t _M0L6_2atmpS2211;
  moonbit_string_t _M0L6_2atmpS2212;
  moonbit_string_t* _M0L6_2atmpS2208;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2207;
  #line 11 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L4TextS2226
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text));
  Moonbit_object_header(_M0L4TextS2226)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents4Text, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L4TextS2226)->$0
  = (moonbit_string_t)moonbit_string_literal_18.data;
  #line 12 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L5inputS1009
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L4TextS2226);
  _M0L6_2atmpS2223
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File*)&_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0C2224l18$closure.data;
  #line 18 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6renameS1010 = _M0FP36mulpjs4mulp6stream10map__files(_M0L6_2atmpS2223);
  _M0L6_2atmpS2222
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2222[0] = _M0L5inputS1009;
  _M0L6_2atmpS2221
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2221)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2221->$0 = _M0L6_2atmpS2222;
  _M0L6_2atmpS2221->$1 = 1;
  #line 21 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2218
  = _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2221);
  _M0L6_2atmpS2220
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2220[0] = _M0L6renameS1010;
  _M0L6_2atmpS2219
  = (struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L6_2atmpS2219)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2219->$0 = _M0L6_2atmpS2220;
  _M0L6_2atmpS2219->$1 = 1;
  #line 21 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2216
  = _M0FP36mulpjs4mulp16stream__pipeline8pipeline(_M0L6_2atmpS2218, _M0L6_2atmpS2219);
  #line 22 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2217
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context();
  #line 21 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2215
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS2216, _M0L6_2atmpS2217);
  #line 21 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6outputS1012
  = _M0MPC16result6Result6unwrapGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS2215);
  moonbit_incref(_M0L6outputS1012);
  #line 24 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2202
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1012);
  _M0L14_2aboxed__selfS2203
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2203)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2203->$0 = _M0L6_2atmpS2202;
  _M0L6_2atmpS2195
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2203
  };
  _M0L6_2atmpS2198 = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS2199 = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS2200 = 0;
  _M0L6_2atmpS2201 = 0;
  _M0L6_2atmpS2197 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2197[0] = _M0L6_2atmpS2198;
  _M0L6_2atmpS2197[1] = _M0L6_2atmpS2199;
  _M0L6_2atmpS2197[2] = _M0L6_2atmpS2200;
  _M0L6_2atmpS2197[3] = _M0L6_2atmpS2201;
  _M0L6_2atmpS2196
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2196)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2196->$0 = _M0L6_2atmpS2197;
  _M0L6_2atmpS2196->$1 = 4;
  #line 24 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _tmp_3173
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2195, (moonbit_string_t)moonbit_string_literal_65.data, (moonbit_string_t)moonbit_string_literal_74.data, _M0L6_2atmpS2196);
  if (_tmp_3173.tag) {
    int32_t const _M0L5_2aokS2204 = _tmp_3173.data.ok;
  } else {
    void* const _M0L6_2aerrS2205 = _tmp_3173.data.err;
    struct moonbit_result_0 _result_3174;
    moonbit_decref(_M0L6outputS1012);
    _result_3174.tag = 0;
    _result_3174.data.err = _M0L6_2aerrS2205;
    return _result_3174;
  }
  #line 25 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2214
  = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L6outputS1012, 0);
  #line 25 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2213
  = _M0MP36mulpjs4mulp6stream4File8basename(_M0L6_2atmpS2214);
  _M0L6_2atmpS2206
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2213
  };
  _M0L6_2atmpS2209 = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS2210 = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS2211 = 0;
  _M0L6_2atmpS2212 = 0;
  _M0L6_2atmpS2208 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2208[0] = _M0L6_2atmpS2209;
  _M0L6_2atmpS2208[1] = _M0L6_2atmpS2210;
  _M0L6_2atmpS2208[2] = _M0L6_2atmpS2211;
  _M0L6_2atmpS2208[3] = _M0L6_2atmpS2212;
  _M0L6_2atmpS2207
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2207)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2207->$0 = _M0L6_2atmpS2208;
  _M0L6_2atmpS2207->$1 = 4;
  #line 25 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2206, (moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_77.data, _M0L6_2atmpS2207);
}

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test47____test__706970656c696e655f746573742e6d6274__0C2224l18(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L6_2aenvS2225,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1011
) {
  #line 18 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  moonbit_decref(_M0L6_2aenvS2225);
  #line 19 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0MP36mulpjs4mulp6stream4File10with__path(_M0L4fileS1011, (moonbit_string_t)moonbit_string_literal_61.data);
}

struct _M0TP36mulpjs4mulp4core7Context* _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test23pipeline__test__context(
  
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L6_2atmpS2194;
  #line 2 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  #line 6 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  _M0L6_2atmpS2194 = _M0FP36mulpjs4mulp4core24new__cancellation__token();
  #line 3 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline_test.mbt"
  return _M0FP36mulpjs4mulp4core12new__context((moonbit_string_t)moonbit_string_literal_78.data, 0ll, _M0L6_2atmpS2194);
}

struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through225file__transformer_2einner(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L3mapS987,
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L5flushS994
) {
  struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__* _closure_3175;
  #line 138 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _closure_3175
  = (struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__*)moonbit_malloc(sizeof(struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__));
  Moonbit_object_header(_closure_3175)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__, $0) >> 2, 2, 0);
  _closure_3175->code
  = &_M0FP36mulpjs4mulp8through225file__transformer_2einnerC2175l142;
  _closure_3175->$0 = _M0L5flushS994;
  _closure_3175->$1 = _M0L3mapS987;
  return (struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*)_closure_3175;
}

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through225file__transformer_2einnerC2175l142(
  struct _M0TWEWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2176
) {
  struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__* _M0L14_2acasted__envS2177;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L3mapS987;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2666;
  int32_t _M0L6_2acntS2925;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L5flushS994;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L5flushS980;
  struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__* _closure_3178;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2178;
  #line 142 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2177
  = (struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2175__l142__*)_M0L6_2aenvS2176;
  _M0L3mapS987 = _M0L14_2acasted__envS2177->$1;
  _M0L8_2afieldS2666 = _M0L14_2acasted__envS2177->$0;
  _M0L6_2acntS2925 = Moonbit_object_header(_M0L14_2acasted__envS2177)->rc;
  if (_M0L6_2acntS2925 > 1) {
    int32_t _M0L11_2anew__cntS2926 = _M0L6_2acntS2925 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2177)->rc
    = _M0L11_2anew__cntS2926;
    moonbit_incref(_M0L3mapS987);
    if (_M0L8_2afieldS2666) {
      moonbit_incref(_M0L8_2afieldS2666);
    }
  } else if (_M0L6_2acntS2925 == 1) {
    #line 142 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2177);
  }
  _M0L5flushS994 = _M0L8_2afieldS2666;
  if (_M0L5flushS994 == 0) {
    struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__* _closure_3177;
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2186;
    if (_M0L5flushS994) {
      moonbit_decref(_M0L5flushS994);
    }
    _closure_3177
    = (struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__*)moonbit_malloc(sizeof(struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__));
    Moonbit_object_header(_closure_3177)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__, $0) >> 2, 1, 0);
    _closure_3177->code
    = &_M0FP36mulpjs4mulp8through225file__transformer_2einnerC2187l159;
    _closure_3177->$0 = _M0L3mapS987;
    _M0L6_2atmpS2186
    = (struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*)_closure_3177;
    moonbit_incref(_M0FP36mulpjs4mulp8through224empty__file__flush_2eclo);
    #line 158 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    return _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flush(_M0L6_2atmpS2186, _M0FP36mulpjs4mulp8through224empty__file__flush_2eclo);
  } else {
    struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L7_2aSomeS995 =
      _M0L5flushS994;
    struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2aflushS996 =
      _M0L7_2aSomeS995;
    _M0L5flushS980 = _M0L8_2aflushS996;
    goto join_979;
  }
  join_979:;
  _closure_3178
  = (struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__*)moonbit_malloc(sizeof(struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__));
  Moonbit_object_header(_closure_3178)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__, $0) >> 2, 1, 0);
  _closure_3178->code
  = &_M0FP36mulpjs4mulp8through225file__transformer_2einnerC2179l146;
  _closure_3178->$0 = _M0L3mapS987;
  _M0L6_2atmpS2178
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*)_closure_3178;
  #line 145 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  return _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flush(_M0L6_2atmpS2178, _M0L5flushS980);
}

void* _M0FP36mulpjs4mulp8through225file__transformer_2einnerC2187l159(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2188,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS997
) {
  struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__* _M0L14_2acasted__envS2189;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2670;
  int32_t _M0L6_2acntS2927;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L3mapS987;
  void* _M0L3errS999;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6outputS1001;
  void* _M0L7_2abindS1002;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2191;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2190;
  void* _block_3182;
  void* _block_3183;
  #line 159 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2189
  = (struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2187__l159__*)_M0L6_2aenvS2188;
  _M0L8_2afieldS2670 = _M0L14_2acasted__envS2189->$0;
  _M0L6_2acntS2927 = Moonbit_object_header(_M0L14_2acasted__envS2189)->rc;
  if (_M0L6_2acntS2927 > 1) {
    int32_t _M0L11_2anew__cntS2928 = _M0L6_2acntS2927 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2189)->rc
    = _M0L11_2anew__cntS2928;
    moonbit_incref(_M0L8_2afieldS2670);
  } else if (_M0L6_2acntS2927 == 1) {
    #line 159 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2189);
  }
  _M0L3mapS987 = _M0L8_2afieldS2670;
  #line 162 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L7_2abindS1002 = _M0L3mapS987->code(_M0L3mapS987, _M0L4fileS997);
  switch (Moonbit_object_tag(_M0L7_2abindS1002)) {
    case 1: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1003 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1002;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2668 =
        _M0L5_2aOkS1003->$0;
      int32_t _M0L6_2acntS2929 = Moonbit_object_header(_M0L5_2aOkS1003)->rc;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS1004;
      if (_M0L6_2acntS2929 > 1) {
        int32_t _M0L11_2anew__cntS2930 = _M0L6_2acntS2929 - 1;
        Moonbit_object_header(_M0L5_2aOkS1003)->rc = _M0L11_2anew__cntS2930;
        if (_M0L8_2afieldS2668) {
          moonbit_incref(_M0L8_2afieldS2668);
        }
      } else if (_M0L6_2acntS2929 == 1) {
        #line 162 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L5_2aOkS1003);
      }
      _M0L4_2axS1004 = _M0L8_2afieldS2668;
      if (_M0L4_2axS1004 == 0) {
        struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2193;
        struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2192;
        void* _block_3181;
        if (_M0L4_2axS1004) {
          moonbit_decref(_M0L4_2axS1004);
        }
        _M0L6_2atmpS2193
        = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
        _M0L6_2atmpS2192
        = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
        Moonbit_object_header(_M0L6_2atmpS2192)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
        _M0L6_2atmpS2192->$0 = _M0L6_2atmpS2193;
        _M0L6_2atmpS2192->$1 = 0;
        _block_3181
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
        Moonbit_object_header(_block_3181)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
        ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3181)->$0
        = _M0L6_2atmpS2192;
        return _block_3181;
      } else {
        struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS1005 =
          _M0L4_2axS1004;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L9_2aoutputS1006 =
          _M0L7_2aSomeS1005;
        _M0L6outputS1001 = _M0L9_2aoutputS1006;
        goto join_1000;
      }
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1007 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1002;
      void* _M0L8_2afieldS2669 = _M0L6_2aErrS1007->$0;
      int32_t _M0L6_2acntS2931 = Moonbit_object_header(_M0L6_2aErrS1007)->rc;
      void* _M0L6_2aerrS1008;
      if (_M0L6_2acntS2931 > 1) {
        int32_t _M0L11_2anew__cntS2932 = _M0L6_2acntS2931 - 1;
        Moonbit_object_header(_M0L6_2aErrS1007)->rc = _M0L11_2anew__cntS2932;
        moonbit_incref(_M0L8_2afieldS2669);
      } else if (_M0L6_2acntS2931 == 1) {
        #line 162 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L6_2aErrS1007);
      }
      _M0L6_2aerrS1008 = _M0L8_2afieldS2669;
      _M0L3errS999 = _M0L6_2aerrS1008;
      goto join_998;
      break;
    }
  }
  join_1000:;
  _M0L6_2atmpS2191
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2191[0] = _M0L6outputS1001;
  _M0L6_2atmpS2190
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2190)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2190->$0 = _M0L6_2atmpS2191;
  _M0L6_2atmpS2190->$1 = 1;
  _block_3182
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3182)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3182)->$0
  = _M0L6_2atmpS2190;
  return _block_3182;
  join_998:;
  _block_3183
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
  Moonbit_object_header(_block_3183)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3183)->$0
  = _M0L3errS999;
  return _block_3183;
}

void* _M0FP36mulpjs4mulp8through225file__transformer_2einnerC2179l146(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2180,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS981
) {
  struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__* _M0L14_2acasted__envS2181;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2673;
  int32_t _M0L6_2acntS2933;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L3mapS987;
  void* _M0L3errS983;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6outputS985;
  void* _M0L7_2abindS986;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2183;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2182;
  void* _block_3187;
  void* _block_3188;
  #line 146 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2181
  = (struct _M0R77_24mulpjs_2fmulp_2fthrough2_2efile__transformer_2einner_2eanon__u2179__l146__*)_M0L6_2aenvS2180;
  _M0L8_2afieldS2673 = _M0L14_2acasted__envS2181->$0;
  _M0L6_2acntS2933 = Moonbit_object_header(_M0L14_2acasted__envS2181)->rc;
  if (_M0L6_2acntS2933 > 1) {
    int32_t _M0L11_2anew__cntS2934 = _M0L6_2acntS2933 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2181)->rc
    = _M0L11_2anew__cntS2934;
    moonbit_incref(_M0L8_2afieldS2673);
  } else if (_M0L6_2acntS2933 == 1) {
    #line 146 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2181);
  }
  _M0L3mapS987 = _M0L8_2afieldS2673;
  #line 149 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L7_2abindS986 = _M0L3mapS987->code(_M0L3mapS987, _M0L4fileS981);
  switch (Moonbit_object_tag(_M0L7_2abindS986)) {
    case 1: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS988 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS986;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2671 =
        _M0L5_2aOkS988->$0;
      int32_t _M0L6_2acntS2935 = Moonbit_object_header(_M0L5_2aOkS988)->rc;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS989;
      if (_M0L6_2acntS2935 > 1) {
        int32_t _M0L11_2anew__cntS2936 = _M0L6_2acntS2935 - 1;
        Moonbit_object_header(_M0L5_2aOkS988)->rc = _M0L11_2anew__cntS2936;
        if (_M0L8_2afieldS2671) {
          moonbit_incref(_M0L8_2afieldS2671);
        }
      } else if (_M0L6_2acntS2935 == 1) {
        #line 149 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L5_2aOkS988);
      }
      _M0L4_2axS989 = _M0L8_2afieldS2671;
      if (_M0L4_2axS989 == 0) {
        struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2185;
        struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2184;
        void* _block_3186;
        if (_M0L4_2axS989) {
          moonbit_decref(_M0L4_2axS989);
        }
        _M0L6_2atmpS2185
        = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
        _M0L6_2atmpS2184
        = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
        Moonbit_object_header(_M0L6_2atmpS2184)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
        _M0L6_2atmpS2184->$0 = _M0L6_2atmpS2185;
        _M0L6_2atmpS2184->$1 = 0;
        _block_3186
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
        Moonbit_object_header(_block_3186)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
        ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3186)->$0
        = _M0L6_2atmpS2184;
        return _block_3186;
      } else {
        struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS990 =
          _M0L4_2axS989;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L9_2aoutputS991 =
          _M0L7_2aSomeS990;
        _M0L6outputS985 = _M0L9_2aoutputS991;
        goto join_984;
      }
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS992 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS986;
      void* _M0L8_2afieldS2672 = _M0L6_2aErrS992->$0;
      int32_t _M0L6_2acntS2937 = Moonbit_object_header(_M0L6_2aErrS992)->rc;
      void* _M0L6_2aerrS993;
      if (_M0L6_2acntS2937 > 1) {
        int32_t _M0L11_2anew__cntS2938 = _M0L6_2acntS2937 - 1;
        Moonbit_object_header(_M0L6_2aErrS992)->rc = _M0L11_2anew__cntS2938;
        moonbit_incref(_M0L8_2afieldS2672);
      } else if (_M0L6_2acntS2937 == 1) {
        #line 149 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L6_2aErrS992);
      }
      _M0L6_2aerrS993 = _M0L8_2afieldS2672;
      _M0L3errS983 = _M0L6_2aerrS993;
      goto join_982;
      break;
    }
  }
  join_984:;
  _M0L6_2atmpS2183
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2183[0] = _M0L6outputS985;
  _M0L6_2atmpS2182
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2182)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2182->$0 = _M0L6_2atmpS2183;
  _M0L6_2atmpS2182->$1 = 1;
  _block_3187
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3187)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3187)->$0
  = _M0L6_2atmpS2182;
  return _block_3187;
  join_982:;
  _block_3188
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
  Moonbit_object_header(_block_3188)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3188)->$0
  = _M0L3errS983;
  return _block_3188;
}

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through214through__files(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L3mapS972
) {
  struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__* _closure_3189;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2167;
  #line 125 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _closure_3189
  = (struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__*)moonbit_malloc(sizeof(struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__));
  Moonbit_object_header(_closure_3189)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__, $0) >> 2, 1, 0);
  _closure_3189->code = &_M0FP36mulpjs4mulp8through214through__filesC2168l126;
  _closure_3189->$0 = _M0L3mapS972;
  _M0L6_2atmpS2167
  = (struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE*)_closure_3189;
  #line 126 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  return _M0FP36mulpjs4mulp8through220through__files__flat(_M0L6_2atmpS2167);
}

void* _M0FP36mulpjs4mulp8through214through__filesC2168l126(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2169,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS966
) {
  struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__* _M0L14_2acasted__envS2170;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2676;
  int32_t _M0L6_2acntS2939;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L3mapS972;
  void* _M0L3errS968;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6outputS970;
  void* _M0L7_2abindS971;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2172;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2171;
  void* _block_3193;
  void* _block_3194;
  #line 126 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2170
  = (struct _M0R66_24mulpjs_2fmulp_2fthrough2_2ethrough__files_2eanon__u2168__l126__*)_M0L6_2aenvS2169;
  _M0L8_2afieldS2676 = _M0L14_2acasted__envS2170->$0;
  _M0L6_2acntS2939 = Moonbit_object_header(_M0L14_2acasted__envS2170)->rc;
  if (_M0L6_2acntS2939 > 1) {
    int32_t _M0L11_2anew__cntS2940 = _M0L6_2acntS2939 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2170)->rc
    = _M0L11_2anew__cntS2940;
    moonbit_incref(_M0L8_2afieldS2676);
  } else if (_M0L6_2acntS2939 == 1) {
    #line 126 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2170);
  }
  _M0L3mapS972 = _M0L8_2afieldS2676;
  #line 129 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L7_2abindS971 = _M0L3mapS972->code(_M0L3mapS972, _M0L4fileS966);
  switch (Moonbit_object_tag(_M0L7_2abindS971)) {
    case 1: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS973 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS971;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2674 =
        _M0L5_2aOkS973->$0;
      int32_t _M0L6_2acntS2941 = Moonbit_object_header(_M0L5_2aOkS973)->rc;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS974;
      if (_M0L6_2acntS2941 > 1) {
        int32_t _M0L11_2anew__cntS2942 = _M0L6_2acntS2941 - 1;
        Moonbit_object_header(_M0L5_2aOkS973)->rc = _M0L11_2anew__cntS2942;
        if (_M0L8_2afieldS2674) {
          moonbit_incref(_M0L8_2afieldS2674);
        }
      } else if (_M0L6_2acntS2941 == 1) {
        #line 129 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L5_2aOkS973);
      }
      _M0L4_2axS974 = _M0L8_2afieldS2674;
      if (_M0L4_2axS974 == 0) {
        struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2174;
        struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2173;
        void* _block_3192;
        if (_M0L4_2axS974) {
          moonbit_decref(_M0L4_2axS974);
        }
        _M0L6_2atmpS2174
        = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
        _M0L6_2atmpS2173
        = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
        Moonbit_object_header(_M0L6_2atmpS2173)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
        _M0L6_2atmpS2173->$0 = _M0L6_2atmpS2174;
        _M0L6_2atmpS2173->$1 = 0;
        _block_3192
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
        Moonbit_object_header(_block_3192)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
        ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3192)->$0
        = _M0L6_2atmpS2173;
        return _block_3192;
      } else {
        struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS975 =
          _M0L4_2axS974;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L9_2aoutputS976 =
          _M0L7_2aSomeS975;
        _M0L6outputS970 = _M0L9_2aoutputS976;
        goto join_969;
      }
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS977 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS971;
      void* _M0L8_2afieldS2675 = _M0L6_2aErrS977->$0;
      int32_t _M0L6_2acntS2943 = Moonbit_object_header(_M0L6_2aErrS977)->rc;
      void* _M0L6_2aerrS978;
      if (_M0L6_2acntS2943 > 1) {
        int32_t _M0L11_2anew__cntS2944 = _M0L6_2acntS2943 - 1;
        Moonbit_object_header(_M0L6_2aErrS977)->rc = _M0L11_2anew__cntS2944;
        moonbit_incref(_M0L8_2afieldS2675);
      } else if (_M0L6_2acntS2943 == 1) {
        #line 129 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L6_2aErrS977);
      }
      _M0L6_2aerrS978 = _M0L8_2afieldS2675;
      _M0L3errS968 = _M0L6_2aerrS978;
      goto join_967;
      break;
    }
  }
  join_969:;
  _M0L6_2atmpS2172
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2172[0] = _M0L6outputS970;
  _M0L6_2atmpS2171
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2171)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2171->$0 = _M0L6_2atmpS2172;
  _M0L6_2atmpS2171->$1 = 1;
  _block_3193
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3193)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3193)->$0
  = _M0L6_2atmpS2171;
  return _block_3193;
  join_967:;
  _block_3194
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
  Moonbit_object_header(_block_3194)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3194)->$0
  = _M0L3errS968;
  return _block_3194;
}

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through220through__files__flat(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L9flat__mapS961
) {
  struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__* _closure_3195;
  #line 115 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _closure_3195
  = (struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__*)moonbit_malloc(sizeof(struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__));
  Moonbit_object_header(_closure_3195)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__, $0) >> 2, 1, 0);
  _closure_3195->code
  = &_M0FP36mulpjs4mulp8through220through__files__flatC2164l116;
  _closure_3195->$0 = _M0L9flat__mapS961;
  return (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)_closure_3195;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through220through__files__flatC2164l116(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2165,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS955
) {
  struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__* _M0L14_2acasted__envS2166;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2679;
  int32_t _M0L6_2acntS2945;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L9flat__mapS961;
  void* _M0L3errS957;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS959;
  void* _M0L7_2abindS960;
  #line 116 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2166
  = (struct _M0R72_24mulpjs_2fmulp_2fthrough2_2ethrough__files__flat_2eanon__u2164__l116__*)_M0L6_2aenvS2165;
  _M0L8_2afieldS2679 = _M0L14_2acasted__envS2166->$0;
  _M0L6_2acntS2945 = Moonbit_object_header(_M0L14_2acasted__envS2166)->rc;
  if (_M0L6_2acntS2945 > 1) {
    int32_t _M0L11_2anew__cntS2946 = _M0L6_2acntS2945 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2166)->rc
    = _M0L11_2anew__cntS2946;
    moonbit_incref(_M0L8_2afieldS2679);
  } else if (_M0L6_2acntS2945 == 1) {
    #line 116 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2166);
  }
  _M0L9flat__mapS961 = _M0L8_2afieldS2679;
  #line 117 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L7_2abindS960
  = _M0L9flat__mapS961->code(_M0L9flat__mapS961, _M0L4fileS955);
  switch (Moonbit_object_tag(_M0L7_2abindS960)) {
    case 1: {
      struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS962 =
        (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS960;
      struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afieldS2677 =
        _M0L5_2aOkS962->$0;
      int32_t _M0L6_2acntS2947 = Moonbit_object_header(_M0L5_2aOkS962)->rc;
      struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afilesS963;
      if (_M0L6_2acntS2947 > 1) {
        int32_t _M0L11_2anew__cntS2948 = _M0L6_2acntS2947 - 1;
        Moonbit_object_header(_M0L5_2aOkS962)->rc = _M0L11_2anew__cntS2948;
        moonbit_incref(_M0L8_2afieldS2677);
      } else if (_M0L6_2acntS2947 == 1) {
        #line 117 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L5_2aOkS962);
      }
      _M0L8_2afilesS963 = _M0L8_2afieldS2677;
      _M0L5filesS959 = _M0L8_2afilesS963;
      goto join_958;
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS964 =
        (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS960;
      void* _M0L8_2afieldS2678 = _M0L6_2aErrS964->$0;
      int32_t _M0L6_2acntS2949 = Moonbit_object_header(_M0L6_2aErrS964)->rc;
      void* _M0L6_2aerrS965;
      if (_M0L6_2acntS2949 > 1) {
        int32_t _M0L11_2anew__cntS2950 = _M0L6_2acntS2949 - 1;
        Moonbit_object_header(_M0L6_2aErrS964)->rc = _M0L11_2anew__cntS2950;
        moonbit_incref(_M0L8_2afieldS2678);
      } else if (_M0L6_2acntS2949 == 1) {
        #line 117 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L6_2aErrS964);
      }
      _M0L6_2aerrS965 = _M0L8_2afieldS2678;
      _M0L3errS957 = _M0L6_2aerrS965;
      goto join_956;
      break;
    }
  }
  join_958:;
  #line 118 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  return _M0FP36mulpjs4mulp6stream12file__stream(_M0L5filesS959);
  join_956:;
  #line 119 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  return _M0FP36mulpjs4mulp6stream13error__stream(_M0L3errS957);
}

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flush(
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L9flat__mapS916,
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L5flushS917
) {
  struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__* _closure_3198;
  #line 60 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _closure_3198
  = (struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__*)moonbit_malloc(sizeof(struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__));
  Moonbit_object_header(_closure_3198)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__, $0) >> 2, 2, 0);
  _closure_3198->code
  = &_M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2145l64;
  _closure_3198->$0 = _M0L5flushS917;
  _closure_3198->$1 = _M0L9flat__mapS916;
  return (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*)_closure_3198;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2145l64(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2146,
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS914
) {
  struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__* _M0L14_2acasted__envS2147;
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L9flat__mapS916;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2680;
  int32_t _M0L6_2acntS2951;
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L5flushS917;
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0L5stateS915;
  struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__* _closure_3199;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2148;
  struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__* _closure_3200;
  struct _M0TWEu* _M0L6_2atmpS2149;
  #line 64 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2147
  = (struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2145__l64__*)_M0L6_2aenvS2146;
  _M0L9flat__mapS916 = _M0L14_2acasted__envS2147->$1;
  _M0L8_2afieldS2680 = _M0L14_2acasted__envS2147->$0;
  _M0L6_2acntS2951 = Moonbit_object_header(_M0L14_2acasted__envS2147)->rc;
  if (_M0L6_2acntS2951 > 1) {
    int32_t _M0L11_2anew__cntS2952 = _M0L6_2acntS2951 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2147)->rc
    = _M0L11_2anew__cntS2952;
    moonbit_incref(_M0L9flat__mapS916);
    moonbit_incref(_M0L8_2afieldS2680);
  } else if (_M0L6_2acntS2951 == 1) {
    #line 64 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2147);
  }
  _M0L5flushS917 = _M0L8_2afieldS2680;
  #line 65 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L5stateS915
  = _M0FP36mulpjs4mulp8through211file__state(_M0L8upstreamS914, _M0L9flat__mapS916, _M0L5flushS917);
  moonbit_incref(_M0L5stateS915);
  _closure_3199
  = (struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__*)moonbit_malloc(sizeof(struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__));
  Moonbit_object_header(_closure_3199)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__, $0) >> 2, 1, 0);
  _closure_3199->code
  = &_M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2156l67;
  _closure_3199->$0 = _M0L5stateS915;
  _M0L6_2atmpS2148
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_3199;
  _closure_3200
  = (struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__*)moonbit_malloc(sizeof(struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__));
  Moonbit_object_header(_closure_3200)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__, $0) >> 2, 1, 0);
  _closure_3200->code
  = &_M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2150l106;
  _closure_3200->$0 = _M0L5stateS915;
  _M0L6_2atmpS2149 = (struct _M0TWEu*)_closure_3200;
  #line 66 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  return _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L6_2atmpS2148, _M0L6_2atmpS2149);
}

void* _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2156l67(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2157
) {
  struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__* _M0L14_2acasted__envS2158;
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0L8_2afieldS2694;
  int32_t _M0L6_2acntS2953;
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0L5stateS915;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2163;
  void* _block_3214;
  #line 67 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2158
  = (struct _M0R91_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2156__l67__*)_M0L6_2aenvS2157;
  _M0L8_2afieldS2694 = _M0L14_2acasted__envS2158->$0;
  _M0L6_2acntS2953 = Moonbit_object_header(_M0L14_2acasted__envS2158)->rc;
  if (_M0L6_2acntS2953 > 1) {
    int32_t _M0L11_2anew__cntS2954 = _M0L6_2acntS2953 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2158)->rc
    = _M0L11_2anew__cntS2954;
    moonbit_incref(_M0L8_2afieldS2694);
  } else if (_M0L6_2acntS2953 == 1) {
    #line 67 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2158);
  }
  _M0L5stateS915 = _M0L8_2afieldS2694;
  while (1) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS919;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2abindS920;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2159;
    void* _block_3203;
    moonbit_incref(_M0L5stateS915);
    #line 69 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    _M0L7_2abindS920
    = _M0MP36mulpjs4mulp8through216FileThroughState13next__pending(_M0L5stateS915);
    if (_M0L7_2abindS920 == 0) {
      if (_M0L7_2abindS920) {
        moonbit_decref(_M0L7_2abindS920);
      }
    } else {
      struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS921;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS922;
      moonbit_decref(_M0L5stateS915);
      _M0L7_2aSomeS921 = _M0L7_2abindS920;
      _M0L7_2afileS922 = _M0L7_2aSomeS921;
      _M0L4fileS919 = _M0L7_2afileS922;
      goto join_918;
    }
    goto joinlet_3202;
    join_918:;
    _M0L6_2atmpS2159 = _M0L4fileS919;
    _block_3203
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3203)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3203)->$0
    = _M0L6_2atmpS2159;
    return _block_3203;
    joinlet_3202:;
    if (_M0L5stateS915->$5) {
      if (_M0L5stateS915->$6) {
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2160;
        void* _block_3204;
        moonbit_decref(_M0L5stateS915);
        _M0L6_2atmpS2160 = 0;
        _block_3204
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
        Moonbit_object_header(_block_3204)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
        ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3204)->$0
        = _M0L6_2atmpS2160;
        return _block_3204;
      } else {
        void* _M0L3errS924;
        struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS926;
        struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L7_2afuncS928 =
          _M0L5stateS915->$2;
        void* _M0L7_2abindS927;
        struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2aoldS2682;
        void* _block_3207;
        moonbit_incref(_M0L7_2afuncS928);
        #line 77 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        _M0L7_2abindS927 = _M0L7_2afuncS928->code(_M0L7_2afuncS928);
        switch (Moonbit_object_tag(_M0L7_2abindS927)) {
          case 1: {
            struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS929 =
              (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS927;
            struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afieldS2683 =
              _M0L5_2aOkS929->$0;
            int32_t _M0L6_2acntS2955 =
              Moonbit_object_header(_M0L5_2aOkS929)->rc;
            struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afilesS930;
            if (_M0L6_2acntS2955 > 1) {
              int32_t _M0L11_2anew__cntS2956 = _M0L6_2acntS2955 - 1;
              Moonbit_object_header(_M0L5_2aOkS929)->rc
              = _M0L11_2anew__cntS2956;
              moonbit_incref(_M0L8_2afieldS2683);
            } else if (_M0L6_2acntS2955 == 1) {
              #line 77 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
              moonbit_free(_M0L5_2aOkS929);
            }
            _M0L8_2afilesS930 = _M0L8_2afieldS2683;
            _M0L5filesS926 = _M0L8_2afilesS930;
            goto join_925;
            break;
          }
          default: {
            struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS931;
            void* _M0L8_2afieldS2684;
            int32_t _M0L6_2acntS2957;
            void* _M0L6_2aerrS932;
            moonbit_decref(_M0L5stateS915);
            _M0L6_2aErrS931
            = (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS927;
            _M0L8_2afieldS2684 = _M0L6_2aErrS931->$0;
            _M0L6_2acntS2957 = Moonbit_object_header(_M0L6_2aErrS931)->rc;
            if (_M0L6_2acntS2957 > 1) {
              int32_t _M0L11_2anew__cntS2958 = _M0L6_2acntS2957 - 1;
              Moonbit_object_header(_M0L6_2aErrS931)->rc
              = _M0L11_2anew__cntS2958;
              moonbit_incref(_M0L8_2afieldS2684);
            } else if (_M0L6_2acntS2957 == 1) {
              #line 77 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
              moonbit_free(_M0L6_2aErrS931);
            }
            _M0L6_2aerrS932 = _M0L8_2afieldS2684;
            _M0L3errS924 = _M0L6_2aerrS932;
            goto join_923;
            break;
          }
        }
        goto joinlet_3206;
        join_925:;
        _M0L6_2aoldS2682 = _M0L5stateS915->$3;
        moonbit_decref(_M0L6_2aoldS2682);
        _M0L5stateS915->$3 = _M0L5filesS926;
        _M0L5stateS915->$4 = 0;
        _M0L5stateS915->$6 = 1;
        joinlet_3206:;
        goto joinlet_3205;
        join_923:;
        _block_3207
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
        Moonbit_object_header(_block_3207)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
        ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3207)->$0
        = _M0L3errS924;
        return _block_3207;
        joinlet_3205:;
      }
    } else {
      void* _M0L3errS934;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS936;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS2162 =
        _M0L5stateS915->$0;
      void* _M0L7_2abindS947;
      void* _M0L3errS938;
      struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS940;
      struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L7_2afuncS942;
      void* _M0L7_2abindS941;
      struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2aoldS2687;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS2686;
      int32_t _M0L6_2acntS2959;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS2161;
      void* _block_3212;
      void* _block_3213;
      moonbit_incref(_M0L8upstreamS2162);
      #line 87 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
      _M0L7_2abindS947
      = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L8upstreamS2162);
      switch (Moonbit_object_tag(_M0L7_2abindS947)) {
        case 1: {
          struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS948 =
            (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS947;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2691 =
            _M0L5_2aOkS948->$0;
          int32_t _M0L6_2acntS2968 =
            Moonbit_object_header(_M0L5_2aOkS948)->rc;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS949;
          if (_M0L6_2acntS2968 > 1) {
            int32_t _M0L11_2anew__cntS2969 = _M0L6_2acntS2968 - 1;
            Moonbit_object_header(_M0L5_2aOkS948)->rc
            = _M0L11_2anew__cntS2969;
            if (_M0L8_2afieldS2691) {
              moonbit_incref(_M0L8_2afieldS2691);
            }
          } else if (_M0L6_2acntS2968 == 1) {
            #line 87 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
            moonbit_free(_M0L5_2aOkS948);
          }
          _M0L4_2axS949 = _M0L8_2afieldS2691;
          if (_M0L4_2axS949 == 0) {
            if (_M0L4_2axS949) {
              moonbit_decref(_M0L4_2axS949);
            }
            _M0L5stateS915->$5 = 1;
          } else {
            struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS950 =
              _M0L4_2axS949;
            struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS951 =
              _M0L7_2aSomeS950;
            _M0L4fileS936 = _M0L7_2afileS951;
            goto join_935;
          }
          break;
        }
        default: {
          struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS952;
          void* _M0L8_2afieldS2692;
          int32_t _M0L6_2acntS2970;
          void* _M0L6_2aerrS953;
          moonbit_decref(_M0L5stateS915);
          _M0L6_2aErrS952
          = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS947;
          _M0L8_2afieldS2692 = _M0L6_2aErrS952->$0;
          _M0L6_2acntS2970 = Moonbit_object_header(_M0L6_2aErrS952)->rc;
          if (_M0L6_2acntS2970 > 1) {
            int32_t _M0L11_2anew__cntS2971 = _M0L6_2acntS2970 - 1;
            Moonbit_object_header(_M0L6_2aErrS952)->rc
            = _M0L11_2anew__cntS2971;
            moonbit_incref(_M0L8_2afieldS2692);
          } else if (_M0L6_2acntS2970 == 1) {
            #line 87 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
            moonbit_free(_M0L6_2aErrS952);
          }
          _M0L6_2aerrS953 = _M0L8_2afieldS2692;
          _M0L3errS934 = _M0L6_2aerrS953;
          goto join_933;
          break;
        }
      }
      goto joinlet_3209;
      join_935:;
      _M0L7_2afuncS942 = _M0L5stateS915->$1;
      moonbit_incref(_M0L7_2afuncS942);
      #line 89 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
      _M0L7_2abindS941
      = _M0L7_2afuncS942->code(_M0L7_2afuncS942, _M0L4fileS936);
      switch (Moonbit_object_tag(_M0L7_2abindS941)) {
        case 1: {
          struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS943 =
            (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS941;
          struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afieldS2688 =
            _M0L5_2aOkS943->$0;
          int32_t _M0L6_2acntS2964 =
            Moonbit_object_header(_M0L5_2aOkS943)->rc;
          struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afilesS944;
          if (_M0L6_2acntS2964 > 1) {
            int32_t _M0L11_2anew__cntS2965 = _M0L6_2acntS2964 - 1;
            Moonbit_object_header(_M0L5_2aOkS943)->rc
            = _M0L11_2anew__cntS2965;
            moonbit_incref(_M0L8_2afieldS2688);
          } else if (_M0L6_2acntS2964 == 1) {
            #line 89 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
            moonbit_free(_M0L5_2aOkS943);
          }
          _M0L8_2afilesS944 = _M0L8_2afieldS2688;
          _M0L5filesS940 = _M0L8_2afilesS944;
          goto join_939;
          break;
        }
        default: {
          struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS945 =
            (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS941;
          void* _M0L8_2afieldS2689 = _M0L6_2aErrS945->$0;
          int32_t _M0L6_2acntS2966 =
            Moonbit_object_header(_M0L6_2aErrS945)->rc;
          void* _M0L6_2aerrS946;
          if (_M0L6_2acntS2966 > 1) {
            int32_t _M0L11_2anew__cntS2967 = _M0L6_2acntS2966 - 1;
            Moonbit_object_header(_M0L6_2aErrS945)->rc
            = _M0L11_2anew__cntS2967;
            moonbit_incref(_M0L8_2afieldS2689);
          } else if (_M0L6_2acntS2966 == 1) {
            #line 89 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
            moonbit_free(_M0L6_2aErrS945);
          }
          _M0L6_2aerrS946 = _M0L8_2afieldS2689;
          _M0L3errS938 = _M0L6_2aerrS946;
          goto join_937;
          break;
        }
      }
      goto joinlet_3211;
      join_939:;
      _M0L6_2aoldS2687 = _M0L5stateS915->$3;
      moonbit_decref(_M0L6_2aoldS2687);
      _M0L5stateS915->$3 = _M0L5filesS940;
      _M0L5stateS915->$4 = 0;
      joinlet_3211:;
      goto joinlet_3210;
      join_937:;
      _M0L8_2afieldS2686 = _M0L5stateS915->$0;
      _M0L6_2acntS2959 = Moonbit_object_header(_M0L5stateS915)->rc;
      if (_M0L6_2acntS2959 > 1) {
        int32_t _M0L11_2anew__cntS2963 = _M0L6_2acntS2959 - 1;
        Moonbit_object_header(_M0L5stateS915)->rc = _M0L11_2anew__cntS2963;
        moonbit_incref(_M0L8_2afieldS2686);
      } else if (_M0L6_2acntS2959 == 1) {
        struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afieldS2962 =
          _M0L5stateS915->$3;
        struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2961;
        struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2960;
        moonbit_decref(_M0L8_2afieldS2962);
        _M0L8_2afieldS2961 = _M0L5stateS915->$2;
        moonbit_decref(_M0L8_2afieldS2961);
        _M0L8_2afieldS2960 = _M0L5stateS915->$1;
        moonbit_decref(_M0L8_2afieldS2960);
        #line 95 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
        moonbit_free(_M0L5stateS915);
      }
      _M0L8upstreamS2161 = _M0L8_2afieldS2686;
      #line 95 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
      _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L8upstreamS2161);
      _block_3212
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_3212)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3212)->$0
      = _M0L3errS938;
      return _block_3212;
      joinlet_3210:;
      joinlet_3209:;
      goto joinlet_3208;
      join_933:;
      _block_3213
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_3213)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3213)->$0
      = _M0L3errS934;
      return _block_3213;
      joinlet_3208:;
    }
    continue;
    break;
  }
  _M0L6_2atmpS2163 = 0;
  _block_3214
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3214)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3214)->$0
  = _M0L6_2atmpS2163;
  return _block_3214;
}

int32_t _M0FP36mulpjs4mulp8through240through__file__stream__flat__with__flushC2150l106(
  struct _M0TWEu* _M0L6_2aenvS2151
) {
  struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__* _M0L14_2acasted__envS2152;
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0L8_2afieldS2697;
  int32_t _M0L6_2acntS2972;
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0L5stateS915;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2154;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2153;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2aoldS2696;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS2695;
  int32_t _M0L6_2acntS2974;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS2155;
  #line 106 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14_2acasted__envS2152
  = (struct _M0R92_24mulpjs_2fmulp_2fthrough2_2ethrough__file__stream__flat__with__flush_2eanon__u2150__l106__*)_M0L6_2aenvS2151;
  _M0L8_2afieldS2697 = _M0L14_2acasted__envS2152->$0;
  _M0L6_2acntS2972 = Moonbit_object_header(_M0L14_2acasted__envS2152)->rc;
  if (_M0L6_2acntS2972 > 1) {
    int32_t _M0L11_2anew__cntS2973 = _M0L6_2acntS2972 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2152)->rc
    = _M0L11_2anew__cntS2973;
    moonbit_incref(_M0L8_2afieldS2697);
  } else if (_M0L6_2acntS2972 == 1) {
    #line 106 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L14_2acasted__envS2152);
  }
  _M0L5stateS915 = _M0L8_2afieldS2697;
  _M0L6_2atmpS2154
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
  _M0L6_2atmpS2153
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2153)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2153->$0 = _M0L6_2atmpS2154;
  _M0L6_2atmpS2153->$1 = 0;
  _M0L6_2aoldS2696 = _M0L5stateS915->$3;
  moonbit_decref(_M0L6_2aoldS2696);
  _M0L5stateS915->$3 = _M0L6_2atmpS2153;
  _M0L8_2afieldS2695 = _M0L5stateS915->$0;
  _M0L6_2acntS2974 = Moonbit_object_header(_M0L5stateS915)->rc;
  if (_M0L6_2acntS2974 > 1) {
    int32_t _M0L11_2anew__cntS2978 = _M0L6_2acntS2974 - 1;
    Moonbit_object_header(_M0L5stateS915)->rc = _M0L11_2anew__cntS2978;
    moonbit_incref(_M0L8_2afieldS2695);
  } else if (_M0L6_2acntS2974 == 1) {
    struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afieldS2977 =
      _M0L5stateS915->$3;
    struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2976;
    struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS2975;
    moonbit_decref(_M0L8_2afieldS2977);
    _M0L8_2afieldS2976 = _M0L5stateS915->$2;
    moonbit_decref(_M0L8_2afieldS2976);
    _M0L8_2afieldS2975 = _M0L5stateS915->$1;
    moonbit_decref(_M0L8_2afieldS2975);
    #line 108 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    moonbit_free(_M0L5stateS915);
  }
  _M0L8upstreamS2155 = _M0L8_2afieldS2695;
  #line 108 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L8upstreamS2155);
  return 0;
}

struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0FP36mulpjs4mulp8through211file__state(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS911,
  struct _M0TWRP36mulpjs4mulp6stream4FileERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L9flat__mapS912,
  struct _M0TWERPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE* _M0L5flushS913
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2144;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2143;
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _block_3215;
  #line 43 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L6_2atmpS2144
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
  _M0L6_2atmpS2143
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2143)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2143->$0 = _M0L6_2atmpS2144;
  _M0L6_2atmpS2143->$1 = 0;
  _block_3215
  = (struct _M0TP36mulpjs4mulp8through216FileThroughState*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp8through216FileThroughState));
  Moonbit_object_header(_block_3215)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp8through216FileThroughState, $0) >> 2, 4, 0);
  _block_3215->$0 = _M0L8upstreamS911;
  _block_3215->$1 = _M0L9flat__mapS912;
  _block_3215->$2 = _M0L5flushS913;
  _block_3215->$3 = _M0L6_2atmpS2143;
  _block_3215->$4 = 0;
  _block_3215->$5 = 0;
  _block_3215->$6 = 0;
  return _block_3215;
}

struct _M0TP36mulpjs4mulp6stream4File* _M0MP36mulpjs4mulp8through216FileThroughState13next__pending(
  struct _M0TP36mulpjs4mulp8through216FileThroughState* _M0L4selfS909
) {
  int32_t _M0L14pending__indexS2134;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L7pendingS2136;
  int32_t _M0L6_2atmpS2135;
  #line 30 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L14pending__indexS2134 = _M0L4selfS909->$4;
  _M0L7pendingS2136 = _M0L4selfS909->$3;
  moonbit_incref(_M0L7pendingS2136);
  #line 31 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L6_2atmpS2135
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L7pendingS2136);
  if (_M0L14pending__indexS2134 < _M0L6_2atmpS2135) {
    struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L7pendingS2139 =
      _M0L4selfS909->$3;
    int32_t _M0L14pending__indexS2140 = _M0L4selfS909->$4;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS910;
    int32_t _M0L14pending__indexS2138;
    int32_t _M0L6_2atmpS2137;
    moonbit_incref(_M0L7pendingS2139);
    #line 32 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
    _M0L4fileS910
    = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L7pendingS2139, _M0L14pending__indexS2140);
    _M0L14pending__indexS2138 = _M0L4selfS909->$4;
    _M0L6_2atmpS2137 = _M0L14pending__indexS2138 + 1;
    _M0L4selfS909->$4 = _M0L6_2atmpS2137;
    moonbit_decref(_M0L4selfS909);
    return _M0L4fileS910;
  } else {
    struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2142 =
      (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2141 =
      (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
    struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2aoldS2699;
    Moonbit_object_header(_M0L6_2atmpS2141)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2141->$0 = _M0L6_2atmpS2142;
    _M0L6_2atmpS2141->$1 = 0;
    _M0L6_2aoldS2699 = _M0L4selfS909->$3;
    moonbit_decref(_M0L6_2aoldS2699);
    _M0L4selfS909->$3 = _M0L6_2atmpS2141;
    _M0L4selfS909->$4 = 0;
    moonbit_decref(_M0L4selfS909);
    return 0;
  }
}

void* _M0FP36mulpjs4mulp8through218empty__file__flush() {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2133;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2132;
  void* _block_3216;
  #line 25 "/Users/user/workspace/github/gulp/mulp/through2/file.mbt"
  _M0L6_2atmpS2133
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
  _M0L6_2atmpS2132
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2132)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2132->$0 = _M0L6_2atmpS2133;
  _M0L6_2atmpS2132->$1 = 0;
  _block_3216
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3216)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3216)->$0
  = _M0L6_2atmpS2132;
  return _block_3216;
}

void* _M0FP36mulpjs4mulp16stream__pipeline4lead(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6sourceS905,
  struct _M0TP36mulpjs4mulp4core7Context* _M0L3ctxS906
) {
  void* _M0L3errS903;
  void* _M0L7_2abindS904;
  void* _block_3219;
  #line 33 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  #line 37 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  _M0L7_2abindS904
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6sourceS905, _M0L3ctxS906);
  switch (Moonbit_object_tag(_M0L7_2abindS904)) {
    case 1: {
      int32_t _M0L6_2atmpS2131;
      void* _block_3218;
      moonbit_decref(_M0L7_2abindS904);
      _M0L6_2atmpS2131 = 0;
      _block_3218
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
      Moonbit_object_header(_block_3218)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
      ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3218)->$0
      = _M0L6_2atmpS2131;
      return _block_3218;
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS907 =
        (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS904;
      void* _M0L8_2afieldS2701 = _M0L6_2aErrS907->$0;
      int32_t _M0L6_2acntS2979 = Moonbit_object_header(_M0L6_2aErrS907)->rc;
      void* _M0L6_2aerrS908;
      if (_M0L6_2acntS2979 > 1) {
        int32_t _M0L11_2anew__cntS2980 = _M0L6_2acntS2979 - 1;
        Moonbit_object_header(_M0L6_2aErrS907)->rc = _M0L11_2anew__cntS2980;
        moonbit_incref(_M0L8_2afieldS2701);
      } else if (_M0L6_2acntS2979 == 1) {
        #line 37 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
        moonbit_free(_M0L6_2aErrS907);
      }
      _M0L6_2aerrS908 = _M0L8_2afieldS2701;
      _M0L3errS903 = _M0L6_2aerrS908;
      goto join_902;
      break;
    }
  }
  join_902:;
  _block_3219
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
  Moonbit_object_header(_block_3219)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3219)->$0
  = _M0L3errS903;
  return _block_3219;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline7compose(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6sourceS896,
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStreamE* _M0L10transformsS898
) {
  struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE* _M0L7currentS895;
  int32_t _M0L7_2abindS897;
  int32_t _M0L2__S899;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS2702;
  int32_t _M0L6_2acntS2981;
  #line 21 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  _M0L7currentS895
  = (struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L7currentS895)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L7currentS895->$0 = _M0L6sourceS896;
  _M0L7_2abindS897 = _M0L10transformsS898->$1;
  _M0L2__S899 = 0;
  while (1) {
    if (_M0L2__S899 < _M0L7_2abindS897) {
      struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream** _M0L3bufS2130 =
        _M0L10transformsS898->$0;
      struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L9transformS900 =
        (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*)_M0L3bufS2130[
          _M0L2__S899
        ];
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L3valS2128 =
        _M0L7currentS895->$0;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2127;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2aoldS2703;
      int32_t _M0L6_2atmpS2129;
      moonbit_incref(_M0L3valS2128);
      moonbit_incref(_M0L9transformS900);
      #line 27 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
      _M0L6_2atmpS2127
      = _M0MP36mulpjs4mulp6stream10FileStream12pipe__stream(_M0L3valS2128, _M0L9transformS900);
      _M0L6_2aoldS2703 = _M0L7currentS895->$0;
      moonbit_decref(_M0L6_2aoldS2703);
      _M0L7currentS895->$0 = _M0L6_2atmpS2127;
      _M0L6_2atmpS2129 = _M0L2__S899 + 1;
      _M0L2__S899 = _M0L6_2atmpS2129;
      continue;
    } else {
      moonbit_decref(_M0L10transformsS898);
    }
    break;
  }
  _M0L8_2afieldS2702 = _M0L7currentS895->$0;
  _M0L6_2acntS2981 = Moonbit_object_header(_M0L7currentS895)->rc;
  if (_M0L6_2acntS2981 > 1) {
    int32_t _M0L11_2anew__cntS2982 = _M0L6_2acntS2981 - 1;
    Moonbit_object_header(_M0L7currentS895)->rc = _M0L11_2anew__cntS2982;
    moonbit_incref(_M0L8_2afieldS2702);
  } else if (_M0L6_2acntS2981 == 1) {
    #line 25 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
    moonbit_free(_M0L7currentS895);
  }
  return _M0L8_2afieldS2702;
}

struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline11to__through(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L9transformS894
) {
  struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__* _closure_3221;
  #line 14 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  _closure_3221
  = (struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__*)moonbit_malloc(sizeof(struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__));
  Moonbit_object_header(_closure_3221)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__, $0) >> 2, 1, 0);
  _closure_3221->code
  = &_M0FP36mulpjs4mulp16stream__pipeline11to__throughC2124l15;
  _closure_3221->$0 = _M0L9transformS894;
  return (struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream*)_closure_3221;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline11to__throughC2124l15(
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2125,
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6sourceS893
) {
  struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__* _M0L14_2acasted__envS2126;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS2707;
  int32_t _M0L6_2acntS2983;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L9transformS894;
  #line 15 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  _M0L14_2acasted__envS2126
  = (struct _M0R70_24mulpjs_2fmulp_2fstream__pipeline_2eto__through_2eanon__u2124__l15__*)_M0L6_2aenvS2125;
  _M0L8_2afieldS2707 = _M0L14_2acasted__envS2126->$0;
  _M0L6_2acntS2983 = Moonbit_object_header(_M0L14_2acasted__envS2126)->rc;
  if (_M0L6_2acntS2983 > 1) {
    int32_t _M0L11_2anew__cntS2984 = _M0L6_2acntS2983 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2126)->rc
    = _M0L11_2anew__cntS2984;
    moonbit_incref(_M0L8_2afieldS2707);
  } else if (_M0L6_2acntS2983 == 1) {
    #line 15 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
    moonbit_free(_M0L14_2acasted__envS2126);
  }
  _M0L9transformS894 = _M0L8_2afieldS2707;
  #line 16 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  return _M0MP36mulpjs4mulp6stream10FileStream4pipe(_M0L6sourceS893, _M0L9transformS894);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp16stream__pipeline8pipeline(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6sourceS887,
  struct _M0TPB5ArrayGWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStreamE* _M0L10transformsS889
) {
  struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE* _M0L7currentS886;
  int32_t _M0L7_2abindS888;
  int32_t _M0L2__S890;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS2708;
  int32_t _M0L6_2acntS2985;
  #line 2 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
  _M0L7currentS886
  = (struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE));
  Moonbit_object_header(_M0L7currentS886)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB8MutLocalGRP36mulpjs4mulp6stream10FileStreamE, $0) >> 2, 1, 0);
  _M0L7currentS886->$0 = _M0L6sourceS887;
  _M0L7_2abindS888 = _M0L10transformsS889->$1;
  _M0L2__S890 = 0;
  while (1) {
    if (_M0L2__S890 < _M0L7_2abindS888) {
      struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream** _M0L3bufS2123 =
        _M0L10transformsS889->$0;
      struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L9transformS891 =
        (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)_M0L3bufS2123[
          _M0L2__S890
        ];
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L3valS2121 =
        _M0L7currentS886->$0;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2120;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2aoldS2709;
      int32_t _M0L6_2atmpS2122;
      moonbit_incref(_M0L3valS2121);
      moonbit_incref(_M0L9transformS891);
      #line 8 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
      _M0L6_2atmpS2120
      = _M0MP36mulpjs4mulp6stream10FileStream4pipe(_M0L3valS2121, _M0L9transformS891);
      _M0L6_2aoldS2709 = _M0L7currentS886->$0;
      moonbit_decref(_M0L6_2aoldS2709);
      _M0L7currentS886->$0 = _M0L6_2atmpS2120;
      _M0L6_2atmpS2122 = _M0L2__S890 + 1;
      _M0L2__S890 = _M0L6_2atmpS2122;
      continue;
    } else {
      moonbit_decref(_M0L10transformsS889);
    }
    break;
  }
  _M0L8_2afieldS2708 = _M0L7currentS886->$0;
  _M0L6_2acntS2985 = Moonbit_object_header(_M0L7currentS886)->rc;
  if (_M0L6_2acntS2985 > 1) {
    int32_t _M0L11_2anew__cntS2986 = _M0L6_2acntS2985 - 1;
    Moonbit_object_header(_M0L7currentS886)->rc = _M0L11_2anew__cntS2986;
    moonbit_incref(_M0L8_2afieldS2708);
  } else if (_M0L6_2acntS2985 == 1) {
    #line 6 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/pipeline.mbt"
    moonbit_free(_M0L7currentS886);
  }
  return _M0L8_2afieldS2708;
}

moonbit_string_t _M0MP36mulpjs4mulp6stream4File8basename(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4selfS882
) {
  int32_t _M0L5indexS881;
  moonbit_string_t _M0L4pathS2119;
  int64_t _M0L7_2abindS883;
  moonbit_string_t _M0L8_2afieldS2713;
  int32_t _M0L6_2acntS2987;
  moonbit_string_t _M0L4pathS2117;
  int32_t _M0L6_2atmpS2118;
  struct _M0TPC16string10StringView _M0L6_2atmpS2116;
  #line 100 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L4pathS2119 = _M0L4selfS882->$2;
  moonbit_incref(_M0L4pathS2119);
  #line 101 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L7_2abindS883
  = _M0FP36mulpjs4mulp6stream15last__index__of(_M0L4pathS2119, (moonbit_string_t)moonbit_string_literal_79.data);
  if (_M0L7_2abindS883 == 4294967296ll) {
    moonbit_string_t _M0L8_2afieldS2714 = _M0L4selfS882->$2;
    int32_t _M0L6_2acntS2994 = Moonbit_object_header(_M0L4selfS882)->rc;
    if (_M0L6_2acntS2994 > 1) {
      int32_t _M0L11_2anew__cntS3000 = _M0L6_2acntS2994 - 1;
      Moonbit_object_header(_M0L4selfS882)->rc = _M0L11_2anew__cntS3000;
      moonbit_incref(_M0L8_2afieldS2714);
    } else if (_M0L6_2acntS2994 == 1) {
      moonbit_string_t _M0L8_2afieldS2999 = _M0L4selfS882->$5;
      struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS2998;
      void* _M0L8_2afieldS2997;
      moonbit_string_t _M0L8_2afieldS2996;
      moonbit_string_t _M0L8_2afieldS2995;
      if (_M0L8_2afieldS2999) {
        moonbit_decref(_M0L8_2afieldS2999);
      }
      _M0L8_2afieldS2998 = _M0L4selfS882->$4;
      moonbit_decref(_M0L8_2afieldS2998);
      _M0L8_2afieldS2997 = _M0L4selfS882->$3;
      moonbit_decref(_M0L8_2afieldS2997);
      _M0L8_2afieldS2996 = _M0L4selfS882->$1;
      moonbit_decref(_M0L8_2afieldS2996);
      _M0L8_2afieldS2995 = _M0L4selfS882->$0;
      moonbit_decref(_M0L8_2afieldS2995);
      #line 103 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
      moonbit_free(_M0L4selfS882);
    }
    return _M0L8_2afieldS2714;
  } else {
    int64_t _M0L7_2aSomeS884 = _M0L7_2abindS883;
    int32_t _M0L8_2aindexS885 = (int32_t)_M0L7_2aSomeS884;
    _M0L5indexS881 = _M0L8_2aindexS885;
    goto join_880;
  }
  join_880:;
  _M0L8_2afieldS2713 = _M0L4selfS882->$2;
  _M0L6_2acntS2987 = Moonbit_object_header(_M0L4selfS882)->rc;
  if (_M0L6_2acntS2987 > 1) {
    int32_t _M0L11_2anew__cntS2993 = _M0L6_2acntS2987 - 1;
    Moonbit_object_header(_M0L4selfS882)->rc = _M0L11_2anew__cntS2993;
    moonbit_incref(_M0L8_2afieldS2713);
  } else if (_M0L6_2acntS2987 == 1) {
    moonbit_string_t _M0L8_2afieldS2992 = _M0L4selfS882->$5;
    struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS2991;
    void* _M0L8_2afieldS2990;
    moonbit_string_t _M0L8_2afieldS2989;
    moonbit_string_t _M0L8_2afieldS2988;
    if (_M0L8_2afieldS2992) {
      moonbit_decref(_M0L8_2afieldS2992);
    }
    _M0L8_2afieldS2991 = _M0L4selfS882->$4;
    moonbit_decref(_M0L8_2afieldS2991);
    _M0L8_2afieldS2990 = _M0L4selfS882->$3;
    moonbit_decref(_M0L8_2afieldS2990);
    _M0L8_2afieldS2989 = _M0L4selfS882->$1;
    moonbit_decref(_M0L8_2afieldS2989);
    _M0L8_2afieldS2988 = _M0L4selfS882->$0;
    moonbit_decref(_M0L8_2afieldS2988);
    #line 102 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    moonbit_free(_M0L4selfS882);
  }
  _M0L4pathS2117 = _M0L8_2afieldS2713;
  _M0L6_2atmpS2118 = _M0L5indexS881 + 1;
  #line 102 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L6_2atmpS2116
  = _M0MPC16string6String11sub_2einner(_M0L4pathS2117, _M0L6_2atmpS2118, 4294967296ll);
  #line 102 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L6_2atmpS2116);
}

int64_t _M0FP36mulpjs4mulp6stream15last__index__of(
  moonbit_string_t _M0L5valueS877,
  moonbit_string_t _M0L6needleS878
) {
  struct _M0TPB8MutLocalGOiE* _M0L5foundS875;
  struct _M0TPB8MutLocalGiE* _M0L5indexS876;
  int64_t _result_3225;
  #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L5foundS875
  = (struct _M0TPB8MutLocalGOiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGOiE));
  Moonbit_object_header(_M0L5foundS875)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGOiE) >> 2, 0, 0);
  _M0L5foundS875->$0 = 4294967296ll;
  _M0L5indexS876
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5indexS876)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5indexS876->$0 = 0;
  while (1) {
    int32_t _M0L3valS2103 = _M0L5indexS876->$0;
    int32_t _M0L6_2atmpS2104 = Moonbit_array_length(_M0L5valueS877);
    if (_M0L3valS2103 < _M0L6_2atmpS2104) {
      int32_t _M0L3valS2108 = _M0L5indexS876->$0;
      int32_t _M0L3valS2111 = _M0L5indexS876->$0;
      int32_t _M0L6_2atmpS2110 = _M0L3valS2111 + 1;
      int64_t _M0L6_2atmpS2109 = (int64_t)_M0L6_2atmpS2110;
      struct _M0TPC16string10StringView _M0L6_2atmpS2105;
      int32_t _M0L6_2atmpS2107;
      struct _M0TPC16string10StringView _M0L6_2atmpS2106;
      int32_t _M0L3valS2115;
      int32_t _M0L6_2atmpS2114;
      moonbit_incref(_M0L5valueS877);
      #line 72 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
      _M0L6_2atmpS2105
      = _M0MPC16string6String11sub_2einner(_M0L5valueS877, _M0L3valS2108, _M0L6_2atmpS2109);
      _M0L6_2atmpS2107 = Moonbit_array_length(_M0L6needleS878);
      moonbit_incref(_M0L6needleS878);
      _M0L6_2atmpS2106
      = (struct _M0TPC16string10StringView){
        0, _M0L6_2atmpS2107, _M0L6needleS878
      };
      #line 72 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
      if (
        _M0IPC16string10StringViewPB2Eq5equal(_M0L6_2atmpS2105, _M0L6_2atmpS2106)
      ) {
        int32_t _M0L3valS2113 = _M0L5indexS876->$0;
        int64_t _M0L6_2atmpS2112 = (int64_t)_M0L3valS2113;
        _M0L5foundS875->$0 = _M0L6_2atmpS2112;
      }
      _M0L3valS2115 = _M0L5indexS876->$0;
      _M0L6_2atmpS2114 = _M0L3valS2115 + 1;
      _M0L5indexS876->$0 = _M0L6_2atmpS2114;
      continue;
    } else {
      moonbit_decref(_M0L6needleS878);
      moonbit_decref(_M0L5valueS877);
      moonbit_decref(_M0L5indexS876);
    }
    break;
  }
  _result_3225 = _M0L5foundS875->$0;
  moonbit_decref(_M0L5foundS875);
  return _result_3225;
}

void* _M0MP36mulpjs4mulp6stream10FileStream7collect(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS862,
  struct _M0TP36mulpjs4mulp4core7Context* _M0L3ctxS861
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2102;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS860;
  void* _block_3232;
  #line 374 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS2102
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
  _M0L5filesS860
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L5filesS860)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L5filesS860->$0 = _M0L6_2atmpS2102;
  _M0L5filesS860->$1 = 0;
  while (1) {
    void* _M0L3errS864;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS866;
    void* _M0L7_2abindS867;
    void* _block_3231;
    moonbit_incref(_M0L3ctxS861);
    #line 380 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    if (_M0MP36mulpjs4mulp4core7Context13is__cancelled(_M0L3ctxS861)) {
      void* _M0L6_2atmpS2101;
      void* _block_3227;
      moonbit_decref(_M0L3ctxS861);
      moonbit_decref(_M0L5filesS860);
      #line 381 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS862);
      #line 382 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0L6_2atmpS2101
      = _M0FP36mulpjs4mulp4core12task__failed((moonbit_string_t)moonbit_string_literal_80.data, (moonbit_string_t)moonbit_string_literal_81.data);
      _block_3227
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_3227)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3227)->$0
      = _M0L6_2atmpS2101;
      return _block_3227;
    }
    moonbit_incref(_M0L4selfS862);
    #line 384 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2abindS867
    = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L4selfS862);
    switch (Moonbit_object_tag(_M0L7_2abindS867)) {
      case 1: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS868 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS867;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2716 =
          _M0L5_2aOkS868->$0;
        int32_t _M0L6_2acntS3001 = Moonbit_object_header(_M0L5_2aOkS868)->rc;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS869;
        if (_M0L6_2acntS3001 > 1) {
          int32_t _M0L11_2anew__cntS3002 = _M0L6_2acntS3001 - 1;
          Moonbit_object_header(_M0L5_2aOkS868)->rc = _M0L11_2anew__cntS3002;
          if (_M0L8_2afieldS2716) {
            moonbit_incref(_M0L8_2afieldS2716);
          }
        } else if (_M0L6_2acntS3001 == 1) {
          #line 384 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L5_2aOkS868);
        }
        _M0L4_2axS869 = _M0L8_2afieldS2716;
        if (_M0L4_2axS869 == 0) {
          void* _block_3230;
          if (_M0L4_2axS869) {
            moonbit_decref(_M0L4_2axS869);
          }
          moonbit_decref(_M0L4selfS862);
          moonbit_decref(_M0L3ctxS861);
          _block_3230
          = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
          Moonbit_object_header(_block_3230)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
          ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3230)->$0
          = _M0L5filesS860;
          return _block_3230;
        } else {
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS870 =
            _M0L4_2axS869;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS871 =
            _M0L7_2aSomeS870;
          _M0L4fileS866 = _M0L7_2afileS871;
          goto join_865;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS872;
        void* _M0L8_2afieldS2717;
        int32_t _M0L6_2acntS3003;
        void* _M0L6_2aerrS873;
        moonbit_decref(_M0L3ctxS861);
        moonbit_decref(_M0L5filesS860);
        _M0L6_2aErrS872
        = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS867;
        _M0L8_2afieldS2717 = _M0L6_2aErrS872->$0;
        _M0L6_2acntS3003 = Moonbit_object_header(_M0L6_2aErrS872)->rc;
        if (_M0L6_2acntS3003 > 1) {
          int32_t _M0L11_2anew__cntS3004 = _M0L6_2acntS3003 - 1;
          Moonbit_object_header(_M0L6_2aErrS872)->rc = _M0L11_2anew__cntS3004;
          moonbit_incref(_M0L8_2afieldS2717);
        } else if (_M0L6_2acntS3003 == 1) {
          #line 384 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L6_2aErrS872);
        }
        _M0L6_2aerrS873 = _M0L8_2afieldS2717;
        _M0L3errS864 = _M0L6_2aerrS873;
        goto join_863;
        break;
      }
    }
    goto joinlet_3229;
    join_865:;
    moonbit_incref(_M0L5filesS860);
    #line 385 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0MPC15array5Array4pushGRP36mulpjs4mulp6stream4FileE(_M0L5filesS860, _M0L4fileS866);
    joinlet_3229:;
    goto joinlet_3228;
    join_863:;
    #line 388 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS862);
    _block_3231
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_3231)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3231)->$0
    = _M0L3errS864;
    return _block_3231;
    joinlet_3228:;
    continue;
    break;
  }
  _block_3232
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3232)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3232)->$0
  = _M0L5filesS860;
  return _block_3232;
}

struct _M0TP36mulpjs4mulp6stream4File* _M0MP36mulpjs4mulp6stream4File10with__path(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4selfS858,
  moonbit_string_t _M0L4pathS859
) {
  moonbit_string_t _M0L3cwdS2096;
  moonbit_string_t _M0L4baseS2097;
  void* _M0L8contentsS2098;
  struct _M0TPB5ArrayGUssEE* _M0L8metadataS2099;
  moonbit_string_t _M0L8_2afieldS2718;
  int32_t _M0L6_2acntS3005;
  moonbit_string_t _M0L11source__mapS2100;
  struct _M0TP36mulpjs4mulp6stream4File* _block_3233;
  #line 138 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L3cwdS2096 = _M0L4selfS858->$0;
  _M0L4baseS2097 = _M0L4selfS858->$1;
  _M0L8contentsS2098 = _M0L4selfS858->$3;
  _M0L8metadataS2099 = _M0L4selfS858->$4;
  _M0L8_2afieldS2718 = _M0L4selfS858->$5;
  _M0L6_2acntS3005 = Moonbit_object_header(_M0L4selfS858)->rc;
  if (_M0L6_2acntS3005 > 1) {
    int32_t _M0L11_2anew__cntS3007 = _M0L6_2acntS3005 - 1;
    Moonbit_object_header(_M0L4selfS858)->rc = _M0L11_2anew__cntS3007;
    if (_M0L8_2afieldS2718) {
      moonbit_incref(_M0L8_2afieldS2718);
    }
    moonbit_incref(_M0L8metadataS2099);
    moonbit_incref(_M0L8contentsS2098);
    moonbit_incref(_M0L4baseS2097);
    moonbit_incref(_M0L3cwdS2096);
  } else if (_M0L6_2acntS3005 == 1) {
    moonbit_string_t _M0L8_2afieldS3006 = _M0L4selfS858->$2;
    moonbit_decref(_M0L8_2afieldS3006);
    #line 145 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    moonbit_free(_M0L4selfS858);
  }
  _M0L11source__mapS2100 = _M0L8_2afieldS2718;
  _block_3233
  = (struct _M0TP36mulpjs4mulp6stream4File*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream4File));
  Moonbit_object_header(_block_3233)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream4File, $0) >> 2, 6, 0);
  _block_3233->$0 = _M0L3cwdS2096;
  _block_3233->$1 = _M0L4baseS2097;
  _block_3233->$2 = _M0L4pathS859;
  _block_3233->$3 = _M0L8contentsS2098;
  _block_3233->$4 = _M0L8metadataS2099;
  _block_3233->$5 = _M0L11source__mapS2100;
  return _block_3233;
}

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream10map__files(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L3mapS857
) {
  struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__* _closure_3234;
  #line 269 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _closure_3234
  = (struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__*)moonbit_malloc(sizeof(struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__));
  Moonbit_object_header(_closure_3234)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__, $0) >> 2, 1, 0);
  _closure_3234->code = &_M0FP36mulpjs4mulp6stream10map__filesC2090l270;
  _closure_3234->$0 = _M0L3mapS857;
  return (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)_closure_3234;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream10map__filesC2090l270(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS2091,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS856
) {
  struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__* _M0L14_2acasted__envS2092;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L8_2afieldS2723;
  int32_t _M0L6_2acntS3008;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream4File* _M0L3mapS857;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2095;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2094;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS2093;
  #line 270 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS2092
  = (struct _M0R60_24mulpjs_2fmulp_2fstream_2emap__files_2eanon__u2090__l270__*)_M0L6_2aenvS2091;
  _M0L8_2afieldS2723 = _M0L14_2acasted__envS2092->$0;
  _M0L6_2acntS3008 = Moonbit_object_header(_M0L14_2acasted__envS2092)->rc;
  if (_M0L6_2acntS3008 > 1) {
    int32_t _M0L11_2anew__cntS3009 = _M0L6_2acntS3008 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2092)->rc
    = _M0L11_2anew__cntS3009;
    moonbit_incref(_M0L8_2afieldS2723);
  } else if (_M0L6_2acntS3008 == 1) {
    #line 270 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS2092);
  }
  _M0L3mapS857 = _M0L8_2afieldS2723;
  #line 270 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS2095 = _M0L3mapS857->code(_M0L3mapS857, _M0L4fileS856);
  _M0L6_2atmpS2094
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2094[0] = _M0L6_2atmpS2095;
  _M0L6_2atmpS2093
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS2093)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2093->$0 = _M0L6_2atmpS2094;
  _M0L6_2atmpS2093->$1 = 1;
  #line 270 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream12file__stream(_M0L6_2atmpS2093);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0MP36mulpjs4mulp6stream10FileStream12pipe__stream(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS855,
  struct _M0TWRP36mulpjs4mulp6stream10FileStreamERP36mulpjs4mulp6stream10FileStream* _M0L9transformS854
) {
  #line 250 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  #line 254 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0L9transformS854->code(_M0L9transformS854, _M0L4selfS855);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0MP36mulpjs4mulp6stream10FileStream4pipe(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS818,
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L9transformS819
) {
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2089;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L5stateS817;
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__* _closure_3235;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2073;
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__* _closure_3236;
  struct _M0TWEu* _M0L6_2atmpS2074;
  #line 190 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS2089 = 0;
  _M0L5stateS817
  = (struct _M0TP36mulpjs4mulp6stream9PipeState*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream9PipeState));
  Moonbit_object_header(_M0L5stateS817)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream9PipeState, $0) >> 2, 3, 0);
  _M0L5stateS817->$0 = _M0L4selfS818;
  _M0L5stateS817->$1 = _M0L9transformS819;
  _M0L5stateS817->$2 = _M0L6_2atmpS2089;
  moonbit_incref(_M0L5stateS817);
  _closure_3235
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__*)moonbit_malloc(sizeof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__));
  Moonbit_object_header(_closure_3235)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__, $0) >> 2, 1, 0);
  _closure_3235->code = &_M0MP36mulpjs4mulp6stream10FileStream4pipeC2079l193;
  _closure_3235->$0 = _M0L5stateS817;
  _M0L6_2atmpS2073
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_3235;
  _closure_3236
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__*)moonbit_malloc(sizeof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__));
  Moonbit_object_header(_closure_3236)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__, $0) >> 2, 1, 0);
  _closure_3236->code = &_M0MP36mulpjs4mulp6stream10FileStream4pipeC2075l215;
  _closure_3236->$0 = _M0L5stateS817;
  _M0L6_2atmpS2074 = (struct _M0TWEu*)_closure_3236;
  #line 192 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L6_2atmpS2073, _M0L6_2atmpS2074);
}

void* _M0MP36mulpjs4mulp6stream10FileStream4pipeC2079l193(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2080
) {
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__* _M0L14_2acasted__envS2081;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L8_2afieldS2733;
  int32_t _M0L6_2acntS3010;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L5stateS817;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2088;
  void* _block_3247;
  #line 193 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS2081
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2079__l193__*)_M0L6_2aenvS2080;
  _M0L8_2afieldS2733 = _M0L14_2acasted__envS2081->$0;
  _M0L6_2acntS3010 = Moonbit_object_header(_M0L14_2acasted__envS2081)->rc;
  if (_M0L6_2acntS3010 > 1) {
    int32_t _M0L11_2anew__cntS3011 = _M0L6_2acntS3010 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2081)->rc
    = _M0L11_2anew__cntS3011;
    moonbit_incref(_M0L8_2afieldS2733);
  } else if (_M0L6_2acntS3010 == 1) {
    #line 193 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS2081);
  }
  _M0L5stateS817 = _M0L8_2afieldS2733;
  while (1) {
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7currentS821;
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2abindS833 =
      _M0L5stateS817->$2;
    void* _M0L3errS823;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS825;
    void* _M0L7_2abindS826;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2082;
    void* _block_3245;
    void* _block_3246;
    if (_M0L7_2abindS833 == 0) {
      void* _M0L3errS837;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS839;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS2087 =
        _M0L5stateS817->$0;
      void* _M0L7_2abindS841;
      struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L7_2afuncS840;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2085;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2084;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2aoldS2727;
      void* _block_3242;
      moonbit_incref(_M0L8upstreamS2087);
      #line 206 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0L7_2abindS841
      = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L8upstreamS2087);
      switch (Moonbit_object_tag(_M0L7_2abindS841)) {
        case 1: {
          struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS842 =
            (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS841;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2729 =
            _M0L5_2aOkS842->$0;
          int32_t _M0L6_2acntS3016 =
            Moonbit_object_header(_M0L5_2aOkS842)->rc;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS843;
          if (_M0L6_2acntS3016 > 1) {
            int32_t _M0L11_2anew__cntS3017 = _M0L6_2acntS3016 - 1;
            Moonbit_object_header(_M0L5_2aOkS842)->rc
            = _M0L11_2anew__cntS3017;
            if (_M0L8_2afieldS2729) {
              moonbit_incref(_M0L8_2afieldS2729);
            }
          } else if (_M0L6_2acntS3016 == 1) {
            #line 206 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
            moonbit_free(_M0L5_2aOkS842);
          }
          _M0L4_2axS843 = _M0L8_2afieldS2729;
          if (_M0L4_2axS843 == 0) {
            struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2086;
            void* _block_3241;
            if (_M0L4_2axS843) {
              moonbit_decref(_M0L4_2axS843);
            }
            moonbit_decref(_M0L5stateS817);
            _M0L6_2atmpS2086 = 0;
            _block_3241
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
            Moonbit_object_header(_block_3241)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
            ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3241)->$0
            = _M0L6_2atmpS2086;
            return _block_3241;
          } else {
            struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS844 =
              _M0L4_2axS843;
            struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS845 =
              _M0L7_2aSomeS844;
            _M0L4fileS839 = _M0L7_2afileS845;
            goto join_838;
          }
          break;
        }
        default: {
          struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS846;
          void* _M0L8_2afieldS2730;
          int32_t _M0L6_2acntS3018;
          void* _M0L6_2aerrS847;
          moonbit_decref(_M0L5stateS817);
          _M0L6_2aErrS846
          = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS841;
          _M0L8_2afieldS2730 = _M0L6_2aErrS846->$0;
          _M0L6_2acntS3018 = Moonbit_object_header(_M0L6_2aErrS846)->rc;
          if (_M0L6_2acntS3018 > 1) {
            int32_t _M0L11_2anew__cntS3019 = _M0L6_2acntS3018 - 1;
            Moonbit_object_header(_M0L6_2aErrS846)->rc
            = _M0L11_2anew__cntS3019;
            moonbit_incref(_M0L8_2afieldS2730);
          } else if (_M0L6_2acntS3018 == 1) {
            #line 206 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
            moonbit_free(_M0L6_2aErrS846);
          }
          _M0L6_2aerrS847 = _M0L8_2afieldS2730;
          _M0L3errS837 = _M0L6_2aerrS847;
          goto join_836;
          break;
        }
      }
      goto joinlet_3240;
      join_838:;
      _M0L7_2afuncS840 = _M0L5stateS817->$1;
      moonbit_incref(_M0L7_2afuncS840);
      #line 207 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0L6_2atmpS2085
      = _M0L7_2afuncS840->code(_M0L7_2afuncS840, _M0L4fileS839);
      _M0L6_2atmpS2084 = _M0L6_2atmpS2085;
      _M0L6_2aoldS2727 = _M0L5stateS817->$2;
      if (_M0L6_2aoldS2727) {
        moonbit_decref(_M0L6_2aoldS2727);
      }
      _M0L5stateS817->$2 = _M0L6_2atmpS2084;
      joinlet_3240:;
      goto joinlet_3239;
      join_836:;
      _block_3242
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_3242)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3242)->$0
      = _M0L3errS837;
      return _block_3242;
      joinlet_3239:;
    } else {
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2aSomeS834 =
        _M0L7_2abindS833;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L10_2acurrentS835 =
        _M0L7_2aSomeS834;
      moonbit_incref(_M0L10_2acurrentS835);
      _M0L7currentS821 = _M0L10_2acurrentS835;
      goto join_820;
    }
    goto joinlet_3238;
    join_820:;
    moonbit_incref(_M0L7currentS821);
    #line 197 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2abindS826
    = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L7currentS821);
    switch (Moonbit_object_tag(_M0L7_2abindS826)) {
      case 1: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS827 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS826;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2725 =
          _M0L5_2aOkS827->$0;
        int32_t _M0L6_2acntS3012 = Moonbit_object_header(_M0L5_2aOkS827)->rc;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS828;
        if (_M0L6_2acntS3012 > 1) {
          int32_t _M0L11_2anew__cntS3013 = _M0L6_2acntS3012 - 1;
          Moonbit_object_header(_M0L5_2aOkS827)->rc = _M0L11_2anew__cntS3013;
          if (_M0L8_2afieldS2725) {
            moonbit_incref(_M0L8_2afieldS2725);
          }
        } else if (_M0L6_2acntS3012 == 1) {
          #line 197 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L5_2aOkS827);
        }
        _M0L4_2axS828 = _M0L8_2afieldS2725;
        if (_M0L4_2axS828 == 0) {
          struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS2083;
          struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2aoldS2724;
          if (_M0L4_2axS828) {
            moonbit_decref(_M0L4_2axS828);
          }
          #line 200 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L7currentS821);
          _M0L6_2atmpS2083 = 0;
          _M0L6_2aoldS2724 = _M0L5stateS817->$2;
          if (_M0L6_2aoldS2724) {
            moonbit_decref(_M0L6_2aoldS2724);
          }
          _M0L5stateS817->$2 = _M0L6_2atmpS2083;
        } else {
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS829;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS830;
          moonbit_decref(_M0L7currentS821);
          moonbit_decref(_M0L5stateS817);
          _M0L7_2aSomeS829 = _M0L4_2axS828;
          _M0L7_2afileS830 = _M0L7_2aSomeS829;
          _M0L4fileS825 = _M0L7_2afileS830;
          goto join_824;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS831;
        void* _M0L8_2afieldS2726;
        int32_t _M0L6_2acntS3014;
        void* _M0L6_2aerrS832;
        moonbit_decref(_M0L7currentS821);
        moonbit_decref(_M0L5stateS817);
        _M0L6_2aErrS831
        = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS826;
        _M0L8_2afieldS2726 = _M0L6_2aErrS831->$0;
        _M0L6_2acntS3014 = Moonbit_object_header(_M0L6_2aErrS831)->rc;
        if (_M0L6_2acntS3014 > 1) {
          int32_t _M0L11_2anew__cntS3015 = _M0L6_2acntS3014 - 1;
          Moonbit_object_header(_M0L6_2aErrS831)->rc = _M0L11_2anew__cntS3015;
          moonbit_incref(_M0L8_2afieldS2726);
        } else if (_M0L6_2acntS3014 == 1) {
          #line 197 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L6_2aErrS831);
        }
        _M0L6_2aerrS832 = _M0L8_2afieldS2726;
        _M0L3errS823 = _M0L6_2aerrS832;
        goto join_822;
        break;
      }
    }
    goto joinlet_3244;
    join_824:;
    _M0L6_2atmpS2082 = _M0L4fileS825;
    _block_3245
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3245)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3245)->$0
    = _M0L6_2atmpS2082;
    return _block_3245;
    joinlet_3244:;
    goto joinlet_3243;
    join_822:;
    _block_3246
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_3246)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3246)->$0
    = _M0L3errS823;
    return _block_3246;
    joinlet_3243:;
    joinlet_3238:;
    continue;
    break;
  }
  _M0L6_2atmpS2088 = 0;
  _block_3247
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_3247)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3247)->$0
  = _M0L6_2atmpS2088;
  return _block_3247;
}

int32_t _M0MP36mulpjs4mulp6stream10FileStream4pipeC2075l215(
  struct _M0TWEu* _M0L6_2aenvS2076
) {
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__* _M0L14_2acasted__envS2077;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L8_2afieldS2736;
  int32_t _M0L6_2acntS3020;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L5stateS817;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7currentS850;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2abindS851;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS2734;
  int32_t _M0L6_2acntS3022;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS2078;
  #line 215 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS2077
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u2075__l215__*)_M0L6_2aenvS2076;
  _M0L8_2afieldS2736 = _M0L14_2acasted__envS2077->$0;
  _M0L6_2acntS3020 = Moonbit_object_header(_M0L14_2acasted__envS2077)->rc;
  if (_M0L6_2acntS3020 > 1) {
    int32_t _M0L11_2anew__cntS3021 = _M0L6_2acntS3020 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2077)->rc
    = _M0L11_2anew__cntS3021;
    moonbit_incref(_M0L8_2afieldS2736);
  } else if (_M0L6_2acntS3020 == 1) {
    #line 215 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS2077);
  }
  _M0L5stateS817 = _M0L8_2afieldS2736;
  _M0L7_2abindS851 = _M0L5stateS817->$2;
  if (_M0L7_2abindS851 == 0) {
    
  } else {
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2aSomeS852 =
      _M0L7_2abindS851;
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L10_2acurrentS853 =
      _M0L7_2aSomeS852;
    moonbit_incref(_M0L10_2acurrentS853);
    _M0L7currentS850 = _M0L10_2acurrentS853;
    goto join_849;
  }
  goto joinlet_3248;
  join_849:;
  #line 217 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L7currentS850);
  joinlet_3248:;
  _M0L8_2afieldS2734 = _M0L5stateS817->$0;
  _M0L6_2acntS3022 = Moonbit_object_header(_M0L5stateS817)->rc;
  if (_M0L6_2acntS3022 > 1) {
    int32_t _M0L11_2anew__cntS3025 = _M0L6_2acntS3022 - 1;
    Moonbit_object_header(_M0L5stateS817)->rc = _M0L11_2anew__cntS3025;
    moonbit_incref(_M0L8_2afieldS2734);
  } else if (_M0L6_2acntS3022 == 1) {
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS3024 =
      _M0L5stateS817->$2;
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS3023;
    if (_M0L8_2afieldS3024) {
      moonbit_decref(_M0L8_2afieldS3024);
    }
    _M0L8_2afieldS3023 = _M0L5stateS817->$1;
    moonbit_decref(_M0L8_2afieldS3023);
    #line 220 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L5stateS817);
  }
  _M0L8upstreamS2078 = _M0L8_2afieldS2734;
  #line 220 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L8upstreamS2078);
  return 0;
}

void* _M0MP36mulpjs4mulp6stream10FileStream4next(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS806
) {
  #line 163 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  if (_M0L4selfS806->$2) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2071;
    void* _block_3249;
    moonbit_decref(_M0L4selfS806);
    _M0L6_2atmpS2071 = 0;
    _block_3249
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3249)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3249)->$0
    = _M0L6_2atmpS2071;
    return _block_3249;
  } else {
    void* _M0L3errS808;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS810;
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L7_2afuncS812 =
      _M0L4selfS806->$0;
    void* _M0L7_2abindS811;
    void* _block_3253;
    void* _block_3254;
    moonbit_incref(_M0L7_2afuncS812);
    #line 167 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2abindS811 = _M0L7_2afuncS812->code(_M0L7_2afuncS812);
    switch (Moonbit_object_tag(_M0L7_2abindS811)) {
      case 1: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS813 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS811;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2737 =
          _M0L5_2aOkS813->$0;
        int32_t _M0L6_2acntS3026 = Moonbit_object_header(_M0L5_2aOkS813)->rc;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS814;
        if (_M0L6_2acntS3026 > 1) {
          int32_t _M0L11_2anew__cntS3027 = _M0L6_2acntS3026 - 1;
          Moonbit_object_header(_M0L5_2aOkS813)->rc = _M0L11_2anew__cntS3027;
          if (_M0L8_2afieldS2737) {
            moonbit_incref(_M0L8_2afieldS2737);
          }
        } else if (_M0L6_2acntS3026 == 1) {
          #line 167 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L5_2aOkS813);
        }
        _M0L4_2axS814 = _M0L8_2afieldS2737;
        if (_M0L4_2axS814 == 0) {
          struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2072;
          void* _block_3252;
          if (_M0L4_2axS814) {
            moonbit_decref(_M0L4_2axS814);
          }
          #line 169 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS806);
          _M0L6_2atmpS2072 = 0;
          _block_3252
          = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
          Moonbit_object_header(_block_3252)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
          ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3252)->$0
          = _M0L6_2atmpS2072;
          return _block_3252;
        } else {
          moonbit_decref(_M0L4selfS806);
          _M0L4fileS810 = _M0L4_2axS814;
          goto join_809;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS815 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS811;
        void* _M0L8_2afieldS2738 = _M0L6_2aErrS815->$0;
        int32_t _M0L6_2acntS3028 = Moonbit_object_header(_M0L6_2aErrS815)->rc;
        void* _M0L6_2aerrS816;
        if (_M0L6_2acntS3028 > 1) {
          int32_t _M0L11_2anew__cntS3029 = _M0L6_2acntS3028 - 1;
          Moonbit_object_header(_M0L6_2aErrS815)->rc = _M0L11_2anew__cntS3029;
          moonbit_incref(_M0L8_2afieldS2738);
        } else if (_M0L6_2acntS3028 == 1) {
          #line 167 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L6_2aErrS815);
        }
        _M0L6_2aerrS816 = _M0L8_2afieldS2738;
        _M0L3errS808 = _M0L6_2aerrS816;
        goto join_807;
        break;
      }
    }
    join_809:;
    _block_3253
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3253)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3253)->$0
    = _M0L4fileS810;
    return _block_3253;
    join_807:;
    #line 174 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS806);
    _block_3254
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_3254)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3254)->$0
    = _M0L3errS808;
    return _block_3254;
  }
}

int32_t _M0MP36mulpjs4mulp6stream10FileStream5close(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS804
) {
  int32_t _M0L6closedS2070;
  #line 182 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6closedS2070 = _M0L4selfS804->$2;
  if (!_M0L6closedS2070) {
    struct _M0TWEu* _M0L8_2afieldS2740;
    int32_t _M0L6_2acntS3030;
    struct _M0TWEu* _M0L7_2afuncS805;
    _M0L4selfS804->$2 = 1;
    _M0L8_2afieldS2740 = _M0L4selfS804->$1;
    _M0L6_2acntS3030 = Moonbit_object_header(_M0L4selfS804)->rc;
    if (_M0L6_2acntS3030 > 1) {
      int32_t _M0L11_2anew__cntS3032 = _M0L6_2acntS3030 - 1;
      Moonbit_object_header(_M0L4selfS804)->rc = _M0L11_2anew__cntS3032;
      moonbit_incref(_M0L8_2afieldS2740);
    } else if (_M0L6_2acntS3030 == 1) {
      struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS3031 =
        _M0L4selfS804->$0;
      moonbit_decref(_M0L8_2afieldS3031);
      #line 185 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      moonbit_free(_M0L4selfS804);
    }
    _M0L7_2afuncS805 = _M0L8_2afieldS2740;
    #line 185 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2afuncS805->code(_M0L7_2afuncS805);
  } else {
    moonbit_decref(_M0L4selfS804);
  }
  return 0;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream13error__stream(
  void* _M0L3errS803
) {
  struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__* _closure_3255;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2066;
  #line 67 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _closure_3255
  = (struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__*)moonbit_malloc(sizeof(struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__));
  Moonbit_object_header(_closure_3255)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__, $0) >> 2, 1, 0);
  _closure_3255->code = &_M0FP36mulpjs4mulp6stream13error__streamC2067l68;
  _closure_3255->$0 = _M0L3errS803;
  _M0L6_2atmpS2066
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_3255;
  #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream24file__stream__from__pull(_M0L6_2atmpS2066);
}

void* _M0FP36mulpjs4mulp6stream13error__streamC2067l68(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2068
) {
  struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__* _M0L14_2acasted__envS2069;
  void* _M0L8_2afieldS2741;
  int32_t _M0L6_2acntS3033;
  void* _M0L3errS803;
  void* _block_3256;
  #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS2069
  = (struct _M0R62_24mulpjs_2fmulp_2fstream_2eerror__stream_2eanon__u2067__l68__*)_M0L6_2aenvS2068;
  _M0L8_2afieldS2741 = _M0L14_2acasted__envS2069->$0;
  _M0L6_2acntS3033 = Moonbit_object_header(_M0L14_2acasted__envS2069)->rc;
  if (_M0L6_2acntS3033 > 1) {
    int32_t _M0L11_2anew__cntS3034 = _M0L6_2acntS3033 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2069)->rc
    = _M0L11_2anew__cntS3034;
    moonbit_incref(_M0L8_2afieldS2741);
  } else if (_M0L6_2acntS3033 == 1) {
    #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS2069);
  }
  _M0L3errS803 = _M0L8_2afieldS2741;
  _block_3256
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
  Moonbit_object_header(_block_3256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_3256)->$0
  = _M0L3errS803;
  return _block_3256;
}

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp6stream4file(
  moonbit_string_t _M0L3cwdS799,
  moonbit_string_t _M0L4baseS800,
  moonbit_string_t _M0L4pathS801,
  void* _M0L8contentsS802
) {
  struct _M0TUssE** _M0L6_2atmpS2065;
  struct _M0TPB5ArrayGUssEE* _M0L6_2atmpS2063;
  moonbit_string_t _M0L6_2atmpS2064;
  struct _M0TP36mulpjs4mulp6stream4File* _block_3257;
  #line 22 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L6_2atmpS2065 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2063
  = (struct _M0TPB5ArrayGUssEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUssEE));
  Moonbit_object_header(_M0L6_2atmpS2063)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUssEE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2063->$0 = _M0L6_2atmpS2065;
  _M0L6_2atmpS2063->$1 = 0;
  _M0L6_2atmpS2064 = 0;
  _block_3257
  = (struct _M0TP36mulpjs4mulp6stream4File*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream4File));
  Moonbit_object_header(_block_3257)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream4File, $0) >> 2, 6, 0);
  _block_3257->$0 = _M0L3cwdS799;
  _block_3257->$1 = _M0L4baseS800;
  _block_3257->$2 = _M0L4pathS801;
  _block_3257->$3 = _M0L8contentsS802;
  _block_3257->$4 = _M0L6_2atmpS2063;
  _block_3257->$5 = _M0L6_2atmpS2064;
  return _block_3257;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream12file__stream(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS797
) {
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* _M0L5stateS796;
  struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__* _closure_3258;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS2050;
  #line 127 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L5stateS796
  = (struct _M0TP36mulpjs4mulp6stream14ArrayFileState*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream14ArrayFileState));
  Moonbit_object_header(_M0L5stateS796)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream14ArrayFileState, $0) >> 2, 1, 0);
  _M0L5stateS796->$0 = _M0L5filesS797;
  _M0L5stateS796->$1 = 0;
  _closure_3258
  = (struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__*)moonbit_malloc(sizeof(struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__));
  Moonbit_object_header(_closure_3258)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__, $0) >> 2, 1, 0);
  _closure_3258->code = &_M0FP36mulpjs4mulp6stream12file__streamC2051l129;
  _closure_3258->$0 = _M0L5stateS796;
  _M0L6_2atmpS2050
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_3258;
  #line 129 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream24file__stream__from__pull(_M0L6_2atmpS2050);
}

void* _M0FP36mulpjs4mulp6stream12file__streamC2051l129(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS2052
) {
  struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__* _M0L14_2acasted__envS2053;
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* _M0L8_2afieldS2744;
  int32_t _M0L6_2acntS3035;
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* _M0L5stateS796;
  int32_t _M0L5indexS2054;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS2056;
  int32_t _M0L6_2atmpS2055;
  #line 129 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS2053
  = (struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u2051__l129__*)_M0L6_2aenvS2052;
  _M0L8_2afieldS2744 = _M0L14_2acasted__envS2053->$0;
  _M0L6_2acntS3035 = Moonbit_object_header(_M0L14_2acasted__envS2053)->rc;
  if (_M0L6_2acntS3035 > 1) {
    int32_t _M0L11_2anew__cntS3036 = _M0L6_2acntS3035 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2053)->rc
    = _M0L11_2anew__cntS3036;
    moonbit_incref(_M0L8_2afieldS2744);
  } else if (_M0L6_2acntS3035 == 1) {
    #line 129 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS2053);
  }
  _M0L5stateS796 = _M0L8_2afieldS2744;
  _M0L5indexS2054 = _M0L5stateS796->$1;
  _M0L5filesS2056 = _M0L5stateS796->$0;
  moonbit_incref(_M0L5filesS2056);
  #line 130 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS2055
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L5filesS2056);
  if (_M0L5indexS2054 >= _M0L6_2atmpS2055) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2057;
    void* _block_3259;
    moonbit_decref(_M0L5stateS796);
    _M0L6_2atmpS2057 = 0;
    _block_3259
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3259)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3259)->$0
    = _M0L6_2atmpS2057;
    return _block_3259;
  } else {
    struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS2061 =
      _M0L5stateS796->$0;
    int32_t _M0L5indexS2062 = _M0L5stateS796->$1;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS798;
    int32_t _M0L5indexS2059;
    int32_t _M0L6_2atmpS2058;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2060;
    void* _block_3260;
    moonbit_incref(_M0L5filesS2061);
    #line 133 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L4fileS798
    = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L5filesS2061, _M0L5indexS2062);
    _M0L5indexS2059 = _M0L5stateS796->$1;
    _M0L6_2atmpS2058 = _M0L5indexS2059 + 1;
    _M0L5stateS796->$1 = _M0L6_2atmpS2058;
    moonbit_decref(_M0L5stateS796);
    _M0L6_2atmpS2060 = _M0L4fileS798;
    _block_3260
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_3260)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_3260)->$0
    = _M0L6_2atmpS2060;
    return _block_3260;
  }
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream24file__stream__from__pull(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L4pullS795
) {
  struct _M0TWEu* _M0L6_2atmpS2047;
  #line 54 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS2047
  = (struct _M0TWEu*)&_M0FP36mulpjs4mulp6stream24file__stream__from__pullC2048l55$closure.data;
  #line 55 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L4pullS795, _M0L6_2atmpS2047);
}

int32_t _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2048l55(
  struct _M0TWEu* _M0L6_2aenvS2049
) {
  #line 55 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  moonbit_decref(_M0L6_2aenvS2049);
  return 0;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L4pullS793,
  struct _M0TWEu* _M0L5closeS794
) {
  struct _M0TP36mulpjs4mulp6stream10FileStream* _block_3261;
  #line 59 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _block_3261
  = (struct _M0TP36mulpjs4mulp6stream10FileStream*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream10FileStream));
  Moonbit_object_header(_block_3261)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream10FileStream, $0) >> 2, 2, 0);
  _block_3261->$0 = _M0L4pullS793;
  _block_3261->$1 = _M0L5closeS794;
  _block_3261->$2 = 0;
  return _block_3261;
}

struct _M0TP36mulpjs4mulp4core7Context* _M0FP36mulpjs4mulp4core12new__context(
  moonbit_string_t _M0L3cwdS790,
  int64_t _M0L7now__msS791,
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS792
) {
  struct _M0TP36mulpjs4mulp4core7Context* _block_3262;
  #line 29 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _block_3262
  = (struct _M0TP36mulpjs4mulp4core7Context*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp4core7Context));
  Moonbit_object_header(_block_3262)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp4core7Context, $0) >> 2, 2, 0);
  _block_3262->$0 = _M0L3cwdS790;
  _block_3262->$1 = _M0L7now__msS791;
  _block_3262->$2 = _M0L12cancellationS792;
  return _block_3262;
}

struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0FP36mulpjs4mulp4core24new__cancellation__token(
  
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _block_3263;
  #line 7 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _block_3263
  = (struct _M0TP36mulpjs4mulp4core17CancellationToken*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp4core17CancellationToken));
  Moonbit_object_header(_block_3263)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP36mulpjs4mulp4core17CancellationToken) >> 2, 0, 0);
  _block_3263->$0 = 0;
  return _block_3263;
}

moonbit_string_t _M0MP36mulpjs4mulp4core9MulpError7message(
  void* _M0L4selfS772
) {
  moonbit_string_t _M0L6detailS750;
  moonbit_string_t _M0L6detailS752;
  moonbit_string_t _M0L6detailS754;
  moonbit_string_t _M0L6detailS756;
  moonbit_string_t _M0L6detailS758;
  struct _M0TPB5ArrayGUssEE* _M0L6errorsS760;
  moonbit_string_t _M0L4nameS768;
  moonbit_string_t _M0L5causeS769;
  moonbit_string_t _M0L4nameS771;
  moonbit_string_t _result_3272;
  moonbit_string_t _M0L6_2atmpS2046;
  moonbit_string_t _M0L6_2atmpS2045;
  moonbit_string_t _result_3273;
  moonbit_string_t* _M0L6_2atmpS2044;
  struct _M0TPB5ArrayGsE* _M0L5partsS761;
  int32_t _M0L7_2abindS762;
  int32_t _M0L2__S763;
  moonbit_string_t _M0L7_2abindS766;
  int32_t _M0L6_2atmpS2043;
  struct _M0TPC16string10StringView _M0L6_2atmpS2042;
  moonbit_string_t _M0L6_2atmpS2041;
  moonbit_string_t _result_3275;
  moonbit_string_t _result_3276;
  moonbit_string_t _result_3277;
  moonbit_string_t _result_3278;
  moonbit_string_t _result_3279;
  moonbit_string_t _result_3280;
  #line 54 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  switch (Moonbit_object_tag(_M0L4selfS772)) {
    case 0: {
      struct _M0DTP36mulpjs4mulp4core9MulpError12TaskNotFound* _M0L15_2aTaskNotFoundS773 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError12TaskNotFound*)_M0L4selfS772;
      moonbit_string_t _M0L8_2afieldS2749 = _M0L15_2aTaskNotFoundS773->$0;
      int32_t _M0L6_2acntS3037 =
        Moonbit_object_header(_M0L15_2aTaskNotFoundS773)->rc;
      moonbit_string_t _M0L7_2anameS774;
      if (_M0L6_2acntS3037 > 1) {
        int32_t _M0L11_2anew__cntS3038 = _M0L6_2acntS3037 - 1;
        Moonbit_object_header(_M0L15_2aTaskNotFoundS773)->rc
        = _M0L11_2anew__cntS3038;
        moonbit_incref(_M0L8_2afieldS2749);
      } else if (_M0L6_2acntS3037 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L15_2aTaskNotFoundS773);
      }
      _M0L7_2anameS774 = _M0L8_2afieldS2749;
      _M0L4nameS771 = _M0L7_2anameS774;
      goto join_770;
      break;
    }
    
    case 1: {
      struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed* _M0L13_2aTaskFailedS775 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed*)_M0L4selfS772;
      moonbit_string_t _M0L7_2anameS776 = _M0L13_2aTaskFailedS775->$0;
      moonbit_string_t _M0L8_2afieldS2750 = _M0L13_2aTaskFailedS775->$1;
      int32_t _M0L6_2acntS3039 =
        Moonbit_object_header(_M0L13_2aTaskFailedS775)->rc;
      moonbit_string_t _M0L8_2acauseS777;
      if (_M0L6_2acntS3039 > 1) {
        int32_t _M0L11_2anew__cntS3040 = _M0L6_2acntS3039 - 1;
        Moonbit_object_header(_M0L13_2aTaskFailedS775)->rc
        = _M0L11_2anew__cntS3040;
        moonbit_incref(_M0L8_2afieldS2750);
        moonbit_incref(_M0L7_2anameS776);
      } else if (_M0L6_2acntS3039 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L13_2aTaskFailedS775);
      }
      _M0L8_2acauseS777 = _M0L8_2afieldS2750;
      _M0L4nameS768 = _M0L7_2anameS776;
      _M0L5causeS769 = _M0L8_2acauseS777;
      goto join_767;
      break;
    }
    
    case 2: {
      struct _M0DTP36mulpjs4mulp4core9MulpError14ParallelFailed* _M0L17_2aParallelFailedS778 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError14ParallelFailed*)_M0L4selfS772;
      struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS2752 =
        _M0L17_2aParallelFailedS778->$0;
      int32_t _M0L6_2acntS3041 =
        Moonbit_object_header(_M0L17_2aParallelFailedS778)->rc;
      struct _M0TPB5ArrayGUssEE* _M0L9_2aerrorsS779;
      if (_M0L6_2acntS3041 > 1) {
        int32_t _M0L11_2anew__cntS3042 = _M0L6_2acntS3041 - 1;
        Moonbit_object_header(_M0L17_2aParallelFailedS778)->rc
        = _M0L11_2anew__cntS3042;
        moonbit_incref(_M0L8_2afieldS2752);
      } else if (_M0L6_2acntS3041 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L17_2aParallelFailedS778);
      }
      _M0L9_2aerrorsS779 = _M0L8_2afieldS2752;
      _M0L6errorsS760 = _M0L9_2aerrorsS779;
      goto join_759;
      break;
    }
    
    case 3: {
      struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError* _M0L18_2aFileSystemErrorS780 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError*)_M0L4selfS772;
      moonbit_string_t _M0L8_2afieldS2753 = _M0L18_2aFileSystemErrorS780->$0;
      int32_t _M0L6_2acntS3043 =
        Moonbit_object_header(_M0L18_2aFileSystemErrorS780)->rc;
      moonbit_string_t _M0L9_2adetailS781;
      if (_M0L6_2acntS3043 > 1) {
        int32_t _M0L11_2anew__cntS3044 = _M0L6_2acntS3043 - 1;
        Moonbit_object_header(_M0L18_2aFileSystemErrorS780)->rc
        = _M0L11_2anew__cntS3044;
        moonbit_incref(_M0L8_2afieldS2753);
      } else if (_M0L6_2acntS3043 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L18_2aFileSystemErrorS780);
      }
      _M0L9_2adetailS781 = _M0L8_2afieldS2753;
      _M0L6detailS758 = _M0L9_2adetailS781;
      goto join_757;
      break;
    }
    
    case 4: {
      struct _M0DTP36mulpjs4mulp4core9MulpError9GlobError* _M0L12_2aGlobErrorS782 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError9GlobError*)_M0L4selfS772;
      moonbit_string_t _M0L8_2afieldS2754 = _M0L12_2aGlobErrorS782->$0;
      int32_t _M0L6_2acntS3045 =
        Moonbit_object_header(_M0L12_2aGlobErrorS782)->rc;
      moonbit_string_t _M0L9_2adetailS783;
      if (_M0L6_2acntS3045 > 1) {
        int32_t _M0L11_2anew__cntS3046 = _M0L6_2acntS3045 - 1;
        Moonbit_object_header(_M0L12_2aGlobErrorS782)->rc
        = _M0L11_2anew__cntS3046;
        moonbit_incref(_M0L8_2afieldS2754);
      } else if (_M0L6_2acntS3045 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L12_2aGlobErrorS782);
      }
      _M0L9_2adetailS783 = _M0L8_2afieldS2754;
      _M0L6detailS756 = _M0L9_2adetailS783;
      goto join_755;
      break;
    }
    
    case 5: {
      struct _M0DTP36mulpjs4mulp4core9MulpError10WatchError* _M0L13_2aWatchErrorS784 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError10WatchError*)_M0L4selfS772;
      moonbit_string_t _M0L8_2afieldS2755 = _M0L13_2aWatchErrorS784->$0;
      int32_t _M0L6_2acntS3047 =
        Moonbit_object_header(_M0L13_2aWatchErrorS784)->rc;
      moonbit_string_t _M0L9_2adetailS785;
      if (_M0L6_2acntS3047 > 1) {
        int32_t _M0L11_2anew__cntS3048 = _M0L6_2acntS3047 - 1;
        Moonbit_object_header(_M0L13_2aWatchErrorS784)->rc
        = _M0L11_2anew__cntS3048;
        moonbit_incref(_M0L8_2afieldS2755);
      } else if (_M0L6_2acntS3047 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L13_2aWatchErrorS784);
      }
      _M0L9_2adetailS785 = _M0L8_2afieldS2755;
      _M0L6detailS754 = _M0L9_2adetailS785;
      goto join_753;
      break;
    }
    
    case 6: {
      struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError* _M0L14_2aStreamErrorS786 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError*)_M0L4selfS772;
      moonbit_string_t _M0L8_2afieldS2756 = _M0L14_2aStreamErrorS786->$0;
      int32_t _M0L6_2acntS3049 =
        Moonbit_object_header(_M0L14_2aStreamErrorS786)->rc;
      moonbit_string_t _M0L9_2adetailS787;
      if (_M0L6_2acntS3049 > 1) {
        int32_t _M0L11_2anew__cntS3050 = _M0L6_2acntS3049 - 1;
        Moonbit_object_header(_M0L14_2aStreamErrorS786)->rc
        = _M0L11_2anew__cntS3050;
        moonbit_incref(_M0L8_2afieldS2756);
      } else if (_M0L6_2acntS3049 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L14_2aStreamErrorS786);
      }
      _M0L9_2adetailS787 = _M0L8_2afieldS2756;
      _M0L6detailS752 = _M0L9_2adetailS787;
      goto join_751;
      break;
    }
    default: {
      struct _M0DTP36mulpjs4mulp4core9MulpError11ConfigError* _M0L14_2aConfigErrorS788 =
        (struct _M0DTP36mulpjs4mulp4core9MulpError11ConfigError*)_M0L4selfS772;
      moonbit_string_t _M0L8_2afieldS2757 = _M0L14_2aConfigErrorS788->$0;
      int32_t _M0L6_2acntS3051 =
        Moonbit_object_header(_M0L14_2aConfigErrorS788)->rc;
      moonbit_string_t _M0L9_2adetailS789;
      if (_M0L6_2acntS3051 > 1) {
        int32_t _M0L11_2anew__cntS3052 = _M0L6_2acntS3051 - 1;
        Moonbit_object_header(_M0L14_2aConfigErrorS788)->rc
        = _M0L11_2anew__cntS3052;
        moonbit_incref(_M0L8_2afieldS2757);
      } else if (_M0L6_2acntS3051 == 1) {
        #line 55 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
        moonbit_free(_M0L14_2aConfigErrorS788);
      }
      _M0L9_2adetailS789 = _M0L8_2afieldS2757;
      _M0L6detailS750 = _M0L9_2adetailS789;
      goto join_749;
      break;
    }
  }
  join_770:;
  #line 56 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3272
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_82.data, _M0L4nameS771);
  moonbit_decref(_M0L4nameS771);
  return _result_3272;
  join_767:;
  #line 57 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _M0L6_2atmpS2046
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_83.data, _M0L4nameS768);
  moonbit_decref(_M0L4nameS768);
  #line 57 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _M0L6_2atmpS2045
  = moonbit_add_string(_M0L6_2atmpS2046, (moonbit_string_t)moonbit_string_literal_84.data);
  moonbit_decref(_M0L6_2atmpS2046);
  #line 57 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3273 = moonbit_add_string(_M0L6_2atmpS2045, _M0L5causeS769);
  moonbit_decref(_M0L5causeS769);
  moonbit_decref(_M0L6_2atmpS2045);
  return _result_3273;
  join_759:;
  _M0L6_2atmpS2044 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L5partsS761
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L5partsS761)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L5partsS761->$0 = _M0L6_2atmpS2044;
  _M0L5partsS761->$1 = 0;
  _M0L7_2abindS762 = _M0L6errorsS760->$1;
  _M0L2__S763 = 0;
  while (1) {
    if (_M0L2__S763 < _M0L7_2abindS762) {
      struct _M0TUssE** _M0L3bufS2040 = _M0L6errorsS760->$0;
      struct _M0TUssE* _M0L4itemS764 =
        (struct _M0TUssE*)_M0L3bufS2040[_M0L2__S763];
      moonbit_string_t _M0L6_2atmpS2038 = _M0L4itemS764->$0;
      moonbit_string_t _M0L6_2atmpS2036;
      moonbit_string_t _M0L6_2atmpS2037;
      moonbit_string_t _M0L6_2atmpS2035;
      int32_t _M0L6_2atmpS2039;
      #line 61 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
      _M0L6_2atmpS2036
      = moonbit_add_string(_M0L6_2atmpS2038, (moonbit_string_t)moonbit_string_literal_84.data);
      _M0L6_2atmpS2037 = _M0L4itemS764->$1;
      #line 61 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
      _M0L6_2atmpS2035
      = moonbit_add_string(_M0L6_2atmpS2036, _M0L6_2atmpS2037);
      moonbit_decref(_M0L6_2atmpS2036);
      moonbit_incref(_M0L5partsS761);
      #line 61 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5partsS761, _M0L6_2atmpS2035);
      _M0L6_2atmpS2039 = _M0L2__S763 + 1;
      _M0L2__S763 = _M0L6_2atmpS2039;
      continue;
    } else {
      moonbit_decref(_M0L6errorsS760);
    }
    break;
  }
  _M0L7_2abindS766 = (moonbit_string_t)moonbit_string_literal_85.data;
  _M0L6_2atmpS2043 = Moonbit_array_length(_M0L7_2abindS766);
  _M0L6_2atmpS2042
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2043, _M0L7_2abindS766
  };
  #line 63 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _M0L6_2atmpS2041
  = _M0MPC15array5Array4joinGsE(_M0L5partsS761, _M0L6_2atmpS2042);
  #line 63 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3275
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_86.data, _M0L6_2atmpS2041);
  moonbit_decref(_M0L6_2atmpS2041);
  return _result_3275;
  join_757:;
  #line 65 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3276
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_87.data, _M0L6detailS758);
  moonbit_decref(_M0L6detailS758);
  return _result_3276;
  join_755:;
  #line 66 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3277
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_88.data, _M0L6detailS756);
  moonbit_decref(_M0L6detailS756);
  return _result_3277;
  join_753:;
  #line 67 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3278
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_89.data, _M0L6detailS754);
  moonbit_decref(_M0L6detailS754);
  return _result_3278;
  join_751:;
  #line 68 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3279
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_90.data, _M0L6detailS752);
  moonbit_decref(_M0L6detailS752);
  return _result_3279;
  join_749:;
  #line 69 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _result_3280
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_91.data, _M0L6detailS750);
  moonbit_decref(_M0L6detailS750);
  return _result_3280;
}

void* _M0FP36mulpjs4mulp4core13stream__error(
  moonbit_string_t _M0L6detailS748
) {
  void* _block_3281;
  #line 44 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _block_3281
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError));
  Moonbit_object_header(_block_3281)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError, $0) >> 2, 1, 6);
  ((struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError*)_block_3281)->$0
  = _M0L6detailS748;
  return _block_3281;
}

int32_t _M0MP36mulpjs4mulp4core7Context13is__cancelled(
  struct _M0TP36mulpjs4mulp4core7Context* _M0L4selfS747
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L8_2afieldS2758;
  int32_t _M0L6_2acntS3053;
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS2034;
  #line 38 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _M0L8_2afieldS2758 = _M0L4selfS747->$2;
  _M0L6_2acntS3053 = Moonbit_object_header(_M0L4selfS747)->rc;
  if (_M0L6_2acntS3053 > 1) {
    int32_t _M0L11_2anew__cntS3055 = _M0L6_2acntS3053 - 1;
    Moonbit_object_header(_M0L4selfS747)->rc = _M0L11_2anew__cntS3055;
    moonbit_incref(_M0L8_2afieldS2758);
  } else if (_M0L6_2acntS3053 == 1) {
    moonbit_string_t _M0L8_2afieldS3054 = _M0L4selfS747->$0;
    moonbit_decref(_M0L8_2afieldS3054);
    #line 39 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
    moonbit_free(_M0L4selfS747);
  }
  _M0L12cancellationS2034 = _M0L8_2afieldS2758;
  #line 39 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  return _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(_M0L12cancellationS2034);
}

int32_t _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L4selfS746
) {
  int32_t _result_3282;
  #line 17 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _result_3282 = _M0L4selfS746->$0;
  moonbit_decref(_M0L4selfS746);
  return _result_3282;
}

void* _M0FP36mulpjs4mulp4core12task__failed(
  moonbit_string_t _M0L4nameS744,
  moonbit_string_t _M0L5causeS745
) {
  void* _block_3283;
  #line 19 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _block_3283
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed));
  Moonbit_object_header(_block_3283)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed, $0) >> 2, 2, 1);
  ((struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed*)_block_3283)->$0
  = _M0L4nameS744;
  ((struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed*)_block_3283)->$1
  = _M0L5causeS745;
  return _block_3283;
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS742,
  struct _M0TPC16string10StringView _M0L9separatorS743
) {
  moonbit_string_t* _M0L3bufS2032;
  int32_t _M0L3lenS2033;
  int32_t _M0L6_2acntS3056;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2031;
  #line 2070 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS2032 = _M0L4selfS742->$0;
  _M0L3lenS2033 = _M0L4selfS742->$1;
  _M0L6_2acntS3056 = Moonbit_object_header(_M0L4selfS742)->rc;
  if (_M0L6_2acntS3056 > 1) {
    int32_t _M0L11_2anew__cntS3057 = _M0L6_2acntS3056 - 1;
    Moonbit_object_header(_M0L4selfS742)->rc = _M0L11_2anew__cntS3057;
    moonbit_incref(_M0L3bufS2032);
  } else if (_M0L6_2acntS3056 == 1) {
    #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS742);
  }
  _M0L6_2atmpS2031
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS2033, _M0L3bufS2032
  };
  #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS2031, _M0L9separatorS743);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS737,
  int32_t _M0L5indexS738
) {
  int32_t _M0L3lenS736;
  int32_t _if__result_3284;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS736 = _M0L4selfS737->$1;
  if (_M0L5indexS738 >= 0) {
    _if__result_3284 = _M0L5indexS738 < _M0L3lenS736;
  } else {
    _if__result_3284 = 0;
  }
  if (_if__result_3284) {
    moonbit_string_t* _M0L6_2atmpS2029;
    moonbit_string_t _M0L6_2atmpS2760;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS2029 = _M0MPC15array5Array6bufferGsE(_M0L4selfS737);
    if (
      _M0L5indexS738 < 0
      || _M0L5indexS738 >= Moonbit_array_length(_M0L6_2atmpS2029)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2760 = (moonbit_string_t)_M0L6_2atmpS2029[_M0L5indexS738];
    moonbit_incref(_M0L6_2atmpS2760);
    moonbit_decref(_M0L6_2atmpS2029);
    return _M0L6_2atmpS2760;
  } else {
    moonbit_decref(_M0L4selfS737);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS740,
  int32_t _M0L5indexS741
) {
  int32_t _M0L3lenS739;
  int32_t _if__result_3285;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS739 = _M0L4selfS740->$1;
  if (_M0L5indexS741 >= 0) {
    _if__result_3285 = _M0L5indexS741 < _M0L3lenS739;
  } else {
    _if__result_3285 = 0;
  }
  if (_if__result_3285) {
    struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2030;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS2761;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS2030
    = _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(_M0L4selfS740);
    if (
      _M0L5indexS741 < 0
      || _M0L5indexS741 >= Moonbit_array_length(_M0L6_2atmpS2030)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2761
    = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L6_2atmpS2030[
        _M0L5indexS741
      ];
    if (_M0L6_2atmpS2761) {
      moonbit_incref(_M0L6_2atmpS2761);
    }
    moonbit_decref(_M0L6_2atmpS2030);
    return _M0L6_2atmpS2761;
  } else {
    moonbit_decref(_M0L4selfS740);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS711,
  struct _M0TPC16string10StringView _M0L9separatorS723
) {
  int32_t _M0L3endS2008;
  int32_t _M0L5startS2009;
  int32_t _M0L6_2atmpS2007;
  #line 1369 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2008 = _M0L4selfS711.$2;
  _M0L5startS2009 = _M0L4selfS711.$1;
  _M0L6_2atmpS2007 = _M0L3endS2008 - _M0L5startS2009;
  if (_M0L6_2atmpS2007 == 0) {
    moonbit_decref(_M0L9separatorS723.$0);
    moonbit_decref(_M0L4selfS711.$0);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t* _M0L3bufS2027 = _M0L4selfS711.$0;
    int32_t _M0L5startS2028 = _M0L4selfS711.$1;
    moonbit_string_t _M0L5_2ahdS712 =
      (moonbit_string_t)_M0L3bufS2027[_M0L5startS2028];
    moonbit_string_t* _M0L9_2ax__bufS713 = _M0L4selfS711.$0;
    int32_t _M0L5startS2026 = _M0L4selfS711.$1;
    int32_t _M0L11_2ax__startS714 = 1 + _M0L5startS2026;
    int32_t _M0L9_2ax__endS715 = _M0L4selfS711.$2;
    struct _M0TPC16string10StringView _M0L2hdS716;
    int32_t _M0L7_2abindS717;
    int32_t _M0L6_2atmpS2025;
    int32_t _M0L10size__hintS718;
    int32_t _M0L2__S719;
    int32_t _M0L10size__hintS720;
    int32_t _M0L10size__hintS724;
    struct _M0TPB13StringBuilder* _M0L3bufS725;
    moonbit_string_t _M0L3strS2010;
    int32_t _M0L5startS2011;
    int32_t _M0L3endS2013;
    int64_t _M0L6_2atmpS2012;
    moonbit_incref(_M0L5_2ahdS712);
    #line 1376 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS716
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS712);
    _M0L7_2abindS717 = _M0L9_2ax__endS715 - _M0L11_2ax__startS714;
    moonbit_incref(_M0L2hdS716.$0);
    #line 1377 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L6_2atmpS2025 = _M0MPC16string10StringView6length(_M0L2hdS716);
    _M0L2__S719 = 0;
    _M0L10size__hintS720 = _M0L6_2atmpS2025;
    while (1) {
      if (_M0L2__S719 < _M0L7_2abindS717) {
        int32_t _M0L6_2atmpS2024 = _M0L11_2ax__startS714 + _M0L2__S719;
        moonbit_string_t _M0L1sS721 =
          (moonbit_string_t)_M0L9_2ax__bufS713[_M0L6_2atmpS2024];
        int32_t _M0L6_2atmpS2018 = _M0L2__S719 + 1;
        struct _M0TPC16string10StringView _M0L6_2atmpS2023;
        int32_t _M0L6_2atmpS2022;
        int32_t _M0L6_2atmpS2020;
        int32_t _M0L6_2atmpS2021;
        int32_t _M0L6_2atmpS2019;
        moonbit_incref(_M0L1sS721);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS2023
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS721);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS2022
        = _M0MPC16string10StringView6length(_M0L6_2atmpS2023);
        _M0L6_2atmpS2020 = _M0L10size__hintS720 + _M0L6_2atmpS2022;
        moonbit_incref(_M0L9separatorS723.$0);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS2021
        = _M0MPC16string10StringView6length(_M0L9separatorS723);
        _M0L6_2atmpS2019 = _M0L6_2atmpS2020 + _M0L6_2atmpS2021;
        _M0L2__S719 = _M0L6_2atmpS2018;
        _M0L10size__hintS720 = _M0L6_2atmpS2019;
        continue;
      } else {
        _M0L10size__hintS718 = _M0L10size__hintS720;
      }
      break;
    }
    _M0L10size__hintS724 = _M0L10size__hintS718 << 1;
    #line 1383 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS725 = _M0MPB13StringBuilder11new_2einner(_M0L10size__hintS724);
    moonbit_incref(_M0L3bufS725);
    #line 1385 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS725, _M0L2hdS716);
    _M0L3strS2010 = _M0L9separatorS723.$0;
    _M0L5startS2011 = _M0L9separatorS723.$1;
    _M0L3endS2013 = _M0L9separatorS723.$2;
    _M0L6_2atmpS2012 = (int64_t)_M0L3endS2013;
    moonbit_incref(_M0L3strS2010);
    #line 1386 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2010, 0, _M0L5startS2011, _M0L6_2atmpS2012)
    ) {
      int32_t _M0L7_2abindS726;
      int32_t _M0L2__S727;
      moonbit_decref(_M0L9separatorS723.$0);
      _M0L7_2abindS726 = _M0L9_2ax__endS715 - _M0L11_2ax__startS714;
      _M0L2__S727 = 0;
      while (1) {
        if (_M0L2__S727 < _M0L7_2abindS726) {
          int32_t _M0L6_2atmpS2015 = _M0L11_2ax__startS714 + _M0L2__S727;
          moonbit_string_t _M0L1sS728 =
            (moonbit_string_t)_M0L9_2ax__bufS713[_M0L6_2atmpS2015];
          struct _M0TPC16string10StringView _M0L1sS729;
          int32_t _M0L6_2atmpS2014;
          moonbit_incref(_M0L1sS728);
          #line 1389 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS729
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS728);
          moonbit_incref(_M0L3bufS725);
          #line 1390 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS725, _M0L1sS729);
          _M0L6_2atmpS2014 = _M0L2__S727 + 1;
          _M0L2__S727 = _M0L6_2atmpS2014;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS713);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS731 = _M0L9_2ax__endS715 - _M0L11_2ax__startS714;
      int32_t _M0L2__S732 = 0;
      while (1) {
        if (_M0L2__S732 < _M0L7_2abindS731) {
          int32_t _M0L6_2atmpS2017 = _M0L11_2ax__startS714 + _M0L2__S732;
          moonbit_string_t _M0L1sS733 =
            (moonbit_string_t)_M0L9_2ax__bufS713[_M0L6_2atmpS2017];
          struct _M0TPC16string10StringView _M0L1sS734;
          int32_t _M0L6_2atmpS2016;
          moonbit_incref(_M0L1sS733);
          #line 1394 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS734
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS733);
          moonbit_incref(_M0L3bufS725);
          moonbit_incref(_M0L9separatorS723.$0);
          #line 1395 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS725, _M0L9separatorS723);
          moonbit_incref(_M0L3bufS725);
          #line 1397 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS725, _M0L1sS734);
          _M0L6_2atmpS2016 = _M0L2__S732 + 1;
          _M0L2__S732 = _M0L6_2atmpS2016;
          continue;
        } else {
          moonbit_decref(_M0L9separatorS723.$0);
          moonbit_decref(_M0L9_2ax__bufS713);
        }
        break;
      }
    }
    #line 1400 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS725);
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS710) {
  moonbit_string_t _M0L6_2atmpS2006;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS2006 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS710);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS2006);
  moonbit_decref(_M0L6_2atmpS2006);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS709,
  struct _M0TPB6Hasher* _M0L6hasherS708
) {
  #line 530 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 531 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS708, _M0L4selfS709);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS707,
  struct _M0TPB6Hasher* _M0L6hasherS706
) {
  #line 496 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 497 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS706, _M0L4selfS707);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS704,
  moonbit_string_t _M0L5valueS702
) {
  int32_t _M0L7_2abindS701;
  int32_t _M0L1iS703;
  #line 387 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS701 = Moonbit_array_length(_M0L5valueS702);
  _M0L1iS703 = 0;
  while (1) {
    if (_M0L1iS703 < _M0L7_2abindS701) {
      int32_t _M0L6_2atmpS2004 = _M0L5valueS702[_M0L1iS703];
      int32_t _M0L6_2atmpS2003 = (int32_t)_M0L6_2atmpS2004;
      uint32_t _M0L6_2atmpS2002 = *(uint32_t*)&_M0L6_2atmpS2003;
      int32_t _M0L6_2atmpS2005;
      moonbit_incref(_M0L4selfS704);
      #line 389 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS704, _M0L6_2atmpS2002);
      _M0L6_2atmpS2005 = _M0L1iS703 + 1;
      _M0L1iS703 = _M0L6_2atmpS2005;
      continue;
    } else {
      moonbit_decref(_M0L4selfS704);
      moonbit_decref(_M0L5valueS702);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS699,
  int32_t _M0L3idxS700
) {
  int32_t _result_3290;
  #line 1778 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _result_3290 = _M0L4selfS699[_M0L3idxS700];
  moonbit_decref(_M0L4selfS699);
  return _result_3290;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS686,
  int32_t _M0L3keyS682
) {
  int32_t _M0L4hashS681;
  int32_t _M0L14capacity__maskS1987;
  int32_t _M0L6_2atmpS1986;
  int32_t _M0L1iS683;
  int32_t _M0L3idxS684;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS681 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS682);
  _M0L14capacity__maskS1987 = _M0L4selfS686->$3;
  _M0L6_2atmpS1986 = _M0L4hashS681 & _M0L14capacity__maskS1987;
  _M0L1iS683 = 0;
  _M0L3idxS684 = _M0L6_2atmpS1986;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1985 =
      _M0L4selfS686->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS685;
    if (
      _M0L3idxS684 < 0
      || _M0L3idxS684 >= Moonbit_array_length(_M0L7entriesS1985)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS685
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1985[
        _M0L3idxS684
      ];
    if (_M0L7_2abindS685 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1974;
      if (_M0L7_2abindS685) {
        moonbit_incref(_M0L7_2abindS685);
      }
      moonbit_decref(_M0L4selfS686);
      if (_M0L7_2abindS685) {
        moonbit_decref(_M0L7_2abindS685);
      }
      _M0L6_2atmpS1974 = 0;
      return _M0L6_2atmpS1974;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS687 =
        _M0L7_2abindS685;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS688 =
        _M0L7_2aSomeS687;
      int32_t _M0L4hashS1976 = _M0L8_2aentryS688->$3;
      int32_t _if__result_3292;
      int32_t _M0L3pslS1979;
      int32_t _M0L6_2atmpS1981;
      int32_t _M0L6_2atmpS1983;
      int32_t _M0L14capacity__maskS1984;
      int32_t _M0L6_2atmpS1982;
      if (_M0L4hashS1976 == _M0L4hashS681) {
        int32_t _M0L3keyS1975 = _M0L8_2aentryS688->$4;
        _if__result_3292 = _M0L3keyS1975 == _M0L3keyS682;
      } else {
        _if__result_3292 = 0;
      }
      if (_if__result_3292) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2769;
        int32_t _M0L6_2acntS3058;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1978;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1977;
        moonbit_incref(_M0L8_2aentryS688);
        moonbit_decref(_M0L4selfS686);
        _M0L8_2afieldS2769 = _M0L8_2aentryS688->$5;
        _M0L6_2acntS3058 = Moonbit_object_header(_M0L8_2aentryS688)->rc;
        if (_M0L6_2acntS3058 > 1) {
          int32_t _M0L11_2anew__cntS3060 = _M0L6_2acntS3058 - 1;
          Moonbit_object_header(_M0L8_2aentryS688)->rc
          = _M0L11_2anew__cntS3060;
          moonbit_incref(_M0L8_2afieldS2769);
        } else if (_M0L6_2acntS3058 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3059 =
            _M0L8_2aentryS688->$1;
          if (_M0L8_2afieldS3059) {
            moonbit_decref(_M0L8_2afieldS3059);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS688);
        }
        _M0L5valueS1978 = _M0L8_2afieldS2769;
        _M0L6_2atmpS1977 = _M0L5valueS1978;
        return _M0L6_2atmpS1977;
      } else {
        moonbit_incref(_M0L8_2aentryS688);
      }
      _M0L3pslS1979 = _M0L8_2aentryS688->$2;
      moonbit_decref(_M0L8_2aentryS688);
      if (_M0L1iS683 > _M0L3pslS1979) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1980;
        moonbit_decref(_M0L4selfS686);
        _M0L6_2atmpS1980 = 0;
        return _M0L6_2atmpS1980;
      }
      _M0L6_2atmpS1981 = _M0L1iS683 + 1;
      _M0L6_2atmpS1983 = _M0L3idxS684 + 1;
      _M0L14capacity__maskS1984 = _M0L4selfS686->$3;
      _M0L6_2atmpS1982 = _M0L6_2atmpS1983 & _M0L14capacity__maskS1984;
      _M0L1iS683 = _M0L6_2atmpS1981;
      _M0L3idxS684 = _M0L6_2atmpS1982;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS695,
  moonbit_string_t _M0L3keyS691
) {
  int32_t _M0L4hashS690;
  int32_t _M0L14capacity__maskS2001;
  int32_t _M0L6_2atmpS2000;
  int32_t _M0L1iS692;
  int32_t _M0L3idxS693;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS691);
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS690 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS691);
  _M0L14capacity__maskS2001 = _M0L4selfS695->$3;
  _M0L6_2atmpS2000 = _M0L4hashS690 & _M0L14capacity__maskS2001;
  _M0L1iS692 = 0;
  _M0L3idxS693 = _M0L6_2atmpS2000;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1999 =
      _M0L4selfS695->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS694;
    if (
      _M0L3idxS693 < 0
      || _M0L3idxS693 >= Moonbit_array_length(_M0L7entriesS1999)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS694
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1999[
        _M0L3idxS693
      ];
    if (_M0L7_2abindS694 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1988;
      if (_M0L7_2abindS694) {
        moonbit_incref(_M0L7_2abindS694);
      }
      moonbit_decref(_M0L4selfS695);
      if (_M0L7_2abindS694) {
        moonbit_decref(_M0L7_2abindS694);
      }
      moonbit_decref(_M0L3keyS691);
      _M0L6_2atmpS1988 = 0;
      return _M0L6_2atmpS1988;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS696 =
        _M0L7_2abindS694;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS697 =
        _M0L7_2aSomeS696;
      int32_t _M0L4hashS1990 = _M0L8_2aentryS697->$3;
      int32_t _if__result_3294;
      int32_t _M0L3pslS1993;
      int32_t _M0L6_2atmpS1995;
      int32_t _M0L6_2atmpS1997;
      int32_t _M0L14capacity__maskS1998;
      int32_t _M0L6_2atmpS1996;
      if (_M0L4hashS1990 == _M0L4hashS690) {
        moonbit_string_t _M0L3keyS1989 = _M0L8_2aentryS697->$4;
        #line 220 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3294
        = moonbit_val_array_equal(_M0L3keyS1989, _M0L3keyS691);
      } else {
        _if__result_3294 = 0;
      }
      if (_if__result_3294) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2772;
        int32_t _M0L6_2acntS3061;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1992;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1991;
        moonbit_incref(_M0L8_2aentryS697);
        moonbit_decref(_M0L4selfS695);
        moonbit_decref(_M0L3keyS691);
        _M0L8_2afieldS2772 = _M0L8_2aentryS697->$5;
        _M0L6_2acntS3061 = Moonbit_object_header(_M0L8_2aentryS697)->rc;
        if (_M0L6_2acntS3061 > 1) {
          int32_t _M0L11_2anew__cntS3064 = _M0L6_2acntS3061 - 1;
          Moonbit_object_header(_M0L8_2aentryS697)->rc
          = _M0L11_2anew__cntS3064;
          moonbit_incref(_M0L8_2afieldS2772);
        } else if (_M0L6_2acntS3061 == 1) {
          moonbit_string_t _M0L8_2afieldS3063 = _M0L8_2aentryS697->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3062;
          moonbit_decref(_M0L8_2afieldS3063);
          _M0L8_2afieldS3062 = _M0L8_2aentryS697->$1;
          if (_M0L8_2afieldS3062) {
            moonbit_decref(_M0L8_2afieldS3062);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS697);
        }
        _M0L5valueS1992 = _M0L8_2afieldS2772;
        _M0L6_2atmpS1991 = _M0L5valueS1992;
        return _M0L6_2atmpS1991;
      } else {
        moonbit_incref(_M0L8_2aentryS697);
      }
      _M0L3pslS1993 = _M0L8_2aentryS697->$2;
      moonbit_decref(_M0L8_2aentryS697);
      if (_M0L1iS692 > _M0L3pslS1993) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1994;
        moonbit_decref(_M0L4selfS695);
        moonbit_decref(_M0L3keyS691);
        _M0L6_2atmpS1994 = 0;
        return _M0L6_2atmpS1994;
      }
      _M0L6_2atmpS1995 = _M0L1iS692 + 1;
      _M0L6_2atmpS1997 = _M0L3idxS693 + 1;
      _M0L14capacity__maskS1998 = _M0L4selfS695->$3;
      _M0L6_2atmpS1996 = _M0L6_2atmpS1997 & _M0L14capacity__maskS1998;
      _M0L1iS692 = _M0L6_2atmpS1995;
      _M0L3idxS693 = _M0L6_2atmpS1996;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS666
) {
  int32_t _M0L6lengthS665;
  int32_t _M0Lm8capacityS667;
  int32_t _M0L6_2atmpS1951;
  int32_t _M0L6_2atmpS1950;
  int32_t _M0L6_2atmpS1961;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS668;
  int32_t _M0L3endS1959;
  int32_t _M0L5startS1960;
  int32_t _M0L7_2abindS669;
  int32_t _M0L2__S670;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS666.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS665
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS666);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS667 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS665);
  _M0L6_2atmpS1951 = _M0Lm8capacityS667;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1950 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1951);
  if (_M0L6lengthS665 > _M0L6_2atmpS1950) {
    int32_t _M0L6_2atmpS1952 = _M0Lm8capacityS667;
    _M0Lm8capacityS667 = _M0L6_2atmpS1952 * 2;
  }
  _M0L6_2atmpS1961 = _M0Lm8capacityS667;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS668
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1961);
  _M0L3endS1959 = _M0L3arrS666.$2;
  _M0L5startS1960 = _M0L3arrS666.$1;
  _M0L7_2abindS669 = _M0L3endS1959 - _M0L5startS1960;
  _M0L2__S670 = 0;
  while (1) {
    if (_M0L2__S670 < _M0L7_2abindS669) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1956 =
        _M0L3arrS666.$0;
      int32_t _M0L5startS1958 = _M0L3arrS666.$1;
      int32_t _M0L6_2atmpS1957 = _M0L5startS1958 + _M0L2__S670;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS671 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1956[
          _M0L6_2atmpS1957
        ];
      moonbit_string_t _M0L6_2atmpS1953 = _M0L1eS671->$0;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1954 =
        _M0L1eS671->$1;
      int32_t _M0L6_2atmpS1955;
      moonbit_incref(_M0L6_2atmpS1954);
      moonbit_incref(_M0L6_2atmpS1953);
      moonbit_incref(_M0L1mS668);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS668, _M0L6_2atmpS1953, _M0L6_2atmpS1954);
      _M0L6_2atmpS1955 = _M0L2__S670 + 1;
      _M0L2__S670 = _M0L6_2atmpS1955;
      continue;
    } else {
      moonbit_decref(_M0L3arrS666.$0);
    }
    break;
  }
  return _M0L1mS668;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS674
) {
  int32_t _M0L6lengthS673;
  int32_t _M0Lm8capacityS675;
  int32_t _M0L6_2atmpS1963;
  int32_t _M0L6_2atmpS1962;
  int32_t _M0L6_2atmpS1973;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS676;
  int32_t _M0L3endS1971;
  int32_t _M0L5startS1972;
  int32_t _M0L7_2abindS677;
  int32_t _M0L2__S678;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS674.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS673
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS674);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS675 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS673);
  _M0L6_2atmpS1963 = _M0Lm8capacityS675;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1962 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1963);
  if (_M0L6lengthS673 > _M0L6_2atmpS1962) {
    int32_t _M0L6_2atmpS1964 = _M0Lm8capacityS675;
    _M0Lm8capacityS675 = _M0L6_2atmpS1964 * 2;
  }
  _M0L6_2atmpS1973 = _M0Lm8capacityS675;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS676
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1973);
  _M0L3endS1971 = _M0L3arrS674.$2;
  _M0L5startS1972 = _M0L3arrS674.$1;
  _M0L7_2abindS677 = _M0L3endS1971 - _M0L5startS1972;
  _M0L2__S678 = 0;
  while (1) {
    if (_M0L2__S678 < _M0L7_2abindS677) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1968 =
        _M0L3arrS674.$0;
      int32_t _M0L5startS1970 = _M0L3arrS674.$1;
      int32_t _M0L6_2atmpS1969 = _M0L5startS1970 + _M0L2__S678;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS679 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1968[
          _M0L6_2atmpS1969
        ];
      int32_t _M0L6_2atmpS1965 = _M0L1eS679->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1966 = _M0L1eS679->$1;
      int32_t _M0L6_2atmpS1967;
      moonbit_incref(_M0L6_2atmpS1966);
      moonbit_incref(_M0L1mS676);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS676, _M0L6_2atmpS1965, _M0L6_2atmpS1966);
      _M0L6_2atmpS1967 = _M0L2__S678 + 1;
      _M0L2__S678 = _M0L6_2atmpS1967;
      continue;
    } else {
      moonbit_decref(_M0L3arrS674.$0);
    }
    break;
  }
  return _M0L1mS676;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS659,
  moonbit_string_t _M0L3keyS660,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS661
) {
  int32_t _M0L6_2atmpS1948;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS660);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1948 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS660);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS659, _M0L3keyS660, _M0L5valueS661, _M0L6_2atmpS1948);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS662,
  int32_t _M0L3keyS663,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS664
) {
  int32_t _M0L6_2atmpS1949;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1949 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS663);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS662, _M0L3keyS663, _M0L5valueS664, _M0L6_2atmpS1949);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS638
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS637;
  int32_t _M0L8capacityS1940;
  int32_t _M0L13new__capacityS639;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1935;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1934;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2787;
  int32_t _M0L6_2atmpS1936;
  int32_t _M0L8capacityS1938;
  int32_t _M0L6_2atmpS1937;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1939;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2786;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1xS640;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS637 = _M0L4selfS638->$5;
  _M0L8capacityS1940 = _M0L4selfS638->$2;
  _M0L13new__capacityS639 = _M0L8capacityS1940 << 1;
  _M0L6_2atmpS1935 = 0;
  _M0L6_2atmpS1934
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS639, _M0L6_2atmpS1935);
  _M0L6_2aoldS2787 = _M0L4selfS638->$0;
  if (_M0L9old__headS637) {
    moonbit_incref(_M0L9old__headS637);
  }
  moonbit_decref(_M0L6_2aoldS2787);
  _M0L4selfS638->$0 = _M0L6_2atmpS1934;
  _M0L4selfS638->$2 = _M0L13new__capacityS639;
  _M0L6_2atmpS1936 = _M0L13new__capacityS639 - 1;
  _M0L4selfS638->$3 = _M0L6_2atmpS1936;
  _M0L8capacityS1938 = _M0L4selfS638->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1937 = _M0FPB21calc__grow__threshold(_M0L8capacityS1938);
  _M0L4selfS638->$4 = _M0L6_2atmpS1937;
  _M0L4selfS638->$1 = 0;
  _M0L6_2atmpS1939 = 0;
  _M0L6_2aoldS2786 = _M0L4selfS638->$5;
  if (_M0L6_2aoldS2786) {
    moonbit_decref(_M0L6_2aoldS2786);
  }
  _M0L4selfS638->$5 = _M0L6_2atmpS1939;
  _M0L4selfS638->$6 = -1;
  _M0L1xS640 = _M0L9old__headS637;
  while (1) {
    if (_M0L1xS640 == 0) {
      if (_M0L1xS640) {
        moonbit_decref(_M0L1xS640);
      }
      moonbit_decref(_M0L4selfS638);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS641 =
        _M0L1xS640;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS642 =
        _M0L7_2aSomeS641;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS643 =
        _M0L4_2axS642->$1;
      moonbit_string_t _M0L6_2akeyS644 = _M0L4_2axS642->$4;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS645 =
        _M0L4_2axS642->$5;
      int32_t _M0L7_2ahashS646 = _M0L4_2axS642->$3;
      int32_t _M0L6_2acntS3065 = Moonbit_object_header(_M0L4_2axS642)->rc;
      if (_M0L6_2acntS3065 > 1) {
        int32_t _M0L11_2anew__cntS3066 = _M0L6_2acntS3065 - 1;
        Moonbit_object_header(_M0L4_2axS642)->rc = _M0L11_2anew__cntS3066;
        moonbit_incref(_M0L8_2avalueS645);
        moonbit_incref(_M0L6_2akeyS644);
        if (_M0L7_2anextS643) {
          moonbit_incref(_M0L7_2anextS643);
        }
      } else if (_M0L6_2acntS3065 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS642);
      }
      moonbit_incref(_M0L4selfS638);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS638, _M0L6_2akeyS644, _M0L8_2avalueS645, _M0L7_2ahashS646);
      _M0L1xS640 = _M0L7_2anextS643;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS649
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS648;
  int32_t _M0L8capacityS1947;
  int32_t _M0L13new__capacityS650;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1942;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1941;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2792;
  int32_t _M0L6_2atmpS1943;
  int32_t _M0L8capacityS1945;
  int32_t _M0L6_2atmpS1944;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1946;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2791;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L1xS651;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS648 = _M0L4selfS649->$5;
  _M0L8capacityS1947 = _M0L4selfS649->$2;
  _M0L13new__capacityS650 = _M0L8capacityS1947 << 1;
  _M0L6_2atmpS1942 = 0;
  _M0L6_2atmpS1941
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS650, _M0L6_2atmpS1942);
  _M0L6_2aoldS2792 = _M0L4selfS649->$0;
  if (_M0L9old__headS648) {
    moonbit_incref(_M0L9old__headS648);
  }
  moonbit_decref(_M0L6_2aoldS2792);
  _M0L4selfS649->$0 = _M0L6_2atmpS1941;
  _M0L4selfS649->$2 = _M0L13new__capacityS650;
  _M0L6_2atmpS1943 = _M0L13new__capacityS650 - 1;
  _M0L4selfS649->$3 = _M0L6_2atmpS1943;
  _M0L8capacityS1945 = _M0L4selfS649->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1944 = _M0FPB21calc__grow__threshold(_M0L8capacityS1945);
  _M0L4selfS649->$4 = _M0L6_2atmpS1944;
  _M0L4selfS649->$1 = 0;
  _M0L6_2atmpS1946 = 0;
  _M0L6_2aoldS2791 = _M0L4selfS649->$5;
  if (_M0L6_2aoldS2791) {
    moonbit_decref(_M0L6_2aoldS2791);
  }
  _M0L4selfS649->$5 = _M0L6_2atmpS1946;
  _M0L4selfS649->$6 = -1;
  _M0L1xS651 = _M0L9old__headS648;
  while (1) {
    if (_M0L1xS651 == 0) {
      if (_M0L1xS651) {
        moonbit_decref(_M0L1xS651);
      }
      moonbit_decref(_M0L4selfS649);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS652 =
        _M0L1xS651;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS653 =
        _M0L7_2aSomeS652;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS654 =
        _M0L4_2axS653->$1;
      int32_t _M0L6_2akeyS655 = _M0L4_2axS653->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS656 =
        _M0L4_2axS653->$5;
      int32_t _M0L7_2ahashS657 = _M0L4_2axS653->$3;
      int32_t _M0L6_2acntS3067 = Moonbit_object_header(_M0L4_2axS653)->rc;
      if (_M0L6_2acntS3067 > 1) {
        int32_t _M0L11_2anew__cntS3068 = _M0L6_2acntS3067 - 1;
        Moonbit_object_header(_M0L4_2axS653)->rc = _M0L11_2anew__cntS3068;
        moonbit_incref(_M0L8_2avalueS656);
        if (_M0L7_2anextS654) {
          moonbit_incref(_M0L7_2anextS654);
        }
      } else if (_M0L6_2acntS3067 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS653);
      }
      moonbit_incref(_M0L4selfS649);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS649, _M0L6_2akeyS655, _M0L8_2avalueS656, _M0L7_2ahashS657);
      _M0L1xS651 = _M0L7_2anextS654;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS608,
  moonbit_string_t _M0L3keyS614,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS615,
  int32_t _M0L4hashS610
) {
  int32_t _M0L14capacity__maskS1915;
  int32_t _M0L6_2atmpS1914;
  int32_t _M0L3pslS605;
  int32_t _M0L3idxS606;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1915 = _M0L4selfS608->$3;
  _M0L6_2atmpS1914 = _M0L4hashS610 & _M0L14capacity__maskS1915;
  _M0L3pslS605 = 0;
  _M0L3idxS606 = _M0L6_2atmpS1914;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1913 =
      _M0L4selfS608->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS607;
    if (
      _M0L3idxS606 < 0
      || _M0L3idxS606 >= Moonbit_array_length(_M0L7entriesS1913)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS607
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1913[
        _M0L3idxS606
      ];
    if (_M0L7_2abindS607 == 0) {
      int32_t _M0L4sizeS1898 = _M0L4selfS608->$1;
      int32_t _M0L8grow__atS1899 = _M0L4selfS608->$4;
      int32_t _M0L7_2abindS611;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS612;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS613;
      if (_M0L4sizeS1898 >= _M0L8grow__atS1899) {
        int32_t _M0L14capacity__maskS1901;
        int32_t _M0L6_2atmpS1900;
        moonbit_incref(_M0L4selfS608);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS608);
        _M0L14capacity__maskS1901 = _M0L4selfS608->$3;
        _M0L6_2atmpS1900 = _M0L4hashS610 & _M0L14capacity__maskS1901;
        _M0L3pslS605 = 0;
        _M0L3idxS606 = _M0L6_2atmpS1900;
        continue;
      }
      _M0L7_2abindS611 = _M0L4selfS608->$6;
      _M0L7_2abindS612 = 0;
      _M0L5entryS613
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS613)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS613->$0 = _M0L7_2abindS611;
      _M0L5entryS613->$1 = _M0L7_2abindS612;
      _M0L5entryS613->$2 = _M0L3pslS605;
      _M0L5entryS613->$3 = _M0L4hashS610;
      _M0L5entryS613->$4 = _M0L3keyS614;
      _M0L5entryS613->$5 = _M0L5valueS615;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS608, _M0L3idxS606, _M0L5entryS613);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS616 =
        _M0L7_2abindS607;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS617 =
        _M0L7_2aSomeS616;
      int32_t _M0L4hashS1903 = _M0L14_2acurr__entryS617->$3;
      int32_t _if__result_3300;
      int32_t _M0L3pslS1904;
      int32_t _M0L6_2atmpS1909;
      int32_t _M0L6_2atmpS1911;
      int32_t _M0L14capacity__maskS1912;
      int32_t _M0L6_2atmpS1910;
      if (_M0L4hashS1903 == _M0L4hashS610) {
        moonbit_string_t _M0L3keyS1902 = _M0L14_2acurr__entryS617->$4;
        #line 134 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_3300
        = moonbit_val_array_equal(_M0L3keyS1902, _M0L3keyS614);
      } else {
        _if__result_3300 = 0;
      }
      if (_if__result_3300) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2794;
        moonbit_incref(_M0L14_2acurr__entryS617);
        moonbit_decref(_M0L3keyS614);
        moonbit_decref(_M0L4selfS608);
        _M0L6_2aoldS2794 = _M0L14_2acurr__entryS617->$5;
        moonbit_decref(_M0L6_2aoldS2794);
        _M0L14_2acurr__entryS617->$5 = _M0L5valueS615;
        moonbit_decref(_M0L14_2acurr__entryS617);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS617);
      }
      _M0L3pslS1904 = _M0L14_2acurr__entryS617->$2;
      if (_M0L3pslS605 > _M0L3pslS1904) {
        int32_t _M0L4sizeS1905 = _M0L4selfS608->$1;
        int32_t _M0L8grow__atS1906 = _M0L4selfS608->$4;
        int32_t _M0L7_2abindS618;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS619;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS620;
        if (_M0L4sizeS1905 >= _M0L8grow__atS1906) {
          int32_t _M0L14capacity__maskS1908;
          int32_t _M0L6_2atmpS1907;
          moonbit_decref(_M0L14_2acurr__entryS617);
          moonbit_incref(_M0L4selfS608);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS608);
          _M0L14capacity__maskS1908 = _M0L4selfS608->$3;
          _M0L6_2atmpS1907 = _M0L4hashS610 & _M0L14capacity__maskS1908;
          _M0L3pslS605 = 0;
          _M0L3idxS606 = _M0L6_2atmpS1907;
          continue;
        }
        moonbit_incref(_M0L4selfS608);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS608, _M0L3idxS606, _M0L14_2acurr__entryS617);
        _M0L7_2abindS618 = _M0L4selfS608->$6;
        _M0L7_2abindS619 = 0;
        _M0L5entryS620
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS620)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS620->$0 = _M0L7_2abindS618;
        _M0L5entryS620->$1 = _M0L7_2abindS619;
        _M0L5entryS620->$2 = _M0L3pslS605;
        _M0L5entryS620->$3 = _M0L4hashS610;
        _M0L5entryS620->$4 = _M0L3keyS614;
        _M0L5entryS620->$5 = _M0L5valueS615;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS608, _M0L3idxS606, _M0L5entryS620);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS617);
      }
      _M0L6_2atmpS1909 = _M0L3pslS605 + 1;
      _M0L6_2atmpS1911 = _M0L3idxS606 + 1;
      _M0L14capacity__maskS1912 = _M0L4selfS608->$3;
      _M0L6_2atmpS1910 = _M0L6_2atmpS1911 & _M0L14capacity__maskS1912;
      _M0L3pslS605 = _M0L6_2atmpS1909;
      _M0L3idxS606 = _M0L6_2atmpS1910;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS624,
  int32_t _M0L3keyS630,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS631,
  int32_t _M0L4hashS626
) {
  int32_t _M0L14capacity__maskS1933;
  int32_t _M0L6_2atmpS1932;
  int32_t _M0L3pslS621;
  int32_t _M0L3idxS622;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1933 = _M0L4selfS624->$3;
  _M0L6_2atmpS1932 = _M0L4hashS626 & _M0L14capacity__maskS1933;
  _M0L3pslS621 = 0;
  _M0L3idxS622 = _M0L6_2atmpS1932;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1931 =
      _M0L4selfS624->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS623;
    if (
      _M0L3idxS622 < 0
      || _M0L3idxS622 >= Moonbit_array_length(_M0L7entriesS1931)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS623
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1931[
        _M0L3idxS622
      ];
    if (_M0L7_2abindS623 == 0) {
      int32_t _M0L4sizeS1916 = _M0L4selfS624->$1;
      int32_t _M0L8grow__atS1917 = _M0L4selfS624->$4;
      int32_t _M0L7_2abindS627;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS628;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS629;
      if (_M0L4sizeS1916 >= _M0L8grow__atS1917) {
        int32_t _M0L14capacity__maskS1919;
        int32_t _M0L6_2atmpS1918;
        moonbit_incref(_M0L4selfS624);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624);
        _M0L14capacity__maskS1919 = _M0L4selfS624->$3;
        _M0L6_2atmpS1918 = _M0L4hashS626 & _M0L14capacity__maskS1919;
        _M0L3pslS621 = 0;
        _M0L3idxS622 = _M0L6_2atmpS1918;
        continue;
      }
      _M0L7_2abindS627 = _M0L4selfS624->$6;
      _M0L7_2abindS628 = 0;
      _M0L5entryS629
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS629)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS629->$0 = _M0L7_2abindS627;
      _M0L5entryS629->$1 = _M0L7_2abindS628;
      _M0L5entryS629->$2 = _M0L3pslS621;
      _M0L5entryS629->$3 = _M0L4hashS626;
      _M0L5entryS629->$4 = _M0L3keyS630;
      _M0L5entryS629->$5 = _M0L5valueS631;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624, _M0L3idxS622, _M0L5entryS629);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS632 =
        _M0L7_2abindS623;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS633 =
        _M0L7_2aSomeS632;
      int32_t _M0L4hashS1921 = _M0L14_2acurr__entryS633->$3;
      int32_t _if__result_3302;
      int32_t _M0L3pslS1922;
      int32_t _M0L6_2atmpS1927;
      int32_t _M0L6_2atmpS1929;
      int32_t _M0L14capacity__maskS1930;
      int32_t _M0L6_2atmpS1928;
      if (_M0L4hashS1921 == _M0L4hashS626) {
        int32_t _M0L3keyS1920 = _M0L14_2acurr__entryS633->$4;
        _if__result_3302 = _M0L3keyS1920 == _M0L3keyS630;
      } else {
        _if__result_3302 = 0;
      }
      if (_if__result_3302) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2798;
        moonbit_incref(_M0L14_2acurr__entryS633);
        moonbit_decref(_M0L4selfS624);
        _M0L6_2aoldS2798 = _M0L14_2acurr__entryS633->$5;
        moonbit_decref(_M0L6_2aoldS2798);
        _M0L14_2acurr__entryS633->$5 = _M0L5valueS631;
        moonbit_decref(_M0L14_2acurr__entryS633);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS633);
      }
      _M0L3pslS1922 = _M0L14_2acurr__entryS633->$2;
      if (_M0L3pslS621 > _M0L3pslS1922) {
        int32_t _M0L4sizeS1923 = _M0L4selfS624->$1;
        int32_t _M0L8grow__atS1924 = _M0L4selfS624->$4;
        int32_t _M0L7_2abindS634;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS635;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS636;
        if (_M0L4sizeS1923 >= _M0L8grow__atS1924) {
          int32_t _M0L14capacity__maskS1926;
          int32_t _M0L6_2atmpS1925;
          moonbit_decref(_M0L14_2acurr__entryS633);
          moonbit_incref(_M0L4selfS624);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624);
          _M0L14capacity__maskS1926 = _M0L4selfS624->$3;
          _M0L6_2atmpS1925 = _M0L4hashS626 & _M0L14capacity__maskS1926;
          _M0L3pslS621 = 0;
          _M0L3idxS622 = _M0L6_2atmpS1925;
          continue;
        }
        moonbit_incref(_M0L4selfS624);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624, _M0L3idxS622, _M0L14_2acurr__entryS633);
        _M0L7_2abindS634 = _M0L4selfS624->$6;
        _M0L7_2abindS635 = 0;
        _M0L5entryS636
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS636)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS636->$0 = _M0L7_2abindS634;
        _M0L5entryS636->$1 = _M0L7_2abindS635;
        _M0L5entryS636->$2 = _M0L3pslS621;
        _M0L5entryS636->$3 = _M0L4hashS626;
        _M0L5entryS636->$4 = _M0L3keyS630;
        _M0L5entryS636->$5 = _M0L5valueS631;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624, _M0L3idxS622, _M0L5entryS636);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS633);
      }
      _M0L6_2atmpS1927 = _M0L3pslS621 + 1;
      _M0L6_2atmpS1929 = _M0L3idxS622 + 1;
      _M0L14capacity__maskS1930 = _M0L4selfS624->$3;
      _M0L6_2atmpS1928 = _M0L6_2atmpS1929 & _M0L14capacity__maskS1930;
      _M0L3pslS621 = _M0L6_2atmpS1927;
      _M0L3idxS622 = _M0L6_2atmpS1928;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS589,
  int32_t _M0L3idxS594,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS593
) {
  int32_t _M0L3pslS1881;
  int32_t _M0L6_2atmpS1877;
  int32_t _M0L6_2atmpS1879;
  int32_t _M0L14capacity__maskS1880;
  int32_t _M0L6_2atmpS1878;
  int32_t _M0L3pslS585;
  int32_t _M0L3idxS586;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS587;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1881 = _M0L5entryS593->$2;
  _M0L6_2atmpS1877 = _M0L3pslS1881 + 1;
  _M0L6_2atmpS1879 = _M0L3idxS594 + 1;
  _M0L14capacity__maskS1880 = _M0L4selfS589->$3;
  _M0L6_2atmpS1878 = _M0L6_2atmpS1879 & _M0L14capacity__maskS1880;
  _M0L3pslS585 = _M0L6_2atmpS1877;
  _M0L3idxS586 = _M0L6_2atmpS1878;
  _M0L5entryS587 = _M0L5entryS593;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1876 =
      _M0L4selfS589->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS588;
    if (
      _M0L3idxS586 < 0
      || _M0L3idxS586 >= Moonbit_array_length(_M0L7entriesS1876)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS588
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1876[
        _M0L3idxS586
      ];
    if (_M0L7_2abindS588 == 0) {
      _M0L5entryS587->$2 = _M0L3pslS585;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS589, _M0L5entryS587, _M0L3idxS586);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS591 =
        _M0L7_2abindS588;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS592 =
        _M0L7_2aSomeS591;
      int32_t _M0L3pslS1866 = _M0L14_2acurr__entryS592->$2;
      if (_M0L3pslS585 > _M0L3pslS1866) {
        int32_t _M0L3pslS1871;
        int32_t _M0L6_2atmpS1867;
        int32_t _M0L6_2atmpS1869;
        int32_t _M0L14capacity__maskS1870;
        int32_t _M0L6_2atmpS1868;
        _M0L5entryS587->$2 = _M0L3pslS585;
        moonbit_incref(_M0L14_2acurr__entryS592);
        moonbit_incref(_M0L4selfS589);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS589, _M0L5entryS587, _M0L3idxS586);
        _M0L3pslS1871 = _M0L14_2acurr__entryS592->$2;
        _M0L6_2atmpS1867 = _M0L3pslS1871 + 1;
        _M0L6_2atmpS1869 = _M0L3idxS586 + 1;
        _M0L14capacity__maskS1870 = _M0L4selfS589->$3;
        _M0L6_2atmpS1868 = _M0L6_2atmpS1869 & _M0L14capacity__maskS1870;
        _M0L3pslS585 = _M0L6_2atmpS1867;
        _M0L3idxS586 = _M0L6_2atmpS1868;
        _M0L5entryS587 = _M0L14_2acurr__entryS592;
        continue;
      } else {
        int32_t _M0L6_2atmpS1872 = _M0L3pslS585 + 1;
        int32_t _M0L6_2atmpS1874 = _M0L3idxS586 + 1;
        int32_t _M0L14capacity__maskS1875 = _M0L4selfS589->$3;
        int32_t _M0L6_2atmpS1873 =
          _M0L6_2atmpS1874 & _M0L14capacity__maskS1875;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3304 =
          _M0L5entryS587;
        _M0L3pslS585 = _M0L6_2atmpS1872;
        _M0L3idxS586 = _M0L6_2atmpS1873;
        _M0L5entryS587 = _tmp_3304;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS599,
  int32_t _M0L3idxS604,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS603
) {
  int32_t _M0L3pslS1897;
  int32_t _M0L6_2atmpS1893;
  int32_t _M0L6_2atmpS1895;
  int32_t _M0L14capacity__maskS1896;
  int32_t _M0L6_2atmpS1894;
  int32_t _M0L3pslS595;
  int32_t _M0L3idxS596;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS597;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1897 = _M0L5entryS603->$2;
  _M0L6_2atmpS1893 = _M0L3pslS1897 + 1;
  _M0L6_2atmpS1895 = _M0L3idxS604 + 1;
  _M0L14capacity__maskS1896 = _M0L4selfS599->$3;
  _M0L6_2atmpS1894 = _M0L6_2atmpS1895 & _M0L14capacity__maskS1896;
  _M0L3pslS595 = _M0L6_2atmpS1893;
  _M0L3idxS596 = _M0L6_2atmpS1894;
  _M0L5entryS597 = _M0L5entryS603;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1892 =
      _M0L4selfS599->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS598;
    if (
      _M0L3idxS596 < 0
      || _M0L3idxS596 >= Moonbit_array_length(_M0L7entriesS1892)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS598
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1892[
        _M0L3idxS596
      ];
    if (_M0L7_2abindS598 == 0) {
      _M0L5entryS597->$2 = _M0L3pslS595;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS599, _M0L5entryS597, _M0L3idxS596);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS601 =
        _M0L7_2abindS598;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS602 =
        _M0L7_2aSomeS601;
      int32_t _M0L3pslS1882 = _M0L14_2acurr__entryS602->$2;
      if (_M0L3pslS595 > _M0L3pslS1882) {
        int32_t _M0L3pslS1887;
        int32_t _M0L6_2atmpS1883;
        int32_t _M0L6_2atmpS1885;
        int32_t _M0L14capacity__maskS1886;
        int32_t _M0L6_2atmpS1884;
        _M0L5entryS597->$2 = _M0L3pslS595;
        moonbit_incref(_M0L14_2acurr__entryS602);
        moonbit_incref(_M0L4selfS599);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS599, _M0L5entryS597, _M0L3idxS596);
        _M0L3pslS1887 = _M0L14_2acurr__entryS602->$2;
        _M0L6_2atmpS1883 = _M0L3pslS1887 + 1;
        _M0L6_2atmpS1885 = _M0L3idxS596 + 1;
        _M0L14capacity__maskS1886 = _M0L4selfS599->$3;
        _M0L6_2atmpS1884 = _M0L6_2atmpS1885 & _M0L14capacity__maskS1886;
        _M0L3pslS595 = _M0L6_2atmpS1883;
        _M0L3idxS596 = _M0L6_2atmpS1884;
        _M0L5entryS597 = _M0L14_2acurr__entryS602;
        continue;
      } else {
        int32_t _M0L6_2atmpS1888 = _M0L3pslS595 + 1;
        int32_t _M0L6_2atmpS1890 = _M0L3idxS596 + 1;
        int32_t _M0L14capacity__maskS1891 = _M0L4selfS599->$3;
        int32_t _M0L6_2atmpS1889 =
          _M0L6_2atmpS1890 & _M0L14capacity__maskS1891;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3306 =
          _M0L5entryS597;
        _M0L3pslS595 = _M0L6_2atmpS1888;
        _M0L3idxS596 = _M0L6_2atmpS1889;
        _M0L5entryS597 = _tmp_3306;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS573,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS575,
  int32_t _M0L8new__idxS574
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1862;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1863;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2806;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2805;
  int32_t _M0L6_2acntS3069;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS576;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1862 = _M0L4selfS573->$0;
  moonbit_incref(_M0L5entryS575);
  _M0L6_2atmpS1863 = _M0L5entryS575;
  if (
    _M0L8new__idxS574 < 0
    || _M0L8new__idxS574 >= Moonbit_array_length(_M0L7entriesS1862)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2806
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1862[
      _M0L8new__idxS574
    ];
  if (_M0L6_2aoldS2806) {
    moonbit_decref(_M0L6_2aoldS2806);
  }
  _M0L7entriesS1862[_M0L8new__idxS574] = _M0L6_2atmpS1863;
  _M0L8_2afieldS2805 = _M0L5entryS575->$1;
  _M0L6_2acntS3069 = Moonbit_object_header(_M0L5entryS575)->rc;
  if (_M0L6_2acntS3069 > 1) {
    int32_t _M0L11_2anew__cntS3072 = _M0L6_2acntS3069 - 1;
    Moonbit_object_header(_M0L5entryS575)->rc = _M0L11_2anew__cntS3072;
    if (_M0L8_2afieldS2805) {
      moonbit_incref(_M0L8_2afieldS2805);
    }
  } else if (_M0L6_2acntS3069 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3071 =
      _M0L5entryS575->$5;
    moonbit_string_t _M0L8_2afieldS3070;
    moonbit_decref(_M0L8_2afieldS3071);
    _M0L8_2afieldS3070 = _M0L5entryS575->$4;
    moonbit_decref(_M0L8_2afieldS3070);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS575);
  }
  _M0L7_2abindS576 = _M0L8_2afieldS2805;
  if (_M0L7_2abindS576 == 0) {
    if (_M0L7_2abindS576) {
      moonbit_decref(_M0L7_2abindS576);
    }
    _M0L4selfS573->$6 = _M0L8new__idxS574;
    moonbit_decref(_M0L4selfS573);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS577;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS578;
    moonbit_decref(_M0L4selfS573);
    _M0L7_2aSomeS577 = _M0L7_2abindS576;
    _M0L7_2anextS578 = _M0L7_2aSomeS577;
    _M0L7_2anextS578->$0 = _M0L8new__idxS574;
    moonbit_decref(_M0L7_2anextS578);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS579,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS581,
  int32_t _M0L8new__idxS580
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1864;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1865;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2809;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2808;
  int32_t _M0L6_2acntS3073;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS582;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1864 = _M0L4selfS579->$0;
  moonbit_incref(_M0L5entryS581);
  _M0L6_2atmpS1865 = _M0L5entryS581;
  if (
    _M0L8new__idxS580 < 0
    || _M0L8new__idxS580 >= Moonbit_array_length(_M0L7entriesS1864)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2809
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1864[
      _M0L8new__idxS580
    ];
  if (_M0L6_2aoldS2809) {
    moonbit_decref(_M0L6_2aoldS2809);
  }
  _M0L7entriesS1864[_M0L8new__idxS580] = _M0L6_2atmpS1865;
  _M0L8_2afieldS2808 = _M0L5entryS581->$1;
  _M0L6_2acntS3073 = Moonbit_object_header(_M0L5entryS581)->rc;
  if (_M0L6_2acntS3073 > 1) {
    int32_t _M0L11_2anew__cntS3075 = _M0L6_2acntS3073 - 1;
    Moonbit_object_header(_M0L5entryS581)->rc = _M0L11_2anew__cntS3075;
    if (_M0L8_2afieldS2808) {
      moonbit_incref(_M0L8_2afieldS2808);
    }
  } else if (_M0L6_2acntS3073 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3074 =
      _M0L5entryS581->$5;
    moonbit_decref(_M0L8_2afieldS3074);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS581);
  }
  _M0L7_2abindS582 = _M0L8_2afieldS2808;
  if (_M0L7_2abindS582 == 0) {
    if (_M0L7_2abindS582) {
      moonbit_decref(_M0L7_2abindS582);
    }
    _M0L4selfS579->$6 = _M0L8new__idxS580;
    moonbit_decref(_M0L4selfS579);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS583;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS584;
    moonbit_decref(_M0L4selfS579);
    _M0L7_2aSomeS583 = _M0L7_2abindS582;
    _M0L7_2anextS584 = _M0L7_2aSomeS583;
    _M0L7_2anextS584->$0 = _M0L8new__idxS580;
    moonbit_decref(_M0L7_2anextS584);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS566,
  int32_t _M0L3idxS568,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS567
) {
  int32_t _M0L7_2abindS565;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1849;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1850;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2811;
  int32_t _M0L4sizeS1852;
  int32_t _M0L6_2atmpS1851;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS565 = _M0L4selfS566->$6;
  switch (_M0L7_2abindS565) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1844;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2813;
      moonbit_incref(_M0L5entryS567);
      _M0L6_2atmpS1844 = _M0L5entryS567;
      _M0L6_2aoldS2813 = _M0L4selfS566->$5;
      if (_M0L6_2aoldS2813) {
        moonbit_decref(_M0L6_2aoldS2813);
      }
      _M0L4selfS566->$5 = _M0L6_2atmpS1844;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1848 =
        _M0L4selfS566->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1847;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1845;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1846;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2814;
      if (
        _M0L7_2abindS565 < 0
        || _M0L7_2abindS565 >= Moonbit_array_length(_M0L7entriesS1848)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1847
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1848[
          _M0L7_2abindS565
        ];
      if (_M0L6_2atmpS1847) {
        moonbit_incref(_M0L6_2atmpS1847);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1845
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1847);
      moonbit_incref(_M0L5entryS567);
      _M0L6_2atmpS1846 = _M0L5entryS567;
      _M0L6_2aoldS2814 = _M0L6_2atmpS1845->$1;
      if (_M0L6_2aoldS2814) {
        moonbit_decref(_M0L6_2aoldS2814);
      }
      _M0L6_2atmpS1845->$1 = _M0L6_2atmpS1846;
      moonbit_decref(_M0L6_2atmpS1845);
      break;
    }
  }
  _M0L4selfS566->$6 = _M0L3idxS568;
  _M0L7entriesS1849 = _M0L4selfS566->$0;
  _M0L6_2atmpS1850 = _M0L5entryS567;
  if (
    _M0L3idxS568 < 0
    || _M0L3idxS568 >= Moonbit_array_length(_M0L7entriesS1849)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2811
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1849[
      _M0L3idxS568
    ];
  if (_M0L6_2aoldS2811) {
    moonbit_decref(_M0L6_2aoldS2811);
  }
  _M0L7entriesS1849[_M0L3idxS568] = _M0L6_2atmpS1850;
  _M0L4sizeS1852 = _M0L4selfS566->$1;
  _M0L6_2atmpS1851 = _M0L4sizeS1852 + 1;
  _M0L4selfS566->$1 = _M0L6_2atmpS1851;
  moonbit_decref(_M0L4selfS566);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS570,
  int32_t _M0L3idxS572,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS571
) {
  int32_t _M0L7_2abindS569;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1858;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1859;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2817;
  int32_t _M0L4sizeS1861;
  int32_t _M0L6_2atmpS1860;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS569 = _M0L4selfS570->$6;
  switch (_M0L7_2abindS569) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1853;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2819;
      moonbit_incref(_M0L5entryS571);
      _M0L6_2atmpS1853 = _M0L5entryS571;
      _M0L6_2aoldS2819 = _M0L4selfS570->$5;
      if (_M0L6_2aoldS2819) {
        moonbit_decref(_M0L6_2aoldS2819);
      }
      _M0L4selfS570->$5 = _M0L6_2atmpS1853;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1857 =
        _M0L4selfS570->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1856;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1854;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1855;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2820;
      if (
        _M0L7_2abindS569 < 0
        || _M0L7_2abindS569 >= Moonbit_array_length(_M0L7entriesS1857)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1856
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1857[
          _M0L7_2abindS569
        ];
      if (_M0L6_2atmpS1856) {
        moonbit_incref(_M0L6_2atmpS1856);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1854
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1856);
      moonbit_incref(_M0L5entryS571);
      _M0L6_2atmpS1855 = _M0L5entryS571;
      _M0L6_2aoldS2820 = _M0L6_2atmpS1854->$1;
      if (_M0L6_2aoldS2820) {
        moonbit_decref(_M0L6_2aoldS2820);
      }
      _M0L6_2atmpS1854->$1 = _M0L6_2atmpS1855;
      moonbit_decref(_M0L6_2atmpS1854);
      break;
    }
  }
  _M0L4selfS570->$6 = _M0L3idxS572;
  _M0L7entriesS1858 = _M0L4selfS570->$0;
  _M0L6_2atmpS1859 = _M0L5entryS571;
  if (
    _M0L3idxS572 < 0
    || _M0L3idxS572 >= Moonbit_array_length(_M0L7entriesS1858)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2817
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1858[
      _M0L3idxS572
    ];
  if (_M0L6_2aoldS2817) {
    moonbit_decref(_M0L6_2aoldS2817);
  }
  _M0L7entriesS1858[_M0L3idxS572] = _M0L6_2atmpS1859;
  _M0L4sizeS1861 = _M0L4selfS570->$1;
  _M0L6_2atmpS1860 = _M0L4sizeS1861 + 1;
  _M0L4selfS570->$1 = _M0L6_2atmpS1860;
  moonbit_decref(_M0L4selfS570);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS554
) {
  int32_t _M0L8capacityS553;
  int32_t _M0L7_2abindS555;
  int32_t _M0L7_2abindS556;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1842;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS557;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS558;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3307;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS553
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS554);
  _M0L7_2abindS555 = _M0L8capacityS553 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS556 = _M0FPB21calc__grow__threshold(_M0L8capacityS553);
  _M0L6_2atmpS1842 = 0;
  _M0L7_2abindS557
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS553, _M0L6_2atmpS1842);
  _M0L7_2abindS558 = 0;
  _block_3307
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3307)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3307->$0 = _M0L7_2abindS557;
  _block_3307->$1 = 0;
  _block_3307->$2 = _M0L8capacityS553;
  _block_3307->$3 = _M0L7_2abindS555;
  _block_3307->$4 = _M0L7_2abindS556;
  _block_3307->$5 = _M0L7_2abindS558;
  _block_3307->$6 = -1;
  return _block_3307;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS560
) {
  int32_t _M0L8capacityS559;
  int32_t _M0L7_2abindS561;
  int32_t _M0L7_2abindS562;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1843;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS563;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS564;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3308;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS559
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS560);
  _M0L7_2abindS561 = _M0L8capacityS559 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS562 = _M0FPB21calc__grow__threshold(_M0L8capacityS559);
  _M0L6_2atmpS1843 = 0;
  _M0L7_2abindS563
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS559, _M0L6_2atmpS1843);
  _M0L7_2abindS564 = 0;
  _block_3308
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3308)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3308->$0 = _M0L7_2abindS563;
  _block_3308->$1 = 0;
  _block_3308->$2 = _M0L8capacityS559;
  _block_3308->$3 = _M0L7_2abindS561;
  _block_3308->$4 = _M0L7_2abindS562;
  _block_3308->$5 = _M0L7_2abindS564;
  _block_3308->$6 = -1;
  return _block_3308;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS552) {
  #line 33 "/Users/user/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS552 >= 0) {
    int32_t _M0L6_2atmpS1841;
    int32_t _M0L6_2atmpS1840;
    int32_t _M0L6_2atmpS1839;
    int32_t _M0L6_2atmpS1838;
    if (_M0L4selfS552 <= 1) {
      return 1;
    }
    if (_M0L4selfS552 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1841 = _M0L4selfS552 - 1;
    #line 44 "/Users/user/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS1840 = moonbit_clz32(_M0L6_2atmpS1841);
    _M0L6_2atmpS1839 = _M0L6_2atmpS1840 - 1;
    _M0L6_2atmpS1838 = 2147483647 >> (_M0L6_2atmpS1839 & 31);
    return _M0L6_2atmpS1838 + 1;
  } else {
    #line 34 "/Users/user/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS551) {
  int32_t _M0L6_2atmpS1837;
  #line 510 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1837 = _M0L8capacityS551 * 13;
  return _M0L6_2atmpS1837 / 16;
}

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC16option6Option6unwrapGRP36mulpjs4mulp6stream4FileE(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4selfS545
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS545 == 0) {
    if (_M0L4selfS545) {
      moonbit_decref(_M0L4selfS545);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS546 = _M0L4selfS545;
    return _M0L7_2aSomeS546;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS547
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS547 == 0) {
    if (_M0L4selfS547) {
      moonbit_decref(_M0L4selfS547);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS548 =
      _M0L4selfS547;
    return _M0L7_2aSomeS548;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS549
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS549 == 0) {
    if (_M0L4selfS549) {
      moonbit_decref(_M0L4selfS549);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS550 =
      _M0L4selfS549;
    return _M0L7_2aSomeS550;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS544
) {
  moonbit_string_t* _M0L6_2atmpS1836;
  #line 165 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS1836 = _M0L4selfS544;
  #line 167 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1836);
}

void* _M0MPC16result6Result11unwrap__errGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(
  void* _M0L4selfS540
) {
  #line 286 "/Users/user/.moon/lib/core/builtin/result.mbt"
  switch (Moonbit_object_tag(_M0L4selfS540)) {
    case 1: {
      moonbit_decref(_M0L4selfS540);
      #line 288 "/Users/user/.moon/lib/core/builtin/result.mbt"
      return _M0FPC15abort5abortGRP36mulpjs4mulp4core9MulpErrorE((moonbit_string_t)moonbit_string_literal_92.data);
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS541 =
        (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L4selfS540;
      void* _M0L8_2afieldS2823 = _M0L6_2aErrS541->$0;
      int32_t _M0L6_2acntS3076 = Moonbit_object_header(_M0L6_2aErrS541)->rc;
      if (_M0L6_2acntS3076 > 1) {
        int32_t _M0L11_2anew__cntS3077 = _M0L6_2acntS3076 - 1;
        Moonbit_object_header(_M0L6_2aErrS541)->rc = _M0L11_2anew__cntS3077;
        moonbit_incref(_M0L8_2afieldS2823);
      } else if (_M0L6_2acntS3076 == 1) {
        #line 286 "/Users/user/.moon/lib/core/builtin/result.mbt"
        moonbit_free(_M0L6_2aErrS541);
      }
      return _M0L8_2afieldS2823;
      break;
    }
  }
}

void* _M0MPC16result6Result11unwrap__errGuRP36mulpjs4mulp4core9MulpErrorE(
  void* _M0L4selfS542
) {
  #line 286 "/Users/user/.moon/lib/core/builtin/result.mbt"
  switch (Moonbit_object_tag(_M0L4selfS542)) {
    case 1: {
      moonbit_decref(_M0L4selfS542);
      #line 288 "/Users/user/.moon/lib/core/builtin/result.mbt"
      return _M0FPC15abort5abortGRP36mulpjs4mulp4core9MulpErrorE((moonbit_string_t)moonbit_string_literal_92.data);
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS543 =
        (struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L4selfS542;
      void* _M0L8_2afieldS2824 = _M0L6_2aErrS543->$0;
      int32_t _M0L6_2acntS3078 = Moonbit_object_header(_M0L6_2aErrS543)->rc;
      if (_M0L6_2acntS3078 > 1) {
        int32_t _M0L11_2anew__cntS3079 = _M0L6_2acntS3078 - 1;
        Moonbit_object_header(_M0L6_2aErrS543)->rc = _M0L11_2anew__cntS3079;
        moonbit_incref(_M0L8_2afieldS2824);
      } else if (_M0L6_2acntS3078 == 1) {
        #line 286 "/Users/user/.moon/lib/core/builtin/result.mbt"
        moonbit_free(_M0L6_2aErrS543);
      }
      return _M0L8_2afieldS2824;
      break;
    }
  }
}

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0MPC16result6Result6unwrapGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE(
  void* _M0L4selfS536
) {
  #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
  switch (Moonbit_object_tag(_M0L4selfS536)) {
    case 1: {
      struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS537 =
        (struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L4selfS536;
      struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L8_2afieldS2825 =
        _M0L5_2aOkS537->$0;
      int32_t _M0L6_2acntS3080 = Moonbit_object_header(_M0L5_2aOkS537)->rc;
      if (_M0L6_2acntS3080 > 1) {
        int32_t _M0L11_2anew__cntS3081 = _M0L6_2acntS3080 - 1;
        Moonbit_object_header(_M0L5_2aOkS537)->rc = _M0L11_2anew__cntS3081;
        moonbit_incref(_M0L8_2afieldS2825);
      } else if (_M0L6_2acntS3080 == 1) {
        #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
        moonbit_free(_M0L5_2aOkS537);
      }
      return _M0L8_2afieldS2825;
      break;
    }
    default: {
      moonbit_decref(_M0L4selfS536);
      #line 264 "/Users/user/.moon/lib/core/builtin/result.mbt"
      return _M0FPC15abort5abortGRPB5ArrayGRP36mulpjs4mulp6stream4FileEE((moonbit_string_t)moonbit_string_literal_93.data);
      break;
    }
  }
}

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC16result6Result6unwrapGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE(
  void* _M0L4selfS538
) {
  #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
  switch (Moonbit_object_tag(_M0L4selfS538)) {
    case 1: {
      struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS539 =
        (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L4selfS538;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS2826 =
        _M0L5_2aOkS539->$0;
      int32_t _M0L6_2acntS3082 = Moonbit_object_header(_M0L5_2aOkS539)->rc;
      if (_M0L6_2acntS3082 > 1) {
        int32_t _M0L11_2anew__cntS3083 = _M0L6_2acntS3082 - 1;
        Moonbit_object_header(_M0L5_2aOkS539)->rc = _M0L11_2anew__cntS3083;
        if (_M0L8_2afieldS2826) {
          moonbit_incref(_M0L8_2afieldS2826);
        }
      } else if (_M0L6_2acntS3082 == 1) {
        #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
        moonbit_free(_M0L5_2aOkS539);
      }
      return _M0L8_2afieldS2826;
      break;
    }
    default: {
      moonbit_decref(_M0L4selfS538);
      #line 264 "/Users/user/.moon/lib/core/builtin/result.mbt"
      return _M0FPC15abort5abortGORP36mulpjs4mulp6stream4FileE((moonbit_string_t)moonbit_string_literal_93.data);
      break;
    }
  }
}

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS535,
  struct _M0TPB6Logger _M0L6loggerS534
) {
  struct _M0TWEOs* _M0L6_2atmpS1835;
  #line 304 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 305 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1835 = _M0MPC15array5Array4iterGsE(_M0L4selfS535);
  #line 305 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPB6Logger19write__iter_2einnerGsE(_M0L6loggerS534, _M0L6_2atmpS1835, (moonbit_string_t)moonbit_string_literal_94.data, (moonbit_string_t)moonbit_string_literal_95.data, (moonbit_string_t)moonbit_string_literal_96.data, 0);
  return 0;
}

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS533
) {
  moonbit_string_t* _M0L3bufS1833;
  int32_t _M0L3lenS1834;
  int32_t _M0L6_2acntS3084;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1832;
  #line 1656 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS1833 = _M0L4selfS533->$0;
  _M0L3lenS1834 = _M0L4selfS533->$1;
  _M0L6_2acntS3084 = Moonbit_object_header(_M0L4selfS533)->rc;
  if (_M0L6_2acntS3084 > 1) {
    int32_t _M0L11_2anew__cntS3085 = _M0L6_2acntS3084 - 1;
    Moonbit_object_header(_M0L4selfS533)->rc = _M0L11_2anew__cntS3085;
    moonbit_incref(_M0L3bufS1833);
  } else if (_M0L6_2acntS3084 == 1) {
    #line 1658 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS533);
  }
  _M0L6_2atmpS1832
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1834, _M0L3bufS1833
  };
  #line 1658 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1832);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS532
) {
  moonbit_string_t* _M0L6_2atmpS1830;
  int32_t _M0L6_2atmpS1831;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1829;
  #line 1509 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS532);
  _M0L6_2atmpS1830 = _M0L4selfS532;
  _M0L6_2atmpS1831 = Moonbit_array_length(_M0L4selfS532);
  moonbit_decref(_M0L4selfS532);
  _M0L6_2atmpS1829
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1831, _M0L6_2atmpS1830
  };
  #line 1511 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1829);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS530
) {
  struct _M0TPB8MutLocalGiE* _M0L1iS529;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__* _closure_3309;
  struct _M0TWEOs* _M0L6_2atmpS1817;
  #line 677 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS529
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS529)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS529->$0 = 0;
  _closure_3309
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__));
  Moonbit_object_header(_closure_3309)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__, $0_0) >> 2, 2, 0);
  _closure_3309->code = &_M0MPC15array9ArrayView4iterGsEC1818l680;
  _closure_3309->$0_0 = _M0L4selfS530.$0;
  _closure_3309->$0_1 = _M0L4selfS530.$1;
  _closure_3309->$0_2 = _M0L4selfS530.$2;
  _closure_3309->$1 = _M0L1iS529;
  _M0L6_2atmpS1817 = (struct _M0TWEOs*)_closure_3309;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1817);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1818l680(
  struct _M0TWEOs* _M0L6_2aenvS1819
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__* _M0L14_2acasted__envS1820;
  struct _M0TPB8MutLocalGiE* _M0L1iS529;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2830;
  int32_t _M0L6_2acntS3086;
  struct _M0TPB9ArrayViewGsE _M0L4selfS530;
  int32_t _M0L3valS1821;
  int32_t _M0L6_2atmpS1822;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1820
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1818__l680__*)_M0L6_2aenvS1819;
  _M0L1iS529 = _M0L14_2acasted__envS1820->$1;
  _M0L8_2afieldS2830
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1820->$0_1,
      _M0L14_2acasted__envS1820->$0_2,
      _M0L14_2acasted__envS1820->$0_0
  };
  _M0L6_2acntS3086 = Moonbit_object_header(_M0L14_2acasted__envS1820)->rc;
  if (_M0L6_2acntS3086 > 1) {
    int32_t _M0L11_2anew__cntS3087 = _M0L6_2acntS3086 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1820)->rc
    = _M0L11_2anew__cntS3087;
    moonbit_incref(_M0L1iS529);
    moonbit_incref(_M0L8_2afieldS2830.$0);
  } else if (_M0L6_2acntS3086 == 1) {
    #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1820);
  }
  _M0L4selfS530 = _M0L8_2afieldS2830;
  _M0L3valS1821 = _M0L1iS529->$0;
  moonbit_incref(_M0L4selfS530.$0);
  #line 681 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1822 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS530);
  if (_M0L3valS1821 < _M0L6_2atmpS1822) {
    moonbit_string_t* _M0L3bufS1825 = _M0L4selfS530.$0;
    int32_t _M0L5startS1827 = _M0L4selfS530.$1;
    int32_t _M0L3valS1828 = _M0L1iS529->$0;
    int32_t _M0L6_2atmpS1826 = _M0L5startS1827 + _M0L3valS1828;
    moonbit_string_t _M0L6_2atmpS2828 =
      (moonbit_string_t)_M0L3bufS1825[_M0L6_2atmpS1826];
    moonbit_string_t _M0L4elemS531;
    int32_t _M0L3valS1824;
    int32_t _M0L6_2atmpS1823;
    moonbit_incref(_M0L6_2atmpS2828);
    moonbit_decref(_M0L3bufS1825);
    _M0L4elemS531 = _M0L6_2atmpS2828;
    _M0L3valS1824 = _M0L1iS529->$0;
    _M0L6_2atmpS1823 = _M0L3valS1824 + 1;
    _M0L1iS529->$0 = _M0L6_2atmpS1823;
    moonbit_decref(_M0L1iS529);
    return _M0L4elemS531;
  } else {
    moonbit_decref(_M0L4selfS530.$0);
    moonbit_decref(_M0L1iS529);
    return 0;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS527,
  struct _M0TPB6Logger _M0L6loggerS528
) {
  int32_t _M0L6_2atmpS1816;
  struct _M0TPC16string10StringView _M0L6_2atmpS1815;
  #line 244 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1816 = Moonbit_array_length(_M0L4selfS527);
  _M0L6_2atmpS1815
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1816, _M0L4selfS527
  };
  #line 245 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1815, _M0L6loggerS528, 1);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS526) {
  #line 45 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 46 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS526, 10);
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS525,
  struct _M0TPB6Logger _M0L6loggerS524
) {
  moonbit_string_t _M0L6_2atmpS1814;
  #line 40 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 41 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1814 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS525, 10);
  #line 41 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS524.$0->$method_0(_M0L6loggerS524.$1, _M0L6_2atmpS1814);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS523,
  struct _M0TPB6Logger _M0L6loggerS522
) {
  moonbit_string_t _M0L6_2atmpS1813;
  #line 26 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1813 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS523);
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS522.$0->$method_0(_M0L6loggerS522.$1, _M0L6_2atmpS1813);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS521) {
  #line 31 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS521) {
    return (moonbit_string_t)moonbit_string_literal_12.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_97.data;
  }
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS520
) {
  int32_t _M0L6_2atmpS1812;
  #line 24 "/Users/user/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS1812 = Moonbit_array_length(_M0L4selfS520);
  return (struct _M0TPC16string10StringView){0,
                                               _M0L6_2atmpS1812,
                                               _M0L4selfS520};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS511,
  moonbit_string_t _M0L5valueS513
) {
  int32_t _M0L3lenS1797;
  moonbit_string_t* _M0L6_2atmpS1799;
  int32_t _M0L6_2atmpS1798;
  int32_t _M0L6lengthS512;
  moonbit_string_t* _M0L3bufS1800;
  moonbit_string_t _M0L6_2aoldS2832;
  int32_t _M0L6_2atmpS1801;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1797 = _M0L4selfS511->$1;
  moonbit_incref(_M0L4selfS511);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1799 = _M0MPC15array5Array6bufferGsE(_M0L4selfS511);
  _M0L6_2atmpS1798 = Moonbit_array_length(_M0L6_2atmpS1799);
  moonbit_decref(_M0L6_2atmpS1799);
  if (_M0L3lenS1797 == _M0L6_2atmpS1798) {
    moonbit_incref(_M0L4selfS511);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS511);
  }
  _M0L6lengthS512 = _M0L4selfS511->$1;
  _M0L3bufS1800 = _M0L4selfS511->$0;
  _M0L6_2aoldS2832 = (moonbit_string_t)_M0L3bufS1800[_M0L6lengthS512];
  moonbit_decref(_M0L6_2aoldS2832);
  _M0L3bufS1800[_M0L6lengthS512] = _M0L5valueS513;
  _M0L6_2atmpS1801 = _M0L6lengthS512 + 1;
  _M0L4selfS511->$1 = _M0L6_2atmpS1801;
  moonbit_decref(_M0L4selfS511);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS514,
  struct _M0TUsiE* _M0L5valueS516
) {
  int32_t _M0L3lenS1802;
  struct _M0TUsiE** _M0L6_2atmpS1804;
  int32_t _M0L6_2atmpS1803;
  int32_t _M0L6lengthS515;
  struct _M0TUsiE** _M0L3bufS1805;
  struct _M0TUsiE* _M0L6_2aoldS2834;
  int32_t _M0L6_2atmpS1806;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1802 = _M0L4selfS514->$1;
  moonbit_incref(_M0L4selfS514);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1804 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS514);
  _M0L6_2atmpS1803 = Moonbit_array_length(_M0L6_2atmpS1804);
  moonbit_decref(_M0L6_2atmpS1804);
  if (_M0L3lenS1802 == _M0L6_2atmpS1803) {
    moonbit_incref(_M0L4selfS514);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS514);
  }
  _M0L6lengthS515 = _M0L4selfS514->$1;
  _M0L3bufS1805 = _M0L4selfS514->$0;
  _M0L6_2aoldS2834 = (struct _M0TUsiE*)_M0L3bufS1805[_M0L6lengthS515];
  if (_M0L6_2aoldS2834) {
    moonbit_decref(_M0L6_2aoldS2834);
  }
  _M0L3bufS1805[_M0L6lengthS515] = _M0L5valueS516;
  _M0L6_2atmpS1806 = _M0L6lengthS515 + 1;
  _M0L4selfS514->$1 = _M0L6_2atmpS1806;
  moonbit_decref(_M0L4selfS514);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS517,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5valueS519
) {
  int32_t _M0L3lenS1807;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS1809;
  int32_t _M0L6_2atmpS1808;
  int32_t _M0L6lengthS518;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L3bufS1810;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2aoldS2836;
  int32_t _M0L6_2atmpS1811;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1807 = _M0L4selfS517->$1;
  moonbit_incref(_M0L4selfS517);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1809
  = _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(_M0L4selfS517);
  _M0L6_2atmpS1808 = Moonbit_array_length(_M0L6_2atmpS1809);
  moonbit_decref(_M0L6_2atmpS1809);
  if (_M0L3lenS1807 == _M0L6_2atmpS1808) {
    moonbit_incref(_M0L4selfS517);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP36mulpjs4mulp6stream4FileE(_M0L4selfS517);
  }
  _M0L6lengthS518 = _M0L4selfS517->$1;
  _M0L3bufS1810 = _M0L4selfS517->$0;
  _M0L6_2aoldS2836
  = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3bufS1810[_M0L6lengthS518];
  if (_M0L6_2aoldS2836) {
    moonbit_decref(_M0L6_2aoldS2836);
  }
  _M0L3bufS1810[_M0L6lengthS518] = _M0L5valueS519;
  _M0L6_2atmpS1811 = _M0L6lengthS518 + 1;
  _M0L4selfS517->$1 = _M0L6_2atmpS1811;
  moonbit_decref(_M0L4selfS517);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS503) {
  int32_t _M0L8old__capS502;
  int32_t _M0L8new__capS504;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS502 = _M0L4selfS503->$1;
  if (_M0L8old__capS502 == 0) {
    _M0L8new__capS504 = 8;
  } else {
    _M0L8new__capS504 = _M0L8old__capS502 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS503, _M0L8new__capS504);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS506
) {
  int32_t _M0L8old__capS505;
  int32_t _M0L8new__capS507;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS505 = _M0L4selfS506->$1;
  if (_M0L8old__capS505 == 0) {
    _M0L8new__capS507 = 8;
  } else {
    _M0L8new__capS507 = _M0L8old__capS505 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS506, _M0L8new__capS507);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS509
) {
  int32_t _M0L8old__capS508;
  int32_t _M0L8new__capS510;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS508 = _M0L4selfS509->$1;
  if (_M0L8old__capS508 == 0) {
    _M0L8new__capS510 = 8;
  } else {
    _M0L8new__capS510 = _M0L8old__capS508 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp6stream4FileE(_M0L4selfS509, _M0L8new__capS510);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS487,
  int32_t _M0L13new__capacityS485
) {
  moonbit_string_t* _M0L8new__bufS484;
  moonbit_string_t* _M0L8old__bufS486;
  int32_t _M0L8old__capS488;
  int32_t _M0L9copy__lenS489;
  moonbit_string_t* _M0L6_2aoldS2838;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS484
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS485, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8old__bufS486 = _M0L4selfS487->$0;
  _M0L8old__capS488 = Moonbit_array_length(_M0L8old__bufS486);
  if (_M0L8old__capS488 < _M0L13new__capacityS485) {
    _M0L9copy__lenS489 = _M0L8old__capS488;
  } else {
    _M0L9copy__lenS489 = _M0L13new__capacityS485;
  }
  moonbit_incref(_M0L8old__bufS486);
  moonbit_incref(_M0L8new__bufS484);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS484, 0, _M0L8old__bufS486, 0, _M0L9copy__lenS489);
  _M0L6_2aoldS2838 = _M0L4selfS487->$0;
  moonbit_decref(_M0L6_2aoldS2838);
  _M0L4selfS487->$0 = _M0L8new__bufS484;
  moonbit_decref(_M0L4selfS487);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS493,
  int32_t _M0L13new__capacityS491
) {
  struct _M0TUsiE** _M0L8new__bufS490;
  struct _M0TUsiE** _M0L8old__bufS492;
  int32_t _M0L8old__capS494;
  int32_t _M0L9copy__lenS495;
  struct _M0TUsiE** _M0L6_2aoldS2840;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS490
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS491, 0);
  _M0L8old__bufS492 = _M0L4selfS493->$0;
  _M0L8old__capS494 = Moonbit_array_length(_M0L8old__bufS492);
  if (_M0L8old__capS494 < _M0L13new__capacityS491) {
    _M0L9copy__lenS495 = _M0L8old__capS494;
  } else {
    _M0L9copy__lenS495 = _M0L13new__capacityS491;
  }
  moonbit_incref(_M0L8old__bufS492);
  moonbit_incref(_M0L8new__bufS490);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS490, 0, _M0L8old__bufS492, 0, _M0L9copy__lenS495);
  _M0L6_2aoldS2840 = _M0L4selfS493->$0;
  moonbit_decref(_M0L6_2aoldS2840);
  _M0L4selfS493->$0 = _M0L8new__bufS490;
  moonbit_decref(_M0L4selfS493);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS499,
  int32_t _M0L13new__capacityS497
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L8new__bufS496;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L8old__bufS498;
  int32_t _M0L8old__capS500;
  int32_t _M0L9copy__lenS501;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2aoldS2842;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS496
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array(_M0L13new__capacityS497, 0);
  _M0L8old__bufS498 = _M0L4selfS499->$0;
  _M0L8old__capS500 = Moonbit_array_length(_M0L8old__bufS498);
  if (_M0L8old__capS500 < _M0L13new__capacityS497) {
    _M0L9copy__lenS501 = _M0L8old__capS500;
  } else {
    _M0L9copy__lenS501 = _M0L13new__capacityS497;
  }
  moonbit_incref(_M0L8old__bufS498);
  moonbit_incref(_M0L8new__bufS496);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp6stream4FileE(_M0L8new__bufS496, 0, _M0L8old__bufS498, 0, _M0L9copy__lenS501);
  _M0L6_2aoldS2842 = _M0L4selfS499->$0;
  moonbit_decref(_M0L6_2aoldS2842);
  _M0L4selfS499->$0 = _M0L8new__bufS496;
  moonbit_decref(_M0L4selfS499);
  return 0;
}

int32_t _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS483
) {
  int32_t _result_3310;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_3310 = _M0L4selfS483->$1;
  moonbit_decref(_M0L4selfS483);
  return _result_3310;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS480
) {
  moonbit_string_t* _M0L8_2afieldS2844;
  int32_t _M0L6_2acntS3088;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2844 = _M0L4selfS480->$0;
  _M0L6_2acntS3088 = Moonbit_object_header(_M0L4selfS480)->rc;
  if (_M0L6_2acntS3088 > 1) {
    int32_t _M0L11_2anew__cntS3089 = _M0L6_2acntS3088 - 1;
    Moonbit_object_header(_M0L4selfS480)->rc = _M0L11_2anew__cntS3089;
    moonbit_incref(_M0L8_2afieldS2844);
  } else if (_M0L6_2acntS3088 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS480);
  }
  return _M0L8_2afieldS2844;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS481
) {
  struct _M0TUsiE** _M0L8_2afieldS2845;
  int32_t _M0L6_2acntS3090;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2845 = _M0L4selfS481->$0;
  _M0L6_2acntS3090 = Moonbit_object_header(_M0L4selfS481)->rc;
  if (_M0L6_2acntS3090 > 1) {
    int32_t _M0L11_2anew__cntS3091 = _M0L6_2acntS3090 - 1;
    Moonbit_object_header(_M0L4selfS481)->rc = _M0L11_2anew__cntS3091;
    moonbit_incref(_M0L8_2afieldS2845);
  } else if (_M0L6_2acntS3090 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS481);
  }
  return _M0L8_2afieldS2845;
}

struct _M0TP36mulpjs4mulp6stream4File** _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS482
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L8_2afieldS2846;
  int32_t _M0L6_2acntS3092;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2846 = _M0L4selfS482->$0;
  _M0L6_2acntS3092 = Moonbit_object_header(_M0L4selfS482)->rc;
  if (_M0L6_2acntS3092 > 1) {
    int32_t _M0L11_2anew__cntS3093 = _M0L6_2acntS3092 - 1;
    Moonbit_object_header(_M0L4selfS482)->rc = _M0L11_2anew__cntS3093;
    moonbit_incref(_M0L8_2afieldS2846);
  } else if (_M0L6_2acntS3092 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS482);
  }
  return _M0L8_2afieldS2846;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS479
) {
  #line 53 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS479 == 0) {
    moonbit_string_t* _M0L6_2atmpS1795 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3311 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3311)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3311->$0 = _M0L6_2atmpS1795;
    _block_3311->$1 = 0;
    return _block_3311;
  } else {
    moonbit_string_t* _M0L6_2atmpS1796 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS479, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3312 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3312)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3312->$0 = _M0L6_2atmpS1796;
    _block_3312->$1 = 0;
    return _block_3312;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS478
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS478;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS477,
  struct _M0TPC16string10StringView _M0L3strS476
) {
  int32_t _M0L8str__lenS475;
  int32_t _M0L3lenS1788;
  int32_t _M0L6_2atmpS1787;
  uint16_t* _M0L4dataS1789;
  int32_t _M0L3lenS1790;
  moonbit_string_t _M0L6_2atmpS1791;
  int32_t _M0L6_2atmpS1792;
  int32_t _M0L3lenS1794;
  int32_t _M0L6_2atmpS1793;
  #line 126 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  moonbit_incref(_M0L3strS476.$0);
  #line 130 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS475 = _M0MPC16string10StringView6length(_M0L3strS476);
  _M0L3lenS1788 = _M0L4selfS477->$1;
  _M0L6_2atmpS1787 = _M0L3lenS1788 + _M0L8str__lenS475;
  moonbit_incref(_M0L4selfS477);
  #line 131 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS477, _M0L6_2atmpS1787);
  _M0L4dataS1789 = _M0L4selfS477->$0;
  _M0L3lenS1790 = _M0L4selfS477->$1;
  moonbit_incref(_M0L4dataS1789);
  moonbit_incref(_M0L3strS476.$0);
  #line 134 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1791 = _M0MPC16string10StringView4data(_M0L3strS476);
  #line 135 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1792 = _M0MPC16string10StringView13start__offset(_M0L3strS476);
  #line 132 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1789, _M0L3lenS1790, _M0L6_2atmpS1791, _M0L6_2atmpS1792, _M0L8str__lenS475);
  _M0L3lenS1794 = _M0L4selfS477->$1;
  _M0L6_2atmpS1793 = _M0L3lenS1794 + _M0L8str__lenS475;
  _M0L4selfS477->$1 = _M0L6_2atmpS1793;
  moonbit_decref(_M0L4selfS477);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS467,
  int32_t _M0L3lenS470,
  int32_t _M0L13start__offsetS474,
  int64_t _M0L11end__offsetS465
) {
  int32_t _M0L11end__offsetS464;
  int32_t _M0L5indexS468;
  int32_t _M0L5countS469;
  #line 441 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS465 == 4294967296ll) {
    _M0L11end__offsetS464 = Moonbit_array_length(_M0L4selfS467);
  } else {
    int64_t _M0L7_2aSomeS466 = _M0L11end__offsetS465;
    _M0L11end__offsetS464 = (int32_t)_M0L7_2aSomeS466;
  }
  _M0L5indexS468 = _M0L13start__offsetS474;
  _M0L5countS469 = 0;
  while (1) {
    int32_t _if__result_3314;
    if (_M0L5indexS468 < _M0L11end__offsetS464) {
      _if__result_3314 = _M0L5countS469 < _M0L3lenS470;
    } else {
      _if__result_3314 = 0;
    }
    if (_if__result_3314) {
      int32_t _M0L2c1S471 = _M0L4selfS467[_M0L5indexS468];
      int32_t _if__result_3315;
      int32_t _M0L6_2atmpS1785;
      int32_t _M0L6_2atmpS1786;
      #line 452 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S471)) {
        int32_t _M0L6_2atmpS1781 = _M0L5indexS468 + 1;
        _if__result_3315 = _M0L6_2atmpS1781 < _M0L11end__offsetS464;
      } else {
        _if__result_3315 = 0;
      }
      if (_if__result_3315) {
        int32_t _M0L6_2atmpS1784 = _M0L5indexS468 + 1;
        int32_t _M0L2c2S472 = _M0L4selfS467[_M0L6_2atmpS1784];
        #line 454 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S472)) {
          int32_t _M0L6_2atmpS1782 = _M0L5indexS468 + 2;
          int32_t _M0L6_2atmpS1783 = _M0L5countS469 + 1;
          _M0L5indexS468 = _M0L6_2atmpS1782;
          _M0L5countS469 = _M0L6_2atmpS1783;
          continue;
        } else {
          #line 457 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_98.data);
        }
      }
      _M0L6_2atmpS1785 = _M0L5indexS468 + 1;
      _M0L6_2atmpS1786 = _M0L5countS469 + 1;
      _M0L5indexS468 = _M0L6_2atmpS1785;
      _M0L5countS469 = _M0L6_2atmpS1786;
      continue;
    } else {
      moonbit_decref(_M0L4selfS467);
      return _M0L5countS469 >= _M0L3lenS470;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS456,
  int32_t _M0L3lenS459,
  int32_t _M0L13start__offsetS463,
  int64_t _M0L11end__offsetS454
) {
  int32_t _M0L11end__offsetS453;
  int32_t _M0L5indexS457;
  int32_t _M0L5countS458;
  #line 413 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS454 == 4294967296ll) {
    _M0L11end__offsetS453 = Moonbit_array_length(_M0L4selfS456);
  } else {
    int64_t _M0L7_2aSomeS455 = _M0L11end__offsetS454;
    _M0L11end__offsetS453 = (int32_t)_M0L7_2aSomeS455;
  }
  _M0L5indexS457 = _M0L13start__offsetS463;
  _M0L5countS458 = 0;
  while (1) {
    int32_t _if__result_3317;
    if (_M0L5indexS457 < _M0L11end__offsetS453) {
      _if__result_3317 = _M0L5countS458 < _M0L3lenS459;
    } else {
      _if__result_3317 = 0;
    }
    if (_if__result_3317) {
      int32_t _M0L2c1S460 = _M0L4selfS456[_M0L5indexS457];
      int32_t _if__result_3318;
      int32_t _M0L6_2atmpS1779;
      int32_t _M0L6_2atmpS1780;
      #line 424 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S460)) {
        int32_t _M0L6_2atmpS1775 = _M0L5indexS457 + 1;
        _if__result_3318 = _M0L6_2atmpS1775 < _M0L11end__offsetS453;
      } else {
        _if__result_3318 = 0;
      }
      if (_if__result_3318) {
        int32_t _M0L6_2atmpS1778 = _M0L5indexS457 + 1;
        int32_t _M0L2c2S461 = _M0L4selfS456[_M0L6_2atmpS1778];
        #line 426 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S461)) {
          int32_t _M0L6_2atmpS1776 = _M0L5indexS457 + 2;
          int32_t _M0L6_2atmpS1777 = _M0L5countS458 + 1;
          _M0L5indexS457 = _M0L6_2atmpS1776;
          _M0L5countS458 = _M0L6_2atmpS1777;
          continue;
        } else {
          #line 429 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_98.data);
        }
      }
      _M0L6_2atmpS1779 = _M0L5indexS457 + 1;
      _M0L6_2atmpS1780 = _M0L5countS458 + 1;
      _M0L5indexS457 = _M0L6_2atmpS1779;
      _M0L5countS458 = _M0L6_2atmpS1780;
      continue;
    } else {
      moonbit_decref(_M0L4selfS456);
      if (_M0L5countS458 == _M0L3lenS459) {
        return _M0L5indexS457 == _M0L11end__offsetS453;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS450
) {
  int32_t _M0L3endS1769;
  int32_t _M0L5startS1770;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1769 = _M0L4selfS450.$2;
  _M0L5startS1770 = _M0L4selfS450.$1;
  moonbit_decref(_M0L4selfS450.$0);
  return _M0L3endS1769 - _M0L5startS1770;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS451
) {
  int32_t _M0L3endS1771;
  int32_t _M0L5startS1772;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1771 = _M0L4selfS451.$2;
  _M0L5startS1772 = _M0L4selfS451.$1;
  moonbit_decref(_M0L4selfS451.$0);
  return _M0L3endS1771 - _M0L5startS1772;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS452
) {
  int32_t _M0L3endS1773;
  int32_t _M0L5startS1774;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1773 = _M0L4selfS452.$2;
  _M0L5startS1774 = _M0L4selfS452.$1;
  moonbit_decref(_M0L4selfS452.$0);
  return _M0L3endS1773 - _M0L5startS1774;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS448,
  int64_t _M0L19start__offset_2eoptS446,
  int64_t _M0L11end__offsetS449
) {
  int32_t _M0L13start__offsetS445;
  if (_M0L19start__offset_2eoptS446 == 4294967296ll) {
    _M0L13start__offsetS445 = 0;
  } else {
    int64_t _M0L7_2aSomeS447 = _M0L19start__offset_2eoptS446;
    _M0L13start__offsetS445 = (int32_t)_M0L7_2aSomeS447;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS448, _M0L13start__offsetS445, _M0L11end__offsetS449);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS443,
  int32_t _M0L13start__offsetS444,
  int64_t _M0L11end__offsetS441
) {
  int32_t _M0L11end__offsetS440;
  int32_t _if__result_3319;
  #line 512 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS441 == 4294967296ll) {
    _M0L11end__offsetS440 = Moonbit_array_length(_M0L4selfS443);
  } else {
    int64_t _M0L7_2aSomeS442 = _M0L11end__offsetS441;
    _M0L11end__offsetS440 = (int32_t)_M0L7_2aSomeS442;
  }
  if (_M0L13start__offsetS444 >= 0) {
    if (_M0L13start__offsetS444 <= _M0L11end__offsetS440) {
      int32_t _M0L6_2atmpS1768 = Moonbit_array_length(_M0L4selfS443);
      _if__result_3319 = _M0L11end__offsetS440 <= _M0L6_2atmpS1768;
    } else {
      _if__result_3319 = 0;
    }
  } else {
    _if__result_3319 = 0;
  }
  if (_if__result_3319) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS444,
                                                 _M0L11end__offsetS440,
                                                 _M0L4selfS443};
  } else {
    moonbit_decref(_M0L4selfS443);
    #line 521 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_99.data);
  }
}

int32_t _M0IPC16string10StringViewPB2Eq5equal(
  struct _M0TPC16string10StringView _M0L4selfS436,
  struct _M0TPC16string10StringView _M0L5otherS437
) {
  int32_t _M0L3lenS435;
  int32_t _M0L6_2atmpS1754;
  #line 279 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  moonbit_incref(_M0L4selfS436.$0);
  #line 280 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS435 = _M0MPC16string10StringView6length(_M0L4selfS436);
  moonbit_incref(_M0L5otherS437.$0);
  #line 281 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L6_2atmpS1754 = _M0MPC16string10StringView6length(_M0L5otherS437);
  if (_M0L3lenS435 == _M0L6_2atmpS1754) {
    moonbit_string_t _M0L3strS1757 = _M0L4selfS436.$0;
    moonbit_string_t _M0L3strS1758 = _M0L5otherS437.$0;
    int32_t _if__result_3320;
    int32_t _M0L1iS438;
    if (_M0L3strS1757 == _M0L3strS1758) {
      int32_t _M0L5startS1755 = _M0L4selfS436.$1;
      int32_t _M0L5startS1756 = _M0L5otherS437.$1;
      _if__result_3320 = _M0L5startS1755 == _M0L5startS1756;
    } else {
      _if__result_3320 = 0;
    }
    if (_if__result_3320) {
      moonbit_decref(_M0L5otherS437.$0);
      moonbit_decref(_M0L4selfS436.$0);
      return 1;
    }
    _M0L1iS438 = 0;
    while (1) {
      if (_M0L1iS438 < _M0L3lenS435) {
        moonbit_string_t _M0L3strS1764 = _M0L4selfS436.$0;
        int32_t _M0L5startS1766 = _M0L4selfS436.$1;
        int32_t _M0L6_2atmpS1765 = _M0L5startS1766 + _M0L1iS438;
        int32_t _M0L6_2atmpS1759 = _M0L3strS1764[_M0L6_2atmpS1765];
        moonbit_string_t _M0L3strS1761 = _M0L5otherS437.$0;
        int32_t _M0L5startS1763 = _M0L5otherS437.$1;
        int32_t _M0L6_2atmpS1762 = _M0L5startS1763 + _M0L1iS438;
        int32_t _M0L6_2atmpS1760 = _M0L3strS1761[_M0L6_2atmpS1762];
        int32_t _M0L6_2atmpS1767;
        if (_M0L6_2atmpS1759 == _M0L6_2atmpS1760) {
          
        } else {
          moonbit_decref(_M0L5otherS437.$0);
          moonbit_decref(_M0L4selfS436.$0);
          return 0;
        }
        _M0L6_2atmpS1767 = _M0L1iS438 + 1;
        _M0L1iS438 = _M0L6_2atmpS1767;
        continue;
      } else {
        moonbit_decref(_M0L5otherS437.$0);
        moonbit_decref(_M0L4selfS436.$0);
      }
      break;
    }
    return 1;
  } else {
    moonbit_decref(_M0L5otherS437.$0);
    moonbit_decref(_M0L4selfS436.$0);
    return 0;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS434
) {
  #line 197 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 198 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L4selfS434);
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS433
) {
  moonbit_string_t _M0L3strS1751;
  int32_t _M0L5startS1752;
  int32_t _M0L3endS1753;
  #line 190 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1751 = _M0L4selfS433.$0;
  _M0L5startS1752 = _M0L4selfS433.$1;
  _M0L3endS1753 = _M0L4selfS433.$2;
  #line 193 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1751, _M0L5startS1752, _M0L3endS1753);
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS430,
  int32_t _M0L5startS428,
  int32_t _M0L3endS429
) {
  int32_t _if__result_3322;
  int32_t _M0L3lenS431;
  int32_t _M0L6_2atmpS1749;
  int32_t _M0L6_2atmpS1750;
  moonbit_bytes_t _M0L5bytesS432;
  moonbit_bytes_t _M0L6_2atmpS1748;
  #line 91 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS428 == 0) {
    int32_t _M0L6_2atmpS1747 = Moonbit_array_length(_M0L3strS430);
    _if__result_3322 = _M0L3endS429 == _M0L6_2atmpS1747;
  } else {
    _if__result_3322 = 0;
  }
  if (_if__result_3322) {
    return _M0L3strS430;
  }
  _M0L3lenS431 = _M0L3endS429 - _M0L5startS428;
  _M0L6_2atmpS1749 = _M0L3lenS431 * 2;
  #line 101 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1750 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS432
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1749, _M0L6_2atmpS1750);
  moonbit_incref(_M0L5bytesS432);
  #line 102 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS432, 0, _M0L3strS430, _M0L5startS428, _M0L3lenS431);
  _M0L6_2atmpS1748 = _M0L5bytesS432;
  #line 103 "/Users/user/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1748, 0, 4294967296ll);
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS423,
  int32_t _M0L6offsetS427,
  int64_t _M0L6lengthS425
) {
  int32_t _M0L3lenS422;
  int32_t _M0L6lengthS424;
  int32_t _if__result_3323;
  #line 76 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS422 = Moonbit_array_length(_M0L4selfS423);
  if (_M0L6lengthS425 == 4294967296ll) {
    _M0L6lengthS424 = _M0L3lenS422 - _M0L6offsetS427;
  } else {
    int64_t _M0L7_2aSomeS426 = _M0L6lengthS425;
    _M0L6lengthS424 = (int32_t)_M0L7_2aSomeS426;
  }
  if (_M0L6offsetS427 >= 0) {
    if (_M0L6lengthS424 >= 0) {
      int32_t _M0L6_2atmpS1746 = _M0L6offsetS427 + _M0L6lengthS424;
      _if__result_3323 = _M0L6_2atmpS1746 <= _M0L3lenS422;
    } else {
      _if__result_3323 = 0;
    }
  } else {
    _if__result_3323 = 0;
  }
  if (_if__result_3323) {
    #line 84 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS423, _M0L6offsetS427, _M0L6lengthS424);
  } else {
    moonbit_decref(_M0L4selfS423);
    #line 83 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS414,
  int32_t _M0L13bytes__offsetS409,
  moonbit_string_t _M0L3strS416,
  int32_t _M0L11str__offsetS412,
  int32_t _M0L6lengthS410
) {
  int32_t _M0L6_2atmpS1745;
  int32_t _M0L6_2atmpS1744;
  int32_t _M0L2e1S408;
  int32_t _M0L6_2atmpS1743;
  int32_t _M0L2e2S411;
  int32_t _M0L4len1S413;
  int32_t _M0L4len2S415;
  int32_t _if__result_3324;
  #line 124 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1745 = _M0L6lengthS410 * 2;
  _M0L6_2atmpS1744 = _M0L13bytes__offsetS409 + _M0L6_2atmpS1745;
  _M0L2e1S408 = _M0L6_2atmpS1744 - 1;
  _M0L6_2atmpS1743 = _M0L11str__offsetS412 + _M0L6lengthS410;
  _M0L2e2S411 = _M0L6_2atmpS1743 - 1;
  _M0L4len1S413 = Moonbit_array_length(_M0L4selfS414);
  _M0L4len2S415 = Moonbit_array_length(_M0L3strS416);
  if (_M0L6lengthS410 >= 0) {
    if (_M0L13bytes__offsetS409 >= 0) {
      if (_M0L2e1S408 < _M0L4len1S413) {
        if (_M0L11str__offsetS412 >= 0) {
          _if__result_3324 = _M0L2e2S411 < _M0L4len2S415;
        } else {
          _if__result_3324 = 0;
        }
      } else {
        _if__result_3324 = 0;
      }
    } else {
      _if__result_3324 = 0;
    }
  } else {
    _if__result_3324 = 0;
  }
  if (_if__result_3324) {
    int32_t _M0L16end__str__offsetS417 =
      _M0L11str__offsetS412 + _M0L6lengthS410;
    int32_t _M0L1iS418 = _M0L11str__offsetS412;
    int32_t _M0L1jS419 = _M0L13bytes__offsetS409;
    while (1) {
      if (_M0L1iS418 < _M0L16end__str__offsetS417) {
        int32_t _M0L6_2atmpS1740 = _M0L3strS416[_M0L1iS418];
        int32_t _M0L6_2atmpS1739 = (int32_t)_M0L6_2atmpS1740;
        uint32_t _M0L1cS420 = *(uint32_t*)&_M0L6_2atmpS1739;
        uint32_t _M0L6_2atmpS1735 = _M0L1cS420 & 255u;
        int32_t _M0L6_2atmpS1734;
        int32_t _M0L6_2atmpS1736;
        uint32_t _M0L6_2atmpS1738;
        int32_t _M0L6_2atmpS1737;
        int32_t _M0L6_2atmpS1741;
        int32_t _M0L6_2atmpS1742;
        #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1734 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1735);
        if (
          _M0L1jS419 < 0 || _M0L1jS419 >= Moonbit_array_length(_M0L4selfS414)
        ) {
          #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS414[_M0L1jS419] = _M0L6_2atmpS1734;
        _M0L6_2atmpS1736 = _M0L1jS419 + 1;
        _M0L6_2atmpS1738 = _M0L1cS420 >> 8;
        #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1737 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1738);
        if (
          _M0L6_2atmpS1736 < 0
          || _M0L6_2atmpS1736 >= Moonbit_array_length(_M0L4selfS414)
        ) {
          #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS414[_M0L6_2atmpS1736] = _M0L6_2atmpS1737;
        _M0L6_2atmpS1741 = _M0L1iS418 + 1;
        _M0L6_2atmpS1742 = _M0L1jS419 + 2;
        _M0L1iS418 = _M0L6_2atmpS1741;
        _M0L1jS419 = _M0L6_2atmpS1742;
        continue;
      } else {
        moonbit_decref(_M0L3strS416);
        moonbit_decref(_M0L4selfS414);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS416);
    moonbit_decref(_M0L4selfS414);
    #line 137 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS407) {
  int32_t _M0L6_2atmpS1733;
  #line 2518 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1733 = *(int32_t*)&_M0L4selfS407;
  return _M0L6_2atmpS1733 & 0xff;
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS405,
  struct _M0TPB6Logger _M0L6loggerS406
) {
  #line 166 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 167 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L4selfS405, _M0L6loggerS406, 1);
  return 0;
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS404) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS404;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS388,
  int32_t _M0L5radixS387
) {
  int32_t _if__result_3326;
  int32_t _M0L12is__negativeS389;
  uint32_t _M0L3numS390;
  uint16_t* _M0L6bufferS391;
  #line 209 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS387 < 2) {
    _if__result_3326 = 1;
  } else {
    _if__result_3326 = _M0L5radixS387 > 36;
  }
  if (_if__result_3326) {
    #line 213 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_100.data);
  }
  if (_M0L4selfS388 == 0) {
    return (moonbit_string_t)moonbit_string_literal_101.data;
  }
  _M0L12is__negativeS389 = _M0L4selfS388 < 0;
  if (_M0L12is__negativeS389) {
    int32_t _M0L6_2atmpS1732 = -_M0L4selfS388;
    _M0L3numS390 = *(uint32_t*)&_M0L6_2atmpS1732;
  } else {
    _M0L3numS390 = *(uint32_t*)&_M0L4selfS388;
  }
  switch (_M0L5radixS387) {
    case 10: {
      int32_t _M0L10digit__lenS392;
      int32_t _M0L6_2atmpS1729;
      int32_t _M0L10total__lenS393;
      uint16_t* _M0L6bufferS394;
      int32_t _M0L12digit__startS395;
      #line 235 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS392 = _M0FPB12dec__count32(_M0L3numS390);
      if (_M0L12is__negativeS389) {
        _M0L6_2atmpS1729 = 1;
      } else {
        _M0L6_2atmpS1729 = 0;
      }
      _M0L10total__lenS393 = _M0L10digit__lenS392 + _M0L6_2atmpS1729;
      _M0L6bufferS394
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS393, 0);
      if (_M0L12is__negativeS389) {
        _M0L12digit__startS395 = 1;
      } else {
        _M0L12digit__startS395 = 0;
      }
      moonbit_incref(_M0L6bufferS394);
      #line 239 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS394, _M0L3numS390, _M0L12digit__startS395, _M0L10total__lenS393);
      _M0L6bufferS391 = _M0L6bufferS394;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS396;
      int32_t _M0L6_2atmpS1730;
      int32_t _M0L10total__lenS397;
      uint16_t* _M0L6bufferS398;
      int32_t _M0L12digit__startS399;
      #line 243 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS396 = _M0FPB12hex__count32(_M0L3numS390);
      if (_M0L12is__negativeS389) {
        _M0L6_2atmpS1730 = 1;
      } else {
        _M0L6_2atmpS1730 = 0;
      }
      _M0L10total__lenS397 = _M0L10digit__lenS396 + _M0L6_2atmpS1730;
      _M0L6bufferS398
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS397, 0);
      if (_M0L12is__negativeS389) {
        _M0L12digit__startS399 = 1;
      } else {
        _M0L12digit__startS399 = 0;
      }
      moonbit_incref(_M0L6bufferS398);
      #line 247 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS398, _M0L3numS390, _M0L12digit__startS399, _M0L10total__lenS397);
      _M0L6bufferS391 = _M0L6bufferS398;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS400;
      int32_t _M0L6_2atmpS1731;
      int32_t _M0L10total__lenS401;
      uint16_t* _M0L6bufferS402;
      int32_t _M0L12digit__startS403;
      #line 251 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS400
      = _M0FPB14radix__count32(_M0L3numS390, _M0L5radixS387);
      if (_M0L12is__negativeS389) {
        _M0L6_2atmpS1731 = 1;
      } else {
        _M0L6_2atmpS1731 = 0;
      }
      _M0L10total__lenS401 = _M0L10digit__lenS400 + _M0L6_2atmpS1731;
      _M0L6bufferS402
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS401, 0);
      if (_M0L12is__negativeS389) {
        _M0L12digit__startS403 = 1;
      } else {
        _M0L12digit__startS403 = 0;
      }
      moonbit_incref(_M0L6bufferS402);
      #line 255 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS402, _M0L3numS390, _M0L12digit__startS403, _M0L10total__lenS401, _M0L5radixS387);
      _M0L6bufferS391 = _M0L6bufferS402;
      break;
    }
  }
  if (_M0L12is__negativeS389) {
    _M0L6bufferS391[0] = 45;
  }
  return _M0L6bufferS391;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS381,
  int32_t _M0L5radixS383
) {
  uint32_t _M0L4baseS382;
  uint32_t _M0L3numS384;
  int32_t _M0L5countS385;
  #line 189 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS381 == 0u) {
    return 1;
  }
  _M0L4baseS382 = *(uint32_t*)&_M0L5radixS383;
  _M0L3numS384 = _M0L5valueS381;
  _M0L5countS385 = 0;
  while (1) {
    if (_M0L3numS384 > 0u) {
      uint32_t _M0L6_2atmpS1727 = _M0L3numS384 / _M0L4baseS382;
      int32_t _M0L6_2atmpS1728 = _M0L5countS385 + 1;
      _M0L3numS384 = _M0L6_2atmpS1727;
      _M0L5countS385 = _M0L6_2atmpS1728;
      continue;
    } else {
      return _M0L5countS385;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS379) {
  #line 177 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS379 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS380;
    int32_t _M0L6_2atmpS1726;
    int32_t _M0L6_2atmpS1725;
    #line 182 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS380 = moonbit_clz32(_M0L5valueS379);
    _M0L6_2atmpS1726 = 31 - _M0L14leading__zerosS380;
    _M0L6_2atmpS1725 = _M0L6_2atmpS1726 / 4;
    return _M0L6_2atmpS1725 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS378) {
  #line 143 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS378 >= 100000u) {
    if (_M0L5valueS378 >= 10000000u) {
      if (_M0L5valueS378 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS378 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS378 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS378 >= 1000u) {
    if (_M0L5valueS378 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS378 >= 100u) {
    return 3;
  } else if (_M0L5valueS378 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS364,
  uint32_t _M0L3numS376,
  int32_t _M0L12digit__startS365,
  int32_t _M0L10total__lenS377
) {
  int32_t _M0L6_2atmpS1724;
  uint32_t _M0L3numS354;
  int32_t _M0L6offsetS355;
  #line 88 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1724 = _M0L10total__lenS377 - _M0L12digit__startS365;
  _M0L3numS354 = _M0L3numS376;
  _M0L6offsetS355 = _M0L6_2atmpS1724;
  while (1) {
    if (_M0L3numS354 >= 10000u) {
      uint32_t _M0L1tS356 = _M0L3numS354 / 10000u;
      uint32_t _M0L6_2atmpS1701 = _M0L3numS354 % 10000u;
      int32_t _M0L1rS357 = *(int32_t*)&_M0L6_2atmpS1701;
      int32_t _M0L2d1S358 = _M0L1rS357 / 100;
      int32_t _M0L2d2S359 = _M0L1rS357 % 100;
      int32_t _M0L6_2atmpS1700 = _M0L2d1S358 / 10;
      int32_t _M0L6_2atmpS1699 = 48 + _M0L6_2atmpS1700;
      int32_t _M0L6d1__hiS360 = (uint16_t)_M0L6_2atmpS1699;
      int32_t _M0L6_2atmpS1698 = _M0L2d1S358 % 10;
      int32_t _M0L6_2atmpS1697 = 48 + _M0L6_2atmpS1698;
      int32_t _M0L6d1__loS361 = (uint16_t)_M0L6_2atmpS1697;
      int32_t _M0L6_2atmpS1696 = _M0L2d2S359 / 10;
      int32_t _M0L6_2atmpS1695 = 48 + _M0L6_2atmpS1696;
      int32_t _M0L6d2__hiS362 = (uint16_t)_M0L6_2atmpS1695;
      int32_t _M0L6_2atmpS1694 = _M0L2d2S359 % 10;
      int32_t _M0L6_2atmpS1693 = 48 + _M0L6_2atmpS1694;
      int32_t _M0L6d2__loS363 = (uint16_t)_M0L6_2atmpS1693;
      int32_t _M0L6_2atmpS1685 = _M0L12digit__startS365 + _M0L6offsetS355;
      int32_t _M0L6_2atmpS1684 = _M0L6_2atmpS1685 - 4;
      int32_t _M0L6_2atmpS1687;
      int32_t _M0L6_2atmpS1686;
      int32_t _M0L6_2atmpS1689;
      int32_t _M0L6_2atmpS1688;
      int32_t _M0L6_2atmpS1691;
      int32_t _M0L6_2atmpS1690;
      int32_t _M0L6_2atmpS1692;
      _M0L6bufferS364[_M0L6_2atmpS1684] = _M0L6d1__hiS360;
      _M0L6_2atmpS1687 = _M0L12digit__startS365 + _M0L6offsetS355;
      _M0L6_2atmpS1686 = _M0L6_2atmpS1687 - 3;
      _M0L6bufferS364[_M0L6_2atmpS1686] = _M0L6d1__loS361;
      _M0L6_2atmpS1689 = _M0L12digit__startS365 + _M0L6offsetS355;
      _M0L6_2atmpS1688 = _M0L6_2atmpS1689 - 2;
      _M0L6bufferS364[_M0L6_2atmpS1688] = _M0L6d2__hiS362;
      _M0L6_2atmpS1691 = _M0L12digit__startS365 + _M0L6offsetS355;
      _M0L6_2atmpS1690 = _M0L6_2atmpS1691 - 1;
      _M0L6bufferS364[_M0L6_2atmpS1690] = _M0L6d2__loS363;
      _M0L6_2atmpS1692 = _M0L6offsetS355 - 4;
      _M0L3numS354 = _M0L1tS356;
      _M0L6offsetS355 = _M0L6_2atmpS1692;
      continue;
    } else {
      int32_t _M0L6_2atmpS1723 = *(int32_t*)&_M0L3numS354;
      int32_t _M0L9remainingS367 = _M0L6_2atmpS1723;
      int32_t _M0L6offsetS368 = _M0L6offsetS355;
      while (1) {
        if (_M0L9remainingS367 >= 100) {
          int32_t _M0L1tS369 = _M0L9remainingS367 / 100;
          int32_t _M0L1dS370 = _M0L9remainingS367 % 100;
          int32_t _M0L6_2atmpS1710 = _M0L1dS370 / 10;
          int32_t _M0L6_2atmpS1709 = 48 + _M0L6_2atmpS1710;
          int32_t _M0L5d__hiS371 = (uint16_t)_M0L6_2atmpS1709;
          int32_t _M0L6_2atmpS1708 = _M0L1dS370 % 10;
          int32_t _M0L6_2atmpS1707 = 48 + _M0L6_2atmpS1708;
          int32_t _M0L5d__loS372 = (uint16_t)_M0L6_2atmpS1707;
          int32_t _M0L6_2atmpS1703 = _M0L12digit__startS365 + _M0L6offsetS368;
          int32_t _M0L6_2atmpS1702 = _M0L6_2atmpS1703 - 2;
          int32_t _M0L6_2atmpS1705;
          int32_t _M0L6_2atmpS1704;
          int32_t _M0L6_2atmpS1706;
          _M0L6bufferS364[_M0L6_2atmpS1702] = _M0L5d__hiS371;
          _M0L6_2atmpS1705 = _M0L12digit__startS365 + _M0L6offsetS368;
          _M0L6_2atmpS1704 = _M0L6_2atmpS1705 - 1;
          _M0L6bufferS364[_M0L6_2atmpS1704] = _M0L5d__loS372;
          _M0L6_2atmpS1706 = _M0L6offsetS368 - 2;
          _M0L9remainingS367 = _M0L1tS369;
          _M0L6offsetS368 = _M0L6_2atmpS1706;
          continue;
        } else if (_M0L9remainingS367 >= 10) {
          int32_t _M0L6_2atmpS1718 = _M0L9remainingS367 / 10;
          int32_t _M0L6_2atmpS1717 = 48 + _M0L6_2atmpS1718;
          int32_t _M0L5d__hiS374 = (uint16_t)_M0L6_2atmpS1717;
          int32_t _M0L6_2atmpS1716 = _M0L9remainingS367 % 10;
          int32_t _M0L6_2atmpS1715 = 48 + _M0L6_2atmpS1716;
          int32_t _M0L5d__loS375 = (uint16_t)_M0L6_2atmpS1715;
          int32_t _M0L6_2atmpS1712 = _M0L12digit__startS365 + _M0L6offsetS368;
          int32_t _M0L6_2atmpS1711 = _M0L6_2atmpS1712 - 2;
          int32_t _M0L6_2atmpS1714;
          int32_t _M0L6_2atmpS1713;
          _M0L6bufferS364[_M0L6_2atmpS1711] = _M0L5d__hiS374;
          _M0L6_2atmpS1714 = _M0L12digit__startS365 + _M0L6offsetS368;
          _M0L6_2atmpS1713 = _M0L6_2atmpS1714 - 1;
          _M0L6bufferS364[_M0L6_2atmpS1713] = _M0L5d__loS375;
          moonbit_decref(_M0L6bufferS364);
        } else {
          int32_t _M0L6_2atmpS1722 = _M0L12digit__startS365 + _M0L6offsetS368;
          int32_t _M0L6_2atmpS1719 = _M0L6_2atmpS1722 - 1;
          int32_t _M0L6_2atmpS1721 = 48 + _M0L9remainingS367;
          int32_t _M0L6_2atmpS1720 = (uint16_t)_M0L6_2atmpS1721;
          _M0L6bufferS364[_M0L6_2atmpS1719] = _M0L6_2atmpS1720;
          moonbit_decref(_M0L6bufferS364);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS344,
  uint32_t _M0L3numS348,
  int32_t _M0L12digit__startS345,
  int32_t _M0L10total__lenS347,
  int32_t _M0L5radixS338
) {
  uint32_t _M0L4baseS337;
  int32_t _M0L6_2atmpS1669;
  int32_t _M0L6_2atmpS1668;
  #line 57 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS337 = *(uint32_t*)&_M0L5radixS338;
  _M0L6_2atmpS1669 = _M0L5radixS338 - 1;
  _M0L6_2atmpS1668 = _M0L5radixS338 & _M0L6_2atmpS1669;
  if (_M0L6_2atmpS1668 == 0) {
    int32_t _M0L5shiftS339;
    uint32_t _M0L4maskS340;
    int32_t _M0L6_2atmpS1676;
    int32_t _M0L6offsetS341;
    uint32_t _M0L1nS342;
    #line 68 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS339 = moonbit_ctz32(_M0L5radixS338);
    _M0L4maskS340 = _M0L4baseS337 - 1u;
    _M0L6_2atmpS1676 = _M0L10total__lenS347 - _M0L12digit__startS345;
    _M0L6offsetS341 = _M0L6_2atmpS1676;
    _M0L1nS342 = _M0L3numS348;
    while (1) {
      if (_M0L1nS342 > 0u) {
        uint32_t _M0L6_2atmpS1675 = _M0L1nS342 & _M0L4maskS340;
        int32_t _M0L5digitS343 = *(int32_t*)&_M0L6_2atmpS1675;
        int32_t _M0L6_2atmpS1672 = _M0L12digit__startS345 + _M0L6offsetS341;
        int32_t _M0L6_2atmpS1670 = _M0L6_2atmpS1672 - 1;
        int32_t _M0L6_2atmpS1671 =
          ((moonbit_string_t)moonbit_string_literal_102.data)[_M0L5digitS343];
        int32_t _M0L6_2atmpS1673;
        uint32_t _M0L6_2atmpS1674;
        _M0L6bufferS344[_M0L6_2atmpS1670] = _M0L6_2atmpS1671;
        _M0L6_2atmpS1673 = _M0L6offsetS341 - 1;
        _M0L6_2atmpS1674 = _M0L1nS342 >> (_M0L5shiftS339 & 31);
        _M0L6offsetS341 = _M0L6_2atmpS1673;
        _M0L1nS342 = _M0L6_2atmpS1674;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS344);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1683 = _M0L10total__lenS347 - _M0L12digit__startS345;
    int32_t _M0L6offsetS349 = _M0L6_2atmpS1683;
    uint32_t _M0L1nS350 = _M0L3numS348;
    while (1) {
      if (_M0L1nS350 > 0u) {
        uint32_t _M0L1qS351 = _M0L1nS350 / _M0L4baseS337;
        uint32_t _M0L6_2atmpS1682 = _M0L1qS351 * _M0L4baseS337;
        uint32_t _M0L6_2atmpS1681 = _M0L1nS350 - _M0L6_2atmpS1682;
        int32_t _M0L5digitS352 = *(int32_t*)&_M0L6_2atmpS1681;
        int32_t _M0L6_2atmpS1679 = _M0L12digit__startS345 + _M0L6offsetS349;
        int32_t _M0L6_2atmpS1677 = _M0L6_2atmpS1679 - 1;
        int32_t _M0L6_2atmpS1678 =
          ((moonbit_string_t)moonbit_string_literal_102.data)[_M0L5digitS352];
        int32_t _M0L6_2atmpS1680;
        _M0L6bufferS344[_M0L6_2atmpS1677] = _M0L6_2atmpS1678;
        _M0L6_2atmpS1680 = _M0L6offsetS349 - 1;
        _M0L6offsetS349 = _M0L6_2atmpS1680;
        _M0L1nS350 = _M0L1qS351;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS344);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS331,
  uint32_t _M0L3numS336,
  int32_t _M0L12digit__startS332,
  int32_t _M0L10total__lenS335
) {
  int32_t _M0L6_2atmpS1667;
  int32_t _M0L6offsetS326;
  uint32_t _M0L1nS327;
  #line 29 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1667 = _M0L10total__lenS335 - _M0L12digit__startS332;
  _M0L6offsetS326 = _M0L6_2atmpS1667;
  _M0L1nS327 = _M0L3numS336;
  while (1) {
    if (_M0L6offsetS326 >= 2) {
      uint32_t _M0L6_2atmpS1664 = _M0L1nS327 & 255u;
      int32_t _M0L9byte__valS328 = *(int32_t*)&_M0L6_2atmpS1664;
      int32_t _M0L2hiS329 = _M0L9byte__valS328 / 16;
      int32_t _M0L2loS330 = _M0L9byte__valS328 % 16;
      int32_t _M0L6_2atmpS1658 = _M0L12digit__startS332 + _M0L6offsetS326;
      int32_t _M0L6_2atmpS1656 = _M0L6_2atmpS1658 - 2;
      int32_t _M0L6_2atmpS1657 =
        ((moonbit_string_t)moonbit_string_literal_102.data)[_M0L2hiS329];
      int32_t _M0L6_2atmpS1661;
      int32_t _M0L6_2atmpS1659;
      int32_t _M0L6_2atmpS1660;
      int32_t _M0L6_2atmpS1662;
      uint32_t _M0L6_2atmpS1663;
      _M0L6bufferS331[_M0L6_2atmpS1656] = _M0L6_2atmpS1657;
      _M0L6_2atmpS1661 = _M0L12digit__startS332 + _M0L6offsetS326;
      _M0L6_2atmpS1659 = _M0L6_2atmpS1661 - 1;
      _M0L6_2atmpS1660
      = ((moonbit_string_t)moonbit_string_literal_102.data)[
        _M0L2loS330
      ];
      _M0L6bufferS331[_M0L6_2atmpS1659] = _M0L6_2atmpS1660;
      _M0L6_2atmpS1662 = _M0L6offsetS326 - 2;
      _M0L6_2atmpS1663 = _M0L1nS327 >> 8;
      _M0L6offsetS326 = _M0L6_2atmpS1662;
      _M0L1nS327 = _M0L6_2atmpS1663;
      continue;
    } else if (_M0L6offsetS326 == 1) {
      uint32_t _M0L6_2atmpS1666 = _M0L1nS327 & 15u;
      int32_t _M0L6nibbleS334 = *(int32_t*)&_M0L6_2atmpS1666;
      int32_t _M0L6_2atmpS1665 =
        ((moonbit_string_t)moonbit_string_literal_102.data)[_M0L6nibbleS334];
      _M0L6bufferS331[_M0L12digit__startS332] = _M0L6_2atmpS1665;
      moonbit_decref(_M0L6bufferS331);
    } else {
      moonbit_decref(_M0L6bufferS331);
    }
    break;
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger _M0L4selfS309,
  struct _M0TWEOs* _M0L4iterS313,
  moonbit_string_t _M0L6prefixS310,
  moonbit_string_t _M0L6suffixS325,
  moonbit_string_t _M0L3sepS316,
  int32_t _M0L8trailingS311
) {
  #line 161 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  if (_M0L4selfS309.$1) {
    moonbit_incref(_M0L4selfS309.$1);
  }
  #line 169 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS309.$0->$method_0(_M0L4selfS309.$1, _M0L6prefixS310);
  if (_M0L8trailingS311) {
    while (1) {
      moonbit_string_t _M0L7_2abindS312;
      moonbit_incref(_M0L4iterS313);
      #line 171 "/Users/user/.moon/lib/core/builtin/traits.mbt"
      _M0L7_2abindS312 = _M0MPB4Iter4nextGsE(_M0L4iterS313);
      if (_M0L7_2abindS312 == 0) {
        moonbit_decref(_M0L3sepS316);
        moonbit_decref(_M0L4iterS313);
        if (_M0L7_2abindS312) {
          moonbit_decref(_M0L7_2abindS312);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS314 = _M0L7_2abindS312;
        moonbit_string_t _M0L4_2axS315 = _M0L7_2aSomeS314;
        if (_M0L4selfS309.$1) {
          moonbit_incref(_M0L4selfS309.$1);
        }
        #line 172 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0MPB6Logger13write__objectGsE(_M0L4selfS309, _M0L4_2axS315);
        moonbit_incref(_M0L3sepS316);
        if (_M0L4selfS309.$1) {
          moonbit_incref(_M0L4selfS309.$1);
        }
        #line 173 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0L4selfS309.$0->$method_0(_M0L4selfS309.$1, _M0L3sepS316);
        continue;
      }
      break;
    }
  } else {
    moonbit_string_t _M0L7_2abindS318;
    moonbit_incref(_M0L4iterS313);
    #line 175 "/Users/user/.moon/lib/core/builtin/traits.mbt"
    _M0L7_2abindS318 = _M0MPB4Iter4nextGsE(_M0L4iterS313);
    if (_M0L7_2abindS318 == 0) {
      if (_M0L7_2abindS318) {
        moonbit_decref(_M0L7_2abindS318);
      }
      moonbit_decref(_M0L3sepS316);
      moonbit_decref(_M0L4iterS313);
    } else {
      moonbit_string_t _M0L7_2aSomeS319 = _M0L7_2abindS318;
      moonbit_string_t _M0L4_2axS320 = _M0L7_2aSomeS319;
      if (_M0L4selfS309.$1) {
        moonbit_incref(_M0L4selfS309.$1);
      }
      #line 176 "/Users/user/.moon/lib/core/builtin/traits.mbt"
      _M0MPB6Logger13write__objectGsE(_M0L4selfS309, _M0L4_2axS320);
      while (1) {
        moonbit_string_t _M0L7_2abindS321;
        moonbit_incref(_M0L4iterS313);
        #line 177 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0L7_2abindS321 = _M0MPB4Iter4nextGsE(_M0L4iterS313);
        if (_M0L7_2abindS321 == 0) {
          if (_M0L7_2abindS321) {
            moonbit_decref(_M0L7_2abindS321);
          }
          moonbit_decref(_M0L3sepS316);
          moonbit_decref(_M0L4iterS313);
        } else {
          moonbit_string_t _M0L7_2aSomeS322 = _M0L7_2abindS321;
          moonbit_string_t _M0L4_2axS323 = _M0L7_2aSomeS322;
          moonbit_incref(_M0L3sepS316);
          if (_M0L4selfS309.$1) {
            moonbit_incref(_M0L4selfS309.$1);
          }
          #line 178 "/Users/user/.moon/lib/core/builtin/traits.mbt"
          _M0L4selfS309.$0->$method_0(_M0L4selfS309.$1, _M0L3sepS316);
          if (_M0L4selfS309.$1) {
            moonbit_incref(_M0L4selfS309.$1);
          }
          #line 179 "/Users/user/.moon/lib/core/builtin/traits.mbt"
          _M0MPB6Logger13write__objectGsE(_M0L4selfS309, _M0L4_2axS323);
          continue;
        }
        break;
      }
    }
  }
  #line 182 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS309.$0->$method_0(_M0L4selfS309.$1, _M0L6suffixS325);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS308) {
  struct _M0TWEOs* _M0L7_2afuncS307;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS307 = _M0L4selfS308;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS307->code(_M0L7_2afuncS307);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE* _M0L4selfS304
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS303;
  struct _M0TPB6Logger _M0L6_2atmpS1654;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS303 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS303);
  _M0L6_2atmpS1654
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS303
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L4selfS304, _M0L6_2atmpS1654);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS303);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS306
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS305;
  struct _M0TPB6Logger _M0L6_2atmpS1655;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS305 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS305);
  _M0L6_2atmpS1655
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS305
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS306, _M0L6_2atmpS1655);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS305);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS302
) {
  int32_t _result_3335;
  #line 98 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _result_3335 = _M0L4selfS302.$1;
  moonbit_decref(_M0L4selfS302.$0);
  return _result_3335;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS301
) {
  #line 91 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS301.$0;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS297,
  moonbit_string_t _M0L5valueS298,
  int32_t _M0L5startS299,
  int32_t _M0L3lenS300
) {
  int32_t _M0L6_2atmpS1653;
  int64_t _M0L6_2atmpS1652;
  struct _M0TPC16string10StringView _M0L6_2atmpS1651;
  #line 102 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1653 = _M0L5startS299 + _M0L3lenS300;
  _M0L6_2atmpS1652 = (int64_t)_M0L6_2atmpS1653;
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1651
  = _M0MPC16string6String11sub_2einner(_M0L5valueS298, _M0L5startS299, _M0L6_2atmpS1652);
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS297, _M0L6_2atmpS1651);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS290,
  int32_t _M0L5startS296,
  int64_t _M0L3endS292
) {
  int32_t _M0L3lenS289;
  int32_t _M0L3endS291;
  int32_t _M0L5startS295;
  int32_t _if__result_3336;
  #line 653 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS289 = Moonbit_array_length(_M0L4selfS290);
  if (_M0L3endS292 == 4294967296ll) {
    _M0L3endS291 = _M0L3lenS289;
  } else {
    int64_t _M0L7_2aSomeS293 = _M0L3endS292;
    int32_t _M0L6_2aendS294 = (int32_t)_M0L7_2aSomeS293;
    if (_M0L6_2aendS294 < 0) {
      _M0L3endS291 = _M0L3lenS289 + _M0L6_2aendS294;
    } else {
      _M0L3endS291 = _M0L6_2aendS294;
    }
  }
  if (_M0L5startS296 < 0) {
    _M0L5startS295 = _M0L3lenS289 + _M0L5startS296;
  } else {
    _M0L5startS295 = _M0L5startS296;
  }
  if (_M0L5startS295 >= 0) {
    if (_M0L5startS295 <= _M0L3endS291) {
      _if__result_3336 = _M0L3endS291 <= _M0L3lenS289;
    } else {
      _if__result_3336 = 0;
    }
  } else {
    _if__result_3336 = 0;
  }
  if (_if__result_3336) {
    if (_M0L5startS295 < _M0L3lenS289) {
      int32_t _M0L6_2atmpS1648 = _M0L4selfS290[_M0L5startS295];
      int32_t _M0L6_2atmpS1647;
      #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1647
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1648);
      if (!_M0L6_2atmpS1647) {
        
      } else {
        #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS291 < _M0L3lenS289) {
      int32_t _M0L6_2atmpS1650 = _M0L4selfS290[_M0L3endS291];
      int32_t _M0L6_2atmpS1649;
      #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1649
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1650);
      if (!_M0L6_2atmpS1649) {
        
      } else {
        #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS295,
                                                 _M0L3endS291,
                                                 _M0L4selfS290};
  } else {
    moonbit_decref(_M0L4selfS290);
    #line 661 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS286) {
  struct _M0TPB6Hasher* _M0L1hS285;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS285 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS285);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS285, _M0L4selfS286);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS285);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS288
) {
  struct _M0TPB6Hasher* _M0L1hS287;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS287 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS287);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS287, _M0L4selfS288);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS287);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS283) {
  int32_t _M0L4seedS282;
  if (_M0L10seed_2eoptS283 == 4294967296ll) {
    _M0L4seedS282 = 0;
  } else {
    int64_t _M0L7_2aSomeS284 = _M0L10seed_2eoptS283;
    _M0L4seedS282 = (int32_t)_M0L7_2aSomeS284;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS282);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS281) {
  uint32_t _M0L6_2atmpS1646;
  uint32_t _M0L6_2atmpS1645;
  struct _M0TPB6Hasher* _block_3337;
  #line 75 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1646 = *(uint32_t*)&_M0L4seedS281;
  _M0L6_2atmpS1645 = _M0L6_2atmpS1646 + 374761393u;
  _block_3337
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3337)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3337->$0 = _M0L6_2atmpS1645;
  return _block_3337;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS280) {
  uint32_t _M0L6_2atmpS1644;
  #line 435 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1644 = _M0MPB6Hasher9avalanche(_M0L4selfS280);
  return *(int32_t*)&_M0L6_2atmpS1644;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS279) {
  uint32_t _M0Lm3accS278;
  uint32_t _M0L6_2atmpS1633;
  uint32_t _M0L6_2atmpS1635;
  uint32_t _M0L6_2atmpS1634;
  uint32_t _M0L6_2atmpS1636;
  uint32_t _M0L6_2atmpS1637;
  uint32_t _M0L6_2atmpS1639;
  uint32_t _M0L6_2atmpS1638;
  uint32_t _M0L6_2atmpS1640;
  uint32_t _M0L6_2atmpS1641;
  uint32_t _M0L6_2atmpS1643;
  uint32_t _M0L6_2atmpS1642;
  #line 440 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS278 = _M0L4selfS279->$0;
  moonbit_decref(_M0L4selfS279);
  _M0L6_2atmpS1633 = _M0Lm3accS278;
  _M0L6_2atmpS1635 = _M0Lm3accS278;
  _M0L6_2atmpS1634 = _M0L6_2atmpS1635 >> 15;
  _M0Lm3accS278 = _M0L6_2atmpS1633 ^ _M0L6_2atmpS1634;
  _M0L6_2atmpS1636 = _M0Lm3accS278;
  _M0Lm3accS278 = _M0L6_2atmpS1636 * 2246822519u;
  _M0L6_2atmpS1637 = _M0Lm3accS278;
  _M0L6_2atmpS1639 = _M0Lm3accS278;
  _M0L6_2atmpS1638 = _M0L6_2atmpS1639 >> 13;
  _M0Lm3accS278 = _M0L6_2atmpS1637 ^ _M0L6_2atmpS1638;
  _M0L6_2atmpS1640 = _M0Lm3accS278;
  _M0Lm3accS278 = _M0L6_2atmpS1640 * 3266489917u;
  _M0L6_2atmpS1641 = _M0Lm3accS278;
  _M0L6_2atmpS1643 = _M0Lm3accS278;
  _M0L6_2atmpS1642 = _M0L6_2atmpS1643 >> 16;
  _M0Lm3accS278 = _M0L6_2atmpS1641 ^ _M0L6_2atmpS1642;
  return _M0Lm3accS278;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS276,
  moonbit_string_t _M0L1yS277
) {
  int32_t _M0L6_2atmpS1632;
  #line 23 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 24 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1632 = moonbit_val_array_equal(_M0L1xS276, _M0L1yS277);
  moonbit_decref(_M0L1yS277);
  moonbit_decref(_M0L1xS276);
  return !_M0L6_2atmpS1632;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS273,
  int32_t _M0L5valueS272
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS272, _M0L4selfS273);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS275,
  moonbit_string_t _M0L5valueS274
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS274, _M0L4selfS275);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS270,
  int32_t _M0L5valueS271
) {
  uint32_t _M0L6_2atmpS1631;
  #line 187 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1631 = *(uint32_t*)&_M0L5valueS271;
  #line 188 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS270, _M0L6_2atmpS1631);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS260,
  moonbit_string_t _M0L7contentS261,
  moonbit_string_t _M0L3locS263,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS265
) {
  moonbit_string_t _M0L6actualS259;
  #line 184 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 191 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6actualS259 = _M0L3objS260.$0->$method_1(_M0L3objS260.$1);
  moonbit_incref(_M0L7contentS261);
  moonbit_incref(_M0L6actualS259);
  #line 192 "/Users/user/.moon/lib/core/builtin/console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS259, _M0L7contentS261)
  ) {
    moonbit_string_t _M0L3locS262;
    moonbit_string_t _M0L9args__locS264;
    moonbit_string_t _M0L15expect__escapedS266;
    moonbit_string_t _M0L15actual__escapedS267;
    moonbit_string_t _M0L6_2atmpS1629;
    moonbit_string_t _M0L6_2atmpS1628;
    moonbit_string_t _M0L6_2atmpS1627;
    moonbit_string_t _M0L14expect__base64S268;
    moonbit_string_t _M0L6_2atmpS1626;
    moonbit_string_t _M0L6_2atmpS1625;
    moonbit_string_t _M0L6_2atmpS1624;
    moonbit_string_t _M0L14actual__base64S269;
    moonbit_string_t _M0L6_2atmpS1623;
    moonbit_string_t _M0L6_2atmpS1622;
    moonbit_string_t _M0L6_2atmpS1620;
    moonbit_string_t _M0L6_2atmpS1621;
    moonbit_string_t _M0L6_2atmpS1619;
    moonbit_string_t _M0L6_2atmpS1617;
    moonbit_string_t _M0L6_2atmpS1618;
    moonbit_string_t _M0L6_2atmpS1616;
    moonbit_string_t _M0L6_2atmpS1614;
    moonbit_string_t _M0L6_2atmpS1615;
    moonbit_string_t _M0L6_2atmpS1613;
    moonbit_string_t _M0L6_2atmpS1611;
    moonbit_string_t _M0L6_2atmpS1612;
    moonbit_string_t _M0L6_2atmpS1610;
    moonbit_string_t _M0L6_2atmpS1608;
    moonbit_string_t _M0L6_2atmpS1609;
    moonbit_string_t _M0L6_2atmpS1607;
    moonbit_string_t _M0L6_2atmpS1606;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1605;
    struct moonbit_result_0 _result_3338;
    #line 193 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L3locS262 = _M0MPB9SourceLoc16to__json__string(_M0L3locS263);
    #line 194 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L9args__locS264 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS265);
    moonbit_incref(_M0L7contentS261);
    #line 195 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L15expect__escapedS266
    = _M0MPC16string6String14escape_2einner(_M0L7contentS261, 1);
    moonbit_incref(_M0L6actualS259);
    #line 196 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L15actual__escapedS267
    = _M0MPC16string6String14escape_2einner(_M0L6actualS259, 1);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1629
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS261);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1628
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1629);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1627
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_103.data, _M0L6_2atmpS1628);
    moonbit_decref(_M0L6_2atmpS1628);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14expect__base64S268
    = moonbit_add_string(_M0L6_2atmpS1627, (moonbit_string_t)moonbit_string_literal_103.data);
    moonbit_decref(_M0L6_2atmpS1627);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1626
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS259);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1625
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1626);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1624
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_103.data, _M0L6_2atmpS1625);
    moonbit_decref(_M0L6_2atmpS1625);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14actual__base64S269
    = moonbit_add_string(_M0L6_2atmpS1624, (moonbit_string_t)moonbit_string_literal_103.data);
    moonbit_decref(_M0L6_2atmpS1624);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1623 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS262);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1622
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_104.data, _M0L6_2atmpS1623);
    moonbit_decref(_M0L6_2atmpS1623);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1620
    = moonbit_add_string(_M0L6_2atmpS1622, (moonbit_string_t)moonbit_string_literal_105.data);
    moonbit_decref(_M0L6_2atmpS1622);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1621
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS264);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1619 = moonbit_add_string(_M0L6_2atmpS1620, _M0L6_2atmpS1621);
    moonbit_decref(_M0L6_2atmpS1621);
    moonbit_decref(_M0L6_2atmpS1620);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1617
    = moonbit_add_string(_M0L6_2atmpS1619, (moonbit_string_t)moonbit_string_literal_106.data);
    moonbit_decref(_M0L6_2atmpS1619);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1618
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS266);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1616 = moonbit_add_string(_M0L6_2atmpS1617, _M0L6_2atmpS1618);
    moonbit_decref(_M0L6_2atmpS1618);
    moonbit_decref(_M0L6_2atmpS1617);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1614
    = moonbit_add_string(_M0L6_2atmpS1616, (moonbit_string_t)moonbit_string_literal_107.data);
    moonbit_decref(_M0L6_2atmpS1616);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1615
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS267);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1613 = moonbit_add_string(_M0L6_2atmpS1614, _M0L6_2atmpS1615);
    moonbit_decref(_M0L6_2atmpS1615);
    moonbit_decref(_M0L6_2atmpS1614);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1611
    = moonbit_add_string(_M0L6_2atmpS1613, (moonbit_string_t)moonbit_string_literal_108.data);
    moonbit_decref(_M0L6_2atmpS1613);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1612
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S268);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1610 = moonbit_add_string(_M0L6_2atmpS1611, _M0L6_2atmpS1612);
    moonbit_decref(_M0L6_2atmpS1612);
    moonbit_decref(_M0L6_2atmpS1611);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1608
    = moonbit_add_string(_M0L6_2atmpS1610, (moonbit_string_t)moonbit_string_literal_109.data);
    moonbit_decref(_M0L6_2atmpS1610);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1609
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S269);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1607 = moonbit_add_string(_M0L6_2atmpS1608, _M0L6_2atmpS1609);
    moonbit_decref(_M0L6_2atmpS1609);
    moonbit_decref(_M0L6_2atmpS1608);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1606
    = moonbit_add_string(_M0L6_2atmpS1607, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1607);
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1605
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1605)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1605)->$0
    = _M0L6_2atmpS1606;
    _result_3338.tag = 0;
    _result_3338.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1605;
    return _result_3338;
  } else {
    int32_t _M0L6_2atmpS1630;
    struct moonbit_result_0 _result_3339;
    moonbit_decref(_M0L9args__locS265);
    moonbit_decref(_M0L3locS263);
    moonbit_decref(_M0L7contentS261);
    moonbit_decref(_M0L6actualS259);
    _M0L6_2atmpS1630 = 0;
    _result_3339.tag = 1;
    _result_3339.data.ok = _M0L6_2atmpS1630;
    return _result_3339;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS252
) {
  struct _M0TPB13StringBuilder* _M0L3bufS250;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS251;
  int32_t _M0L7_2abindS253;
  int32_t _M0L1iS254;
  #line 124 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 125 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L3bufS250 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS251 = _M0L4selfS252;
  moonbit_incref(_M0L3bufS250);
  #line 127 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS250, 91);
  _M0L7_2abindS253 = _M0L7_2aselfS251->$1;
  _M0L1iS254 = 0;
  while (1) {
    if (_M0L1iS254 < _M0L7_2abindS253) {
      moonbit_string_t* _M0L3bufS1604 = _M0L7_2aselfS251->$0;
      moonbit_string_t _M0L4itemS255 =
        (moonbit_string_t)_M0L3bufS1604[_M0L1iS254];
      int32_t _M0L6_2atmpS1603;
      if (_M0L1iS254 != 0) {
        if (_M0L4itemS255) {
          moonbit_incref(_M0L4itemS255);
        }
        moonbit_incref(_M0L3bufS250);
        #line 130 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, (moonbit_string_t)moonbit_string_literal_96.data);
      } else if (_M0L4itemS255) {
        moonbit_incref(_M0L4itemS255);
      }
      if (_M0L4itemS255 == 0) {
        if (_M0L4itemS255) {
          moonbit_decref(_M0L4itemS255);
        }
        moonbit_incref(_M0L3bufS250);
        #line 133 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, (moonbit_string_t)moonbit_string_literal_110.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS256 = _M0L4itemS255;
        moonbit_string_t _M0L6_2alocS257 = _M0L7_2aSomeS256;
        moonbit_string_t _M0L6_2atmpS1602;
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1602
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS257);
        moonbit_incref(_M0L3bufS250);
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, _M0L6_2atmpS1602);
      }
      _M0L6_2atmpS1603 = _M0L1iS254 + 1;
      _M0L1iS254 = _M0L6_2atmpS1603;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS251);
    }
    break;
  }
  moonbit_incref(_M0L3bufS250);
  #line 137 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS250, 93);
  #line 138 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS250);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS249
) {
  moonbit_string_t _M0L6_2atmpS1601;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1600;
  #line 95 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1601 = _M0L4selfS249;
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1600 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1601);
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1600);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L2sbS247;
  struct _M0TPC16string10StringView _M0L8filenameS1586;
  struct _M0TPC16string10StringView _M0L11start__lineS1589;
  moonbit_string_t _M0L6_2atmpS1588;
  moonbit_string_t _M0L6_2atmpS1587;
  struct _M0TPC16string10StringView _M0L13start__columnS1592;
  moonbit_string_t _M0L6_2atmpS1591;
  moonbit_string_t _M0L6_2atmpS1590;
  struct _M0TPC16string10StringView _M0L9end__lineS1595;
  moonbit_string_t _M0L6_2atmpS1594;
  moonbit_string_t _M0L6_2atmpS1593;
  struct _M0TPC16string10StringView _M0L8_2afieldS2856;
  int32_t _M0L6_2acntS3094;
  struct _M0TPC16string10StringView _M0L11end__columnS1599;
  moonbit_string_t _M0L6_2atmpS1598;
  moonbit_string_t _M0L6_2atmpS1597;
  moonbit_string_t _M0L6_2atmpS1596;
  #line 82 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 83 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L2sbS247);
  #line 84 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, (moonbit_string_t)moonbit_string_literal_111.data);
  _M0L8filenameS1586
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$0_1, _M0L4selfS248->$0_2, _M0L4selfS248->$0_0
  };
  moonbit_incref(_M0L8filenameS1586.$0);
  moonbit_incref(_M0L2sbS247);
  #line 85 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS247, _M0L8filenameS1586);
  _M0L11start__lineS1589
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$1_1, _M0L4selfS248->$1_2, _M0L4selfS248->$1_0
  };
  moonbit_incref(_M0L11start__lineS1589.$0);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1588
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1589);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1587
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_112.data, _M0L6_2atmpS1588);
  moonbit_decref(_M0L6_2atmpS1588);
  moonbit_incref(_M0L2sbS247);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1587);
  _M0L13start__columnS1592
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$2_1, _M0L4selfS248->$2_2, _M0L4selfS248->$2_0
  };
  moonbit_incref(_M0L13start__columnS1592.$0);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1591
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1592);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1590
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_113.data, _M0L6_2atmpS1591);
  moonbit_decref(_M0L6_2atmpS1591);
  moonbit_incref(_M0L2sbS247);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1590);
  _M0L9end__lineS1595
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$3_1, _M0L4selfS248->$3_2, _M0L4selfS248->$3_0
  };
  moonbit_incref(_M0L9end__lineS1595.$0);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1594
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1595);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1593
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_114.data, _M0L6_2atmpS1594);
  moonbit_decref(_M0L6_2atmpS1594);
  moonbit_incref(_M0L2sbS247);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1593);
  _M0L8_2afieldS2856
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$4_1, _M0L4selfS248->$4_2, _M0L4selfS248->$4_0
  };
  _M0L6_2acntS3094 = Moonbit_object_header(_M0L4selfS248)->rc;
  if (_M0L6_2acntS3094 > 1) {
    int32_t _M0L11_2anew__cntS3099 = _M0L6_2acntS3094 - 1;
    Moonbit_object_header(_M0L4selfS248)->rc = _M0L11_2anew__cntS3099;
    moonbit_incref(_M0L8_2afieldS2856.$0);
  } else if (_M0L6_2acntS3094 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3098 =
      (struct _M0TPC16string10StringView){_M0L4selfS248->$3_1,
                                            _M0L4selfS248->$3_2,
                                            _M0L4selfS248->$3_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3097;
    struct _M0TPC16string10StringView _M0L8_2afieldS3096;
    struct _M0TPC16string10StringView _M0L8_2afieldS3095;
    moonbit_decref(_M0L8_2afieldS3098.$0);
    _M0L8_2afieldS3097
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$2_1, _M0L4selfS248->$2_2, _M0L4selfS248->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3097.$0);
    _M0L8_2afieldS3096
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$1_1, _M0L4selfS248->$1_2, _M0L4selfS248->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3096.$0);
    _M0L8_2afieldS3095
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$0_1, _M0L4selfS248->$0_2, _M0L4selfS248->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3095.$0);
    #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS248);
  }
  _M0L11end__columnS1599 = _M0L8_2afieldS2856;
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1598
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1599);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1597
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_115.data, _M0L6_2atmpS1598);
  moonbit_decref(_M0L6_2atmpS1598);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1596
  = moonbit_add_string(_M0L6_2atmpS1597, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1597);
  moonbit_incref(_M0L2sbS247);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1596);
  #line 90 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS247);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS246,
  moonbit_string_t _M0L3strS245
) {
  int32_t _M0L8str__lenS244;
  int32_t _M0L3lenS1581;
  int32_t _M0L6_2atmpS1580;
  uint16_t* _M0L4dataS1582;
  int32_t _M0L3lenS1583;
  int32_t _M0L3lenS1585;
  int32_t _M0L6_2atmpS1584;
  #line 81 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS244 = Moonbit_array_length(_M0L3strS245);
  _M0L3lenS1581 = _M0L4selfS246->$1;
  _M0L6_2atmpS1580 = _M0L3lenS1581 + _M0L8str__lenS244;
  moonbit_incref(_M0L4selfS246);
  #line 83 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS246, _M0L6_2atmpS1580);
  _M0L4dataS1582 = _M0L4selfS246->$0;
  _M0L3lenS1583 = _M0L4selfS246->$1;
  moonbit_incref(_M0L4dataS1582);
  #line 84 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1582, _M0L3lenS1583, _M0L3strS245, 0, _M0L8str__lenS244);
  _M0L3lenS1585 = _M0L4selfS246->$1;
  _M0L6_2atmpS1584 = _M0L3lenS1585 + _M0L8str__lenS244;
  _M0L4selfS246->$1 = _M0L6_2atmpS1584;
  moonbit_decref(_M0L4selfS246);
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS240,
  int32_t _M0L11dst__offsetS243,
  moonbit_string_t _M0L3strS241,
  int32_t _M0L11str__offsetS236,
  int32_t _M0L3lenS237
) {
  int32_t _M0L16end__str__offsetS235;
  int32_t _M0L1iS238;
  int32_t _M0L1jS239;
  #line 66 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS235 = _M0L11str__offsetS236 + _M0L3lenS237;
  _M0L1iS238 = _M0L11str__offsetS236;
  _M0L1jS239 = _M0L11dst__offsetS243;
  while (1) {
    if (_M0L1iS238 < _M0L16end__str__offsetS235) {
      int32_t _M0L6_2atmpS1577 = _M0L3strS241[_M0L1iS238];
      int32_t _M0L6_2atmpS1578;
      int32_t _M0L6_2atmpS1579;
      if (
        _M0L1jS239 < 0 || _M0L1jS239 >= Moonbit_array_length(_M0L4selfS240)
      ) {
        #line 75 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS240[_M0L1jS239] = _M0L6_2atmpS1577;
      _M0L6_2atmpS1578 = _M0L1iS238 + 1;
      _M0L6_2atmpS1579 = _M0L1jS239 + 1;
      _M0L1iS238 = _M0L6_2atmpS1578;
      _M0L1jS239 = _M0L6_2atmpS1579;
      continue;
    } else {
      moonbit_decref(_M0L3strS241);
      moonbit_decref(_M0L4selfS240);
    }
    break;
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS234,
  struct _M0TPC16string10StringView _M0L3objS233
) {
  struct _M0TPB6Logger _M0L6_2atmpS1576;
  #line 17 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1576
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS234
  };
  #line 21 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS233, _M0L6_2atmpS1576);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS178
) {
  int32_t _M0L6_2atmpS1575;
  struct _M0TPC16string10StringView _M0L7_2abindS177;
  moonbit_string_t _M0L7_2adataS179;
  int32_t _M0L8_2astartS180;
  int32_t _M0L6_2atmpS1574;
  int32_t _M0L6_2aendS181;
  int32_t _M0Lm9_2acursorS182;
  int32_t _M0Lm13accept__stateS183;
  int32_t _M0Lm10match__endS184;
  int32_t _M0Lm20match__tag__saver__0S185;
  int32_t _M0Lm20match__tag__saver__1S186;
  int32_t _M0Lm20match__tag__saver__2S187;
  int32_t _M0Lm20match__tag__saver__3S188;
  int32_t _M0Lm20match__tag__saver__4S189;
  int32_t _M0Lm6tag__0S190;
  int32_t _M0Lm9tag__0__1S191;
  int32_t _M0Lm9tag__0__2S192;
  int32_t _M0Lm6tag__2S193;
  int32_t _M0Lm6tag__1S194;
  int32_t _M0Lm9tag__1__1S195;
  int32_t _M0Lm6tag__4S196;
  int32_t _M0Lm6tag__3S197;
  int32_t _M0L6_2atmpS1533;
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1575 = Moonbit_array_length(_M0L4reprS178);
  _M0L7_2abindS177
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1575, _M0L4reprS178
  };
  moonbit_incref(_M0L7_2abindS177.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS179 = _M0MPC16string10StringView4data(_M0L7_2abindS177);
  moonbit_incref(_M0L7_2abindS177.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS180
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS177);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1574 = _M0MPC16string10StringView6length(_M0L7_2abindS177);
  _M0L6_2aendS181 = _M0L8_2astartS180 + _M0L6_2atmpS1574;
  _M0Lm9_2acursorS182 = _M0L8_2astartS180;
  _M0Lm13accept__stateS183 = -1;
  _M0Lm10match__endS184 = -1;
  _M0Lm20match__tag__saver__0S185 = -1;
  _M0Lm20match__tag__saver__1S186 = -1;
  _M0Lm20match__tag__saver__2S187 = -1;
  _M0Lm20match__tag__saver__3S188 = -1;
  _M0Lm20match__tag__saver__4S189 = -1;
  _M0Lm6tag__0S190 = -1;
  _M0Lm9tag__0__1S191 = -1;
  _M0Lm9tag__0__2S192 = -1;
  _M0Lm6tag__2S193 = -1;
  _M0Lm6tag__1S194 = -1;
  _M0Lm9tag__1__1S195 = -1;
  _M0Lm6tag__4S196 = -1;
  _M0Lm6tag__3S197 = -1;
  _M0L6_2atmpS1533 = _M0Lm9_2acursorS182;
  if (_M0L6_2atmpS1533 < _M0L6_2aendS181) {
    int32_t _M0L6_2atmpS1534 = _M0Lm9_2acursorS182;
    int32_t _M0L12dispatch__15S205;
    _M0Lm9_2acursorS182 = _M0L6_2atmpS1534 + 1;
    _M0L12dispatch__15S205 = 0;
    loop__label__15_208:;
    while (1) {
      int32_t _M0L6_2atmpS1538;
      int32_t _M0L6_2atmpS1535;
      switch (_M0L12dispatch__15S205) {
        case 6: {
          int32_t _M0L6_2atmpS1541;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1541 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1541 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1543 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS213;
            int32_t _M0L6_2atmpS1542;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS213
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1543);
            _M0L6_2atmpS1542 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1542 + 1;
            if (_M0L10next__charS213 == 58) {
              _M0L12dispatch__15S205 = 1;
              goto loop__label__15_208;
            } else {
              _M0L12dispatch__15S205 = 6;
              goto loop__label__15_208;
            }
          } else {
            goto join_210;
          }
          break;
        }
        
        case 3: {
          int32_t _M0L6_2atmpS1544;
          _M0Lm9tag__0__2S192 = _M0Lm9tag__0__1S191;
          _M0Lm9tag__0__1S191 = _M0Lm6tag__0S190;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1544 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1544 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1549 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS215;
            int32_t _M0L6_2atmpS1545;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS215
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1549);
            _M0L6_2atmpS1545 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1545 + 1;
            if (_M0L10next__charS215 < 58) {
              if (_M0L10next__charS215 < 48) {
                goto join_214;
              } else {
                int32_t _M0L6_2atmpS1546;
                _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
                _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
                _M0Lm6tag__2S193 = _M0Lm9_2acursorS182;
                _M0L6_2atmpS1546 = _M0Lm9_2acursorS182;
                if (_M0L6_2atmpS1546 < _M0L6_2aendS181) {
                  int32_t _M0L6_2atmpS1548 = _M0Lm9_2acursorS182;
                  int32_t _M0L10next__charS217;
                  int32_t _M0L6_2atmpS1547;
                  moonbit_incref(_M0L7_2adataS179);
                  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS217
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1548);
                  _M0L6_2atmpS1547 = _M0Lm9_2acursorS182;
                  _M0Lm9_2acursorS182 = _M0L6_2atmpS1547 + 1;
                  if (_M0L10next__charS217 < 48) {
                    if (_M0L10next__charS217 == 45) {
                      goto join_206;
                    } else {
                      goto join_216;
                    }
                  } else if (_M0L10next__charS217 > 57) {
                    if (_M0L10next__charS217 < 59) {
                      _M0L12dispatch__15S205 = 3;
                      goto loop__label__15_208;
                    } else {
                      goto join_216;
                    }
                  } else {
                    _M0L12dispatch__15S205 = 7;
                    goto loop__label__15_208;
                  }
                  join_216:;
                  _M0L12dispatch__15S205 = 0;
                  goto loop__label__15_208;
                } else {
                  goto join_198;
                }
              }
            } else if (_M0L10next__charS215 > 58) {
              goto join_214;
            } else {
              _M0L12dispatch__15S205 = 1;
              goto loop__label__15_208;
            }
            join_214:;
            _M0L12dispatch__15S205 = 0;
            goto loop__label__15_208;
          } else {
            goto join_198;
          }
          break;
        }
        
        case 7: {
          int32_t _M0L6_2atmpS1550;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
          _M0Lm6tag__2S193 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1550 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1550 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1552 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS219;
            int32_t _M0L6_2atmpS1551;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS219
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1552);
            _M0L6_2atmpS1551 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1551 + 1;
            if (_M0L10next__charS219 < 48) {
              if (_M0L10next__charS219 == 45) {
                goto join_206;
              } else {
                goto join_218;
              }
            } else if (_M0L10next__charS219 > 57) {
              if (_M0L10next__charS219 < 59) {
                _M0L12dispatch__15S205 = 3;
                goto loop__label__15_208;
              } else {
                goto join_218;
              }
            } else {
              _M0L12dispatch__15S205 = 7;
              goto loop__label__15_208;
            }
            join_218:;
            _M0L12dispatch__15S205 = 0;
            goto loop__label__15_208;
          } else {
            goto join_198;
          }
          break;
        }
        
        case 5: {
          int32_t _M0L6_2atmpS1553;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
          _M0Lm6tag__4S196 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1553 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1553 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1555 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS221;
            int32_t _M0L6_2atmpS1554;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS221
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1555);
            _M0L6_2atmpS1554 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1554 + 1;
            if (_M0L10next__charS221 < 59) {
              if (_M0L10next__charS221 < 48) {
                goto join_220;
              } else if (_M0L10next__charS221 > 57) {
                _M0L12dispatch__15S205 = 3;
                goto loop__label__15_208;
              } else {
                _M0L12dispatch__15S205 = 5;
                goto loop__label__15_208;
              }
            } else if (_M0L10next__charS221 > 63) {
              if (_M0L10next__charS221 < 65) {
                goto join_211;
              } else {
                goto join_220;
              }
            } else {
              goto join_220;
            }
            join_220:;
            _M0L12dispatch__15S205 = 0;
            goto loop__label__15_208;
          } else {
            goto join_198;
          }
          break;
        }
        
        case 1: {
          int32_t _M0L6_2atmpS1556;
          _M0Lm9tag__0__1S191 = _M0Lm6tag__0S190;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1556 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1556 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1558 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS223;
            int32_t _M0L6_2atmpS1557;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS223
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1558);
            _M0L6_2atmpS1557 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1557 + 1;
            if (_M0L10next__charS223 < 58) {
              if (_M0L10next__charS223 < 48) {
                goto join_222;
              } else {
                _M0L12dispatch__15S205 = 2;
                goto loop__label__15_208;
              }
            } else if (_M0L10next__charS223 > 58) {
              goto join_222;
            } else {
              _M0L12dispatch__15S205 = 1;
              goto loop__label__15_208;
            }
            join_222:;
            _M0L12dispatch__15S205 = 0;
            goto loop__label__15_208;
          } else {
            goto join_198;
          }
          break;
        }
        
        case 4: {
          int32_t _M0L6_2atmpS1559;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__3S197 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1559 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1559 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1567 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS225;
            int32_t _M0L6_2atmpS1560;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS225
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1567);
            _M0L6_2atmpS1560 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1560 + 1;
            if (_M0L10next__charS225 < 58) {
              if (_M0L10next__charS225 < 48) {
                goto join_224;
              } else {
                _M0L12dispatch__15S205 = 4;
                goto loop__label__15_208;
              }
            } else if (_M0L10next__charS225 > 58) {
              goto join_224;
            } else {
              int32_t _M0L6_2atmpS1561;
              _M0Lm9tag__0__2S192 = _M0Lm9tag__0__1S191;
              _M0Lm9tag__0__1S191 = _M0Lm6tag__0S190;
              _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
              _M0L6_2atmpS1561 = _M0Lm9_2acursorS182;
              if (_M0L6_2atmpS1561 < _M0L6_2aendS181) {
                int32_t _M0L6_2atmpS1566 = _M0Lm9_2acursorS182;
                int32_t _M0L10next__charS227;
                int32_t _M0L6_2atmpS1562;
                moonbit_incref(_M0L7_2adataS179);
                #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                _M0L10next__charS227
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1566);
                _M0L6_2atmpS1562 = _M0Lm9_2acursorS182;
                _M0Lm9_2acursorS182 = _M0L6_2atmpS1562 + 1;
                if (_M0L10next__charS227 < 58) {
                  if (_M0L10next__charS227 < 48) {
                    goto join_226;
                  } else {
                    int32_t _M0L6_2atmpS1563;
                    _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
                    _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__4S196 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1563 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1563 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1565 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS229;
                      int32_t _M0L6_2atmpS1564;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS229
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1565);
                      _M0L6_2atmpS1564 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1564 + 1;
                      if (_M0L10next__charS229 < 59) {
                        if (_M0L10next__charS229 < 48) {
                          goto join_228;
                        } else if (_M0L10next__charS229 > 57) {
                          _M0L12dispatch__15S205 = 3;
                          goto loop__label__15_208;
                        } else {
                          _M0L12dispatch__15S205 = 5;
                          goto loop__label__15_208;
                        }
                      } else if (_M0L10next__charS229 > 63) {
                        if (_M0L10next__charS229 < 65) {
                          goto join_211;
                        } else {
                          goto join_228;
                        }
                      } else {
                        goto join_228;
                      }
                      join_228:;
                      _M0L12dispatch__15S205 = 0;
                      goto loop__label__15_208;
                    } else {
                      goto join_198;
                    }
                  }
                } else if (_M0L10next__charS227 > 58) {
                  goto join_226;
                } else {
                  _M0L12dispatch__15S205 = 1;
                  goto loop__label__15_208;
                }
                join_226:;
                _M0L12dispatch__15S205 = 0;
                goto loop__label__15_208;
              } else {
                goto join_198;
              }
            }
            join_224:;
            _M0L12dispatch__15S205 = 0;
            goto loop__label__15_208;
          } else {
            goto join_198;
          }
          break;
        }
        
        case 2: {
          int32_t _M0L6_2atmpS1568;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1568 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1568 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1570 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS231;
            int32_t _M0L6_2atmpS1569;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS231
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1570);
            _M0L6_2atmpS1569 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1569 + 1;
            if (_M0L10next__charS231 < 58) {
              if (_M0L10next__charS231 < 48) {
                goto join_230;
              } else {
                _M0L12dispatch__15S205 = 2;
                goto loop__label__15_208;
              }
            } else if (_M0L10next__charS231 > 58) {
              goto join_230;
            } else {
              _M0L12dispatch__15S205 = 3;
              goto loop__label__15_208;
            }
            join_230:;
            _M0L12dispatch__15S205 = 0;
            goto loop__label__15_208;
          } else {
            goto join_198;
          }
          break;
        }
        
        case 0: {
          int32_t _M0L6_2atmpS1571;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1571 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1571 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1573 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS232;
            int32_t _M0L6_2atmpS1572;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS232
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1573);
            _M0L6_2atmpS1572 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1572 + 1;
            if (_M0L10next__charS232 == 58) {
              _M0L12dispatch__15S205 = 1;
              goto loop__label__15_208;
            } else {
              _M0L12dispatch__15S205 = 0;
              goto loop__label__15_208;
            }
          } else {
            goto join_198;
          }
          break;
        }
        default: {
          goto join_198;
          break;
        }
      }
      join_211:;
      _M0Lm9tag__0__1S191 = _M0Lm9tag__0__2S192;
      _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
      _M0Lm6tag__1S194 = _M0Lm9tag__1__1S195;
      _M0L6_2atmpS1538 = _M0Lm9_2acursorS182;
      if (_M0L6_2atmpS1538 < _M0L6_2aendS181) {
        int32_t _M0L6_2atmpS1540 = _M0Lm9_2acursorS182;
        int32_t _M0L10next__charS212;
        int32_t _M0L6_2atmpS1539;
        moonbit_incref(_M0L7_2adataS179);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS212
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1540);
        _M0L6_2atmpS1539 = _M0Lm9_2acursorS182;
        _M0Lm9_2acursorS182 = _M0L6_2atmpS1539 + 1;
        if (_M0L10next__charS212 == 58) {
          _M0L12dispatch__15S205 = 1;
          continue;
        } else {
          _M0L12dispatch__15S205 = 6;
          continue;
        }
      } else {
        goto join_210;
      }
      join_210:;
      _M0Lm6tag__0S190 = _M0Lm9tag__0__1S191;
      _M0Lm20match__tag__saver__0S185 = _M0Lm6tag__0S190;
      _M0Lm20match__tag__saver__1S186 = _M0Lm6tag__1S194;
      _M0Lm20match__tag__saver__2S187 = _M0Lm6tag__2S193;
      _M0Lm20match__tag__saver__3S188 = _M0Lm6tag__3S197;
      _M0Lm20match__tag__saver__4S189 = _M0Lm6tag__4S196;
      _M0Lm13accept__stateS183 = 0;
      _M0Lm10match__endS184 = _M0Lm9_2acursorS182;
      goto join_198;
      join_206:;
      _M0Lm9tag__0__1S191 = _M0Lm9tag__0__2S192;
      _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
      _M0Lm6tag__1S194 = _M0Lm9tag__1__1S195;
      _M0L6_2atmpS1535 = _M0Lm9_2acursorS182;
      if (_M0L6_2atmpS1535 < _M0L6_2aendS181) {
        int32_t _M0L6_2atmpS1537 = _M0Lm9_2acursorS182;
        int32_t _M0L10next__charS209;
        int32_t _M0L6_2atmpS1536;
        moonbit_incref(_M0L7_2adataS179);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS209
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1537);
        _M0L6_2atmpS1536 = _M0Lm9_2acursorS182;
        _M0Lm9_2acursorS182 = _M0L6_2atmpS1536 + 1;
        if (_M0L10next__charS209 < 58) {
          if (_M0L10next__charS209 < 48) {
            goto join_207;
          } else {
            _M0L12dispatch__15S205 = 4;
            continue;
          }
        } else if (_M0L10next__charS209 > 58) {
          goto join_207;
        } else {
          _M0L12dispatch__15S205 = 1;
          continue;
        }
        join_207:;
        _M0L12dispatch__15S205 = 0;
        continue;
      } else {
        goto join_198;
      }
      break;
    }
  } else {
    goto join_198;
  }
  join_198:;
  switch (_M0Lm13accept__stateS183) {
    case 0: {
      int32_t _M0L6_2atmpS1532 = _M0Lm20match__tag__saver__0S185;
      int32_t _M0L6_2atmpS1531 = _M0L6_2atmpS1532 + 1;
      int64_t _M0L6_2atmpS1528 = (int64_t)_M0L6_2atmpS1531;
      int32_t _M0L6_2atmpS1530 = _M0Lm20match__tag__saver__1S186;
      int64_t _M0L6_2atmpS1529 = (int64_t)_M0L6_2atmpS1530;
      struct _M0TPC16string10StringView _M0L11start__lineS199;
      int32_t _M0L6_2atmpS1527;
      int32_t _M0L6_2atmpS1526;
      int64_t _M0L6_2atmpS1523;
      int32_t _M0L6_2atmpS1525;
      int64_t _M0L6_2atmpS1524;
      struct _M0TPC16string10StringView _M0L13start__columnS200;
      int64_t _M0L6_2atmpS1520;
      int32_t _M0L6_2atmpS1522;
      int64_t _M0L6_2atmpS1521;
      struct _M0TPC16string10StringView _M0L8filenameS201;
      int32_t _M0L6_2atmpS1519;
      int32_t _M0L6_2atmpS1518;
      int64_t _M0L6_2atmpS1515;
      int32_t _M0L6_2atmpS1517;
      int64_t _M0L6_2atmpS1516;
      struct _M0TPC16string10StringView _M0L9end__lineS202;
      int32_t _M0L6_2atmpS1514;
      int32_t _M0L6_2atmpS1513;
      int64_t _M0L6_2atmpS1510;
      int32_t _M0L6_2atmpS1512;
      int64_t _M0L6_2atmpS1511;
      struct _M0TPC16string10StringView _M0L11end__columnS203;
      int32_t _M0L6_2atmpS1509;
      int32_t _M0L6_2atmpS1508;
      int64_t _M0L6_2atmpS1505;
      int32_t _M0L6_2atmpS1507;
      int64_t _M0L6_2atmpS1506;
      struct _M0TPC16string10StringView _M0L6_2atmpS2862;
      struct _M0TPB13SourceLocRepr* _block_3357;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS199
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1528, _M0L6_2atmpS1529);
      _M0L6_2atmpS1527 = _M0Lm20match__tag__saver__1S186;
      _M0L6_2atmpS1526 = _M0L6_2atmpS1527 + 1;
      _M0L6_2atmpS1523 = (int64_t)_M0L6_2atmpS1526;
      _M0L6_2atmpS1525 = _M0Lm20match__tag__saver__2S187;
      _M0L6_2atmpS1524 = (int64_t)_M0L6_2atmpS1525;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS200
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1523, _M0L6_2atmpS1524);
      _M0L6_2atmpS1520 = (int64_t)_M0L8_2astartS180;
      _M0L6_2atmpS1522 = _M0Lm20match__tag__saver__0S185;
      _M0L6_2atmpS1521 = (int64_t)_M0L6_2atmpS1522;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS201
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1520, _M0L6_2atmpS1521);
      _M0L6_2atmpS1519 = _M0Lm20match__tag__saver__2S187;
      _M0L6_2atmpS1518 = _M0L6_2atmpS1519 + 1;
      _M0L6_2atmpS1515 = (int64_t)_M0L6_2atmpS1518;
      _M0L6_2atmpS1517 = _M0Lm20match__tag__saver__3S188;
      _M0L6_2atmpS1516 = (int64_t)_M0L6_2atmpS1517;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS202
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1515, _M0L6_2atmpS1516);
      _M0L6_2atmpS1514 = _M0Lm20match__tag__saver__3S188;
      _M0L6_2atmpS1513 = _M0L6_2atmpS1514 + 1;
      _M0L6_2atmpS1510 = (int64_t)_M0L6_2atmpS1513;
      _M0L6_2atmpS1512 = _M0Lm20match__tag__saver__4S189;
      _M0L6_2atmpS1511 = (int64_t)_M0L6_2atmpS1512;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS203
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1510, _M0L6_2atmpS1511);
      _M0L6_2atmpS1509 = _M0Lm20match__tag__saver__4S189;
      _M0L6_2atmpS1508 = _M0L6_2atmpS1509 + 1;
      _M0L6_2atmpS1505 = (int64_t)_M0L6_2atmpS1508;
      _M0L6_2atmpS1507 = _M0Lm10match__endS184;
      _M0L6_2atmpS1506 = (int64_t)_M0L6_2atmpS1507;
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6_2atmpS2862
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1505, _M0L6_2atmpS1506);
      moonbit_decref(_M0L6_2atmpS2862.$0);
      _block_3357
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3357)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 5, 0);
      _block_3357->$0_0 = _M0L8filenameS201.$0;
      _block_3357->$0_1 = _M0L8filenameS201.$1;
      _block_3357->$0_2 = _M0L8filenameS201.$2;
      _block_3357->$1_0 = _M0L11start__lineS199.$0;
      _block_3357->$1_1 = _M0L11start__lineS199.$1;
      _block_3357->$1_2 = _M0L11start__lineS199.$2;
      _block_3357->$2_0 = _M0L13start__columnS200.$0;
      _block_3357->$2_1 = _M0L13start__columnS200.$1;
      _block_3357->$2_2 = _M0L13start__columnS200.$2;
      _block_3357->$3_0 = _M0L9end__lineS202.$0;
      _block_3357->$3_1 = _M0L9end__lineS202.$1;
      _block_3357->$3_2 = _M0L9end__lineS202.$2;
      _block_3357->$4_0 = _M0L11end__columnS203.$0;
      _block_3357->$4_1 = _M0L11end__columnS203.$1;
      _block_3357->$4_2 = _M0L11end__columnS203.$2;
      return _block_3357;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS179);
      #line 77 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC16string6String14escape_2einner(
  moonbit_string_t _M0L4selfS175,
  int32_t _M0L5quoteS176
) {
  struct _M0TPB13StringBuilder* _M0L3bufS174;
  int32_t _M0L6_2atmpS1504;
  struct _M0TPC16string10StringView _M0L6_2atmpS1502;
  struct _M0TPB6Logger _M0L6_2atmpS1503;
  #line 145 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 146 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS174 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS1504 = Moonbit_array_length(_M0L4selfS175);
  _M0L6_2atmpS1502
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1504, _M0L4selfS175
  };
  moonbit_incref(_M0L3bufS174);
  _M0L6_2atmpS1503
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS174
  };
  #line 147 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1502, _M0L6_2atmpS1503, _M0L5quoteS176);
  #line 148 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS174);
}

int32_t _M0MPC16string10StringView18escape__to_2einner(
  struct _M0TPC16string10StringView _M0L4selfS166,
  struct _M0TPB6Logger _M0L6loggerS164,
  int32_t _M0L5quoteS163
) {
  int32_t _M0L3lenS165;
  struct _M0TURPB6LoggerRPC16string10StringViewE* _M0L6_2aenvS167;
  int32_t _M0L1iS168;
  int32_t _M0L3segS169;
  #line 179 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L5quoteS163) {
    if (_M0L6loggerS164.$1) {
      moonbit_incref(_M0L6loggerS164.$1);
    }
    #line 185 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, 34);
  }
  moonbit_incref(_M0L4selfS166.$0);
  #line 187 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3lenS165 = _M0MPC16string10StringView6length(_M0L4selfS166);
  if (_M0L6loggerS164.$1) {
    moonbit_incref(_M0L6loggerS164.$1);
  }
  moonbit_incref(_M0L4selfS166.$0);
  _M0L6_2aenvS167
  = (struct _M0TURPB6LoggerRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TURPB6LoggerRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2aenvS167)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPB6LoggerRPC16string10StringViewE, $0_0) >> 2, 3, 0);
  _M0L6_2aenvS167->$0_0 = _M0L6loggerS164.$0;
  _M0L6_2aenvS167->$0_1 = _M0L6loggerS164.$1;
  _M0L6_2aenvS167->$1_0 = _M0L4selfS166.$0;
  _M0L6_2aenvS167->$1_1 = _M0L4selfS166.$1;
  _M0L6_2aenvS167->$1_2 = _M0L4selfS166.$2;
  _M0L1iS168 = 0;
  _M0L3segS169 = 0;
  _2afor_170:;
  while (1) {
    int32_t _M0L4codeS171;
    int32_t _M0L1cS173;
    int32_t _M0L6_2atmpS1486;
    int32_t _M0L6_2atmpS1487;
    int32_t _M0L6_2atmpS1488;
    int32_t _tmp_3361;
    int32_t _tmp_3362;
    if (_M0L1iS168 >= _M0L3lenS165) {
      moonbit_decref(_M0L4selfS166.$0);
      #line 195 "/Users/user/.moon/lib/core/builtin/show.mbt"
      _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
      break;
    }
    moonbit_incref(_M0L4selfS166.$0);
    #line 198 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L4codeS171
    = _M0MPC16string10StringView11unsafe__get(_M0L4selfS166, _M0L1iS168);
    switch (_M0L4codeS171) {
      case 34: {
        _M0L1cS173 = _M0L4codeS171;
        goto join_172;
        break;
      }
      
      case 92: {
        _M0L1cS173 = _M0L4codeS171;
        goto join_172;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1489;
        int32_t _M0L6_2atmpS1490;
        moonbit_incref(_M0L6_2aenvS167);
        #line 207 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 208 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_116.data);
        _M0L6_2atmpS1489 = _M0L1iS168 + 1;
        _M0L6_2atmpS1490 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1489;
        _M0L3segS169 = _M0L6_2atmpS1490;
        goto _2afor_170;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1491;
        int32_t _M0L6_2atmpS1492;
        moonbit_incref(_M0L6_2aenvS167);
        #line 212 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 213 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_117.data);
        _M0L6_2atmpS1491 = _M0L1iS168 + 1;
        _M0L6_2atmpS1492 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1491;
        _M0L3segS169 = _M0L6_2atmpS1492;
        goto _2afor_170;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1493;
        int32_t _M0L6_2atmpS1494;
        moonbit_incref(_M0L6_2aenvS167);
        #line 217 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 218 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_118.data);
        _M0L6_2atmpS1493 = _M0L1iS168 + 1;
        _M0L6_2atmpS1494 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1493;
        _M0L3segS169 = _M0L6_2atmpS1494;
        goto _2afor_170;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1495;
        int32_t _M0L6_2atmpS1496;
        moonbit_incref(_M0L6_2aenvS167);
        #line 222 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 223 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_119.data);
        _M0L6_2atmpS1495 = _M0L1iS168 + 1;
        _M0L6_2atmpS1496 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1495;
        _M0L3segS169 = _M0L6_2atmpS1496;
        goto _2afor_170;
        break;
      }
      default: {
        if (_M0L4codeS171 < 32) {
          int32_t _M0L6_2atmpS1498;
          moonbit_string_t _M0L6_2atmpS1497;
          int32_t _M0L6_2atmpS1499;
          int32_t _M0L6_2atmpS1500;
          moonbit_incref(_M0L6_2aenvS167);
          #line 228 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
          if (_M0L6loggerS164.$1) {
            moonbit_incref(_M0L6loggerS164.$1);
          }
          #line 229 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_120.data);
          _M0L6_2atmpS1498 = _M0L4codeS171 & 0xff;
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1497 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1498);
          if (_M0L6loggerS164.$1) {
            moonbit_incref(_M0L6loggerS164.$1);
          }
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, _M0L6_2atmpS1497);
          if (_M0L6loggerS164.$1) {
            moonbit_incref(_M0L6loggerS164.$1);
          }
          #line 231 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, 125);
          _M0L6_2atmpS1499 = _M0L1iS168 + 1;
          _M0L6_2atmpS1500 = _M0L1iS168 + 1;
          _M0L1iS168 = _M0L6_2atmpS1499;
          _M0L3segS169 = _M0L6_2atmpS1500;
          goto _2afor_170;
        } else {
          int32_t _M0L6_2atmpS1501 = _M0L1iS168 + 1;
          int32_t _tmp_3360 = _M0L3segS169;
          _M0L1iS168 = _M0L6_2atmpS1501;
          _M0L3segS169 = _tmp_3360;
          goto _2afor_170;
        }
        break;
      }
    }
    goto joinlet_3359;
    join_172:;
    moonbit_incref(_M0L6_2aenvS167);
    #line 201 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
    if (_M0L6loggerS164.$1) {
      moonbit_incref(_M0L6loggerS164.$1);
    }
    #line 202 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, 92);
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1486 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS173);
    if (_M0L6loggerS164.$1) {
      moonbit_incref(_M0L6loggerS164.$1);
    }
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, _M0L6_2atmpS1486);
    _M0L6_2atmpS1487 = _M0L1iS168 + 1;
    _M0L6_2atmpS1488 = _M0L1iS168 + 1;
    _M0L1iS168 = _M0L6_2atmpS1487;
    _M0L3segS169 = _M0L6_2atmpS1488;
    continue;
    joinlet_3359:;
    _tmp_3361 = _M0L1iS168;
    _tmp_3362 = _M0L3segS169;
    _M0L1iS168 = _tmp_3361;
    _M0L3segS169 = _tmp_3362;
    continue;
    break;
  }
  if (_M0L5quoteS163) {
    #line 239 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, 34);
  } else if (_M0L6loggerS164.$1) {
    moonbit_decref(_M0L6loggerS164.$1);
  }
  return 0;
}

int32_t _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(
  struct _M0TURPB6LoggerRPC16string10StringViewE* _M0L6_2aenvS159,
  int32_t _M0L3segS162,
  int32_t _M0L1iS161
) {
  struct _M0TPC16string10StringView _M0L4selfS158;
  struct _M0TPB6Logger _M0L8_2afieldS2863;
  int32_t _M0L6_2acntS3100;
  struct _M0TPB6Logger _M0L6loggerS160;
  #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L4selfS158
  = (struct _M0TPC16string10StringView){
    _M0L6_2aenvS159->$1_1, _M0L6_2aenvS159->$1_2, _M0L6_2aenvS159->$1_0
  };
  _M0L8_2afieldS2863
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS159->$0_0, _M0L6_2aenvS159->$0_1
  };
  _M0L6_2acntS3100 = Moonbit_object_header(_M0L6_2aenvS159)->rc;
  if (_M0L6_2acntS3100 > 1) {
    int32_t _M0L11_2anew__cntS3101 = _M0L6_2acntS3100 - 1;
    Moonbit_object_header(_M0L6_2aenvS159)->rc = _M0L11_2anew__cntS3101;
    moonbit_incref(_M0L4selfS158.$0);
    if (_M0L8_2afieldS2863.$1) {
      moonbit_incref(_M0L8_2afieldS2863.$1);
    }
  } else if (_M0L6_2acntS3100 == 1) {
    #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS159);
  }
  _M0L6loggerS160 = _M0L8_2afieldS2863;
  if (_M0L1iS161 > _M0L3segS162) {
    int64_t _M0L6_2atmpS1485 = (int64_t)_M0L1iS161;
    struct _M0TPC16string10StringView _M0L6_2atmpS1484;
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1484
    = _M0MPC16string10StringView11sub_2einner(_M0L4selfS158, _M0L3segS162, _M0L6_2atmpS1485);
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS160.$0->$method_2(_M0L6loggerS160.$1, _M0L6_2atmpS1484);
  } else {
    if (_M0L6loggerS160.$1) {
      moonbit_decref(_M0L6loggerS160.$1);
    }
    moonbit_decref(_M0L4selfS158.$0);
  }
  return 0;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS156,
  int32_t _M0L5indexS157
) {
  moonbit_string_t _M0L3strS1481;
  int32_t _M0L5startS1483;
  int32_t _M0L6_2atmpS1482;
  int32_t _result_3363;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1481 = _M0L4selfS156.$0;
  _M0L5startS1483 = _M0L4selfS156.$1;
  _M0L6_2atmpS1482 = _M0L5startS1483 + _M0L5indexS157;
  _result_3363 = _M0L3strS1481[_M0L6_2atmpS1482];
  moonbit_decref(_M0L3strS1481);
  return _result_3363;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView _M0L4selfS149,
  int32_t _M0L5startS155,
  int64_t _M0L3endS151
) {
  moonbit_string_t _M0L3strS1480;
  int32_t _M0L8str__lenS148;
  int32_t _M0L8abs__endS150;
  int32_t _M0L10abs__startS154;
  int32_t _M0L5startS1468;
  int32_t _if__result_3364;
  #line 712 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1480 = _M0L4selfS149.$0;
  _M0L8str__lenS148 = Moonbit_array_length(_M0L3strS1480);
  if (_M0L3endS151 == 4294967296ll) {
    _M0L8abs__endS150 = _M0L4selfS149.$2;
  } else {
    int64_t _M0L7_2aSomeS152 = _M0L3endS151;
    int32_t _M0L6_2aendS153 = (int32_t)_M0L7_2aSomeS152;
    if (_M0L6_2aendS153 < 0) {
      int32_t _M0L3endS1478 = _M0L4selfS149.$2;
      _M0L8abs__endS150 = _M0L3endS1478 + _M0L6_2aendS153;
    } else {
      int32_t _M0L5startS1479 = _M0L4selfS149.$1;
      _M0L8abs__endS150 = _M0L5startS1479 + _M0L6_2aendS153;
    }
  }
  if (_M0L5startS155 < 0) {
    int32_t _M0L3endS1476 = _M0L4selfS149.$2;
    _M0L10abs__startS154 = _M0L3endS1476 + _M0L5startS155;
  } else {
    int32_t _M0L5startS1477 = _M0L4selfS149.$1;
    _M0L10abs__startS154 = _M0L5startS1477 + _M0L5startS155;
  }
  _M0L5startS1468 = _M0L4selfS149.$1;
  if (_M0L10abs__startS154 >= _M0L5startS1468) {
    if (_M0L10abs__startS154 <= _M0L8abs__endS150) {
      int32_t _M0L3endS1467 = _M0L4selfS149.$2;
      _if__result_3364 = _M0L8abs__endS150 <= _M0L3endS1467;
    } else {
      _if__result_3364 = 0;
    }
  } else {
    _if__result_3364 = 0;
  }
  if (_if__result_3364) {
    moonbit_string_t _M0L3strS1475;
    if (_M0L10abs__startS154 < _M0L8str__lenS148) {
      moonbit_string_t _M0L3strS1471 = _M0L4selfS149.$0;
      int32_t _M0L6_2atmpS1470 = _M0L3strS1471[_M0L10abs__startS154];
      int32_t _M0L6_2atmpS1469;
      #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1469
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1470);
      if (!_M0L6_2atmpS1469) {
        
      } else {
        #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L8abs__endS150 < _M0L8str__lenS148) {
      moonbit_string_t _M0L3strS1474 = _M0L4selfS149.$0;
      int32_t _M0L6_2atmpS1473 = _M0L3strS1474[_M0L8abs__endS150];
      int32_t _M0L6_2atmpS1472;
      #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1472
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1473);
      if (!_M0L6_2atmpS1472) {
        
      } else {
        #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    _M0L3strS1475 = _M0L4selfS149.$0;
    return (struct _M0TPC16string10StringView){_M0L10abs__startS154,
                                                 _M0L8abs__endS150,
                                                 _M0L3strS1475};
  } else {
    moonbit_decref(_M0L4selfS149.$0);
    #line 732 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS147
) {
  int32_t _M0L3endS1465;
  int32_t _M0L5startS1466;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1465 = _M0L4selfS147.$2;
  _M0L5startS1466 = _M0L4selfS147.$1;
  moonbit_decref(_M0L4selfS147.$0);
  return _M0L3endS1465 - _M0L5startS1466;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS146) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS145;
  int32_t _M0L6_2atmpS1462;
  int32_t _M0L6_2atmpS1461;
  int32_t _M0L6_2atmpS1464;
  int32_t _M0L6_2atmpS1463;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1460;
  #line 109 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS145 = _M0MPB13StringBuilder11new_2einner(0);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1462 = _M0IPC14byte4BytePB3Div3div(_M0L1bS146, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1461
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1462);
  moonbit_incref(_M0L7_2aselfS145);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS145, _M0L6_2atmpS1461);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1464 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS146, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1463
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1464);
  moonbit_incref(_M0L7_2aselfS145);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS145, _M0L6_2atmpS1463);
  _M0L6_2atmpS1460 = _M0L7_2aselfS145;
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1460);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t _M0L1iS144) {
  #line 110 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS144 < 10) {
    int32_t _M0L6_2atmpS1457;
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1457 = _M0IPC14byte4BytePB3Add3add(_M0L1iS144, 48);
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1457);
  } else {
    int32_t _M0L6_2atmpS1459;
    int32_t _M0L6_2atmpS1458;
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1459 = _M0IPC14byte4BytePB3Add3add(_M0L1iS144, 97);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1458 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1459, 10);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1458);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS142,
  int32_t _M0L4thatS143
) {
  int32_t _M0L6_2atmpS1455;
  int32_t _M0L6_2atmpS1456;
  int32_t _M0L6_2atmpS1454;
  #line 120 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1455 = (int32_t)_M0L4selfS142;
  _M0L6_2atmpS1456 = (int32_t)_M0L4thatS143;
  _M0L6_2atmpS1454 = _M0L6_2atmpS1455 - _M0L6_2atmpS1456;
  return _M0L6_2atmpS1454 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS140,
  int32_t _M0L4thatS141
) {
  int32_t _M0L6_2atmpS1452;
  int32_t _M0L6_2atmpS1453;
  int32_t _M0L6_2atmpS1451;
  #line 67 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1452 = (int32_t)_M0L4selfS140;
  _M0L6_2atmpS1453 = (int32_t)_M0L4thatS141;
  _M0L6_2atmpS1451 = _M0L6_2atmpS1452 % _M0L6_2atmpS1453;
  return _M0L6_2atmpS1451 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS138,
  int32_t _M0L4thatS139
) {
  int32_t _M0L6_2atmpS1449;
  int32_t _M0L6_2atmpS1450;
  int32_t _M0L6_2atmpS1448;
  #line 62 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1449 = (int32_t)_M0L4selfS138;
  _M0L6_2atmpS1450 = (int32_t)_M0L4thatS139;
  _M0L6_2atmpS1448 = _M0L6_2atmpS1449 / _M0L6_2atmpS1450;
  return _M0L6_2atmpS1448 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS136,
  int32_t _M0L4thatS137
) {
  int32_t _M0L6_2atmpS1446;
  int32_t _M0L6_2atmpS1447;
  int32_t _M0L6_2atmpS1445;
  #line 106 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1446 = (int32_t)_M0L4selfS136;
  _M0L6_2atmpS1447 = (int32_t)_M0L4thatS137;
  _M0L6_2atmpS1445 = _M0L6_2atmpS1446 + _M0L6_2atmpS1447;
  return _M0L6_2atmpS1445 & 0xff;
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS130
) {
  int32_t _M0L17codepoint__lengthS129;
  int32_t _M0L6_2atmpS1444;
  moonbit_bytes_t _M0L4dataS131;
  int32_t _M0L1iS132;
  int32_t _M0L12utf16__indexS133;
  #line 102 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS130);
  #line 104 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS129
  = _M0MPC16string6String20char__length_2einner(_M0L1sS130, 0, 4294967296ll);
  _M0L6_2atmpS1444 = _M0L17codepoint__lengthS129 * 4;
  _M0L4dataS131 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1444, 0);
  _M0L1iS132 = 0;
  _M0L12utf16__indexS133 = 0;
  while (1) {
    if (_M0L1iS132 < _M0L17codepoint__lengthS129) {
      int32_t _M0L6_2atmpS1441;
      int32_t _M0L1cS134;
      int32_t _M0L6_2atmpS1442;
      int32_t _M0L6_2atmpS1443;
      moonbit_incref(_M0L1sS130);
      #line 109 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1441
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS130, _M0L12utf16__indexS133);
      _M0L1cS134 = _M0L6_2atmpS1441;
      if (_M0L1cS134 > 65535) {
        int32_t _M0L6_2atmpS1409 = _M0L1iS132 * 4;
        int32_t _M0L6_2atmpS1411 = _M0L1cS134 & 255;
        int32_t _M0L6_2atmpS1410 = _M0L6_2atmpS1411 & 0xff;
        int32_t _M0L6_2atmpS1416;
        int32_t _M0L6_2atmpS1412;
        int32_t _M0L6_2atmpS1415;
        int32_t _M0L6_2atmpS1414;
        int32_t _M0L6_2atmpS1413;
        int32_t _M0L6_2atmpS1421;
        int32_t _M0L6_2atmpS1417;
        int32_t _M0L6_2atmpS1420;
        int32_t _M0L6_2atmpS1419;
        int32_t _M0L6_2atmpS1418;
        int32_t _M0L6_2atmpS1426;
        int32_t _M0L6_2atmpS1422;
        int32_t _M0L6_2atmpS1425;
        int32_t _M0L6_2atmpS1424;
        int32_t _M0L6_2atmpS1423;
        int32_t _M0L6_2atmpS1427;
        int32_t _M0L6_2atmpS1428;
        if (
          _M0L6_2atmpS1409 < 0
          || _M0L6_2atmpS1409 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 111 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1409] = _M0L6_2atmpS1410;
        _M0L6_2atmpS1416 = _M0L1iS132 * 4;
        _M0L6_2atmpS1412 = _M0L6_2atmpS1416 + 1;
        _M0L6_2atmpS1415 = _M0L1cS134 >> 8;
        _M0L6_2atmpS1414 = _M0L6_2atmpS1415 & 255;
        _M0L6_2atmpS1413 = _M0L6_2atmpS1414 & 0xff;
        if (
          _M0L6_2atmpS1412 < 0
          || _M0L6_2atmpS1412 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 112 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1412] = _M0L6_2atmpS1413;
        _M0L6_2atmpS1421 = _M0L1iS132 * 4;
        _M0L6_2atmpS1417 = _M0L6_2atmpS1421 + 2;
        _M0L6_2atmpS1420 = _M0L1cS134 >> 16;
        _M0L6_2atmpS1419 = _M0L6_2atmpS1420 & 255;
        _M0L6_2atmpS1418 = _M0L6_2atmpS1419 & 0xff;
        if (
          _M0L6_2atmpS1417 < 0
          || _M0L6_2atmpS1417 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 113 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1417] = _M0L6_2atmpS1418;
        _M0L6_2atmpS1426 = _M0L1iS132 * 4;
        _M0L6_2atmpS1422 = _M0L6_2atmpS1426 + 3;
        _M0L6_2atmpS1425 = _M0L1cS134 >> 24;
        _M0L6_2atmpS1424 = _M0L6_2atmpS1425 & 255;
        _M0L6_2atmpS1423 = _M0L6_2atmpS1424 & 0xff;
        if (
          _M0L6_2atmpS1422 < 0
          || _M0L6_2atmpS1422 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 114 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1422] = _M0L6_2atmpS1423;
        _M0L6_2atmpS1427 = _M0L1iS132 + 1;
        _M0L6_2atmpS1428 = _M0L12utf16__indexS133 + 2;
        _M0L1iS132 = _M0L6_2atmpS1427;
        _M0L12utf16__indexS133 = _M0L6_2atmpS1428;
        continue;
      } else {
        int32_t _M0L6_2atmpS1429 = _M0L1iS132 * 4;
        int32_t _M0L6_2atmpS1431 = _M0L1cS134 & 255;
        int32_t _M0L6_2atmpS1430 = _M0L6_2atmpS1431 & 0xff;
        int32_t _M0L6_2atmpS1436;
        int32_t _M0L6_2atmpS1432;
        int32_t _M0L6_2atmpS1435;
        int32_t _M0L6_2atmpS1434;
        int32_t _M0L6_2atmpS1433;
        int32_t _M0L6_2atmpS1438;
        int32_t _M0L6_2atmpS1437;
        int32_t _M0L6_2atmpS1440;
        int32_t _M0L6_2atmpS1439;
        if (
          _M0L6_2atmpS1429 < 0
          || _M0L6_2atmpS1429 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 117 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1429] = _M0L6_2atmpS1430;
        _M0L6_2atmpS1436 = _M0L1iS132 * 4;
        _M0L6_2atmpS1432 = _M0L6_2atmpS1436 + 1;
        _M0L6_2atmpS1435 = _M0L1cS134 >> 8;
        _M0L6_2atmpS1434 = _M0L6_2atmpS1435 & 255;
        _M0L6_2atmpS1433 = _M0L6_2atmpS1434 & 0xff;
        if (
          _M0L6_2atmpS1432 < 0
          || _M0L6_2atmpS1432 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 118 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1432] = _M0L6_2atmpS1433;
        _M0L6_2atmpS1438 = _M0L1iS132 * 4;
        _M0L6_2atmpS1437 = _M0L6_2atmpS1438 + 2;
        if (
          _M0L6_2atmpS1437 < 0
          || _M0L6_2atmpS1437 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 119 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1437] = 0;
        _M0L6_2atmpS1440 = _M0L1iS132 * 4;
        _M0L6_2atmpS1439 = _M0L6_2atmpS1440 + 3;
        if (
          _M0L6_2atmpS1439 < 0
          || _M0L6_2atmpS1439 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 120 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1439] = 0;
      }
      _M0L6_2atmpS1442 = _M0L1iS132 + 1;
      _M0L6_2atmpS1443 = _M0L12utf16__indexS133 + 1;
      _M0L1iS132 = _M0L6_2atmpS1442;
      _M0L12utf16__indexS133 = _M0L6_2atmpS1443;
      continue;
    } else {
      moonbit_decref(_M0L1sS130);
    }
    break;
  }
  #line 123 "/Users/user/.moon/lib/core/builtin/console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS131);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS126,
  int32_t _M0L5indexS127
) {
  int32_t _M0L2c1S125;
  #line 91 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
  _M0L2c1S125 = _M0L4selfS126[_M0L5indexS127];
  #line 94 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S125)) {
    int32_t _M0L6_2atmpS1408 = _M0L5indexS127 + 1;
    int32_t _M0L2c2S128 = _M0L4selfS126[_M0L6_2atmpS1408];
    int32_t _M0L6_2atmpS1406;
    int32_t _M0L6_2atmpS1407;
    moonbit_decref(_M0L4selfS126);
    _M0L6_2atmpS1406 = (int32_t)_M0L2c1S125;
    _M0L6_2atmpS1407 = (int32_t)_M0L2c2S128;
    #line 96 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1406, _M0L6_2atmpS1407);
  } else {
    moonbit_decref(_M0L4selfS126);
    #line 98 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S125);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS124) {
  int32_t _M0L6_2atmpS1405;
  #line 68 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS1405 = (int32_t)_M0L4selfS124;
  return _M0L6_2atmpS1405;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS122,
  int32_t _M0L8trailingS123
) {
  int32_t _M0L6_2atmpS1404;
  int32_t _M0L6_2atmpS1403;
  int32_t _M0L6_2atmpS1402;
  int32_t _M0L6_2atmpS1401;
  int32_t _M0L6_2atmpS1400;
  #line 40 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1404 = _M0L7leadingS122 - 55296;
  _M0L6_2atmpS1403 = _M0L6_2atmpS1404 * 1024;
  _M0L6_2atmpS1402 = _M0L6_2atmpS1403 + _M0L8trailingS123;
  _M0L6_2atmpS1401 = _M0L6_2atmpS1402 - 56320;
  _M0L6_2atmpS1400 = _M0L6_2atmpS1401 + 65536;
  return _M0L6_2atmpS1400;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS115,
  int32_t _M0L13start__offsetS116,
  int64_t _M0L11end__offsetS113
) {
  int32_t _M0L11end__offsetS112;
  int32_t _if__result_3366;
  #line 60 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS113 == 4294967296ll) {
    _M0L11end__offsetS112 = Moonbit_array_length(_M0L4selfS115);
  } else {
    int64_t _M0L7_2aSomeS114 = _M0L11end__offsetS113;
    _M0L11end__offsetS112 = (int32_t)_M0L7_2aSomeS114;
  }
  if (_M0L13start__offsetS116 >= 0) {
    if (_M0L13start__offsetS116 <= _M0L11end__offsetS112) {
      int32_t _M0L6_2atmpS1393 = Moonbit_array_length(_M0L4selfS115);
      _if__result_3366 = _M0L11end__offsetS112 <= _M0L6_2atmpS1393;
    } else {
      _if__result_3366 = 0;
    }
  } else {
    _if__result_3366 = 0;
  }
  if (_if__result_3366) {
    int32_t _M0L12utf16__indexS117 = _M0L13start__offsetS116;
    int32_t _M0L11char__countS118 = 0;
    while (1) {
      if (_M0L12utf16__indexS117 < _M0L11end__offsetS112) {
        int32_t _M0L2c1S119 = _M0L4selfS115[_M0L12utf16__indexS117];
        int32_t _if__result_3368;
        int32_t _M0L6_2atmpS1398;
        int32_t _M0L6_2atmpS1399;
        #line 76 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S119)) {
          int32_t _M0L6_2atmpS1394 = _M0L12utf16__indexS117 + 1;
          _if__result_3368 = _M0L6_2atmpS1394 < _M0L11end__offsetS112;
        } else {
          _if__result_3368 = 0;
        }
        if (_if__result_3368) {
          int32_t _M0L6_2atmpS1397 = _M0L12utf16__indexS117 + 1;
          int32_t _M0L2c2S120 = _M0L4selfS115[_M0L6_2atmpS1397];
          #line 78 "/Users/user/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S120)) {
            int32_t _M0L6_2atmpS1395 = _M0L12utf16__indexS117 + 2;
            int32_t _M0L6_2atmpS1396 = _M0L11char__countS118 + 1;
            _M0L12utf16__indexS117 = _M0L6_2atmpS1395;
            _M0L11char__countS118 = _M0L6_2atmpS1396;
            continue;
          } else {
            #line 81 "/Users/user/.moon/lib/core/builtin/string.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_98.data);
          }
        }
        _M0L6_2atmpS1398 = _M0L12utf16__indexS117 + 1;
        _M0L6_2atmpS1399 = _M0L11char__countS118 + 1;
        _M0L12utf16__indexS117 = _M0L6_2atmpS1398;
        _M0L11char__countS118 = _M0L6_2atmpS1399;
        continue;
      } else {
        moonbit_decref(_M0L4selfS115);
        return _M0L11char__countS118;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS115);
    #line 70 "/Users/user/.moon/lib/core/builtin/string.mbt"
    return _M0FPC15abort5abortGiE((moonbit_string_t)moonbit_string_literal_121.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS111) {
  #line 45 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS111 >= 56320) {
    return _M0L4selfS111 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS110) {
  #line 28 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS110 >= 55296) {
    return _M0L4selfS110 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS91) {
  struct _M0TPB13StringBuilder* _M0L3bufS89;
  int32_t _M0L3lenS90;
  int32_t _M0L3remS92;
  int32_t _M0L1iS93;
  #line 61 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 63 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L3bufS89 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS90 = Moonbit_array_length(_M0L4dataS91);
  _M0L3remS92 = _M0L3lenS90 % 3;
  _M0L1iS93 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1345 = _M0L3lenS90 - _M0L3remS92;
    if (_M0L1iS93 < _M0L6_2atmpS1345) {
      int32_t _M0L6_2atmpS1367;
      int32_t _M0L2b0S94;
      int32_t _M0L6_2atmpS1366;
      int32_t _M0L6_2atmpS1365;
      int32_t _M0L2b1S95;
      int32_t _M0L6_2atmpS1364;
      int32_t _M0L6_2atmpS1363;
      int32_t _M0L2b2S96;
      int32_t _M0L6_2atmpS1362;
      int32_t _M0L6_2atmpS1361;
      int32_t _M0L2x0S97;
      int32_t _M0L6_2atmpS1360;
      int32_t _M0L6_2atmpS1357;
      int32_t _M0L6_2atmpS1359;
      int32_t _M0L6_2atmpS1358;
      int32_t _M0L6_2atmpS1356;
      int32_t _M0L2x1S98;
      int32_t _M0L6_2atmpS1355;
      int32_t _M0L6_2atmpS1352;
      int32_t _M0L6_2atmpS1354;
      int32_t _M0L6_2atmpS1353;
      int32_t _M0L6_2atmpS1351;
      int32_t _M0L2x2S99;
      int32_t _M0L6_2atmpS1350;
      int32_t _M0L2x3S100;
      int32_t _M0L6_2atmpS1346;
      int32_t _M0L6_2atmpS1347;
      int32_t _M0L6_2atmpS1348;
      int32_t _M0L6_2atmpS1349;
      int32_t _M0L6_2atmpS1368;
      if (_M0L1iS93 < 0 || _M0L1iS93 >= Moonbit_array_length(_M0L4dataS91)) {
        #line 67 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1367 = (int32_t)_M0L4dataS91[_M0L1iS93];
      _M0L2b0S94 = (int32_t)_M0L6_2atmpS1367;
      _M0L6_2atmpS1366 = _M0L1iS93 + 1;
      if (
        _M0L6_2atmpS1366 < 0
        || _M0L6_2atmpS1366 >= Moonbit_array_length(_M0L4dataS91)
      ) {
        #line 68 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1365 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1366];
      _M0L2b1S95 = (int32_t)_M0L6_2atmpS1365;
      _M0L6_2atmpS1364 = _M0L1iS93 + 2;
      if (
        _M0L6_2atmpS1364 < 0
        || _M0L6_2atmpS1364 >= Moonbit_array_length(_M0L4dataS91)
      ) {
        #line 69 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1363 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1364];
      _M0L2b2S96 = (int32_t)_M0L6_2atmpS1363;
      _M0L6_2atmpS1362 = _M0L2b0S94 & 252;
      _M0L6_2atmpS1361 = _M0L6_2atmpS1362 >> 2;
      if (
        _M0L6_2atmpS1361 < 0
        || _M0L6_2atmpS1361
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 70 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S97 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1361];
      _M0L6_2atmpS1360 = _M0L2b0S94 & 3;
      _M0L6_2atmpS1357 = _M0L6_2atmpS1360 << 4;
      _M0L6_2atmpS1359 = _M0L2b1S95 & 240;
      _M0L6_2atmpS1358 = _M0L6_2atmpS1359 >> 4;
      _M0L6_2atmpS1356 = _M0L6_2atmpS1357 | _M0L6_2atmpS1358;
      if (
        _M0L6_2atmpS1356 < 0
        || _M0L6_2atmpS1356
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 71 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S98 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1356];
      _M0L6_2atmpS1355 = _M0L2b1S95 & 15;
      _M0L6_2atmpS1352 = _M0L6_2atmpS1355 << 2;
      _M0L6_2atmpS1354 = _M0L2b2S96 & 192;
      _M0L6_2atmpS1353 = _M0L6_2atmpS1354 >> 6;
      _M0L6_2atmpS1351 = _M0L6_2atmpS1352 | _M0L6_2atmpS1353;
      if (
        _M0L6_2atmpS1351 < 0
        || _M0L6_2atmpS1351
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 72 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S99 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1351];
      _M0L6_2atmpS1350 = _M0L2b2S96 & 63;
      if (
        _M0L6_2atmpS1350 < 0
        || _M0L6_2atmpS1350
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 73 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S100 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1350];
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1346 = _M0MPC14byte4Byte8to__char(_M0L2x0S97);
      moonbit_incref(_M0L3bufS89);
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1346);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1347 = _M0MPC14byte4Byte8to__char(_M0L2x1S98);
      moonbit_incref(_M0L3bufS89);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1347);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1348 = _M0MPC14byte4Byte8to__char(_M0L2x2S99);
      moonbit_incref(_M0L3bufS89);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1348);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1349 = _M0MPC14byte4Byte8to__char(_M0L2x3S100);
      moonbit_incref(_M0L3bufS89);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1349);
      _M0L6_2atmpS1368 = _M0L1iS93 + 3;
      _M0L1iS93 = _M0L6_2atmpS1368;
      continue;
    }
    break;
  }
  if (_M0L3remS92 == 1) {
    int32_t _M0L6_2atmpS1376 = _M0L3lenS90 - 1;
    int32_t _M0L6_2atmpS1375;
    int32_t _M0L2b0S102;
    int32_t _M0L6_2atmpS1374;
    int32_t _M0L6_2atmpS1373;
    int32_t _M0L2x0S103;
    int32_t _M0L6_2atmpS1372;
    int32_t _M0L6_2atmpS1371;
    int32_t _M0L2x1S104;
    int32_t _M0L6_2atmpS1369;
    int32_t _M0L6_2atmpS1370;
    if (
      _M0L6_2atmpS1376 < 0
      || _M0L6_2atmpS1376 >= Moonbit_array_length(_M0L4dataS91)
    ) {
      #line 80 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1375 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1376];
    moonbit_decref(_M0L4dataS91);
    _M0L2b0S102 = (int32_t)_M0L6_2atmpS1375;
    _M0L6_2atmpS1374 = _M0L2b0S102 & 252;
    _M0L6_2atmpS1373 = _M0L6_2atmpS1374 >> 2;
    if (
      _M0L6_2atmpS1373 < 0
      || _M0L6_2atmpS1373
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 81 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S103 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1373];
    _M0L6_2atmpS1372 = _M0L2b0S102 & 3;
    _M0L6_2atmpS1371 = _M0L6_2atmpS1372 << 4;
    if (
      _M0L6_2atmpS1371 < 0
      || _M0L6_2atmpS1371
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 82 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S104 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1371];
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1369 = _M0MPC14byte4Byte8to__char(_M0L2x0S103);
    moonbit_incref(_M0L3bufS89);
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1369);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1370 = _M0MPC14byte4Byte8to__char(_M0L2x1S104);
    moonbit_incref(_M0L3bufS89);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1370);
    moonbit_incref(_M0L3bufS89);
    #line 85 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, 61);
    moonbit_incref(_M0L3bufS89);
    #line 86 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, 61);
  } else if (_M0L3remS92 == 2) {
    int32_t _M0L6_2atmpS1392 = _M0L3lenS90 - 2;
    int32_t _M0L6_2atmpS1391;
    int32_t _M0L2b0S105;
    int32_t _M0L6_2atmpS1390;
    int32_t _M0L6_2atmpS1389;
    int32_t _M0L2b1S106;
    int32_t _M0L6_2atmpS1388;
    int32_t _M0L6_2atmpS1387;
    int32_t _M0L2x0S107;
    int32_t _M0L6_2atmpS1386;
    int32_t _M0L6_2atmpS1383;
    int32_t _M0L6_2atmpS1385;
    int32_t _M0L6_2atmpS1384;
    int32_t _M0L6_2atmpS1382;
    int32_t _M0L2x1S108;
    int32_t _M0L6_2atmpS1381;
    int32_t _M0L6_2atmpS1380;
    int32_t _M0L2x2S109;
    int32_t _M0L6_2atmpS1377;
    int32_t _M0L6_2atmpS1378;
    int32_t _M0L6_2atmpS1379;
    if (
      _M0L6_2atmpS1392 < 0
      || _M0L6_2atmpS1392 >= Moonbit_array_length(_M0L4dataS91)
    ) {
      #line 88 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1391 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1392];
    _M0L2b0S105 = (int32_t)_M0L6_2atmpS1391;
    _M0L6_2atmpS1390 = _M0L3lenS90 - 1;
    if (
      _M0L6_2atmpS1390 < 0
      || _M0L6_2atmpS1390 >= Moonbit_array_length(_M0L4dataS91)
    ) {
      #line 89 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1389 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1390];
    moonbit_decref(_M0L4dataS91);
    _M0L2b1S106 = (int32_t)_M0L6_2atmpS1389;
    _M0L6_2atmpS1388 = _M0L2b0S105 & 252;
    _M0L6_2atmpS1387 = _M0L6_2atmpS1388 >> 2;
    if (
      _M0L6_2atmpS1387 < 0
      || _M0L6_2atmpS1387
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 90 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S107 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1387];
    _M0L6_2atmpS1386 = _M0L2b0S105 & 3;
    _M0L6_2atmpS1383 = _M0L6_2atmpS1386 << 4;
    _M0L6_2atmpS1385 = _M0L2b1S106 & 240;
    _M0L6_2atmpS1384 = _M0L6_2atmpS1385 >> 4;
    _M0L6_2atmpS1382 = _M0L6_2atmpS1383 | _M0L6_2atmpS1384;
    if (
      _M0L6_2atmpS1382 < 0
      || _M0L6_2atmpS1382
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 91 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S108 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1382];
    _M0L6_2atmpS1381 = _M0L2b1S106 & 15;
    _M0L6_2atmpS1380 = _M0L6_2atmpS1381 << 2;
    if (
      _M0L6_2atmpS1380 < 0
      || _M0L6_2atmpS1380
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 92 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S109 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1380];
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1377 = _M0MPC14byte4Byte8to__char(_M0L2x0S107);
    moonbit_incref(_M0L3bufS89);
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1377);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1378 = _M0MPC14byte4Byte8to__char(_M0L2x1S108);
    moonbit_incref(_M0L3bufS89);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1378);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1379 = _M0MPC14byte4Byte8to__char(_M0L2x2S109);
    moonbit_incref(_M0L3bufS89);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1379);
    moonbit_incref(_M0L3bufS89);
    #line 96 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, 61);
  } else {
    moonbit_decref(_M0L4dataS91);
  }
  #line 98 "/Users/user/.moon/lib/core/builtin/console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS89);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS87,
  int32_t _M0L2chS86
) {
  uint32_t _M0L4codeS85;
  #line 90 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 91 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS85 = _M0MPC14char4Char8to__uint(_M0L2chS86);
  if (_M0L4codeS85 <= 65535u) {
    int32_t _M0L3lenS1324 = _M0L4selfS87->$1;
    int32_t _M0L6_2atmpS1323 = _M0L3lenS1324 + 1;
    uint16_t* _M0L4dataS1325;
    int32_t _M0L3lenS1326;
    int32_t _M0L6_2atmpS1327;
    int32_t _M0L3lenS1329;
    int32_t _M0L6_2atmpS1328;
    moonbit_incref(_M0L4selfS87);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS87, _M0L6_2atmpS1323);
    _M0L4dataS1325 = _M0L4selfS87->$0;
    _M0L3lenS1326 = _M0L4selfS87->$1;
    moonbit_incref(_M0L4dataS1325);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1327 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS85);
    if (
      _M0L3lenS1326 < 0
      || _M0L3lenS1326 >= Moonbit_array_length(_M0L4dataS1325)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1325[_M0L3lenS1326] = _M0L6_2atmpS1327;
    moonbit_decref(_M0L4dataS1325);
    _M0L3lenS1329 = _M0L4selfS87->$1;
    _M0L6_2atmpS1328 = _M0L3lenS1329 + 1;
    _M0L4selfS87->$1 = _M0L6_2atmpS1328;
    moonbit_decref(_M0L4selfS87);
  } else if (_M0L4codeS85 <= 1114111u) {
    int32_t _M0L3lenS1331 = _M0L4selfS87->$1;
    int32_t _M0L6_2atmpS1330 = _M0L3lenS1331 + 2;
    uint32_t _M0L4codeS88;
    uint16_t* _M0L4dataS1332;
    int32_t _M0L3lenS1333;
    uint32_t _M0L6_2atmpS1336;
    uint32_t _M0L6_2atmpS1335;
    int32_t _M0L6_2atmpS1334;
    uint16_t* _M0L4dataS1337;
    int32_t _M0L3lenS1342;
    int32_t _M0L6_2atmpS1338;
    uint32_t _M0L6_2atmpS1341;
    uint32_t _M0L6_2atmpS1340;
    int32_t _M0L6_2atmpS1339;
    int32_t _M0L3lenS1344;
    int32_t _M0L6_2atmpS1343;
    moonbit_incref(_M0L4selfS87);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS87, _M0L6_2atmpS1330);
    _M0L4codeS88 = _M0L4codeS85 - 65536u;
    _M0L4dataS1332 = _M0L4selfS87->$0;
    _M0L3lenS1333 = _M0L4selfS87->$1;
    _M0L6_2atmpS1336 = _M0L4codeS88 >> 10;
    _M0L6_2atmpS1335 = 55296u + _M0L6_2atmpS1336;
    moonbit_incref(_M0L4dataS1332);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1334 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1335);
    if (
      _M0L3lenS1333 < 0
      || _M0L3lenS1333 >= Moonbit_array_length(_M0L4dataS1332)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1332[_M0L3lenS1333] = _M0L6_2atmpS1334;
    moonbit_decref(_M0L4dataS1332);
    _M0L4dataS1337 = _M0L4selfS87->$0;
    _M0L3lenS1342 = _M0L4selfS87->$1;
    _M0L6_2atmpS1338 = _M0L3lenS1342 + 1;
    _M0L6_2atmpS1341 = _M0L4codeS88 & 1023u;
    _M0L6_2atmpS1340 = 56320u + _M0L6_2atmpS1341;
    moonbit_incref(_M0L4dataS1337);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1339 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1340);
    if (
      _M0L6_2atmpS1338 < 0
      || _M0L6_2atmpS1338 >= Moonbit_array_length(_M0L4dataS1337)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1337[_M0L6_2atmpS1338] = _M0L6_2atmpS1339;
    moonbit_decref(_M0L4dataS1337);
    _M0L3lenS1344 = _M0L4selfS87->$1;
    _M0L6_2atmpS1343 = _M0L3lenS1344 + 2;
    _M0L4selfS87->$1 = _M0L6_2atmpS1343;
    moonbit_decref(_M0L4selfS87);
  } else {
    moonbit_decref(_M0L4selfS87);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_122.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS79,
  int32_t _M0L8requiredS80
) {
  uint16_t* _M0L4dataS1322;
  int32_t _M0L12current__lenS78;
  int32_t _M0L13enough__spaceS81;
  int32_t _M0L13enough__spaceS82;
  int32_t _M0L6_2atmpS1320;
  uint16_t* _M0L9new__dataS84;
  uint16_t* _M0L4dataS1318;
  int32_t _M0L3lenS1319;
  uint16_t* _M0L6_2aoldS2873;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS1322 = _M0L4selfS79->$0;
  _M0L12current__lenS78 = Moonbit_array_length(_M0L4dataS1322);
  if (_M0L8requiredS80 <= _M0L12current__lenS78) {
    moonbit_decref(_M0L4selfS79);
    return 0;
  }
  _M0L13enough__spaceS82 = _M0L12current__lenS78;
  while (1) {
    if (_M0L13enough__spaceS82 < _M0L8requiredS80) {
      int32_t _M0L6_2atmpS1321 = _M0L13enough__spaceS82 * 2;
      _M0L13enough__spaceS82 = _M0L6_2atmpS1321;
      continue;
    } else {
      _M0L13enough__spaceS81 = _M0L13enough__spaceS82;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1320 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS84
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS81, _M0L6_2atmpS1320);
  _M0L4dataS1318 = _M0L4selfS79->$0;
  _M0L3lenS1319 = _M0L4selfS79->$1;
  moonbit_incref(_M0L4dataS1318);
  moonbit_incref(_M0L9new__dataS84);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS84, 0, _M0L4dataS1318, 0, _M0L3lenS1319);
  _M0L6_2aoldS2873 = _M0L4selfS79->$0;
  moonbit_decref(_M0L6_2aoldS2873);
  _M0L4selfS79->$0 = _M0L9new__dataS84;
  moonbit_decref(_M0L4selfS79);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS77) {
  int32_t _M0L6_2atmpS1317;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1317 = *(int32_t*)&_M0L4selfS77;
  return (uint16_t)_M0L6_2atmpS1317;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS76) {
  int32_t _M0L6_2atmpS1316;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1316 = _M0L4selfS76;
  return *(uint32_t*)&_M0L6_2atmpS1316;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS74
) {
  int32_t _M0L3lenS1308;
  uint16_t* _M0L4dataS1310;
  int32_t _M0L6_2atmpS1309;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1308 = _M0L4selfS74->$1;
  _M0L4dataS1310 = _M0L4selfS74->$0;
  _M0L6_2atmpS1309 = Moonbit_array_length(_M0L4dataS1310);
  if (_M0L3lenS1308 == _M0L6_2atmpS1309) {
    uint16_t* _M0L8_2afieldS2876 = _M0L4selfS74->$0;
    int32_t _M0L6_2acntS3102 = Moonbit_object_header(_M0L4selfS74)->rc;
    uint16_t* _M0L4dataS1311;
    if (_M0L6_2acntS3102 > 1) {
      int32_t _M0L11_2anew__cntS3103 = _M0L6_2acntS3102 - 1;
      Moonbit_object_header(_M0L4selfS74)->rc = _M0L11_2anew__cntS3103;
      moonbit_incref(_M0L8_2afieldS2876);
    } else if (_M0L6_2acntS3102 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS74);
    }
    _M0L4dataS1311 = _M0L8_2afieldS2876;
    return _M0L4dataS1311;
  } else {
    int32_t _M0L3lenS1314 = _M0L4selfS74->$1;
    int32_t _M0L6_2atmpS1315;
    uint16_t* _M0L4dataS75;
    uint16_t* _M0L4dataS1312;
    int32_t _M0L3lenS1313;
    int32_t _M0L6_2acntS3104;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1315 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS75
    = (uint16_t*)moonbit_make_string(_M0L3lenS1314, _M0L6_2atmpS1315);
    _M0L4dataS1312 = _M0L4selfS74->$0;
    _M0L3lenS1313 = _M0L4selfS74->$1;
    _M0L6_2acntS3104 = Moonbit_object_header(_M0L4selfS74)->rc;
    if (_M0L6_2acntS3104 > 1) {
      int32_t _M0L11_2anew__cntS3105 = _M0L6_2acntS3104 - 1;
      Moonbit_object_header(_M0L4selfS74)->rc = _M0L11_2anew__cntS3105;
      moonbit_incref(_M0L4dataS1312);
    } else if (_M0L6_2acntS3104 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS74);
    }
    moonbit_incref(_M0L4dataS75);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS75, 0, _M0L4dataS1312, 0, _M0L3lenS1313);
    return _M0L4dataS75;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 153 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS72
) {
  int32_t _M0L7initialS71;
  uint16_t* _M0L4dataS73;
  struct _M0TPB13StringBuilder* _block_3371;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS72 < 1) {
    _M0L7initialS71 = 1;
  } else {
    int32_t _M0L6_2atmpS1307 = _M0L10size__hintS72 + 1;
    _M0L7initialS71 = _M0L6_2atmpS1307 / 2;
  }
  _M0L4dataS73 = (uint16_t*)moonbit_make_string(_M0L7initialS71, 0);
  _block_3371
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3371)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3371->$0 = _M0L4dataS73;
  _block_3371->$1 = 0;
  return _block_3371;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS1306;
  #line 1867 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1306 = (int32_t)_M0L4selfS70;
  return _M0L6_2atmpS1306;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS55,
  int32_t _M0L11dst__offsetS56,
  moonbit_string_t* _M0L3srcS57,
  int32_t _M0L11src__offsetS58,
  int32_t _M0L3lenS59
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS55, _M0L11dst__offsetS56, _M0L3srcS57, _M0L11src__offsetS58, _M0L3lenS59);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS60,
  int32_t _M0L11dst__offsetS61,
  struct _M0TUsiE** _M0L3srcS62,
  int32_t _M0L11src__offsetS63,
  int32_t _M0L3lenS64
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS60, _M0L11dst__offsetS61, _M0L3srcS62, _M0L11src__offsetS63, _M0L3lenS64);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp6stream4FileE(
  struct _M0TP36mulpjs4mulp6stream4File** _M0L3dstS65,
  int32_t _M0L11dst__offsetS66,
  struct _M0TP36mulpjs4mulp6stream4File** _M0L3srcS67,
  int32_t _M0L11src__offsetS68,
  int32_t _M0L3lenS69
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP36mulpjs4mulp6stream4FileEE(_M0L3dstS65, _M0L11dst__offsetS66, _M0L3srcS67, _M0L11src__offsetS68, _M0L3lenS69);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS19,
  int32_t _M0L11dst__offsetS21,
  uint16_t* _M0L3srcS20,
  int32_t _M0L11src__offsetS22,
  int32_t _M0L3lenS24
) {
  int32_t _if__result_3372;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS19 == _M0L3srcS20) {
    _if__result_3372 = _M0L11dst__offsetS21 < _M0L11src__offsetS22;
  } else {
    _if__result_3372 = 0;
  }
  if (_if__result_3372) {
    int32_t _M0L1iS23 = 0;
    while (1) {
      if (_M0L1iS23 < _M0L3lenS24) {
        int32_t _M0L6_2atmpS1270 = _M0L11dst__offsetS21 + _M0L1iS23;
        int32_t _M0L6_2atmpS1272 = _M0L11src__offsetS22 + _M0L1iS23;
        int32_t _M0L6_2atmpS1271;
        int32_t _M0L6_2atmpS1273;
        if (
          _M0L6_2atmpS1272 < 0
          || _M0L6_2atmpS1272 >= Moonbit_array_length(_M0L3srcS20)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1271 = (int32_t)_M0L3srcS20[_M0L6_2atmpS1272];
        if (
          _M0L6_2atmpS1270 < 0
          || _M0L6_2atmpS1270 >= Moonbit_array_length(_M0L3dstS19)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS19[_M0L6_2atmpS1270] = _M0L6_2atmpS1271;
        _M0L6_2atmpS1273 = _M0L1iS23 + 1;
        _M0L1iS23 = _M0L6_2atmpS1273;
        continue;
      } else {
        moonbit_decref(_M0L3srcS20);
        moonbit_decref(_M0L3dstS19);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1278 = _M0L3lenS24 - 1;
    int32_t _M0L1iS26 = _M0L6_2atmpS1278;
    while (1) {
      if (_M0L1iS26 >= 0) {
        int32_t _M0L6_2atmpS1274 = _M0L11dst__offsetS21 + _M0L1iS26;
        int32_t _M0L6_2atmpS1276 = _M0L11src__offsetS22 + _M0L1iS26;
        int32_t _M0L6_2atmpS1275;
        int32_t _M0L6_2atmpS1277;
        if (
          _M0L6_2atmpS1276 < 0
          || _M0L6_2atmpS1276 >= Moonbit_array_length(_M0L3srcS20)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1275 = (int32_t)_M0L3srcS20[_M0L6_2atmpS1276];
        if (
          _M0L6_2atmpS1274 < 0
          || _M0L6_2atmpS1274 >= Moonbit_array_length(_M0L3dstS19)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS19[_M0L6_2atmpS1274] = _M0L6_2atmpS1275;
        _M0L6_2atmpS1277 = _M0L1iS26 - 1;
        _M0L1iS26 = _M0L6_2atmpS1277;
        continue;
      } else {
        moonbit_decref(_M0L3srcS20);
        moonbit_decref(_M0L3dstS19);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS28,
  int32_t _M0L11dst__offsetS30,
  moonbit_string_t* _M0L3srcS29,
  int32_t _M0L11src__offsetS31,
  int32_t _M0L3lenS33
) {
  int32_t _if__result_3375;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS28 == _M0L3srcS29) {
    _if__result_3375 = _M0L11dst__offsetS30 < _M0L11src__offsetS31;
  } else {
    _if__result_3375 = 0;
  }
  if (_if__result_3375) {
    int32_t _M0L1iS32 = 0;
    while (1) {
      if (_M0L1iS32 < _M0L3lenS33) {
        int32_t _M0L6_2atmpS1279 = _M0L11dst__offsetS30 + _M0L1iS32;
        int32_t _M0L6_2atmpS1281 = _M0L11src__offsetS31 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS1280;
        moonbit_string_t _M0L6_2aoldS2879;
        int32_t _M0L6_2atmpS1282;
        if (
          _M0L6_2atmpS1281 < 0
          || _M0L6_2atmpS1281 >= Moonbit_array_length(_M0L3srcS29)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1280 = (moonbit_string_t)_M0L3srcS29[_M0L6_2atmpS1281];
        if (
          _M0L6_2atmpS1279 < 0
          || _M0L6_2atmpS1279 >= Moonbit_array_length(_M0L3dstS28)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2879 = (moonbit_string_t)_M0L3dstS28[_M0L6_2atmpS1279];
        moonbit_incref(_M0L6_2atmpS1280);
        moonbit_decref(_M0L6_2aoldS2879);
        _M0L3dstS28[_M0L6_2atmpS1279] = _M0L6_2atmpS1280;
        _M0L6_2atmpS1282 = _M0L1iS32 + 1;
        _M0L1iS32 = _M0L6_2atmpS1282;
        continue;
      } else {
        moonbit_decref(_M0L3srcS29);
        moonbit_decref(_M0L3dstS28);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1287 = _M0L3lenS33 - 1;
    int32_t _M0L1iS35 = _M0L6_2atmpS1287;
    while (1) {
      if (_M0L1iS35 >= 0) {
        int32_t _M0L6_2atmpS1283 = _M0L11dst__offsetS30 + _M0L1iS35;
        int32_t _M0L6_2atmpS1285 = _M0L11src__offsetS31 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS1284;
        moonbit_string_t _M0L6_2aoldS2881;
        int32_t _M0L6_2atmpS1286;
        if (
          _M0L6_2atmpS1285 < 0
          || _M0L6_2atmpS1285 >= Moonbit_array_length(_M0L3srcS29)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1284 = (moonbit_string_t)_M0L3srcS29[_M0L6_2atmpS1285];
        if (
          _M0L6_2atmpS1283 < 0
          || _M0L6_2atmpS1283 >= Moonbit_array_length(_M0L3dstS28)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2881 = (moonbit_string_t)_M0L3dstS28[_M0L6_2atmpS1283];
        moonbit_incref(_M0L6_2atmpS1284);
        moonbit_decref(_M0L6_2aoldS2881);
        _M0L3dstS28[_M0L6_2atmpS1283] = _M0L6_2atmpS1284;
        _M0L6_2atmpS1286 = _M0L1iS35 - 1;
        _M0L1iS35 = _M0L6_2atmpS1286;
        continue;
      } else {
        moonbit_decref(_M0L3srcS29);
        moonbit_decref(_M0L3dstS28);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS37,
  int32_t _M0L11dst__offsetS39,
  struct _M0TUsiE** _M0L3srcS38,
  int32_t _M0L11src__offsetS40,
  int32_t _M0L3lenS42
) {
  int32_t _if__result_3378;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS37 == _M0L3srcS38) {
    _if__result_3378 = _M0L11dst__offsetS39 < _M0L11src__offsetS40;
  } else {
    _if__result_3378 = 0;
  }
  if (_if__result_3378) {
    int32_t _M0L1iS41 = 0;
    while (1) {
      if (_M0L1iS41 < _M0L3lenS42) {
        int32_t _M0L6_2atmpS1288 = _M0L11dst__offsetS39 + _M0L1iS41;
        int32_t _M0L6_2atmpS1290 = _M0L11src__offsetS40 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS1289;
        struct _M0TUsiE* _M0L6_2aoldS2883;
        int32_t _M0L6_2atmpS1291;
        if (
          _M0L6_2atmpS1290 < 0
          || _M0L6_2atmpS1290 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1289 = (struct _M0TUsiE*)_M0L3srcS38[_M0L6_2atmpS1290];
        if (
          _M0L6_2atmpS1288 < 0
          || _M0L6_2atmpS1288 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2883 = (struct _M0TUsiE*)_M0L3dstS37[_M0L6_2atmpS1288];
        if (_M0L6_2atmpS1289) {
          moonbit_incref(_M0L6_2atmpS1289);
        }
        if (_M0L6_2aoldS2883) {
          moonbit_decref(_M0L6_2aoldS2883);
        }
        _M0L3dstS37[_M0L6_2atmpS1288] = _M0L6_2atmpS1289;
        _M0L6_2atmpS1291 = _M0L1iS41 + 1;
        _M0L1iS41 = _M0L6_2atmpS1291;
        continue;
      } else {
        moonbit_decref(_M0L3srcS38);
        moonbit_decref(_M0L3dstS37);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1296 = _M0L3lenS42 - 1;
    int32_t _M0L1iS44 = _M0L6_2atmpS1296;
    while (1) {
      if (_M0L1iS44 >= 0) {
        int32_t _M0L6_2atmpS1292 = _M0L11dst__offsetS39 + _M0L1iS44;
        int32_t _M0L6_2atmpS1294 = _M0L11src__offsetS40 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS1293;
        struct _M0TUsiE* _M0L6_2aoldS2885;
        int32_t _M0L6_2atmpS1295;
        if (
          _M0L6_2atmpS1294 < 0
          || _M0L6_2atmpS1294 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1293 = (struct _M0TUsiE*)_M0L3srcS38[_M0L6_2atmpS1294];
        if (
          _M0L6_2atmpS1292 < 0
          || _M0L6_2atmpS1292 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2885 = (struct _M0TUsiE*)_M0L3dstS37[_M0L6_2atmpS1292];
        if (_M0L6_2atmpS1293) {
          moonbit_incref(_M0L6_2atmpS1293);
        }
        if (_M0L6_2aoldS2885) {
          moonbit_decref(_M0L6_2aoldS2885);
        }
        _M0L3dstS37[_M0L6_2atmpS1292] = _M0L6_2atmpS1293;
        _M0L6_2atmpS1295 = _M0L1iS44 - 1;
        _M0L1iS44 = _M0L6_2atmpS1295;
        continue;
      } else {
        moonbit_decref(_M0L3srcS38);
        moonbit_decref(_M0L3dstS37);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP36mulpjs4mulp6stream4FileEE(
  struct _M0TP36mulpjs4mulp6stream4File** _M0L3dstS46,
  int32_t _M0L11dst__offsetS48,
  struct _M0TP36mulpjs4mulp6stream4File** _M0L3srcS47,
  int32_t _M0L11src__offsetS49,
  int32_t _M0L3lenS51
) {
  int32_t _if__result_3381;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS46 == _M0L3srcS47) {
    _if__result_3381 = _M0L11dst__offsetS48 < _M0L11src__offsetS49;
  } else {
    _if__result_3381 = 0;
  }
  if (_if__result_3381) {
    int32_t _M0L1iS50 = 0;
    while (1) {
      if (_M0L1iS50 < _M0L3lenS51) {
        int32_t _M0L6_2atmpS1297 = _M0L11dst__offsetS48 + _M0L1iS50;
        int32_t _M0L6_2atmpS1299 = _M0L11src__offsetS49 + _M0L1iS50;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS1298;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2aoldS2887;
        int32_t _M0L6_2atmpS1300;
        if (
          _M0L6_2atmpS1299 < 0
          || _M0L6_2atmpS1299 >= Moonbit_array_length(_M0L3srcS47)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1298
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3srcS47[
            _M0L6_2atmpS1299
          ];
        if (
          _M0L6_2atmpS1297 < 0
          || _M0L6_2atmpS1297 >= Moonbit_array_length(_M0L3dstS46)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2887
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3dstS46[
            _M0L6_2atmpS1297
          ];
        if (_M0L6_2atmpS1298) {
          moonbit_incref(_M0L6_2atmpS1298);
        }
        if (_M0L6_2aoldS2887) {
          moonbit_decref(_M0L6_2aoldS2887);
        }
        _M0L3dstS46[_M0L6_2atmpS1297] = _M0L6_2atmpS1298;
        _M0L6_2atmpS1300 = _M0L1iS50 + 1;
        _M0L1iS50 = _M0L6_2atmpS1300;
        continue;
      } else {
        moonbit_decref(_M0L3srcS47);
        moonbit_decref(_M0L3dstS46);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1305 = _M0L3lenS51 - 1;
    int32_t _M0L1iS53 = _M0L6_2atmpS1305;
    while (1) {
      if (_M0L1iS53 >= 0) {
        int32_t _M0L6_2atmpS1301 = _M0L11dst__offsetS48 + _M0L1iS53;
        int32_t _M0L6_2atmpS1303 = _M0L11src__offsetS49 + _M0L1iS53;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS1302;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2aoldS2889;
        int32_t _M0L6_2atmpS1304;
        if (
          _M0L6_2atmpS1303 < 0
          || _M0L6_2atmpS1303 >= Moonbit_array_length(_M0L3srcS47)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1302
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3srcS47[
            _M0L6_2atmpS1303
          ];
        if (
          _M0L6_2atmpS1301 < 0
          || _M0L6_2atmpS1301 >= Moonbit_array_length(_M0L3dstS46)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2889
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3dstS46[
            _M0L6_2atmpS1301
          ];
        if (_M0L6_2atmpS1302) {
          moonbit_incref(_M0L6_2atmpS1302);
        }
        if (_M0L6_2aoldS2889) {
          moonbit_decref(_M0L6_2aoldS2889);
        }
        _M0L3dstS46[_M0L6_2atmpS1301] = _M0L6_2atmpS1302;
        _M0L6_2atmpS1304 = _M0L1iS53 - 1;
        _M0L1iS53 = _M0L6_2atmpS1304;
        continue;
      } else {
        moonbit_decref(_M0L3srcS47);
        moonbit_decref(_M0L3dstS46);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5valueS18
) {
  uint32_t _M0L3accS1269;
  uint32_t _M0L6_2atmpS1268;
  #line 236 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1269 = _M0L4selfS17->$0;
  _M0L6_2atmpS1268 = _M0L3accS1269 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1268;
  #line 238 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1266;
  uint32_t _M0L6_2atmpS1267;
  uint32_t _M0L6_2atmpS1265;
  uint32_t _M0L6_2atmpS1264;
  uint32_t _M0L6_2atmpS1263;
  #line 451 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1266 = _M0L4selfS15->$0;
  _M0L6_2atmpS1267 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1265 = _M0L3accS1266 + _M0L6_2atmpS1267;
  #line 452 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1264 = _M0FPB4rotl(_M0L6_2atmpS1265, 17);
  _M0L6_2atmpS1263 = _M0L6_2atmpS1264 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1263;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1260;
  int32_t _M0L6_2atmpS1262;
  uint32_t _M0L6_2atmpS1261;
  #line 461 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1260 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1262 = 32 - _M0L1rS14;
  _M0L6_2atmpS1261 = _M0L1xS13 >> (_M0L6_2atmpS1262 & 31);
  return _M0L6_2atmpS1260 | _M0L6_2atmpS1261;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__5208S9,
  struct _M0TPB6Logger _M0L10_2ax__5209S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS2891;
  int32_t _M0L6_2acntS3106;
  moonbit_string_t _M0L15_2a_2aarg__5210S11;
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__5208S9;
  _M0L8_2afieldS2891 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS3106 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS3106 > 1) {
    int32_t _M0L11_2anew__cntS3107 = _M0L6_2acntS3106 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS3107;
    moonbit_incref(_M0L8_2afieldS2891);
  } else if (_M0L6_2acntS3106 == 1) {
    #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__5210S11 = _M0L8_2afieldS2891;
  if (_M0L10_2ax__5209S12.$1) {
    moonbit_incref(_M0L10_2ax__5209S12.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S12.$0->$method_0(_M0L10_2ax__5209S12.$1, (moonbit_string_t)moonbit_string_literal_123.data);
  if (_M0L10_2ax__5209S12.$1) {
    moonbit_incref(_M0L10_2ax__5209S12.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__5209S12, _M0L15_2a_2aarg__5210S11);
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S12.$0->$method_0(_M0L10_2ax__5209S12.$1, (moonbit_string_t)moonbit_string_literal_124.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS8,
  moonbit_string_t _M0L3objS7
) {
  #line 155 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 156 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS7, _M0L4selfS8);
  return 0;
}

int32_t _M0FPC15abort5abortGuE(moonbit_string_t _M0L3msgS1) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS1);
  moonbit_decref(_M0L3msgS1);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
  return 0;
}

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0FPC15abort5abortGRPB5ArrayGRP36mulpjs4mulp6stream4FileEE(
  moonbit_string_t _M0L3msgS2
) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS2);
  moonbit_decref(_M0L3msgS2);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TP36mulpjs4mulp6stream4File* _M0FPC15abort5abortGORP36mulpjs4mulp6stream4FileE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

void* _M0FPC15abort5abortGRP36mulpjs4mulp4core9MulpErrorE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS5) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS5);
  moonbit_decref(_M0L3msgS5);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS6
) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS6);
  moonbit_decref(_M0L3msgS6);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1182) {
  switch (Moonbit_object_tag(_M0L4_2aeS1182)) {
    case 1: {
      moonbit_decref(_M0L4_2aeS1182);
      return (moonbit_string_t)moonbit_string_literal_125.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1182);
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1182);
      return (moonbit_string_t)moonbit_string_literal_126.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1182);
      return (moonbit_string_t)moonbit_string_literal_127.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1182);
      return (moonbit_string_t)moonbit_string_literal_128.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1223
) {
  moonbit_string_t _M0L7_2aselfS1222 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1223;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS1222);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1221,
  struct _M0TPB6Logger _M0L8_2aparamS1220
) {
  moonbit_string_t _M0L7_2aselfS1219 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1221;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS1219, _M0L8_2aparamS1220);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1217
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1218 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1217;
  int32_t _M0L7_2aselfS1216 = _M0L14_2aboxed__selfS1218->$0;
  moonbit_decref(_M0L14_2aboxed__selfS1218);
  return _M0IPC14bool4BoolPB4Show10to__string(_M0L7_2aselfS1216);
}

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1214,
  struct _M0TPB6Logger _M0L8_2aparamS1213
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1215 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1214;
  int32_t _M0L7_2aselfS1212 = _M0L14_2aboxed__selfS1215->$0;
  moonbit_decref(_M0L14_2aboxed__selfS1215);
  _M0IPC14bool4BoolPB4Show6output(_M0L7_2aselfS1212, _M0L8_2aparamS1213);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1211,
  int32_t _M0L8_2aparamS1210
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1209 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1211;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1209, _M0L8_2aparamS1210);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1208,
  struct _M0TPC16string10StringView _M0L8_2aparamS1207
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1206 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1208;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1206, _M0L8_2aparamS1207);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1205,
  moonbit_string_t _M0L8_2aparamS1202,
  int32_t _M0L8_2aparamS1203,
  int32_t _M0L8_2aparamS1204
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1201 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1205;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1201, _M0L8_2aparamS1202, _M0L8_2aparamS1203, _M0L8_2aparamS1204);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1200,
  moonbit_string_t _M0L8_2aparamS1199
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1198 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1200;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1198, _M0L8_2aparamS1199);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1196
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1197 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1196;
  int32_t _M0L7_2aselfS1195 = _M0L14_2aboxed__selfS1197->$0;
  moonbit_decref(_M0L14_2aboxed__selfS1197);
  return _M0IPC13int3IntPB4Show10to__string(_M0L7_2aselfS1195);
}

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1193,
  struct _M0TPB6Logger _M0L8_2aparamS1192
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1194 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1193;
  int32_t _M0L7_2aselfS1191 = _M0L14_2aboxed__selfS1194->$0;
  moonbit_decref(_M0L14_2aboxed__selfS1194);
  _M0IPC13int3IntPB4Show6output(_M0L7_2aselfS1191, _M0L8_2aparamS1192);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGRPB5ArrayGsEE(
  void* _M0L11_2aobj__ptrS1190
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS1189 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS1190;
  return _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(_M0L7_2aselfS1189);
}

int32_t _M0IPC15array5ArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGsE(
  void* _M0L11_2aobj__ptrS1188,
  struct _M0TPB6Logger _M0L8_2aparamS1187
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS1186 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS1188;
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L7_2aselfS1186, _M0L8_2aparamS1187);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1259 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1258;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1239;
  moonbit_string_t* _M0L6_2atmpS1257;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1256;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1240;
  moonbit_string_t* _M0L6_2atmpS1255;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1254;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1241;
  moonbit_string_t* _M0L6_2atmpS1253;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1252;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1242;
  moonbit_string_t* _M0L6_2atmpS1251;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1250;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1243;
  moonbit_string_t* _M0L6_2atmpS1249;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1248;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1244;
  moonbit_string_t* _M0L6_2atmpS1247;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1246;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1245;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1108;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1238;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1237;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1236;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1231;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1109;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1235;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1234;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1233;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1232;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1107;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1230;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1229;
  _M0L6_2atmpS1259[0] = (moonbit_string_t)moonbit_string_literal_129.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1258
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1258)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1258->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1258->$1 = _M0L6_2atmpS1259;
  _M0L8_2atupleS1239
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1239)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1239->$0 = 0;
  _M0L8_2atupleS1239->$1 = _M0L8_2atupleS1258;
  _M0L6_2atmpS1257 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1257[0] = (moonbit_string_t)moonbit_string_literal_130.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1256
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1256)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1256->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1256->$1 = _M0L6_2atmpS1257;
  _M0L8_2atupleS1240
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1240)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1240->$0 = 1;
  _M0L8_2atupleS1240->$1 = _M0L8_2atupleS1256;
  _M0L6_2atmpS1255 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1255[0] = (moonbit_string_t)moonbit_string_literal_131.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1254
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1254)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1254->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1254->$1 = _M0L6_2atmpS1255;
  _M0L8_2atupleS1241
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1241)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1241->$0 = 2;
  _M0L8_2atupleS1241->$1 = _M0L8_2atupleS1254;
  _M0L6_2atmpS1253 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1253[0] = (moonbit_string_t)moonbit_string_literal_132.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1252
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1252)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1252->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1252->$1 = _M0L6_2atmpS1253;
  _M0L8_2atupleS1242
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1242)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1242->$0 = 3;
  _M0L8_2atupleS1242->$1 = _M0L8_2atupleS1252;
  _M0L6_2atmpS1251 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1251[0] = (moonbit_string_t)moonbit_string_literal_133.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__4_2eclo);
  _M0L8_2atupleS1250
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1250)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1250->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__4_2eclo;
  _M0L8_2atupleS1250->$1 = _M0L6_2atmpS1251;
  _M0L8_2atupleS1243
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1243)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1243->$0 = 4;
  _M0L8_2atupleS1243->$1 = _M0L8_2atupleS1250;
  _M0L6_2atmpS1249 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1249[0] = (moonbit_string_t)moonbit_string_literal_134.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__5_2eclo);
  _M0L8_2atupleS1248
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1248)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1248->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__5_2eclo;
  _M0L8_2atupleS1248->$1 = _M0L6_2atmpS1249;
  _M0L8_2atupleS1244
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1244)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1244->$0 = 5;
  _M0L8_2atupleS1244->$1 = _M0L8_2atupleS1248;
  _M0L6_2atmpS1247 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1247[0] = (moonbit_string_t)moonbit_string_literal_135.data;
  moonbit_incref(_M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__6_2eclo);
  _M0L8_2atupleS1246
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1246)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1246->$0
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test53____test__706970656c696e655f746573742e6d6274__6_2eclo;
  _M0L8_2atupleS1246->$1 = _M0L6_2atmpS1247;
  _M0L8_2atupleS1245
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1245)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1245->$0 = 6;
  _M0L8_2atupleS1245->$1 = _M0L8_2atupleS1246;
  _M0L7_2abindS1108
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(7);
  _M0L7_2abindS1108[0] = _M0L8_2atupleS1239;
  _M0L7_2abindS1108[1] = _M0L8_2atupleS1240;
  _M0L7_2abindS1108[2] = _M0L8_2atupleS1241;
  _M0L7_2abindS1108[3] = _M0L8_2atupleS1242;
  _M0L7_2abindS1108[4] = _M0L8_2atupleS1243;
  _M0L7_2abindS1108[5] = _M0L8_2atupleS1244;
  _M0L7_2abindS1108[6] = _M0L8_2atupleS1245;
  _M0L6_2atmpS1238 = _M0L7_2abindS1108;
  _M0L6_2atmpS1237
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 7, _M0L6_2atmpS1238
  };
  #line 398 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1236
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1237);
  _M0L8_2atupleS1231
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1231)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1231->$0 = (moonbit_string_t)moonbit_string_literal_136.data;
  _M0L8_2atupleS1231->$1 = _M0L6_2atmpS1236;
  _M0L7_2abindS1109
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1235 = _M0L7_2abindS1109;
  _M0L6_2atmpS1234
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1235
  };
  #line 407 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1233
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1234);
  _M0L8_2atupleS1232
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1232)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1232->$0 = (moonbit_string_t)moonbit_string_literal_137.data;
  _M0L8_2atupleS1232->$1 = _M0L6_2atmpS1233;
  _M0L7_2abindS1107
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1107[0] = _M0L8_2atupleS1231;
  _M0L7_2abindS1107[1] = _M0L8_2atupleS1232;
  _M0L6_2atmpS1230 = _M0L7_2abindS1107;
  _M0L6_2atmpS1229
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1230
  };
  #line 397 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1229);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1228;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1176;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1177;
  int32_t _M0L7_2abindS1178;
  int32_t _M0L2__S1179;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1228
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1176
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1176)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1176->$0 = _M0L6_2atmpS1228;
  _M0L12async__testsS1176->$1 = 0;
  #line 446 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1177
  = _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1178 = _M0L7_2abindS1177->$1;
  _M0L2__S1179 = 0;
  while (1) {
    if (_M0L2__S1179 < _M0L7_2abindS1178) {
      struct _M0TUsiE** _M0L3bufS1227 = _M0L7_2abindS1177->$0;
      struct _M0TUsiE* _M0L3argS1180 =
        (struct _M0TUsiE*)_M0L3bufS1227[_M0L2__S1179];
      moonbit_string_t _M0L6_2atmpS1224 = _M0L3argS1180->$0;
      int32_t _M0L6_2atmpS1225 = _M0L3argS1180->$1;
      int32_t _M0L6_2atmpS1226;
      moonbit_incref(_M0L6_2atmpS1224);
      moonbit_incref(_M0L12async__testsS1176);
      #line 447 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
      _M0FP36mulpjs4mulp32stream__pipeline__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1176, _M0L6_2atmpS1224, _M0L6_2atmpS1225);
      _M0L6_2atmpS1226 = _M0L2__S1179 + 1;
      _M0L2__S1179 = _M0L6_2atmpS1226;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1177);
    }
    break;
  }
  #line 449 "/Users/user/workspace/github/gulp/mulp/stream_pipeline/__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP36mulpjs4mulp32stream__pipeline__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp32stream__pipeline__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1176);
  return 0;
}