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
struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTP36mulpjs4mulp4core9MulpError9GlobError;

struct _M0TP36mulpjs4mulp6stream9DestState;

struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTP36mulpjs4mulp4core9MulpError11ConfigError;

struct _M0DTPC15error5Error92mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0TPB6Logger;

struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__;

struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE;

struct _M0TURPB6LoggerRPC16string10StringViewE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOs;

struct _M0TP36mulpjs4mulp6stream4File;

struct _M0TPB4Show;

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE;

struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TPC15bytes9BytesView;

struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__;

struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed;

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TP36mulpjs4mulp6stream14ArrayFileState;

struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp6stream33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0DTP36mulpjs4mulp4core9MulpError14ParallelFailed;

struct _M0TPB9ArrayViewGsE;

struct _M0TWEu;

struct _M0DTP36mulpjs4mulp4core9MulpError10WatchError;

struct _M0TP36mulpjs4mulp4core7Context;

struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes;

struct _M0KTPB4ShowS4Bool;

struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink;

struct _M0Y4Bool;

struct _M0DTP36mulpjs4mulp4core9MulpError12TaskNotFound;

struct _M0TPB13StringBuilder;

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream;

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__;

struct _M0TUssE;

struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__;

struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0BTPB6Logger;

struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp6stream33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323;

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__;

struct _M0TP36mulpjs4mulp6stream9PipeState;

struct _M0TPB8MutLocalGiE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TP36mulpjs4mulp6stream10ByteStream;

struct _M0TPB5ArrayGUssEE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0TP36mulpjs4mulp4core17CancellationToken;

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok;

struct _M0DTP36mulpjs4mulp6stream12FileContents6Buffer;

struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__;

struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE;

struct _M0BTPB4Show;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS6String;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0TP36mulpjs4mulp6stream10FileStream;

struct _M0DTP36mulpjs4mulp6stream12FileContents4Text;

struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
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

struct _M0TP36mulpjs4mulp6stream9DestState {
  int32_t $0;
  
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

struct _M0DTP36mulpjs4mulp4core9MulpError11ConfigError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error92mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok {
  moonbit_string_t $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
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

struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE {
  int32_t $1;
  struct _M0TP36mulpjs4mulp6stream4File** $0;
  
};

struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp6stream9DestState* $0;
  struct _M0TP36mulpjs4mulp6stream4File* $1;
  
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

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok {
  struct _M0TP36mulpjs4mulp6stream10ByteStream* $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB8MutLocalGiE* $1;
  
};

struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok {
  struct _M0TP36mulpjs4mulp6stream4File* $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP36mulpjs4mulp6stream14ArrayFileState {
  int32_t $1;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* $0;
  
};

struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp6stream33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError14ParallelFailed {
  struct _M0TPB5ArrayGUssEE* $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0DTP36mulpjs4mulp4core9MulpError10WatchError {
  moonbit_string_t $0;
  
};

struct _M0TP36mulpjs4mulp4core7Context {
  int64_t $1;
  moonbit_string_t $0;
  struct _M0TP36mulpjs4mulp4core17CancellationToken* $2;
  
};

struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err {
  void* $0;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__ {
  int32_t(* code)(struct _M0TWEu*);
  void* $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes {
  struct _M0TP36mulpjs4mulp6stream10ByteStream* $0;
  
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

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TP36mulpjs4mulp6stream9PipeState* $0;
  
};

struct _M0TUssE {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__ {
  struct _M0TP36mulpjs4mulp6stream10FileStream*(* code)(
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
    struct _M0TP36mulpjs4mulp6stream4File*
  );
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

struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp6stream33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TP36mulpjs4mulp6stream9PipeState* $0;
  
};

struct _M0TP36mulpjs4mulp6stream9PipeState {
  struct _M0TP36mulpjs4mulp6stream10FileStream* $0;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* $1;
  struct _M0TP36mulpjs4mulp6stream10FileStream* $2;
  
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

struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok {
  moonbit_bytes_t $0;
  
};

struct _M0TP36mulpjs4mulp4core17CancellationToken {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTP36mulpjs4mulp6stream12FileContents6Buffer {
  moonbit_bytes_t $0;
  
};

struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__ {
  void*(* code)(
    struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*
  );
  struct _M0TWEu* $0;
  void* $1;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPB8MutLocalGiE* $1;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS6String {
  struct _M0BTPB4Show* $0;
  void* $1;
  
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

struct moonbit_result_0 _M0FP36mulpjs4mulp6stream63____test__66696c655f646573745f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN17error__to__stringS1332(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN14handle__resultS1323(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testC3392l425(
  struct _M0TWEu*
);

int32_t _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testC3388l426(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP36mulpjs4mulp6stream45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1257(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1252(
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1239(
  int32_t,
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP36mulpjs4mulp6stream28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp6stream34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp6stream53____test__66696c655f646573745f7762746573742e6d6274__0(
  
);

void* _M0MP36mulpjs4mulp6stream10ByteStream9read__all(
  struct _M0TP36mulpjs4mulp6stream10ByteStream*,
  int32_t
);

#define _M0FP36mulpjs4mulp6stream29symlink__supported__for__test mulp_symlink_supported_for_test

int32_t _M0FP36mulpjs4mulp6stream28path__is__symlink__for__test(
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp6stream33path__is__symlink__for__test__ffi mulp_path_is_symlink_for_test

int32_t _M0FP36mulpjs4mulp6stream30path__is__directory__for__test(
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp6stream35path__is__directory__for__test__ffi mulp_path_is_directory_for_test

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream10file__dest(
  moonbit_string_t
);

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream25file__dest__with__options(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream25file__dest__with__optionsC3154l119(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp6stream4File*
);

void* _M0FP36mulpjs4mulp6stream25file__dest__with__optionsC3158l126(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

void* _M0FP36mulpjs4mulp6stream21write__file__contents(
  struct _M0TP36mulpjs4mulp6stream4File*
);

void* _M0FP36mulpjs4mulp6stream25write__real__file__stream(
  moonbit_string_t,
  struct _M0TP36mulpjs4mulp6stream10ByteStream*
);

#define _M0FP36mulpjs4mulp6stream18open__file__writer mulp_open_file_writer

#define _M0FP36mulpjs4mulp6stream19file__writer__write mulp_file_writer_write

#define _M0FP36mulpjs4mulp6stream22file__writer__is__open mulp_file_writer_is_open

#define _M0FP36mulpjs4mulp6stream19file__writer__close mulp_file_writer_close

void* _M0FP36mulpjs4mulp6stream17write__real__file(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP36mulpjs4mulp6stream19write__real__buffer(
  moonbit_string_t,
  moonbit_bytes_t
);

#define _M0FP36mulpjs4mulp6stream18write__file__bytes mulp_write_file_bytes

#define _M0FP36mulpjs4mulp6stream19make__symlink__path mulp_make_symlink_path

#define _M0FP36mulpjs4mulp6stream21make__directory__path mulp_make_directory_path

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp6stream22with__real__dest__path(
  struct _M0TP36mulpjs4mulp6stream4File*,
  moonbit_string_t
);

moonbit_string_t _M0FP36mulpjs4mulp6stream24resolve__real__dest__dir(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0FP36mulpjs4mulp6stream22join__real__dest__path(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP36mulpjs4mulp6stream18file__byte__stream(
  moonbit_string_t,
  int32_t
);

int32_t _M0FP36mulpjs4mulp6stream18file__byte__streamC3091l9(struct _M0TWEu*);

void* _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__check(
  moonbit_string_t,
  int32_t,
  struct _M0TWEu*
);

void* _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__checkC3080l38(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__checkC3077l50(
  struct _M0TWEu*
);

#define _M0FP36mulpjs4mulp6stream18open__file__reader mulp_open_file_reader

#define _M0FP36mulpjs4mulp6stream18file__reader__read mulp_file_reader_read

#define _M0FP36mulpjs4mulp6stream22file__reader__is__open mulp_file_reader_is_open

int32_t _M0FP36mulpjs4mulp6stream19file__reader__close(void*);

void* _M0MP36mulpjs4mulp6stream10FileStream7collect(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TP36mulpjs4mulp4core7Context*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0MP36mulpjs4mulp6stream10FileStream4pipe(
  struct _M0TP36mulpjs4mulp6stream10FileStream*,
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*
);

void* _M0MP36mulpjs4mulp6stream10FileStream4pipeC3057l193(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0MP36mulpjs4mulp6stream10FileStream4pipeC3053l215(struct _M0TWEu*);

void* _M0MP36mulpjs4mulp6stream10FileStream4next(
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

int32_t _M0MP36mulpjs4mulp6stream10FileStream5close(
  struct _M0TP36mulpjs4mulp6stream10FileStream*
);

moonbit_string_t _M0MP36mulpjs4mulp6stream4File14relative__path(
  struct _M0TP36mulpjs4mulp6stream4File*
);

moonbit_string_t _M0FP36mulpjs4mulp6stream20trim__leading__slash(
  moonbit_string_t
);

int32_t _M0FP36mulpjs4mulp6stream22path__is__inside__base(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0MP36mulpjs4mulp6stream10ByteStream10read__next(
  struct _M0TP36mulpjs4mulp6stream10ByteStream*
);

int32_t _M0MP36mulpjs4mulp6stream10ByteStream5close(
  struct _M0TP36mulpjs4mulp6stream10ByteStream*
);

moonbit_string_t _M0MP36mulpjs4mulp6stream4File11source__map(
  struct _M0TP36mulpjs4mulp6stream4File*
);

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp6stream4file(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t,
  void*
);

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FP36mulpjs4mulp6stream12byte__stream(
  struct _M0TPB5ArrayGsE*
);

void* _M0FP36mulpjs4mulp6stream12byte__streamC3018l44(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FP36mulpjs4mulp6stream24byte__stream__from__pull(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp6stream24byte__stream__from__pullC3015l16(
  struct _M0TWEu*
);

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FP36mulpjs4mulp6stream37byte__stream__from__pull__with__close(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*,
  struct _M0TWEu*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream3src(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream12file__stream(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*
);

void* _M0FP36mulpjs4mulp6stream12file__streamC3002l129(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream24file__stream__from__pull(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*
);

int32_t _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2999l55(
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

void* _M0FP36mulpjs4mulp4core13stream__error(moonbit_string_t);

void* _M0FP36mulpjs4mulp4core19file__system__error(moonbit_string_t);

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

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView,
  int32_t
);

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView,
  int32_t
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

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15bytes9BytesView6length(struct _M0TPC15bytes9BytesView);

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

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0MPC16result6Result6unwrapGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE(
  void*
);

moonbit_string_t _M0MPC16result6Result6unwrapGsRP36mulpjs4mulp4core9MulpErrorE(
  void*
);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2020l680(struct _M0TWEOs*);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

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

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

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

int32_t _M0MPC16string6String11has__prefix(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView11has__prefix(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string6String11has__suffix(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView11has__suffix(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0MPC16string10StringView9rev__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB23brute__force__rev__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int64_t _M0FPB33boyer__moore__horspool__rev__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

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

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs*);

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

int32_t _M0MPC16uint166UInt1613is__surrogate(int32_t);

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

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FPC15abort5abortGRP36mulpjs4mulp6stream10ByteStreamE(
  moonbit_string_t
);

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t);

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t
);

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

int32_t mulp_file_writer_close(void*);

int32_t mulp_path_is_symlink_for_test(moonbit_bytes_t);

int32_t mulp_path_is_directory_for_test(moonbit_bytes_t);

int32_t mulp_file_reader_is_open(void*);

void* mulp_open_file_reader(moonbit_bytes_t, int32_t);

int32_t mulp_file_writer_is_open(void*);

void mulp_file_reader_close(void*);

int32_t mulp_file_writer_write(void*, moonbit_bytes_t);

moonbit_bytes_t mulp_file_reader_read(void*);

int32_t mulp_symlink_supported_for_test();

int32_t mulp_make_symlink_path(moonbit_bytes_t, moonbit_bytes_t);

void* mulp_open_file_writer(moonbit_bytes_t);

int32_t mulp_write_file_bytes(moonbit_bytes_t, moonbit_bytes_t);

int32_t mulp_make_directory_path(moonbit_bytes_t);

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 52, 58, 53, 45, 51, 52, 58, 56, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    47, 116, 109, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[44]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 43), 
    99, 97, 108, 108, 101, 100, 32, 96, 82, 101, 115, 117, 108, 116, 
    58, 58, 117, 110, 119, 114, 97, 112, 40, 41, 96, 32, 111, 110, 32, 
    97, 110, 32, 96, 69, 114, 114, 96, 32, 118, 97, 108, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_48 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    123, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    99, 97, 110, 110, 111, 116, 32, 99, 114, 101, 97, 116, 101, 32, 115, 
    121, 109, 108, 105, 110, 107, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 48, 58, 55, 54, 45, 51, 48, 58, 56, 50, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[87]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 86), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 115, 116, 
    114, 101, 97, 109, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 109, 117, 108, 112, 45, 100, 105, 114, 45, 111, 117, 116, 112, 
    117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[85]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 84), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 115, 116, 
    114, 101, 97, 109, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_72 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 56, 58, 49, 49, 45, 50, 56, 58, 54, 49, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    99, 97, 110, 110, 111, 116, 32, 119, 114, 105, 116, 101, 32, 102, 
    105, 108, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 56, 58, 51, 45, 50, 56, 58, 55, 56, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_38 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 47, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    98, 121, 116, 101, 32, 115, 116, 114, 101, 97, 109, 32, 101, 120, 
    99, 101, 101, 100, 101, 100, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 52, 58, 49, 51, 45, 51, 52, 58, 54, 54, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    99, 97, 110, 99, 101, 108, 108, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[36]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 35), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 109, 117, 108, 112, 45, 108, 105, 110, 107, 45, 111, 117, 116, 
    112, 117, 116, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    98, 121, 116, 101, 32, 115, 116, 114, 101, 97, 109, 32, 97, 108, 
    114, 101, 97, 100, 121, 32, 99, 111, 110, 115, 117, 109, 101, 100, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    116, 97, 114, 103, 101, 116, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 48, 58, 49, 51, 45, 51, 48, 58, 54, 54, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 50, 58, 49, 51, 45, 51, 50, 58, 53, 48, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    102, 105, 108, 101, 95, 100, 101, 115, 116, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    99, 97, 110, 110, 111, 116, 32, 99, 114, 101, 97, 116, 101, 32, 100, 
    105, 114, 101, 99, 116, 111, 114, 121, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 50, 58, 53, 45, 51, 50, 58, 54, 57, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    47, 116, 109, 112, 47, 109, 117, 108, 112, 45, 108, 105, 110, 107, 
    45, 111, 117, 116, 112, 117, 116, 46, 116, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    47, 116, 109, 112, 47, 109, 117, 108, 112, 45, 115, 121, 109, 108, 
    105, 110, 107, 45, 116, 97, 114, 103, 101, 116, 46, 116, 120, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[47]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 46), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 109, 117, 
    108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 115, 116, 114, 101, 
    97, 109, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[45]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 44), 
    102, 105, 108, 101, 32, 98, 121, 116, 101, 32, 115, 116, 114, 101, 
    97, 109, 32, 99, 104, 117, 110, 107, 32, 115, 105, 122, 101, 32, 
    109, 117, 115, 116, 32, 98, 101, 32, 112, 111, 115, 105, 116, 105, 
    118, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    32, 98, 121, 116, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    47, 116, 109, 112, 47, 109, 117, 108, 112, 45, 100, 105, 114, 45, 
    111, 117, 116, 112, 117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    102, 105, 108, 101, 32, 100, 101, 115, 116, 32, 99, 114, 101, 97, 
    116, 101, 115, 32, 100, 105, 114, 101, 99, 116, 111, 114, 105, 101, 
    115, 32, 97, 110, 100, 32, 115, 121, 109, 108, 105, 110, 107, 115, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 52, 58, 55, 54, 45, 51, 52, 58, 56, 51, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 48, 58, 53, 45, 51, 48, 58, 56, 51, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 56, 58, 55, 49, 45, 50, 56, 58, 55, 55, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_50 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 116, 114, 101, 97, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    115, 116, 114, 101, 97, 109, 47, 102, 105, 108, 101, 95, 100, 101, 
    115, 116, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 50, 58, 54, 48, 45, 51, 50, 58, 54, 56, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    99, 97, 110, 110, 111, 116, 32, 111, 112, 101, 110, 32, 102, 105, 
    108, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 115, 114, 99, 
    47, 109, 117, 108, 112, 45, 115, 121, 109, 108, 105, 110, 107, 45, 
    116, 97, 114, 103, 101, 116, 46, 116, 120, 116, 0
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

struct moonbit_object const moonbit_constant_constructor_4 =
  { -1, Moonbit_make_regular_object_header(2, 0, 4)};

struct { int32_t rc; uint32_t meta; struct _M0TWEu data; 
} const _M0FP36mulpjs4mulp6stream24byte__stream__from__pullC3015l16$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp6stream24byte__stream__from__pullC3015l16
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEu data; 
} const _M0FP36mulpjs4mulp6stream18file__byte__streamC3091l9$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp6stream18file__byte__streamC3091l9
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEu data; 
} const _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2999l55$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2999l55
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN17error__to__stringS1332$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN17error__to__stringS1332
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp6stream63____test__66696c655f646573745f7762746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp6stream63____test__66696c655f646573745f7762746573742e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp6stream59____test__66696c655f646573745f7762746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp6stream63____test__66696c655f646573745f7762746573742e6d6274__0_2edyncall$closure.data;

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

int64_t _M0FPB18brute__force__findN6constrS9146 = 0ll;

int64_t _M0FPB28boyer__moore__horspool__findN6constrS9145 = 0ll;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP36mulpjs4mulp6stream48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP36mulpjs4mulp6stream63____test__66696c655f646573745f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3423
) {
  return _M0FP36mulpjs4mulp6stream53____test__66696c655f646573745f7762746573742e6d6274__0();
}

int32_t _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1353,
  moonbit_string_t _M0L8filenameS1328,
  int32_t _M0L5indexS1331
) {
  struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323* _closure_3979;
  struct _M0TWssbEu* _M0L14handle__resultS1323;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1332;
  void* _M0L11_2atry__errS1347;
  struct moonbit_result_0 _tmp_3981;
  int32_t _handle__error__result_3982;
  int32_t _M0L6_2atmpS3411;
  void* _M0L3errS1348;
  moonbit_string_t _M0L4nameS1350;
  struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1351;
  moonbit_string_t _M0L8_2afieldS3424;
  int32_t _M0L6_2acntS3795;
  moonbit_string_t _M0L7_2anameS1352;
  #line 524 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_incref(_M0L8filenameS1328);
  _closure_3979
  = (struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323*)moonbit_malloc(sizeof(struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323));
  Moonbit_object_header(_closure_3979)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323, $1) >> 2, 1, 0);
  _closure_3979->code
  = &_M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN14handle__resultS1323;
  _closure_3979->$0 = _M0L5indexS1331;
  _closure_3979->$1 = _M0L8filenameS1328;
  _M0L14handle__resultS1323 = (struct _M0TWssbEu*)_closure_3979;
  _M0L17error__to__stringS1332
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN17error__to__stringS1332$closure.data;
  moonbit_incref(_M0L12async__testsS1353);
  moonbit_incref(_M0L17error__to__stringS1332);
  moonbit_incref(_M0L8filenameS1328);
  moonbit_incref(_M0L14handle__resultS1323);
  #line 558 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _tmp_3981
  = _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__test(_M0L12async__testsS1353, _M0L8filenameS1328, _M0L5indexS1331, _M0L14handle__resultS1323, _M0L17error__to__stringS1332);
  if (_tmp_3981.tag) {
    int32_t const _M0L5_2aokS3420 = _tmp_3981.data.ok;
    _handle__error__result_3982 = _M0L5_2aokS3420;
  } else {
    void* const _M0L6_2aerrS3421 = _tmp_3981.data.err;
    moonbit_decref(_M0L12async__testsS1353);
    moonbit_decref(_M0L17error__to__stringS1332);
    moonbit_decref(_M0L8filenameS1328);
    _M0L11_2atry__errS1347 = _M0L6_2aerrS3421;
    goto join_1346;
  }
  if (_handle__error__result_3982) {
    moonbit_decref(_M0L12async__testsS1353);
    moonbit_decref(_M0L17error__to__stringS1332);
    moonbit_decref(_M0L8filenameS1328);
    _M0L6_2atmpS3411 = 1;
  } else {
    struct moonbit_result_0 _tmp_3983;
    int32_t _handle__error__result_3984;
    moonbit_incref(_M0L12async__testsS1353);
    moonbit_incref(_M0L17error__to__stringS1332);
    moonbit_incref(_M0L8filenameS1328);
    moonbit_incref(_M0L14handle__resultS1323);
    #line 561 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    _tmp_3983
    = _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1353, _M0L8filenameS1328, _M0L5indexS1331, _M0L14handle__resultS1323, _M0L17error__to__stringS1332);
    if (_tmp_3983.tag) {
      int32_t const _M0L5_2aokS3418 = _tmp_3983.data.ok;
      _handle__error__result_3984 = _M0L5_2aokS3418;
    } else {
      void* const _M0L6_2aerrS3419 = _tmp_3983.data.err;
      moonbit_decref(_M0L12async__testsS1353);
      moonbit_decref(_M0L17error__to__stringS1332);
      moonbit_decref(_M0L8filenameS1328);
      _M0L11_2atry__errS1347 = _M0L6_2aerrS3419;
      goto join_1346;
    }
    if (_handle__error__result_3984) {
      moonbit_decref(_M0L12async__testsS1353);
      moonbit_decref(_M0L17error__to__stringS1332);
      moonbit_decref(_M0L8filenameS1328);
      _M0L6_2atmpS3411 = 1;
    } else {
      struct moonbit_result_0 _tmp_3985;
      int32_t _handle__error__result_3986;
      moonbit_incref(_M0L12async__testsS1353);
      moonbit_incref(_M0L17error__to__stringS1332);
      moonbit_incref(_M0L8filenameS1328);
      moonbit_incref(_M0L14handle__resultS1323);
      #line 564 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _tmp_3985
      = _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1353, _M0L8filenameS1328, _M0L5indexS1331, _M0L14handle__resultS1323, _M0L17error__to__stringS1332);
      if (_tmp_3985.tag) {
        int32_t const _M0L5_2aokS3416 = _tmp_3985.data.ok;
        _handle__error__result_3986 = _M0L5_2aokS3416;
      } else {
        void* const _M0L6_2aerrS3417 = _tmp_3985.data.err;
        moonbit_decref(_M0L12async__testsS1353);
        moonbit_decref(_M0L17error__to__stringS1332);
        moonbit_decref(_M0L8filenameS1328);
        _M0L11_2atry__errS1347 = _M0L6_2aerrS3417;
        goto join_1346;
      }
      if (_handle__error__result_3986) {
        moonbit_decref(_M0L12async__testsS1353);
        moonbit_decref(_M0L17error__to__stringS1332);
        moonbit_decref(_M0L8filenameS1328);
        _M0L6_2atmpS3411 = 1;
      } else {
        struct moonbit_result_0 _tmp_3987;
        int32_t _handle__error__result_3988;
        moonbit_incref(_M0L12async__testsS1353);
        moonbit_incref(_M0L17error__to__stringS1332);
        moonbit_incref(_M0L8filenameS1328);
        moonbit_incref(_M0L14handle__resultS1323);
        #line 567 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        _tmp_3987
        = _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1353, _M0L8filenameS1328, _M0L5indexS1331, _M0L14handle__resultS1323, _M0L17error__to__stringS1332);
        if (_tmp_3987.tag) {
          int32_t const _M0L5_2aokS3414 = _tmp_3987.data.ok;
          _handle__error__result_3988 = _M0L5_2aokS3414;
        } else {
          void* const _M0L6_2aerrS3415 = _tmp_3987.data.err;
          moonbit_decref(_M0L12async__testsS1353);
          moonbit_decref(_M0L17error__to__stringS1332);
          moonbit_decref(_M0L8filenameS1328);
          _M0L11_2atry__errS1347 = _M0L6_2aerrS3415;
          goto join_1346;
        }
        if (_handle__error__result_3988) {
          moonbit_decref(_M0L12async__testsS1353);
          moonbit_decref(_M0L17error__to__stringS1332);
          moonbit_decref(_M0L8filenameS1328);
          _M0L6_2atmpS3411 = 1;
        } else {
          struct moonbit_result_0 _tmp_3989;
          moonbit_incref(_M0L14handle__resultS1323);
          #line 570 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
          _tmp_3989
          = _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1353, _M0L8filenameS1328, _M0L5indexS1331, _M0L14handle__resultS1323, _M0L17error__to__stringS1332);
          if (_tmp_3989.tag) {
            int32_t const _M0L5_2aokS3412 = _tmp_3989.data.ok;
            _M0L6_2atmpS3411 = _M0L5_2aokS3412;
          } else {
            void* const _M0L6_2aerrS3413 = _tmp_3989.data.err;
            _M0L11_2atry__errS1347 = _M0L6_2aerrS3413;
            goto join_1346;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3411) {
    void* _M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3422 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3422)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3422)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1347
    = _M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3422;
    goto join_1346;
  } else {
    moonbit_decref(_M0L14handle__resultS1323);
  }
  goto joinlet_3980;
  join_1346:;
  _M0L3errS1348 = _M0L11_2atry__errS1347;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1351
  = (struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1348;
  _M0L8_2afieldS3424 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1351->$0;
  _M0L6_2acntS3795
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1351)->rc;
  if (_M0L6_2acntS3795 > 1) {
    int32_t _M0L11_2anew__cntS3796 = _M0L6_2acntS3795 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1351)->rc
    = _M0L11_2anew__cntS3796;
    moonbit_incref(_M0L8_2afieldS3424);
  } else if (_M0L6_2acntS3795 == 1) {
    #line 577 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1351);
  }
  _M0L7_2anameS1352 = _M0L8_2afieldS3424;
  _M0L4nameS1350 = _M0L7_2anameS1352;
  goto join_1349;
  goto joinlet_3990;
  join_1349:;
  #line 578 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN14handle__resultS1323(_M0L14handle__resultS1323, _M0L4nameS1350, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3990:;
  joinlet_3980:;
  return 0;
}

moonbit_string_t _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN17error__to__stringS1332(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3410,
  void* _M0L3errS1333
) {
  void* _M0L1eS1335;
  moonbit_string_t _M0L1eS1337;
  #line 547 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3410);
  switch (Moonbit_object_tag(_M0L3errS1333)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1338 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1333;
      moonbit_string_t _M0L8_2afieldS3425 = _M0L10_2aFailureS1338->$0;
      int32_t _M0L6_2acntS3797 =
        Moonbit_object_header(_M0L10_2aFailureS1338)->rc;
      moonbit_string_t _M0L4_2aeS1339;
      if (_M0L6_2acntS3797 > 1) {
        int32_t _M0L11_2anew__cntS3798 = _M0L6_2acntS3797 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1338)->rc
        = _M0L11_2anew__cntS3798;
        moonbit_incref(_M0L8_2afieldS3425);
      } else if (_M0L6_2acntS3797 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1338);
      }
      _M0L4_2aeS1339 = _M0L8_2afieldS3425;
      _M0L1eS1337 = _M0L4_2aeS1339;
      goto join_1336;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1340 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1333;
      moonbit_string_t _M0L8_2afieldS3426 = _M0L15_2aInspectErrorS1340->$0;
      int32_t _M0L6_2acntS3799 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1340)->rc;
      moonbit_string_t _M0L4_2aeS1341;
      if (_M0L6_2acntS3799 > 1) {
        int32_t _M0L11_2anew__cntS3800 = _M0L6_2acntS3799 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1340)->rc
        = _M0L11_2anew__cntS3800;
        moonbit_incref(_M0L8_2afieldS3426);
      } else if (_M0L6_2acntS3799 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1340);
      }
      _M0L4_2aeS1341 = _M0L8_2afieldS3426;
      _M0L1eS1337 = _M0L4_2aeS1341;
      goto join_1336;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1342 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1333;
      moonbit_string_t _M0L8_2afieldS3427 = _M0L16_2aSnapshotErrorS1342->$0;
      int32_t _M0L6_2acntS3801 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1342)->rc;
      moonbit_string_t _M0L4_2aeS1343;
      if (_M0L6_2acntS3801 > 1) {
        int32_t _M0L11_2anew__cntS3802 = _M0L6_2acntS3801 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1342)->rc
        = _M0L11_2anew__cntS3802;
        moonbit_incref(_M0L8_2afieldS3427);
      } else if (_M0L6_2acntS3801 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1342);
      }
      _M0L4_2aeS1343 = _M0L8_2afieldS3427;
      _M0L1eS1337 = _M0L4_2aeS1343;
      goto join_1336;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error92mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1344 =
        (struct _M0DTPC15error5Error92mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1333;
      moonbit_string_t _M0L8_2afieldS3428 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1344->$0;
      int32_t _M0L6_2acntS3803 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1344)->rc;
      moonbit_string_t _M0L4_2aeS1345;
      if (_M0L6_2acntS3803 > 1) {
        int32_t _M0L11_2anew__cntS3804 = _M0L6_2acntS3803 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1344)->rc
        = _M0L11_2anew__cntS3804;
        moonbit_incref(_M0L8_2afieldS3428);
      } else if (_M0L6_2acntS3803 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1344);
      }
      _M0L4_2aeS1345 = _M0L8_2afieldS3428;
      _M0L1eS1337 = _M0L4_2aeS1345;
      goto join_1336;
      break;
    }
    default: {
      _M0L1eS1335 = _M0L3errS1333;
      goto join_1334;
      break;
    }
  }
  join_1336:;
  return _M0L1eS1337;
  join_1334:;
  #line 553 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1335);
}

int32_t _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__executeN14handle__resultS1323(
  struct _M0TWssbEu* _M0L6_2aenvS3396,
  moonbit_string_t _M0L8testnameS1324,
  moonbit_string_t _M0L7messageS1325,
  int32_t _M0L7skippedS1326
) {
  struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323* _M0L14_2acasted__envS3397;
  moonbit_string_t _M0L8filenameS1328;
  int32_t _M0L5indexS1331;
  int32_t _M0L6_2acntS3805;
  int32_t _if__result_3993;
  moonbit_string_t _M0L10file__nameS1327;
  moonbit_string_t _M0L10test__nameS1329;
  moonbit_string_t _M0L7messageS1330;
  moonbit_string_t _M0L6_2atmpS3409;
  moonbit_string_t _M0L6_2atmpS3408;
  moonbit_string_t _M0L6_2atmpS3406;
  moonbit_string_t _M0L6_2atmpS3407;
  moonbit_string_t _M0L6_2atmpS3405;
  moonbit_string_t _M0L6_2atmpS3403;
  moonbit_string_t _M0L6_2atmpS3404;
  moonbit_string_t _M0L6_2atmpS3402;
  moonbit_string_t _M0L6_2atmpS3400;
  moonbit_string_t _M0L6_2atmpS3401;
  moonbit_string_t _M0L6_2atmpS3399;
  moonbit_string_t _M0L6_2atmpS3398;
  #line 531 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS3397
  = (struct _M0R96_24mulpjs_2fmulp_2fstream_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1323*)_M0L6_2aenvS3396;
  _M0L8filenameS1328 = _M0L14_2acasted__envS3397->$1;
  _M0L5indexS1331 = _M0L14_2acasted__envS3397->$0;
  _M0L6_2acntS3805 = Moonbit_object_header(_M0L14_2acasted__envS3397)->rc;
  if (_M0L6_2acntS3805 > 1) {
    int32_t _M0L11_2anew__cntS3806 = _M0L6_2acntS3805 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3397)->rc
    = _M0L11_2anew__cntS3806;
    moonbit_incref(_M0L8filenameS1328);
  } else if (_M0L6_2acntS3805 == 1) {
    #line 531 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3397);
  }
  if (!_M0L7skippedS1326) {
    _if__result_3993 = 1;
  } else {
    _if__result_3993 = 0;
  }
  if (_if__result_3993) {
    
  }
  #line 537 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L10file__nameS1327
  = _M0MPC16string6String14escape_2einner(_M0L8filenameS1328, 1);
  #line 538 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L10test__nameS1329
  = _M0MPC16string6String14escape_2einner(_M0L8testnameS1324, 1);
  #line 539 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L7messageS1330
  = _M0MPC16string6String14escape_2einner(_M0L7messageS1325, 1);
  #line 540 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 542 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3409
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1327);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3408
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3409);
  moonbit_decref(_M0L6_2atmpS3409);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3406
  = moonbit_add_string(_M0L6_2atmpS3408, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3408);
  #line 542 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3407 = _M0IPC13int3IntPB4Show10to__string(_M0L5indexS1331);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3405 = moonbit_add_string(_M0L6_2atmpS3406, _M0L6_2atmpS3407);
  moonbit_decref(_M0L6_2atmpS3407);
  moonbit_decref(_M0L6_2atmpS3406);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3403
  = moonbit_add_string(_M0L6_2atmpS3405, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3405);
  #line 542 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3404
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1329);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3402 = moonbit_add_string(_M0L6_2atmpS3403, _M0L6_2atmpS3404);
  moonbit_decref(_M0L6_2atmpS3404);
  moonbit_decref(_M0L6_2atmpS3403);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3400
  = moonbit_add_string(_M0L6_2atmpS3402, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3402);
  #line 542 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3401
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1330);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3399 = moonbit_add_string(_M0L6_2atmpS3400, _M0L6_2atmpS3401);
  moonbit_decref(_M0L6_2atmpS3401);
  moonbit_decref(_M0L6_2atmpS3400);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3398
  = moonbit_add_string(_M0L6_2atmpS3399, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3399);
  #line 541 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3398);
  #line 544 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1322,
  moonbit_string_t _M0L8filenameS1319,
  int32_t _M0L5indexS1313,
  struct _M0TWssbEu* _M0L14handle__resultS1309,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1311
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1289;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1318;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1291;
  moonbit_string_t* _M0L5attrsS1292;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1312;
  moonbit_string_t _M0L4nameS1295;
  moonbit_string_t _M0L4nameS1293;
  int32_t _M0L6_2atmpS3395;
  struct _M0TWEOs* _M0L5_2aitS1297;
  struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__* _closure_4002;
  struct _M0TWEu* _M0L6_2atmpS3386;
  struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__* _closure_4003;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3387;
  struct moonbit_result_0 _result_4004;
  #line 405 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1322);
  moonbit_incref(_M0FP36mulpjs4mulp6stream48moonbit__test__driver__internal__no__args__tests);
  #line 412 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1318
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP36mulpjs4mulp6stream48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1319);
  if (_M0L7_2abindS1318 == 0) {
    struct moonbit_result_0 _result_3995;
    if (_M0L7_2abindS1318) {
      moonbit_decref(_M0L7_2abindS1318);
    }
    moonbit_decref(_M0L17error__to__stringS1311);
    moonbit_decref(_M0L14handle__resultS1309);
    _result_3995.tag = 1;
    _result_3995.data.ok = 0;
    return _result_3995;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1320 =
      _M0L7_2abindS1318;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1321 =
      _M0L7_2aSomeS1320;
    _M0L10index__mapS1289 = _M0L13_2aindex__mapS1321;
    goto join_1288;
  }
  join_1288:;
  #line 414 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1312
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1289, _M0L5indexS1313);
  if (_M0L7_2abindS1312 == 0) {
    struct moonbit_result_0 _result_3997;
    if (_M0L7_2abindS1312) {
      moonbit_decref(_M0L7_2abindS1312);
    }
    moonbit_decref(_M0L17error__to__stringS1311);
    moonbit_decref(_M0L14handle__resultS1309);
    _result_3997.tag = 1;
    _result_3997.data.ok = 0;
    return _result_3997;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1314 =
      _M0L7_2abindS1312;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1315 = _M0L7_2aSomeS1314;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1316 = _M0L4_2axS1315->$0;
    moonbit_string_t* _M0L8_2afieldS3431 = _M0L4_2axS1315->$1;
    int32_t _M0L6_2acntS3807 = Moonbit_object_header(_M0L4_2axS1315)->rc;
    moonbit_string_t* _M0L8_2aattrsS1317;
    if (_M0L6_2acntS3807 > 1) {
      int32_t _M0L11_2anew__cntS3808 = _M0L6_2acntS3807 - 1;
      Moonbit_object_header(_M0L4_2axS1315)->rc = _M0L11_2anew__cntS3808;
      moonbit_incref(_M0L8_2afieldS3431);
      moonbit_incref(_M0L4_2afS1316);
    } else if (_M0L6_2acntS3807 == 1) {
      #line 412 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      moonbit_free(_M0L4_2axS1315);
    }
    _M0L8_2aattrsS1317 = _M0L8_2afieldS3431;
    _M0L1fS1291 = _M0L4_2afS1316;
    _M0L5attrsS1292 = _M0L8_2aattrsS1317;
    goto join_1290;
  }
  join_1290:;
  _M0L6_2atmpS3395 = Moonbit_array_length(_M0L5attrsS1292);
  if (_M0L6_2atmpS3395 >= 1) {
    moonbit_string_t _M0L7_2anameS1296 = (moonbit_string_t)_M0L5attrsS1292[0];
    moonbit_incref(_M0L7_2anameS1296);
    _M0L4nameS1295 = _M0L7_2anameS1296;
    goto join_1294;
  } else {
    _M0L4nameS1293 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3998;
  join_1294:;
  _M0L4nameS1293 = _M0L4nameS1295;
  joinlet_3998:;
  #line 415 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L5_2aitS1297 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1292);
  while (1) {
    moonbit_string_t _M0L4attrS1299;
    moonbit_string_t _M0L7_2abindS1306;
    int32_t _M0L6_2atmpS3379;
    int64_t _M0L6_2atmpS3378;
    moonbit_incref(_M0L5_2aitS1297);
    #line 417 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    _M0L7_2abindS1306 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1297);
    if (_M0L7_2abindS1306 == 0) {
      if (_M0L7_2abindS1306) {
        moonbit_decref(_M0L7_2abindS1306);
      }
      moonbit_decref(_M0L5_2aitS1297);
    } else {
      moonbit_string_t _M0L7_2aSomeS1307 = _M0L7_2abindS1306;
      moonbit_string_t _M0L7_2aattrS1308 = _M0L7_2aSomeS1307;
      _M0L4attrS1299 = _M0L7_2aattrS1308;
      goto join_1298;
    }
    goto joinlet_4000;
    join_1298:;
    _M0L6_2atmpS3379 = Moonbit_array_length(_M0L4attrS1299);
    _M0L6_2atmpS3378 = (int64_t)_M0L6_2atmpS3379;
    moonbit_incref(_M0L4attrS1299);
    #line 418 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1299, 5, 0, _M0L6_2atmpS3378)
    ) {
      int32_t _M0L6_2atmpS3385 = _M0L4attrS1299[0];
      int32_t _M0L4_2axS1300 = _M0L6_2atmpS3385;
      if (_M0L4_2axS1300 == 112) {
        int32_t _M0L6_2atmpS3384 = _M0L4attrS1299[1];
        int32_t _M0L4_2axS1301 = _M0L6_2atmpS3384;
        if (_M0L4_2axS1301 == 97) {
          int32_t _M0L6_2atmpS3383 = _M0L4attrS1299[2];
          int32_t _M0L4_2axS1302 = _M0L6_2atmpS3383;
          if (_M0L4_2axS1302 == 110) {
            int32_t _M0L6_2atmpS3382 = _M0L4attrS1299[3];
            int32_t _M0L4_2axS1303 = _M0L6_2atmpS3382;
            if (_M0L4_2axS1303 == 105) {
              int32_t _M0L6_2atmpS3381 = _M0L4attrS1299[4];
              int32_t _M0L4_2axS1304;
              moonbit_decref(_M0L4attrS1299);
              _M0L4_2axS1304 = _M0L6_2atmpS3381;
              if (_M0L4_2axS1304 == 99) {
                void* _M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3380;
                struct moonbit_result_0 _result_4001;
                moonbit_decref(_M0L17error__to__stringS1311);
                moonbit_decref(_M0L14handle__resultS1309);
                moonbit_decref(_M0L5_2aitS1297);
                moonbit_decref(_M0L1fS1291);
                _M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3380
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3380)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3380)->$0
                = _M0L4nameS1293;
                _result_4001.tag = 0;
                _result_4001.data.err
                = _M0L94mulpjs_2fmulp_2fstream_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3380;
                return _result_4001;
              }
            } else {
              moonbit_decref(_M0L4attrS1299);
            }
          } else {
            moonbit_decref(_M0L4attrS1299);
          }
        } else {
          moonbit_decref(_M0L4attrS1299);
        }
      } else {
        moonbit_decref(_M0L4attrS1299);
      }
    } else {
      moonbit_decref(_M0L4attrS1299);
    }
    continue;
    joinlet_4000:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1309);
  moonbit_incref(_M0L4nameS1293);
  _closure_4002
  = (struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__*)moonbit_malloc(sizeof(struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__));
  Moonbit_object_header(_closure_4002)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__, $0) >> 2, 2, 0);
  _closure_4002->code
  = &_M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testC3392l425;
  _closure_4002->$0 = _M0L14handle__resultS1309;
  _closure_4002->$1 = _M0L4nameS1293;
  _M0L6_2atmpS3386 = (struct _M0TWEu*)_closure_4002;
  _closure_4003
  = (struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__*)moonbit_malloc(sizeof(struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__));
  Moonbit_object_header(_closure_4003)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__, $0) >> 2, 3, 0);
  _closure_4003->code
  = &_M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testC3388l426;
  _closure_4003->$0 = _M0L17error__to__stringS1311;
  _closure_4003->$1 = _M0L14handle__resultS1309;
  _closure_4003->$2 = _M0L4nameS1293;
  _M0L6_2atmpS3387 = (struct _M0TWRPC15error5ErrorEu*)_closure_4003;
  #line 423 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0FP36mulpjs4mulp6stream45moonbit__test__driver__internal__catch__error(_M0L1fS1291, _M0L6_2atmpS3386, _M0L6_2atmpS3387);
  _result_4004.tag = 1;
  _result_4004.data.ok = 1;
  return _result_4004;
}

int32_t _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testC3392l425(
  struct _M0TWEu* _M0L6_2aenvS3393
) {
  struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__* _M0L14_2acasted__envS3394;
  moonbit_string_t _M0L4nameS1293;
  struct _M0TWssbEu* _M0L8_2afieldS3433;
  int32_t _M0L6_2acntS3809;
  struct _M0TWssbEu* _M0L14handle__resultS1309;
  #line 425 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS3394
  = (struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3392__l425__*)_M0L6_2aenvS3393;
  _M0L4nameS1293 = _M0L14_2acasted__envS3394->$1;
  _M0L8_2afieldS3433 = _M0L14_2acasted__envS3394->$0;
  _M0L6_2acntS3809 = Moonbit_object_header(_M0L14_2acasted__envS3394)->rc;
  if (_M0L6_2acntS3809 > 1) {
    int32_t _M0L11_2anew__cntS3810 = _M0L6_2acntS3809 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3394)->rc
    = _M0L11_2anew__cntS3810;
    moonbit_incref(_M0L4nameS1293);
    moonbit_incref(_M0L8_2afieldS3433);
  } else if (_M0L6_2acntS3809 == 1) {
    #line 425 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3394);
  }
  _M0L14handle__resultS1309 = _M0L8_2afieldS3433;
  #line 425 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS1309->code(_M0L14handle__resultS1309, _M0L4nameS1293, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP36mulpjs4mulp6stream41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testC3388l426(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3389,
  void* _M0L3errS1310
) {
  struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__* _M0L14_2acasted__envS3390;
  moonbit_string_t _M0L4nameS1293;
  struct _M0TWssbEu* _M0L14handle__resultS1309;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3435;
  int32_t _M0L6_2acntS3811;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1311;
  moonbit_string_t _M0L6_2atmpS3391;
  #line 426 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS3390
  = (struct _M0R161_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fstream_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3388__l426__*)_M0L6_2aenvS3389;
  _M0L4nameS1293 = _M0L14_2acasted__envS3390->$2;
  _M0L14handle__resultS1309 = _M0L14_2acasted__envS3390->$1;
  _M0L8_2afieldS3435 = _M0L14_2acasted__envS3390->$0;
  _M0L6_2acntS3811 = Moonbit_object_header(_M0L14_2acasted__envS3390)->rc;
  if (_M0L6_2acntS3811 > 1) {
    int32_t _M0L11_2anew__cntS3812 = _M0L6_2acntS3811 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3390)->rc
    = _M0L11_2anew__cntS3812;
    moonbit_incref(_M0L4nameS1293);
    moonbit_incref(_M0L14handle__resultS1309);
    moonbit_incref(_M0L8_2afieldS3435);
  } else if (_M0L6_2acntS3811 == 1) {
    #line 426 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3390);
  }
  _M0L17error__to__stringS1311 = _M0L8_2afieldS3435;
  #line 426 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3391
  = _M0L17error__to__stringS1311->code(_M0L17error__to__stringS1311, _M0L3errS1310);
  #line 426 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS1309->code(_M0L14handle__resultS1309, _M0L4nameS1293, _M0L6_2atmpS3391, 0);
  return 0;
}

int32_t _M0FP36mulpjs4mulp6stream45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1284,
  struct _M0TWEu* _M0L6on__okS1285,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1282
) {
  void* _M0L11_2atry__errS1280;
  struct moonbit_result_0 _tmp_4006;
  void* _M0L3errS1281;
  #line 375 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  #line 382 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _tmp_4006 = _M0L1fS1284->code(_M0L1fS1284);
  if (_tmp_4006.tag) {
    int32_t const _M0L5_2aokS3376 = _tmp_4006.data.ok;
    moonbit_decref(_M0L7on__errS1282);
  } else {
    void* const _M0L6_2aerrS3377 = _tmp_4006.data.err;
    moonbit_decref(_M0L6on__okS1285);
    _M0L11_2atry__errS1280 = _M0L6_2aerrS3377;
    goto join_1279;
  }
  #line 382 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6on__okS1285->code(_M0L6on__okS1285);
  goto joinlet_4005;
  join_1279:;
  _M0L3errS1281 = _M0L11_2atry__errS1280;
  #line 383 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L7on__errS1282->code(_M0L7on__errS1282, _M0L3errS1281);
  joinlet_4005:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1239;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1252;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1257;
  struct _M0TUsiE** _M0L6_2atmpS3375;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1264;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1265;
  moonbit_string_t _M0L6_2atmpS3374;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1266;
  int32_t _M0L7_2abindS1267;
  int32_t _M0L2__S1268;
  #line 193 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1239 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1252
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1257 = 0;
  _M0L6_2atmpS3375 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1264
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1264)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1264->$0 = _M0L6_2atmpS3375;
  _M0L16file__and__indexS1264->$1 = 0;
  #line 282 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L9cli__argsS1265
  = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1252(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1252);
  #line 284 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3374 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1265, 1);
  #line 283 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L10test__argsS1266
  = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1257(_M0L51moonbit__test__driver__internal__split__mbt__stringS1257, _M0L6_2atmpS3374, 47);
  _M0L7_2abindS1267 = _M0L10test__argsS1266->$1;
  _M0L2__S1268 = 0;
  while (1) {
    if (_M0L2__S1268 < _M0L7_2abindS1267) {
      moonbit_string_t* _M0L3bufS3373 = _M0L10test__argsS1266->$0;
      moonbit_string_t _M0L3argS1269 =
        (moonbit_string_t)_M0L3bufS3373[_M0L2__S1268];
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1270;
      moonbit_string_t _M0L4fileS1271;
      moonbit_string_t _M0L5rangeS1272;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1273;
      moonbit_string_t _M0L6_2atmpS3371;
      int32_t _M0L5startS1274;
      moonbit_string_t _M0L6_2atmpS3370;
      int32_t _M0L3endS1275;
      int32_t _M0L1iS1276;
      int32_t _M0L6_2atmpS3372;
      moonbit_incref(_M0L3argS1269);
      #line 288 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L16file__and__rangeS1270
      = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1257(_M0L51moonbit__test__driver__internal__split__mbt__stringS1257, _M0L3argS1269, 58);
      moonbit_incref(_M0L16file__and__rangeS1270);
      #line 289 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L4fileS1271
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1270, 0);
      #line 290 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L5rangeS1272
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1270, 1);
      #line 291 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L15start__and__endS1273
      = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1257(_M0L51moonbit__test__driver__internal__split__mbt__stringS1257, _M0L5rangeS1272, 45);
      moonbit_incref(_M0L15start__and__endS1273);
      #line 294 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS3371
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1273, 0);
      #line 294 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L5startS1274
      = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1239(_M0L45moonbit__test__driver__internal__parse__int__S1239, _M0L6_2atmpS3371);
      #line 295 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS3370
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1273, 1);
      #line 295 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L3endS1275
      = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1239(_M0L45moonbit__test__driver__internal__parse__int__S1239, _M0L6_2atmpS3370);
      _M0L1iS1276 = _M0L5startS1274;
      while (1) {
        if (_M0L1iS1276 < _M0L3endS1275) {
          struct _M0TUsiE* _M0L8_2atupleS3368;
          int32_t _M0L6_2atmpS3369;
          moonbit_incref(_M0L4fileS1271);
          _M0L8_2atupleS3368
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3368)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3368->$0 = _M0L4fileS1271;
          _M0L8_2atupleS3368->$1 = _M0L1iS1276;
          moonbit_incref(_M0L16file__and__indexS1264);
          #line 297 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1264, _M0L8_2atupleS3368);
          _M0L6_2atmpS3369 = _M0L1iS1276 + 1;
          _M0L1iS1276 = _M0L6_2atmpS3369;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1271);
        }
        break;
      }
      _M0L6_2atmpS3372 = _M0L2__S1268 + 1;
      _M0L2__S1268 = _M0L6_2atmpS3372;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1266);
    }
    break;
  }
  return _M0L16file__and__indexS1264;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1257(
  int32_t _M0L6_2aenvS3349,
  moonbit_string_t _M0L1sS1258,
  int32_t _M0L3sepS1259
) {
  moonbit_string_t* _M0L6_2atmpS3367;
  struct _M0TPB5ArrayGsE* _M0L3resS1260;
  struct _M0TPB8MutLocalGiE* _M0L1iS1261;
  struct _M0TPB8MutLocalGiE* _M0L5startS1262;
  int32_t _M0L3valS3362;
  int32_t _M0L6_2atmpS3363;
  #line 261 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3367 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1260
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1260)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1260->$0 = _M0L6_2atmpS3367;
  _M0L3resS1260->$1 = 0;
  _M0L1iS1261
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1261)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS1261->$0 = 0;
  _M0L5startS1262
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS1262)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5startS1262->$0 = 0;
  while (1) {
    int32_t _M0L3valS3350 = _M0L1iS1261->$0;
    int32_t _M0L6_2atmpS3351 = Moonbit_array_length(_M0L1sS1258);
    if (_M0L3valS3350 < _M0L6_2atmpS3351) {
      int32_t _M0L3valS3354 = _M0L1iS1261->$0;
      int32_t _M0L6_2atmpS3353;
      int32_t _M0L6_2atmpS3352;
      int32_t _M0L3valS3361;
      int32_t _M0L6_2atmpS3360;
      if (
        _M0L3valS3354 < 0
        || _M0L3valS3354 >= Moonbit_array_length(_M0L1sS1258)
      ) {
        #line 269 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3353 = _M0L1sS1258[_M0L3valS3354];
      _M0L6_2atmpS3352 = _M0L6_2atmpS3353;
      if (_M0L6_2atmpS3352 == _M0L3sepS1259) {
        int32_t _M0L3valS3356 = _M0L5startS1262->$0;
        int32_t _M0L3valS3357 = _M0L1iS1261->$0;
        moonbit_string_t _M0L6_2atmpS3355;
        int32_t _M0L3valS3359;
        int32_t _M0L6_2atmpS3358;
        moonbit_incref(_M0L1sS1258);
        #line 270 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        _M0L6_2atmpS3355
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1258, _M0L3valS3356, _M0L3valS3357);
        moonbit_incref(_M0L3resS1260);
        #line 270 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1260, _M0L6_2atmpS3355);
        _M0L3valS3359 = _M0L1iS1261->$0;
        _M0L6_2atmpS3358 = _M0L3valS3359 + 1;
        _M0L5startS1262->$0 = _M0L6_2atmpS3358;
      }
      _M0L3valS3361 = _M0L1iS1261->$0;
      _M0L6_2atmpS3360 = _M0L3valS3361 + 1;
      _M0L1iS1261->$0 = _M0L6_2atmpS3360;
      continue;
    } else {
      moonbit_decref(_M0L1iS1261);
    }
    break;
  }
  _M0L3valS3362 = _M0L5startS1262->$0;
  _M0L6_2atmpS3363 = Moonbit_array_length(_M0L1sS1258);
  if (_M0L3valS3362 < _M0L6_2atmpS3363) {
    int32_t _M0L3valS3365 = _M0L5startS1262->$0;
    int32_t _M0L6_2atmpS3366;
    moonbit_string_t _M0L6_2atmpS3364;
    moonbit_decref(_M0L5startS1262);
    _M0L6_2atmpS3366 = Moonbit_array_length(_M0L1sS1258);
    #line 276 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    _M0L6_2atmpS3364
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1258, _M0L3valS3365, _M0L6_2atmpS3366);
    moonbit_incref(_M0L3resS1260);
    #line 276 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1260, _M0L6_2atmpS3364);
  } else {
    moonbit_decref(_M0L5startS1262);
    moonbit_decref(_M0L1sS1258);
  }
  return _M0L3resS1260;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1252(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245
) {
  moonbit_bytes_t* _M0L3tmpS1253;
  int32_t _M0L6_2atmpS3348;
  struct _M0TPB5ArrayGsE* _M0L3resS1254;
  int32_t _M0L1iS1255;
  #line 250 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  #line 253 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L3tmpS1253
  = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3348 = Moonbit_array_length(_M0L3tmpS1253);
  #line 254 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1254 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3348);
  _M0L1iS1255 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3344 = Moonbit_array_length(_M0L3tmpS1253);
    if (_M0L1iS1255 < _M0L6_2atmpS3344) {
      moonbit_bytes_t _M0L6_2atmpS3346;
      moonbit_string_t _M0L6_2atmpS3345;
      int32_t _M0L6_2atmpS3347;
      if (
        _M0L1iS1255 < 0 || _M0L1iS1255 >= Moonbit_array_length(_M0L3tmpS1253)
      ) {
        #line 256 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3346 = (moonbit_bytes_t)_M0L3tmpS1253[_M0L1iS1255];
      moonbit_incref(_M0L6_2atmpS3346);
      #line 256 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS3345
      = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245, _M0L6_2atmpS3346);
      moonbit_incref(_M0L3resS1254);
      #line 256 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1254, _M0L6_2atmpS3345);
      _M0L6_2atmpS3347 = _M0L1iS1255 + 1;
      _M0L1iS1255 = _M0L6_2atmpS3347;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1253);
    }
    break;
  }
  return _M0L3resS1254;
}

moonbit_string_t _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1245(
  int32_t _M0L6_2aenvS3258,
  moonbit_bytes_t _M0L5bytesS1246
) {
  struct _M0TPB13StringBuilder* _M0L3resS1247;
  int32_t _M0L3lenS1248;
  struct _M0TPB8MutLocalGiE* _M0L1iS1249;
  #line 206 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  #line 209 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1247 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1248 = Moonbit_array_length(_M0L5bytesS1246);
  _M0L1iS1249
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1249)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS1249->$0 = 0;
  while (1) {
    int32_t _M0L3valS3259 = _M0L1iS1249->$0;
    if (_M0L3valS3259 < _M0L3lenS1248) {
      int32_t _M0L3valS3343 = _M0L1iS1249->$0;
      int32_t _M0L6_2atmpS3342;
      int32_t _M0L6_2atmpS3341;
      struct _M0TPB8MutLocalGiE* _M0L1cS1250;
      int32_t _M0L3valS3260;
      if (
        _M0L3valS3343 < 0
        || _M0L3valS3343 >= Moonbit_array_length(_M0L5bytesS1246)
      ) {
        #line 213 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3342 = _M0L5bytesS1246[_M0L3valS3343];
      _M0L6_2atmpS3341 = (int32_t)_M0L6_2atmpS3342;
      _M0L1cS1250
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L1cS1250)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
      _M0L1cS1250->$0 = _M0L6_2atmpS3341;
      _M0L3valS3260 = _M0L1cS1250->$0;
      if (_M0L3valS3260 < 128) {
        int32_t _M0L3valS3262 = _M0L1cS1250->$0;
        int32_t _M0L6_2atmpS3261;
        int32_t _M0L3valS3264;
        int32_t _M0L6_2atmpS3263;
        moonbit_decref(_M0L1cS1250);
        _M0L6_2atmpS3261 = _M0L3valS3262;
        moonbit_incref(_M0L3resS1247);
        #line 215 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1247, _M0L6_2atmpS3261);
        _M0L3valS3264 = _M0L1iS1249->$0;
        _M0L6_2atmpS3263 = _M0L3valS3264 + 1;
        _M0L1iS1249->$0 = _M0L6_2atmpS3263;
      } else {
        int32_t _M0L3valS3265 = _M0L1cS1250->$0;
        if (_M0L3valS3265 < 224) {
          int32_t _M0L3valS3267 = _M0L1iS1249->$0;
          int32_t _M0L6_2atmpS3266 = _M0L3valS3267 + 1;
          int32_t _M0L3valS3276;
          int32_t _M0L6_2atmpS3275;
          int32_t _M0L6_2atmpS3269;
          int32_t _M0L3valS3274;
          int32_t _M0L6_2atmpS3273;
          int32_t _M0L6_2atmpS3272;
          int32_t _M0L6_2atmpS3271;
          int32_t _M0L6_2atmpS3270;
          int32_t _M0L6_2atmpS3268;
          int32_t _M0L3valS3278;
          int32_t _M0L6_2atmpS3277;
          int32_t _M0L3valS3280;
          int32_t _M0L6_2atmpS3279;
          if (_M0L6_2atmpS3266 >= _M0L3lenS1248) {
            moonbit_decref(_M0L1cS1250);
            moonbit_decref(_M0L1iS1249);
            moonbit_decref(_M0L5bytesS1246);
            break;
          }
          _M0L3valS3276 = _M0L1cS1250->$0;
          _M0L6_2atmpS3275 = _M0L3valS3276 & 31;
          _M0L6_2atmpS3269 = _M0L6_2atmpS3275 << 6;
          _M0L3valS3274 = _M0L1iS1249->$0;
          _M0L6_2atmpS3273 = _M0L3valS3274 + 1;
          if (
            _M0L6_2atmpS3273 < 0
            || _M0L6_2atmpS3273 >= Moonbit_array_length(_M0L5bytesS1246)
          ) {
            #line 221 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3272 = _M0L5bytesS1246[_M0L6_2atmpS3273];
          _M0L6_2atmpS3271 = (int32_t)_M0L6_2atmpS3272;
          _M0L6_2atmpS3270 = _M0L6_2atmpS3271 & 63;
          _M0L6_2atmpS3268 = _M0L6_2atmpS3269 | _M0L6_2atmpS3270;
          _M0L1cS1250->$0 = _M0L6_2atmpS3268;
          _M0L3valS3278 = _M0L1cS1250->$0;
          moonbit_decref(_M0L1cS1250);
          _M0L6_2atmpS3277 = _M0L3valS3278;
          moonbit_incref(_M0L3resS1247);
          #line 222 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1247, _M0L6_2atmpS3277);
          _M0L3valS3280 = _M0L1iS1249->$0;
          _M0L6_2atmpS3279 = _M0L3valS3280 + 2;
          _M0L1iS1249->$0 = _M0L6_2atmpS3279;
        } else {
          int32_t _M0L3valS3281 = _M0L1cS1250->$0;
          if (_M0L3valS3281 < 240) {
            int32_t _M0L3valS3283 = _M0L1iS1249->$0;
            int32_t _M0L6_2atmpS3282 = _M0L3valS3283 + 2;
            int32_t _M0L3valS3299;
            int32_t _M0L6_2atmpS3298;
            int32_t _M0L6_2atmpS3291;
            int32_t _M0L3valS3297;
            int32_t _M0L6_2atmpS3296;
            int32_t _M0L6_2atmpS3295;
            int32_t _M0L6_2atmpS3294;
            int32_t _M0L6_2atmpS3293;
            int32_t _M0L6_2atmpS3292;
            int32_t _M0L6_2atmpS3285;
            int32_t _M0L3valS3290;
            int32_t _M0L6_2atmpS3289;
            int32_t _M0L6_2atmpS3288;
            int32_t _M0L6_2atmpS3287;
            int32_t _M0L6_2atmpS3286;
            int32_t _M0L6_2atmpS3284;
            int32_t _M0L3valS3301;
            int32_t _M0L6_2atmpS3300;
            int32_t _M0L3valS3303;
            int32_t _M0L6_2atmpS3302;
            if (_M0L6_2atmpS3282 >= _M0L3lenS1248) {
              moonbit_decref(_M0L1cS1250);
              moonbit_decref(_M0L1iS1249);
              moonbit_decref(_M0L5bytesS1246);
              break;
            }
            _M0L3valS3299 = _M0L1cS1250->$0;
            _M0L6_2atmpS3298 = _M0L3valS3299 & 15;
            _M0L6_2atmpS3291 = _M0L6_2atmpS3298 << 12;
            _M0L3valS3297 = _M0L1iS1249->$0;
            _M0L6_2atmpS3296 = _M0L3valS3297 + 1;
            if (
              _M0L6_2atmpS3296 < 0
              || _M0L6_2atmpS3296 >= Moonbit_array_length(_M0L5bytesS1246)
            ) {
              #line 229 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3295 = _M0L5bytesS1246[_M0L6_2atmpS3296];
            _M0L6_2atmpS3294 = (int32_t)_M0L6_2atmpS3295;
            _M0L6_2atmpS3293 = _M0L6_2atmpS3294 & 63;
            _M0L6_2atmpS3292 = _M0L6_2atmpS3293 << 6;
            _M0L6_2atmpS3285 = _M0L6_2atmpS3291 | _M0L6_2atmpS3292;
            _M0L3valS3290 = _M0L1iS1249->$0;
            _M0L6_2atmpS3289 = _M0L3valS3290 + 2;
            if (
              _M0L6_2atmpS3289 < 0
              || _M0L6_2atmpS3289 >= Moonbit_array_length(_M0L5bytesS1246)
            ) {
              #line 230 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3288 = _M0L5bytesS1246[_M0L6_2atmpS3289];
            _M0L6_2atmpS3287 = (int32_t)_M0L6_2atmpS3288;
            _M0L6_2atmpS3286 = _M0L6_2atmpS3287 & 63;
            _M0L6_2atmpS3284 = _M0L6_2atmpS3285 | _M0L6_2atmpS3286;
            _M0L1cS1250->$0 = _M0L6_2atmpS3284;
            _M0L3valS3301 = _M0L1cS1250->$0;
            moonbit_decref(_M0L1cS1250);
            _M0L6_2atmpS3300 = _M0L3valS3301;
            moonbit_incref(_M0L3resS1247);
            #line 231 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1247, _M0L6_2atmpS3300);
            _M0L3valS3303 = _M0L1iS1249->$0;
            _M0L6_2atmpS3302 = _M0L3valS3303 + 3;
            _M0L1iS1249->$0 = _M0L6_2atmpS3302;
          } else {
            int32_t _M0L3valS3305 = _M0L1iS1249->$0;
            int32_t _M0L6_2atmpS3304 = _M0L3valS3305 + 3;
            int32_t _M0L3valS3328;
            int32_t _M0L6_2atmpS3327;
            int32_t _M0L6_2atmpS3320;
            int32_t _M0L3valS3326;
            int32_t _M0L6_2atmpS3325;
            int32_t _M0L6_2atmpS3324;
            int32_t _M0L6_2atmpS3323;
            int32_t _M0L6_2atmpS3322;
            int32_t _M0L6_2atmpS3321;
            int32_t _M0L6_2atmpS3313;
            int32_t _M0L3valS3319;
            int32_t _M0L6_2atmpS3318;
            int32_t _M0L6_2atmpS3317;
            int32_t _M0L6_2atmpS3316;
            int32_t _M0L6_2atmpS3315;
            int32_t _M0L6_2atmpS3314;
            int32_t _M0L6_2atmpS3307;
            int32_t _M0L3valS3312;
            int32_t _M0L6_2atmpS3311;
            int32_t _M0L6_2atmpS3310;
            int32_t _M0L6_2atmpS3309;
            int32_t _M0L6_2atmpS3308;
            int32_t _M0L6_2atmpS3306;
            int32_t _M0L3valS3330;
            int32_t _M0L6_2atmpS3329;
            int32_t _M0L3valS3334;
            int32_t _M0L6_2atmpS3333;
            int32_t _M0L6_2atmpS3332;
            int32_t _M0L6_2atmpS3331;
            int32_t _M0L3valS3338;
            int32_t _M0L6_2atmpS3337;
            int32_t _M0L6_2atmpS3336;
            int32_t _M0L6_2atmpS3335;
            int32_t _M0L3valS3340;
            int32_t _M0L6_2atmpS3339;
            if (_M0L6_2atmpS3304 >= _M0L3lenS1248) {
              moonbit_decref(_M0L1cS1250);
              moonbit_decref(_M0L1iS1249);
              moonbit_decref(_M0L5bytesS1246);
              break;
            }
            _M0L3valS3328 = _M0L1cS1250->$0;
            _M0L6_2atmpS3327 = _M0L3valS3328 & 7;
            _M0L6_2atmpS3320 = _M0L6_2atmpS3327 << 18;
            _M0L3valS3326 = _M0L1iS1249->$0;
            _M0L6_2atmpS3325 = _M0L3valS3326 + 1;
            if (
              _M0L6_2atmpS3325 < 0
              || _M0L6_2atmpS3325 >= Moonbit_array_length(_M0L5bytesS1246)
            ) {
              #line 238 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3324 = _M0L5bytesS1246[_M0L6_2atmpS3325];
            _M0L6_2atmpS3323 = (int32_t)_M0L6_2atmpS3324;
            _M0L6_2atmpS3322 = _M0L6_2atmpS3323 & 63;
            _M0L6_2atmpS3321 = _M0L6_2atmpS3322 << 12;
            _M0L6_2atmpS3313 = _M0L6_2atmpS3320 | _M0L6_2atmpS3321;
            _M0L3valS3319 = _M0L1iS1249->$0;
            _M0L6_2atmpS3318 = _M0L3valS3319 + 2;
            if (
              _M0L6_2atmpS3318 < 0
              || _M0L6_2atmpS3318 >= Moonbit_array_length(_M0L5bytesS1246)
            ) {
              #line 239 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3317 = _M0L5bytesS1246[_M0L6_2atmpS3318];
            _M0L6_2atmpS3316 = (int32_t)_M0L6_2atmpS3317;
            _M0L6_2atmpS3315 = _M0L6_2atmpS3316 & 63;
            _M0L6_2atmpS3314 = _M0L6_2atmpS3315 << 6;
            _M0L6_2atmpS3307 = _M0L6_2atmpS3313 | _M0L6_2atmpS3314;
            _M0L3valS3312 = _M0L1iS1249->$0;
            _M0L6_2atmpS3311 = _M0L3valS3312 + 3;
            if (
              _M0L6_2atmpS3311 < 0
              || _M0L6_2atmpS3311 >= Moonbit_array_length(_M0L5bytesS1246)
            ) {
              #line 240 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3310 = _M0L5bytesS1246[_M0L6_2atmpS3311];
            _M0L6_2atmpS3309 = (int32_t)_M0L6_2atmpS3310;
            _M0L6_2atmpS3308 = _M0L6_2atmpS3309 & 63;
            _M0L6_2atmpS3306 = _M0L6_2atmpS3307 | _M0L6_2atmpS3308;
            _M0L1cS1250->$0 = _M0L6_2atmpS3306;
            _M0L3valS3330 = _M0L1cS1250->$0;
            _M0L6_2atmpS3329 = _M0L3valS3330 - 65536;
            _M0L1cS1250->$0 = _M0L6_2atmpS3329;
            _M0L3valS3334 = _M0L1cS1250->$0;
            _M0L6_2atmpS3333 = _M0L3valS3334 >> 10;
            _M0L6_2atmpS3332 = _M0L6_2atmpS3333 + 55296;
            _M0L6_2atmpS3331 = _M0L6_2atmpS3332;
            moonbit_incref(_M0L3resS1247);
            #line 242 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1247, _M0L6_2atmpS3331);
            _M0L3valS3338 = _M0L1cS1250->$0;
            moonbit_decref(_M0L1cS1250);
            _M0L6_2atmpS3337 = _M0L3valS3338 & 1023;
            _M0L6_2atmpS3336 = _M0L6_2atmpS3337 + 56320;
            _M0L6_2atmpS3335 = _M0L6_2atmpS3336;
            moonbit_incref(_M0L3resS1247);
            #line 243 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1247, _M0L6_2atmpS3335);
            _M0L3valS3340 = _M0L1iS1249->$0;
            _M0L6_2atmpS3339 = _M0L3valS3340 + 4;
            _M0L1iS1249->$0 = _M0L6_2atmpS3339;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1249);
      moonbit_decref(_M0L5bytesS1246);
    }
    break;
  }
  #line 247 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1247);
}

int32_t _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1239(
  int32_t _M0L6_2aenvS3251,
  moonbit_string_t _M0L1sS1240
) {
  struct _M0TPB8MutLocalGiE* _M0L3resS1241;
  int32_t _M0L3lenS1242;
  int32_t _M0L1iS1243;
  int32_t _result_4013;
  #line 197 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1241
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L3resS1241)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L3resS1241->$0 = 0;
  _M0L3lenS1242 = Moonbit_array_length(_M0L1sS1240);
  _M0L1iS1243 = 0;
  while (1) {
    if (_M0L1iS1243 < _M0L3lenS1242) {
      int32_t _M0L3valS3256 = _M0L3resS1241->$0;
      int32_t _M0L6_2atmpS3253 = _M0L3valS3256 * 10;
      int32_t _M0L6_2atmpS3255;
      int32_t _M0L6_2atmpS3254;
      int32_t _M0L6_2atmpS3252;
      int32_t _M0L6_2atmpS3257;
      if (
        _M0L1iS1243 < 0 || _M0L1iS1243 >= Moonbit_array_length(_M0L1sS1240)
      ) {
        #line 201 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3255 = _M0L1sS1240[_M0L1iS1243];
      _M0L6_2atmpS3254 = _M0L6_2atmpS3255 - 48;
      _M0L6_2atmpS3252 = _M0L6_2atmpS3253 + _M0L6_2atmpS3254;
      _M0L3resS1241->$0 = _M0L6_2atmpS3252;
      _M0L6_2atmpS3257 = _M0L1iS1243 + 1;
      _M0L1iS1243 = _M0L6_2atmpS3257;
      continue;
    } else {
      moonbit_decref(_M0L1sS1240);
    }
    break;
  }
  _result_4013 = _M0L3resS1241->$0;
  moonbit_decref(_M0L3resS1241);
  return _result_4013;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1219,
  moonbit_string_t _M0L12_2adiscard__S1220,
  int32_t _M0L12_2adiscard__S1221,
  struct _M0TWssbEu* _M0L12_2adiscard__S1222,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1223
) {
  struct moonbit_result_0 _result_4014;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1223);
  moonbit_decref(_M0L12_2adiscard__S1222);
  moonbit_decref(_M0L12_2adiscard__S1220);
  moonbit_decref(_M0L12_2adiscard__S1219);
  _result_4014.tag = 1;
  _result_4014.data.ok = 0;
  return _result_4014;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1224,
  moonbit_string_t _M0L12_2adiscard__S1225,
  int32_t _M0L12_2adiscard__S1226,
  struct _M0TWssbEu* _M0L12_2adiscard__S1227,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1228
) {
  struct moonbit_result_0 _result_4015;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1228);
  moonbit_decref(_M0L12_2adiscard__S1227);
  moonbit_decref(_M0L12_2adiscard__S1225);
  moonbit_decref(_M0L12_2adiscard__S1224);
  _result_4015.tag = 1;
  _result_4015.data.ok = 0;
  return _result_4015;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1229,
  moonbit_string_t _M0L12_2adiscard__S1230,
  int32_t _M0L12_2adiscard__S1231,
  struct _M0TWssbEu* _M0L12_2adiscard__S1232,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1233
) {
  struct moonbit_result_0 _result_4016;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1233);
  moonbit_decref(_M0L12_2adiscard__S1232);
  moonbit_decref(_M0L12_2adiscard__S1230);
  moonbit_decref(_M0L12_2adiscard__S1229);
  _result_4016.tag = 1;
  _result_4016.data.ok = 0;
  return _result_4016;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp6stream21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp6stream50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1234,
  moonbit_string_t _M0L12_2adiscard__S1235,
  int32_t _M0L12_2adiscard__S1236,
  struct _M0TWssbEu* _M0L12_2adiscard__S1237,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1238
) {
  struct moonbit_result_0 _result_4017;
  #line 34 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1238);
  moonbit_decref(_M0L12_2adiscard__S1237);
  moonbit_decref(_M0L12_2adiscard__S1235);
  moonbit_decref(_M0L12_2adiscard__S1234);
  _result_4017.tag = 1;
  _result_4017.data.ok = 0;
  return _result_4017;
}

int32_t _M0IP016_24default__implP36mulpjs4mulp6stream28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp6stream34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1218
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1218);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp6stream53____test__66696c655f646573745f7762746573742e6d6274__0(
  
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L6_2atmpS3250;
  struct _M0TP36mulpjs4mulp4core7Context* _M0L3ctxS1213;
  moonbit_string_t* _M0L6_2atmpS3249;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3248;
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L6_2atmpS3247;
  void* _M0L5BytesS3246;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6targetS1214;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS3196;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS3195;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3193;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3194;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3192;
  void* _M0L6_2atmpS3191;
  void* _M0L9DirectoryS3245;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L9directoryS1215;
  void* _M0L7SymlinkS3244;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L7symlinkS1216;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS3202;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L6_2atmpS3201;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3199;
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3200;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3198;
  void* _M0L6_2atmpS3197;
  int32_t _M0L6_2atmpS3210;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3211;
  struct _M0TPB4Show _M0L6_2atmpS3203;
  moonbit_string_t _M0L6_2atmpS3206;
  moonbit_string_t _M0L6_2atmpS3207;
  moonbit_string_t _M0L6_2atmpS3208;
  moonbit_string_t _M0L6_2atmpS3209;
  moonbit_string_t* _M0L6_2atmpS3205;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3204;
  struct moonbit_result_0 _tmp_4018;
  #line 2 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  #line 6 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3250 = _M0FP36mulpjs4mulp4core24new__cancellation__token();
  #line 3 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L3ctxS1213
  = _M0FP36mulpjs4mulp4core12new__context((moonbit_string_t)moonbit_string_literal_9.data, 0ll, _M0L6_2atmpS3250);
  _M0L6_2atmpS3249 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3249[0] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3248
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3248)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3248->$0 = _M0L6_2atmpS3249;
  _M0L6_2atmpS3248->$1 = 1;
  #line 12 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3247
  = _M0FP36mulpjs4mulp6stream12byte__stream(_M0L6_2atmpS3248);
  _M0L5BytesS3246
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes));
  Moonbit_object_header(_M0L5BytesS3246)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes, $0) >> 2, 1, 3);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes*)_M0L5BytesS3246)->$0
  = _M0L6_2atmpS3247;
  #line 8 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6targetS1214
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data, _M0L5BytesS3246);
  _M0L6_2atmpS3196
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3196[0] = _M0L6targetS1214;
  _M0L6_2atmpS3195
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS3195)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3195->$0 = _M0L6_2atmpS3196;
  _M0L6_2atmpS3195->$1 = 1;
  #line 14 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3193 = _M0FP36mulpjs4mulp6stream3src(_M0L6_2atmpS3195);
  #line 14 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3194
  = _M0FP36mulpjs4mulp6stream10file__dest((moonbit_string_t)moonbit_string_literal_13.data);
  #line 14 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3192
  = _M0MP36mulpjs4mulp6stream10FileStream4pipe(_M0L6_2atmpS3193, _M0L6_2atmpS3194);
  moonbit_incref(_M0L3ctxS1213);
  #line 14 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3191
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS3192, _M0L3ctxS1213);
  moonbit_decref(_M0L6_2atmpS3191);
  _M0L9DirectoryS3245
  = (struct moonbit_object*)&moonbit_constant_constructor_4 + 1;
  #line 15 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L9directoryS1215
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_14.data, _M0L9DirectoryS3245);
  _M0L7SymlinkS3244
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink));
  Moonbit_object_header(_M0L7SymlinkS3244)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink, $0) >> 2, 1, 5);
  ((struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink*)_M0L7SymlinkS3244)->$0
  = (moonbit_string_t)moonbit_string_literal_15.data;
  #line 21 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L7symlinkS1216
  = _M0FP36mulpjs4mulp6stream4file((moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_16.data, _M0L7SymlinkS3244);
  _M0L6_2atmpS3202
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3202[0] = _M0L9directoryS1215;
  _M0L6_2atmpS3202[1] = _M0L7symlinkS1216;
  _M0L6_2atmpS3201
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L6_2atmpS3201)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3201->$0 = _M0L6_2atmpS3202;
  _M0L6_2atmpS3201->$1 = 2;
  #line 27 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3199 = _M0FP36mulpjs4mulp6stream3src(_M0L6_2atmpS3201);
  #line 27 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3200
  = _M0FP36mulpjs4mulp6stream10file__dest((moonbit_string_t)moonbit_string_literal_13.data);
  #line 27 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3198
  = _M0MP36mulpjs4mulp6stream10FileStream4pipe(_M0L6_2atmpS3199, _M0L6_2atmpS3200);
  #line 27 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3197
  = _M0MP36mulpjs4mulp6stream10FileStream7collect(_M0L6_2atmpS3198, _M0L3ctxS1213);
  moonbit_decref(_M0L6_2atmpS3197);
  #line 28 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3210
  = _M0FP36mulpjs4mulp6stream30path__is__directory__for__test((moonbit_string_t)moonbit_string_literal_17.data);
  _M0L14_2aboxed__selfS3211
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3211)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3211->$0 = _M0L6_2atmpS3210;
  _M0L6_2atmpS3203
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3211
  };
  _M0L6_2atmpS3206 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3207 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3208 = 0;
  _M0L6_2atmpS3209 = 0;
  _M0L6_2atmpS3205 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3205[0] = _M0L6_2atmpS3206;
  _M0L6_2atmpS3205[1] = _M0L6_2atmpS3207;
  _M0L6_2atmpS3205[2] = _M0L6_2atmpS3208;
  _M0L6_2atmpS3205[3] = _M0L6_2atmpS3209;
  _M0L6_2atmpS3204
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3204)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3204->$0 = _M0L6_2atmpS3205;
  _M0L6_2atmpS3204->$1 = 4;
  #line 28 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _tmp_4018
  = _M0FPB15inspect_2einner(_M0L6_2atmpS3203, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L6_2atmpS3204);
  if (_tmp_4018.tag) {
    int32_t const _M0L5_2aokS3212 = _tmp_4018.data.ok;
  } else {
    void* const _M0L6_2aerrS3213 = _tmp_4018.data.err;
    struct moonbit_result_0 _result_4019;
    _result_4019.tag = 0;
    _result_4019.data.err = _M0L6_2aerrS3213;
    return _result_4019;
  }
  #line 29 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  if (_M0FP36mulpjs4mulp6stream29symlink__supported__for__test()) {
    int32_t _M0L6_2atmpS3221;
    struct _M0Y4Bool* _M0L14_2aboxed__selfS3222;
    struct _M0TPB4Show _M0L6_2atmpS3214;
    moonbit_string_t _M0L6_2atmpS3217;
    moonbit_string_t _M0L6_2atmpS3218;
    moonbit_string_t _M0L6_2atmpS3219;
    moonbit_string_t _M0L6_2atmpS3220;
    moonbit_string_t* _M0L6_2atmpS3216;
    struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3215;
    struct moonbit_result_0 _tmp_4020;
    void* _M0L6_2atmpS3234;
    struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L6linkedS1217;
    void* _M0L6_2atmpS3233;
    moonbit_string_t _M0L6_2atmpS3232;
    struct _M0TPB4Show _M0L6_2atmpS3225;
    moonbit_string_t _M0L6_2atmpS3228;
    moonbit_string_t _M0L6_2atmpS3229;
    moonbit_string_t _M0L6_2atmpS3230;
    moonbit_string_t _M0L6_2atmpS3231;
    moonbit_string_t* _M0L6_2atmpS3227;
    struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3226;
    #line 30 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _M0L6_2atmpS3221
    = _M0FP36mulpjs4mulp6stream28path__is__symlink__for__test((moonbit_string_t)moonbit_string_literal_22.data);
    _M0L14_2aboxed__selfS3222
    = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
    Moonbit_object_header(_M0L14_2aboxed__selfS3222)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
    _M0L14_2aboxed__selfS3222->$0 = _M0L6_2atmpS3221;
    _M0L6_2atmpS3214
    = (struct _M0TPB4Show){
      _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
        _M0L14_2aboxed__selfS3222
    };
    _M0L6_2atmpS3217 = (moonbit_string_t)moonbit_string_literal_23.data;
    _M0L6_2atmpS3218 = (moonbit_string_t)moonbit_string_literal_24.data;
    _M0L6_2atmpS3219 = 0;
    _M0L6_2atmpS3220 = 0;
    _M0L6_2atmpS3216 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
    _M0L6_2atmpS3216[0] = _M0L6_2atmpS3217;
    _M0L6_2atmpS3216[1] = _M0L6_2atmpS3218;
    _M0L6_2atmpS3216[2] = _M0L6_2atmpS3219;
    _M0L6_2atmpS3216[3] = _M0L6_2atmpS3220;
    _M0L6_2atmpS3215
    = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
    Moonbit_object_header(_M0L6_2atmpS3215)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
    _M0L6_2atmpS3215->$0 = _M0L6_2atmpS3216;
    _M0L6_2atmpS3215->$1 = 4;
    #line 30 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _tmp_4020
    = _M0FPB15inspect_2einner(_M0L6_2atmpS3214, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_25.data, _M0L6_2atmpS3215);
    if (_tmp_4020.tag) {
      int32_t const _M0L5_2aokS3223 = _tmp_4020.data.ok;
    } else {
      void* const _M0L6_2aerrS3224 = _tmp_4020.data.err;
      struct moonbit_result_0 _result_4021;
      _result_4021.tag = 0;
      _result_4021.data.err = _M0L6_2aerrS3224;
      return _result_4021;
    }
    #line 31 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _M0L6_2atmpS3234
    = _M0FP36mulpjs4mulp6stream18file__byte__stream((moonbit_string_t)moonbit_string_literal_22.data, 8);
    #line 31 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _M0L6linkedS1217
    = _M0MPC16result6Result6unwrapGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS3234);
    #line 32 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _M0L6_2atmpS3233
    = _M0MP36mulpjs4mulp6stream10ByteStream9read__all(_M0L6linkedS1217, 8);
    #line 32 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _M0L6_2atmpS3232
    = _M0MPC16result6Result6unwrapGsRP36mulpjs4mulp4core9MulpErrorE(_M0L6_2atmpS3233);
    _M0L6_2atmpS3225
    = (struct _M0TPB4Show){
      _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
        _M0L6_2atmpS3232
    };
    _M0L6_2atmpS3228 = (moonbit_string_t)moonbit_string_literal_26.data;
    _M0L6_2atmpS3229 = (moonbit_string_t)moonbit_string_literal_27.data;
    _M0L6_2atmpS3230 = 0;
    _M0L6_2atmpS3231 = 0;
    _M0L6_2atmpS3227 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
    _M0L6_2atmpS3227[0] = _M0L6_2atmpS3228;
    _M0L6_2atmpS3227[1] = _M0L6_2atmpS3229;
    _M0L6_2atmpS3227[2] = _M0L6_2atmpS3230;
    _M0L6_2atmpS3227[3] = _M0L6_2atmpS3231;
    _M0L6_2atmpS3226
    = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
    Moonbit_object_header(_M0L6_2atmpS3226)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
    _M0L6_2atmpS3226->$0 = _M0L6_2atmpS3227;
    _M0L6_2atmpS3226->$1 = 4;
    #line 32 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    return _M0FPB15inspect_2einner(_M0L6_2atmpS3225, (moonbit_string_t)moonbit_string_literal_10.data, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS3226);
  } else {
    int32_t _M0L6_2atmpS3242;
    struct _M0Y4Bool* _M0L14_2aboxed__selfS3243;
    struct _M0TPB4Show _M0L6_2atmpS3235;
    moonbit_string_t _M0L6_2atmpS3238;
    moonbit_string_t _M0L6_2atmpS3239;
    moonbit_string_t _M0L6_2atmpS3240;
    moonbit_string_t _M0L6_2atmpS3241;
    moonbit_string_t* _M0L6_2atmpS3237;
    struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3236;
    #line 34 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    _M0L6_2atmpS3242
    = _M0FP36mulpjs4mulp6stream28path__is__symlink__for__test((moonbit_string_t)moonbit_string_literal_22.data);
    _M0L14_2aboxed__selfS3243
    = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
    Moonbit_object_header(_M0L14_2aboxed__selfS3243)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
    _M0L14_2aboxed__selfS3243->$0 = _M0L6_2atmpS3242;
    _M0L6_2atmpS3235
    = (struct _M0TPB4Show){
      _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
        _M0L14_2aboxed__selfS3243
    };
    _M0L6_2atmpS3238 = (moonbit_string_t)moonbit_string_literal_29.data;
    _M0L6_2atmpS3239 = (moonbit_string_t)moonbit_string_literal_30.data;
    _M0L6_2atmpS3240 = 0;
    _M0L6_2atmpS3241 = 0;
    _M0L6_2atmpS3237 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
    _M0L6_2atmpS3237[0] = _M0L6_2atmpS3238;
    _M0L6_2atmpS3237[1] = _M0L6_2atmpS3239;
    _M0L6_2atmpS3237[2] = _M0L6_2atmpS3240;
    _M0L6_2atmpS3237[3] = _M0L6_2atmpS3241;
    _M0L6_2atmpS3236
    = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
    Moonbit_object_header(_M0L6_2atmpS3236)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
    _M0L6_2atmpS3236->$0 = _M0L6_2atmpS3237;
    _M0L6_2atmpS3236->$1 = 4;
    #line 34 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
    return _M0FPB15inspect_2einner(_M0L6_2atmpS3235, (moonbit_string_t)moonbit_string_literal_31.data, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS3236);
  }
}

void* _M0MP36mulpjs4mulp6stream10ByteStream9read__all(
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L4selfS1202,
  int32_t _M0L10max__bytesS1201
) {
  moonbit_string_t* _M0L6_2atmpS3190;
  struct _M0TPB5ArrayGsE* _M0L6chunksS1195;
  struct _M0TPB8MutLocalGiE* _M0L5totalS1196;
  moonbit_string_t _M0L7_2abindS1212;
  int32_t _M0L6_2atmpS3189;
  struct _M0TPC16string10StringView _M0L6_2atmpS3188;
  moonbit_string_t _M0L6_2atmpS3187;
  void* _block_4028;
  #line 85 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L6_2atmpS3190 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6chunksS1195
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6chunksS1195)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6chunksS1195->$0 = _M0L6_2atmpS3190;
  _M0L6chunksS1195->$1 = 0;
  _M0L5totalS1196
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5totalS1196)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5totalS1196->$0 = 0;
  while (1) {
    void* _M0L3errS1198;
    moonbit_bytes_t _M0L5chunkS1200;
    void* _M0L7_2abindS1203;
    int32_t _M0L3valS3173;
    int32_t _M0L6_2atmpS3174;
    int32_t _M0L6_2atmpS3172;
    int32_t _M0L3valS3175;
    int32_t _M0L6_2atmpS3183;
    int64_t _M0L6_2atmpS3182;
    struct _M0TPC15bytes9BytesView _M0L6_2atmpS3181;
    moonbit_string_t _M0L6_2atmpS3180;
    void* _block_4027;
    moonbit_incref(_M0L4selfS1202);
    #line 92 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L7_2abindS1203
    = _M0MP36mulpjs4mulp6stream10ByteStream10read__next(_M0L4selfS1202);
    switch (Moonbit_object_tag(_M0L7_2abindS1203)) {
      case 1: {
        struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1204 =
          (struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1203;
        moonbit_bytes_t _M0L8_2afieldS3441 = _M0L5_2aOkS1204->$0;
        int32_t _M0L6_2acntS3813 = Moonbit_object_header(_M0L5_2aOkS1204)->rc;
        moonbit_bytes_t _M0L4_2axS1205;
        if (_M0L6_2acntS3813 > 1) {
          int32_t _M0L11_2anew__cntS3814 = _M0L6_2acntS3813 - 1;
          Moonbit_object_header(_M0L5_2aOkS1204)->rc = _M0L11_2anew__cntS3814;
          if (_M0L8_2afieldS3441) {
            moonbit_incref(_M0L8_2afieldS3441);
          }
        } else if (_M0L6_2acntS3813 == 1) {
          #line 92 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
          moonbit_free(_M0L5_2aOkS1204);
        }
        _M0L4_2axS1205 = _M0L8_2afieldS3441;
        if (_M0L4_2axS1205 == 0) {
          moonbit_string_t _M0L7_2abindS1208;
          int32_t _M0L6_2atmpS3186;
          struct _M0TPC16string10StringView _M0L6_2atmpS3185;
          moonbit_string_t _M0L6_2atmpS3184;
          void* _block_4025;
          if (_M0L4_2axS1205) {
            moonbit_decref(_M0L4_2axS1205);
          }
          moonbit_decref(_M0L4selfS1202);
          moonbit_decref(_M0L5totalS1196);
          _M0L7_2abindS1208 = (moonbit_string_t)moonbit_string_literal_0.data;
          _M0L6_2atmpS3186 = Moonbit_array_length(_M0L7_2abindS1208);
          _M0L6_2atmpS3185
          = (struct _M0TPC16string10StringView){
            0, _M0L6_2atmpS3186, _M0L7_2abindS1208
          };
          #line 103 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
          _M0L6_2atmpS3184
          = _M0MPC15array5Array4joinGsE(_M0L6chunksS1195, _M0L6_2atmpS3185);
          _block_4025
          = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok));
          Moonbit_object_header(_block_4025)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
          ((struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4025)->$0
          = _M0L6_2atmpS3184;
          return _block_4025;
        } else {
          moonbit_bytes_t _M0L7_2aSomeS1206 = _M0L4_2axS1205;
          moonbit_bytes_t _M0L8_2achunkS1207 = _M0L7_2aSomeS1206;
          _M0L5chunkS1200 = _M0L8_2achunkS1207;
          goto join_1199;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1209;
        void* _M0L8_2afieldS3442;
        int32_t _M0L6_2acntS3815;
        void* _M0L6_2aerrS1210;
        moonbit_decref(_M0L4selfS1202);
        moonbit_decref(_M0L5totalS1196);
        moonbit_decref(_M0L6chunksS1195);
        _M0L6_2aErrS1209
        = (struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1203;
        _M0L8_2afieldS3442 = _M0L6_2aErrS1209->$0;
        _M0L6_2acntS3815 = Moonbit_object_header(_M0L6_2aErrS1209)->rc;
        if (_M0L6_2acntS3815 > 1) {
          int32_t _M0L11_2anew__cntS3816 = _M0L6_2acntS3815 - 1;
          Moonbit_object_header(_M0L6_2aErrS1209)->rc
          = _M0L11_2anew__cntS3816;
          moonbit_incref(_M0L8_2afieldS3442);
        } else if (_M0L6_2acntS3815 == 1) {
          #line 92 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
          moonbit_free(_M0L6_2aErrS1209);
        }
        _M0L6_2aerrS1210 = _M0L8_2afieldS3442;
        _M0L3errS1198 = _M0L6_2aerrS1210;
        goto join_1197;
        break;
      }
    }
    goto joinlet_4024;
    join_1199:;
    _M0L3valS3173 = _M0L5totalS1196->$0;
    _M0L6_2atmpS3174 = Moonbit_array_length(_M0L5chunkS1200);
    _M0L6_2atmpS3172 = _M0L3valS3173 + _M0L6_2atmpS3174;
    _M0L5totalS1196->$0 = _M0L6_2atmpS3172;
    _M0L3valS3175 = _M0L5totalS1196->$0;
    if (_M0L3valS3175 > _M0L10max__bytesS1201) {
      moonbit_string_t _M0L6_2atmpS3179;
      moonbit_string_t _M0L6_2atmpS3178;
      moonbit_string_t _M0L6_2atmpS3177;
      void* _M0L6_2atmpS3176;
      void* _block_4026;
      moonbit_decref(_M0L5chunkS1200);
      moonbit_decref(_M0L5totalS1196);
      moonbit_decref(_M0L6chunksS1195);
      #line 96 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
      _M0MP36mulpjs4mulp6stream10ByteStream5close(_M0L4selfS1202);
      #line 98 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
      _M0L6_2atmpS3179
      = _M0IPC13int3IntPB4Show10to__string(_M0L10max__bytesS1201);
      #line 98 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
      _M0L6_2atmpS3178
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_33.data, _M0L6_2atmpS3179);
      moonbit_decref(_M0L6_2atmpS3179);
      #line 98 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
      _M0L6_2atmpS3177
      = moonbit_add_string(_M0L6_2atmpS3178, (moonbit_string_t)moonbit_string_literal_34.data);
      moonbit_decref(_M0L6_2atmpS3178);
      #line 98 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
      _M0L6_2atmpS3176
      = _M0FP36mulpjs4mulp4core13stream__error(_M0L6_2atmpS3177);
      _block_4026
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_4026)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4026)->$0
      = _M0L6_2atmpS3176;
      return _block_4026;
    }
    _M0L6_2atmpS3183 = Moonbit_array_length(_M0L5chunkS1200);
    _M0L6_2atmpS3182 = (int64_t)_M0L6_2atmpS3183;
    #line 101 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L6_2atmpS3181
    = _M0MPC15bytes5Bytes12view_2einner(_M0L5chunkS1200, 0, _M0L6_2atmpS3182);
    #line 101 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L6_2atmpS3180
    = _M0FPC28encoding4utf821decode__lossy_2einner(_M0L6_2atmpS3181, 0);
    moonbit_incref(_M0L6chunksS1195);
    #line 101 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0MPC15array5Array4pushGsE(_M0L6chunksS1195, _M0L6_2atmpS3180);
    joinlet_4024:;
    goto joinlet_4023;
    join_1197:;
    _block_4027
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4027)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4027)->$0
    = _M0L3errS1198;
    return _block_4027;
    joinlet_4023:;
    continue;
    break;
  }
  _M0L7_2abindS1212 = (moonbit_string_t)moonbit_string_literal_0.data;
  _M0L6_2atmpS3189 = Moonbit_array_length(_M0L7_2abindS1212);
  _M0L6_2atmpS3188
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3189, _M0L7_2abindS1212
  };
  #line 107 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L6_2atmpS3187
  = _M0MPC15array5Array4joinGsE(_M0L6chunksS1195, _M0L6_2atmpS3188);
  _block_4028
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_4028)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4028)->$0
  = _M0L6_2atmpS3187;
  return _block_4028;
}

int32_t _M0FP36mulpjs4mulp6stream28path__is__symlink__for__test(
  moonbit_string_t _M0L4pathS1194
) {
  int32_t _M0L6_2atmpS3171;
  struct _M0TPC16string10StringView _M0L6_2atmpS3170;
  moonbit_bytes_t _M0L6_2atmpS3169;
  int32_t _result_4029;
  #line 55 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3171 = Moonbit_array_length(_M0L4pathS1194);
  _M0L6_2atmpS3170
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3171, _M0L4pathS1194
  };
  #line 56 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3169
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3170, 0);
  #line 56 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _result_4029
  = _M0FP36mulpjs4mulp6stream33path__is__symlink__for__test__ffi(_M0L6_2atmpS3169);
  moonbit_decref(_M0L6_2atmpS3169);
  return _result_4029;
}

int32_t _M0FP36mulpjs4mulp6stream30path__is__directory__for__test(
  moonbit_string_t _M0L4pathS1193
) {
  int32_t _M0L6_2atmpS3168;
  struct _M0TPC16string10StringView _M0L6_2atmpS3167;
  moonbit_bytes_t _M0L6_2atmpS3166;
  int32_t _result_4030;
  #line 50 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3168 = Moonbit_array_length(_M0L4pathS1193);
  _M0L6_2atmpS3167
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3168, _M0L4pathS1193
  };
  #line 51 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _M0L6_2atmpS3166
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3167, 0);
  #line 51 "/Users/user/workspace/github/gulp/mulp/stream/file_dest_wbtest.mbt"
  _result_4030
  = _M0FP36mulpjs4mulp6stream35path__is__directory__for__test__ffi(_M0L6_2atmpS3166);
  moonbit_decref(_M0L6_2atmpS3166);
  return _result_4030;
}

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream10file__dest(
  moonbit_string_t _M0L8out__dirS1192
) {
  moonbit_string_t _M0L6_2atmpS3165;
  #line 113 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3165 = 0;
  #line 114 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  return _M0FP36mulpjs4mulp6stream25file__dest__with__options(_M0L8out__dirS1192, _M0L6_2atmpS3165);
}

struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream25file__dest__with__options(
  moonbit_string_t _M0L8out__dirS1183,
  moonbit_string_t _M0L3cwdS1184
) {
  struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__* _closure_4031;
  #line 118 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _closure_4031
  = (struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__*)moonbit_malloc(sizeof(struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__));
  Moonbit_object_header(_closure_4031)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__, $0) >> 2, 2, 0);
  _closure_4031->code
  = &_M0FP36mulpjs4mulp6stream25file__dest__with__optionsC3154l119;
  _closure_4031->$0 = _M0L3cwdS1184;
  _closure_4031->$1 = _M0L8out__dirS1183;
  return (struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream*)_closure_4031;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream25file__dest__with__optionsC3154l119(
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L6_2aenvS3155,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5inputS1181
) {
  struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__* _M0L14_2acasted__envS3156;
  moonbit_string_t _M0L8out__dirS1183;
  moonbit_string_t _M0L8_2afieldS3443;
  int32_t _M0L6_2acntS3817;
  moonbit_string_t _M0L3cwdS1184;
  moonbit_string_t _M0L8out__dirS1182;
  moonbit_string_t _M0L6_2atmpS3164;
  moonbit_string_t _M0L6_2atmpS3163;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6outputS1185;
  struct _M0TP36mulpjs4mulp6stream9DestState* _M0L5stateS1186;
  struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__* _closure_4032;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS3157;
  #line 119 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L14_2acasted__envS3156
  = (struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3154__l119__*)_M0L6_2aenvS3155;
  _M0L8out__dirS1183 = _M0L14_2acasted__envS3156->$1;
  _M0L8_2afieldS3443 = _M0L14_2acasted__envS3156->$0;
  _M0L6_2acntS3817 = Moonbit_object_header(_M0L14_2acasted__envS3156)->rc;
  if (_M0L6_2acntS3817 > 1) {
    int32_t _M0L11_2anew__cntS3818 = _M0L6_2acntS3817 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3156)->rc
    = _M0L11_2anew__cntS3818;
    moonbit_incref(_M0L8out__dirS1183);
    if (_M0L8_2afieldS3443) {
      moonbit_incref(_M0L8_2afieldS3443);
    }
  } else if (_M0L6_2acntS3817 == 1) {
    #line 119 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    moonbit_free(_M0L14_2acasted__envS3156);
  }
  _M0L3cwdS1184 = _M0L8_2afieldS3443;
  #line 120 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L8out__dirS1182
  = _M0FP36mulpjs4mulp6stream24resolve__real__dest__dir(_M0L8out__dirS1183, _M0L3cwdS1184);
  moonbit_incref(_M0L5inputS1181);
  #line 123 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3164
  = _M0MP36mulpjs4mulp6stream4File14relative__path(_M0L5inputS1181);
  #line 123 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3163
  = _M0FP36mulpjs4mulp6stream22join__real__dest__path(_M0L8out__dirS1182, _M0L6_2atmpS3164);
  #line 121 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6outputS1185
  = _M0FP36mulpjs4mulp6stream22with__real__dest__path(_M0L5inputS1181, _M0L6_2atmpS3163);
  _M0L5stateS1186
  = (struct _M0TP36mulpjs4mulp6stream9DestState*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream9DestState));
  Moonbit_object_header(_M0L5stateS1186)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP36mulpjs4mulp6stream9DestState) >> 2, 0, 0);
  _M0L5stateS1186->$0 = 0;
  _closure_4032
  = (struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__*)moonbit_malloc(sizeof(struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__));
  Moonbit_object_header(_closure_4032)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__, $0) >> 2, 2, 0);
  _closure_4032->code
  = &_M0FP36mulpjs4mulp6stream25file__dest__with__optionsC3158l126;
  _closure_4032->$0 = _M0L5stateS1186;
  _closure_4032->$1 = _M0L6outputS1185;
  _M0L6_2atmpS3157
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_4032;
  #line 126 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  return _M0FP36mulpjs4mulp6stream24file__stream__from__pull(_M0L6_2atmpS3157);
}

void* _M0FP36mulpjs4mulp6stream25file__dest__with__optionsC3158l126(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS3159
) {
  struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__* _M0L14_2acasted__envS3160;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6outputS1185;
  struct _M0TP36mulpjs4mulp6stream9DestState* _M0L8_2afieldS3446;
  int32_t _M0L6_2acntS3819;
  struct _M0TP36mulpjs4mulp6stream9DestState* _M0L5stateS1186;
  #line 126 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L14_2acasted__envS3160
  = (struct _M0R75_24mulpjs_2fmulp_2fstream_2efile__dest__with__options_2eanon__u3158__l126__*)_M0L6_2aenvS3159;
  _M0L6outputS1185 = _M0L14_2acasted__envS3160->$1;
  _M0L8_2afieldS3446 = _M0L14_2acasted__envS3160->$0;
  _M0L6_2acntS3819 = Moonbit_object_header(_M0L14_2acasted__envS3160)->rc;
  if (_M0L6_2acntS3819 > 1) {
    int32_t _M0L11_2anew__cntS3820 = _M0L6_2acntS3819 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3160)->rc
    = _M0L11_2anew__cntS3820;
    moonbit_incref(_M0L6outputS1185);
    moonbit_incref(_M0L8_2afieldS3446);
  } else if (_M0L6_2acntS3819 == 1) {
    #line 126 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    moonbit_free(_M0L14_2acasted__envS3160);
  }
  _M0L5stateS1186 = _M0L8_2afieldS3446;
  if (_M0L5stateS1186->$0) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3161;
    void* _block_4033;
    moonbit_decref(_M0L5stateS1186);
    moonbit_decref(_M0L6outputS1185);
    _M0L6_2atmpS3161 = 0;
    _block_4033
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4033)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4033)->$0
    = _M0L6_2atmpS3161;
    return _block_4033;
  } else {
    void* _M0L3errS1188;
    void* _M0L7_2abindS1189;
    void* _block_4036;
    moonbit_incref(_M0L6outputS1185);
    #line 130 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L7_2abindS1189
    = _M0FP36mulpjs4mulp6stream21write__file__contents(_M0L6outputS1185);
    switch (Moonbit_object_tag(_M0L7_2abindS1189)) {
      case 1: {
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3162;
        void* _block_4035;
        moonbit_decref(_M0L7_2abindS1189);
        _M0L5stateS1186->$0 = 1;
        moonbit_decref(_M0L5stateS1186);
        _M0L6_2atmpS3162 = _M0L6outputS1185;
        _block_4035
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
        Moonbit_object_header(_block_4035)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
        ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4035)->$0
        = _M0L6_2atmpS3162;
        return _block_4035;
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1190;
        void* _M0L8_2afieldS3445;
        int32_t _M0L6_2acntS3821;
        void* _M0L6_2aerrS1191;
        moonbit_decref(_M0L5stateS1186);
        moonbit_decref(_M0L6outputS1185);
        _M0L6_2aErrS1190
        = (struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1189;
        _M0L8_2afieldS3445 = _M0L6_2aErrS1190->$0;
        _M0L6_2acntS3821 = Moonbit_object_header(_M0L6_2aErrS1190)->rc;
        if (_M0L6_2acntS3821 > 1) {
          int32_t _M0L11_2anew__cntS3822 = _M0L6_2acntS3821 - 1;
          Moonbit_object_header(_M0L6_2aErrS1190)->rc
          = _M0L11_2anew__cntS3822;
          moonbit_incref(_M0L8_2afieldS3445);
        } else if (_M0L6_2acntS3821 == 1) {
          #line 130 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
          moonbit_free(_M0L6_2aErrS1190);
        }
        _M0L6_2aerrS1191 = _M0L8_2afieldS3445;
        _M0L3errS1188 = _M0L6_2aerrS1191;
        goto join_1187;
        break;
      }
    }
    join_1187:;
    _block_4036
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4036)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4036)->$0
    = _M0L3errS1188;
    return _block_4036;
  }
}

void* _M0FP36mulpjs4mulp6stream21write__file__contents(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1164
) {
  moonbit_string_t _M0L6targetS1162;
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L6streamS1166;
  moonbit_bytes_t _M0L8contentsS1168;
  moonbit_string_t _M0L8contentsS1170;
  void* _M0L7_2abindS1171;
  moonbit_string_t _M0L8_2afieldS3452;
  int32_t _M0L6_2acntS3844;
  moonbit_string_t _M0L4pathS3145;
  moonbit_string_t _M0L8_2afieldS3451;
  int32_t _M0L6_2acntS3837;
  moonbit_string_t _M0L4pathS3144;
  moonbit_string_t _M0L8_2afieldS3450;
  int32_t _M0L6_2acntS3830;
  moonbit_string_t _M0L4pathS3143;
  int32_t _M0L6_2atmpS3138;
  struct _M0TPC16string10StringView _M0L6_2atmpS3137;
  moonbit_bytes_t _M0L6_2atmpS3133;
  moonbit_string_t _M0L7_2abindS1163;
  int32_t _M0L6_2atmpS3136;
  struct _M0TPC16string10StringView _M0L6_2atmpS3135;
  moonbit_bytes_t _M0L6_2atmpS3134;
  int32_t _result_4044;
  #line 91 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L7_2abindS1171 = _M0L4fileS1164->$3;
  switch (Moonbit_object_tag(_M0L7_2abindS1171)) {
    case 1: {
      struct _M0DTP36mulpjs4mulp6stream12FileContents4Text* _M0L7_2aTextS1172 =
        (struct _M0DTP36mulpjs4mulp6stream12FileContents4Text*)_M0L7_2abindS1171;
      moonbit_string_t _M0L11_2acontentsS1173 = _M0L7_2aTextS1172->$0;
      moonbit_incref(_M0L11_2acontentsS1173);
      _M0L8contentsS1170 = _M0L11_2acontentsS1173;
      goto join_1169;
      break;
    }
    
    case 2: {
      struct _M0DTP36mulpjs4mulp6stream12FileContents6Buffer* _M0L9_2aBufferS1174 =
        (struct _M0DTP36mulpjs4mulp6stream12FileContents6Buffer*)_M0L7_2abindS1171;
      moonbit_bytes_t _M0L11_2acontentsS1175 = _M0L9_2aBufferS1174->$0;
      moonbit_incref(_M0L11_2acontentsS1175);
      _M0L8contentsS1168 = _M0L11_2acontentsS1175;
      goto join_1167;
      break;
    }
    
    case 3: {
      struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes* _M0L8_2aBytesS1176 =
        (struct _M0DTP36mulpjs4mulp6stream12FileContents5Bytes*)_M0L7_2abindS1171;
      struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L9_2astreamS1177 =
        _M0L8_2aBytesS1176->$0;
      moonbit_incref(_M0L9_2astreamS1177);
      _M0L6streamS1166 = _M0L9_2astreamS1177;
      goto join_1165;
      break;
    }
    
    case 0: {
      moonbit_string_t _M0L8_2afieldS3456 = _M0L4fileS1164->$2;
      int32_t _M0L6_2acntS3851 = Moonbit_object_header(_M0L4fileS1164)->rc;
      moonbit_string_t _M0L4pathS3153;
      if (_M0L6_2acntS3851 > 1) {
        int32_t _M0L11_2anew__cntS3856 = _M0L6_2acntS3851 - 1;
        Moonbit_object_header(_M0L4fileS1164)->rc = _M0L11_2anew__cntS3856;
        moonbit_incref(_M0L7_2abindS1171);
        moonbit_incref(_M0L8_2afieldS3456);
      } else if (_M0L6_2acntS3851 == 1) {
        moonbit_string_t _M0L8_2afieldS3855 = _M0L4fileS1164->$5;
        struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3854;
        moonbit_string_t _M0L8_2afieldS3853;
        moonbit_string_t _M0L8_2afieldS3852;
        if (_M0L8_2afieldS3855) {
          moonbit_decref(_M0L8_2afieldS3855);
        }
        _M0L8_2afieldS3854 = _M0L4fileS1164->$4;
        moonbit_decref(_M0L8_2afieldS3854);
        _M0L8_2afieldS3853 = _M0L4fileS1164->$1;
        moonbit_decref(_M0L8_2afieldS3853);
        _M0L8_2afieldS3852 = _M0L4fileS1164->$0;
        moonbit_decref(_M0L8_2afieldS3852);
        #line 96 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
        moonbit_free(_M0L4fileS1164);
      }
      _M0L4pathS3153 = _M0L8_2afieldS3456;
      #line 96 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      return _M0FP36mulpjs4mulp6stream17write__real__file(_M0L4pathS3153, (moonbit_string_t)moonbit_string_literal_0.data);
      break;
    }
    
    case 4: {
      moonbit_string_t _M0L7_2abindS1178 = _M0L4fileS1164->$2;
      int32_t _M0L6_2atmpS3148 = Moonbit_array_length(_M0L7_2abindS1178);
      struct _M0TPC16string10StringView _M0L6_2atmpS3147;
      moonbit_bytes_t _M0L6_2atmpS3146;
      int32_t _result_4041;
      moonbit_incref(_M0L7_2abindS1178);
      _M0L6_2atmpS3147
      = (struct _M0TPC16string10StringView){
        0, _M0L6_2atmpS3148, _M0L7_2abindS1178
      };
      moonbit_incref(_M0L7_2abindS1171);
      #line 98 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      _M0L6_2atmpS3146
      = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3147, 0);
      #line 98 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      _result_4041
      = _M0FP36mulpjs4mulp6stream21make__directory__path(_M0L6_2atmpS3146);
      moonbit_decref(_M0L6_2atmpS3146);
      if (_result_4041) {
        int32_t _M0L6_2atmpS3149;
        void* _block_4042;
        moonbit_decref(_M0L4fileS1164);
        _M0L6_2atmpS3149 = 0;
        _block_4042
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
        Moonbit_object_header(_block_4042)->meta
        = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
        ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4042)->$0
        = _M0L6_2atmpS3149;
        return _block_4042;
      } else {
        moonbit_string_t _M0L8_2afieldS3457 = _M0L4fileS1164->$2;
        int32_t _M0L6_2acntS3857 = Moonbit_object_header(_M0L4fileS1164)->rc;
        moonbit_string_t _M0L4pathS3152;
        moonbit_string_t _M0L6_2atmpS3151;
        void* _M0L6_2atmpS3150;
        void* _block_4043;
        if (_M0L6_2acntS3857 > 1) {
          int32_t _M0L11_2anew__cntS3863 = _M0L6_2acntS3857 - 1;
          Moonbit_object_header(_M0L4fileS1164)->rc = _M0L11_2anew__cntS3863;
          moonbit_incref(_M0L8_2afieldS3457);
        } else if (_M0L6_2acntS3857 == 1) {
          moonbit_string_t _M0L8_2afieldS3862 = _M0L4fileS1164->$5;
          struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3861;
          void* _M0L8_2afieldS3860;
          moonbit_string_t _M0L8_2afieldS3859;
          moonbit_string_t _M0L8_2afieldS3858;
          if (_M0L8_2afieldS3862) {
            moonbit_decref(_M0L8_2afieldS3862);
          }
          _M0L8_2afieldS3861 = _M0L4fileS1164->$4;
          moonbit_decref(_M0L8_2afieldS3861);
          _M0L8_2afieldS3860 = _M0L4fileS1164->$3;
          moonbit_decref(_M0L8_2afieldS3860);
          _M0L8_2afieldS3859 = _M0L4fileS1164->$1;
          moonbit_decref(_M0L8_2afieldS3859);
          _M0L8_2afieldS3858 = _M0L4fileS1164->$0;
          moonbit_decref(_M0L8_2afieldS3858);
          #line 101 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
          moonbit_free(_M0L4fileS1164);
        }
        _M0L4pathS3152 = _M0L8_2afieldS3457;
        #line 101 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
        _M0L6_2atmpS3151
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_35.data, _M0L4pathS3152);
        moonbit_decref(_M0L4pathS3152);
        #line 101 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
        _M0L6_2atmpS3150
        = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3151);
        _block_4043
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
        Moonbit_object_header(_block_4043)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
        ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4043)->$0
        = _M0L6_2atmpS3150;
        return _block_4043;
      }
      break;
    }
    default: {
      struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink* _M0L10_2aSymlinkS1179 =
        (struct _M0DTP36mulpjs4mulp6stream12FileContents7Symlink*)_M0L7_2abindS1171;
      moonbit_string_t _M0L9_2atargetS1180 = _M0L10_2aSymlinkS1179->$0;
      moonbit_incref(_M0L9_2atargetS1180);
      _M0L6targetS1162 = _M0L9_2atargetS1180;
      goto join_1161;
      break;
    }
  }
  join_1169:;
  _M0L8_2afieldS3452 = _M0L4fileS1164->$2;
  _M0L6_2acntS3844 = Moonbit_object_header(_M0L4fileS1164)->rc;
  if (_M0L6_2acntS3844 > 1) {
    int32_t _M0L11_2anew__cntS3850 = _M0L6_2acntS3844 - 1;
    Moonbit_object_header(_M0L4fileS1164)->rc = _M0L11_2anew__cntS3850;
    moonbit_incref(_M0L8_2afieldS3452);
  } else if (_M0L6_2acntS3844 == 1) {
    moonbit_string_t _M0L8_2afieldS3849 = _M0L4fileS1164->$5;
    struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3848;
    void* _M0L8_2afieldS3847;
    moonbit_string_t _M0L8_2afieldS3846;
    moonbit_string_t _M0L8_2afieldS3845;
    if (_M0L8_2afieldS3849) {
      moonbit_decref(_M0L8_2afieldS3849);
    }
    _M0L8_2afieldS3848 = _M0L4fileS1164->$4;
    moonbit_decref(_M0L8_2afieldS3848);
    _M0L8_2afieldS3847 = _M0L4fileS1164->$3;
    moonbit_decref(_M0L8_2afieldS3847);
    _M0L8_2afieldS3846 = _M0L4fileS1164->$1;
    moonbit_decref(_M0L8_2afieldS3846);
    _M0L8_2afieldS3845 = _M0L4fileS1164->$0;
    moonbit_decref(_M0L8_2afieldS3845);
    #line 93 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    moonbit_free(_M0L4fileS1164);
  }
  _M0L4pathS3145 = _M0L8_2afieldS3452;
  #line 93 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  return _M0FP36mulpjs4mulp6stream17write__real__file(_M0L4pathS3145, _M0L8contentsS1170);
  join_1167:;
  _M0L8_2afieldS3451 = _M0L4fileS1164->$2;
  _M0L6_2acntS3837 = Moonbit_object_header(_M0L4fileS1164)->rc;
  if (_M0L6_2acntS3837 > 1) {
    int32_t _M0L11_2anew__cntS3843 = _M0L6_2acntS3837 - 1;
    Moonbit_object_header(_M0L4fileS1164)->rc = _M0L11_2anew__cntS3843;
    moonbit_incref(_M0L8_2afieldS3451);
  } else if (_M0L6_2acntS3837 == 1) {
    moonbit_string_t _M0L8_2afieldS3842 = _M0L4fileS1164->$5;
    struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3841;
    void* _M0L8_2afieldS3840;
    moonbit_string_t _M0L8_2afieldS3839;
    moonbit_string_t _M0L8_2afieldS3838;
    if (_M0L8_2afieldS3842) {
      moonbit_decref(_M0L8_2afieldS3842);
    }
    _M0L8_2afieldS3841 = _M0L4fileS1164->$4;
    moonbit_decref(_M0L8_2afieldS3841);
    _M0L8_2afieldS3840 = _M0L4fileS1164->$3;
    moonbit_decref(_M0L8_2afieldS3840);
    _M0L8_2afieldS3839 = _M0L4fileS1164->$1;
    moonbit_decref(_M0L8_2afieldS3839);
    _M0L8_2afieldS3838 = _M0L4fileS1164->$0;
    moonbit_decref(_M0L8_2afieldS3838);
    #line 94 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    moonbit_free(_M0L4fileS1164);
  }
  _M0L4pathS3144 = _M0L8_2afieldS3451;
  #line 94 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  return _M0FP36mulpjs4mulp6stream19write__real__buffer(_M0L4pathS3144, _M0L8contentsS1168);
  join_1165:;
  _M0L8_2afieldS3450 = _M0L4fileS1164->$2;
  _M0L6_2acntS3830 = Moonbit_object_header(_M0L4fileS1164)->rc;
  if (_M0L6_2acntS3830 > 1) {
    int32_t _M0L11_2anew__cntS3836 = _M0L6_2acntS3830 - 1;
    Moonbit_object_header(_M0L4fileS1164)->rc = _M0L11_2anew__cntS3836;
    moonbit_incref(_M0L8_2afieldS3450);
  } else if (_M0L6_2acntS3830 == 1) {
    moonbit_string_t _M0L8_2afieldS3835 = _M0L4fileS1164->$5;
    struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3834;
    void* _M0L8_2afieldS3833;
    moonbit_string_t _M0L8_2afieldS3832;
    moonbit_string_t _M0L8_2afieldS3831;
    if (_M0L8_2afieldS3835) {
      moonbit_decref(_M0L8_2afieldS3835);
    }
    _M0L8_2afieldS3834 = _M0L4fileS1164->$4;
    moonbit_decref(_M0L8_2afieldS3834);
    _M0L8_2afieldS3833 = _M0L4fileS1164->$3;
    moonbit_decref(_M0L8_2afieldS3833);
    _M0L8_2afieldS3832 = _M0L4fileS1164->$1;
    moonbit_decref(_M0L8_2afieldS3832);
    _M0L8_2afieldS3831 = _M0L4fileS1164->$0;
    moonbit_decref(_M0L8_2afieldS3831);
    #line 95 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    moonbit_free(_M0L4fileS1164);
  }
  _M0L4pathS3143 = _M0L8_2afieldS3450;
  #line 95 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  return _M0FP36mulpjs4mulp6stream25write__real__file__stream(_M0L4pathS3143, _M0L6streamS1166);
  join_1161:;
  _M0L6_2atmpS3138 = Moonbit_array_length(_M0L6targetS1162);
  _M0L6_2atmpS3137
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3138, _M0L6targetS1162
  };
  #line 104 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3133
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3137, 0);
  _M0L7_2abindS1163 = _M0L4fileS1164->$2;
  _M0L6_2atmpS3136 = Moonbit_array_length(_M0L7_2abindS1163);
  moonbit_incref(_M0L7_2abindS1163);
  _M0L6_2atmpS3135
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3136, _M0L7_2abindS1163
  };
  #line 104 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3134
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3135, 0);
  #line 104 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _result_4044
  = _M0FP36mulpjs4mulp6stream19make__symlink__path(_M0L6_2atmpS3133, _M0L6_2atmpS3134);
  moonbit_decref(_M0L6_2atmpS3133);
  moonbit_decref(_M0L6_2atmpS3134);
  if (_result_4044) {
    int32_t _M0L6_2atmpS3139;
    void* _block_4045;
    moonbit_decref(_M0L4fileS1164);
    _M0L6_2atmpS3139 = 0;
    _block_4045
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4045)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4045)->$0
    = _M0L6_2atmpS3139;
    return _block_4045;
  } else {
    moonbit_string_t _M0L8_2afieldS3448 = _M0L4fileS1164->$2;
    int32_t _M0L6_2acntS3823 = Moonbit_object_header(_M0L4fileS1164)->rc;
    moonbit_string_t _M0L4pathS3142;
    moonbit_string_t _M0L6_2atmpS3141;
    void* _M0L6_2atmpS3140;
    void* _block_4046;
    if (_M0L6_2acntS3823 > 1) {
      int32_t _M0L11_2anew__cntS3829 = _M0L6_2acntS3823 - 1;
      Moonbit_object_header(_M0L4fileS1164)->rc = _M0L11_2anew__cntS3829;
      moonbit_incref(_M0L8_2afieldS3448);
    } else if (_M0L6_2acntS3823 == 1) {
      moonbit_string_t _M0L8_2afieldS3828 = _M0L4fileS1164->$5;
      struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3827;
      void* _M0L8_2afieldS3826;
      moonbit_string_t _M0L8_2afieldS3825;
      moonbit_string_t _M0L8_2afieldS3824;
      if (_M0L8_2afieldS3828) {
        moonbit_decref(_M0L8_2afieldS3828);
      }
      _M0L8_2afieldS3827 = _M0L4fileS1164->$4;
      moonbit_decref(_M0L8_2afieldS3827);
      _M0L8_2afieldS3826 = _M0L4fileS1164->$3;
      moonbit_decref(_M0L8_2afieldS3826);
      _M0L8_2afieldS3825 = _M0L4fileS1164->$1;
      moonbit_decref(_M0L8_2afieldS3825);
      _M0L8_2afieldS3824 = _M0L4fileS1164->$0;
      moonbit_decref(_M0L8_2afieldS3824);
      #line 107 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      moonbit_free(_M0L4fileS1164);
    }
    _M0L4pathS3142 = _M0L8_2afieldS3448;
    #line 107 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3141
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_36.data, _M0L4pathS3142);
    moonbit_decref(_M0L4pathS3142);
    #line 107 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3140
    = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3141);
    _block_4046
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4046)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4046)->$0
    = _M0L6_2atmpS3140;
    return _block_4046;
  }
}

void* _M0FP36mulpjs4mulp6stream25write__real__file__stream(
  moonbit_string_t _M0L4pathS1147,
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L6streamS1153
) {
  int32_t _M0L6_2atmpS3132;
  struct _M0TPC16string10StringView _M0L6_2atmpS3131;
  moonbit_bytes_t _M0L6_2atmpS3130;
  void* _M0L6writerS1146;
  int32_t _M0L6_2atmpS3118;
  #line 59 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3132 = Moonbit_array_length(_M0L4pathS1147);
  moonbit_incref(_M0L4pathS1147);
  _M0L6_2atmpS3131
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3132, _M0L4pathS1147
  };
  #line 63 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3130
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3131, 0);
  #line 63 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6writerS1146
  = _M0FP36mulpjs4mulp6stream18open__file__writer(_M0L6_2atmpS3130);
  moonbit_decref(_M0L6_2atmpS3130);
  #line 64 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3118
  = _M0FP36mulpjs4mulp6stream22file__writer__is__open(_M0L6writerS1146);
  if (!_M0L6_2atmpS3118) {
    moonbit_string_t _M0L6_2atmpS3120;
    void* _M0L6_2atmpS3119;
    void* _block_4047;
    moonbit_decref(_M0L6streamS1153);
    if (_M0L6writerS1146) {
      moonbit_decref(_M0L6writerS1146);
    }
    #line 65 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3120
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L4pathS1147);
    moonbit_decref(_M0L4pathS1147);
    #line 65 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3119
    = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3120);
    _block_4047
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4047)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4047)->$0
    = _M0L6_2atmpS3119;
    return _block_4047;
  } else {
    int32_t _M0L6_2atmpS3129;
    void* _block_4056;
    while (1) {
      void* _M0L3errS1149;
      moonbit_bytes_t _M0L5chunkS1151;
      void* _M0L7_2abindS1152;
      int32_t _M0L6_2atmpS3122;
      int32_t _M0L6_2atmpS3121;
      void* _block_4055;
      moonbit_incref(_M0L6streamS1153);
      #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      _M0L7_2abindS1152
      = _M0MP36mulpjs4mulp6stream10ByteStream10read__next(_M0L6streamS1153);
      switch (Moonbit_object_tag(_M0L7_2abindS1152)) {
        case 1: {
          struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1154 =
            (struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1152;
          moonbit_bytes_t _M0L8_2afieldS3461 = _M0L5_2aOkS1154->$0;
          int32_t _M0L6_2acntS3864 =
            Moonbit_object_header(_M0L5_2aOkS1154)->rc;
          moonbit_bytes_t _M0L4_2axS1155;
          if (_M0L6_2acntS3864 > 1) {
            int32_t _M0L11_2anew__cntS3865 = _M0L6_2acntS3864 - 1;
            Moonbit_object_header(_M0L5_2aOkS1154)->rc
            = _M0L11_2anew__cntS3865;
            if (_M0L8_2afieldS3461) {
              moonbit_incref(_M0L8_2afieldS3461);
            }
          } else if (_M0L6_2acntS3864 == 1) {
            #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
            moonbit_free(_M0L5_2aOkS1154);
          }
          _M0L4_2axS1155 = _M0L8_2afieldS3461;
          if (_M0L4_2axS1155 == 0) {
            int32_t _result_4051;
            if (_M0L4_2axS1155) {
              moonbit_decref(_M0L4_2axS1155);
            }
            moonbit_decref(_M0L6streamS1153);
            #line 75 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
            _result_4051
            = _M0FP36mulpjs4mulp6stream19file__writer__close(_M0L6writerS1146);
            if (_M0L6writerS1146) {
              moonbit_decref(_M0L6writerS1146);
            }
            if (_result_4051) {
              int32_t _M0L6_2atmpS3126;
              void* _block_4052;
              moonbit_decref(_M0L4pathS1147);
              _M0L6_2atmpS3126 = 0;
              _block_4052
              = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
              Moonbit_object_header(_block_4052)->meta
              = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
              ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4052)->$0
              = _M0L6_2atmpS3126;
              return _block_4052;
            } else {
              moonbit_string_t _M0L6_2atmpS3128;
              void* _M0L6_2atmpS3127;
              void* _block_4053;
              #line 78 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
              _M0L6_2atmpS3128
              = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L4pathS1147);
              moonbit_decref(_M0L4pathS1147);
              #line 78 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
              _M0L6_2atmpS3127
              = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3128);
              _block_4053
              = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
              Moonbit_object_header(_block_4053)->meta
              = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
              ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4053)->$0
              = _M0L6_2atmpS3127;
              return _block_4053;
            }
          } else {
            moonbit_bytes_t _M0L7_2aSomeS1156 = _M0L4_2axS1155;
            moonbit_bytes_t _M0L8_2achunkS1157 = _M0L7_2aSomeS1156;
            _M0L5chunkS1151 = _M0L8_2achunkS1157;
            goto join_1150;
          }
          break;
        }
        default: {
          struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1158;
          void* _M0L8_2afieldS3462;
          int32_t _M0L6_2acntS3866;
          void* _M0L6_2aerrS1159;
          moonbit_decref(_M0L6streamS1153);
          moonbit_decref(_M0L4pathS1147);
          _M0L6_2aErrS1158
          = (struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1152;
          _M0L8_2afieldS3462 = _M0L6_2aErrS1158->$0;
          _M0L6_2acntS3866 = Moonbit_object_header(_M0L6_2aErrS1158)->rc;
          if (_M0L6_2acntS3866 > 1) {
            int32_t _M0L11_2anew__cntS3867 = _M0L6_2acntS3866 - 1;
            Moonbit_object_header(_M0L6_2aErrS1158)->rc
            = _M0L11_2anew__cntS3867;
            moonbit_incref(_M0L8_2afieldS3462);
          } else if (_M0L6_2acntS3866 == 1) {
            #line 68 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
            moonbit_free(_M0L6_2aErrS1158);
          }
          _M0L6_2aerrS1159 = _M0L8_2afieldS3462;
          _M0L3errS1149 = _M0L6_2aerrS1159;
          goto join_1148;
          break;
        }
      }
      goto joinlet_4050;
      join_1150:;
      #line 70 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      _M0L6_2atmpS3122
      = _M0FP36mulpjs4mulp6stream19file__writer__write(_M0L6writerS1146, _M0L5chunkS1151);
      moonbit_decref(_M0L5chunkS1151);
      if (!_M0L6_2atmpS3122) {
        int32_t _M0L6_2atmpS3123;
        moonbit_string_t _M0L6_2atmpS3125;
        void* _M0L6_2atmpS3124;
        void* _block_4054;
        moonbit_decref(_M0L6streamS1153);
        #line 71 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
        _M0L6_2atmpS3123
        = _M0FP36mulpjs4mulp6stream19file__writer__close(_M0L6writerS1146);
        if (_M0L6writerS1146) {
          moonbit_decref(_M0L6writerS1146);
        }
        #line 72 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
        _M0L6_2atmpS3125
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L4pathS1147);
        moonbit_decref(_M0L4pathS1147);
        #line 72 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
        _M0L6_2atmpS3124
        = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3125);
        _block_4054
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
        Moonbit_object_header(_block_4054)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
        ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4054)->$0
        = _M0L6_2atmpS3124;
        return _block_4054;
      }
      joinlet_4050:;
      goto joinlet_4049;
      join_1148:;
      #line 81 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
      _M0L6_2atmpS3121
      = _M0FP36mulpjs4mulp6stream19file__writer__close(_M0L6writerS1146);
      if (_M0L6writerS1146) {
        moonbit_decref(_M0L6writerS1146);
      }
      _block_4055
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_4055)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4055)->$0
      = _M0L3errS1149;
      return _block_4055;
      joinlet_4049:;
      continue;
      break;
    }
    _M0L6_2atmpS3129 = 0;
    _block_4056
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4056)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4056)->$0
    = _M0L6_2atmpS3129;
    return _block_4056;
  }
}

void* _M0FP36mulpjs4mulp6stream17write__real__file(
  moonbit_string_t _M0L4pathS1144,
  moonbit_string_t _M0L8contentsS1145
) {
  int32_t _M0L6_2atmpS3114;
  struct _M0TPC16string10StringView _M0L6_2atmpS3113;
  moonbit_bytes_t _M0L6_2atmpS3109;
  int32_t _M0L6_2atmpS3112;
  struct _M0TPC16string10StringView _M0L6_2atmpS3111;
  moonbit_bytes_t _M0L6_2atmpS3110;
  int32_t _result_4057;
  #line 35 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3114 = Moonbit_array_length(_M0L4pathS1144);
  moonbit_incref(_M0L4pathS1144);
  _M0L6_2atmpS3113
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3114, _M0L4pathS1144
  };
  #line 39 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3109
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3113, 0);
  _M0L6_2atmpS3112 = Moonbit_array_length(_M0L8contentsS1145);
  _M0L6_2atmpS3111
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3112, _M0L8contentsS1145
  };
  #line 39 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3110
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3111, 0);
  #line 39 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _result_4057
  = _M0FP36mulpjs4mulp6stream18write__file__bytes(_M0L6_2atmpS3109, _M0L6_2atmpS3110);
  moonbit_decref(_M0L6_2atmpS3109);
  moonbit_decref(_M0L6_2atmpS3110);
  if (_result_4057) {
    int32_t _M0L6_2atmpS3115;
    void* _block_4058;
    moonbit_decref(_M0L4pathS1144);
    _M0L6_2atmpS3115 = 0;
    _block_4058
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4058)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4058)->$0
    = _M0L6_2atmpS3115;
    return _block_4058;
  } else {
    moonbit_string_t _M0L6_2atmpS3117;
    void* _M0L6_2atmpS3116;
    void* _block_4059;
    #line 42 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3117
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L4pathS1144);
    moonbit_decref(_M0L4pathS1144);
    #line 42 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3116
    = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3117);
    _block_4059
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4059)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4059)->$0
    = _M0L6_2atmpS3116;
    return _block_4059;
  }
}

void* _M0FP36mulpjs4mulp6stream19write__real__buffer(
  moonbit_string_t _M0L4pathS1142,
  moonbit_bytes_t _M0L8contentsS1143
) {
  int32_t _M0L6_2atmpS3105;
  struct _M0TPC16string10StringView _M0L6_2atmpS3104;
  moonbit_bytes_t _M0L6_2atmpS3103;
  int32_t _result_4060;
  #line 47 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3105 = Moonbit_array_length(_M0L4pathS1142);
  moonbit_incref(_M0L4pathS1142);
  _M0L6_2atmpS3104
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3105, _M0L4pathS1142
  };
  #line 51 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3103
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3104, 0);
  #line 51 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _result_4060
  = _M0FP36mulpjs4mulp6stream18write__file__bytes(_M0L6_2atmpS3103, _M0L8contentsS1143);
  moonbit_decref(_M0L6_2atmpS3103);
  moonbit_decref(_M0L8contentsS1143);
  if (_result_4060) {
    int32_t _M0L6_2atmpS3106;
    void* _block_4061;
    moonbit_decref(_M0L4pathS1142);
    _M0L6_2atmpS3106 = 0;
    _block_4061
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4061)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok) >> 2, 0, 1);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4061)->$0
    = _M0L6_2atmpS3106;
    return _block_4061;
  } else {
    moonbit_string_t _M0L6_2atmpS3108;
    void* _M0L6_2atmpS3107;
    void* _block_4062;
    #line 54 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3108
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L4pathS1142);
    moonbit_decref(_M0L4pathS1142);
    #line 54 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3107
    = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3108);
    _block_4062
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4062)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGuRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4062)->$0
    = _M0L6_2atmpS3107;
    return _block_4062;
  }
}

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp6stream22with__real__dest__path(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1140,
  moonbit_string_t _M0L4pathS1141
) {
  moonbit_string_t _M0L3cwdS3098;
  moonbit_string_t _M0L4baseS3099;
  void* _M0L8contentsS3100;
  struct _M0TPB5ArrayGUssEE* _M0L8metadataS3101;
  moonbit_string_t _M0L6_2atmpS3102;
  struct _M0TP36mulpjs4mulp6stream4File* _block_4063;
  #line 23 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L3cwdS3098 = _M0L4fileS1140->$0;
  _M0L4baseS3099 = _M0L4fileS1140->$1;
  _M0L8contentsS3100 = _M0L4fileS1140->$3;
  _M0L8metadataS3101 = _M0L4fileS1140->$4;
  moonbit_incref(_M0L8metadataS3101);
  moonbit_incref(_M0L8contentsS3100);
  moonbit_incref(_M0L4baseS3099);
  moonbit_incref(_M0L3cwdS3098);
  #line 30 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L6_2atmpS3102
  = _M0MP36mulpjs4mulp6stream4File11source__map(_M0L4fileS1140);
  _block_4063
  = (struct _M0TP36mulpjs4mulp6stream4File*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream4File));
  Moonbit_object_header(_block_4063)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream4File, $0) >> 2, 6, 0);
  _block_4063->$0 = _M0L3cwdS3098;
  _block_4063->$1 = _M0L4baseS3099;
  _block_4063->$2 = _M0L4pathS1141;
  _block_4063->$3 = _M0L8contentsS3100;
  _block_4063->$4 = _M0L8metadataS3101;
  _block_4063->$5 = _M0L6_2atmpS3102;
  return _block_4063;
}

moonbit_string_t _M0FP36mulpjs4mulp6stream24resolve__real__dest__dir(
  moonbit_string_t _M0L8out__dirS1133,
  moonbit_string_t _M0L3cwdS1137
) {
  moonbit_string_t _M0L7_2abindS1134;
  int32_t _M0L6_2atmpS3097;
  struct _M0TPC16string10StringView _M0L6_2atmpS3096;
  #line 11 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L7_2abindS1134 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3097 = Moonbit_array_length(_M0L7_2abindS1134);
  _M0L6_2atmpS3096
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3097, _M0L7_2abindS1134
  };
  moonbit_incref(_M0L8out__dirS1133);
  #line 12 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  if (
    _M0MPC16string6String11has__prefix(_M0L8out__dirS1133, _M0L6_2atmpS3096)
  ) {
    if (_M0L3cwdS1137) {
      moonbit_decref(_M0L3cwdS1137);
    }
    return _M0L8out__dirS1133;
  } else {
    moonbit_string_t _M0L4rootS1136;
    if (_M0L3cwdS1137 == 0) {
      if (_M0L3cwdS1137) {
        moonbit_decref(_M0L3cwdS1137);
      }
      return _M0L8out__dirS1133;
    } else {
      moonbit_string_t _M0L7_2aSomeS1138 = _M0L3cwdS1137;
      moonbit_string_t _M0L7_2arootS1139 = _M0L7_2aSomeS1138;
      _M0L4rootS1136 = _M0L7_2arootS1139;
      goto join_1135;
    }
    join_1135:;
    #line 16 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    return _M0FP36mulpjs4mulp6stream22join__real__dest__path(_M0L4rootS1136, _M0L8out__dirS1133);
  }
}

moonbit_string_t _M0FP36mulpjs4mulp6stream22join__real__dest__path(
  moonbit_string_t _M0L3dirS1130,
  moonbit_string_t _M0L3relS1132
) {
  moonbit_string_t _M0L7_2abindS1131;
  int32_t _M0L6_2atmpS3094;
  struct _M0TPC16string10StringView _M0L6_2atmpS3093;
  #line 2 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  _M0L7_2abindS1131 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3094 = Moonbit_array_length(_M0L7_2abindS1131);
  _M0L6_2atmpS3093
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3094, _M0L7_2abindS1131
  };
  moonbit_incref(_M0L3dirS1130);
  #line 3 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
  if (_M0MPC16string6String11has__suffix(_M0L3dirS1130, _M0L6_2atmpS3093)) {
    moonbit_string_t _result_4065;
    #line 4 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _result_4065 = moonbit_add_string(_M0L3dirS1130, _M0L3relS1132);
    moonbit_decref(_M0L3relS1132);
    moonbit_decref(_M0L3dirS1130);
    return _result_4065;
  } else {
    moonbit_string_t _M0L6_2atmpS3095;
    moonbit_string_t _result_4066;
    #line 6 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _M0L6_2atmpS3095
    = moonbit_add_string(_M0L3dirS1130, (moonbit_string_t)moonbit_string_literal_38.data);
    moonbit_decref(_M0L3dirS1130);
    #line 6 "/Users/user/workspace/github/gulp/mulp/stream/file_dest.mbt"
    _result_4066 = moonbit_add_string(_M0L6_2atmpS3095, _M0L3relS1132);
    moonbit_decref(_M0L3relS1132);
    moonbit_decref(_M0L6_2atmpS3095);
    return _result_4066;
  }
}

void* _M0FP36mulpjs4mulp6stream18file__byte__stream(
  moonbit_string_t _M0L4pathS1128,
  int32_t _M0L11chunk__sizeS1129
) {
  struct _M0TWEu* _M0L6_2atmpS3090;
  #line 5 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  _M0L6_2atmpS3090
  = (struct _M0TWEu*)&_M0FP36mulpjs4mulp6stream18file__byte__streamC3091l9$closure.data;
  #line 9 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  return _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__check(_M0L4pathS1128, _M0L11chunk__sizeS1129, _M0L6_2atmpS3090);
}

int32_t _M0FP36mulpjs4mulp6stream18file__byte__streamC3091l9(
  struct _M0TWEu* _M0L6_2aenvS3092
) {
  #line 9 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  moonbit_decref(_M0L6_2aenvS3092);
  return 0;
}

void* _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__check(
  moonbit_string_t _M0L4pathS1125,
  int32_t _M0L11chunk__sizeS1123,
  struct _M0TWEu* _M0L13is__cancelledS1126
) {
  #line 24 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  if (_M0L11chunk__sizeS1123 <= 0) {
    void* _M0L6_2atmpS3070;
    void* _block_4067;
    moonbit_decref(_M0L13is__cancelledS1126);
    moonbit_decref(_M0L4pathS1125);
    #line 30 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    _M0L6_2atmpS3070
    = _M0FP36mulpjs4mulp4core13stream__error((moonbit_string_t)moonbit_string_literal_39.data);
    _block_4067
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4067)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4067)->$0
    = _M0L6_2atmpS3070;
    return _block_4067;
  } else {
    int32_t _M0L6_2atmpS3089 = Moonbit_array_length(_M0L4pathS1125);
    struct _M0TPC16string10StringView _M0L6_2atmpS3088;
    moonbit_bytes_t _M0L6_2atmpS3087;
    void* _M0L6readerS1124;
    int32_t _M0L6_2atmpS3071;
    moonbit_incref(_M0L4pathS1125);
    _M0L6_2atmpS3088
    = (struct _M0TPC16string10StringView){
      0, _M0L6_2atmpS3089, _M0L4pathS1125
    };
    #line 32 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    _M0L6_2atmpS3087
    = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3088, 0);
    #line 32 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    _M0L6readerS1124
    = _M0FP36mulpjs4mulp6stream18open__file__reader(_M0L6_2atmpS3087, _M0L11chunk__sizeS1123);
    moonbit_decref(_M0L6_2atmpS3087);
    #line 33 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    _M0L6_2atmpS3071
    = _M0FP36mulpjs4mulp6stream22file__reader__is__open(_M0L6readerS1124);
    if (!_M0L6_2atmpS3071) {
      moonbit_string_t _M0L6_2atmpS3073;
      void* _M0L6_2atmpS3072;
      void* _block_4068;
      moonbit_decref(_M0L13is__cancelledS1126);
      if (_M0L6readerS1124) {
        moonbit_decref(_M0L6readerS1124);
      }
      #line 34 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
      _M0L6_2atmpS3073
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_40.data, _M0L4pathS1125);
      moonbit_decref(_M0L4pathS1125);
      #line 34 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
      _M0L6_2atmpS3072
      = _M0FP36mulpjs4mulp4core19file__system__error(_M0L6_2atmpS3073);
      _block_4068
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_4068)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4068)->$0
      = _M0L6_2atmpS3072;
      return _block_4068;
    } else {
      struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__* _closure_4069;
      struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS3075;
      struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__* _closure_4070;
      struct _M0TWEu* _M0L6_2atmpS3076;
      struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L6_2atmpS3074;
      void* _block_4071;
      moonbit_decref(_M0L4pathS1125);
      if (_M0L6readerS1124) {
        moonbit_incref(_M0L6readerS1124);
      }
      _closure_4069
      = (struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__*)moonbit_malloc(sizeof(struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__));
      Moonbit_object_header(_closure_4069)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__, $0) >> 2, 2, 0);
      _closure_4069->code
      = &_M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__checkC3080l38;
      _closure_4069->$0 = _M0L13is__cancelledS1126;
      _closure_4069->$1 = _M0L6readerS1124;
      _M0L6_2atmpS3075
      = (struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*)_closure_4069;
      _closure_4070
      = (struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__*)moonbit_malloc(sizeof(struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__));
      Moonbit_object_header(_closure_4070)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__, $0) >> 2, 1, 0);
      _closure_4070->code
      = &_M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__checkC3077l50;
      _closure_4070->$0 = _M0L6readerS1124;
      _M0L6_2atmpS3076 = (struct _M0TWEu*)_closure_4070;
      #line 37 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
      _M0L6_2atmpS3074
      = _M0FP36mulpjs4mulp6stream37byte__stream__from__pull__with__close(_M0L6_2atmpS3075, _M0L6_2atmpS3076);
      _block_4071
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok));
      Moonbit_object_header(_block_4071)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
      ((struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4071)->$0
      = _M0L6_2atmpS3074;
      return _block_4071;
    }
  }
}

void* _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__checkC3080l38(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS3081
) {
  struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__* _M0L14_2acasted__envS3082;
  void* _M0L6readerS1124;
  struct _M0TWEu* _M0L8_2afieldS3467;
  int32_t _M0L6_2acntS3868;
  struct _M0TWEu* _M0L13is__cancelledS1126;
  #line 38 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  _M0L14_2acasted__envS3082
  = (struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3080__l38__*)_M0L6_2aenvS3081;
  _M0L6readerS1124 = _M0L14_2acasted__envS3082->$1;
  _M0L8_2afieldS3467 = _M0L14_2acasted__envS3082->$0;
  _M0L6_2acntS3868 = Moonbit_object_header(_M0L14_2acasted__envS3082)->rc;
  if (_M0L6_2acntS3868 > 1) {
    int32_t _M0L11_2anew__cntS3869 = _M0L6_2acntS3868 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3082)->rc
    = _M0L11_2anew__cntS3869;
    if (_M0L6readerS1124) {
      moonbit_incref(_M0L6readerS1124);
    }
    moonbit_incref(_M0L8_2afieldS3467);
  } else if (_M0L6_2acntS3868 == 1) {
    #line 38 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS3082);
  }
  _M0L13is__cancelledS1126 = _M0L8_2afieldS3467;
  #line 39 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  if (_M0L13is__cancelledS1126->code(_M0L13is__cancelledS1126)) {
    void* _M0L6_2atmpS3083;
    void* _block_4072;
    if (_M0L6readerS1124) {
      moonbit_decref(_M0L6readerS1124);
    }
    #line 40 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    _M0L6_2atmpS3083
    = _M0FP36mulpjs4mulp4core12task__failed((moonbit_string_t)moonbit_string_literal_41.data, (moonbit_string_t)moonbit_string_literal_42.data);
    _block_4072
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4072)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4072)->$0
    = _M0L6_2atmpS3083;
    return _block_4072;
  } else {
    moonbit_bytes_t _M0L5chunkS1127;
    int32_t _M0L6_2atmpS3084;
    #line 42 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    _M0L5chunkS1127
    = _M0FP36mulpjs4mulp6stream18file__reader__read(_M0L6readerS1124);
    if (_M0L6readerS1124) {
      moonbit_decref(_M0L6readerS1124);
    }
    _M0L6_2atmpS3084 = Moonbit_array_length(_M0L5chunkS1127);
    if (_M0L6_2atmpS3084 == 0) {
      moonbit_bytes_t _M0L6_2atmpS3085;
      void* _block_4073;
      moonbit_decref(_M0L5chunkS1127);
      _M0L6_2atmpS3085 = 0;
      _block_4073
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok));
      Moonbit_object_header(_block_4073)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
      ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4073)->$0
      = _M0L6_2atmpS3085;
      return _block_4073;
    } else {
      moonbit_bytes_t _M0L6_2atmpS3086 = _M0L5chunkS1127;
      void* _block_4074 =
        (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok));
      Moonbit_object_header(_block_4074)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
      ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4074)->$0
      = _M0L6_2atmpS3086;
      return _block_4074;
    }
  }
}

int32_t _M0FP36mulpjs4mulp6stream39file__byte__stream__with__cancel__checkC3077l50(
  struct _M0TWEu* _M0L6_2aenvS3078
) {
  struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__* _M0L14_2acasted__envS3079;
  void* _M0L8_2afieldS3469;
  int32_t _M0L6_2acntS3870;
  void* _M0L6readerS1124;
  #line 50 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  _M0L14_2acasted__envS3079
  = (struct _M0R88_24mulpjs_2fmulp_2fstream_2efile__byte__stream__with__cancel__check_2eanon__u3077__l50__*)_M0L6_2aenvS3078;
  _M0L8_2afieldS3469 = _M0L14_2acasted__envS3079->$0;
  _M0L6_2acntS3870 = Moonbit_object_header(_M0L14_2acasted__envS3079)->rc;
  if (_M0L6_2acntS3870 > 1) {
    int32_t _M0L11_2anew__cntS3871 = _M0L6_2acntS3870 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3079)->rc
    = _M0L11_2anew__cntS3871;
    if (_M0L8_2afieldS3469) {
      moonbit_incref(_M0L8_2afieldS3469);
    }
  } else if (_M0L6_2acntS3870 == 1) {
    #line 50 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS3079);
  }
  _M0L6readerS1124 = _M0L8_2afieldS3469;
  #line 50 "/Users/user/workspace/github/gulp/mulp/stream/file_byte_stream.mbt"
  _M0FP36mulpjs4mulp6stream19file__reader__close(_M0L6readerS1124);
  if (_M0L6readerS1124) {
    moonbit_decref(_M0L6readerS1124);
  }
  return 0;
}

int32_t _M0FP36mulpjs4mulp6stream19file__reader__close(
  void* _M0L8_2aparamS1364
) {
  mulp_file_reader_close(_M0L8_2aparamS1364);
  return 0;
}

void* _M0MP36mulpjs4mulp6stream10FileStream7collect(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS1110,
  struct _M0TP36mulpjs4mulp4core7Context* _M0L3ctxS1109
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS3069;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS1108;
  void* _block_4081;
  #line 374 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS3069
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_empty_ref_array;
  _M0L5filesS1108
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE));
  Moonbit_object_header(_M0L5filesS1108)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE, $0) >> 2, 1, 0);
  _M0L5filesS1108->$0 = _M0L6_2atmpS3069;
  _M0L5filesS1108->$1 = 0;
  while (1) {
    void* _M0L3errS1112;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1114;
    void* _M0L7_2abindS1115;
    void* _block_4080;
    moonbit_incref(_M0L3ctxS1109);
    #line 380 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    if (_M0MP36mulpjs4mulp4core7Context13is__cancelled(_M0L3ctxS1109)) {
      void* _M0L6_2atmpS3068;
      void* _block_4076;
      moonbit_decref(_M0L3ctxS1109);
      moonbit_decref(_M0L5filesS1108);
      #line 381 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS1110);
      #line 382 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0L6_2atmpS3068
      = _M0FP36mulpjs4mulp4core12task__failed((moonbit_string_t)moonbit_string_literal_41.data, (moonbit_string_t)moonbit_string_literal_42.data);
      _block_4076
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_4076)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4076)->$0
      = _M0L6_2atmpS3068;
      return _block_4076;
    }
    moonbit_incref(_M0L4selfS1110);
    #line 384 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2abindS1115
    = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L4selfS1110);
    switch (Moonbit_object_tag(_M0L7_2abindS1115)) {
      case 1: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1116 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1115;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS3470 =
          _M0L5_2aOkS1116->$0;
        int32_t _M0L6_2acntS3872 = Moonbit_object_header(_M0L5_2aOkS1116)->rc;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS1117;
        if (_M0L6_2acntS3872 > 1) {
          int32_t _M0L11_2anew__cntS3873 = _M0L6_2acntS3872 - 1;
          Moonbit_object_header(_M0L5_2aOkS1116)->rc = _M0L11_2anew__cntS3873;
          if (_M0L8_2afieldS3470) {
            moonbit_incref(_M0L8_2afieldS3470);
          }
        } else if (_M0L6_2acntS3872 == 1) {
          #line 384 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L5_2aOkS1116);
        }
        _M0L4_2axS1117 = _M0L8_2afieldS3470;
        if (_M0L4_2axS1117 == 0) {
          void* _block_4079;
          if (_M0L4_2axS1117) {
            moonbit_decref(_M0L4_2axS1117);
          }
          moonbit_decref(_M0L4selfS1110);
          moonbit_decref(_M0L3ctxS1109);
          _block_4079
          = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
          Moonbit_object_header(_block_4079)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
          ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4079)->$0
          = _M0L5filesS1108;
          return _block_4079;
        } else {
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS1118 =
            _M0L4_2axS1117;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS1119 =
            _M0L7_2aSomeS1118;
          _M0L4fileS1114 = _M0L7_2afileS1119;
          goto join_1113;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1120;
        void* _M0L8_2afieldS3471;
        int32_t _M0L6_2acntS3874;
        void* _M0L6_2aerrS1121;
        moonbit_decref(_M0L3ctxS1109);
        moonbit_decref(_M0L5filesS1108);
        _M0L6_2aErrS1120
        = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1115;
        _M0L8_2afieldS3471 = _M0L6_2aErrS1120->$0;
        _M0L6_2acntS3874 = Moonbit_object_header(_M0L6_2aErrS1120)->rc;
        if (_M0L6_2acntS3874 > 1) {
          int32_t _M0L11_2anew__cntS3875 = _M0L6_2acntS3874 - 1;
          Moonbit_object_header(_M0L6_2aErrS1120)->rc
          = _M0L11_2anew__cntS3875;
          moonbit_incref(_M0L8_2afieldS3471);
        } else if (_M0L6_2acntS3874 == 1) {
          #line 384 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L6_2aErrS1120);
        }
        _M0L6_2aerrS1121 = _M0L8_2afieldS3471;
        _M0L3errS1112 = _M0L6_2aerrS1121;
        goto join_1111;
        break;
      }
    }
    goto joinlet_4078;
    join_1113:;
    moonbit_incref(_M0L5filesS1108);
    #line 385 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0MPC15array5Array4pushGRP36mulpjs4mulp6stream4FileE(_M0L5filesS1108, _M0L4fileS1114);
    joinlet_4078:;
    goto joinlet_4077;
    join_1111:;
    #line 388 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS1110);
    _block_4080
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4080)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4080)->$0
    = _M0L3errS1112;
    return _block_4080;
    joinlet_4077:;
    continue;
    break;
  }
  _block_4081
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_4081)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRPB5ArrayGRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4081)->$0
  = _M0L5filesS1108;
  return _block_4081;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0MP36mulpjs4mulp6stream10FileStream4pipe(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS1072,
  struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L9transformS1073
) {
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3067;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L5stateS1071;
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__* _closure_4082;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS3051;
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__* _closure_4083;
  struct _M0TWEu* _M0L6_2atmpS3052;
  #line 190 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS3067 = 0;
  _M0L5stateS1071
  = (struct _M0TP36mulpjs4mulp6stream9PipeState*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream9PipeState));
  Moonbit_object_header(_M0L5stateS1071)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream9PipeState, $0) >> 2, 3, 0);
  _M0L5stateS1071->$0 = _M0L4selfS1072;
  _M0L5stateS1071->$1 = _M0L9transformS1073;
  _M0L5stateS1071->$2 = _M0L6_2atmpS3067;
  moonbit_incref(_M0L5stateS1071);
  _closure_4082
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__*)moonbit_malloc(sizeof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__));
  Moonbit_object_header(_closure_4082)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__, $0) >> 2, 1, 0);
  _closure_4082->code = &_M0MP36mulpjs4mulp6stream10FileStream4pipeC3057l193;
  _closure_4082->$0 = _M0L5stateS1071;
  _M0L6_2atmpS3051
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_4082;
  _closure_4083
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__*)moonbit_malloc(sizeof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__));
  Moonbit_object_header(_closure_4083)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__, $0) >> 2, 1, 0);
  _closure_4083->code = &_M0MP36mulpjs4mulp6stream10FileStream4pipeC3053l215;
  _closure_4083->$0 = _M0L5stateS1071;
  _M0L6_2atmpS3052 = (struct _M0TWEu*)_closure_4083;
  #line 192 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L6_2atmpS3051, _M0L6_2atmpS3052);
}

void* _M0MP36mulpjs4mulp6stream10FileStream4pipeC3057l193(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS3058
) {
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__* _M0L14_2acasted__envS3059;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L8_2afieldS3481;
  int32_t _M0L6_2acntS3876;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L5stateS1071;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3066;
  void* _block_4094;
  #line 193 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS3059
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3057__l193__*)_M0L6_2aenvS3058;
  _M0L8_2afieldS3481 = _M0L14_2acasted__envS3059->$0;
  _M0L6_2acntS3876 = Moonbit_object_header(_M0L14_2acasted__envS3059)->rc;
  if (_M0L6_2acntS3876 > 1) {
    int32_t _M0L11_2anew__cntS3877 = _M0L6_2acntS3876 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3059)->rc
    = _M0L11_2anew__cntS3877;
    moonbit_incref(_M0L8_2afieldS3481);
  } else if (_M0L6_2acntS3876 == 1) {
    #line 193 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS3059);
  }
  _M0L5stateS1071 = _M0L8_2afieldS3481;
  while (1) {
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7currentS1075;
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2abindS1087 =
      _M0L5stateS1071->$2;
    void* _M0L3errS1077;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1079;
    void* _M0L7_2abindS1080;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3060;
    void* _block_4092;
    void* _block_4093;
    if (_M0L7_2abindS1087 == 0) {
      void* _M0L3errS1091;
      struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1093;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS3065 =
        _M0L5stateS1071->$0;
      void* _M0L7_2abindS1095;
      struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L7_2afuncS1094;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3063;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3062;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2aoldS3475;
      void* _block_4089;
      moonbit_incref(_M0L8upstreamS3065);
      #line 206 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0L7_2abindS1095
      = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L8upstreamS3065);
      switch (Moonbit_object_tag(_M0L7_2abindS1095)) {
        case 1: {
          struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1096 =
            (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1095;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS3477 =
            _M0L5_2aOkS1096->$0;
          int32_t _M0L6_2acntS3882 =
            Moonbit_object_header(_M0L5_2aOkS1096)->rc;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS1097;
          if (_M0L6_2acntS3882 > 1) {
            int32_t _M0L11_2anew__cntS3883 = _M0L6_2acntS3882 - 1;
            Moonbit_object_header(_M0L5_2aOkS1096)->rc
            = _M0L11_2anew__cntS3883;
            if (_M0L8_2afieldS3477) {
              moonbit_incref(_M0L8_2afieldS3477);
            }
          } else if (_M0L6_2acntS3882 == 1) {
            #line 206 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
            moonbit_free(_M0L5_2aOkS1096);
          }
          _M0L4_2axS1097 = _M0L8_2afieldS3477;
          if (_M0L4_2axS1097 == 0) {
            struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3064;
            void* _block_4088;
            if (_M0L4_2axS1097) {
              moonbit_decref(_M0L4_2axS1097);
            }
            moonbit_decref(_M0L5stateS1071);
            _M0L6_2atmpS3064 = 0;
            _block_4088
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
            Moonbit_object_header(_block_4088)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
            ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4088)->$0
            = _M0L6_2atmpS3064;
            return _block_4088;
          } else {
            struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS1098 =
              _M0L4_2axS1097;
            struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS1099 =
              _M0L7_2aSomeS1098;
            _M0L4fileS1093 = _M0L7_2afileS1099;
            goto join_1092;
          }
          break;
        }
        default: {
          struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1100;
          void* _M0L8_2afieldS3478;
          int32_t _M0L6_2acntS3884;
          void* _M0L6_2aerrS1101;
          moonbit_decref(_M0L5stateS1071);
          _M0L6_2aErrS1100
          = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1095;
          _M0L8_2afieldS3478 = _M0L6_2aErrS1100->$0;
          _M0L6_2acntS3884 = Moonbit_object_header(_M0L6_2aErrS1100)->rc;
          if (_M0L6_2acntS3884 > 1) {
            int32_t _M0L11_2anew__cntS3885 = _M0L6_2acntS3884 - 1;
            Moonbit_object_header(_M0L6_2aErrS1100)->rc
            = _M0L11_2anew__cntS3885;
            moonbit_incref(_M0L8_2afieldS3478);
          } else if (_M0L6_2acntS3884 == 1) {
            #line 206 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
            moonbit_free(_M0L6_2aErrS1100);
          }
          _M0L6_2aerrS1101 = _M0L8_2afieldS3478;
          _M0L3errS1091 = _M0L6_2aerrS1101;
          goto join_1090;
          break;
        }
      }
      goto joinlet_4087;
      join_1092:;
      _M0L7_2afuncS1094 = _M0L5stateS1071->$1;
      moonbit_incref(_M0L7_2afuncS1094);
      #line 207 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      _M0L6_2atmpS3063
      = _M0L7_2afuncS1094->code(_M0L7_2afuncS1094, _M0L4fileS1093);
      _M0L6_2atmpS3062 = _M0L6_2atmpS3063;
      _M0L6_2aoldS3475 = _M0L5stateS1071->$2;
      if (_M0L6_2aoldS3475) {
        moonbit_decref(_M0L6_2aoldS3475);
      }
      _M0L5stateS1071->$2 = _M0L6_2atmpS3062;
      joinlet_4087:;
      goto joinlet_4086;
      join_1090:;
      _block_4089
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
      Moonbit_object_header(_block_4089)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
      ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4089)->$0
      = _M0L3errS1091;
      return _block_4089;
      joinlet_4086:;
    } else {
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2aSomeS1088 =
        _M0L7_2abindS1087;
      struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L10_2acurrentS1089 =
        _M0L7_2aSomeS1088;
      moonbit_incref(_M0L10_2acurrentS1089);
      _M0L7currentS1075 = _M0L10_2acurrentS1089;
      goto join_1074;
    }
    goto joinlet_4085;
    join_1074:;
    moonbit_incref(_M0L7currentS1075);
    #line 197 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2abindS1080
    = _M0MP36mulpjs4mulp6stream10FileStream4next(_M0L7currentS1075);
    switch (Moonbit_object_tag(_M0L7_2abindS1080)) {
      case 1: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1081 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1080;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS3473 =
          _M0L5_2aOkS1081->$0;
        int32_t _M0L6_2acntS3878 = Moonbit_object_header(_M0L5_2aOkS1081)->rc;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS1082;
        if (_M0L6_2acntS3878 > 1) {
          int32_t _M0L11_2anew__cntS3879 = _M0L6_2acntS3878 - 1;
          Moonbit_object_header(_M0L5_2aOkS1081)->rc = _M0L11_2anew__cntS3879;
          if (_M0L8_2afieldS3473) {
            moonbit_incref(_M0L8_2afieldS3473);
          }
        } else if (_M0L6_2acntS3878 == 1) {
          #line 197 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L5_2aOkS1081);
        }
        _M0L4_2axS1082 = _M0L8_2afieldS3473;
        if (_M0L4_2axS1082 == 0) {
          struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2atmpS3061;
          struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L6_2aoldS3472;
          if (_M0L4_2axS1082) {
            moonbit_decref(_M0L4_2axS1082);
          }
          #line 200 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L7currentS1075);
          _M0L6_2atmpS3061 = 0;
          _M0L6_2aoldS3472 = _M0L5stateS1071->$2;
          if (_M0L6_2aoldS3472) {
            moonbit_decref(_M0L6_2aoldS3472);
          }
          _M0L5stateS1071->$2 = _M0L6_2atmpS3061;
        } else {
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2aSomeS1083;
          struct _M0TP36mulpjs4mulp6stream4File* _M0L7_2afileS1084;
          moonbit_decref(_M0L7currentS1075);
          moonbit_decref(_M0L5stateS1071);
          _M0L7_2aSomeS1083 = _M0L4_2axS1082;
          _M0L7_2afileS1084 = _M0L7_2aSomeS1083;
          _M0L4fileS1079 = _M0L7_2afileS1084;
          goto join_1078;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1085;
        void* _M0L8_2afieldS3474;
        int32_t _M0L6_2acntS3880;
        void* _M0L6_2aerrS1086;
        moonbit_decref(_M0L7currentS1075);
        moonbit_decref(_M0L5stateS1071);
        _M0L6_2aErrS1085
        = (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1080;
        _M0L8_2afieldS3474 = _M0L6_2aErrS1085->$0;
        _M0L6_2acntS3880 = Moonbit_object_header(_M0L6_2aErrS1085)->rc;
        if (_M0L6_2acntS3880 > 1) {
          int32_t _M0L11_2anew__cntS3881 = _M0L6_2acntS3880 - 1;
          Moonbit_object_header(_M0L6_2aErrS1085)->rc
          = _M0L11_2anew__cntS3881;
          moonbit_incref(_M0L8_2afieldS3474);
        } else if (_M0L6_2acntS3880 == 1) {
          #line 197 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L6_2aErrS1085);
        }
        _M0L6_2aerrS1086 = _M0L8_2afieldS3474;
        _M0L3errS1077 = _M0L6_2aerrS1086;
        goto join_1076;
        break;
      }
    }
    goto joinlet_4091;
    join_1078:;
    _M0L6_2atmpS3060 = _M0L4fileS1079;
    _block_4092
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4092)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4092)->$0
    = _M0L6_2atmpS3060;
    return _block_4092;
    joinlet_4091:;
    goto joinlet_4090;
    join_1076:;
    _block_4093
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4093)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4093)->$0
    = _M0L3errS1077;
    return _block_4093;
    joinlet_4090:;
    joinlet_4085:;
    continue;
    break;
  }
  _M0L6_2atmpS3066 = 0;
  _block_4094
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
  Moonbit_object_header(_block_4094)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4094)->$0
  = _M0L6_2atmpS3066;
  return _block_4094;
}

int32_t _M0MP36mulpjs4mulp6stream10FileStream4pipeC3053l215(
  struct _M0TWEu* _M0L6_2aenvS3054
) {
  struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__* _M0L14_2acasted__envS3055;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L8_2afieldS3484;
  int32_t _M0L6_2acntS3886;
  struct _M0TP36mulpjs4mulp6stream9PipeState* _M0L5stateS1071;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7currentS1104;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2abindS1105;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS3482;
  int32_t _M0L6_2acntS3888;
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8upstreamS3056;
  #line 215 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS3055
  = (struct _M0R70_40mulpjs_2fmulp_2fstream_2eFileStream_3a_3apipe_2eanon__u3053__l215__*)_M0L6_2aenvS3054;
  _M0L8_2afieldS3484 = _M0L14_2acasted__envS3055->$0;
  _M0L6_2acntS3886 = Moonbit_object_header(_M0L14_2acasted__envS3055)->rc;
  if (_M0L6_2acntS3886 > 1) {
    int32_t _M0L11_2anew__cntS3887 = _M0L6_2acntS3886 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3055)->rc
    = _M0L11_2anew__cntS3887;
    moonbit_incref(_M0L8_2afieldS3484);
  } else if (_M0L6_2acntS3886 == 1) {
    #line 215 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS3055);
  }
  _M0L5stateS1071 = _M0L8_2afieldS3484;
  _M0L7_2abindS1105 = _M0L5stateS1071->$2;
  if (_M0L7_2abindS1105 == 0) {
    
  } else {
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L7_2aSomeS1106 =
      _M0L7_2abindS1105;
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L10_2acurrentS1107 =
      _M0L7_2aSomeS1106;
    moonbit_incref(_M0L10_2acurrentS1107);
    _M0L7currentS1104 = _M0L10_2acurrentS1107;
    goto join_1103;
  }
  goto joinlet_4095;
  join_1103:;
  #line 217 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L7currentS1104);
  joinlet_4095:;
  _M0L8_2afieldS3482 = _M0L5stateS1071->$0;
  _M0L6_2acntS3888 = Moonbit_object_header(_M0L5stateS1071)->rc;
  if (_M0L6_2acntS3888 > 1) {
    int32_t _M0L11_2anew__cntS3891 = _M0L6_2acntS3888 - 1;
    Moonbit_object_header(_M0L5stateS1071)->rc = _M0L11_2anew__cntS3891;
    moonbit_incref(_M0L8_2afieldS3482);
  } else if (_M0L6_2acntS3888 == 1) {
    struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS3890 =
      _M0L5stateS1071->$2;
    struct _M0TWRP36mulpjs4mulp6stream4FileERP36mulpjs4mulp6stream10FileStream* _M0L8_2afieldS3889;
    if (_M0L8_2afieldS3890) {
      moonbit_decref(_M0L8_2afieldS3890);
    }
    _M0L8_2afieldS3889 = _M0L5stateS1071->$1;
    moonbit_decref(_M0L8_2afieldS3889);
    #line 220 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L5stateS1071);
  }
  _M0L8upstreamS3056 = _M0L8_2afieldS3482;
  #line 220 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L8upstreamS3056);
  return 0;
}

void* _M0MP36mulpjs4mulp6stream10FileStream4next(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS1060
) {
  #line 163 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  if (_M0L4selfS1060->$2) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3049;
    void* _block_4096;
    moonbit_decref(_M0L4selfS1060);
    _M0L6_2atmpS3049 = 0;
    _block_4096
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4096)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4096)->$0
    = _M0L6_2atmpS3049;
    return _block_4096;
  } else {
    void* _M0L3errS1062;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1064;
    struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L7_2afuncS1066 =
      _M0L4selfS1060->$0;
    void* _M0L7_2abindS1065;
    void* _block_4100;
    void* _block_4101;
    moonbit_incref(_M0L7_2afuncS1066);
    #line 167 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2abindS1065 = _M0L7_2afuncS1066->code(_M0L7_2afuncS1066);
    switch (Moonbit_object_tag(_M0L7_2abindS1065)) {
      case 1: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1067 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1065;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L8_2afieldS3485 =
          _M0L5_2aOkS1067->$0;
        int32_t _M0L6_2acntS3892 = Moonbit_object_header(_M0L5_2aOkS1067)->rc;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L4_2axS1068;
        if (_M0L6_2acntS3892 > 1) {
          int32_t _M0L11_2anew__cntS3893 = _M0L6_2acntS3892 - 1;
          Moonbit_object_header(_M0L5_2aOkS1067)->rc = _M0L11_2anew__cntS3893;
          if (_M0L8_2afieldS3485) {
            moonbit_incref(_M0L8_2afieldS3485);
          }
        } else if (_M0L6_2acntS3892 == 1) {
          #line 167 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L5_2aOkS1067);
        }
        _M0L4_2axS1068 = _M0L8_2afieldS3485;
        if (_M0L4_2axS1068 == 0) {
          struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3050;
          void* _block_4099;
          if (_M0L4_2axS1068) {
            moonbit_decref(_M0L4_2axS1068);
          }
          #line 169 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS1060);
          _M0L6_2atmpS3050 = 0;
          _block_4099
          = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
          Moonbit_object_header(_block_4099)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
          ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4099)->$0
          = _M0L6_2atmpS3050;
          return _block_4099;
        } else {
          moonbit_decref(_M0L4selfS1060);
          _M0L4fileS1064 = _M0L4_2axS1068;
          goto join_1063;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1069 =
          (struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1065;
        void* _M0L8_2afieldS3486 = _M0L6_2aErrS1069->$0;
        int32_t _M0L6_2acntS3894 =
          Moonbit_object_header(_M0L6_2aErrS1069)->rc;
        void* _M0L6_2aerrS1070;
        if (_M0L6_2acntS3894 > 1) {
          int32_t _M0L11_2anew__cntS3895 = _M0L6_2acntS3894 - 1;
          Moonbit_object_header(_M0L6_2aErrS1069)->rc
          = _M0L11_2anew__cntS3895;
          moonbit_incref(_M0L8_2afieldS3486);
        } else if (_M0L6_2acntS3894 == 1) {
          #line 167 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
          moonbit_free(_M0L6_2aErrS1069);
        }
        _M0L6_2aerrS1070 = _M0L8_2afieldS3486;
        _M0L3errS1062 = _M0L6_2aerrS1070;
        goto join_1061;
        break;
      }
    }
    join_1063:;
    _block_4100
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4100)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4100)->$0
    = _M0L4fileS1064;
    return _block_4100;
    join_1061:;
    #line 174 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0MP36mulpjs4mulp6stream10FileStream5close(_M0L4selfS1060);
    _block_4101
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4101)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4101)->$0
    = _M0L3errS1062;
    return _block_4101;
  }
}

int32_t _M0MP36mulpjs4mulp6stream10FileStream5close(
  struct _M0TP36mulpjs4mulp6stream10FileStream* _M0L4selfS1058
) {
  int32_t _M0L6closedS3048;
  #line 182 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6closedS3048 = _M0L4selfS1058->$2;
  if (!_M0L6closedS3048) {
    struct _M0TWEu* _M0L8_2afieldS3488;
    int32_t _M0L6_2acntS3896;
    struct _M0TWEu* _M0L7_2afuncS1059;
    _M0L4selfS1058->$2 = 1;
    _M0L8_2afieldS3488 = _M0L4selfS1058->$1;
    _M0L6_2acntS3896 = Moonbit_object_header(_M0L4selfS1058)->rc;
    if (_M0L6_2acntS3896 > 1) {
      int32_t _M0L11_2anew__cntS3898 = _M0L6_2acntS3896 - 1;
      Moonbit_object_header(_M0L4selfS1058)->rc = _M0L11_2anew__cntS3898;
      moonbit_incref(_M0L8_2afieldS3488);
    } else if (_M0L6_2acntS3896 == 1) {
      struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS3897 =
        _M0L4selfS1058->$0;
      moonbit_decref(_M0L8_2afieldS3897);
      #line 185 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
      moonbit_free(_M0L4selfS1058);
    }
    _M0L7_2afuncS1059 = _M0L8_2afieldS3488;
    #line 185 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L7_2afuncS1059->code(_M0L7_2afuncS1059);
  } else {
    moonbit_decref(_M0L4selfS1058);
  }
  return 0;
}

moonbit_string_t _M0MP36mulpjs4mulp6stream4File14relative__path(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4selfS1056
) {
  moonbit_string_t _M0L4pathS3042;
  moonbit_string_t _M0L4baseS3043;
  #line 81 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L4pathS3042 = _M0L4selfS1056->$2;
  _M0L4baseS3043 = _M0L4selfS1056->$1;
  moonbit_incref(_M0L4baseS3043);
  moonbit_incref(_M0L4pathS3042);
  #line 82 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  if (
    _M0FP36mulpjs4mulp6stream22path__is__inside__base(_M0L4pathS3042, _M0L4baseS3043)
  ) {
    moonbit_string_t _M0L4pathS3045 = _M0L4selfS1056->$2;
    moonbit_string_t _M0L8_2afieldS3489 = _M0L4selfS1056->$1;
    int32_t _M0L6_2acntS3899 = Moonbit_object_header(_M0L4selfS1056)->rc;
    moonbit_string_t _M0L4baseS3047;
    int32_t _M0L6_2atmpS3046;
    struct _M0TPC16string10StringView _M0L6_2atmpS3044;
    moonbit_string_t _M0L3rawS1057;
    if (_M0L6_2acntS3899 > 1) {
      int32_t _M0L11_2anew__cntS3904 = _M0L6_2acntS3899 - 1;
      Moonbit_object_header(_M0L4selfS1056)->rc = _M0L11_2anew__cntS3904;
      moonbit_incref(_M0L4pathS3045);
      moonbit_incref(_M0L8_2afieldS3489);
    } else if (_M0L6_2acntS3899 == 1) {
      moonbit_string_t _M0L8_2afieldS3903 = _M0L4selfS1056->$5;
      struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3902;
      void* _M0L8_2afieldS3901;
      moonbit_string_t _M0L8_2afieldS3900;
      if (_M0L8_2afieldS3903) {
        moonbit_decref(_M0L8_2afieldS3903);
      }
      _M0L8_2afieldS3902 = _M0L4selfS1056->$4;
      moonbit_decref(_M0L8_2afieldS3902);
      _M0L8_2afieldS3901 = _M0L4selfS1056->$3;
      moonbit_decref(_M0L8_2afieldS3901);
      _M0L8_2afieldS3900 = _M0L4selfS1056->$0;
      moonbit_decref(_M0L8_2afieldS3900);
      #line 83 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
      moonbit_free(_M0L4selfS1056);
    }
    _M0L4baseS3047 = _M0L8_2afieldS3489;
    _M0L6_2atmpS3046 = Moonbit_array_length(_M0L4baseS3047);
    moonbit_decref(_M0L4baseS3047);
    #line 83 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    _M0L6_2atmpS3044
    = _M0MPC16string6String11sub_2einner(_M0L4pathS3045, _M0L6_2atmpS3046, 4294967296ll);
    #line 83 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    _M0L3rawS1057 = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS3044);
    #line 84 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    return _M0FP36mulpjs4mulp6stream20trim__leading__slash(_M0L3rawS1057);
  } else {
    moonbit_string_t _M0L8_2afieldS3491 = _M0L4selfS1056->$2;
    int32_t _M0L6_2acntS3905 = Moonbit_object_header(_M0L4selfS1056)->rc;
    if (_M0L6_2acntS3905 > 1) {
      int32_t _M0L11_2anew__cntS3911 = _M0L6_2acntS3905 - 1;
      Moonbit_object_header(_M0L4selfS1056)->rc = _M0L11_2anew__cntS3911;
      moonbit_incref(_M0L8_2afieldS3491);
    } else if (_M0L6_2acntS3905 == 1) {
      moonbit_string_t _M0L8_2afieldS3910 = _M0L4selfS1056->$5;
      struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3909;
      void* _M0L8_2afieldS3908;
      moonbit_string_t _M0L8_2afieldS3907;
      moonbit_string_t _M0L8_2afieldS3906;
      if (_M0L8_2afieldS3910) {
        moonbit_decref(_M0L8_2afieldS3910);
      }
      _M0L8_2afieldS3909 = _M0L4selfS1056->$4;
      moonbit_decref(_M0L8_2afieldS3909);
      _M0L8_2afieldS3908 = _M0L4selfS1056->$3;
      moonbit_decref(_M0L8_2afieldS3908);
      _M0L8_2afieldS3907 = _M0L4selfS1056->$1;
      moonbit_decref(_M0L8_2afieldS3907);
      _M0L8_2afieldS3906 = _M0L4selfS1056->$0;
      moonbit_decref(_M0L8_2afieldS3906);
      #line 86 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
      moonbit_free(_M0L4selfS1056);
    }
    return _M0L8_2afieldS3491;
  }
}

moonbit_string_t _M0FP36mulpjs4mulp6stream20trim__leading__slash(
  moonbit_string_t _M0L4pathS1054
) {
  moonbit_string_t _M0L7_2abindS1055;
  int32_t _M0L6_2atmpS3040;
  struct _M0TPC16string10StringView _M0L6_2atmpS3039;
  #line 54 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L7_2abindS1055 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3040 = Moonbit_array_length(_M0L7_2abindS1055);
  _M0L6_2atmpS3039
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3040, _M0L7_2abindS1055
  };
  moonbit_incref(_M0L4pathS1054);
  #line 55 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  if (_M0MPC16string6String11has__prefix(_M0L4pathS1054, _M0L6_2atmpS3039)) {
    struct _M0TPC16string10StringView _M0L6_2atmpS3041;
    #line 56 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    _M0L6_2atmpS3041
    = _M0MPC16string6String11sub_2einner(_M0L4pathS1054, 1, 4294967296ll);
    #line 56 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    return _M0MPC16string10StringView9to__owned(_M0L6_2atmpS3041);
  } else {
    return _M0L4pathS1054;
  }
}

int32_t _M0FP36mulpjs4mulp6stream22path__is__inside__base(
  moonbit_string_t _M0L4pathS1051,
  moonbit_string_t _M0L4baseS1052
) {
  #line 63 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  #line 64 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  if (moonbit_val_array_equal(_M0L4pathS1051, _M0L4baseS1052)) {
    moonbit_decref(_M0L4baseS1052);
    moonbit_decref(_M0L4pathS1051);
    return 1;
  } else {
    moonbit_string_t _M0L7_2abindS1053;
    int32_t _M0L6_2atmpS3038;
    struct _M0TPC16string10StringView _M0L6_2atmpS3037;
    #line 64 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    _M0L7_2abindS1053
    = moonbit_add_string(_M0L4baseS1052, (moonbit_string_t)moonbit_string_literal_38.data);
    moonbit_decref(_M0L4baseS1052);
    _M0L6_2atmpS3038 = Moonbit_array_length(_M0L7_2abindS1053);
    _M0L6_2atmpS3037
    = (struct _M0TPC16string10StringView){
      0, _M0L6_2atmpS3038, _M0L7_2abindS1053
    };
    #line 64 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    return _M0MPC16string6String11has__prefix(_M0L4pathS1051, _M0L6_2atmpS3037);
  }
}

void* _M0MP36mulpjs4mulp6stream10ByteStream10read__next(
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L4selfS1038
) {
  #line 56 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  if (_M0L4selfS1038->$2) {
    void* _M0L6_2atmpS3034;
    void* _block_4102;
    moonbit_decref(_M0L4selfS1038);
    #line 60 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L6_2atmpS3034
    = _M0FP36mulpjs4mulp4core13stream__error((moonbit_string_t)moonbit_string_literal_43.data);
    _block_4102
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4102)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4102)->$0
    = _M0L6_2atmpS3034;
    return _block_4102;
  } else {
    void* _M0L3errS1040;
    moonbit_bytes_t _M0L5chunkS1042;
    struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L7_2afuncS1044 =
      _M0L4selfS1038->$0;
    void* _M0L7_2abindS1043;
    moonbit_bytes_t _M0L6_2atmpS3035;
    void* _block_4106;
    void* _block_4107;
    moonbit_incref(_M0L7_2afuncS1044);
    #line 62 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L7_2abindS1043 = _M0L7_2afuncS1044->code(_M0L7_2afuncS1044);
    switch (Moonbit_object_tag(_M0L7_2abindS1043)) {
      case 1: {
        struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS1045 =
          (struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L7_2abindS1043;
        moonbit_bytes_t _M0L8_2afieldS3494 = _M0L5_2aOkS1045->$0;
        int32_t _M0L6_2acntS3912 = Moonbit_object_header(_M0L5_2aOkS1045)->rc;
        moonbit_bytes_t _M0L4_2axS1046;
        if (_M0L6_2acntS3912 > 1) {
          int32_t _M0L11_2anew__cntS3913 = _M0L6_2acntS3912 - 1;
          Moonbit_object_header(_M0L5_2aOkS1045)->rc = _M0L11_2anew__cntS3913;
          if (_M0L8_2afieldS3494) {
            moonbit_incref(_M0L8_2afieldS3494);
          }
        } else if (_M0L6_2acntS3912 == 1) {
          #line 62 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
          moonbit_free(_M0L5_2aOkS1045);
        }
        _M0L4_2axS1046 = _M0L8_2afieldS3494;
        if (_M0L4_2axS1046 == 0) {
          moonbit_bytes_t _M0L6_2atmpS3036;
          void* _block_4105;
          if (_M0L4_2axS1046) {
            moonbit_decref(_M0L4_2axS1046);
          }
          #line 65 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
          _M0MP36mulpjs4mulp6stream10ByteStream5close(_M0L4selfS1038);
          _M0L6_2atmpS3036 = 0;
          _block_4105
          = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok));
          Moonbit_object_header(_block_4105)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
          ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4105)->$0
          = _M0L6_2atmpS3036;
          return _block_4105;
        } else {
          moonbit_bytes_t _M0L7_2aSomeS1047;
          moonbit_bytes_t _M0L8_2achunkS1048;
          moonbit_decref(_M0L4selfS1038);
          _M0L7_2aSomeS1047 = _M0L4_2axS1046;
          _M0L8_2achunkS1048 = _M0L7_2aSomeS1047;
          _M0L5chunkS1042 = _M0L8_2achunkS1048;
          goto join_1041;
        }
        break;
      }
      default: {
        struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err* _M0L6_2aErrS1049 =
          (struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err*)_M0L7_2abindS1043;
        void* _M0L8_2afieldS3495 = _M0L6_2aErrS1049->$0;
        int32_t _M0L6_2acntS3914 =
          Moonbit_object_header(_M0L6_2aErrS1049)->rc;
        void* _M0L6_2aerrS1050;
        if (_M0L6_2acntS3914 > 1) {
          int32_t _M0L11_2anew__cntS3915 = _M0L6_2acntS3914 - 1;
          Moonbit_object_header(_M0L6_2aErrS1049)->rc
          = _M0L11_2anew__cntS3915;
          moonbit_incref(_M0L8_2afieldS3495);
        } else if (_M0L6_2acntS3914 == 1) {
          #line 62 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
          moonbit_free(_M0L6_2aErrS1049);
        }
        _M0L6_2aerrS1050 = _M0L8_2afieldS3495;
        _M0L3errS1040 = _M0L6_2aerrS1050;
        goto join_1039;
        break;
      }
    }
    join_1041:;
    _M0L6_2atmpS3035 = _M0L5chunkS1042;
    _block_4106
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4106)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4106)->$0
    = _M0L6_2atmpS3035;
    return _block_4106;
    join_1039:;
    #line 69 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0MP36mulpjs4mulp6stream10ByteStream5close(_M0L4selfS1038);
    _block_4107
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err));
    Moonbit_object_header(_block_4107)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE3Err*)_block_4107)->$0
    = _M0L3errS1040;
    return _block_4107;
  }
}

int32_t _M0MP36mulpjs4mulp6stream10ByteStream5close(
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L4selfS1036
) {
  int32_t _M0L8consumedS3033;
  #line 77 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L8consumedS3033 = _M0L4selfS1036->$2;
  if (!_M0L8consumedS3033) {
    struct _M0TWEu* _M0L8_2afieldS3497;
    int32_t _M0L6_2acntS3916;
    struct _M0TWEu* _M0L7_2afuncS1037;
    _M0L4selfS1036->$2 = 1;
    _M0L8_2afieldS3497 = _M0L4selfS1036->$1;
    _M0L6_2acntS3916 = Moonbit_object_header(_M0L4selfS1036)->rc;
    if (_M0L6_2acntS3916 > 1) {
      int32_t _M0L11_2anew__cntS3918 = _M0L6_2acntS3916 - 1;
      Moonbit_object_header(_M0L4selfS1036)->rc = _M0L11_2anew__cntS3918;
      moonbit_incref(_M0L8_2afieldS3497);
    } else if (_M0L6_2acntS3916 == 1) {
      struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L8_2afieldS3917 =
        _M0L4selfS1036->$0;
      moonbit_decref(_M0L8_2afieldS3917);
      #line 80 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
      moonbit_free(_M0L4selfS1036);
    }
    _M0L7_2afuncS1037 = _M0L8_2afieldS3497;
    #line 80 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L7_2afuncS1037->code(_M0L7_2afuncS1037);
  } else {
    moonbit_decref(_M0L4selfS1036);
  }
  return 0;
}

moonbit_string_t _M0MP36mulpjs4mulp6stream4File11source__map(
  struct _M0TP36mulpjs4mulp6stream4File* _M0L4selfS1035
) {
  moonbit_string_t _M0L8_2afieldS3498;
  int32_t _M0L6_2acntS3919;
  #line 150 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L8_2afieldS3498 = _M0L4selfS1035->$5;
  _M0L6_2acntS3919 = Moonbit_object_header(_M0L4selfS1035)->rc;
  if (_M0L6_2acntS3919 > 1) {
    int32_t _M0L11_2anew__cntS3925 = _M0L6_2acntS3919 - 1;
    Moonbit_object_header(_M0L4selfS1035)->rc = _M0L11_2anew__cntS3925;
    if (_M0L8_2afieldS3498) {
      moonbit_incref(_M0L8_2afieldS3498);
    }
  } else if (_M0L6_2acntS3919 == 1) {
    struct _M0TPB5ArrayGUssEE* _M0L8_2afieldS3924 = _M0L4selfS1035->$4;
    void* _M0L8_2afieldS3923;
    moonbit_string_t _M0L8_2afieldS3922;
    moonbit_string_t _M0L8_2afieldS3921;
    moonbit_string_t _M0L8_2afieldS3920;
    moonbit_decref(_M0L8_2afieldS3924);
    _M0L8_2afieldS3923 = _M0L4selfS1035->$3;
    moonbit_decref(_M0L8_2afieldS3923);
    _M0L8_2afieldS3922 = _M0L4selfS1035->$2;
    moonbit_decref(_M0L8_2afieldS3922);
    _M0L8_2afieldS3921 = _M0L4selfS1035->$1;
    moonbit_decref(_M0L8_2afieldS3921);
    _M0L8_2afieldS3920 = _M0L4selfS1035->$0;
    moonbit_decref(_M0L8_2afieldS3920);
    #line 151 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
    moonbit_free(_M0L4selfS1035);
  }
  return _M0L8_2afieldS3498;
}

struct _M0TP36mulpjs4mulp6stream4File* _M0FP36mulpjs4mulp6stream4file(
  moonbit_string_t _M0L3cwdS1031,
  moonbit_string_t _M0L4baseS1032,
  moonbit_string_t _M0L4pathS1033,
  void* _M0L8contentsS1034
) {
  struct _M0TUssE** _M0L6_2atmpS3032;
  struct _M0TPB5ArrayGUssEE* _M0L6_2atmpS3030;
  moonbit_string_t _M0L6_2atmpS3031;
  struct _M0TP36mulpjs4mulp6stream4File* _block_4108;
  #line 22 "/Users/user/workspace/github/gulp/mulp/stream/file.mbt"
  _M0L6_2atmpS3032 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3030
  = (struct _M0TPB5ArrayGUssEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUssEE));
  Moonbit_object_header(_M0L6_2atmpS3030)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUssEE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3030->$0 = _M0L6_2atmpS3032;
  _M0L6_2atmpS3030->$1 = 0;
  _M0L6_2atmpS3031 = 0;
  _block_4108
  = (struct _M0TP36mulpjs4mulp6stream4File*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream4File));
  Moonbit_object_header(_block_4108)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream4File, $0) >> 2, 6, 0);
  _block_4108->$0 = _M0L3cwdS1031;
  _block_4108->$1 = _M0L4baseS1032;
  _block_4108->$2 = _M0L4pathS1033;
  _block_4108->$3 = _M0L8contentsS1034;
  _block_4108->$4 = _M0L6_2atmpS3030;
  _block_4108->$5 = _M0L6_2atmpS3031;
  return _block_4108;
}

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FP36mulpjs4mulp6stream12byte__stream(
  struct _M0TPB5ArrayGsE* _M0L6chunksS1028
) {
  struct _M0TPB8MutLocalGiE* _M0L5indexS1027;
  struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__* _closure_4109;
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS3017;
  #line 42 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L5indexS1027
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5indexS1027)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5indexS1027->$0 = 0;
  _closure_4109
  = (struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__*)moonbit_malloc(sizeof(struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__));
  Moonbit_object_header(_closure_4109)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__, $0) >> 2, 2, 0);
  _closure_4109->code = &_M0FP36mulpjs4mulp6stream12byte__streamC3018l44;
  _closure_4109->$0 = _M0L6chunksS1028;
  _closure_4109->$1 = _M0L5indexS1027;
  _M0L6_2atmpS3017
  = (struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE*)_closure_4109;
  #line 44 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  return _M0FP36mulpjs4mulp6stream24byte__stream__from__pull(_M0L6_2atmpS3017);
}

void* _M0FP36mulpjs4mulp6stream12byte__streamC3018l44(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS3019
) {
  struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__* _M0L14_2acasted__envS3020;
  struct _M0TPB8MutLocalGiE* _M0L5indexS1027;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS3499;
  int32_t _M0L6_2acntS3926;
  struct _M0TPB5ArrayGsE* _M0L6chunksS1028;
  int32_t _M0L3valS3021;
  int32_t _M0L6_2atmpS3022;
  #line 44 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L14_2acasted__envS3020
  = (struct _M0R61_24mulpjs_2fmulp_2fstream_2ebyte__stream_2eanon__u3018__l44__*)_M0L6_2aenvS3019;
  _M0L5indexS1027 = _M0L14_2acasted__envS3020->$1;
  _M0L8_2afieldS3499 = _M0L14_2acasted__envS3020->$0;
  _M0L6_2acntS3926 = Moonbit_object_header(_M0L14_2acasted__envS3020)->rc;
  if (_M0L6_2acntS3926 > 1) {
    int32_t _M0L11_2anew__cntS3927 = _M0L6_2acntS3926 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3020)->rc
    = _M0L11_2anew__cntS3927;
    moonbit_incref(_M0L5indexS1027);
    moonbit_incref(_M0L8_2afieldS3499);
  } else if (_M0L6_2acntS3926 == 1) {
    #line 44 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS3020);
  }
  _M0L6chunksS1028 = _M0L8_2afieldS3499;
  _M0L3valS3021 = _M0L5indexS1027->$0;
  moonbit_incref(_M0L6chunksS1028);
  #line 45 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L6_2atmpS3022 = _M0MPC15array5Array6lengthGsE(_M0L6chunksS1028);
  if (_M0L3valS3021 >= _M0L6_2atmpS3022) {
    moonbit_bytes_t _M0L6_2atmpS3023;
    void* _block_4110;
    moonbit_decref(_M0L6chunksS1028);
    moonbit_decref(_M0L5indexS1027);
    _M0L6_2atmpS3023 = 0;
    _block_4110
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4110)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4110)->$0
    = _M0L6_2atmpS3023;
    return _block_4110;
  } else {
    int32_t _M0L3valS3029 = _M0L5indexS1027->$0;
    moonbit_string_t _M0L7_2abindS1030;
    int32_t _M0L6_2atmpS3028;
    struct _M0TPC16string10StringView _M0L6_2atmpS3027;
    moonbit_bytes_t _M0L5chunkS1029;
    int32_t _M0L3valS3025;
    int32_t _M0L6_2atmpS3024;
    moonbit_bytes_t _M0L6_2atmpS3026;
    void* _block_4111;
    #line 48 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L7_2abindS1030
    = _M0MPC15array5Array2atGsE(_M0L6chunksS1028, _M0L3valS3029);
    _M0L6_2atmpS3028 = Moonbit_array_length(_M0L7_2abindS1030);
    _M0L6_2atmpS3027
    = (struct _M0TPC16string10StringView){
      0, _M0L6_2atmpS3028, _M0L7_2abindS1030
    };
    #line 48 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
    _M0L5chunkS1029
    = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3027, 0);
    _M0L3valS3025 = _M0L5indexS1027->$0;
    _M0L6_2atmpS3024 = _M0L3valS3025 + 1;
    _M0L5indexS1027->$0 = _M0L6_2atmpS3024;
    moonbit_decref(_M0L5indexS1027);
    _M0L6_2atmpS3026 = _M0L5chunkS1029;
    _block_4111
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4111)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4111)->$0
    = _M0L6_2atmpS3026;
    return _block_4111;
  }
}

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FP36mulpjs4mulp6stream24byte__stream__from__pull(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L4pullS1026
) {
  struct _M0TWEu* _M0L6_2atmpS3014;
  #line 15 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _M0L6_2atmpS3014
  = (struct _M0TWEu*)&_M0FP36mulpjs4mulp6stream24byte__stream__from__pullC3015l16$closure.data;
  #line 16 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  return _M0FP36mulpjs4mulp6stream37byte__stream__from__pull__with__close(_M0L4pullS1026, _M0L6_2atmpS3014);
}

int32_t _M0FP36mulpjs4mulp6stream24byte__stream__from__pullC3015l16(
  struct _M0TWEu* _M0L6_2aenvS3016
) {
  #line 16 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  moonbit_decref(_M0L6_2aenvS3016);
  return 0;
}

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FP36mulpjs4mulp6stream37byte__stream__from__pull__with__close(
  struct _M0TWERPC16result6ResultGOzRP36mulpjs4mulp4core9MulpErrorE* _M0L4pullS1024,
  struct _M0TWEu* _M0L5closeS1025
) {
  struct _M0TP36mulpjs4mulp6stream10ByteStream* _block_4112;
  #line 20 "/Users/user/workspace/github/gulp/mulp/stream/byte_stream.mbt"
  _block_4112
  = (struct _M0TP36mulpjs4mulp6stream10ByteStream*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream10ByteStream));
  Moonbit_object_header(_block_4112)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream10ByteStream, $0) >> 2, 2, 0);
  _block_4112->$0 = _M0L4pullS1024;
  _block_4112->$1 = _M0L5closeS1025;
  _block_4112->$2 = 0;
  return _block_4112;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream3src(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS1023
) {
  #line 2 "/Users/user/workspace/github/gulp/mulp/stream/memory_io.mbt"
  #line 3 "/Users/user/workspace/github/gulp/mulp/stream/memory_io.mbt"
  return _M0FP36mulpjs4mulp6stream12file__stream(_M0L5filesS1023);
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream12file__stream(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS1021
) {
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* _M0L5stateS1020;
  struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__* _closure_4113;
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2atmpS3001;
  #line 127 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L5stateS1020
  = (struct _M0TP36mulpjs4mulp6stream14ArrayFileState*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream14ArrayFileState));
  Moonbit_object_header(_M0L5stateS1020)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream14ArrayFileState, $0) >> 2, 1, 0);
  _M0L5stateS1020->$0 = _M0L5filesS1021;
  _M0L5stateS1020->$1 = 0;
  _closure_4113
  = (struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__*)moonbit_malloc(sizeof(struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__));
  Moonbit_object_header(_closure_4113)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__, $0) >> 2, 1, 0);
  _closure_4113->code = &_M0FP36mulpjs4mulp6stream12file__streamC3002l129;
  _closure_4113->$0 = _M0L5stateS1020;
  _M0L6_2atmpS3001
  = (struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE*)_closure_4113;
  #line 129 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream24file__stream__from__pull(_M0L6_2atmpS3001);
}

void* _M0FP36mulpjs4mulp6stream12file__streamC3002l129(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L6_2aenvS3003
) {
  struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__* _M0L14_2acasted__envS3004;
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* _M0L8_2afieldS3503;
  int32_t _M0L6_2acntS3928;
  struct _M0TP36mulpjs4mulp6stream14ArrayFileState* _M0L5stateS1020;
  int32_t _M0L5indexS3005;
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS3007;
  int32_t _M0L6_2atmpS3006;
  #line 129 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L14_2acasted__envS3004
  = (struct _M0R62_24mulpjs_2fmulp_2fstream_2efile__stream_2eanon__u3002__l129__*)_M0L6_2aenvS3003;
  _M0L8_2afieldS3503 = _M0L14_2acasted__envS3004->$0;
  _M0L6_2acntS3928 = Moonbit_object_header(_M0L14_2acasted__envS3004)->rc;
  if (_M0L6_2acntS3928 > 1) {
    int32_t _M0L11_2anew__cntS3929 = _M0L6_2acntS3928 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3004)->rc
    = _M0L11_2anew__cntS3929;
    moonbit_incref(_M0L8_2afieldS3503);
  } else if (_M0L6_2acntS3928 == 1) {
    #line 129 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    moonbit_free(_M0L14_2acasted__envS3004);
  }
  _M0L5stateS1020 = _M0L8_2afieldS3503;
  _M0L5indexS3005 = _M0L5stateS1020->$1;
  _M0L5filesS3007 = _M0L5stateS1020->$0;
  moonbit_incref(_M0L5filesS3007);
  #line 130 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS3006
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(_M0L5filesS3007);
  if (_M0L5indexS3005 >= _M0L6_2atmpS3006) {
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3008;
    void* _block_4114;
    moonbit_decref(_M0L5stateS1020);
    _M0L6_2atmpS3008 = 0;
    _block_4114
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4114)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4114)->$0
    = _M0L6_2atmpS3008;
    return _block_4114;
  } else {
    struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L5filesS3012 =
      _M0L5stateS1020->$0;
    int32_t _M0L5indexS3013 = _M0L5stateS1020->$1;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L4fileS1022;
    int32_t _M0L5indexS3010;
    int32_t _M0L6_2atmpS3009;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3011;
    void* _block_4115;
    moonbit_incref(_M0L5filesS3012);
    #line 133 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
    _M0L4fileS1022
    = _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(_M0L5filesS3012, _M0L5indexS3013);
    _M0L5indexS3010 = _M0L5stateS1020->$1;
    _M0L6_2atmpS3009 = _M0L5indexS3010 + 1;
    _M0L5stateS1020->$1 = _M0L6_2atmpS3009;
    moonbit_decref(_M0L5stateS1020);
    _M0L6_2atmpS3011 = _M0L4fileS1022;
    _block_4115
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok));
    Moonbit_object_header(_block_4115)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok, $0) >> 2, 1, 1);
    ((struct _M0DTPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE2Ok*)_block_4115)->$0
    = _M0L6_2atmpS3011;
    return _block_4115;
  }
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream24file__stream__from__pull(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L4pullS1019
) {
  struct _M0TWEu* _M0L6_2atmpS2998;
  #line 54 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _M0L6_2atmpS2998
  = (struct _M0TWEu*)&_M0FP36mulpjs4mulp6stream24file__stream__from__pullC2999l55$closure.data;
  #line 55 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  return _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(_M0L4pullS1019, _M0L6_2atmpS2998);
}

int32_t _M0FP36mulpjs4mulp6stream24file__stream__from__pullC2999l55(
  struct _M0TWEu* _M0L6_2aenvS3000
) {
  #line 55 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  moonbit_decref(_M0L6_2aenvS3000);
  return 0;
}

struct _M0TP36mulpjs4mulp6stream10FileStream* _M0FP36mulpjs4mulp6stream37file__stream__from__pull__with__close(
  struct _M0TWERPC16result6ResultGORP36mulpjs4mulp6stream4FileRP36mulpjs4mulp4core9MulpErrorE* _M0L4pullS1017,
  struct _M0TWEu* _M0L5closeS1018
) {
  struct _M0TP36mulpjs4mulp6stream10FileStream* _block_4116;
  #line 59 "/Users/user/workspace/github/gulp/mulp/stream/file_stream.mbt"
  _block_4116
  = (struct _M0TP36mulpjs4mulp6stream10FileStream*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp6stream10FileStream));
  Moonbit_object_header(_block_4116)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp6stream10FileStream, $0) >> 2, 2, 0);
  _block_4116->$0 = _M0L4pullS1017;
  _block_4116->$1 = _M0L5closeS1018;
  _block_4116->$2 = 0;
  return _block_4116;
}

struct _M0TP36mulpjs4mulp4core7Context* _M0FP36mulpjs4mulp4core12new__context(
  moonbit_string_t _M0L3cwdS1014,
  int64_t _M0L7now__msS1015,
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS1016
) {
  struct _M0TP36mulpjs4mulp4core7Context* _block_4117;
  #line 29 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _block_4117
  = (struct _M0TP36mulpjs4mulp4core7Context*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp4core7Context));
  Moonbit_object_header(_block_4117)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp4core7Context, $0) >> 2, 2, 0);
  _block_4117->$0 = _M0L3cwdS1014;
  _block_4117->$1 = _M0L7now__msS1015;
  _block_4117->$2 = _M0L12cancellationS1016;
  return _block_4117;
}

struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0FP36mulpjs4mulp4core24new__cancellation__token(
  
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _block_4118;
  #line 7 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _block_4118
  = (struct _M0TP36mulpjs4mulp4core17CancellationToken*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp4core17CancellationToken));
  Moonbit_object_header(_block_4118)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP36mulpjs4mulp4core17CancellationToken) >> 2, 0, 0);
  _block_4118->$0 = 0;
  return _block_4118;
}

void* _M0FP36mulpjs4mulp4core13stream__error(
  moonbit_string_t _M0L6detailS1013
) {
  void* _block_4119;
  #line 44 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _block_4119
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError));
  Moonbit_object_header(_block_4119)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError, $0) >> 2, 1, 6);
  ((struct _M0DTP36mulpjs4mulp4core9MulpError11StreamError*)_block_4119)->$0
  = _M0L6detailS1013;
  return _block_4119;
}

void* _M0FP36mulpjs4mulp4core19file__system__error(
  moonbit_string_t _M0L6detailS1012
) {
  void* _block_4120;
  #line 29 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _block_4120
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError));
  Moonbit_object_header(_block_4120)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError, $0) >> 2, 1, 3);
  ((struct _M0DTP36mulpjs4mulp4core9MulpError15FileSystemError*)_block_4120)->$0
  = _M0L6detailS1012;
  return _block_4120;
}

int32_t _M0MP36mulpjs4mulp4core7Context13is__cancelled(
  struct _M0TP36mulpjs4mulp4core7Context* _M0L4selfS1011
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L8_2afieldS3504;
  int32_t _M0L6_2acntS3930;
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS2997;
  #line 38 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _M0L8_2afieldS3504 = _M0L4selfS1011->$2;
  _M0L6_2acntS3930 = Moonbit_object_header(_M0L4selfS1011)->rc;
  if (_M0L6_2acntS3930 > 1) {
    int32_t _M0L11_2anew__cntS3932 = _M0L6_2acntS3930 - 1;
    Moonbit_object_header(_M0L4selfS1011)->rc = _M0L11_2anew__cntS3932;
    moonbit_incref(_M0L8_2afieldS3504);
  } else if (_M0L6_2acntS3930 == 1) {
    moonbit_string_t _M0L8_2afieldS3931 = _M0L4selfS1011->$0;
    moonbit_decref(_M0L8_2afieldS3931);
    #line 39 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
    moonbit_free(_M0L4selfS1011);
  }
  _M0L12cancellationS2997 = _M0L8_2afieldS3504;
  #line 39 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  return _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(_M0L12cancellationS2997);
}

int32_t _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L4selfS1010
) {
  int32_t _result_4121;
  #line 17 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _result_4121 = _M0L4selfS1010->$0;
  moonbit_decref(_M0L4selfS1010);
  return _result_4121;
}

void* _M0FP36mulpjs4mulp4core12task__failed(
  moonbit_string_t _M0L4nameS1008,
  moonbit_string_t _M0L5causeS1009
) {
  void* _block_4122;
  #line 19 "/Users/user/workspace/github/gulp/mulp/core/error.mbt"
  _block_4122
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed));
  Moonbit_object_header(_block_4122)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed, $0) >> 2, 2, 1);
  ((struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed*)_block_4122)->$0
  = _M0L4nameS1008;
  ((struct _M0DTP36mulpjs4mulp4core9MulpError10TaskFailed*)_block_4122)->$1
  = _M0L5causeS1009;
  return _block_4122;
}

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS800,
  int32_t _M0L11ignore__bomS801
) {
  struct _M0TPC15bytes9BytesView _M0L5bytesS798;
  int32_t _M0L6_2atmpS2981;
  int32_t _M0L6_2atmpS2980;
  moonbit_bytes_t _M0L1tS806;
  int32_t _M0L4tlenS807;
  int32_t _M0L4tlenS808;
  struct _M0TPC15bytes9BytesView _M0L2bsS809;
  moonbit_bytes_t _M0L6_2atmpS2299;
  int64_t _M0L6_2atmpS2300;
  #line 132 "/Users/user/.moon/lib/core/encoding/utf8/decode.mbt"
  if (_M0L11ignore__bomS801) {
    int32_t _M0L3endS2983 = _M0L5bytesS800.$2;
    int32_t _M0L5startS2984 = _M0L5bytesS800.$1;
    int32_t _M0L6_2atmpS2982 = _M0L3endS2983 - _M0L5startS2984;
    if (_M0L6_2atmpS2982 >= 3) {
      moonbit_bytes_t _M0L5bytesS2995 = _M0L5bytesS800.$0;
      int32_t _M0L5startS2996 = _M0L5bytesS800.$1;
      int32_t _M0L4_2axS803 = _M0L5bytesS2995[_M0L5startS2996];
      if (_M0L4_2axS803 == 239) {
        moonbit_bytes_t _M0L5bytesS2992 = _M0L5bytesS800.$0;
        int32_t _M0L5startS2994 = _M0L5bytesS800.$1;
        int32_t _M0L6_2atmpS2993 = _M0L5startS2994 + 1;
        int32_t _M0L4_2axS804 = _M0L5bytesS2992[_M0L6_2atmpS2993];
        if (_M0L4_2axS804 == 187) {
          moonbit_bytes_t _M0L5bytesS2989 = _M0L5bytesS800.$0;
          int32_t _M0L5startS2991 = _M0L5bytesS800.$1;
          int32_t _M0L6_2atmpS2990 = _M0L5startS2991 + 2;
          int32_t _M0L4_2axS805 = _M0L5bytesS2989[_M0L6_2atmpS2990];
          if (_M0L4_2axS805 == 191) {
            moonbit_bytes_t _M0L5bytesS2985 = _M0L5bytesS800.$0;
            int32_t _M0L5startS2988 = _M0L5bytesS800.$1;
            int32_t _M0L6_2atmpS2986 = _M0L5startS2988 + 3;
            int32_t _M0L3endS2987 = _M0L5bytesS800.$2;
            _M0L5bytesS798
            = (struct _M0TPC15bytes9BytesView){
              _M0L6_2atmpS2986, _M0L3endS2987, _M0L5bytesS2985
            };
          } else {
            goto join_802;
          }
        } else {
          goto join_802;
        }
      } else {
        goto join_802;
      }
    } else {
      goto join_802;
    }
    goto joinlet_4124;
    join_802:;
    goto join_799;
    joinlet_4124:;
  } else {
    goto join_799;
  }
  goto joinlet_4123;
  join_799:;
  _M0L5bytesS798 = _M0L5bytesS800;
  joinlet_4123:;
  moonbit_incref(_M0L5bytesS798.$0);
  #line 138 "/Users/user/.moon/lib/core/encoding/utf8/decode.mbt"
  _M0L6_2atmpS2981 = _M0MPC15bytes9BytesView6length(_M0L5bytesS798);
  _M0L6_2atmpS2980 = _M0L6_2atmpS2981 * 2;
  _M0L1tS806 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2980, 0);
  _M0L4tlenS808 = 0;
  _M0L2bsS809 = _M0L5bytesS798;
  while (1) {
    int32_t _M0L4tlenS811;
    struct _M0TPC15bytes9BytesView _M0L4restS812;
    struct _M0TPC15bytes9BytesView _M0L4restS815;
    int32_t _M0L4tlenS816;
    struct _M0TPC15bytes9BytesView _M0L4restS818;
    int32_t _M0L4tlenS819;
    struct _M0TPC15bytes9BytesView _M0L4restS821;
    int32_t _M0L4tlenS822;
    int32_t _M0L4tlenS824;
    int32_t _M0L2b0S825;
    int32_t _M0L2b1S826;
    int32_t _M0L2b2S827;
    int32_t _M0L2b3S828;
    struct _M0TPC15bytes9BytesView _M0L4restS829;
    int32_t _M0L4tlenS835;
    int32_t _M0L2b0S836;
    int32_t _M0L2b1S837;
    int32_t _M0L2b2S838;
    struct _M0TPC15bytes9BytesView _M0L4restS839;
    int32_t _M0L4tlenS842;
    struct _M0TPC15bytes9BytesView _M0L4restS843;
    int32_t _M0L2b0S844;
    int32_t _M0L2b1S845;
    int32_t _M0L4tlenS848;
    struct _M0TPC15bytes9BytesView _M0L4restS849;
    int32_t _M0L1bS850;
    int32_t _M0L3endS2360 = _M0L2bsS809.$2;
    int32_t _M0L5startS2361 = _M0L2bsS809.$1;
    int32_t _M0L6_2atmpS2359 = _M0L3endS2360 - _M0L5startS2361;
    int32_t _M0L6_2atmpS2358;
    int32_t _M0L6_2atmpS2357;
    int32_t _M0L6_2atmpS2356;
    int32_t _M0L6_2atmpS2353;
    int32_t _M0L6_2atmpS2355;
    int32_t _M0L6_2atmpS2354;
    int32_t _M0L2chS846;
    int32_t _M0L6_2atmpS2348;
    int32_t _M0L6_2atmpS2349;
    int32_t _M0L6_2atmpS2351;
    int32_t _M0L6_2atmpS2350;
    int32_t _M0L6_2atmpS2352;
    int32_t _M0L6_2atmpS2347;
    int32_t _M0L6_2atmpS2346;
    int32_t _M0L6_2atmpS2342;
    int32_t _M0L6_2atmpS2345;
    int32_t _M0L6_2atmpS2344;
    int32_t _M0L6_2atmpS2343;
    int32_t _M0L6_2atmpS2339;
    int32_t _M0L6_2atmpS2341;
    int32_t _M0L6_2atmpS2340;
    int32_t _M0L2chS840;
    int32_t _M0L6_2atmpS2334;
    int32_t _M0L6_2atmpS2335;
    int32_t _M0L6_2atmpS2337;
    int32_t _M0L6_2atmpS2336;
    int32_t _M0L6_2atmpS2338;
    int32_t _M0L6_2atmpS2333;
    int32_t _M0L6_2atmpS2332;
    int32_t _M0L6_2atmpS2328;
    int32_t _M0L6_2atmpS2331;
    int32_t _M0L6_2atmpS2330;
    int32_t _M0L6_2atmpS2329;
    int32_t _M0L6_2atmpS2324;
    int32_t _M0L6_2atmpS2327;
    int32_t _M0L6_2atmpS2326;
    int32_t _M0L6_2atmpS2325;
    int32_t _M0L6_2atmpS2321;
    int32_t _M0L6_2atmpS2323;
    int32_t _M0L6_2atmpS2322;
    int32_t _M0L2chS830;
    int32_t _M0L3chmS831;
    int32_t _M0L6_2atmpS2320;
    int32_t _M0L3ch1S832;
    int32_t _M0L6_2atmpS2319;
    int32_t _M0L3ch2S833;
    int32_t _M0L6_2atmpS2309;
    int32_t _M0L6_2atmpS2310;
    int32_t _M0L6_2atmpS2312;
    int32_t _M0L6_2atmpS2311;
    int32_t _M0L6_2atmpS2313;
    int32_t _M0L6_2atmpS2314;
    int32_t _M0L6_2atmpS2315;
    int32_t _M0L6_2atmpS2317;
    int32_t _M0L6_2atmpS2316;
    int32_t _M0L6_2atmpS2318;
    int32_t _M0L6_2atmpS2307;
    int32_t _M0L6_2atmpS2308;
    int32_t _M0L6_2atmpS2305;
    int32_t _M0L6_2atmpS2306;
    int32_t _M0L6_2atmpS2303;
    int32_t _M0L6_2atmpS2304;
    int32_t _M0L6_2atmpS2301;
    int32_t _M0L6_2atmpS2302;
    int32_t _tmp_4134;
    struct _M0TPC15bytes9BytesView _tmp_4135;
    if (_M0L6_2atmpS2359 == 0) {
      moonbit_decref(_M0L2bsS809.$0);
      _M0L4tlenS807 = _M0L4tlenS808;
      break;
    } else {
      int32_t _M0L3endS2363 = _M0L2bsS809.$2;
      int32_t _M0L5startS2364 = _M0L2bsS809.$1;
      int32_t _M0L6_2atmpS2362 = _M0L3endS2363 - _M0L5startS2364;
      if (_M0L6_2atmpS2362 >= 8) {
        moonbit_bytes_t _M0L5bytesS2588 = _M0L2bsS809.$0;
        int32_t _M0L5startS2589 = _M0L2bsS809.$1;
        int32_t _M0L4_2axS851 = _M0L5bytesS2588[_M0L5startS2589];
        if (_M0L4_2axS851 <= 127) {
          moonbit_bytes_t _M0L5bytesS2585 = _M0L2bsS809.$0;
          int32_t _M0L5startS2587 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2586 = _M0L5startS2587 + 1;
          int32_t _M0L4_2axS852 = _M0L5bytesS2585[_M0L6_2atmpS2586];
          if (_M0L4_2axS852 <= 127) {
            moonbit_bytes_t _M0L5bytesS2582 = _M0L2bsS809.$0;
            int32_t _M0L5startS2584 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2583 = _M0L5startS2584 + 2;
            int32_t _M0L4_2axS853 = _M0L5bytesS2582[_M0L6_2atmpS2583];
            if (_M0L4_2axS853 <= 127) {
              moonbit_bytes_t _M0L5bytesS2579 = _M0L2bsS809.$0;
              int32_t _M0L5startS2581 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2580 = _M0L5startS2581 + 3;
              int32_t _M0L4_2axS854 = _M0L5bytesS2579[_M0L6_2atmpS2580];
              if (_M0L4_2axS854 <= 127) {
                moonbit_bytes_t _M0L5bytesS2576 = _M0L2bsS809.$0;
                int32_t _M0L5startS2578 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2577 = _M0L5startS2578 + 4;
                int32_t _M0L4_2axS855 = _M0L5bytesS2576[_M0L6_2atmpS2577];
                if (_M0L4_2axS855 <= 127) {
                  moonbit_bytes_t _M0L5bytesS2573 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2575 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2574 = _M0L5startS2575 + 5;
                  int32_t _M0L4_2axS856 = _M0L5bytesS2573[_M0L6_2atmpS2574];
                  if (_M0L4_2axS856 <= 127) {
                    moonbit_bytes_t _M0L5bytesS2570 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2572 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2571 = _M0L5startS2572 + 6;
                    int32_t _M0L4_2axS857 = _M0L5bytesS2570[_M0L6_2atmpS2571];
                    if (_M0L4_2axS857 <= 127) {
                      moonbit_bytes_t _M0L5bytesS2567 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2569 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2568 = _M0L5startS2569 + 7;
                      int32_t _M0L4_2axS858 =
                        _M0L5bytesS2567[_M0L6_2atmpS2568];
                      if (_M0L4_2axS858 <= 127) {
                        moonbit_bytes_t _M0L5bytesS2563 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2566 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2564 = _M0L5startS2566 + 8;
                        int32_t _M0L3endS2565 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS859 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2564,
                                                             _M0L3endS2565,
                                                             _M0L5bytesS2563};
                        int32_t _M0L6_2atmpS2555;
                        int32_t _M0L6_2atmpS2556;
                        int32_t _M0L6_2atmpS2557;
                        int32_t _M0L6_2atmpS2558;
                        int32_t _M0L6_2atmpS2559;
                        int32_t _M0L6_2atmpS2560;
                        int32_t _M0L6_2atmpS2561;
                        int32_t _M0L6_2atmpS2562;
                        _M0L1tS806[_M0L4tlenS808] = _M0L4_2axS851;
                        _M0L6_2atmpS2555 = _M0L4tlenS808 + 2;
                        _M0L1tS806[_M0L6_2atmpS2555] = _M0L4_2axS852;
                        _M0L6_2atmpS2556 = _M0L4tlenS808 + 4;
                        _M0L1tS806[_M0L6_2atmpS2556] = _M0L4_2axS853;
                        _M0L6_2atmpS2557 = _M0L4tlenS808 + 6;
                        _M0L1tS806[_M0L6_2atmpS2557] = _M0L4_2axS854;
                        _M0L6_2atmpS2558 = _M0L4tlenS808 + 8;
                        _M0L1tS806[_M0L6_2atmpS2558] = _M0L4_2axS855;
                        _M0L6_2atmpS2559 = _M0L4tlenS808 + 10;
                        _M0L1tS806[_M0L6_2atmpS2559] = _M0L4_2axS856;
                        _M0L6_2atmpS2560 = _M0L4tlenS808 + 12;
                        _M0L1tS806[_M0L6_2atmpS2560] = _M0L4_2axS857;
                        _M0L6_2atmpS2561 = _M0L4tlenS808 + 14;
                        _M0L1tS806[_M0L6_2atmpS2561] = _M0L4_2axS858;
                        _M0L6_2atmpS2562 = _M0L4tlenS808 + 16;
                        _M0L4tlenS808 = _M0L6_2atmpS2562;
                        _M0L2bsS809 = _M0L4_2axS859;
                        continue;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2551 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2554 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2552 = _M0L5startS2554 + 1;
                        int32_t _M0L3endS2553 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS860 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2552,
                                                             _M0L3endS2553,
                                                             _M0L5bytesS2551};
                        _M0L4tlenS848 = _M0L4tlenS808;
                        _M0L4restS849 = _M0L4_2axS860;
                        _M0L1bS850 = _M0L4_2axS851;
                        goto join_847;
                      }
                    } else {
                      moonbit_bytes_t _M0L5bytesS2547 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2550 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2548 = _M0L5startS2550 + 1;
                      int32_t _M0L3endS2549 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS861 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2548,
                                                           _M0L3endS2549,
                                                           _M0L5bytesS2547};
                      _M0L4tlenS848 = _M0L4tlenS808;
                      _M0L4restS849 = _M0L4_2axS861;
                      _M0L1bS850 = _M0L4_2axS851;
                      goto join_847;
                    }
                  } else {
                    moonbit_bytes_t _M0L5bytesS2543 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2546 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2544 = _M0L5startS2546 + 1;
                    int32_t _M0L3endS2545 = _M0L2bsS809.$2;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS862 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2544,
                                                         _M0L3endS2545,
                                                         _M0L5bytesS2543};
                    _M0L4tlenS848 = _M0L4tlenS808;
                    _M0L4restS849 = _M0L4_2axS862;
                    _M0L1bS850 = _M0L4_2axS851;
                    goto join_847;
                  }
                } else {
                  moonbit_bytes_t _M0L5bytesS2539 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2542 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2540 = _M0L5startS2542 + 1;
                  int32_t _M0L3endS2541 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS863 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2540,
                                                       _M0L3endS2541,
                                                       _M0L5bytesS2539};
                  _M0L4tlenS848 = _M0L4tlenS808;
                  _M0L4restS849 = _M0L4_2axS863;
                  _M0L1bS850 = _M0L4_2axS851;
                  goto join_847;
                }
              } else {
                moonbit_bytes_t _M0L5bytesS2535 = _M0L2bsS809.$0;
                int32_t _M0L5startS2538 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2536 = _M0L5startS2538 + 1;
                int32_t _M0L3endS2537 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS864 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2536,
                                                     _M0L3endS2537,
                                                     _M0L5bytesS2535};
                _M0L4tlenS848 = _M0L4tlenS808;
                _M0L4restS849 = _M0L4_2axS864;
                _M0L1bS850 = _M0L4_2axS851;
                goto join_847;
              }
            } else {
              moonbit_bytes_t _M0L5bytesS2531 = _M0L2bsS809.$0;
              int32_t _M0L5startS2534 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2532 = _M0L5startS2534 + 1;
              int32_t _M0L3endS2533 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS865 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2532,
                                                   _M0L3endS2533,
                                                   _M0L5bytesS2531};
              _M0L4tlenS848 = _M0L4tlenS808;
              _M0L4restS849 = _M0L4_2axS865;
              _M0L1bS850 = _M0L4_2axS851;
              goto join_847;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2527 = _M0L2bsS809.$0;
            int32_t _M0L5startS2530 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2528 = _M0L5startS2530 + 1;
            int32_t _M0L3endS2529 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS866 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2528,
                                                 _M0L3endS2529,
                                                 _M0L5bytesS2527};
            _M0L4tlenS848 = _M0L4tlenS808;
            _M0L4restS849 = _M0L4_2axS866;
            _M0L1bS850 = _M0L4_2axS851;
            goto join_847;
          }
        } else if (_M0L4_2axS851 >= 194 && _M0L4_2axS851 <= 223) {
          moonbit_bytes_t _M0L5bytesS2524 = _M0L2bsS809.$0;
          int32_t _M0L5startS2526 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2525 = _M0L5startS2526 + 1;
          int32_t _M0L4_2axS867 = _M0L5bytesS2524[_M0L6_2atmpS2525];
          if (_M0L4_2axS867 >= 128 && _M0L4_2axS867 <= 191) {
            moonbit_bytes_t _M0L5bytesS2520 = _M0L2bsS809.$0;
            int32_t _M0L5startS2523 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2521 = _M0L5startS2523 + 2;
            int32_t _M0L3endS2522 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS868 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2521,
                                                 _M0L3endS2522,
                                                 _M0L5bytesS2520};
            _M0L4tlenS842 = _M0L4tlenS808;
            _M0L4restS843 = _M0L4_2axS868;
            _M0L2b0S844 = _M0L4_2axS851;
            _M0L2b1S845 = _M0L4_2axS867;
            goto join_841;
          } else {
            moonbit_bytes_t _M0L5bytesS2516 = _M0L2bsS809.$0;
            int32_t _M0L5startS2519 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2517 = _M0L5startS2519 + 1;
            int32_t _M0L3endS2518 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS869 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2517,
                                                 _M0L3endS2518,
                                                 _M0L5bytesS2516};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS869;
            goto join_810;
          }
        } else if (_M0L4_2axS851 == 224) {
          moonbit_bytes_t _M0L5bytesS2513 = _M0L2bsS809.$0;
          int32_t _M0L5startS2515 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2514 = _M0L5startS2515 + 1;
          int32_t _M0L4_2axS870 = _M0L5bytesS2513[_M0L6_2atmpS2514];
          if (_M0L4_2axS870 >= 160 && _M0L4_2axS870 <= 191) {
            moonbit_bytes_t _M0L5bytesS2510 = _M0L2bsS809.$0;
            int32_t _M0L5startS2512 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2511 = _M0L5startS2512 + 2;
            int32_t _M0L4_2axS871 = _M0L5bytesS2510[_M0L6_2atmpS2511];
            if (_M0L4_2axS871 >= 128 && _M0L4_2axS871 <= 191) {
              moonbit_bytes_t _M0L5bytesS2506 = _M0L2bsS809.$0;
              int32_t _M0L5startS2509 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2507 = _M0L5startS2509 + 3;
              int32_t _M0L3endS2508 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS872 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2507,
                                                   _M0L3endS2508,
                                                   _M0L5bytesS2506};
              _M0L4tlenS835 = _M0L4tlenS808;
              _M0L2b0S836 = _M0L4_2axS851;
              _M0L2b1S837 = _M0L4_2axS870;
              _M0L2b2S838 = _M0L4_2axS871;
              _M0L4restS839 = _M0L4_2axS872;
              goto join_834;
            } else {
              moonbit_bytes_t _M0L5bytesS2502 = _M0L2bsS809.$0;
              int32_t _M0L5startS2505 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2503 = _M0L5startS2505 + 2;
              int32_t _M0L3endS2504 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS873 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2503,
                                                   _M0L3endS2504,
                                                   _M0L5bytesS2502};
              _M0L4restS821 = _M0L4_2axS873;
              _M0L4tlenS822 = _M0L4tlenS808;
              goto join_820;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2498 = _M0L2bsS809.$0;
            int32_t _M0L5startS2501 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2499 = _M0L5startS2501 + 1;
            int32_t _M0L3endS2500 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS874 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2499,
                                                 _M0L3endS2500,
                                                 _M0L5bytesS2498};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS874;
            goto join_810;
          }
        } else if (_M0L4_2axS851 >= 225 && _M0L4_2axS851 <= 236) {
          moonbit_bytes_t _M0L5bytesS2495 = _M0L2bsS809.$0;
          int32_t _M0L5startS2497 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2496 = _M0L5startS2497 + 1;
          int32_t _M0L4_2axS875 = _M0L5bytesS2495[_M0L6_2atmpS2496];
          if (_M0L4_2axS875 >= 128 && _M0L4_2axS875 <= 191) {
            moonbit_bytes_t _M0L5bytesS2492 = _M0L2bsS809.$0;
            int32_t _M0L5startS2494 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2493 = _M0L5startS2494 + 2;
            int32_t _M0L4_2axS876 = _M0L5bytesS2492[_M0L6_2atmpS2493];
            if (_M0L4_2axS876 >= 128 && _M0L4_2axS876 <= 191) {
              moonbit_bytes_t _M0L5bytesS2488 = _M0L2bsS809.$0;
              int32_t _M0L5startS2491 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2489 = _M0L5startS2491 + 3;
              int32_t _M0L3endS2490 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS877 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2489,
                                                   _M0L3endS2490,
                                                   _M0L5bytesS2488};
              _M0L4tlenS835 = _M0L4tlenS808;
              _M0L2b0S836 = _M0L4_2axS851;
              _M0L2b1S837 = _M0L4_2axS875;
              _M0L2b2S838 = _M0L4_2axS876;
              _M0L4restS839 = _M0L4_2axS877;
              goto join_834;
            } else {
              moonbit_bytes_t _M0L5bytesS2484 = _M0L2bsS809.$0;
              int32_t _M0L5startS2487 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2485 = _M0L5startS2487 + 2;
              int32_t _M0L3endS2486 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS878 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2485,
                                                   _M0L3endS2486,
                                                   _M0L5bytesS2484};
              _M0L4restS821 = _M0L4_2axS878;
              _M0L4tlenS822 = _M0L4tlenS808;
              goto join_820;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2480 = _M0L2bsS809.$0;
            int32_t _M0L5startS2483 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2481 = _M0L5startS2483 + 1;
            int32_t _M0L3endS2482 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS879 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2481,
                                                 _M0L3endS2482,
                                                 _M0L5bytesS2480};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS879;
            goto join_810;
          }
        } else if (_M0L4_2axS851 == 237) {
          moonbit_bytes_t _M0L5bytesS2477 = _M0L2bsS809.$0;
          int32_t _M0L5startS2479 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2478 = _M0L5startS2479 + 1;
          int32_t _M0L4_2axS880 = _M0L5bytesS2477[_M0L6_2atmpS2478];
          if (_M0L4_2axS880 >= 128 && _M0L4_2axS880 <= 159) {
            moonbit_bytes_t _M0L5bytesS2474 = _M0L2bsS809.$0;
            int32_t _M0L5startS2476 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2475 = _M0L5startS2476 + 2;
            int32_t _M0L4_2axS881 = _M0L5bytesS2474[_M0L6_2atmpS2475];
            if (_M0L4_2axS881 >= 128 && _M0L4_2axS881 <= 191) {
              moonbit_bytes_t _M0L5bytesS2470 = _M0L2bsS809.$0;
              int32_t _M0L5startS2473 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2471 = _M0L5startS2473 + 3;
              int32_t _M0L3endS2472 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS882 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2471,
                                                   _M0L3endS2472,
                                                   _M0L5bytesS2470};
              _M0L4tlenS835 = _M0L4tlenS808;
              _M0L2b0S836 = _M0L4_2axS851;
              _M0L2b1S837 = _M0L4_2axS880;
              _M0L2b2S838 = _M0L4_2axS881;
              _M0L4restS839 = _M0L4_2axS882;
              goto join_834;
            } else {
              moonbit_bytes_t _M0L5bytesS2466 = _M0L2bsS809.$0;
              int32_t _M0L5startS2469 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2467 = _M0L5startS2469 + 2;
              int32_t _M0L3endS2468 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS883 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2467,
                                                   _M0L3endS2468,
                                                   _M0L5bytesS2466};
              _M0L4restS821 = _M0L4_2axS883;
              _M0L4tlenS822 = _M0L4tlenS808;
              goto join_820;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2462 = _M0L2bsS809.$0;
            int32_t _M0L5startS2465 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2463 = _M0L5startS2465 + 1;
            int32_t _M0L3endS2464 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS884 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2463,
                                                 _M0L3endS2464,
                                                 _M0L5bytesS2462};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS884;
            goto join_810;
          }
        } else if (_M0L4_2axS851 >= 238 && _M0L4_2axS851 <= 239) {
          moonbit_bytes_t _M0L5bytesS2459 = _M0L2bsS809.$0;
          int32_t _M0L5startS2461 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2460 = _M0L5startS2461 + 1;
          int32_t _M0L4_2axS885 = _M0L5bytesS2459[_M0L6_2atmpS2460];
          if (_M0L4_2axS885 >= 128 && _M0L4_2axS885 <= 191) {
            moonbit_bytes_t _M0L5bytesS2456 = _M0L2bsS809.$0;
            int32_t _M0L5startS2458 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2457 = _M0L5startS2458 + 2;
            int32_t _M0L4_2axS886 = _M0L5bytesS2456[_M0L6_2atmpS2457];
            if (_M0L4_2axS886 >= 128 && _M0L4_2axS886 <= 191) {
              moonbit_bytes_t _M0L5bytesS2452 = _M0L2bsS809.$0;
              int32_t _M0L5startS2455 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2453 = _M0L5startS2455 + 3;
              int32_t _M0L3endS2454 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS887 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2453,
                                                   _M0L3endS2454,
                                                   _M0L5bytesS2452};
              _M0L4tlenS835 = _M0L4tlenS808;
              _M0L2b0S836 = _M0L4_2axS851;
              _M0L2b1S837 = _M0L4_2axS885;
              _M0L2b2S838 = _M0L4_2axS886;
              _M0L4restS839 = _M0L4_2axS887;
              goto join_834;
            } else {
              moonbit_bytes_t _M0L5bytesS2448 = _M0L2bsS809.$0;
              int32_t _M0L5startS2451 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2449 = _M0L5startS2451 + 2;
              int32_t _M0L3endS2450 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS888 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2449,
                                                   _M0L3endS2450,
                                                   _M0L5bytesS2448};
              _M0L4restS821 = _M0L4_2axS888;
              _M0L4tlenS822 = _M0L4tlenS808;
              goto join_820;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2444 = _M0L2bsS809.$0;
            int32_t _M0L5startS2447 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2445 = _M0L5startS2447 + 1;
            int32_t _M0L3endS2446 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS889 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2445,
                                                 _M0L3endS2446,
                                                 _M0L5bytesS2444};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS889;
            goto join_810;
          }
        } else if (_M0L4_2axS851 == 240) {
          moonbit_bytes_t _M0L5bytesS2441 = _M0L2bsS809.$0;
          int32_t _M0L5startS2443 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2442 = _M0L5startS2443 + 1;
          int32_t _M0L4_2axS890 = _M0L5bytesS2441[_M0L6_2atmpS2442];
          if (_M0L4_2axS890 >= 144 && _M0L4_2axS890 <= 191) {
            moonbit_bytes_t _M0L5bytesS2438 = _M0L2bsS809.$0;
            int32_t _M0L5startS2440 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2439 = _M0L5startS2440 + 2;
            int32_t _M0L4_2axS891 = _M0L5bytesS2438[_M0L6_2atmpS2439];
            if (_M0L4_2axS891 >= 128 && _M0L4_2axS891 <= 191) {
              moonbit_bytes_t _M0L5bytesS2435 = _M0L2bsS809.$0;
              int32_t _M0L5startS2437 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2436 = _M0L5startS2437 + 3;
              int32_t _M0L4_2axS892 = _M0L5bytesS2435[_M0L6_2atmpS2436];
              if (_M0L4_2axS892 >= 128 && _M0L4_2axS892 <= 191) {
                moonbit_bytes_t _M0L5bytesS2431 = _M0L2bsS809.$0;
                int32_t _M0L5startS2434 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2432 = _M0L5startS2434 + 4;
                int32_t _M0L3endS2433 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS893 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2432,
                                                     _M0L3endS2433,
                                                     _M0L5bytesS2431};
                _M0L4tlenS824 = _M0L4tlenS808;
                _M0L2b0S825 = _M0L4_2axS851;
                _M0L2b1S826 = _M0L4_2axS890;
                _M0L2b2S827 = _M0L4_2axS891;
                _M0L2b3S828 = _M0L4_2axS892;
                _M0L4restS829 = _M0L4_2axS893;
                goto join_823;
              } else {
                moonbit_bytes_t _M0L5bytesS2427 = _M0L2bsS809.$0;
                int32_t _M0L5startS2430 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2428 = _M0L5startS2430 + 3;
                int32_t _M0L3endS2429 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS894 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2428,
                                                     _M0L3endS2429,
                                                     _M0L5bytesS2427};
                _M0L4restS818 = _M0L4_2axS894;
                _M0L4tlenS819 = _M0L4tlenS808;
                goto join_817;
              }
            } else {
              moonbit_bytes_t _M0L5bytesS2423 = _M0L2bsS809.$0;
              int32_t _M0L5startS2426 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2424 = _M0L5startS2426 + 2;
              int32_t _M0L3endS2425 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS895 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2424,
                                                   _M0L3endS2425,
                                                   _M0L5bytesS2423};
              _M0L4restS815 = _M0L4_2axS895;
              _M0L4tlenS816 = _M0L4tlenS808;
              goto join_814;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2419 = _M0L2bsS809.$0;
            int32_t _M0L5startS2422 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2420 = _M0L5startS2422 + 1;
            int32_t _M0L3endS2421 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS896 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2420,
                                                 _M0L3endS2421,
                                                 _M0L5bytesS2419};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS896;
            goto join_810;
          }
        } else if (_M0L4_2axS851 >= 241 && _M0L4_2axS851 <= 243) {
          moonbit_bytes_t _M0L5bytesS2416 = _M0L2bsS809.$0;
          int32_t _M0L5startS2418 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2417 = _M0L5startS2418 + 1;
          int32_t _M0L4_2axS897 = _M0L5bytesS2416[_M0L6_2atmpS2417];
          if (_M0L4_2axS897 >= 128 && _M0L4_2axS897 <= 191) {
            moonbit_bytes_t _M0L5bytesS2413 = _M0L2bsS809.$0;
            int32_t _M0L5startS2415 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2414 = _M0L5startS2415 + 2;
            int32_t _M0L4_2axS898 = _M0L5bytesS2413[_M0L6_2atmpS2414];
            if (_M0L4_2axS898 >= 128 && _M0L4_2axS898 <= 191) {
              moonbit_bytes_t _M0L5bytesS2410 = _M0L2bsS809.$0;
              int32_t _M0L5startS2412 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2411 = _M0L5startS2412 + 3;
              int32_t _M0L4_2axS899 = _M0L5bytesS2410[_M0L6_2atmpS2411];
              if (_M0L4_2axS899 >= 128 && _M0L4_2axS899 <= 191) {
                moonbit_bytes_t _M0L5bytesS2406 = _M0L2bsS809.$0;
                int32_t _M0L5startS2409 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2407 = _M0L5startS2409 + 4;
                int32_t _M0L3endS2408 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS900 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2407,
                                                     _M0L3endS2408,
                                                     _M0L5bytesS2406};
                _M0L4tlenS824 = _M0L4tlenS808;
                _M0L2b0S825 = _M0L4_2axS851;
                _M0L2b1S826 = _M0L4_2axS897;
                _M0L2b2S827 = _M0L4_2axS898;
                _M0L2b3S828 = _M0L4_2axS899;
                _M0L4restS829 = _M0L4_2axS900;
                goto join_823;
              } else {
                moonbit_bytes_t _M0L5bytesS2402 = _M0L2bsS809.$0;
                int32_t _M0L5startS2405 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2403 = _M0L5startS2405 + 3;
                int32_t _M0L3endS2404 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS901 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2403,
                                                     _M0L3endS2404,
                                                     _M0L5bytesS2402};
                _M0L4restS818 = _M0L4_2axS901;
                _M0L4tlenS819 = _M0L4tlenS808;
                goto join_817;
              }
            } else {
              moonbit_bytes_t _M0L5bytesS2398 = _M0L2bsS809.$0;
              int32_t _M0L5startS2401 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2399 = _M0L5startS2401 + 2;
              int32_t _M0L3endS2400 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS902 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2399,
                                                   _M0L3endS2400,
                                                   _M0L5bytesS2398};
              _M0L4restS815 = _M0L4_2axS902;
              _M0L4tlenS816 = _M0L4tlenS808;
              goto join_814;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2394 = _M0L2bsS809.$0;
            int32_t _M0L5startS2397 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2395 = _M0L5startS2397 + 1;
            int32_t _M0L3endS2396 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS903 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2395,
                                                 _M0L3endS2396,
                                                 _M0L5bytesS2394};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS903;
            goto join_810;
          }
        } else if (_M0L4_2axS851 == 244) {
          moonbit_bytes_t _M0L5bytesS2391 = _M0L2bsS809.$0;
          int32_t _M0L5startS2393 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2392 = _M0L5startS2393 + 1;
          int32_t _M0L4_2axS904 = _M0L5bytesS2391[_M0L6_2atmpS2392];
          if (_M0L4_2axS904 >= 128 && _M0L4_2axS904 <= 143) {
            moonbit_bytes_t _M0L5bytesS2388 = _M0L2bsS809.$0;
            int32_t _M0L5startS2390 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2389 = _M0L5startS2390 + 2;
            int32_t _M0L4_2axS905 = _M0L5bytesS2388[_M0L6_2atmpS2389];
            if (_M0L4_2axS905 >= 128 && _M0L4_2axS905 <= 191) {
              moonbit_bytes_t _M0L5bytesS2385 = _M0L2bsS809.$0;
              int32_t _M0L5startS2387 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2386 = _M0L5startS2387 + 3;
              int32_t _M0L4_2axS906 = _M0L5bytesS2385[_M0L6_2atmpS2386];
              if (_M0L4_2axS906 >= 128 && _M0L4_2axS906 <= 191) {
                moonbit_bytes_t _M0L5bytesS2381 = _M0L2bsS809.$0;
                int32_t _M0L5startS2384 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2382 = _M0L5startS2384 + 4;
                int32_t _M0L3endS2383 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS907 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2382,
                                                     _M0L3endS2383,
                                                     _M0L5bytesS2381};
                _M0L4tlenS824 = _M0L4tlenS808;
                _M0L2b0S825 = _M0L4_2axS851;
                _M0L2b1S826 = _M0L4_2axS904;
                _M0L2b2S827 = _M0L4_2axS905;
                _M0L2b3S828 = _M0L4_2axS906;
                _M0L4restS829 = _M0L4_2axS907;
                goto join_823;
              } else {
                moonbit_bytes_t _M0L5bytesS2377 = _M0L2bsS809.$0;
                int32_t _M0L5startS2380 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2378 = _M0L5startS2380 + 3;
                int32_t _M0L3endS2379 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS908 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2378,
                                                     _M0L3endS2379,
                                                     _M0L5bytesS2377};
                _M0L4restS818 = _M0L4_2axS908;
                _M0L4tlenS819 = _M0L4tlenS808;
                goto join_817;
              }
            } else {
              moonbit_bytes_t _M0L5bytesS2373 = _M0L2bsS809.$0;
              int32_t _M0L5startS2376 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2374 = _M0L5startS2376 + 2;
              int32_t _M0L3endS2375 = _M0L2bsS809.$2;
              struct _M0TPC15bytes9BytesView _M0L4_2axS909 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2374,
                                                   _M0L3endS2375,
                                                   _M0L5bytesS2373};
              _M0L4restS815 = _M0L4_2axS909;
              _M0L4tlenS816 = _M0L4tlenS808;
              goto join_814;
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2369 = _M0L2bsS809.$0;
            int32_t _M0L5startS2372 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2370 = _M0L5startS2372 + 1;
            int32_t _M0L3endS2371 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS910 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2370,
                                                 _M0L3endS2371,
                                                 _M0L5bytesS2369};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS910;
            goto join_810;
          }
        } else {
          moonbit_bytes_t _M0L5bytesS2365 = _M0L2bsS809.$0;
          int32_t _M0L5startS2368 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2366 = _M0L5startS2368 + 1;
          int32_t _M0L3endS2367 = _M0L2bsS809.$2;
          struct _M0TPC15bytes9BytesView _M0L4_2axS911 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2366,
                                               _M0L3endS2367,
                                               _M0L5bytesS2365};
          _M0L4tlenS811 = _M0L4tlenS808;
          _M0L4restS812 = _M0L4_2axS911;
          goto join_810;
        }
      } else {
        moonbit_bytes_t _M0L5bytesS2978 = _M0L2bsS809.$0;
        int32_t _M0L5startS2979 = _M0L2bsS809.$1;
        int32_t _M0L4_2axS912 = _M0L5bytesS2978[_M0L5startS2979];
        if (_M0L4_2axS912 >= 0 && _M0L4_2axS912 <= 127) {
          moonbit_bytes_t _M0L5bytesS2974 = _M0L2bsS809.$0;
          int32_t _M0L5startS2977 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2975 = _M0L5startS2977 + 1;
          int32_t _M0L3endS2976 = _M0L2bsS809.$2;
          struct _M0TPC15bytes9BytesView _M0L4_2axS913 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2975,
                                               _M0L3endS2976,
                                               _M0L5bytesS2974};
          _M0L4tlenS848 = _M0L4tlenS808;
          _M0L4restS849 = _M0L4_2axS913;
          _M0L1bS850 = _M0L4_2axS912;
          goto join_847;
        } else {
          int32_t _M0L3endS2591 = _M0L2bsS809.$2;
          int32_t _M0L5startS2592 = _M0L2bsS809.$1;
          int32_t _M0L6_2atmpS2590 = _M0L3endS2591 - _M0L5startS2592;
          if (_M0L6_2atmpS2590 >= 2) {
            if (_M0L4_2axS912 >= 194 && _M0L4_2axS912 <= 223) {
              moonbit_bytes_t _M0L5bytesS2967 = _M0L2bsS809.$0;
              int32_t _M0L5startS2969 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2968 = _M0L5startS2969 + 1;
              int32_t _M0L4_2axS914 = _M0L5bytesS2967[_M0L6_2atmpS2968];
              if (_M0L4_2axS914 >= 128 && _M0L4_2axS914 <= 191) {
                moonbit_bytes_t _M0L5bytesS2963 = _M0L2bsS809.$0;
                int32_t _M0L5startS2966 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2964 = _M0L5startS2966 + 2;
                int32_t _M0L3endS2965 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS915 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2964,
                                                     _M0L3endS2965,
                                                     _M0L5bytesS2963};
                _M0L4tlenS842 = _M0L4tlenS808;
                _M0L4restS843 = _M0L4_2axS915;
                _M0L2b0S844 = _M0L4_2axS912;
                _M0L2b1S845 = _M0L4_2axS914;
                goto join_841;
              } else {
                int32_t _M0L3endS2946 = _M0L2bsS809.$2;
                int32_t _M0L5startS2947 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2945 = _M0L3endS2946 - _M0L5startS2947;
                if (_M0L6_2atmpS2945 >= 3) {
                  int32_t _M0L3endS2949 = _M0L2bsS809.$2;
                  int32_t _M0L5startS2950 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2948 = _M0L3endS2949 - _M0L5startS2950;
                  if (_M0L6_2atmpS2948 >= 4) {
                    moonbit_bytes_t _M0L5bytesS2951 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2954 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2952 = _M0L5startS2954 + 1;
                    int32_t _M0L3endS2953 = _M0L2bsS809.$2;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS916 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2952,
                                                         _M0L3endS2953,
                                                         _M0L5bytesS2951};
                    _M0L4tlenS811 = _M0L4tlenS808;
                    _M0L4restS812 = _M0L4_2axS916;
                    goto join_810;
                  } else {
                    moonbit_bytes_t _M0L5bytesS2955 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2958 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2956 = _M0L5startS2958 + 1;
                    int32_t _M0L3endS2957 = _M0L2bsS809.$2;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS917 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2956,
                                                         _M0L3endS2957,
                                                         _M0L5bytesS2955};
                    _M0L4tlenS811 = _M0L4tlenS808;
                    _M0L4restS812 = _M0L4_2axS917;
                    goto join_810;
                  }
                } else {
                  moonbit_bytes_t _M0L5bytesS2959 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2962 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2960 = _M0L5startS2962 + 1;
                  int32_t _M0L3endS2961 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS918 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2960,
                                                       _M0L3endS2961,
                                                       _M0L5bytesS2959};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS918;
                  goto join_810;
                }
              }
            } else {
              int32_t _M0L3endS2594 = _M0L2bsS809.$2;
              int32_t _M0L5startS2595 = _M0L2bsS809.$1;
              int32_t _M0L6_2atmpS2593 = _M0L3endS2594 - _M0L5startS2595;
              if (_M0L6_2atmpS2593 >= 3) {
                if (_M0L4_2axS912 == 224) {
                  moonbit_bytes_t _M0L5bytesS2861 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2863 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2862 = _M0L5startS2863 + 1;
                  int32_t _M0L4_2axS919 = _M0L5bytesS2861[_M0L6_2atmpS2862];
                  if (_M0L4_2axS919 >= 160 && _M0L4_2axS919 <= 191) {
                    moonbit_bytes_t _M0L5bytesS2858 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2860 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2859 = _M0L5startS2860 + 2;
                    int32_t _M0L4_2axS920 = _M0L5bytesS2858[_M0L6_2atmpS2859];
                    if (_M0L4_2axS920 >= 128 && _M0L4_2axS920 <= 191) {
                      moonbit_bytes_t _M0L5bytesS2854 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2857 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2855 = _M0L5startS2857 + 3;
                      int32_t _M0L3endS2856 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS921 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2855,
                                                           _M0L3endS2856,
                                                           _M0L5bytesS2854};
                      _M0L4tlenS835 = _M0L4tlenS808;
                      _M0L2b0S836 = _M0L4_2axS912;
                      _M0L2b1S837 = _M0L4_2axS919;
                      _M0L2b2S838 = _M0L4_2axS920;
                      _M0L4restS839 = _M0L4_2axS921;
                      goto join_834;
                    } else {
                      int32_t _M0L3endS2844 = _M0L2bsS809.$2;
                      int32_t _M0L5startS2845 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2843 =
                        _M0L3endS2844 - _M0L5startS2845;
                      if (_M0L6_2atmpS2843 >= 4) {
                        moonbit_bytes_t _M0L5bytesS2846 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2849 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2847 = _M0L5startS2849 + 2;
                        int32_t _M0L3endS2848 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS922 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2847,
                                                             _M0L3endS2848,
                                                             _M0L5bytesS2846};
                        _M0L4restS821 = _M0L4_2axS922;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2850 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2853 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2851 = _M0L5startS2853 + 2;
                        int32_t _M0L3endS2852 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS923 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2851,
                                                             _M0L3endS2852,
                                                             _M0L5bytesS2850};
                        _M0L4restS821 = _M0L4_2axS923;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2833 = _M0L2bsS809.$2;
                    int32_t _M0L5startS2834 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2832 =
                      _M0L3endS2833 - _M0L5startS2834;
                    if (_M0L6_2atmpS2832 >= 4) {
                      moonbit_bytes_t _M0L5bytesS2835 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2838 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2836 = _M0L5startS2838 + 1;
                      int32_t _M0L3endS2837 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS924 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2836,
                                                           _M0L3endS2837,
                                                           _M0L5bytesS2835};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS924;
                      goto join_810;
                    } else {
                      moonbit_bytes_t _M0L5bytesS2839 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2842 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2840 = _M0L5startS2842 + 1;
                      int32_t _M0L3endS2841 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS925 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2840,
                                                           _M0L3endS2841,
                                                           _M0L5bytesS2839};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS925;
                      goto join_810;
                    }
                  }
                } else if (_M0L4_2axS912 >= 225 && _M0L4_2axS912 <= 236) {
                  moonbit_bytes_t _M0L5bytesS2829 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2831 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2830 = _M0L5startS2831 + 1;
                  int32_t _M0L4_2axS926 = _M0L5bytesS2829[_M0L6_2atmpS2830];
                  if (_M0L4_2axS926 >= 128 && _M0L4_2axS926 <= 191) {
                    moonbit_bytes_t _M0L5bytesS2826 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2828 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2827 = _M0L5startS2828 + 2;
                    int32_t _M0L4_2axS927 = _M0L5bytesS2826[_M0L6_2atmpS2827];
                    if (_M0L4_2axS927 >= 128 && _M0L4_2axS927 <= 191) {
                      moonbit_bytes_t _M0L5bytesS2822 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2825 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2823 = _M0L5startS2825 + 3;
                      int32_t _M0L3endS2824 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS928 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2823,
                                                           _M0L3endS2824,
                                                           _M0L5bytesS2822};
                      _M0L4tlenS835 = _M0L4tlenS808;
                      _M0L2b0S836 = _M0L4_2axS912;
                      _M0L2b1S837 = _M0L4_2axS926;
                      _M0L2b2S838 = _M0L4_2axS927;
                      _M0L4restS839 = _M0L4_2axS928;
                      goto join_834;
                    } else {
                      int32_t _M0L3endS2812 = _M0L2bsS809.$2;
                      int32_t _M0L5startS2813 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2811 =
                        _M0L3endS2812 - _M0L5startS2813;
                      if (_M0L6_2atmpS2811 >= 4) {
                        moonbit_bytes_t _M0L5bytesS2814 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2817 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2815 = _M0L5startS2817 + 2;
                        int32_t _M0L3endS2816 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS929 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2815,
                                                             _M0L3endS2816,
                                                             _M0L5bytesS2814};
                        _M0L4restS821 = _M0L4_2axS929;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2818 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2821 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2819 = _M0L5startS2821 + 2;
                        int32_t _M0L3endS2820 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS930 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2819,
                                                             _M0L3endS2820,
                                                             _M0L5bytesS2818};
                        _M0L4restS821 = _M0L4_2axS930;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2801 = _M0L2bsS809.$2;
                    int32_t _M0L5startS2802 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2800 =
                      _M0L3endS2801 - _M0L5startS2802;
                    if (_M0L6_2atmpS2800 >= 4) {
                      moonbit_bytes_t _M0L5bytesS2803 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2806 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2804 = _M0L5startS2806 + 1;
                      int32_t _M0L3endS2805 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS931 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2804,
                                                           _M0L3endS2805,
                                                           _M0L5bytesS2803};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS931;
                      goto join_810;
                    } else {
                      moonbit_bytes_t _M0L5bytesS2807 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2810 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2808 = _M0L5startS2810 + 1;
                      int32_t _M0L3endS2809 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS932 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2808,
                                                           _M0L3endS2809,
                                                           _M0L5bytesS2807};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS932;
                      goto join_810;
                    }
                  }
                } else if (_M0L4_2axS912 == 237) {
                  moonbit_bytes_t _M0L5bytesS2797 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2799 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2798 = _M0L5startS2799 + 1;
                  int32_t _M0L4_2axS933 = _M0L5bytesS2797[_M0L6_2atmpS2798];
                  if (_M0L4_2axS933 >= 128 && _M0L4_2axS933 <= 159) {
                    moonbit_bytes_t _M0L5bytesS2794 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2796 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2795 = _M0L5startS2796 + 2;
                    int32_t _M0L4_2axS934 = _M0L5bytesS2794[_M0L6_2atmpS2795];
                    if (_M0L4_2axS934 >= 128 && _M0L4_2axS934 <= 191) {
                      moonbit_bytes_t _M0L5bytesS2790 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2793 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2791 = _M0L5startS2793 + 3;
                      int32_t _M0L3endS2792 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS935 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2791,
                                                           _M0L3endS2792,
                                                           _M0L5bytesS2790};
                      _M0L4tlenS835 = _M0L4tlenS808;
                      _M0L2b0S836 = _M0L4_2axS912;
                      _M0L2b1S837 = _M0L4_2axS933;
                      _M0L2b2S838 = _M0L4_2axS934;
                      _M0L4restS839 = _M0L4_2axS935;
                      goto join_834;
                    } else {
                      int32_t _M0L3endS2780 = _M0L2bsS809.$2;
                      int32_t _M0L5startS2781 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2779 =
                        _M0L3endS2780 - _M0L5startS2781;
                      if (_M0L6_2atmpS2779 >= 4) {
                        moonbit_bytes_t _M0L5bytesS2782 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2785 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2783 = _M0L5startS2785 + 2;
                        int32_t _M0L3endS2784 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS936 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2783,
                                                             _M0L3endS2784,
                                                             _M0L5bytesS2782};
                        _M0L4restS821 = _M0L4_2axS936;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2786 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2789 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2787 = _M0L5startS2789 + 2;
                        int32_t _M0L3endS2788 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS937 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2787,
                                                             _M0L3endS2788,
                                                             _M0L5bytesS2786};
                        _M0L4restS821 = _M0L4_2axS937;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2769 = _M0L2bsS809.$2;
                    int32_t _M0L5startS2770 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2768 =
                      _M0L3endS2769 - _M0L5startS2770;
                    if (_M0L6_2atmpS2768 >= 4) {
                      moonbit_bytes_t _M0L5bytesS2771 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2774 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2772 = _M0L5startS2774 + 1;
                      int32_t _M0L3endS2773 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS938 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2772,
                                                           _M0L3endS2773,
                                                           _M0L5bytesS2771};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS938;
                      goto join_810;
                    } else {
                      moonbit_bytes_t _M0L5bytesS2775 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2778 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2776 = _M0L5startS2778 + 1;
                      int32_t _M0L3endS2777 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS939 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2776,
                                                           _M0L3endS2777,
                                                           _M0L5bytesS2775};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS939;
                      goto join_810;
                    }
                  }
                } else if (_M0L4_2axS912 >= 238 && _M0L4_2axS912 <= 239) {
                  moonbit_bytes_t _M0L5bytesS2765 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2767 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2766 = _M0L5startS2767 + 1;
                  int32_t _M0L4_2axS940 = _M0L5bytesS2765[_M0L6_2atmpS2766];
                  if (_M0L4_2axS940 >= 128 && _M0L4_2axS940 <= 191) {
                    moonbit_bytes_t _M0L5bytesS2762 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2764 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2763 = _M0L5startS2764 + 2;
                    int32_t _M0L4_2axS941 = _M0L5bytesS2762[_M0L6_2atmpS2763];
                    if (_M0L4_2axS941 >= 128 && _M0L4_2axS941 <= 191) {
                      moonbit_bytes_t _M0L5bytesS2758 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2761 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2759 = _M0L5startS2761 + 3;
                      int32_t _M0L3endS2760 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS942 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2759,
                                                           _M0L3endS2760,
                                                           _M0L5bytesS2758};
                      _M0L4tlenS835 = _M0L4tlenS808;
                      _M0L2b0S836 = _M0L4_2axS912;
                      _M0L2b1S837 = _M0L4_2axS940;
                      _M0L2b2S838 = _M0L4_2axS941;
                      _M0L4restS839 = _M0L4_2axS942;
                      goto join_834;
                    } else {
                      int32_t _M0L3endS2748 = _M0L2bsS809.$2;
                      int32_t _M0L5startS2749 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2747 =
                        _M0L3endS2748 - _M0L5startS2749;
                      if (_M0L6_2atmpS2747 >= 4) {
                        moonbit_bytes_t _M0L5bytesS2750 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2753 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2751 = _M0L5startS2753 + 2;
                        int32_t _M0L3endS2752 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS943 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2751,
                                                             _M0L3endS2752,
                                                             _M0L5bytesS2750};
                        _M0L4restS821 = _M0L4_2axS943;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2754 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2757 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2755 = _M0L5startS2757 + 2;
                        int32_t _M0L3endS2756 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS944 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2755,
                                                             _M0L3endS2756,
                                                             _M0L5bytesS2754};
                        _M0L4restS821 = _M0L4_2axS944;
                        _M0L4tlenS822 = _M0L4tlenS808;
                        goto join_820;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2737 = _M0L2bsS809.$2;
                    int32_t _M0L5startS2738 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2736 =
                      _M0L3endS2737 - _M0L5startS2738;
                    if (_M0L6_2atmpS2736 >= 4) {
                      moonbit_bytes_t _M0L5bytesS2739 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2742 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2740 = _M0L5startS2742 + 1;
                      int32_t _M0L3endS2741 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS945 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2740,
                                                           _M0L3endS2741,
                                                           _M0L5bytesS2739};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS945;
                      goto join_810;
                    } else {
                      moonbit_bytes_t _M0L5bytesS2743 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2746 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2744 = _M0L5startS2746 + 1;
                      int32_t _M0L3endS2745 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS946 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2744,
                                                           _M0L3endS2745,
                                                           _M0L5bytesS2743};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS946;
                      goto join_810;
                    }
                  }
                } else {
                  int32_t _M0L3endS2597 = _M0L2bsS809.$2;
                  int32_t _M0L5startS2598 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2596 = _M0L3endS2597 - _M0L5startS2598;
                  if (_M0L6_2atmpS2596 >= 4) {
                    if (_M0L4_2axS912 == 240) {
                      moonbit_bytes_t _M0L5bytesS2675 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2677 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2676 = _M0L5startS2677 + 1;
                      int32_t _M0L4_2axS947 =
                        _M0L5bytesS2675[_M0L6_2atmpS2676];
                      if (_M0L4_2axS947 >= 144 && _M0L4_2axS947 <= 191) {
                        moonbit_bytes_t _M0L5bytesS2672 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2674 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2673 = _M0L5startS2674 + 2;
                        int32_t _M0L4_2axS948 =
                          _M0L5bytesS2672[_M0L6_2atmpS2673];
                        if (_M0L4_2axS948 >= 128 && _M0L4_2axS948 <= 191) {
                          moonbit_bytes_t _M0L5bytesS2669 = _M0L2bsS809.$0;
                          int32_t _M0L5startS2671 = _M0L2bsS809.$1;
                          int32_t _M0L6_2atmpS2670 = _M0L5startS2671 + 3;
                          int32_t _M0L4_2axS949 =
                            _M0L5bytesS2669[_M0L6_2atmpS2670];
                          if (_M0L4_2axS949 >= 128 && _M0L4_2axS949 <= 191) {
                            moonbit_bytes_t _M0L5bytesS2665 = _M0L2bsS809.$0;
                            int32_t _M0L5startS2668 = _M0L2bsS809.$1;
                            int32_t _M0L6_2atmpS2666 = _M0L5startS2668 + 4;
                            int32_t _M0L3endS2667 = _M0L2bsS809.$2;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS950 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2666,
                                                                 _M0L3endS2667,
                                                                 _M0L5bytesS2665};
                            _M0L4tlenS824 = _M0L4tlenS808;
                            _M0L2b0S825 = _M0L4_2axS912;
                            _M0L2b1S826 = _M0L4_2axS947;
                            _M0L2b2S827 = _M0L4_2axS948;
                            _M0L2b3S828 = _M0L4_2axS949;
                            _M0L4restS829 = _M0L4_2axS950;
                            goto join_823;
                          } else {
                            moonbit_bytes_t _M0L5bytesS2661 = _M0L2bsS809.$0;
                            int32_t _M0L5startS2664 = _M0L2bsS809.$1;
                            int32_t _M0L6_2atmpS2662 = _M0L5startS2664 + 3;
                            int32_t _M0L3endS2663 = _M0L2bsS809.$2;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS951 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2662,
                                                                 _M0L3endS2663,
                                                                 _M0L5bytesS2661};
                            _M0L4restS818 = _M0L4_2axS951;
                            _M0L4tlenS819 = _M0L4tlenS808;
                            goto join_817;
                          }
                        } else {
                          moonbit_bytes_t _M0L5bytesS2657 = _M0L2bsS809.$0;
                          int32_t _M0L5startS2660 = _M0L2bsS809.$1;
                          int32_t _M0L6_2atmpS2658 = _M0L5startS2660 + 2;
                          int32_t _M0L3endS2659 = _M0L2bsS809.$2;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS952 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2658,
                                                               _M0L3endS2659,
                                                               _M0L5bytesS2657};
                          _M0L4restS815 = _M0L4_2axS952;
                          _M0L4tlenS816 = _M0L4tlenS808;
                          goto join_814;
                        }
                      } else {
                        moonbit_bytes_t _M0L5bytesS2653 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2656 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2654 = _M0L5startS2656 + 1;
                        int32_t _M0L3endS2655 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS953 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2654,
                                                             _M0L3endS2655,
                                                             _M0L5bytesS2653};
                        _M0L4tlenS811 = _M0L4tlenS808;
                        _M0L4restS812 = _M0L4_2axS953;
                        goto join_810;
                      }
                    } else if (_M0L4_2axS912 >= 241 && _M0L4_2axS912 <= 243) {
                      moonbit_bytes_t _M0L5bytesS2650 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2652 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2651 = _M0L5startS2652 + 1;
                      int32_t _M0L4_2axS954 =
                        _M0L5bytesS2650[_M0L6_2atmpS2651];
                      if (_M0L4_2axS954 >= 128 && _M0L4_2axS954 <= 191) {
                        moonbit_bytes_t _M0L5bytesS2647 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2649 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2648 = _M0L5startS2649 + 2;
                        int32_t _M0L4_2axS955 =
                          _M0L5bytesS2647[_M0L6_2atmpS2648];
                        if (_M0L4_2axS955 >= 128 && _M0L4_2axS955 <= 191) {
                          moonbit_bytes_t _M0L5bytesS2644 = _M0L2bsS809.$0;
                          int32_t _M0L5startS2646 = _M0L2bsS809.$1;
                          int32_t _M0L6_2atmpS2645 = _M0L5startS2646 + 3;
                          int32_t _M0L4_2axS956 =
                            _M0L5bytesS2644[_M0L6_2atmpS2645];
                          if (_M0L4_2axS956 >= 128 && _M0L4_2axS956 <= 191) {
                            moonbit_bytes_t _M0L5bytesS2640 = _M0L2bsS809.$0;
                            int32_t _M0L5startS2643 = _M0L2bsS809.$1;
                            int32_t _M0L6_2atmpS2641 = _M0L5startS2643 + 4;
                            int32_t _M0L3endS2642 = _M0L2bsS809.$2;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS957 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2641,
                                                                 _M0L3endS2642,
                                                                 _M0L5bytesS2640};
                            _M0L4tlenS824 = _M0L4tlenS808;
                            _M0L2b0S825 = _M0L4_2axS912;
                            _M0L2b1S826 = _M0L4_2axS954;
                            _M0L2b2S827 = _M0L4_2axS955;
                            _M0L2b3S828 = _M0L4_2axS956;
                            _M0L4restS829 = _M0L4_2axS957;
                            goto join_823;
                          } else {
                            moonbit_bytes_t _M0L5bytesS2636 = _M0L2bsS809.$0;
                            int32_t _M0L5startS2639 = _M0L2bsS809.$1;
                            int32_t _M0L6_2atmpS2637 = _M0L5startS2639 + 3;
                            int32_t _M0L3endS2638 = _M0L2bsS809.$2;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS958 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2637,
                                                                 _M0L3endS2638,
                                                                 _M0L5bytesS2636};
                            _M0L4restS818 = _M0L4_2axS958;
                            _M0L4tlenS819 = _M0L4tlenS808;
                            goto join_817;
                          }
                        } else {
                          moonbit_bytes_t _M0L5bytesS2632 = _M0L2bsS809.$0;
                          int32_t _M0L5startS2635 = _M0L2bsS809.$1;
                          int32_t _M0L6_2atmpS2633 = _M0L5startS2635 + 2;
                          int32_t _M0L3endS2634 = _M0L2bsS809.$2;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS959 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2633,
                                                               _M0L3endS2634,
                                                               _M0L5bytesS2632};
                          _M0L4restS815 = _M0L4_2axS959;
                          _M0L4tlenS816 = _M0L4tlenS808;
                          goto join_814;
                        }
                      } else {
                        moonbit_bytes_t _M0L5bytesS2628 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2631 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2629 = _M0L5startS2631 + 1;
                        int32_t _M0L3endS2630 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS960 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2629,
                                                             _M0L3endS2630,
                                                             _M0L5bytesS2628};
                        _M0L4tlenS811 = _M0L4tlenS808;
                        _M0L4restS812 = _M0L4_2axS960;
                        goto join_810;
                      }
                    } else if (_M0L4_2axS912 == 244) {
                      moonbit_bytes_t _M0L5bytesS2625 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2627 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2626 = _M0L5startS2627 + 1;
                      int32_t _M0L4_2axS961 =
                        _M0L5bytesS2625[_M0L6_2atmpS2626];
                      if (_M0L4_2axS961 >= 128 && _M0L4_2axS961 <= 143) {
                        moonbit_bytes_t _M0L5bytesS2622 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2624 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2623 = _M0L5startS2624 + 2;
                        int32_t _M0L4_2axS962 =
                          _M0L5bytesS2622[_M0L6_2atmpS2623];
                        if (_M0L4_2axS962 >= 128 && _M0L4_2axS962 <= 191) {
                          moonbit_bytes_t _M0L5bytesS2619 = _M0L2bsS809.$0;
                          int32_t _M0L5startS2621 = _M0L2bsS809.$1;
                          int32_t _M0L6_2atmpS2620 = _M0L5startS2621 + 3;
                          int32_t _M0L4_2axS963 =
                            _M0L5bytesS2619[_M0L6_2atmpS2620];
                          if (_M0L4_2axS963 >= 128 && _M0L4_2axS963 <= 191) {
                            moonbit_bytes_t _M0L5bytesS2615 = _M0L2bsS809.$0;
                            int32_t _M0L5startS2618 = _M0L2bsS809.$1;
                            int32_t _M0L6_2atmpS2616 = _M0L5startS2618 + 4;
                            int32_t _M0L3endS2617 = _M0L2bsS809.$2;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS964 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2616,
                                                                 _M0L3endS2617,
                                                                 _M0L5bytesS2615};
                            _M0L4tlenS824 = _M0L4tlenS808;
                            _M0L2b0S825 = _M0L4_2axS912;
                            _M0L2b1S826 = _M0L4_2axS961;
                            _M0L2b2S827 = _M0L4_2axS962;
                            _M0L2b3S828 = _M0L4_2axS963;
                            _M0L4restS829 = _M0L4_2axS964;
                            goto join_823;
                          } else {
                            moonbit_bytes_t _M0L5bytesS2611 = _M0L2bsS809.$0;
                            int32_t _M0L5startS2614 = _M0L2bsS809.$1;
                            int32_t _M0L6_2atmpS2612 = _M0L5startS2614 + 3;
                            int32_t _M0L3endS2613 = _M0L2bsS809.$2;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS965 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2612,
                                                                 _M0L3endS2613,
                                                                 _M0L5bytesS2611};
                            _M0L4restS818 = _M0L4_2axS965;
                            _M0L4tlenS819 = _M0L4tlenS808;
                            goto join_817;
                          }
                        } else {
                          moonbit_bytes_t _M0L5bytesS2607 = _M0L2bsS809.$0;
                          int32_t _M0L5startS2610 = _M0L2bsS809.$1;
                          int32_t _M0L6_2atmpS2608 = _M0L5startS2610 + 2;
                          int32_t _M0L3endS2609 = _M0L2bsS809.$2;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS966 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2608,
                                                               _M0L3endS2609,
                                                               _M0L5bytesS2607};
                          _M0L4restS815 = _M0L4_2axS966;
                          _M0L4tlenS816 = _M0L4tlenS808;
                          goto join_814;
                        }
                      } else {
                        moonbit_bytes_t _M0L5bytesS2603 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2606 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2604 = _M0L5startS2606 + 1;
                        int32_t _M0L3endS2605 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS967 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2604,
                                                             _M0L3endS2605,
                                                             _M0L5bytesS2603};
                        _M0L4tlenS811 = _M0L4tlenS808;
                        _M0L4restS812 = _M0L4_2axS967;
                        goto join_810;
                      }
                    } else {
                      moonbit_bytes_t _M0L5bytesS2599 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2602 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2600 = _M0L5startS2602 + 1;
                      int32_t _M0L3endS2601 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS968 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2600,
                                                           _M0L3endS2601,
                                                           _M0L5bytesS2599};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS968;
                      goto join_810;
                    }
                  } else if (_M0L4_2axS912 == 240) {
                    moonbit_bytes_t _M0L5bytesS2733 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2735 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2734 = _M0L5startS2735 + 1;
                    int32_t _M0L4_2axS969 = _M0L5bytesS2733[_M0L6_2atmpS2734];
                    if (_M0L4_2axS969 >= 144 && _M0L4_2axS969 <= 191) {
                      moonbit_bytes_t _M0L5bytesS2730 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2732 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2731 = _M0L5startS2732 + 2;
                      int32_t _M0L4_2axS970 =
                        _M0L5bytesS2730[_M0L6_2atmpS2731];
                      if (_M0L4_2axS970 >= 128 && _M0L4_2axS970 <= 191) {
                        moonbit_bytes_t _M0L5bytesS2726 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2729 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2727 = _M0L5startS2729 + 3;
                        int32_t _M0L3endS2728 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS971 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2727,
                                                             _M0L3endS2728,
                                                             _M0L5bytesS2726};
                        _M0L4restS818 = _M0L4_2axS971;
                        _M0L4tlenS819 = _M0L4tlenS808;
                        goto join_817;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2722 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2725 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2723 = _M0L5startS2725 + 2;
                        int32_t _M0L3endS2724 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS972 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2723,
                                                             _M0L3endS2724,
                                                             _M0L5bytesS2722};
                        _M0L4restS815 = _M0L4_2axS972;
                        _M0L4tlenS816 = _M0L4tlenS808;
                        goto join_814;
                      }
                    } else {
                      moonbit_bytes_t _M0L5bytesS2718 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2721 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2719 = _M0L5startS2721 + 1;
                      int32_t _M0L3endS2720 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS973 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2719,
                                                           _M0L3endS2720,
                                                           _M0L5bytesS2718};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS973;
                      goto join_810;
                    }
                  } else if (_M0L4_2axS912 >= 241 && _M0L4_2axS912 <= 243) {
                    moonbit_bytes_t _M0L5bytesS2715 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2717 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2716 = _M0L5startS2717 + 1;
                    int32_t _M0L4_2axS974 = _M0L5bytesS2715[_M0L6_2atmpS2716];
                    if (_M0L4_2axS974 >= 128 && _M0L4_2axS974 <= 191) {
                      moonbit_bytes_t _M0L5bytesS2712 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2714 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2713 = _M0L5startS2714 + 2;
                      int32_t _M0L4_2axS975 =
                        _M0L5bytesS2712[_M0L6_2atmpS2713];
                      if (_M0L4_2axS975 >= 128 && _M0L4_2axS975 <= 191) {
                        moonbit_bytes_t _M0L5bytesS2708 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2711 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2709 = _M0L5startS2711 + 3;
                        int32_t _M0L3endS2710 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS976 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2709,
                                                             _M0L3endS2710,
                                                             _M0L5bytesS2708};
                        _M0L4restS818 = _M0L4_2axS976;
                        _M0L4tlenS819 = _M0L4tlenS808;
                        goto join_817;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2704 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2707 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2705 = _M0L5startS2707 + 2;
                        int32_t _M0L3endS2706 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS977 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2705,
                                                             _M0L3endS2706,
                                                             _M0L5bytesS2704};
                        _M0L4restS815 = _M0L4_2axS977;
                        _M0L4tlenS816 = _M0L4tlenS808;
                        goto join_814;
                      }
                    } else {
                      moonbit_bytes_t _M0L5bytesS2700 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2703 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2701 = _M0L5startS2703 + 1;
                      int32_t _M0L3endS2702 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS978 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2701,
                                                           _M0L3endS2702,
                                                           _M0L5bytesS2700};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS978;
                      goto join_810;
                    }
                  } else if (_M0L4_2axS912 == 244) {
                    moonbit_bytes_t _M0L5bytesS2697 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2699 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2698 = _M0L5startS2699 + 1;
                    int32_t _M0L4_2axS979 = _M0L5bytesS2697[_M0L6_2atmpS2698];
                    if (_M0L4_2axS979 >= 128 && _M0L4_2axS979 <= 143) {
                      moonbit_bytes_t _M0L5bytesS2694 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2696 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2695 = _M0L5startS2696 + 2;
                      int32_t _M0L4_2axS980 =
                        _M0L5bytesS2694[_M0L6_2atmpS2695];
                      if (_M0L4_2axS980 >= 128 && _M0L4_2axS980 <= 191) {
                        moonbit_bytes_t _M0L5bytesS2690 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2693 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2691 = _M0L5startS2693 + 3;
                        int32_t _M0L3endS2692 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS981 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2691,
                                                             _M0L3endS2692,
                                                             _M0L5bytesS2690};
                        _M0L4restS818 = _M0L4_2axS981;
                        _M0L4tlenS819 = _M0L4tlenS808;
                        goto join_817;
                      } else {
                        moonbit_bytes_t _M0L5bytesS2686 = _M0L2bsS809.$0;
                        int32_t _M0L5startS2689 = _M0L2bsS809.$1;
                        int32_t _M0L6_2atmpS2687 = _M0L5startS2689 + 2;
                        int32_t _M0L3endS2688 = _M0L2bsS809.$2;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS982 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2687,
                                                             _M0L3endS2688,
                                                             _M0L5bytesS2686};
                        _M0L4restS815 = _M0L4_2axS982;
                        _M0L4tlenS816 = _M0L4tlenS808;
                        goto join_814;
                      }
                    } else {
                      moonbit_bytes_t _M0L5bytesS2682 = _M0L2bsS809.$0;
                      int32_t _M0L5startS2685 = _M0L2bsS809.$1;
                      int32_t _M0L6_2atmpS2683 = _M0L5startS2685 + 1;
                      int32_t _M0L3endS2684 = _M0L2bsS809.$2;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS983 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2683,
                                                           _M0L3endS2684,
                                                           _M0L5bytesS2682};
                      _M0L4tlenS811 = _M0L4tlenS808;
                      _M0L4restS812 = _M0L4_2axS983;
                      goto join_810;
                    }
                  } else {
                    moonbit_bytes_t _M0L5bytesS2678 = _M0L2bsS809.$0;
                    int32_t _M0L5startS2681 = _M0L2bsS809.$1;
                    int32_t _M0L6_2atmpS2679 = _M0L5startS2681 + 1;
                    int32_t _M0L3endS2680 = _M0L2bsS809.$2;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS984 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2679,
                                                         _M0L3endS2680,
                                                         _M0L5bytesS2678};
                    _M0L4tlenS811 = _M0L4tlenS808;
                    _M0L4restS812 = _M0L4_2axS984;
                    goto join_810;
                  }
                }
              } else if (_M0L4_2axS912 == 224) {
                moonbit_bytes_t _M0L5bytesS2942 = _M0L2bsS809.$0;
                int32_t _M0L5startS2944 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2943 = _M0L5startS2944 + 1;
                int32_t _M0L4_2axS985 = _M0L5bytesS2942[_M0L6_2atmpS2943];
                if (_M0L4_2axS985 >= 160 && _M0L4_2axS985 <= 191) {
                  moonbit_bytes_t _M0L5bytesS2938 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2941 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2939 = _M0L5startS2941 + 2;
                  int32_t _M0L3endS2940 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS986 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2939,
                                                       _M0L3endS2940,
                                                       _M0L5bytesS2938};
                  _M0L4restS821 = _M0L4_2axS986;
                  _M0L4tlenS822 = _M0L4tlenS808;
                  goto join_820;
                } else {
                  moonbit_bytes_t _M0L5bytesS2934 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2937 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2935 = _M0L5startS2937 + 1;
                  int32_t _M0L3endS2936 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS987 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2935,
                                                       _M0L3endS2936,
                                                       _M0L5bytesS2934};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS987;
                  goto join_810;
                }
              } else if (_M0L4_2axS912 >= 225 && _M0L4_2axS912 <= 236) {
                moonbit_bytes_t _M0L5bytesS2931 = _M0L2bsS809.$0;
                int32_t _M0L5startS2933 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2932 = _M0L5startS2933 + 1;
                int32_t _M0L4_2axS988 = _M0L5bytesS2931[_M0L6_2atmpS2932];
                if (_M0L4_2axS988 >= 128 && _M0L4_2axS988 <= 191) {
                  moonbit_bytes_t _M0L5bytesS2927 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2930 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2928 = _M0L5startS2930 + 2;
                  int32_t _M0L3endS2929 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS989 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2928,
                                                       _M0L3endS2929,
                                                       _M0L5bytesS2927};
                  _M0L4restS821 = _M0L4_2axS989;
                  _M0L4tlenS822 = _M0L4tlenS808;
                  goto join_820;
                } else {
                  moonbit_bytes_t _M0L5bytesS2923 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2926 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2924 = _M0L5startS2926 + 1;
                  int32_t _M0L3endS2925 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS990 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2924,
                                                       _M0L3endS2925,
                                                       _M0L5bytesS2923};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS990;
                  goto join_810;
                }
              } else if (_M0L4_2axS912 == 237) {
                moonbit_bytes_t _M0L5bytesS2920 = _M0L2bsS809.$0;
                int32_t _M0L5startS2922 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2921 = _M0L5startS2922 + 1;
                int32_t _M0L4_2axS991 = _M0L5bytesS2920[_M0L6_2atmpS2921];
                if (_M0L4_2axS991 >= 128 && _M0L4_2axS991 <= 159) {
                  moonbit_bytes_t _M0L5bytesS2916 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2919 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2917 = _M0L5startS2919 + 2;
                  int32_t _M0L3endS2918 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS992 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2917,
                                                       _M0L3endS2918,
                                                       _M0L5bytesS2916};
                  _M0L4restS821 = _M0L4_2axS992;
                  _M0L4tlenS822 = _M0L4tlenS808;
                  goto join_820;
                } else {
                  moonbit_bytes_t _M0L5bytesS2912 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2915 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2913 = _M0L5startS2915 + 1;
                  int32_t _M0L3endS2914 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS993 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2913,
                                                       _M0L3endS2914,
                                                       _M0L5bytesS2912};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS993;
                  goto join_810;
                }
              } else if (_M0L4_2axS912 >= 238 && _M0L4_2axS912 <= 239) {
                moonbit_bytes_t _M0L5bytesS2909 = _M0L2bsS809.$0;
                int32_t _M0L5startS2911 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2910 = _M0L5startS2911 + 1;
                int32_t _M0L4_2axS994 = _M0L5bytesS2909[_M0L6_2atmpS2910];
                if (_M0L4_2axS994 >= 128 && _M0L4_2axS994 <= 191) {
                  moonbit_bytes_t _M0L5bytesS2905 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2908 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2906 = _M0L5startS2908 + 2;
                  int32_t _M0L3endS2907 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS995 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2906,
                                                       _M0L3endS2907,
                                                       _M0L5bytesS2905};
                  _M0L4restS821 = _M0L4_2axS995;
                  _M0L4tlenS822 = _M0L4tlenS808;
                  goto join_820;
                } else {
                  moonbit_bytes_t _M0L5bytesS2901 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2904 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2902 = _M0L5startS2904 + 1;
                  int32_t _M0L3endS2903 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS996 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2902,
                                                       _M0L3endS2903,
                                                       _M0L5bytesS2901};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS996;
                  goto join_810;
                }
              } else if (_M0L4_2axS912 == 240) {
                moonbit_bytes_t _M0L5bytesS2898 = _M0L2bsS809.$0;
                int32_t _M0L5startS2900 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2899 = _M0L5startS2900 + 1;
                int32_t _M0L4_2axS997 = _M0L5bytesS2898[_M0L6_2atmpS2899];
                if (_M0L4_2axS997 >= 144 && _M0L4_2axS997 <= 191) {
                  moonbit_bytes_t _M0L5bytesS2894 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2897 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2895 = _M0L5startS2897 + 2;
                  int32_t _M0L3endS2896 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS998 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2895,
                                                       _M0L3endS2896,
                                                       _M0L5bytesS2894};
                  _M0L4restS815 = _M0L4_2axS998;
                  _M0L4tlenS816 = _M0L4tlenS808;
                  goto join_814;
                } else {
                  moonbit_bytes_t _M0L5bytesS2890 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2893 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2891 = _M0L5startS2893 + 1;
                  int32_t _M0L3endS2892 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS999 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2891,
                                                       _M0L3endS2892,
                                                       _M0L5bytesS2890};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS999;
                  goto join_810;
                }
              } else if (_M0L4_2axS912 >= 241 && _M0L4_2axS912 <= 243) {
                moonbit_bytes_t _M0L5bytesS2887 = _M0L2bsS809.$0;
                int32_t _M0L5startS2889 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2888 = _M0L5startS2889 + 1;
                int32_t _M0L4_2axS1000 = _M0L5bytesS2887[_M0L6_2atmpS2888];
                if (_M0L4_2axS1000 >= 128 && _M0L4_2axS1000 <= 191) {
                  moonbit_bytes_t _M0L5bytesS2883 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2886 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2884 = _M0L5startS2886 + 2;
                  int32_t _M0L3endS2885 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1001 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2884,
                                                       _M0L3endS2885,
                                                       _M0L5bytesS2883};
                  _M0L4restS815 = _M0L4_2axS1001;
                  _M0L4tlenS816 = _M0L4tlenS808;
                  goto join_814;
                } else {
                  moonbit_bytes_t _M0L5bytesS2879 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2882 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2880 = _M0L5startS2882 + 1;
                  int32_t _M0L3endS2881 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1002 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2880,
                                                       _M0L3endS2881,
                                                       _M0L5bytesS2879};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS1002;
                  goto join_810;
                }
              } else if (_M0L4_2axS912 == 244) {
                moonbit_bytes_t _M0L5bytesS2876 = _M0L2bsS809.$0;
                int32_t _M0L5startS2878 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2877 = _M0L5startS2878 + 1;
                int32_t _M0L4_2axS1003 = _M0L5bytesS2876[_M0L6_2atmpS2877];
                if (_M0L4_2axS1003 >= 128 && _M0L4_2axS1003 <= 143) {
                  moonbit_bytes_t _M0L5bytesS2872 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2875 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2873 = _M0L5startS2875 + 2;
                  int32_t _M0L3endS2874 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1004 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2873,
                                                       _M0L3endS2874,
                                                       _M0L5bytesS2872};
                  _M0L4restS815 = _M0L4_2axS1004;
                  _M0L4tlenS816 = _M0L4tlenS808;
                  goto join_814;
                } else {
                  moonbit_bytes_t _M0L5bytesS2868 = _M0L2bsS809.$0;
                  int32_t _M0L5startS2871 = _M0L2bsS809.$1;
                  int32_t _M0L6_2atmpS2869 = _M0L5startS2871 + 1;
                  int32_t _M0L3endS2870 = _M0L2bsS809.$2;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1005 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2869,
                                                       _M0L3endS2870,
                                                       _M0L5bytesS2868};
                  _M0L4tlenS811 = _M0L4tlenS808;
                  _M0L4restS812 = _M0L4_2axS1005;
                  goto join_810;
                }
              } else {
                moonbit_bytes_t _M0L5bytesS2864 = _M0L2bsS809.$0;
                int32_t _M0L5startS2867 = _M0L2bsS809.$1;
                int32_t _M0L6_2atmpS2865 = _M0L5startS2867 + 1;
                int32_t _M0L3endS2866 = _M0L2bsS809.$2;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1006 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2865,
                                                     _M0L3endS2866,
                                                     _M0L5bytesS2864};
                _M0L4tlenS811 = _M0L4tlenS808;
                _M0L4restS812 = _M0L4_2axS1006;
                goto join_810;
              }
            }
          } else {
            moonbit_bytes_t _M0L5bytesS2970 = _M0L2bsS809.$0;
            int32_t _M0L5startS2973 = _M0L2bsS809.$1;
            int32_t _M0L6_2atmpS2971 = _M0L5startS2973 + 1;
            int32_t _M0L3endS2972 = _M0L2bsS809.$2;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1007 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2971,
                                                 _M0L3endS2972,
                                                 _M0L5bytesS2970};
            _M0L4tlenS811 = _M0L4tlenS808;
            _M0L4restS812 = _M0L4_2axS1007;
            goto join_810;
          }
        }
      }
    }
    goto joinlet_4133;
    join_847:;
    _M0L1tS806[_M0L4tlenS848] = _M0L1bS850;
    _M0L6_2atmpS2358 = _M0L4tlenS848 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2358;
    _M0L2bsS809 = _M0L4restS849;
    continue;
    joinlet_4133:;
    goto joinlet_4132;
    join_841:;
    _M0L6_2atmpS2357 = (int32_t)_M0L2b0S844;
    _M0L6_2atmpS2356 = _M0L6_2atmpS2357 & 31;
    _M0L6_2atmpS2353 = _M0L6_2atmpS2356 << 6;
    _M0L6_2atmpS2355 = (int32_t)_M0L2b1S845;
    _M0L6_2atmpS2354 = _M0L6_2atmpS2355 & 63;
    _M0L2chS846 = _M0L6_2atmpS2353 | _M0L6_2atmpS2354;
    _M0L6_2atmpS2348 = _M0L2chS846 & 0xff;
    _M0L1tS806[_M0L4tlenS842] = _M0L6_2atmpS2348;
    _M0L6_2atmpS2349 = _M0L4tlenS842 + 1;
    _M0L6_2atmpS2351 = _M0L2chS846 >> 8;
    _M0L6_2atmpS2350 = _M0L6_2atmpS2351 & 0xff;
    _M0L1tS806[_M0L6_2atmpS2349] = _M0L6_2atmpS2350;
    _M0L6_2atmpS2352 = _M0L4tlenS842 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2352;
    _M0L2bsS809 = _M0L4restS843;
    continue;
    joinlet_4132:;
    goto joinlet_4131;
    join_834:;
    _M0L6_2atmpS2347 = (int32_t)_M0L2b0S836;
    _M0L6_2atmpS2346 = _M0L6_2atmpS2347 & 15;
    _M0L6_2atmpS2342 = _M0L6_2atmpS2346 << 12;
    _M0L6_2atmpS2345 = (int32_t)_M0L2b1S837;
    _M0L6_2atmpS2344 = _M0L6_2atmpS2345 & 63;
    _M0L6_2atmpS2343 = _M0L6_2atmpS2344 << 6;
    _M0L6_2atmpS2339 = _M0L6_2atmpS2342 | _M0L6_2atmpS2343;
    _M0L6_2atmpS2341 = (int32_t)_M0L2b2S838;
    _M0L6_2atmpS2340 = _M0L6_2atmpS2341 & 63;
    _M0L2chS840 = _M0L6_2atmpS2339 | _M0L6_2atmpS2340;
    _M0L6_2atmpS2334 = _M0L2chS840 & 0xff;
    _M0L1tS806[_M0L4tlenS835] = _M0L6_2atmpS2334;
    _M0L6_2atmpS2335 = _M0L4tlenS835 + 1;
    _M0L6_2atmpS2337 = _M0L2chS840 >> 8;
    _M0L6_2atmpS2336 = _M0L6_2atmpS2337 & 0xff;
    _M0L1tS806[_M0L6_2atmpS2335] = _M0L6_2atmpS2336;
    _M0L6_2atmpS2338 = _M0L4tlenS835 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2338;
    _M0L2bsS809 = _M0L4restS839;
    continue;
    joinlet_4131:;
    goto joinlet_4130;
    join_823:;
    _M0L6_2atmpS2333 = (int32_t)_M0L2b0S825;
    _M0L6_2atmpS2332 = _M0L6_2atmpS2333 & 7;
    _M0L6_2atmpS2328 = _M0L6_2atmpS2332 << 18;
    _M0L6_2atmpS2331 = (int32_t)_M0L2b1S826;
    _M0L6_2atmpS2330 = _M0L6_2atmpS2331 & 63;
    _M0L6_2atmpS2329 = _M0L6_2atmpS2330 << 12;
    _M0L6_2atmpS2324 = _M0L6_2atmpS2328 | _M0L6_2atmpS2329;
    _M0L6_2atmpS2327 = (int32_t)_M0L2b2S827;
    _M0L6_2atmpS2326 = _M0L6_2atmpS2327 & 63;
    _M0L6_2atmpS2325 = _M0L6_2atmpS2326 << 6;
    _M0L6_2atmpS2321 = _M0L6_2atmpS2324 | _M0L6_2atmpS2325;
    _M0L6_2atmpS2323 = (int32_t)_M0L2b3S828;
    _M0L6_2atmpS2322 = _M0L6_2atmpS2323 & 63;
    _M0L2chS830 = _M0L6_2atmpS2321 | _M0L6_2atmpS2322;
    _M0L3chmS831 = _M0L2chS830 - 65536;
    _M0L6_2atmpS2320 = _M0L3chmS831 >> 10;
    _M0L3ch1S832 = _M0L6_2atmpS2320 + 55296;
    _M0L6_2atmpS2319 = _M0L3chmS831 & 1023;
    _M0L3ch2S833 = _M0L6_2atmpS2319 + 56320;
    _M0L6_2atmpS2309 = _M0L3ch1S832 & 0xff;
    _M0L1tS806[_M0L4tlenS824] = _M0L6_2atmpS2309;
    _M0L6_2atmpS2310 = _M0L4tlenS824 + 1;
    _M0L6_2atmpS2312 = _M0L3ch1S832 >> 8;
    _M0L6_2atmpS2311 = _M0L6_2atmpS2312 & 0xff;
    _M0L1tS806[_M0L6_2atmpS2310] = _M0L6_2atmpS2311;
    _M0L6_2atmpS2313 = _M0L4tlenS824 + 2;
    _M0L6_2atmpS2314 = _M0L3ch2S833 & 0xff;
    _M0L1tS806[_M0L6_2atmpS2313] = _M0L6_2atmpS2314;
    _M0L6_2atmpS2315 = _M0L4tlenS824 + 3;
    _M0L6_2atmpS2317 = _M0L3ch2S833 >> 8;
    _M0L6_2atmpS2316 = _M0L6_2atmpS2317 & 0xff;
    _M0L1tS806[_M0L6_2atmpS2315] = _M0L6_2atmpS2316;
    _M0L6_2atmpS2318 = _M0L4tlenS824 + 4;
    _M0L4tlenS808 = _M0L6_2atmpS2318;
    _M0L2bsS809 = _M0L4restS829;
    continue;
    joinlet_4130:;
    goto joinlet_4129;
    join_820:;
    _M0L1tS806[_M0L4tlenS822] = 253;
    _M0L6_2atmpS2307 = _M0L4tlenS822 + 1;
    _M0L1tS806[_M0L6_2atmpS2307] = 255;
    _M0L6_2atmpS2308 = _M0L4tlenS822 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2308;
    _M0L2bsS809 = _M0L4restS821;
    continue;
    joinlet_4129:;
    goto joinlet_4128;
    join_817:;
    _M0L1tS806[_M0L4tlenS819] = 253;
    _M0L6_2atmpS2305 = _M0L4tlenS819 + 1;
    _M0L1tS806[_M0L6_2atmpS2305] = 255;
    _M0L6_2atmpS2306 = _M0L4tlenS819 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2306;
    _M0L2bsS809 = _M0L4restS818;
    continue;
    joinlet_4128:;
    goto joinlet_4127;
    join_814:;
    _M0L1tS806[_M0L4tlenS816] = 253;
    _M0L6_2atmpS2303 = _M0L4tlenS816 + 1;
    _M0L1tS806[_M0L6_2atmpS2303] = 255;
    _M0L6_2atmpS2304 = _M0L4tlenS816 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2304;
    _M0L2bsS809 = _M0L4restS815;
    continue;
    joinlet_4127:;
    goto joinlet_4126;
    join_810:;
    _M0L1tS806[_M0L4tlenS811] = 253;
    _M0L6_2atmpS2301 = _M0L4tlenS811 + 1;
    _M0L1tS806[_M0L6_2atmpS2301] = 255;
    _M0L6_2atmpS2302 = _M0L4tlenS811 + 2;
    _M0L4tlenS808 = _M0L6_2atmpS2302;
    _M0L2bsS809 = _M0L4restS812;
    continue;
    joinlet_4126:;
    _tmp_4134 = _M0L4tlenS808;
    _tmp_4135 = _M0L2bsS809;
    _M0L4tlenS808 = _tmp_4134;
    _M0L2bsS809 = _tmp_4135;
    continue;
    break;
  }
  _M0L6_2atmpS2299 = _M0L1tS806;
  _M0L6_2atmpS2300 = (int64_t)_M0L4tlenS807;
  #line 263 "/Users/user/.moon/lib/core/encoding/utf8/decode.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2299, 0, _M0L6_2atmpS2300);
}

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView _M0L3strS783,
  int32_t _M0L3bomS789
) {
  int32_t _M0L6lengthS782;
  int32_t _M0L12utf8__lengthS784;
  int32_t _M0L1iS785;
  int32_t _M0L12utf8__lengthS786;
  moonbit_bytes_t _M0L3arrS790;
  int32_t _M0L6_2atmpS2287;
  int32_t _M0L1iS791;
  int32_t _M0L6offsetS792;
  #line 28 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
  moonbit_incref(_M0L3strS783.$0);
  #line 29 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
  _M0L6lengthS782 = _M0MPC16string10StringView6length(_M0L3strS783);
  _M0L1iS785 = 0;
  _M0L12utf8__lengthS786 = 0;
  while (1) {
    if (_M0L1iS785 < _M0L6lengthS782) {
      int32_t _M0L4codeS787;
      int32_t _tmp_4138;
      int32_t _tmp_4139;
      moonbit_incref(_M0L3strS783.$0);
      #line 31 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      _M0L4codeS787
      = _M0MPC16string10StringView11unsafe__get(_M0L3strS783, _M0L1iS785);
      if (_M0L4codeS787 < 128) {
        int32_t _M0L6_2atmpS2288 = _M0L1iS785 + 1;
        int32_t _M0L6_2atmpS2289 = _M0L12utf8__lengthS786 + 1;
        _M0L1iS785 = _M0L6_2atmpS2288;
        _M0L12utf8__lengthS786 = _M0L6_2atmpS2289;
        continue;
      } else if (_M0L4codeS787 < 2048) {
        int32_t _M0L6_2atmpS2290 = _M0L1iS785 + 1;
        int32_t _M0L6_2atmpS2291 = _M0L12utf8__lengthS786 + 2;
        _M0L1iS785 = _M0L6_2atmpS2290;
        _M0L12utf8__lengthS786 = _M0L6_2atmpS2291;
        continue;
      } else {
        int32_t _if__result_4137;
        #line 36 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L4codeS787)) {
          int32_t _M0L6_2atmpS2294 = _M0L1iS785 + 1;
          if (_M0L6_2atmpS2294 < _M0L6lengthS782) {
            int32_t _M0L6_2atmpS2293 = _M0L1iS785 + 1;
            int32_t _M0L6_2atmpS2292;
            moonbit_incref(_M0L3strS783.$0);
            #line 38 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0L6_2atmpS2292
            = _M0MPC16string10StringView11unsafe__get(_M0L3strS783, _M0L6_2atmpS2293);
            #line 38 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _if__result_4137
            = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2292);
          } else {
            _if__result_4137 = 0;
          }
        } else {
          _if__result_4137 = 0;
        }
        if (_if__result_4137) {
          int32_t _M0L6_2atmpS2295 = _M0L1iS785 + 2;
          int32_t _M0L6_2atmpS2296 = _M0L12utf8__lengthS786 + 4;
          _M0L1iS785 = _M0L6_2atmpS2295;
          _M0L12utf8__lengthS786 = _M0L6_2atmpS2296;
          continue;
        } else {
          #line 40 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          if (_M0MPC16uint166UInt1613is__surrogate(_M0L4codeS787)) {
            #line 41 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_44.data);
          } else {
            int32_t _M0L6_2atmpS2297 = _M0L1iS785 + 1;
            int32_t _M0L6_2atmpS2298 = _M0L12utf8__lengthS786 + 3;
            _M0L1iS785 = _M0L6_2atmpS2297;
            _M0L12utf8__lengthS786 = _M0L6_2atmpS2298;
            continue;
          }
        }
      }
      _tmp_4138 = _M0L1iS785;
      _tmp_4139 = _M0L12utf8__lengthS786;
      _M0L1iS785 = _tmp_4138;
      _M0L12utf8__lengthS786 = _tmp_4139;
      continue;
    } else if (_M0L3bomS789) {
      _M0L12utf8__lengthS784 = 3 + _M0L12utf8__lengthS786;
    } else {
      _M0L12utf8__lengthS784 = _M0L12utf8__lengthS786;
    }
    break;
  }
  _M0L3arrS790
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L12utf8__lengthS784, 0);
  if (_M0L3bomS789) {
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L3arrS790)) {
      #line 55 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      moonbit_panic();
    }
    _M0L3arrS790[0] = 239;
    if (1 < 0 || 1 >= Moonbit_array_length(_M0L3arrS790)) {
      #line 56 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      moonbit_panic();
    }
    _M0L3arrS790[1] = 187;
    if (2 < 0 || 2 >= Moonbit_array_length(_M0L3arrS790)) {
      #line 57 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      moonbit_panic();
    }
    _M0L3arrS790[2] = 191;
    _M0L6_2atmpS2287 = 3;
  } else {
    _M0L6_2atmpS2287 = 0;
  }
  _M0L1iS791 = 0;
  _M0L6offsetS792 = _M0L6_2atmpS2287;
  while (1) {
    if (_M0L1iS791 < _M0L6lengthS782) {
      int32_t _M0L10code__unitS793;
      int32_t _M0L4codeS794;
      int32_t _tmp_4142;
      int32_t _tmp_4143;
      moonbit_incref(_M0L3strS783.$0);
      #line 62 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      _M0L10code__unitS793
      = _M0MPC16string10StringView11unsafe__get(_M0L3strS783, _M0L1iS791);
      _M0L4codeS794 = (int32_t)_M0L10code__unitS793;
      if (_M0L4codeS794 < 128) {
        int32_t _M0L6_2atmpS2235 = _M0L4codeS794 & 0xff;
        int32_t _M0L6_2atmpS2236;
        int32_t _M0L6_2atmpS2237;
        if (
          _M0L6offsetS792 < 0
          || _M0L6offsetS792 >= Moonbit_array_length(_M0L3arrS790)
        ) {
          #line 65 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          moonbit_panic();
        }
        _M0L3arrS790[_M0L6offsetS792] = _M0L6_2atmpS2235;
        _M0L6_2atmpS2236 = _M0L1iS791 + 1;
        _M0L6_2atmpS2237 = _M0L6offsetS792 + 1;
        _M0L1iS791 = _M0L6_2atmpS2236;
        _M0L6offsetS792 = _M0L6_2atmpS2237;
        continue;
      } else if (_M0L4codeS794 < 2048) {
        int32_t _M0L6_2atmpS2240 = _M0L4codeS794 >> 6;
        int32_t _M0L6_2atmpS2239 = 192 + _M0L6_2atmpS2240;
        int32_t _M0L6_2atmpS2238 = _M0L6_2atmpS2239 & 0xff;
        int32_t _M0L6_2atmpS2241;
        int32_t _M0L6_2atmpS2244;
        int32_t _M0L6_2atmpS2243;
        int32_t _M0L6_2atmpS2242;
        int32_t _M0L6_2atmpS2245;
        int32_t _M0L6_2atmpS2246;
        if (
          _M0L6offsetS792 < 0
          || _M0L6offsetS792 >= Moonbit_array_length(_M0L3arrS790)
        ) {
          #line 68 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          moonbit_panic();
        }
        _M0L3arrS790[_M0L6offsetS792] = _M0L6_2atmpS2238;
        _M0L6_2atmpS2241 = _M0L6offsetS792 + 1;
        _M0L6_2atmpS2244 = _M0L4codeS794 & 63;
        _M0L6_2atmpS2243 = 128 + _M0L6_2atmpS2244;
        _M0L6_2atmpS2242 = _M0L6_2atmpS2243 & 0xff;
        if (
          _M0L6_2atmpS2241 < 0
          || _M0L6_2atmpS2241 >= Moonbit_array_length(_M0L3arrS790)
        ) {
          #line 69 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          moonbit_panic();
        }
        _M0L3arrS790[_M0L6_2atmpS2241] = _M0L6_2atmpS2242;
        _M0L6_2atmpS2245 = _M0L1iS791 + 1;
        _M0L6_2atmpS2246 = _M0L6offsetS792 + 2;
        _M0L1iS791 = _M0L6_2atmpS2245;
        _M0L6offsetS792 = _M0L6_2atmpS2246;
        continue;
      } else {
        int32_t _if__result_4141;
        #line 71 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
        if (
          _M0MPC16uint166UInt1622is__leading__surrogate(_M0L10code__unitS793)
        ) {
          int32_t _M0L6_2atmpS2247 = _M0L1iS791 + 1;
          _if__result_4141 = _M0L6_2atmpS2247 < _M0L6lengthS782;
        } else {
          _if__result_4141 = 0;
        }
        if (_if__result_4141) {
          int32_t _M0L6_2atmpS2272 = _M0L1iS791 + 1;
          int32_t _M0L8trailingS796;
          moonbit_incref(_M0L3strS783.$0);
          #line 72 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          _M0L8trailingS796
          = _M0MPC16string10StringView11unsafe__get(_M0L3strS783, _M0L6_2atmpS2272);
          #line 73 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          if (
            _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L8trailingS796)
          ) {
            int32_t _M0L6_2atmpS2271 = _M0L4codeS794 - 55296;
            int32_t _M0L6_2atmpS2268 = _M0L6_2atmpS2271 << 10;
            int32_t _M0L6_2atmpS2270 = (int32_t)_M0L8trailingS796;
            int32_t _M0L6_2atmpS2269 = _M0L6_2atmpS2270 - 56320;
            int32_t _M0L6_2atmpS2267 = _M0L6_2atmpS2268 + _M0L6_2atmpS2269;
            int32_t _M0L4codeS797 = _M0L6_2atmpS2267 + 65536;
            int32_t _M0L6_2atmpS2250 = _M0L4codeS797 >> 18;
            int32_t _M0L6_2atmpS2249 = 240 + _M0L6_2atmpS2250;
            int32_t _M0L6_2atmpS2248 = _M0L6_2atmpS2249 & 0xff;
            int32_t _M0L6_2atmpS2251;
            int32_t _M0L6_2atmpS2255;
            int32_t _M0L6_2atmpS2254;
            int32_t _M0L6_2atmpS2253;
            int32_t _M0L6_2atmpS2252;
            int32_t _M0L6_2atmpS2256;
            int32_t _M0L6_2atmpS2260;
            int32_t _M0L6_2atmpS2259;
            int32_t _M0L6_2atmpS2258;
            int32_t _M0L6_2atmpS2257;
            int32_t _M0L6_2atmpS2261;
            int32_t _M0L6_2atmpS2264;
            int32_t _M0L6_2atmpS2263;
            int32_t _M0L6_2atmpS2262;
            int32_t _M0L6_2atmpS2265;
            int32_t _M0L6_2atmpS2266;
            if (
              _M0L6offsetS792 < 0
              || _M0L6offsetS792 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 77 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6offsetS792] = _M0L6_2atmpS2248;
            _M0L6_2atmpS2251 = _M0L6offsetS792 + 1;
            _M0L6_2atmpS2255 = _M0L4codeS797 >> 12;
            _M0L6_2atmpS2254 = _M0L6_2atmpS2255 & 63;
            _M0L6_2atmpS2253 = 128 + _M0L6_2atmpS2254;
            _M0L6_2atmpS2252 = _M0L6_2atmpS2253 & 0xff;
            if (
              _M0L6_2atmpS2251 < 0
              || _M0L6_2atmpS2251 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 78 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6_2atmpS2251] = _M0L6_2atmpS2252;
            _M0L6_2atmpS2256 = _M0L6offsetS792 + 2;
            _M0L6_2atmpS2260 = _M0L4codeS797 >> 6;
            _M0L6_2atmpS2259 = _M0L6_2atmpS2260 & 63;
            _M0L6_2atmpS2258 = 128 + _M0L6_2atmpS2259;
            _M0L6_2atmpS2257 = _M0L6_2atmpS2258 & 0xff;
            if (
              _M0L6_2atmpS2256 < 0
              || _M0L6_2atmpS2256 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 79 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6_2atmpS2256] = _M0L6_2atmpS2257;
            _M0L6_2atmpS2261 = _M0L6offsetS792 + 3;
            _M0L6_2atmpS2264 = _M0L4codeS797 & 63;
            _M0L6_2atmpS2263 = 128 + _M0L6_2atmpS2264;
            _M0L6_2atmpS2262 = _M0L6_2atmpS2263 & 0xff;
            if (
              _M0L6_2atmpS2261 < 0
              || _M0L6_2atmpS2261 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 80 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6_2atmpS2261] = _M0L6_2atmpS2262;
            _M0L6_2atmpS2265 = _M0L1iS791 + 2;
            _M0L6_2atmpS2266 = _M0L6offsetS792 + 4;
            _M0L1iS791 = _M0L6_2atmpS2265;
            _M0L6offsetS792 = _M0L6_2atmpS2266;
            continue;
          } else {
            #line 83 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_44.data);
          }
        } else {
          #line 85 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          if (_M0MPC16uint166UInt1613is__surrogate(_M0L10code__unitS793)) {
            #line 86 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_44.data);
          } else {
            int32_t _M0L6_2atmpS2275 = _M0L4codeS794 >> 12;
            int32_t _M0L6_2atmpS2274 = 224 + _M0L6_2atmpS2275;
            int32_t _M0L6_2atmpS2273 = _M0L6_2atmpS2274 & 0xff;
            int32_t _M0L6_2atmpS2276;
            int32_t _M0L6_2atmpS2280;
            int32_t _M0L6_2atmpS2279;
            int32_t _M0L6_2atmpS2278;
            int32_t _M0L6_2atmpS2277;
            int32_t _M0L6_2atmpS2281;
            int32_t _M0L6_2atmpS2284;
            int32_t _M0L6_2atmpS2283;
            int32_t _M0L6_2atmpS2282;
            int32_t _M0L6_2atmpS2285;
            int32_t _M0L6_2atmpS2286;
            if (
              _M0L6offsetS792 < 0
              || _M0L6offsetS792 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 88 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6offsetS792] = _M0L6_2atmpS2273;
            _M0L6_2atmpS2276 = _M0L6offsetS792 + 1;
            _M0L6_2atmpS2280 = _M0L4codeS794 >> 6;
            _M0L6_2atmpS2279 = _M0L6_2atmpS2280 & 63;
            _M0L6_2atmpS2278 = 128 + _M0L6_2atmpS2279;
            _M0L6_2atmpS2277 = _M0L6_2atmpS2278 & 0xff;
            if (
              _M0L6_2atmpS2276 < 0
              || _M0L6_2atmpS2276 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 89 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6_2atmpS2276] = _M0L6_2atmpS2277;
            _M0L6_2atmpS2281 = _M0L6offsetS792 + 2;
            _M0L6_2atmpS2284 = _M0L4codeS794 & 63;
            _M0L6_2atmpS2283 = 128 + _M0L6_2atmpS2284;
            _M0L6_2atmpS2282 = _M0L6_2atmpS2283 & 0xff;
            if (
              _M0L6_2atmpS2281 < 0
              || _M0L6_2atmpS2281 >= Moonbit_array_length(_M0L3arrS790)
            ) {
              #line 90 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS790[_M0L6_2atmpS2281] = _M0L6_2atmpS2282;
            _M0L6_2atmpS2285 = _M0L1iS791 + 1;
            _M0L6_2atmpS2286 = _M0L6offsetS792 + 3;
            _M0L1iS791 = _M0L6_2atmpS2285;
            _M0L6offsetS792 = _M0L6_2atmpS2286;
            continue;
          }
        }
      }
      _tmp_4142 = _M0L1iS791;
      _tmp_4143 = _M0L6offsetS792;
      _M0L1iS791 = _tmp_4142;
      _M0L6offsetS792 = _tmp_4143;
      continue;
    } else {
      moonbit_decref(_M0L3strS783.$0);
    }
    break;
  }
  return _M0L3arrS790;
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS780,
  struct _M0TPC16string10StringView _M0L9separatorS781
) {
  moonbit_string_t* _M0L3bufS2233;
  int32_t _M0L3lenS2234;
  int32_t _M0L6_2acntS3933;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2232;
  #line 2070 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS2233 = _M0L4selfS780->$0;
  _M0L3lenS2234 = _M0L4selfS780->$1;
  _M0L6_2acntS3933 = Moonbit_object_header(_M0L4selfS780)->rc;
  if (_M0L6_2acntS3933 > 1) {
    int32_t _M0L11_2anew__cntS3934 = _M0L6_2acntS3933 - 1;
    Moonbit_object_header(_M0L4selfS780)->rc = _M0L11_2anew__cntS3934;
    moonbit_incref(_M0L3bufS2233);
  } else if (_M0L6_2acntS3933 == 1) {
    #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS780);
  }
  _M0L6_2atmpS2232
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS2234, _M0L3bufS2233
  };
  #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS2232, _M0L9separatorS781);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS775,
  int32_t _M0L5indexS776
) {
  int32_t _M0L3lenS774;
  int32_t _if__result_4144;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS774 = _M0L4selfS775->$1;
  if (_M0L5indexS776 >= 0) {
    _if__result_4144 = _M0L5indexS776 < _M0L3lenS774;
  } else {
    _if__result_4144 = 0;
  }
  if (_if__result_4144) {
    moonbit_string_t* _M0L6_2atmpS2230;
    moonbit_string_t _M0L6_2atmpS3667;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS2230 = _M0MPC15array5Array6bufferGsE(_M0L4selfS775);
    if (
      _M0L5indexS776 < 0
      || _M0L5indexS776 >= Moonbit_array_length(_M0L6_2atmpS2230)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3667 = (moonbit_string_t)_M0L6_2atmpS2230[_M0L5indexS776];
    moonbit_incref(_M0L6_2atmpS3667);
    moonbit_decref(_M0L6_2atmpS2230);
    return _M0L6_2atmpS3667;
  } else {
    moonbit_decref(_M0L4selfS775);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TP36mulpjs4mulp6stream4File* _M0MPC15array5Array2atGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS778,
  int32_t _M0L5indexS779
) {
  int32_t _M0L3lenS777;
  int32_t _if__result_4145;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS777 = _M0L4selfS778->$1;
  if (_M0L5indexS779 >= 0) {
    _if__result_4145 = _M0L5indexS779 < _M0L3lenS777;
  } else {
    _if__result_4145 = 0;
  }
  if (_if__result_4145) {
    struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2231;
    struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS3668;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS2231
    = _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(_M0L4selfS778);
    if (
      _M0L5indexS779 < 0
      || _M0L5indexS779 >= Moonbit_array_length(_M0L6_2atmpS2231)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3668
    = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L6_2atmpS2231[
        _M0L5indexS779
      ];
    if (_M0L6_2atmpS3668) {
      moonbit_incref(_M0L6_2atmpS3668);
    }
    moonbit_decref(_M0L6_2atmpS2231);
    return _M0L6_2atmpS3668;
  } else {
    moonbit_decref(_M0L4selfS778);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS749,
  struct _M0TPC16string10StringView _M0L9separatorS761
) {
  int32_t _M0L3endS2209;
  int32_t _M0L5startS2210;
  int32_t _M0L6_2atmpS2208;
  #line 1369 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2209 = _M0L4selfS749.$2;
  _M0L5startS2210 = _M0L4selfS749.$1;
  _M0L6_2atmpS2208 = _M0L3endS2209 - _M0L5startS2210;
  if (_M0L6_2atmpS2208 == 0) {
    moonbit_decref(_M0L9separatorS761.$0);
    moonbit_decref(_M0L4selfS749.$0);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t* _M0L3bufS2228 = _M0L4selfS749.$0;
    int32_t _M0L5startS2229 = _M0L4selfS749.$1;
    moonbit_string_t _M0L5_2ahdS750 =
      (moonbit_string_t)_M0L3bufS2228[_M0L5startS2229];
    moonbit_string_t* _M0L9_2ax__bufS751 = _M0L4selfS749.$0;
    int32_t _M0L5startS2227 = _M0L4selfS749.$1;
    int32_t _M0L11_2ax__startS752 = 1 + _M0L5startS2227;
    int32_t _M0L9_2ax__endS753 = _M0L4selfS749.$2;
    struct _M0TPC16string10StringView _M0L2hdS754;
    int32_t _M0L7_2abindS755;
    int32_t _M0L6_2atmpS2226;
    int32_t _M0L10size__hintS756;
    int32_t _M0L2__S757;
    int32_t _M0L10size__hintS758;
    int32_t _M0L10size__hintS762;
    struct _M0TPB13StringBuilder* _M0L3bufS763;
    moonbit_string_t _M0L3strS2211;
    int32_t _M0L5startS2212;
    int32_t _M0L3endS2214;
    int64_t _M0L6_2atmpS2213;
    moonbit_incref(_M0L5_2ahdS750);
    #line 1376 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS754
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS750);
    _M0L7_2abindS755 = _M0L9_2ax__endS753 - _M0L11_2ax__startS752;
    moonbit_incref(_M0L2hdS754.$0);
    #line 1377 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L6_2atmpS2226 = _M0MPC16string10StringView6length(_M0L2hdS754);
    _M0L2__S757 = 0;
    _M0L10size__hintS758 = _M0L6_2atmpS2226;
    while (1) {
      if (_M0L2__S757 < _M0L7_2abindS755) {
        int32_t _M0L6_2atmpS2225 = _M0L11_2ax__startS752 + _M0L2__S757;
        moonbit_string_t _M0L1sS759 =
          (moonbit_string_t)_M0L9_2ax__bufS751[_M0L6_2atmpS2225];
        int32_t _M0L6_2atmpS2219 = _M0L2__S757 + 1;
        struct _M0TPC16string10StringView _M0L6_2atmpS2224;
        int32_t _M0L6_2atmpS2223;
        int32_t _M0L6_2atmpS2221;
        int32_t _M0L6_2atmpS2222;
        int32_t _M0L6_2atmpS2220;
        moonbit_incref(_M0L1sS759);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS2224
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS759);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS2223
        = _M0MPC16string10StringView6length(_M0L6_2atmpS2224);
        _M0L6_2atmpS2221 = _M0L10size__hintS758 + _M0L6_2atmpS2223;
        moonbit_incref(_M0L9separatorS761.$0);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS2222
        = _M0MPC16string10StringView6length(_M0L9separatorS761);
        _M0L6_2atmpS2220 = _M0L6_2atmpS2221 + _M0L6_2atmpS2222;
        _M0L2__S757 = _M0L6_2atmpS2219;
        _M0L10size__hintS758 = _M0L6_2atmpS2220;
        continue;
      } else {
        _M0L10size__hintS756 = _M0L10size__hintS758;
      }
      break;
    }
    _M0L10size__hintS762 = _M0L10size__hintS756 << 1;
    #line 1383 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS763 = _M0MPB13StringBuilder11new_2einner(_M0L10size__hintS762);
    moonbit_incref(_M0L3bufS763);
    #line 1385 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS763, _M0L2hdS754);
    _M0L3strS2211 = _M0L9separatorS761.$0;
    _M0L5startS2212 = _M0L9separatorS761.$1;
    _M0L3endS2214 = _M0L9separatorS761.$2;
    _M0L6_2atmpS2213 = (int64_t)_M0L3endS2214;
    moonbit_incref(_M0L3strS2211);
    #line 1386 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2211, 0, _M0L5startS2212, _M0L6_2atmpS2213)
    ) {
      int32_t _M0L7_2abindS764;
      int32_t _M0L2__S765;
      moonbit_decref(_M0L9separatorS761.$0);
      _M0L7_2abindS764 = _M0L9_2ax__endS753 - _M0L11_2ax__startS752;
      _M0L2__S765 = 0;
      while (1) {
        if (_M0L2__S765 < _M0L7_2abindS764) {
          int32_t _M0L6_2atmpS2216 = _M0L11_2ax__startS752 + _M0L2__S765;
          moonbit_string_t _M0L1sS766 =
            (moonbit_string_t)_M0L9_2ax__bufS751[_M0L6_2atmpS2216];
          struct _M0TPC16string10StringView _M0L1sS767;
          int32_t _M0L6_2atmpS2215;
          moonbit_incref(_M0L1sS766);
          #line 1389 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS767
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS766);
          moonbit_incref(_M0L3bufS763);
          #line 1390 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS763, _M0L1sS767);
          _M0L6_2atmpS2215 = _M0L2__S765 + 1;
          _M0L2__S765 = _M0L6_2atmpS2215;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS751);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS769 = _M0L9_2ax__endS753 - _M0L11_2ax__startS752;
      int32_t _M0L2__S770 = 0;
      while (1) {
        if (_M0L2__S770 < _M0L7_2abindS769) {
          int32_t _M0L6_2atmpS2218 = _M0L11_2ax__startS752 + _M0L2__S770;
          moonbit_string_t _M0L1sS771 =
            (moonbit_string_t)_M0L9_2ax__bufS751[_M0L6_2atmpS2218];
          struct _M0TPC16string10StringView _M0L1sS772;
          int32_t _M0L6_2atmpS2217;
          moonbit_incref(_M0L1sS771);
          #line 1394 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS772
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS771);
          moonbit_incref(_M0L3bufS763);
          moonbit_incref(_M0L9separatorS761.$0);
          #line 1395 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS763, _M0L9separatorS761);
          moonbit_incref(_M0L3bufS763);
          #line 1397 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS763, _M0L1sS772);
          _M0L6_2atmpS2217 = _M0L2__S770 + 1;
          _M0L2__S770 = _M0L6_2atmpS2217;
          continue;
        } else {
          moonbit_decref(_M0L9separatorS761.$0);
          moonbit_decref(_M0L9_2ax__bufS751);
        }
        break;
      }
    }
    #line 1400 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS763);
  }
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS741,
  int32_t _M0L5startS747,
  int64_t _M0L3endS743
) {
  int32_t _M0L3lenS740;
  int32_t _M0L3endS742;
  int32_t _M0L5startS746;
  int32_t _if__result_4149;
  #line 170 "/Users/user/.moon/lib/core/builtin/bytesview.mbt"
  _M0L3lenS740 = Moonbit_array_length(_M0L4selfS741);
  if (_M0L3endS743 == 4294967296ll) {
    _M0L3endS742 = _M0L3lenS740;
  } else {
    int64_t _M0L7_2aSomeS744 = _M0L3endS743;
    int32_t _M0L6_2aendS745 = (int32_t)_M0L7_2aSomeS744;
    if (_M0L6_2aendS745 < 0) {
      _M0L3endS742 = _M0L3lenS740 + _M0L6_2aendS745;
    } else {
      _M0L3endS742 = _M0L6_2aendS745;
    }
  }
  if (_M0L5startS747 < 0) {
    _M0L5startS746 = _M0L3lenS740 + _M0L5startS747;
  } else {
    _M0L5startS746 = _M0L5startS747;
  }
  if (_M0L5startS746 >= 0) {
    if (_M0L5startS746 <= _M0L3endS742) {
      _if__result_4149 = _M0L3endS742 <= _M0L3lenS740;
    } else {
      _if__result_4149 = 0;
    }
  } else {
    _if__result_4149 = 0;
  }
  if (_if__result_4149) {
    int32_t _M0L7_2abindS748 = _M0L3endS742 - _M0L5startS746;
    int32_t _M0L6_2atmpS2207 = _M0L5startS746 + _M0L7_2abindS748;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS746,
                                              _M0L6_2atmpS2207,
                                              _M0L4selfS741};
  } else {
    moonbit_decref(_M0L4selfS741);
    #line 180 "/Users/user/.moon/lib/core/builtin/bytesview.mbt"
    return _M0FPC15abort5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_45.data);
  }
}

int32_t _M0MPC15bytes9BytesView6length(
  struct _M0TPC15bytes9BytesView _M0L4selfS739
) {
  int32_t _M0L3endS2205;
  int32_t _M0L5startS2206;
  #line 45 "/Users/user/.moon/lib/core/builtin/bytesview.mbt"
  _M0L3endS2205 = _M0L4selfS739.$2;
  _M0L5startS2206 = _M0L4selfS739.$1;
  moonbit_decref(_M0L4selfS739.$0);
  return _M0L3endS2205 - _M0L5startS2206;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS738) {
  moonbit_string_t _M0L6_2atmpS2204;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS2204 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS738);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS2204);
  moonbit_decref(_M0L6_2atmpS2204);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS737,
  struct _M0TPB6Hasher* _M0L6hasherS736
) {
  #line 530 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 531 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS736, _M0L4selfS737);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS735,
  struct _M0TPB6Hasher* _M0L6hasherS734
) {
  #line 496 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 497 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS734, _M0L4selfS735);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS732,
  moonbit_string_t _M0L5valueS730
) {
  int32_t _M0L7_2abindS729;
  int32_t _M0L1iS731;
  #line 387 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS729 = Moonbit_array_length(_M0L5valueS730);
  _M0L1iS731 = 0;
  while (1) {
    if (_M0L1iS731 < _M0L7_2abindS729) {
      int32_t _M0L6_2atmpS2202 = _M0L5valueS730[_M0L1iS731];
      int32_t _M0L6_2atmpS2201 = (int32_t)_M0L6_2atmpS2202;
      uint32_t _M0L6_2atmpS2200 = *(uint32_t*)&_M0L6_2atmpS2201;
      int32_t _M0L6_2atmpS2203;
      moonbit_incref(_M0L4selfS732);
      #line 389 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS732, _M0L6_2atmpS2200);
      _M0L6_2atmpS2203 = _M0L1iS731 + 1;
      _M0L1iS731 = _M0L6_2atmpS2203;
      continue;
    } else {
      moonbit_decref(_M0L4selfS732);
      moonbit_decref(_M0L5valueS730);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS727,
  int32_t _M0L3idxS728
) {
  int32_t _result_4151;
  #line 1778 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _result_4151 = _M0L4selfS727[_M0L3idxS728];
  moonbit_decref(_M0L4selfS727);
  return _result_4151;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS714,
  int32_t _M0L3keyS710
) {
  int32_t _M0L4hashS709;
  int32_t _M0L14capacity__maskS2185;
  int32_t _M0L6_2atmpS2184;
  int32_t _M0L1iS711;
  int32_t _M0L3idxS712;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS709 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS710);
  _M0L14capacity__maskS2185 = _M0L4selfS714->$3;
  _M0L6_2atmpS2184 = _M0L4hashS709 & _M0L14capacity__maskS2185;
  _M0L1iS711 = 0;
  _M0L3idxS712 = _M0L6_2atmpS2184;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2183 =
      _M0L4selfS714->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS713;
    if (
      _M0L3idxS712 < 0
      || _M0L3idxS712 >= Moonbit_array_length(_M0L7entriesS2183)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS713
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2183[
        _M0L3idxS712
      ];
    if (_M0L7_2abindS713 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2172;
      if (_M0L7_2abindS713) {
        moonbit_incref(_M0L7_2abindS713);
      }
      moonbit_decref(_M0L4selfS714);
      if (_M0L7_2abindS713) {
        moonbit_decref(_M0L7_2abindS713);
      }
      _M0L6_2atmpS2172 = 0;
      return _M0L6_2atmpS2172;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS715 =
        _M0L7_2abindS713;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS716 =
        _M0L7_2aSomeS715;
      int32_t _M0L4hashS2174 = _M0L8_2aentryS716->$3;
      int32_t _if__result_4153;
      int32_t _M0L3pslS2177;
      int32_t _M0L6_2atmpS2179;
      int32_t _M0L6_2atmpS2181;
      int32_t _M0L14capacity__maskS2182;
      int32_t _M0L6_2atmpS2180;
      if (_M0L4hashS2174 == _M0L4hashS709) {
        int32_t _M0L3keyS2173 = _M0L8_2aentryS716->$4;
        _if__result_4153 = _M0L3keyS2173 == _M0L3keyS710;
      } else {
        _if__result_4153 = 0;
      }
      if (_if__result_4153) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3676;
        int32_t _M0L6_2acntS3935;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2176;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2175;
        moonbit_incref(_M0L8_2aentryS716);
        moonbit_decref(_M0L4selfS714);
        _M0L8_2afieldS3676 = _M0L8_2aentryS716->$5;
        _M0L6_2acntS3935 = Moonbit_object_header(_M0L8_2aentryS716)->rc;
        if (_M0L6_2acntS3935 > 1) {
          int32_t _M0L11_2anew__cntS3937 = _M0L6_2acntS3935 - 1;
          Moonbit_object_header(_M0L8_2aentryS716)->rc
          = _M0L11_2anew__cntS3937;
          moonbit_incref(_M0L8_2afieldS3676);
        } else if (_M0L6_2acntS3935 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3936 =
            _M0L8_2aentryS716->$1;
          if (_M0L8_2afieldS3936) {
            moonbit_decref(_M0L8_2afieldS3936);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS716);
        }
        _M0L5valueS2176 = _M0L8_2afieldS3676;
        _M0L6_2atmpS2175 = _M0L5valueS2176;
        return _M0L6_2atmpS2175;
      } else {
        moonbit_incref(_M0L8_2aentryS716);
      }
      _M0L3pslS2177 = _M0L8_2aentryS716->$2;
      moonbit_decref(_M0L8_2aentryS716);
      if (_M0L1iS711 > _M0L3pslS2177) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2178;
        moonbit_decref(_M0L4selfS714);
        _M0L6_2atmpS2178 = 0;
        return _M0L6_2atmpS2178;
      }
      _M0L6_2atmpS2179 = _M0L1iS711 + 1;
      _M0L6_2atmpS2181 = _M0L3idxS712 + 1;
      _M0L14capacity__maskS2182 = _M0L4selfS714->$3;
      _M0L6_2atmpS2180 = _M0L6_2atmpS2181 & _M0L14capacity__maskS2182;
      _M0L1iS711 = _M0L6_2atmpS2179;
      _M0L3idxS712 = _M0L6_2atmpS2180;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS723,
  moonbit_string_t _M0L3keyS719
) {
  int32_t _M0L4hashS718;
  int32_t _M0L14capacity__maskS2199;
  int32_t _M0L6_2atmpS2198;
  int32_t _M0L1iS720;
  int32_t _M0L3idxS721;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS719);
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS718 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS719);
  _M0L14capacity__maskS2199 = _M0L4selfS723->$3;
  _M0L6_2atmpS2198 = _M0L4hashS718 & _M0L14capacity__maskS2199;
  _M0L1iS720 = 0;
  _M0L3idxS721 = _M0L6_2atmpS2198;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2197 =
      _M0L4selfS723->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS722;
    if (
      _M0L3idxS721 < 0
      || _M0L3idxS721 >= Moonbit_array_length(_M0L7entriesS2197)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS722
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2197[
        _M0L3idxS721
      ];
    if (_M0L7_2abindS722 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2186;
      if (_M0L7_2abindS722) {
        moonbit_incref(_M0L7_2abindS722);
      }
      moonbit_decref(_M0L4selfS723);
      if (_M0L7_2abindS722) {
        moonbit_decref(_M0L7_2abindS722);
      }
      moonbit_decref(_M0L3keyS719);
      _M0L6_2atmpS2186 = 0;
      return _M0L6_2atmpS2186;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS724 =
        _M0L7_2abindS722;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS725 =
        _M0L7_2aSomeS724;
      int32_t _M0L4hashS2188 = _M0L8_2aentryS725->$3;
      int32_t _if__result_4155;
      int32_t _M0L3pslS2191;
      int32_t _M0L6_2atmpS2193;
      int32_t _M0L6_2atmpS2195;
      int32_t _M0L14capacity__maskS2196;
      int32_t _M0L6_2atmpS2194;
      if (_M0L4hashS2188 == _M0L4hashS718) {
        moonbit_string_t _M0L3keyS2187 = _M0L8_2aentryS725->$4;
        #line 220 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4155
        = moonbit_val_array_equal(_M0L3keyS2187, _M0L3keyS719);
      } else {
        _if__result_4155 = 0;
      }
      if (_if__result_4155) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3679;
        int32_t _M0L6_2acntS3938;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2190;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2189;
        moonbit_incref(_M0L8_2aentryS725);
        moonbit_decref(_M0L4selfS723);
        moonbit_decref(_M0L3keyS719);
        _M0L8_2afieldS3679 = _M0L8_2aentryS725->$5;
        _M0L6_2acntS3938 = Moonbit_object_header(_M0L8_2aentryS725)->rc;
        if (_M0L6_2acntS3938 > 1) {
          int32_t _M0L11_2anew__cntS3941 = _M0L6_2acntS3938 - 1;
          Moonbit_object_header(_M0L8_2aentryS725)->rc
          = _M0L11_2anew__cntS3941;
          moonbit_incref(_M0L8_2afieldS3679);
        } else if (_M0L6_2acntS3938 == 1) {
          moonbit_string_t _M0L8_2afieldS3940 = _M0L8_2aentryS725->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3939;
          moonbit_decref(_M0L8_2afieldS3940);
          _M0L8_2afieldS3939 = _M0L8_2aentryS725->$1;
          if (_M0L8_2afieldS3939) {
            moonbit_decref(_M0L8_2afieldS3939);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS725);
        }
        _M0L5valueS2190 = _M0L8_2afieldS3679;
        _M0L6_2atmpS2189 = _M0L5valueS2190;
        return _M0L6_2atmpS2189;
      } else {
        moonbit_incref(_M0L8_2aentryS725);
      }
      _M0L3pslS2191 = _M0L8_2aentryS725->$2;
      moonbit_decref(_M0L8_2aentryS725);
      if (_M0L1iS720 > _M0L3pslS2191) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2192;
        moonbit_decref(_M0L4selfS723);
        moonbit_decref(_M0L3keyS719);
        _M0L6_2atmpS2192 = 0;
        return _M0L6_2atmpS2192;
      }
      _M0L6_2atmpS2193 = _M0L1iS720 + 1;
      _M0L6_2atmpS2195 = _M0L3idxS721 + 1;
      _M0L14capacity__maskS2196 = _M0L4selfS723->$3;
      _M0L6_2atmpS2194 = _M0L6_2atmpS2195 & _M0L14capacity__maskS2196;
      _M0L1iS720 = _M0L6_2atmpS2193;
      _M0L3idxS721 = _M0L6_2atmpS2194;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS694
) {
  int32_t _M0L6lengthS693;
  int32_t _M0Lm8capacityS695;
  int32_t _M0L6_2atmpS2149;
  int32_t _M0L6_2atmpS2148;
  int32_t _M0L6_2atmpS2159;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS696;
  int32_t _M0L3endS2157;
  int32_t _M0L5startS2158;
  int32_t _M0L7_2abindS697;
  int32_t _M0L2__S698;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS694.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS693
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS694);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS695 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS693);
  _M0L6_2atmpS2149 = _M0Lm8capacityS695;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2148 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2149);
  if (_M0L6lengthS693 > _M0L6_2atmpS2148) {
    int32_t _M0L6_2atmpS2150 = _M0Lm8capacityS695;
    _M0Lm8capacityS695 = _M0L6_2atmpS2150 * 2;
  }
  _M0L6_2atmpS2159 = _M0Lm8capacityS695;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS696
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2159);
  _M0L3endS2157 = _M0L3arrS694.$2;
  _M0L5startS2158 = _M0L3arrS694.$1;
  _M0L7_2abindS697 = _M0L3endS2157 - _M0L5startS2158;
  _M0L2__S698 = 0;
  while (1) {
    if (_M0L2__S698 < _M0L7_2abindS697) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2154 =
        _M0L3arrS694.$0;
      int32_t _M0L5startS2156 = _M0L3arrS694.$1;
      int32_t _M0L6_2atmpS2155 = _M0L5startS2156 + _M0L2__S698;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS699 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2154[
          _M0L6_2atmpS2155
        ];
      moonbit_string_t _M0L6_2atmpS2151 = _M0L1eS699->$0;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2152 =
        _M0L1eS699->$1;
      int32_t _M0L6_2atmpS2153;
      moonbit_incref(_M0L6_2atmpS2152);
      moonbit_incref(_M0L6_2atmpS2151);
      moonbit_incref(_M0L1mS696);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS696, _M0L6_2atmpS2151, _M0L6_2atmpS2152);
      _M0L6_2atmpS2153 = _M0L2__S698 + 1;
      _M0L2__S698 = _M0L6_2atmpS2153;
      continue;
    } else {
      moonbit_decref(_M0L3arrS694.$0);
    }
    break;
  }
  return _M0L1mS696;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS702
) {
  int32_t _M0L6lengthS701;
  int32_t _M0Lm8capacityS703;
  int32_t _M0L6_2atmpS2161;
  int32_t _M0L6_2atmpS2160;
  int32_t _M0L6_2atmpS2171;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS704;
  int32_t _M0L3endS2169;
  int32_t _M0L5startS2170;
  int32_t _M0L7_2abindS705;
  int32_t _M0L2__S706;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS702.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS701
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS702);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS703 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS701);
  _M0L6_2atmpS2161 = _M0Lm8capacityS703;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2160 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2161);
  if (_M0L6lengthS701 > _M0L6_2atmpS2160) {
    int32_t _M0L6_2atmpS2162 = _M0Lm8capacityS703;
    _M0Lm8capacityS703 = _M0L6_2atmpS2162 * 2;
  }
  _M0L6_2atmpS2171 = _M0Lm8capacityS703;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS704
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2171);
  _M0L3endS2169 = _M0L3arrS702.$2;
  _M0L5startS2170 = _M0L3arrS702.$1;
  _M0L7_2abindS705 = _M0L3endS2169 - _M0L5startS2170;
  _M0L2__S706 = 0;
  while (1) {
    if (_M0L2__S706 < _M0L7_2abindS705) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2166 =
        _M0L3arrS702.$0;
      int32_t _M0L5startS2168 = _M0L3arrS702.$1;
      int32_t _M0L6_2atmpS2167 = _M0L5startS2168 + _M0L2__S706;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS707 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2166[
          _M0L6_2atmpS2167
        ];
      int32_t _M0L6_2atmpS2163 = _M0L1eS707->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2164 = _M0L1eS707->$1;
      int32_t _M0L6_2atmpS2165;
      moonbit_incref(_M0L6_2atmpS2164);
      moonbit_incref(_M0L1mS704);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS704, _M0L6_2atmpS2163, _M0L6_2atmpS2164);
      _M0L6_2atmpS2165 = _M0L2__S706 + 1;
      _M0L2__S706 = _M0L6_2atmpS2165;
      continue;
    } else {
      moonbit_decref(_M0L3arrS702.$0);
    }
    break;
  }
  return _M0L1mS704;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS687,
  moonbit_string_t _M0L3keyS688,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS689
) {
  int32_t _M0L6_2atmpS2146;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS688);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2146 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS688);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS687, _M0L3keyS688, _M0L5valueS689, _M0L6_2atmpS2146);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS690,
  int32_t _M0L3keyS691,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS692
) {
  int32_t _M0L6_2atmpS2147;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2147 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS691);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS690, _M0L3keyS691, _M0L5valueS692, _M0L6_2atmpS2147);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS666
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS665;
  int32_t _M0L8capacityS2138;
  int32_t _M0L13new__capacityS667;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2133;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2132;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3694;
  int32_t _M0L6_2atmpS2134;
  int32_t _M0L8capacityS2136;
  int32_t _M0L6_2atmpS2135;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2137;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3693;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1xS668;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS665 = _M0L4selfS666->$5;
  _M0L8capacityS2138 = _M0L4selfS666->$2;
  _M0L13new__capacityS667 = _M0L8capacityS2138 << 1;
  _M0L6_2atmpS2133 = 0;
  _M0L6_2atmpS2132
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS667, _M0L6_2atmpS2133);
  _M0L6_2aoldS3694 = _M0L4selfS666->$0;
  if (_M0L9old__headS665) {
    moonbit_incref(_M0L9old__headS665);
  }
  moonbit_decref(_M0L6_2aoldS3694);
  _M0L4selfS666->$0 = _M0L6_2atmpS2132;
  _M0L4selfS666->$2 = _M0L13new__capacityS667;
  _M0L6_2atmpS2134 = _M0L13new__capacityS667 - 1;
  _M0L4selfS666->$3 = _M0L6_2atmpS2134;
  _M0L8capacityS2136 = _M0L4selfS666->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2135 = _M0FPB21calc__grow__threshold(_M0L8capacityS2136);
  _M0L4selfS666->$4 = _M0L6_2atmpS2135;
  _M0L4selfS666->$1 = 0;
  _M0L6_2atmpS2137 = 0;
  _M0L6_2aoldS3693 = _M0L4selfS666->$5;
  if (_M0L6_2aoldS3693) {
    moonbit_decref(_M0L6_2aoldS3693);
  }
  _M0L4selfS666->$5 = _M0L6_2atmpS2137;
  _M0L4selfS666->$6 = -1;
  _M0L1xS668 = _M0L9old__headS665;
  while (1) {
    if (_M0L1xS668 == 0) {
      if (_M0L1xS668) {
        moonbit_decref(_M0L1xS668);
      }
      moonbit_decref(_M0L4selfS666);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS669 =
        _M0L1xS668;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS670 =
        _M0L7_2aSomeS669;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS671 =
        _M0L4_2axS670->$1;
      moonbit_string_t _M0L6_2akeyS672 = _M0L4_2axS670->$4;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS673 =
        _M0L4_2axS670->$5;
      int32_t _M0L7_2ahashS674 = _M0L4_2axS670->$3;
      int32_t _M0L6_2acntS3942 = Moonbit_object_header(_M0L4_2axS670)->rc;
      if (_M0L6_2acntS3942 > 1) {
        int32_t _M0L11_2anew__cntS3943 = _M0L6_2acntS3942 - 1;
        Moonbit_object_header(_M0L4_2axS670)->rc = _M0L11_2anew__cntS3943;
        moonbit_incref(_M0L8_2avalueS673);
        moonbit_incref(_M0L6_2akeyS672);
        if (_M0L7_2anextS671) {
          moonbit_incref(_M0L7_2anextS671);
        }
      } else if (_M0L6_2acntS3942 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS670);
      }
      moonbit_incref(_M0L4selfS666);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS666, _M0L6_2akeyS672, _M0L8_2avalueS673, _M0L7_2ahashS674);
      _M0L1xS668 = _M0L7_2anextS671;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS677
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS676;
  int32_t _M0L8capacityS2145;
  int32_t _M0L13new__capacityS678;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2140;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2139;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3699;
  int32_t _M0L6_2atmpS2141;
  int32_t _M0L8capacityS2143;
  int32_t _M0L6_2atmpS2142;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2144;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3698;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L1xS679;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS676 = _M0L4selfS677->$5;
  _M0L8capacityS2145 = _M0L4selfS677->$2;
  _M0L13new__capacityS678 = _M0L8capacityS2145 << 1;
  _M0L6_2atmpS2140 = 0;
  _M0L6_2atmpS2139
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS678, _M0L6_2atmpS2140);
  _M0L6_2aoldS3699 = _M0L4selfS677->$0;
  if (_M0L9old__headS676) {
    moonbit_incref(_M0L9old__headS676);
  }
  moonbit_decref(_M0L6_2aoldS3699);
  _M0L4selfS677->$0 = _M0L6_2atmpS2139;
  _M0L4selfS677->$2 = _M0L13new__capacityS678;
  _M0L6_2atmpS2141 = _M0L13new__capacityS678 - 1;
  _M0L4selfS677->$3 = _M0L6_2atmpS2141;
  _M0L8capacityS2143 = _M0L4selfS677->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2142 = _M0FPB21calc__grow__threshold(_M0L8capacityS2143);
  _M0L4selfS677->$4 = _M0L6_2atmpS2142;
  _M0L4selfS677->$1 = 0;
  _M0L6_2atmpS2144 = 0;
  _M0L6_2aoldS3698 = _M0L4selfS677->$5;
  if (_M0L6_2aoldS3698) {
    moonbit_decref(_M0L6_2aoldS3698);
  }
  _M0L4selfS677->$5 = _M0L6_2atmpS2144;
  _M0L4selfS677->$6 = -1;
  _M0L1xS679 = _M0L9old__headS676;
  while (1) {
    if (_M0L1xS679 == 0) {
      if (_M0L1xS679) {
        moonbit_decref(_M0L1xS679);
      }
      moonbit_decref(_M0L4selfS677);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS680 =
        _M0L1xS679;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS681 =
        _M0L7_2aSomeS680;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS682 =
        _M0L4_2axS681->$1;
      int32_t _M0L6_2akeyS683 = _M0L4_2axS681->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS684 =
        _M0L4_2axS681->$5;
      int32_t _M0L7_2ahashS685 = _M0L4_2axS681->$3;
      int32_t _M0L6_2acntS3944 = Moonbit_object_header(_M0L4_2axS681)->rc;
      if (_M0L6_2acntS3944 > 1) {
        int32_t _M0L11_2anew__cntS3945 = _M0L6_2acntS3944 - 1;
        Moonbit_object_header(_M0L4_2axS681)->rc = _M0L11_2anew__cntS3945;
        moonbit_incref(_M0L8_2avalueS684);
        if (_M0L7_2anextS682) {
          moonbit_incref(_M0L7_2anextS682);
        }
      } else if (_M0L6_2acntS3944 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS681);
      }
      moonbit_incref(_M0L4selfS677);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS677, _M0L6_2akeyS683, _M0L8_2avalueS684, _M0L7_2ahashS685);
      _M0L1xS679 = _M0L7_2anextS682;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS636,
  moonbit_string_t _M0L3keyS642,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS643,
  int32_t _M0L4hashS638
) {
  int32_t _M0L14capacity__maskS2113;
  int32_t _M0L6_2atmpS2112;
  int32_t _M0L3pslS633;
  int32_t _M0L3idxS634;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2113 = _M0L4selfS636->$3;
  _M0L6_2atmpS2112 = _M0L4hashS638 & _M0L14capacity__maskS2113;
  _M0L3pslS633 = 0;
  _M0L3idxS634 = _M0L6_2atmpS2112;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2111 =
      _M0L4selfS636->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS635;
    if (
      _M0L3idxS634 < 0
      || _M0L3idxS634 >= Moonbit_array_length(_M0L7entriesS2111)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS635
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2111[
        _M0L3idxS634
      ];
    if (_M0L7_2abindS635 == 0) {
      int32_t _M0L4sizeS2096 = _M0L4selfS636->$1;
      int32_t _M0L8grow__atS2097 = _M0L4selfS636->$4;
      int32_t _M0L7_2abindS639;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS640;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS641;
      if (_M0L4sizeS2096 >= _M0L8grow__atS2097) {
        int32_t _M0L14capacity__maskS2099;
        int32_t _M0L6_2atmpS2098;
        moonbit_incref(_M0L4selfS636);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS636);
        _M0L14capacity__maskS2099 = _M0L4selfS636->$3;
        _M0L6_2atmpS2098 = _M0L4hashS638 & _M0L14capacity__maskS2099;
        _M0L3pslS633 = 0;
        _M0L3idxS634 = _M0L6_2atmpS2098;
        continue;
      }
      _M0L7_2abindS639 = _M0L4selfS636->$6;
      _M0L7_2abindS640 = 0;
      _M0L5entryS641
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS641)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS641->$0 = _M0L7_2abindS639;
      _M0L5entryS641->$1 = _M0L7_2abindS640;
      _M0L5entryS641->$2 = _M0L3pslS633;
      _M0L5entryS641->$3 = _M0L4hashS638;
      _M0L5entryS641->$4 = _M0L3keyS642;
      _M0L5entryS641->$5 = _M0L5valueS643;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS636, _M0L3idxS634, _M0L5entryS641);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS644 =
        _M0L7_2abindS635;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS645 =
        _M0L7_2aSomeS644;
      int32_t _M0L4hashS2101 = _M0L14_2acurr__entryS645->$3;
      int32_t _if__result_4161;
      int32_t _M0L3pslS2102;
      int32_t _M0L6_2atmpS2107;
      int32_t _M0L6_2atmpS2109;
      int32_t _M0L14capacity__maskS2110;
      int32_t _M0L6_2atmpS2108;
      if (_M0L4hashS2101 == _M0L4hashS638) {
        moonbit_string_t _M0L3keyS2100 = _M0L14_2acurr__entryS645->$4;
        #line 134 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4161
        = moonbit_val_array_equal(_M0L3keyS2100, _M0L3keyS642);
      } else {
        _if__result_4161 = 0;
      }
      if (_if__result_4161) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3701;
        moonbit_incref(_M0L14_2acurr__entryS645);
        moonbit_decref(_M0L3keyS642);
        moonbit_decref(_M0L4selfS636);
        _M0L6_2aoldS3701 = _M0L14_2acurr__entryS645->$5;
        moonbit_decref(_M0L6_2aoldS3701);
        _M0L14_2acurr__entryS645->$5 = _M0L5valueS643;
        moonbit_decref(_M0L14_2acurr__entryS645);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS645);
      }
      _M0L3pslS2102 = _M0L14_2acurr__entryS645->$2;
      if (_M0L3pslS633 > _M0L3pslS2102) {
        int32_t _M0L4sizeS2103 = _M0L4selfS636->$1;
        int32_t _M0L8grow__atS2104 = _M0L4selfS636->$4;
        int32_t _M0L7_2abindS646;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS647;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS648;
        if (_M0L4sizeS2103 >= _M0L8grow__atS2104) {
          int32_t _M0L14capacity__maskS2106;
          int32_t _M0L6_2atmpS2105;
          moonbit_decref(_M0L14_2acurr__entryS645);
          moonbit_incref(_M0L4selfS636);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS636);
          _M0L14capacity__maskS2106 = _M0L4selfS636->$3;
          _M0L6_2atmpS2105 = _M0L4hashS638 & _M0L14capacity__maskS2106;
          _M0L3pslS633 = 0;
          _M0L3idxS634 = _M0L6_2atmpS2105;
          continue;
        }
        moonbit_incref(_M0L4selfS636);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS636, _M0L3idxS634, _M0L14_2acurr__entryS645);
        _M0L7_2abindS646 = _M0L4selfS636->$6;
        _M0L7_2abindS647 = 0;
        _M0L5entryS648
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS648)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS648->$0 = _M0L7_2abindS646;
        _M0L5entryS648->$1 = _M0L7_2abindS647;
        _M0L5entryS648->$2 = _M0L3pslS633;
        _M0L5entryS648->$3 = _M0L4hashS638;
        _M0L5entryS648->$4 = _M0L3keyS642;
        _M0L5entryS648->$5 = _M0L5valueS643;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS636, _M0L3idxS634, _M0L5entryS648);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS645);
      }
      _M0L6_2atmpS2107 = _M0L3pslS633 + 1;
      _M0L6_2atmpS2109 = _M0L3idxS634 + 1;
      _M0L14capacity__maskS2110 = _M0L4selfS636->$3;
      _M0L6_2atmpS2108 = _M0L6_2atmpS2109 & _M0L14capacity__maskS2110;
      _M0L3pslS633 = _M0L6_2atmpS2107;
      _M0L3idxS634 = _M0L6_2atmpS2108;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS652,
  int32_t _M0L3keyS658,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS659,
  int32_t _M0L4hashS654
) {
  int32_t _M0L14capacity__maskS2131;
  int32_t _M0L6_2atmpS2130;
  int32_t _M0L3pslS649;
  int32_t _M0L3idxS650;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2131 = _M0L4selfS652->$3;
  _M0L6_2atmpS2130 = _M0L4hashS654 & _M0L14capacity__maskS2131;
  _M0L3pslS649 = 0;
  _M0L3idxS650 = _M0L6_2atmpS2130;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2129 =
      _M0L4selfS652->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS651;
    if (
      _M0L3idxS650 < 0
      || _M0L3idxS650 >= Moonbit_array_length(_M0L7entriesS2129)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS651
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2129[
        _M0L3idxS650
      ];
    if (_M0L7_2abindS651 == 0) {
      int32_t _M0L4sizeS2114 = _M0L4selfS652->$1;
      int32_t _M0L8grow__atS2115 = _M0L4selfS652->$4;
      int32_t _M0L7_2abindS655;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS656;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS657;
      if (_M0L4sizeS2114 >= _M0L8grow__atS2115) {
        int32_t _M0L14capacity__maskS2117;
        int32_t _M0L6_2atmpS2116;
        moonbit_incref(_M0L4selfS652);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS652);
        _M0L14capacity__maskS2117 = _M0L4selfS652->$3;
        _M0L6_2atmpS2116 = _M0L4hashS654 & _M0L14capacity__maskS2117;
        _M0L3pslS649 = 0;
        _M0L3idxS650 = _M0L6_2atmpS2116;
        continue;
      }
      _M0L7_2abindS655 = _M0L4selfS652->$6;
      _M0L7_2abindS656 = 0;
      _M0L5entryS657
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS657)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS657->$0 = _M0L7_2abindS655;
      _M0L5entryS657->$1 = _M0L7_2abindS656;
      _M0L5entryS657->$2 = _M0L3pslS649;
      _M0L5entryS657->$3 = _M0L4hashS654;
      _M0L5entryS657->$4 = _M0L3keyS658;
      _M0L5entryS657->$5 = _M0L5valueS659;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS652, _M0L3idxS650, _M0L5entryS657);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS660 =
        _M0L7_2abindS651;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS661 =
        _M0L7_2aSomeS660;
      int32_t _M0L4hashS2119 = _M0L14_2acurr__entryS661->$3;
      int32_t _if__result_4163;
      int32_t _M0L3pslS2120;
      int32_t _M0L6_2atmpS2125;
      int32_t _M0L6_2atmpS2127;
      int32_t _M0L14capacity__maskS2128;
      int32_t _M0L6_2atmpS2126;
      if (_M0L4hashS2119 == _M0L4hashS654) {
        int32_t _M0L3keyS2118 = _M0L14_2acurr__entryS661->$4;
        _if__result_4163 = _M0L3keyS2118 == _M0L3keyS658;
      } else {
        _if__result_4163 = 0;
      }
      if (_if__result_4163) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3705;
        moonbit_incref(_M0L14_2acurr__entryS661);
        moonbit_decref(_M0L4selfS652);
        _M0L6_2aoldS3705 = _M0L14_2acurr__entryS661->$5;
        moonbit_decref(_M0L6_2aoldS3705);
        _M0L14_2acurr__entryS661->$5 = _M0L5valueS659;
        moonbit_decref(_M0L14_2acurr__entryS661);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS661);
      }
      _M0L3pslS2120 = _M0L14_2acurr__entryS661->$2;
      if (_M0L3pslS649 > _M0L3pslS2120) {
        int32_t _M0L4sizeS2121 = _M0L4selfS652->$1;
        int32_t _M0L8grow__atS2122 = _M0L4selfS652->$4;
        int32_t _M0L7_2abindS662;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS663;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS664;
        if (_M0L4sizeS2121 >= _M0L8grow__atS2122) {
          int32_t _M0L14capacity__maskS2124;
          int32_t _M0L6_2atmpS2123;
          moonbit_decref(_M0L14_2acurr__entryS661);
          moonbit_incref(_M0L4selfS652);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS652);
          _M0L14capacity__maskS2124 = _M0L4selfS652->$3;
          _M0L6_2atmpS2123 = _M0L4hashS654 & _M0L14capacity__maskS2124;
          _M0L3pslS649 = 0;
          _M0L3idxS650 = _M0L6_2atmpS2123;
          continue;
        }
        moonbit_incref(_M0L4selfS652);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS652, _M0L3idxS650, _M0L14_2acurr__entryS661);
        _M0L7_2abindS662 = _M0L4selfS652->$6;
        _M0L7_2abindS663 = 0;
        _M0L5entryS664
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS664)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS664->$0 = _M0L7_2abindS662;
        _M0L5entryS664->$1 = _M0L7_2abindS663;
        _M0L5entryS664->$2 = _M0L3pslS649;
        _M0L5entryS664->$3 = _M0L4hashS654;
        _M0L5entryS664->$4 = _M0L3keyS658;
        _M0L5entryS664->$5 = _M0L5valueS659;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS652, _M0L3idxS650, _M0L5entryS664);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS661);
      }
      _M0L6_2atmpS2125 = _M0L3pslS649 + 1;
      _M0L6_2atmpS2127 = _M0L3idxS650 + 1;
      _M0L14capacity__maskS2128 = _M0L4selfS652->$3;
      _M0L6_2atmpS2126 = _M0L6_2atmpS2127 & _M0L14capacity__maskS2128;
      _M0L3pslS649 = _M0L6_2atmpS2125;
      _M0L3idxS650 = _M0L6_2atmpS2126;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS617,
  int32_t _M0L3idxS622,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS621
) {
  int32_t _M0L3pslS2079;
  int32_t _M0L6_2atmpS2075;
  int32_t _M0L6_2atmpS2077;
  int32_t _M0L14capacity__maskS2078;
  int32_t _M0L6_2atmpS2076;
  int32_t _M0L3pslS613;
  int32_t _M0L3idxS614;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS615;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2079 = _M0L5entryS621->$2;
  _M0L6_2atmpS2075 = _M0L3pslS2079 + 1;
  _M0L6_2atmpS2077 = _M0L3idxS622 + 1;
  _M0L14capacity__maskS2078 = _M0L4selfS617->$3;
  _M0L6_2atmpS2076 = _M0L6_2atmpS2077 & _M0L14capacity__maskS2078;
  _M0L3pslS613 = _M0L6_2atmpS2075;
  _M0L3idxS614 = _M0L6_2atmpS2076;
  _M0L5entryS615 = _M0L5entryS621;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2074 =
      _M0L4selfS617->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS616;
    if (
      _M0L3idxS614 < 0
      || _M0L3idxS614 >= Moonbit_array_length(_M0L7entriesS2074)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS616
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2074[
        _M0L3idxS614
      ];
    if (_M0L7_2abindS616 == 0) {
      _M0L5entryS615->$2 = _M0L3pslS613;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS617, _M0L5entryS615, _M0L3idxS614);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS619 =
        _M0L7_2abindS616;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS620 =
        _M0L7_2aSomeS619;
      int32_t _M0L3pslS2064 = _M0L14_2acurr__entryS620->$2;
      if (_M0L3pslS613 > _M0L3pslS2064) {
        int32_t _M0L3pslS2069;
        int32_t _M0L6_2atmpS2065;
        int32_t _M0L6_2atmpS2067;
        int32_t _M0L14capacity__maskS2068;
        int32_t _M0L6_2atmpS2066;
        _M0L5entryS615->$2 = _M0L3pslS613;
        moonbit_incref(_M0L14_2acurr__entryS620);
        moonbit_incref(_M0L4selfS617);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS617, _M0L5entryS615, _M0L3idxS614);
        _M0L3pslS2069 = _M0L14_2acurr__entryS620->$2;
        _M0L6_2atmpS2065 = _M0L3pslS2069 + 1;
        _M0L6_2atmpS2067 = _M0L3idxS614 + 1;
        _M0L14capacity__maskS2068 = _M0L4selfS617->$3;
        _M0L6_2atmpS2066 = _M0L6_2atmpS2067 & _M0L14capacity__maskS2068;
        _M0L3pslS613 = _M0L6_2atmpS2065;
        _M0L3idxS614 = _M0L6_2atmpS2066;
        _M0L5entryS615 = _M0L14_2acurr__entryS620;
        continue;
      } else {
        int32_t _M0L6_2atmpS2070 = _M0L3pslS613 + 1;
        int32_t _M0L6_2atmpS2072 = _M0L3idxS614 + 1;
        int32_t _M0L14capacity__maskS2073 = _M0L4selfS617->$3;
        int32_t _M0L6_2atmpS2071 =
          _M0L6_2atmpS2072 & _M0L14capacity__maskS2073;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4165 =
          _M0L5entryS615;
        _M0L3pslS613 = _M0L6_2atmpS2070;
        _M0L3idxS614 = _M0L6_2atmpS2071;
        _M0L5entryS615 = _tmp_4165;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS627,
  int32_t _M0L3idxS632,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS631
) {
  int32_t _M0L3pslS2095;
  int32_t _M0L6_2atmpS2091;
  int32_t _M0L6_2atmpS2093;
  int32_t _M0L14capacity__maskS2094;
  int32_t _M0L6_2atmpS2092;
  int32_t _M0L3pslS623;
  int32_t _M0L3idxS624;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS625;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2095 = _M0L5entryS631->$2;
  _M0L6_2atmpS2091 = _M0L3pslS2095 + 1;
  _M0L6_2atmpS2093 = _M0L3idxS632 + 1;
  _M0L14capacity__maskS2094 = _M0L4selfS627->$3;
  _M0L6_2atmpS2092 = _M0L6_2atmpS2093 & _M0L14capacity__maskS2094;
  _M0L3pslS623 = _M0L6_2atmpS2091;
  _M0L3idxS624 = _M0L6_2atmpS2092;
  _M0L5entryS625 = _M0L5entryS631;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2090 =
      _M0L4selfS627->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS626;
    if (
      _M0L3idxS624 < 0
      || _M0L3idxS624 >= Moonbit_array_length(_M0L7entriesS2090)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS626
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2090[
        _M0L3idxS624
      ];
    if (_M0L7_2abindS626 == 0) {
      _M0L5entryS625->$2 = _M0L3pslS623;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L5entryS625, _M0L3idxS624);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS629 =
        _M0L7_2abindS626;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS630 =
        _M0L7_2aSomeS629;
      int32_t _M0L3pslS2080 = _M0L14_2acurr__entryS630->$2;
      if (_M0L3pslS623 > _M0L3pslS2080) {
        int32_t _M0L3pslS2085;
        int32_t _M0L6_2atmpS2081;
        int32_t _M0L6_2atmpS2083;
        int32_t _M0L14capacity__maskS2084;
        int32_t _M0L6_2atmpS2082;
        _M0L5entryS625->$2 = _M0L3pslS623;
        moonbit_incref(_M0L14_2acurr__entryS630);
        moonbit_incref(_M0L4selfS627);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L5entryS625, _M0L3idxS624);
        _M0L3pslS2085 = _M0L14_2acurr__entryS630->$2;
        _M0L6_2atmpS2081 = _M0L3pslS2085 + 1;
        _M0L6_2atmpS2083 = _M0L3idxS624 + 1;
        _M0L14capacity__maskS2084 = _M0L4selfS627->$3;
        _M0L6_2atmpS2082 = _M0L6_2atmpS2083 & _M0L14capacity__maskS2084;
        _M0L3pslS623 = _M0L6_2atmpS2081;
        _M0L3idxS624 = _M0L6_2atmpS2082;
        _M0L5entryS625 = _M0L14_2acurr__entryS630;
        continue;
      } else {
        int32_t _M0L6_2atmpS2086 = _M0L3pslS623 + 1;
        int32_t _M0L6_2atmpS2088 = _M0L3idxS624 + 1;
        int32_t _M0L14capacity__maskS2089 = _M0L4selfS627->$3;
        int32_t _M0L6_2atmpS2087 =
          _M0L6_2atmpS2088 & _M0L14capacity__maskS2089;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4167 =
          _M0L5entryS625;
        _M0L3pslS623 = _M0L6_2atmpS2086;
        _M0L3idxS624 = _M0L6_2atmpS2087;
        _M0L5entryS625 = _tmp_4167;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS601,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS603,
  int32_t _M0L8new__idxS602
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2060;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2061;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3713;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3712;
  int32_t _M0L6_2acntS3946;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS604;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2060 = _M0L4selfS601->$0;
  moonbit_incref(_M0L5entryS603);
  _M0L6_2atmpS2061 = _M0L5entryS603;
  if (
    _M0L8new__idxS602 < 0
    || _M0L8new__idxS602 >= Moonbit_array_length(_M0L7entriesS2060)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3713
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2060[
      _M0L8new__idxS602
    ];
  if (_M0L6_2aoldS3713) {
    moonbit_decref(_M0L6_2aoldS3713);
  }
  _M0L7entriesS2060[_M0L8new__idxS602] = _M0L6_2atmpS2061;
  _M0L8_2afieldS3712 = _M0L5entryS603->$1;
  _M0L6_2acntS3946 = Moonbit_object_header(_M0L5entryS603)->rc;
  if (_M0L6_2acntS3946 > 1) {
    int32_t _M0L11_2anew__cntS3949 = _M0L6_2acntS3946 - 1;
    Moonbit_object_header(_M0L5entryS603)->rc = _M0L11_2anew__cntS3949;
    if (_M0L8_2afieldS3712) {
      moonbit_incref(_M0L8_2afieldS3712);
    }
  } else if (_M0L6_2acntS3946 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3948 =
      _M0L5entryS603->$5;
    moonbit_string_t _M0L8_2afieldS3947;
    moonbit_decref(_M0L8_2afieldS3948);
    _M0L8_2afieldS3947 = _M0L5entryS603->$4;
    moonbit_decref(_M0L8_2afieldS3947);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS603);
  }
  _M0L7_2abindS604 = _M0L8_2afieldS3712;
  if (_M0L7_2abindS604 == 0) {
    if (_M0L7_2abindS604) {
      moonbit_decref(_M0L7_2abindS604);
    }
    _M0L4selfS601->$6 = _M0L8new__idxS602;
    moonbit_decref(_M0L4selfS601);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS605;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS606;
    moonbit_decref(_M0L4selfS601);
    _M0L7_2aSomeS605 = _M0L7_2abindS604;
    _M0L7_2anextS606 = _M0L7_2aSomeS605;
    _M0L7_2anextS606->$0 = _M0L8new__idxS602;
    moonbit_decref(_M0L7_2anextS606);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS607,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS609,
  int32_t _M0L8new__idxS608
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2062;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2063;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3716;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3715;
  int32_t _M0L6_2acntS3950;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS610;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2062 = _M0L4selfS607->$0;
  moonbit_incref(_M0L5entryS609);
  _M0L6_2atmpS2063 = _M0L5entryS609;
  if (
    _M0L8new__idxS608 < 0
    || _M0L8new__idxS608 >= Moonbit_array_length(_M0L7entriesS2062)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3716
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2062[
      _M0L8new__idxS608
    ];
  if (_M0L6_2aoldS3716) {
    moonbit_decref(_M0L6_2aoldS3716);
  }
  _M0L7entriesS2062[_M0L8new__idxS608] = _M0L6_2atmpS2063;
  _M0L8_2afieldS3715 = _M0L5entryS609->$1;
  _M0L6_2acntS3950 = Moonbit_object_header(_M0L5entryS609)->rc;
  if (_M0L6_2acntS3950 > 1) {
    int32_t _M0L11_2anew__cntS3952 = _M0L6_2acntS3950 - 1;
    Moonbit_object_header(_M0L5entryS609)->rc = _M0L11_2anew__cntS3952;
    if (_M0L8_2afieldS3715) {
      moonbit_incref(_M0L8_2afieldS3715);
    }
  } else if (_M0L6_2acntS3950 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3951 =
      _M0L5entryS609->$5;
    moonbit_decref(_M0L8_2afieldS3951);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS609);
  }
  _M0L7_2abindS610 = _M0L8_2afieldS3715;
  if (_M0L7_2abindS610 == 0) {
    if (_M0L7_2abindS610) {
      moonbit_decref(_M0L7_2abindS610);
    }
    _M0L4selfS607->$6 = _M0L8new__idxS608;
    moonbit_decref(_M0L4selfS607);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS611;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS612;
    moonbit_decref(_M0L4selfS607);
    _M0L7_2aSomeS611 = _M0L7_2abindS610;
    _M0L7_2anextS612 = _M0L7_2aSomeS611;
    _M0L7_2anextS612->$0 = _M0L8new__idxS608;
    moonbit_decref(_M0L7_2anextS612);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS594,
  int32_t _M0L3idxS596,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS595
) {
  int32_t _M0L7_2abindS593;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2047;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2048;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3718;
  int32_t _M0L4sizeS2050;
  int32_t _M0L6_2atmpS2049;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS593 = _M0L4selfS594->$6;
  switch (_M0L7_2abindS593) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2042;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3720;
      moonbit_incref(_M0L5entryS595);
      _M0L6_2atmpS2042 = _M0L5entryS595;
      _M0L6_2aoldS3720 = _M0L4selfS594->$5;
      if (_M0L6_2aoldS3720) {
        moonbit_decref(_M0L6_2aoldS3720);
      }
      _M0L4selfS594->$5 = _M0L6_2atmpS2042;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2046 =
        _M0L4selfS594->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2045;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2043;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2044;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3721;
      if (
        _M0L7_2abindS593 < 0
        || _M0L7_2abindS593 >= Moonbit_array_length(_M0L7entriesS2046)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2045
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2046[
          _M0L7_2abindS593
        ];
      if (_M0L6_2atmpS2045) {
        moonbit_incref(_M0L6_2atmpS2045);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2043
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2045);
      moonbit_incref(_M0L5entryS595);
      _M0L6_2atmpS2044 = _M0L5entryS595;
      _M0L6_2aoldS3721 = _M0L6_2atmpS2043->$1;
      if (_M0L6_2aoldS3721) {
        moonbit_decref(_M0L6_2aoldS3721);
      }
      _M0L6_2atmpS2043->$1 = _M0L6_2atmpS2044;
      moonbit_decref(_M0L6_2atmpS2043);
      break;
    }
  }
  _M0L4selfS594->$6 = _M0L3idxS596;
  _M0L7entriesS2047 = _M0L4selfS594->$0;
  _M0L6_2atmpS2048 = _M0L5entryS595;
  if (
    _M0L3idxS596 < 0
    || _M0L3idxS596 >= Moonbit_array_length(_M0L7entriesS2047)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3718
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2047[
      _M0L3idxS596
    ];
  if (_M0L6_2aoldS3718) {
    moonbit_decref(_M0L6_2aoldS3718);
  }
  _M0L7entriesS2047[_M0L3idxS596] = _M0L6_2atmpS2048;
  _M0L4sizeS2050 = _M0L4selfS594->$1;
  _M0L6_2atmpS2049 = _M0L4sizeS2050 + 1;
  _M0L4selfS594->$1 = _M0L6_2atmpS2049;
  moonbit_decref(_M0L4selfS594);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS598,
  int32_t _M0L3idxS600,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS599
) {
  int32_t _M0L7_2abindS597;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2056;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2057;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3724;
  int32_t _M0L4sizeS2059;
  int32_t _M0L6_2atmpS2058;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS597 = _M0L4selfS598->$6;
  switch (_M0L7_2abindS597) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2051;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3726;
      moonbit_incref(_M0L5entryS599);
      _M0L6_2atmpS2051 = _M0L5entryS599;
      _M0L6_2aoldS3726 = _M0L4selfS598->$5;
      if (_M0L6_2aoldS3726) {
        moonbit_decref(_M0L6_2aoldS3726);
      }
      _M0L4selfS598->$5 = _M0L6_2atmpS2051;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2055 =
        _M0L4selfS598->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2054;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2052;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2053;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3727;
      if (
        _M0L7_2abindS597 < 0
        || _M0L7_2abindS597 >= Moonbit_array_length(_M0L7entriesS2055)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2054
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2055[
          _M0L7_2abindS597
        ];
      if (_M0L6_2atmpS2054) {
        moonbit_incref(_M0L6_2atmpS2054);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2052
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2054);
      moonbit_incref(_M0L5entryS599);
      _M0L6_2atmpS2053 = _M0L5entryS599;
      _M0L6_2aoldS3727 = _M0L6_2atmpS2052->$1;
      if (_M0L6_2aoldS3727) {
        moonbit_decref(_M0L6_2aoldS3727);
      }
      _M0L6_2atmpS2052->$1 = _M0L6_2atmpS2053;
      moonbit_decref(_M0L6_2atmpS2052);
      break;
    }
  }
  _M0L4selfS598->$6 = _M0L3idxS600;
  _M0L7entriesS2056 = _M0L4selfS598->$0;
  _M0L6_2atmpS2057 = _M0L5entryS599;
  if (
    _M0L3idxS600 < 0
    || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS2056)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3724
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2056[
      _M0L3idxS600
    ];
  if (_M0L6_2aoldS3724) {
    moonbit_decref(_M0L6_2aoldS3724);
  }
  _M0L7entriesS2056[_M0L3idxS600] = _M0L6_2atmpS2057;
  _M0L4sizeS2059 = _M0L4selfS598->$1;
  _M0L6_2atmpS2058 = _M0L4sizeS2059 + 1;
  _M0L4selfS598->$1 = _M0L6_2atmpS2058;
  moonbit_decref(_M0L4selfS598);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS582
) {
  int32_t _M0L8capacityS581;
  int32_t _M0L7_2abindS583;
  int32_t _M0L7_2abindS584;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2040;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS585;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS586;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4168;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS581
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS582);
  _M0L7_2abindS583 = _M0L8capacityS581 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS584 = _M0FPB21calc__grow__threshold(_M0L8capacityS581);
  _M0L6_2atmpS2040 = 0;
  _M0L7_2abindS585
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS581, _M0L6_2atmpS2040);
  _M0L7_2abindS586 = 0;
  _block_4168
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4168)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4168->$0 = _M0L7_2abindS585;
  _block_4168->$1 = 0;
  _block_4168->$2 = _M0L8capacityS581;
  _block_4168->$3 = _M0L7_2abindS583;
  _block_4168->$4 = _M0L7_2abindS584;
  _block_4168->$5 = _M0L7_2abindS586;
  _block_4168->$6 = -1;
  return _block_4168;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS588
) {
  int32_t _M0L8capacityS587;
  int32_t _M0L7_2abindS589;
  int32_t _M0L7_2abindS590;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2041;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS591;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS592;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4169;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS587
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS588);
  _M0L7_2abindS589 = _M0L8capacityS587 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS590 = _M0FPB21calc__grow__threshold(_M0L8capacityS587);
  _M0L6_2atmpS2041 = 0;
  _M0L7_2abindS591
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS587, _M0L6_2atmpS2041);
  _M0L7_2abindS592 = 0;
  _block_4169
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4169)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4169->$0 = _M0L7_2abindS591;
  _block_4169->$1 = 0;
  _block_4169->$2 = _M0L8capacityS587;
  _block_4169->$3 = _M0L7_2abindS589;
  _block_4169->$4 = _M0L7_2abindS590;
  _block_4169->$5 = _M0L7_2abindS592;
  _block_4169->$6 = -1;
  return _block_4169;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS580) {
  #line 33 "/Users/user/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS580 >= 0) {
    int32_t _M0L6_2atmpS2039;
    int32_t _M0L6_2atmpS2038;
    int32_t _M0L6_2atmpS2037;
    int32_t _M0L6_2atmpS2036;
    if (_M0L4selfS580 <= 1) {
      return 1;
    }
    if (_M0L4selfS580 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2039 = _M0L4selfS580 - 1;
    #line 44 "/Users/user/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2038 = moonbit_clz32(_M0L6_2atmpS2039);
    _M0L6_2atmpS2037 = _M0L6_2atmpS2038 - 1;
    _M0L6_2atmpS2036 = 2147483647 >> (_M0L6_2atmpS2037 & 31);
    return _M0L6_2atmpS2036 + 1;
  } else {
    #line 34 "/Users/user/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS579) {
  int32_t _M0L6_2atmpS2035;
  #line 510 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2035 = _M0L8capacityS579 * 13;
  return _M0L6_2atmpS2035 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS575
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS575 == 0) {
    if (_M0L4selfS575) {
      moonbit_decref(_M0L4selfS575);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS576 =
      _M0L4selfS575;
    return _M0L7_2aSomeS576;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS577
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS577 == 0) {
    if (_M0L4selfS577) {
      moonbit_decref(_M0L4selfS577);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS578 =
      _M0L4selfS577;
    return _M0L7_2aSomeS578;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS574
) {
  moonbit_string_t* _M0L6_2atmpS2034;
  #line 165 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS2034 = _M0L4selfS574;
  #line 167 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2034);
}

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0MPC16result6Result6unwrapGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE(
  void* _M0L4selfS570
) {
  #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
  switch (Moonbit_object_tag(_M0L4selfS570)) {
    case 1: {
      struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS571 =
        (struct _M0DTPC16result6ResultGRP36mulpjs4mulp6stream10ByteStreamRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L4selfS570;
      struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0L8_2afieldS3730 =
        _M0L5_2aOkS571->$0;
      int32_t _M0L6_2acntS3953 = Moonbit_object_header(_M0L5_2aOkS571)->rc;
      if (_M0L6_2acntS3953 > 1) {
        int32_t _M0L11_2anew__cntS3954 = _M0L6_2acntS3953 - 1;
        Moonbit_object_header(_M0L5_2aOkS571)->rc = _M0L11_2anew__cntS3954;
        moonbit_incref(_M0L8_2afieldS3730);
      } else if (_M0L6_2acntS3953 == 1) {
        #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
        moonbit_free(_M0L5_2aOkS571);
      }
      return _M0L8_2afieldS3730;
      break;
    }
    default: {
      moonbit_decref(_M0L4selfS570);
      #line 264 "/Users/user/.moon/lib/core/builtin/result.mbt"
      return _M0FPC15abort5abortGRP36mulpjs4mulp6stream10ByteStreamE((moonbit_string_t)moonbit_string_literal_46.data);
      break;
    }
  }
}

moonbit_string_t _M0MPC16result6Result6unwrapGsRP36mulpjs4mulp4core9MulpErrorE(
  void* _M0L4selfS572
) {
  #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
  switch (Moonbit_object_tag(_M0L4selfS572)) {
    case 1: {
      struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok* _M0L5_2aOkS573 =
        (struct _M0DTPC16result6ResultGsRP36mulpjs4mulp4core9MulpErrorE2Ok*)_M0L4selfS572;
      moonbit_string_t _M0L8_2afieldS3731 = _M0L5_2aOkS573->$0;
      int32_t _M0L6_2acntS3955 = Moonbit_object_header(_M0L5_2aOkS573)->rc;
      if (_M0L6_2acntS3955 > 1) {
        int32_t _M0L11_2anew__cntS3956 = _M0L6_2acntS3955 - 1;
        Moonbit_object_header(_M0L5_2aOkS573)->rc = _M0L11_2anew__cntS3956;
        moonbit_incref(_M0L8_2afieldS3731);
      } else if (_M0L6_2acntS3955 == 1) {
        #line 261 "/Users/user/.moon/lib/core/builtin/result.mbt"
        moonbit_free(_M0L5_2aOkS573);
      }
      return _M0L8_2afieldS3731;
      break;
    }
    default: {
      moonbit_decref(_M0L4selfS572);
      #line 264 "/Users/user/.moon/lib/core/builtin/result.mbt"
      return _M0FPC15abort5abortGsE((moonbit_string_t)moonbit_string_literal_46.data);
      break;
    }
  }
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS569
) {
  moonbit_string_t* _M0L6_2atmpS2032;
  int32_t _M0L6_2atmpS2033;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2031;
  #line 1509 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS569);
  _M0L6_2atmpS2032 = _M0L4selfS569;
  _M0L6_2atmpS2033 = Moonbit_array_length(_M0L4selfS569);
  moonbit_decref(_M0L4selfS569);
  _M0L6_2atmpS2031
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2033, _M0L6_2atmpS2032
  };
  #line 1511 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2031);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS567
) {
  struct _M0TPB8MutLocalGiE* _M0L1iS566;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__* _closure_4170;
  struct _M0TWEOs* _M0L6_2atmpS2019;
  #line 677 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS566
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS566)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS566->$0 = 0;
  _closure_4170
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__));
  Moonbit_object_header(_closure_4170)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__, $0_0) >> 2, 2, 0);
  _closure_4170->code = &_M0MPC15array9ArrayView4iterGsEC2020l680;
  _closure_4170->$0_0 = _M0L4selfS567.$0;
  _closure_4170->$0_1 = _M0L4selfS567.$1;
  _closure_4170->$0_2 = _M0L4selfS567.$2;
  _closure_4170->$1 = _M0L1iS566;
  _M0L6_2atmpS2019 = (struct _M0TWEOs*)_closure_4170;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2019);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2020l680(
  struct _M0TWEOs* _M0L6_2aenvS2021
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__* _M0L14_2acasted__envS2022;
  struct _M0TPB8MutLocalGiE* _M0L1iS566;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3734;
  int32_t _M0L6_2acntS3957;
  struct _M0TPB9ArrayViewGsE _M0L4selfS567;
  int32_t _M0L3valS2023;
  int32_t _M0L6_2atmpS2024;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS2022
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l680__*)_M0L6_2aenvS2021;
  _M0L1iS566 = _M0L14_2acasted__envS2022->$1;
  _M0L8_2afieldS3734
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2022->$0_1,
      _M0L14_2acasted__envS2022->$0_2,
      _M0L14_2acasted__envS2022->$0_0
  };
  _M0L6_2acntS3957 = Moonbit_object_header(_M0L14_2acasted__envS2022)->rc;
  if (_M0L6_2acntS3957 > 1) {
    int32_t _M0L11_2anew__cntS3958 = _M0L6_2acntS3957 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2022)->rc
    = _M0L11_2anew__cntS3958;
    moonbit_incref(_M0L1iS566);
    moonbit_incref(_M0L8_2afieldS3734.$0);
  } else if (_M0L6_2acntS3957 == 1) {
    #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2022);
  }
  _M0L4selfS567 = _M0L8_2afieldS3734;
  _M0L3valS2023 = _M0L1iS566->$0;
  moonbit_incref(_M0L4selfS567.$0);
  #line 681 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS2024 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS567);
  if (_M0L3valS2023 < _M0L6_2atmpS2024) {
    moonbit_string_t* _M0L3bufS2027 = _M0L4selfS567.$0;
    int32_t _M0L5startS2029 = _M0L4selfS567.$1;
    int32_t _M0L3valS2030 = _M0L1iS566->$0;
    int32_t _M0L6_2atmpS2028 = _M0L5startS2029 + _M0L3valS2030;
    moonbit_string_t _M0L6_2atmpS3732 =
      (moonbit_string_t)_M0L3bufS2027[_M0L6_2atmpS2028];
    moonbit_string_t _M0L4elemS568;
    int32_t _M0L3valS2026;
    int32_t _M0L6_2atmpS2025;
    moonbit_incref(_M0L6_2atmpS3732);
    moonbit_decref(_M0L3bufS2027);
    _M0L4elemS568 = _M0L6_2atmpS3732;
    _M0L3valS2026 = _M0L1iS566->$0;
    _M0L6_2atmpS2025 = _M0L3valS2026 + 1;
    _M0L1iS566->$0 = _M0L6_2atmpS2025;
    moonbit_decref(_M0L1iS566);
    return _M0L4elemS568;
  } else {
    moonbit_decref(_M0L4selfS567.$0);
    moonbit_decref(_M0L1iS566);
    return 0;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS564,
  struct _M0TPB6Logger _M0L6loggerS565
) {
  int32_t _M0L6_2atmpS2018;
  struct _M0TPC16string10StringView _M0L6_2atmpS2017;
  #line 244 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS2018 = Moonbit_array_length(_M0L4selfS564);
  _M0L6_2atmpS2017
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2018, _M0L4selfS564
  };
  #line 245 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS2017, _M0L6loggerS565, 1);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS563) {
  #line 45 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 46 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS563, 10);
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS562,
  struct _M0TPB6Logger _M0L6loggerS561
) {
  moonbit_string_t _M0L6_2atmpS2016;
  #line 26 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS2016 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS562);
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS561.$0->$method_0(_M0L6loggerS561.$1, _M0L6_2atmpS2016);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS560) {
  #line 31 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS560) {
    return (moonbit_string_t)moonbit_string_literal_20.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_31.data;
  }
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS559
) {
  int32_t _M0L6_2atmpS2015;
  #line 24 "/Users/user/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2015 = Moonbit_array_length(_M0L4selfS559);
  return (struct _M0TPC16string10StringView){0,
                                               _M0L6_2atmpS2015,
                                               _M0L4selfS559};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS550,
  moonbit_string_t _M0L5valueS552
) {
  int32_t _M0L3lenS2000;
  moonbit_string_t* _M0L6_2atmpS2002;
  int32_t _M0L6_2atmpS2001;
  int32_t _M0L6lengthS551;
  moonbit_string_t* _M0L3bufS2003;
  moonbit_string_t _M0L6_2aoldS3736;
  int32_t _M0L6_2atmpS2004;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2000 = _M0L4selfS550->$1;
  moonbit_incref(_M0L4selfS550);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2002 = _M0MPC15array5Array6bufferGsE(_M0L4selfS550);
  _M0L6_2atmpS2001 = Moonbit_array_length(_M0L6_2atmpS2002);
  moonbit_decref(_M0L6_2atmpS2002);
  if (_M0L3lenS2000 == _M0L6_2atmpS2001) {
    moonbit_incref(_M0L4selfS550);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS550);
  }
  _M0L6lengthS551 = _M0L4selfS550->$1;
  _M0L3bufS2003 = _M0L4selfS550->$0;
  _M0L6_2aoldS3736 = (moonbit_string_t)_M0L3bufS2003[_M0L6lengthS551];
  moonbit_decref(_M0L6_2aoldS3736);
  _M0L3bufS2003[_M0L6lengthS551] = _M0L5valueS552;
  _M0L6_2atmpS2004 = _M0L6lengthS551 + 1;
  _M0L4selfS550->$1 = _M0L6_2atmpS2004;
  moonbit_decref(_M0L4selfS550);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS553,
  struct _M0TUsiE* _M0L5valueS555
) {
  int32_t _M0L3lenS2005;
  struct _M0TUsiE** _M0L6_2atmpS2007;
  int32_t _M0L6_2atmpS2006;
  int32_t _M0L6lengthS554;
  struct _M0TUsiE** _M0L3bufS2008;
  struct _M0TUsiE* _M0L6_2aoldS3738;
  int32_t _M0L6_2atmpS2009;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2005 = _M0L4selfS553->$1;
  moonbit_incref(_M0L4selfS553);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2007 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS553);
  _M0L6_2atmpS2006 = Moonbit_array_length(_M0L6_2atmpS2007);
  moonbit_decref(_M0L6_2atmpS2007);
  if (_M0L3lenS2005 == _M0L6_2atmpS2006) {
    moonbit_incref(_M0L4selfS553);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS553);
  }
  _M0L6lengthS554 = _M0L4selfS553->$1;
  _M0L3bufS2008 = _M0L4selfS553->$0;
  _M0L6_2aoldS3738 = (struct _M0TUsiE*)_M0L3bufS2008[_M0L6lengthS554];
  if (_M0L6_2aoldS3738) {
    moonbit_decref(_M0L6_2aoldS3738);
  }
  _M0L3bufS2008[_M0L6lengthS554] = _M0L5valueS555;
  _M0L6_2atmpS2009 = _M0L6lengthS554 + 1;
  _M0L4selfS553->$1 = _M0L6_2atmpS2009;
  moonbit_decref(_M0L4selfS553);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS556,
  struct _M0TP36mulpjs4mulp6stream4File* _M0L5valueS558
) {
  int32_t _M0L3lenS2010;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2atmpS2012;
  int32_t _M0L6_2atmpS2011;
  int32_t _M0L6lengthS557;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L3bufS2013;
  struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2aoldS3740;
  int32_t _M0L6_2atmpS2014;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2010 = _M0L4selfS556->$1;
  moonbit_incref(_M0L4selfS556);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2012
  = _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(_M0L4selfS556);
  _M0L6_2atmpS2011 = Moonbit_array_length(_M0L6_2atmpS2012);
  moonbit_decref(_M0L6_2atmpS2012);
  if (_M0L3lenS2010 == _M0L6_2atmpS2011) {
    moonbit_incref(_M0L4selfS556);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP36mulpjs4mulp6stream4FileE(_M0L4selfS556);
  }
  _M0L6lengthS557 = _M0L4selfS556->$1;
  _M0L3bufS2013 = _M0L4selfS556->$0;
  _M0L6_2aoldS3740
  = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3bufS2013[_M0L6lengthS557];
  if (_M0L6_2aoldS3740) {
    moonbit_decref(_M0L6_2aoldS3740);
  }
  _M0L3bufS2013[_M0L6lengthS557] = _M0L5valueS558;
  _M0L6_2atmpS2014 = _M0L6lengthS557 + 1;
  _M0L4selfS556->$1 = _M0L6_2atmpS2014;
  moonbit_decref(_M0L4selfS556);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS542) {
  int32_t _M0L8old__capS541;
  int32_t _M0L8new__capS543;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS541 = _M0L4selfS542->$1;
  if (_M0L8old__capS541 == 0) {
    _M0L8new__capS543 = 8;
  } else {
    _M0L8new__capS543 = _M0L8old__capS541 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS542, _M0L8new__capS543);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS545
) {
  int32_t _M0L8old__capS544;
  int32_t _M0L8new__capS546;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS544 = _M0L4selfS545->$1;
  if (_M0L8old__capS544 == 0) {
    _M0L8new__capS546 = 8;
  } else {
    _M0L8new__capS546 = _M0L8old__capS544 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS545, _M0L8new__capS546);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS548
) {
  int32_t _M0L8old__capS547;
  int32_t _M0L8new__capS549;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS547 = _M0L4selfS548->$1;
  if (_M0L8old__capS547 == 0) {
    _M0L8new__capS549 = 8;
  } else {
    _M0L8new__capS549 = _M0L8old__capS547 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp6stream4FileE(_M0L4selfS548, _M0L8new__capS549);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS526,
  int32_t _M0L13new__capacityS524
) {
  moonbit_string_t* _M0L8new__bufS523;
  moonbit_string_t* _M0L8old__bufS525;
  int32_t _M0L8old__capS527;
  int32_t _M0L9copy__lenS528;
  moonbit_string_t* _M0L6_2aoldS3742;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS523
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS524, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8old__bufS525 = _M0L4selfS526->$0;
  _M0L8old__capS527 = Moonbit_array_length(_M0L8old__bufS525);
  if (_M0L8old__capS527 < _M0L13new__capacityS524) {
    _M0L9copy__lenS528 = _M0L8old__capS527;
  } else {
    _M0L9copy__lenS528 = _M0L13new__capacityS524;
  }
  moonbit_incref(_M0L8old__bufS525);
  moonbit_incref(_M0L8new__bufS523);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS523, 0, _M0L8old__bufS525, 0, _M0L9copy__lenS528);
  _M0L6_2aoldS3742 = _M0L4selfS526->$0;
  moonbit_decref(_M0L6_2aoldS3742);
  _M0L4selfS526->$0 = _M0L8new__bufS523;
  moonbit_decref(_M0L4selfS526);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS532,
  int32_t _M0L13new__capacityS530
) {
  struct _M0TUsiE** _M0L8new__bufS529;
  struct _M0TUsiE** _M0L8old__bufS531;
  int32_t _M0L8old__capS533;
  int32_t _M0L9copy__lenS534;
  struct _M0TUsiE** _M0L6_2aoldS3744;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS529
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS530, 0);
  _M0L8old__bufS531 = _M0L4selfS532->$0;
  _M0L8old__capS533 = Moonbit_array_length(_M0L8old__bufS531);
  if (_M0L8old__capS533 < _M0L13new__capacityS530) {
    _M0L9copy__lenS534 = _M0L8old__capS533;
  } else {
    _M0L9copy__lenS534 = _M0L13new__capacityS530;
  }
  moonbit_incref(_M0L8old__bufS531);
  moonbit_incref(_M0L8new__bufS529);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS529, 0, _M0L8old__bufS531, 0, _M0L9copy__lenS534);
  _M0L6_2aoldS3744 = _M0L4selfS532->$0;
  moonbit_decref(_M0L6_2aoldS3744);
  _M0L4selfS532->$0 = _M0L8new__bufS529;
  moonbit_decref(_M0L4selfS532);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS538,
  int32_t _M0L13new__capacityS536
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L8new__bufS535;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L8old__bufS537;
  int32_t _M0L8old__capS539;
  int32_t _M0L9copy__lenS540;
  struct _M0TP36mulpjs4mulp6stream4File** _M0L6_2aoldS3746;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS535
  = (struct _M0TP36mulpjs4mulp6stream4File**)moonbit_make_ref_array(_M0L13new__capacityS536, 0);
  _M0L8old__bufS537 = _M0L4selfS538->$0;
  _M0L8old__capS539 = Moonbit_array_length(_M0L8old__bufS537);
  if (_M0L8old__capS539 < _M0L13new__capacityS536) {
    _M0L9copy__lenS540 = _M0L8old__capS539;
  } else {
    _M0L9copy__lenS540 = _M0L13new__capacityS536;
  }
  moonbit_incref(_M0L8old__bufS537);
  moonbit_incref(_M0L8new__bufS535);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp6stream4FileE(_M0L8new__bufS535, 0, _M0L8old__bufS537, 0, _M0L9copy__lenS540);
  _M0L6_2aoldS3746 = _M0L4selfS538->$0;
  moonbit_decref(_M0L6_2aoldS3746);
  _M0L4selfS538->$0 = _M0L8new__bufS535;
  moonbit_decref(_M0L4selfS538);
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS521) {
  int32_t _result_4171;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_4171 = _M0L4selfS521->$1;
  moonbit_decref(_M0L4selfS521);
  return _result_4171;
}

int32_t _M0MPC15array5Array6lengthGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS522
) {
  int32_t _result_4172;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_4172 = _M0L4selfS522->$1;
  moonbit_decref(_M0L4selfS522);
  return _result_4172;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS518
) {
  moonbit_string_t* _M0L8_2afieldS3748;
  int32_t _M0L6_2acntS3959;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3748 = _M0L4selfS518->$0;
  _M0L6_2acntS3959 = Moonbit_object_header(_M0L4selfS518)->rc;
  if (_M0L6_2acntS3959 > 1) {
    int32_t _M0L11_2anew__cntS3960 = _M0L6_2acntS3959 - 1;
    Moonbit_object_header(_M0L4selfS518)->rc = _M0L11_2anew__cntS3960;
    moonbit_incref(_M0L8_2afieldS3748);
  } else if (_M0L6_2acntS3959 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS518);
  }
  return _M0L8_2afieldS3748;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS519
) {
  struct _M0TUsiE** _M0L8_2afieldS3749;
  int32_t _M0L6_2acntS3961;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3749 = _M0L4selfS519->$0;
  _M0L6_2acntS3961 = Moonbit_object_header(_M0L4selfS519)->rc;
  if (_M0L6_2acntS3961 > 1) {
    int32_t _M0L11_2anew__cntS3962 = _M0L6_2acntS3961 - 1;
    Moonbit_object_header(_M0L4selfS519)->rc = _M0L11_2anew__cntS3962;
    moonbit_incref(_M0L8_2afieldS3749);
  } else if (_M0L6_2acntS3961 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS519);
  }
  return _M0L8_2afieldS3749;
}

struct _M0TP36mulpjs4mulp6stream4File** _M0MPC15array5Array6bufferGRP36mulpjs4mulp6stream4FileE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp6stream4FileE* _M0L4selfS520
) {
  struct _M0TP36mulpjs4mulp6stream4File** _M0L8_2afieldS3750;
  int32_t _M0L6_2acntS3963;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3750 = _M0L4selfS520->$0;
  _M0L6_2acntS3963 = Moonbit_object_header(_M0L4selfS520)->rc;
  if (_M0L6_2acntS3963 > 1) {
    int32_t _M0L11_2anew__cntS3964 = _M0L6_2acntS3963 - 1;
    Moonbit_object_header(_M0L4selfS520)->rc = _M0L11_2anew__cntS3964;
    moonbit_incref(_M0L8_2afieldS3750);
  } else if (_M0L6_2acntS3963 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS520);
  }
  return _M0L8_2afieldS3750;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS517
) {
  #line 53 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS517 == 0) {
    moonbit_string_t* _M0L6_2atmpS1998 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4173 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4173)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4173->$0 = _M0L6_2atmpS1998;
    _block_4173->$1 = 0;
    return _block_4173;
  } else {
    moonbit_string_t* _M0L6_2atmpS1999 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS517, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4174 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4174)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4174->$0 = _M0L6_2atmpS1999;
    _block_4174->$1 = 0;
    return _block_4174;
  }
}

int32_t _M0MPC16string6String11has__prefix(
  moonbit_string_t _M0L4selfS515,
  struct _M0TPC16string10StringView _M0L3strS516
) {
  int32_t _M0L6_2atmpS1997;
  struct _M0TPC16string10StringView _M0L6_2atmpS1996;
  #line 298 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1997 = Moonbit_array_length(_M0L4selfS515);
  _M0L6_2atmpS1996
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1997, _M0L4selfS515
  };
  #line 300 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC16string10StringView11has__prefix(_M0L6_2atmpS1996, _M0L3strS516);
}

int32_t _M0MPC16string10StringView11has__prefix(
  struct _M0TPC16string10StringView _M0L4selfS511,
  struct _M0TPC16string10StringView _M0L3strS512
) {
  int64_t _M0L7_2abindS510;
  #line 291 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  #line 293 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS510
  = _M0MPC16string10StringView4find(_M0L4selfS511, _M0L3strS512);
  if (_M0L7_2abindS510 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS513 = _M0L7_2abindS510;
    int32_t _M0L4_2aiS514 = (int32_t)_M0L7_2aSomeS513;
    return _M0L4_2aiS514 == 0;
  }
}

int32_t _M0MPC16string6String11has__suffix(
  moonbit_string_t _M0L4selfS508,
  struct _M0TPC16string10StringView _M0L3strS509
) {
  int32_t _M0L6_2atmpS1995;
  struct _M0TPC16string10StringView _M0L6_2atmpS1994;
  #line 270 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1995 = Moonbit_array_length(_M0L4selfS508);
  _M0L6_2atmpS1994
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1995, _M0L4selfS508
  };
  #line 272 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC16string10StringView11has__suffix(_M0L6_2atmpS1994, _M0L3strS509);
}

int32_t _M0MPC16string10StringView11has__suffix(
  struct _M0TPC16string10StringView _M0L4selfS504,
  struct _M0TPC16string10StringView _M0L3strS505
) {
  int64_t _M0L7_2abindS503;
  #line 263 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L3strS505.$0);
  moonbit_incref(_M0L4selfS504.$0);
  #line 265 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS503
  = _M0MPC16string10StringView9rev__find(_M0L4selfS504, _M0L3strS505);
  if (_M0L7_2abindS503 == 4294967296ll) {
    moonbit_decref(_M0L3strS505.$0);
    moonbit_decref(_M0L4selfS504.$0);
    return 0;
  } else {
    int64_t _M0L7_2aSomeS506 = _M0L7_2abindS503;
    int32_t _M0L4_2aiS507 = (int32_t)_M0L7_2aSomeS506;
    int32_t _M0L6_2atmpS1992;
    int32_t _M0L6_2atmpS1993;
    int32_t _M0L6_2atmpS1991;
    #line 265 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS1992 = _M0MPC16string10StringView6length(_M0L4selfS504);
    #line 265 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS1993 = _M0MPC16string10StringView6length(_M0L3strS505);
    _M0L6_2atmpS1991 = _M0L6_2atmpS1992 - _M0L6_2atmpS1993;
    return _M0L4_2aiS507 == _M0L6_2atmpS1991;
  }
}

int64_t _M0MPC16string10StringView9rev__find(
  struct _M0TPC16string10StringView _M0L4selfS502,
  struct _M0TPC16string10StringView _M0L3strS501
) {
  int32_t _M0L6_2atmpS1990;
  #line 165 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L3strS501.$0);
  #line 166 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1990 = _M0MPC16string10StringView6length(_M0L3strS501);
  if (_M0L6_2atmpS1990 <= 4) {
    #line 167 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB23brute__force__rev__find(_M0L4selfS502, _M0L3strS501);
  } else {
    #line 169 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB33boyer__moore__horspool__rev__find(_M0L4selfS502, _M0L3strS501);
  }
}

int64_t _M0FPB23brute__force__rev__find(
  struct _M0TPC16string10StringView _M0L8haystackS493,
  struct _M0TPC16string10StringView _M0L6needleS495
) {
  int32_t _M0L13haystack__lenS492;
  int32_t _M0L11needle__lenS494;
  #line 178 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L8haystackS493.$0);
  #line 179 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L13haystack__lenS492
  = _M0MPC16string10StringView6length(_M0L8haystackS493);
  moonbit_incref(_M0L6needleS495.$0);
  #line 180 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L11needle__lenS494 = _M0MPC16string10StringView6length(_M0L6needleS495);
  if (_M0L11needle__lenS494 > 0) {
    if (_M0L13haystack__lenS492 >= _M0L11needle__lenS494) {
      int32_t _M0L13needle__firstS496;
      int32_t _M0L6_2atmpS1989;
      int32_t _M0L1iS497;
      moonbit_incref(_M0L6needleS495.$0);
      #line 183 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
      _M0L13needle__firstS496
      = _M0MPC16string10StringView11unsafe__get(_M0L6needleS495, 0);
      _M0L6_2atmpS1989 = _M0L13haystack__lenS492 - _M0L11needle__lenS494;
      _M0L1iS497 = _M0L6_2atmpS1989;
      while (1) {
        if (_M0L1iS497 >= 0) {
          int32_t _M0L6_2atmpS1982;
          int32_t _M0L1jS499;
          int32_t _M0L6_2atmpS1988;
          moonbit_incref(_M0L8haystackS493.$0);
          #line 185 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1982
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS493, _M0L1iS497);
          if (_M0L6_2atmpS1982 != _M0L13needle__firstS496) {
            int32_t _M0L6_2atmpS1983 = _M0L1iS497 - 1;
            _M0L1iS497 = _M0L6_2atmpS1983;
            continue;
          }
          _M0L1jS499 = 1;
          while (1) {
            if (_M0L1jS499 < _M0L11needle__lenS494) {
              int32_t _M0L6_2atmpS1986 = _M0L1iS497 + _M0L1jS499;
              int32_t _M0L6_2atmpS1984;
              int32_t _M0L6_2atmpS1985;
              int32_t _M0L6_2atmpS1987;
              moonbit_incref(_M0L8haystackS493.$0);
              #line 190 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1984
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS493, _M0L6_2atmpS1986);
              moonbit_incref(_M0L6needleS495.$0);
              #line 190 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1985
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS495, _M0L1jS499);
              if (_M0L6_2atmpS1984 != _M0L6_2atmpS1985) {
                break;
              }
              _M0L6_2atmpS1987 = _M0L1jS499 + 1;
              _M0L1jS499 = _M0L6_2atmpS1987;
              continue;
            } else {
              moonbit_decref(_M0L6needleS495.$0);
              moonbit_decref(_M0L8haystackS493.$0);
              return (int64_t)_M0L1iS497;
            }
            break;
          }
          _M0L6_2atmpS1988 = _M0L1iS497 - 1;
          _M0L1iS497 = _M0L6_2atmpS1988;
          continue;
        } else {
          moonbit_decref(_M0L6needleS495.$0);
          moonbit_decref(_M0L8haystackS493.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS495.$0);
      moonbit_decref(_M0L8haystackS493.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS495.$0);
    moonbit_decref(_M0L8haystackS493.$0);
    return (int64_t)_M0L13haystack__lenS492;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS491
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS491;
}

int64_t _M0FPB33boyer__moore__horspool__rev__find(
  struct _M0TPC16string10StringView _M0L8haystackS481,
  struct _M0TPC16string10StringView _M0L6needleS483
) {
  int32_t _M0L13haystack__lenS480;
  int32_t _M0L11needle__lenS482;
  #line 204 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L8haystackS481.$0);
  #line 208 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L13haystack__lenS480
  = _M0MPC16string10StringView6length(_M0L8haystackS481);
  moonbit_incref(_M0L6needleS483.$0);
  #line 209 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L11needle__lenS482 = _M0MPC16string10StringView6length(_M0L6needleS483);
  if (_M0L11needle__lenS482 > 0) {
    if (_M0L13haystack__lenS480 >= _M0L11needle__lenS482) {
      int32_t* _M0L11skip__tableS484 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS482);
      int32_t _M0L6_2atmpS1971 = _M0L11needle__lenS482 - 1;
      int32_t _M0L1iS485 = _M0L6_2atmpS1971;
      int32_t _M0L6_2atmpS1981;
      int32_t _M0L1iS487;
      while (1) {
        if (_M0L1iS485 >= 1) {
          int32_t _M0L6_2atmpS1969;
          int32_t _M0L6_2atmpS1968;
          int32_t _M0L6_2atmpS1967;
          int32_t _M0L6_2atmpS1970;
          moonbit_incref(_M0L6needleS483.$0);
          #line 214 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1969
          = _M0MPC16string10StringView11unsafe__get(_M0L6needleS483, _M0L1iS485);
          _M0L6_2atmpS1968 = (int32_t)_M0L6_2atmpS1969;
          _M0L6_2atmpS1967 = _M0L6_2atmpS1968 & 255;
          if (
            _M0L6_2atmpS1967 < 0
            || _M0L6_2atmpS1967
               >= Moonbit_array_length(_M0L11skip__tableS484)
          ) {
            #line 214 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS484[_M0L6_2atmpS1967] = _M0L1iS485;
          _M0L6_2atmpS1970 = _M0L1iS485 - 1;
          _M0L1iS485 = _M0L6_2atmpS1970;
          continue;
        }
        break;
      }
      _M0L6_2atmpS1981 = _M0L13haystack__lenS480 - _M0L11needle__lenS482;
      _M0L1iS487 = _M0L6_2atmpS1981;
      while (1) {
        if (_M0L1iS487 >= 0) {
          int32_t _M0L1jS488 = 0;
          int32_t _M0L6_2atmpS1980;
          int32_t _M0L6_2atmpS1979;
          int32_t _M0L6_2atmpS1978;
          int32_t _M0L6_2atmpS1977;
          int32_t _M0L6_2atmpS1976;
          while (1) {
            if (_M0L1jS488 < _M0L11needle__lenS482) {
              int32_t _M0L6_2atmpS1974 = _M0L1iS487 + _M0L1jS488;
              int32_t _M0L6_2atmpS1972;
              int32_t _M0L6_2atmpS1973;
              int32_t _M0L6_2atmpS1975;
              moonbit_incref(_M0L8haystackS481.$0);
              #line 221 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1972
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS481, _M0L6_2atmpS1974);
              moonbit_incref(_M0L6needleS483.$0);
              #line 221 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1973
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS483, _M0L1jS488);
              if (_M0L6_2atmpS1972 != _M0L6_2atmpS1973) {
                break;
              }
              _M0L6_2atmpS1975 = _M0L1jS488 + 1;
              _M0L1jS488 = _M0L6_2atmpS1975;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS484);
              moonbit_decref(_M0L6needleS483.$0);
              moonbit_decref(_M0L8haystackS481.$0);
              return (int64_t)_M0L1iS487;
            }
            break;
          }
          moonbit_incref(_M0L8haystackS481.$0);
          #line 218 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1980
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS481, _M0L1iS487);
          _M0L6_2atmpS1979 = (int32_t)_M0L6_2atmpS1980;
          _M0L6_2atmpS1978 = _M0L6_2atmpS1979 & 255;
          if (
            _M0L6_2atmpS1978 < 0
            || _M0L6_2atmpS1978
               >= Moonbit_array_length(_M0L11skip__tableS484)
          ) {
            #line 218 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1977 = (int32_t)_M0L11skip__tableS484[_M0L6_2atmpS1978];
          _M0L6_2atmpS1976 = _M0L1iS487 - _M0L6_2atmpS1977;
          _M0L1iS487 = _M0L6_2atmpS1976;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS484);
          moonbit_decref(_M0L6needleS483.$0);
          moonbit_decref(_M0L8haystackS481.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS483.$0);
      moonbit_decref(_M0L8haystackS481.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS483.$0);
    moonbit_decref(_M0L8haystackS481.$0);
    return (int64_t)_M0L13haystack__lenS480;
  }
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS479,
  struct _M0TPC16string10StringView _M0L3strS478
) {
  int32_t _M0L6_2atmpS1966;
  #line 18 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L3strS478.$0);
  #line 19 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1966 = _M0MPC16string10StringView6length(_M0L3strS478);
  if (_M0L6_2atmpS1966 <= 4) {
    #line 20 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS479, _M0L3strS478);
  } else {
    #line 22 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS479, _M0L3strS478);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS469,
  struct _M0TPC16string10StringView _M0L6needleS471
) {
  int32_t _M0L13haystack__lenS468;
  int32_t _M0L11needle__lenS470;
  #line 31 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L8haystackS469.$0);
  #line 32 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L13haystack__lenS468
  = _M0MPC16string10StringView6length(_M0L8haystackS469);
  moonbit_incref(_M0L6needleS471.$0);
  #line 33 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L11needle__lenS470 = _M0MPC16string10StringView6length(_M0L6needleS471);
  if (_M0L11needle__lenS470 > 0) {
    if (_M0L13haystack__lenS468 >= _M0L11needle__lenS470) {
      int32_t _M0L13needle__firstS472;
      int32_t _M0L12forward__lenS473;
      int32_t _M0L1iS474;
      moonbit_incref(_M0L6needleS471.$0);
      #line 36 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
      _M0L13needle__firstS472
      = _M0MPC16string10StringView11unsafe__get(_M0L6needleS471, 0);
      _M0L12forward__lenS473
      = _M0L13haystack__lenS468 - _M0L11needle__lenS470;
      _M0L1iS474 = 0;
      while (1) {
        if (_M0L1iS474 <= _M0L12forward__lenS473) {
          int32_t _M0L6_2atmpS1959;
          int32_t _M0L1jS476;
          int32_t _M0L6_2atmpS1965;
          moonbit_incref(_M0L8haystackS469.$0);
          #line 39 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1959
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS469, _M0L1iS474);
          if (_M0L6_2atmpS1959 != _M0L13needle__firstS472) {
            int32_t _M0L6_2atmpS1960 = _M0L1iS474 + 1;
            _M0L1iS474 = _M0L6_2atmpS1960;
            continue;
          }
          _M0L1jS476 = 1;
          while (1) {
            if (_M0L1jS476 < _M0L11needle__lenS470) {
              int32_t _M0L6_2atmpS1963 = _M0L1iS474 + _M0L1jS476;
              int32_t _M0L6_2atmpS1961;
              int32_t _M0L6_2atmpS1962;
              int32_t _M0L6_2atmpS1964;
              moonbit_incref(_M0L8haystackS469.$0);
              #line 44 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1961
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS469, _M0L6_2atmpS1963);
              moonbit_incref(_M0L6needleS471.$0);
              #line 44 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1962
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS471, _M0L1jS476);
              if (_M0L6_2atmpS1961 != _M0L6_2atmpS1962) {
                break;
              }
              _M0L6_2atmpS1964 = _M0L1jS476 + 1;
              _M0L1jS476 = _M0L6_2atmpS1964;
              continue;
            } else {
              moonbit_decref(_M0L6needleS471.$0);
              moonbit_decref(_M0L8haystackS469.$0);
              return (int64_t)_M0L1iS474;
            }
            break;
          }
          _M0L6_2atmpS1965 = _M0L1iS474 + 1;
          _M0L1iS474 = _M0L6_2atmpS1965;
          continue;
        } else {
          moonbit_decref(_M0L6needleS471.$0);
          moonbit_decref(_M0L8haystackS469.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS471.$0);
      moonbit_decref(_M0L8haystackS469.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS471.$0);
    moonbit_decref(_M0L8haystackS469.$0);
    return _M0FPB18brute__force__findN6constrS9146;
  }
}

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView _M0L8haystackS456,
  struct _M0TPC16string10StringView _M0L6needleS458
) {
  int32_t _M0L13haystack__lenS455;
  int32_t _M0L11needle__lenS457;
  #line 58 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L8haystackS456.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L13haystack__lenS455
  = _M0MPC16string10StringView6length(_M0L8haystackS456);
  moonbit_incref(_M0L6needleS458.$0);
  #line 63 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L11needle__lenS457 = _M0MPC16string10StringView6length(_M0L6needleS458);
  if (_M0L11needle__lenS457 > 0) {
    if (_M0L13haystack__lenS455 >= _M0L11needle__lenS457) {
      int32_t* _M0L11skip__tableS459 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS457);
      int32_t _M0L7_2abindS460 = _M0L11needle__lenS457 - 1;
      int32_t _M0L1iS461 = 0;
      int32_t _M0L1iS463;
      while (1) {
        if (_M0L1iS461 < _M0L7_2abindS460) {
          int32_t _M0L6_2atmpS1945;
          int32_t _M0L6_2atmpS1944;
          int32_t _M0L6_2atmpS1941;
          int32_t _M0L6_2atmpS1943;
          int32_t _M0L6_2atmpS1942;
          int32_t _M0L6_2atmpS1946;
          moonbit_incref(_M0L6needleS458.$0);
          #line 69 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1945
          = _M0MPC16string10StringView11unsafe__get(_M0L6needleS458, _M0L1iS461);
          _M0L6_2atmpS1944 = (int32_t)_M0L6_2atmpS1945;
          _M0L6_2atmpS1941 = _M0L6_2atmpS1944 & 255;
          _M0L6_2atmpS1943 = _M0L11needle__lenS457 - 1;
          _M0L6_2atmpS1942 = _M0L6_2atmpS1943 - _M0L1iS461;
          if (
            _M0L6_2atmpS1941 < 0
            || _M0L6_2atmpS1941
               >= Moonbit_array_length(_M0L11skip__tableS459)
          ) {
            #line 69 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS459[_M0L6_2atmpS1941] = _M0L6_2atmpS1942;
          _M0L6_2atmpS1946 = _M0L1iS461 + 1;
          _M0L1iS461 = _M0L6_2atmpS1946;
          continue;
        }
        break;
      }
      _M0L1iS463 = 0;
      while (1) {
        int32_t _M0L6_2atmpS1947 =
          _M0L13haystack__lenS455 - _M0L11needle__lenS457;
        if (_M0L1iS463 <= _M0L6_2atmpS1947) {
          int32_t _M0L7_2abindS464 = _M0L11needle__lenS457 - 1;
          int32_t _M0L1jS465 = 0;
          int32_t _M0L6_2atmpS1958;
          int32_t _M0L6_2atmpS1957;
          int32_t _M0L6_2atmpS1956;
          int32_t _M0L6_2atmpS1955;
          int32_t _M0L6_2atmpS1954;
          int32_t _M0L6_2atmpS1953;
          int32_t _M0L6_2atmpS1952;
          while (1) {
            if (_M0L1jS465 <= _M0L7_2abindS464) {
              int32_t _M0L6_2atmpS1950 = _M0L1iS463 + _M0L1jS465;
              int32_t _M0L6_2atmpS1948;
              int32_t _M0L6_2atmpS1949;
              int32_t _M0L6_2atmpS1951;
              moonbit_incref(_M0L8haystackS456.$0);
              #line 77 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1948
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS456, _M0L6_2atmpS1950);
              moonbit_incref(_M0L6needleS458.$0);
              #line 77 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1949
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS458, _M0L1jS465);
              if (_M0L6_2atmpS1948 != _M0L6_2atmpS1949) {
                break;
              }
              _M0L6_2atmpS1951 = _M0L1jS465 + 1;
              _M0L1jS465 = _M0L6_2atmpS1951;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS459);
              moonbit_decref(_M0L6needleS458.$0);
              moonbit_decref(_M0L8haystackS456.$0);
              return (int64_t)_M0L1iS463;
            }
            break;
          }
          _M0L6_2atmpS1958 = _M0L1iS463 + _M0L11needle__lenS457;
          _M0L6_2atmpS1957 = _M0L6_2atmpS1958 - 1;
          moonbit_incref(_M0L8haystackS456.$0);
          #line 74 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1956
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS456, _M0L6_2atmpS1957);
          _M0L6_2atmpS1955 = (int32_t)_M0L6_2atmpS1956;
          _M0L6_2atmpS1954 = _M0L6_2atmpS1955 & 255;
          if (
            _M0L6_2atmpS1954 < 0
            || _M0L6_2atmpS1954
               >= Moonbit_array_length(_M0L11skip__tableS459)
          ) {
            #line 74 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1953 = (int32_t)_M0L11skip__tableS459[_M0L6_2atmpS1954];
          _M0L6_2atmpS1952 = _M0L1iS463 + _M0L6_2atmpS1953;
          _M0L1iS463 = _M0L6_2atmpS1952;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS459);
          moonbit_decref(_M0L6needleS458.$0);
          moonbit_decref(_M0L8haystackS456.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS458.$0);
      moonbit_decref(_M0L8haystackS456.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS458.$0);
    moonbit_decref(_M0L8haystackS456.$0);
    return _M0FPB28boyer__moore__horspool__findN6constrS9145;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS454,
  struct _M0TPC16string10StringView _M0L3strS453
) {
  int32_t _M0L8str__lenS452;
  int32_t _M0L3lenS1934;
  int32_t _M0L6_2atmpS1933;
  uint16_t* _M0L4dataS1935;
  int32_t _M0L3lenS1936;
  moonbit_string_t _M0L6_2atmpS1937;
  int32_t _M0L6_2atmpS1938;
  int32_t _M0L3lenS1940;
  int32_t _M0L6_2atmpS1939;
  #line 126 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  moonbit_incref(_M0L3strS453.$0);
  #line 130 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS452 = _M0MPC16string10StringView6length(_M0L3strS453);
  _M0L3lenS1934 = _M0L4selfS454->$1;
  _M0L6_2atmpS1933 = _M0L3lenS1934 + _M0L8str__lenS452;
  moonbit_incref(_M0L4selfS454);
  #line 131 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS454, _M0L6_2atmpS1933);
  _M0L4dataS1935 = _M0L4selfS454->$0;
  _M0L3lenS1936 = _M0L4selfS454->$1;
  moonbit_incref(_M0L4dataS1935);
  moonbit_incref(_M0L3strS453.$0);
  #line 134 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1937 = _M0MPC16string10StringView4data(_M0L3strS453);
  #line 135 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1938 = _M0MPC16string10StringView13start__offset(_M0L3strS453);
  #line 132 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1935, _M0L3lenS1936, _M0L6_2atmpS1937, _M0L6_2atmpS1938, _M0L8str__lenS452);
  _M0L3lenS1940 = _M0L4selfS454->$1;
  _M0L6_2atmpS1939 = _M0L3lenS1940 + _M0L8str__lenS452;
  _M0L4selfS454->$1 = _M0L6_2atmpS1939;
  moonbit_decref(_M0L4selfS454);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS444,
  int32_t _M0L3lenS447,
  int32_t _M0L13start__offsetS451,
  int64_t _M0L11end__offsetS442
) {
  int32_t _M0L11end__offsetS441;
  int32_t _M0L5indexS445;
  int32_t _M0L5countS446;
  #line 441 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS442 == 4294967296ll) {
    _M0L11end__offsetS441 = Moonbit_array_length(_M0L4selfS444);
  } else {
    int64_t _M0L7_2aSomeS443 = _M0L11end__offsetS442;
    _M0L11end__offsetS441 = (int32_t)_M0L7_2aSomeS443;
  }
  _M0L5indexS445 = _M0L13start__offsetS451;
  _M0L5countS446 = 0;
  while (1) {
    int32_t _if__result_4186;
    if (_M0L5indexS445 < _M0L11end__offsetS441) {
      _if__result_4186 = _M0L5countS446 < _M0L3lenS447;
    } else {
      _if__result_4186 = 0;
    }
    if (_if__result_4186) {
      int32_t _M0L2c1S448 = _M0L4selfS444[_M0L5indexS445];
      int32_t _if__result_4187;
      int32_t _M0L6_2atmpS1931;
      int32_t _M0L6_2atmpS1932;
      #line 452 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S448)) {
        int32_t _M0L6_2atmpS1927 = _M0L5indexS445 + 1;
        _if__result_4187 = _M0L6_2atmpS1927 < _M0L11end__offsetS441;
      } else {
        _if__result_4187 = 0;
      }
      if (_if__result_4187) {
        int32_t _M0L6_2atmpS1930 = _M0L5indexS445 + 1;
        int32_t _M0L2c2S449 = _M0L4selfS444[_M0L6_2atmpS1930];
        #line 454 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S449)) {
          int32_t _M0L6_2atmpS1928 = _M0L5indexS445 + 2;
          int32_t _M0L6_2atmpS1929 = _M0L5countS446 + 1;
          _M0L5indexS445 = _M0L6_2atmpS1928;
          _M0L5countS446 = _M0L6_2atmpS1929;
          continue;
        } else {
          #line 457 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_44.data);
        }
      }
      _M0L6_2atmpS1931 = _M0L5indexS445 + 1;
      _M0L6_2atmpS1932 = _M0L5countS446 + 1;
      _M0L5indexS445 = _M0L6_2atmpS1931;
      _M0L5countS446 = _M0L6_2atmpS1932;
      continue;
    } else {
      moonbit_decref(_M0L4selfS444);
      return _M0L5countS446 >= _M0L3lenS447;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS433,
  int32_t _M0L3lenS436,
  int32_t _M0L13start__offsetS440,
  int64_t _M0L11end__offsetS431
) {
  int32_t _M0L11end__offsetS430;
  int32_t _M0L5indexS434;
  int32_t _M0L5countS435;
  #line 413 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS431 == 4294967296ll) {
    _M0L11end__offsetS430 = Moonbit_array_length(_M0L4selfS433);
  } else {
    int64_t _M0L7_2aSomeS432 = _M0L11end__offsetS431;
    _M0L11end__offsetS430 = (int32_t)_M0L7_2aSomeS432;
  }
  _M0L5indexS434 = _M0L13start__offsetS440;
  _M0L5countS435 = 0;
  while (1) {
    int32_t _if__result_4189;
    if (_M0L5indexS434 < _M0L11end__offsetS430) {
      _if__result_4189 = _M0L5countS435 < _M0L3lenS436;
    } else {
      _if__result_4189 = 0;
    }
    if (_if__result_4189) {
      int32_t _M0L2c1S437 = _M0L4selfS433[_M0L5indexS434];
      int32_t _if__result_4190;
      int32_t _M0L6_2atmpS1925;
      int32_t _M0L6_2atmpS1926;
      #line 424 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S437)) {
        int32_t _M0L6_2atmpS1921 = _M0L5indexS434 + 1;
        _if__result_4190 = _M0L6_2atmpS1921 < _M0L11end__offsetS430;
      } else {
        _if__result_4190 = 0;
      }
      if (_if__result_4190) {
        int32_t _M0L6_2atmpS1924 = _M0L5indexS434 + 1;
        int32_t _M0L2c2S438 = _M0L4selfS433[_M0L6_2atmpS1924];
        #line 426 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S438)) {
          int32_t _M0L6_2atmpS1922 = _M0L5indexS434 + 2;
          int32_t _M0L6_2atmpS1923 = _M0L5countS435 + 1;
          _M0L5indexS434 = _M0L6_2atmpS1922;
          _M0L5countS435 = _M0L6_2atmpS1923;
          continue;
        } else {
          #line 429 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_44.data);
        }
      }
      _M0L6_2atmpS1925 = _M0L5indexS434 + 1;
      _M0L6_2atmpS1926 = _M0L5countS435 + 1;
      _M0L5indexS434 = _M0L6_2atmpS1925;
      _M0L5countS435 = _M0L6_2atmpS1926;
      continue;
    } else {
      moonbit_decref(_M0L4selfS433);
      if (_M0L5countS435 == _M0L3lenS436) {
        return _M0L5indexS434 == _M0L11end__offsetS430;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS427
) {
  int32_t _M0L3endS1915;
  int32_t _M0L5startS1916;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1915 = _M0L4selfS427.$2;
  _M0L5startS1916 = _M0L4selfS427.$1;
  moonbit_decref(_M0L4selfS427.$0);
  return _M0L3endS1915 - _M0L5startS1916;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS428
) {
  int32_t _M0L3endS1917;
  int32_t _M0L5startS1918;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1917 = _M0L4selfS428.$2;
  _M0L5startS1918 = _M0L4selfS428.$1;
  moonbit_decref(_M0L4selfS428.$0);
  return _M0L3endS1917 - _M0L5startS1918;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS429
) {
  int32_t _M0L3endS1919;
  int32_t _M0L5startS1920;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1919 = _M0L4selfS429.$2;
  _M0L5startS1920 = _M0L4selfS429.$1;
  moonbit_decref(_M0L4selfS429.$0);
  return _M0L3endS1919 - _M0L5startS1920;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS425,
  int64_t _M0L19start__offset_2eoptS423,
  int64_t _M0L11end__offsetS426
) {
  int32_t _M0L13start__offsetS422;
  if (_M0L19start__offset_2eoptS423 == 4294967296ll) {
    _M0L13start__offsetS422 = 0;
  } else {
    int64_t _M0L7_2aSomeS424 = _M0L19start__offset_2eoptS423;
    _M0L13start__offsetS422 = (int32_t)_M0L7_2aSomeS424;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS425, _M0L13start__offsetS422, _M0L11end__offsetS426);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS420,
  int32_t _M0L13start__offsetS421,
  int64_t _M0L11end__offsetS418
) {
  int32_t _M0L11end__offsetS417;
  int32_t _if__result_4191;
  #line 512 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS418 == 4294967296ll) {
    _M0L11end__offsetS417 = Moonbit_array_length(_M0L4selfS420);
  } else {
    int64_t _M0L7_2aSomeS419 = _M0L11end__offsetS418;
    _M0L11end__offsetS417 = (int32_t)_M0L7_2aSomeS419;
  }
  if (_M0L13start__offsetS421 >= 0) {
    if (_M0L13start__offsetS421 <= _M0L11end__offsetS417) {
      int32_t _M0L6_2atmpS1914 = Moonbit_array_length(_M0L4selfS420);
      _if__result_4191 = _M0L11end__offsetS417 <= _M0L6_2atmpS1914;
    } else {
      _if__result_4191 = 0;
    }
  } else {
    _if__result_4191 = 0;
  }
  if (_if__result_4191) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS421,
                                                 _M0L11end__offsetS417,
                                                 _M0L4selfS420};
  } else {
    moonbit_decref(_M0L4selfS420);
    #line 521 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_45.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS416
) {
  #line 197 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 198 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L4selfS416);
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS415
) {
  moonbit_string_t _M0L3strS1911;
  int32_t _M0L5startS1912;
  int32_t _M0L3endS1913;
  #line 190 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1911 = _M0L4selfS415.$0;
  _M0L5startS1912 = _M0L4selfS415.$1;
  _M0L3endS1913 = _M0L4selfS415.$2;
  #line 193 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1911, _M0L5startS1912, _M0L3endS1913);
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS412,
  int32_t _M0L5startS410,
  int32_t _M0L3endS411
) {
  int32_t _if__result_4192;
  int32_t _M0L3lenS413;
  int32_t _M0L6_2atmpS1909;
  int32_t _M0L6_2atmpS1910;
  moonbit_bytes_t _M0L5bytesS414;
  moonbit_bytes_t _M0L6_2atmpS1908;
  #line 91 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS410 == 0) {
    int32_t _M0L6_2atmpS1907 = Moonbit_array_length(_M0L3strS412);
    _if__result_4192 = _M0L3endS411 == _M0L6_2atmpS1907;
  } else {
    _if__result_4192 = 0;
  }
  if (_if__result_4192) {
    return _M0L3strS412;
  }
  _M0L3lenS413 = _M0L3endS411 - _M0L5startS410;
  _M0L6_2atmpS1909 = _M0L3lenS413 * 2;
  #line 101 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1910 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS414
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1909, _M0L6_2atmpS1910);
  moonbit_incref(_M0L5bytesS414);
  #line 102 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS414, 0, _M0L3strS412, _M0L5startS410, _M0L3lenS413);
  _M0L6_2atmpS1908 = _M0L5bytesS414;
  #line 103 "/Users/user/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1908, 0, 4294967296ll);
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS405,
  int32_t _M0L6offsetS409,
  int64_t _M0L6lengthS407
) {
  int32_t _M0L3lenS404;
  int32_t _M0L6lengthS406;
  int32_t _if__result_4193;
  #line 76 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS404 = Moonbit_array_length(_M0L4selfS405);
  if (_M0L6lengthS407 == 4294967296ll) {
    _M0L6lengthS406 = _M0L3lenS404 - _M0L6offsetS409;
  } else {
    int64_t _M0L7_2aSomeS408 = _M0L6lengthS407;
    _M0L6lengthS406 = (int32_t)_M0L7_2aSomeS408;
  }
  if (_M0L6offsetS409 >= 0) {
    if (_M0L6lengthS406 >= 0) {
      int32_t _M0L6_2atmpS1906 = _M0L6offsetS409 + _M0L6lengthS406;
      _if__result_4193 = _M0L6_2atmpS1906 <= _M0L3lenS404;
    } else {
      _if__result_4193 = 0;
    }
  } else {
    _if__result_4193 = 0;
  }
  if (_if__result_4193) {
    #line 84 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS405, _M0L6offsetS409, _M0L6lengthS406);
  } else {
    moonbit_decref(_M0L4selfS405);
    #line 83 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS396,
  int32_t _M0L13bytes__offsetS391,
  moonbit_string_t _M0L3strS398,
  int32_t _M0L11str__offsetS394,
  int32_t _M0L6lengthS392
) {
  int32_t _M0L6_2atmpS1905;
  int32_t _M0L6_2atmpS1904;
  int32_t _M0L2e1S390;
  int32_t _M0L6_2atmpS1903;
  int32_t _M0L2e2S393;
  int32_t _M0L4len1S395;
  int32_t _M0L4len2S397;
  int32_t _if__result_4194;
  #line 124 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1905 = _M0L6lengthS392 * 2;
  _M0L6_2atmpS1904 = _M0L13bytes__offsetS391 + _M0L6_2atmpS1905;
  _M0L2e1S390 = _M0L6_2atmpS1904 - 1;
  _M0L6_2atmpS1903 = _M0L11str__offsetS394 + _M0L6lengthS392;
  _M0L2e2S393 = _M0L6_2atmpS1903 - 1;
  _M0L4len1S395 = Moonbit_array_length(_M0L4selfS396);
  _M0L4len2S397 = Moonbit_array_length(_M0L3strS398);
  if (_M0L6lengthS392 >= 0) {
    if (_M0L13bytes__offsetS391 >= 0) {
      if (_M0L2e1S390 < _M0L4len1S395) {
        if (_M0L11str__offsetS394 >= 0) {
          _if__result_4194 = _M0L2e2S393 < _M0L4len2S397;
        } else {
          _if__result_4194 = 0;
        }
      } else {
        _if__result_4194 = 0;
      }
    } else {
      _if__result_4194 = 0;
    }
  } else {
    _if__result_4194 = 0;
  }
  if (_if__result_4194) {
    int32_t _M0L16end__str__offsetS399 =
      _M0L11str__offsetS394 + _M0L6lengthS392;
    int32_t _M0L1iS400 = _M0L11str__offsetS394;
    int32_t _M0L1jS401 = _M0L13bytes__offsetS391;
    while (1) {
      if (_M0L1iS400 < _M0L16end__str__offsetS399) {
        int32_t _M0L6_2atmpS1900 = _M0L3strS398[_M0L1iS400];
        int32_t _M0L6_2atmpS1899 = (int32_t)_M0L6_2atmpS1900;
        uint32_t _M0L1cS402 = *(uint32_t*)&_M0L6_2atmpS1899;
        uint32_t _M0L6_2atmpS1895 = _M0L1cS402 & 255u;
        int32_t _M0L6_2atmpS1894;
        int32_t _M0L6_2atmpS1896;
        uint32_t _M0L6_2atmpS1898;
        int32_t _M0L6_2atmpS1897;
        int32_t _M0L6_2atmpS1901;
        int32_t _M0L6_2atmpS1902;
        #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1894 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1895);
        if (
          _M0L1jS401 < 0 || _M0L1jS401 >= Moonbit_array_length(_M0L4selfS396)
        ) {
          #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS396[_M0L1jS401] = _M0L6_2atmpS1894;
        _M0L6_2atmpS1896 = _M0L1jS401 + 1;
        _M0L6_2atmpS1898 = _M0L1cS402 >> 8;
        #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1897 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1898);
        if (
          _M0L6_2atmpS1896 < 0
          || _M0L6_2atmpS1896 >= Moonbit_array_length(_M0L4selfS396)
        ) {
          #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS396[_M0L6_2atmpS1896] = _M0L6_2atmpS1897;
        _M0L6_2atmpS1901 = _M0L1iS400 + 1;
        _M0L6_2atmpS1902 = _M0L1jS401 + 2;
        _M0L1iS400 = _M0L6_2atmpS1901;
        _M0L1jS401 = _M0L6_2atmpS1902;
        continue;
      } else {
        moonbit_decref(_M0L3strS398);
        moonbit_decref(_M0L4selfS396);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS398);
    moonbit_decref(_M0L4selfS396);
    #line 137 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS389) {
  int32_t _M0L6_2atmpS1893;
  #line 2518 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1893 = *(int32_t*)&_M0L4selfS389;
  return _M0L6_2atmpS1893 & 0xff;
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS387,
  struct _M0TPB6Logger _M0L6loggerS388
) {
  #line 166 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 167 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L4selfS387, _M0L6loggerS388, 1);
  return 0;
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS386) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS386;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS370,
  int32_t _M0L5radixS369
) {
  int32_t _if__result_4196;
  int32_t _M0L12is__negativeS371;
  uint32_t _M0L3numS372;
  uint16_t* _M0L6bufferS373;
  #line 209 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS369 < 2) {
    _if__result_4196 = 1;
  } else {
    _if__result_4196 = _M0L5radixS369 > 36;
  }
  if (_if__result_4196) {
    #line 213 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_47.data);
  }
  if (_M0L4selfS370 == 0) {
    return (moonbit_string_t)moonbit_string_literal_48.data;
  }
  _M0L12is__negativeS371 = _M0L4selfS370 < 0;
  if (_M0L12is__negativeS371) {
    int32_t _M0L6_2atmpS1892 = -_M0L4selfS370;
    _M0L3numS372 = *(uint32_t*)&_M0L6_2atmpS1892;
  } else {
    _M0L3numS372 = *(uint32_t*)&_M0L4selfS370;
  }
  switch (_M0L5radixS369) {
    case 10: {
      int32_t _M0L10digit__lenS374;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L10total__lenS375;
      uint16_t* _M0L6bufferS376;
      int32_t _M0L12digit__startS377;
      #line 235 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS374 = _M0FPB12dec__count32(_M0L3numS372);
      if (_M0L12is__negativeS371) {
        _M0L6_2atmpS1889 = 1;
      } else {
        _M0L6_2atmpS1889 = 0;
      }
      _M0L10total__lenS375 = _M0L10digit__lenS374 + _M0L6_2atmpS1889;
      _M0L6bufferS376
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS375, 0);
      if (_M0L12is__negativeS371) {
        _M0L12digit__startS377 = 1;
      } else {
        _M0L12digit__startS377 = 0;
      }
      moonbit_incref(_M0L6bufferS376);
      #line 239 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS376, _M0L3numS372, _M0L12digit__startS377, _M0L10total__lenS375);
      _M0L6bufferS373 = _M0L6bufferS376;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS378;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L10total__lenS379;
      uint16_t* _M0L6bufferS380;
      int32_t _M0L12digit__startS381;
      #line 243 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS378 = _M0FPB12hex__count32(_M0L3numS372);
      if (_M0L12is__negativeS371) {
        _M0L6_2atmpS1890 = 1;
      } else {
        _M0L6_2atmpS1890 = 0;
      }
      _M0L10total__lenS379 = _M0L10digit__lenS378 + _M0L6_2atmpS1890;
      _M0L6bufferS380
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS379, 0);
      if (_M0L12is__negativeS371) {
        _M0L12digit__startS381 = 1;
      } else {
        _M0L12digit__startS381 = 0;
      }
      moonbit_incref(_M0L6bufferS380);
      #line 247 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS380, _M0L3numS372, _M0L12digit__startS381, _M0L10total__lenS379);
      _M0L6bufferS373 = _M0L6bufferS380;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS382;
      int32_t _M0L6_2atmpS1891;
      int32_t _M0L10total__lenS383;
      uint16_t* _M0L6bufferS384;
      int32_t _M0L12digit__startS385;
      #line 251 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS382
      = _M0FPB14radix__count32(_M0L3numS372, _M0L5radixS369);
      if (_M0L12is__negativeS371) {
        _M0L6_2atmpS1891 = 1;
      } else {
        _M0L6_2atmpS1891 = 0;
      }
      _M0L10total__lenS383 = _M0L10digit__lenS382 + _M0L6_2atmpS1891;
      _M0L6bufferS384
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS383, 0);
      if (_M0L12is__negativeS371) {
        _M0L12digit__startS385 = 1;
      } else {
        _M0L12digit__startS385 = 0;
      }
      moonbit_incref(_M0L6bufferS384);
      #line 255 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS384, _M0L3numS372, _M0L12digit__startS385, _M0L10total__lenS383, _M0L5radixS369);
      _M0L6bufferS373 = _M0L6bufferS384;
      break;
    }
  }
  if (_M0L12is__negativeS371) {
    _M0L6bufferS373[0] = 45;
  }
  return _M0L6bufferS373;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS363,
  int32_t _M0L5radixS365
) {
  uint32_t _M0L4baseS364;
  uint32_t _M0L3numS366;
  int32_t _M0L5countS367;
  #line 189 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS363 == 0u) {
    return 1;
  }
  _M0L4baseS364 = *(uint32_t*)&_M0L5radixS365;
  _M0L3numS366 = _M0L5valueS363;
  _M0L5countS367 = 0;
  while (1) {
    if (_M0L3numS366 > 0u) {
      uint32_t _M0L6_2atmpS1887 = _M0L3numS366 / _M0L4baseS364;
      int32_t _M0L6_2atmpS1888 = _M0L5countS367 + 1;
      _M0L3numS366 = _M0L6_2atmpS1887;
      _M0L5countS367 = _M0L6_2atmpS1888;
      continue;
    } else {
      return _M0L5countS367;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS361) {
  #line 177 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS361 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS362;
    int32_t _M0L6_2atmpS1886;
    int32_t _M0L6_2atmpS1885;
    #line 182 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS362 = moonbit_clz32(_M0L5valueS361);
    _M0L6_2atmpS1886 = 31 - _M0L14leading__zerosS362;
    _M0L6_2atmpS1885 = _M0L6_2atmpS1886 / 4;
    return _M0L6_2atmpS1885 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS360) {
  #line 143 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS360 >= 100000u) {
    if (_M0L5valueS360 >= 10000000u) {
      if (_M0L5valueS360 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS360 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS360 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS360 >= 1000u) {
    if (_M0L5valueS360 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS360 >= 100u) {
    return 3;
  } else if (_M0L5valueS360 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS346,
  uint32_t _M0L3numS358,
  int32_t _M0L12digit__startS347,
  int32_t _M0L10total__lenS359
) {
  int32_t _M0L6_2atmpS1884;
  uint32_t _M0L3numS336;
  int32_t _M0L6offsetS337;
  #line 88 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1884 = _M0L10total__lenS359 - _M0L12digit__startS347;
  _M0L3numS336 = _M0L3numS358;
  _M0L6offsetS337 = _M0L6_2atmpS1884;
  while (1) {
    if (_M0L3numS336 >= 10000u) {
      uint32_t _M0L1tS338 = _M0L3numS336 / 10000u;
      uint32_t _M0L6_2atmpS1861 = _M0L3numS336 % 10000u;
      int32_t _M0L1rS339 = *(int32_t*)&_M0L6_2atmpS1861;
      int32_t _M0L2d1S340 = _M0L1rS339 / 100;
      int32_t _M0L2d2S341 = _M0L1rS339 % 100;
      int32_t _M0L6_2atmpS1860 = _M0L2d1S340 / 10;
      int32_t _M0L6_2atmpS1859 = 48 + _M0L6_2atmpS1860;
      int32_t _M0L6d1__hiS342 = (uint16_t)_M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1858 = _M0L2d1S340 % 10;
      int32_t _M0L6_2atmpS1857 = 48 + _M0L6_2atmpS1858;
      int32_t _M0L6d1__loS343 = (uint16_t)_M0L6_2atmpS1857;
      int32_t _M0L6_2atmpS1856 = _M0L2d2S341 / 10;
      int32_t _M0L6_2atmpS1855 = 48 + _M0L6_2atmpS1856;
      int32_t _M0L6d2__hiS344 = (uint16_t)_M0L6_2atmpS1855;
      int32_t _M0L6_2atmpS1854 = _M0L2d2S341 % 10;
      int32_t _M0L6_2atmpS1853 = 48 + _M0L6_2atmpS1854;
      int32_t _M0L6d2__loS345 = (uint16_t)_M0L6_2atmpS1853;
      int32_t _M0L6_2atmpS1845 = _M0L12digit__startS347 + _M0L6offsetS337;
      int32_t _M0L6_2atmpS1844 = _M0L6_2atmpS1845 - 4;
      int32_t _M0L6_2atmpS1847;
      int32_t _M0L6_2atmpS1846;
      int32_t _M0L6_2atmpS1849;
      int32_t _M0L6_2atmpS1848;
      int32_t _M0L6_2atmpS1851;
      int32_t _M0L6_2atmpS1850;
      int32_t _M0L6_2atmpS1852;
      _M0L6bufferS346[_M0L6_2atmpS1844] = _M0L6d1__hiS342;
      _M0L6_2atmpS1847 = _M0L12digit__startS347 + _M0L6offsetS337;
      _M0L6_2atmpS1846 = _M0L6_2atmpS1847 - 3;
      _M0L6bufferS346[_M0L6_2atmpS1846] = _M0L6d1__loS343;
      _M0L6_2atmpS1849 = _M0L12digit__startS347 + _M0L6offsetS337;
      _M0L6_2atmpS1848 = _M0L6_2atmpS1849 - 2;
      _M0L6bufferS346[_M0L6_2atmpS1848] = _M0L6d2__hiS344;
      _M0L6_2atmpS1851 = _M0L12digit__startS347 + _M0L6offsetS337;
      _M0L6_2atmpS1850 = _M0L6_2atmpS1851 - 1;
      _M0L6bufferS346[_M0L6_2atmpS1850] = _M0L6d2__loS345;
      _M0L6_2atmpS1852 = _M0L6offsetS337 - 4;
      _M0L3numS336 = _M0L1tS338;
      _M0L6offsetS337 = _M0L6_2atmpS1852;
      continue;
    } else {
      int32_t _M0L6_2atmpS1883 = *(int32_t*)&_M0L3numS336;
      int32_t _M0L9remainingS349 = _M0L6_2atmpS1883;
      int32_t _M0L6offsetS350 = _M0L6offsetS337;
      while (1) {
        if (_M0L9remainingS349 >= 100) {
          int32_t _M0L1tS351 = _M0L9remainingS349 / 100;
          int32_t _M0L1dS352 = _M0L9remainingS349 % 100;
          int32_t _M0L6_2atmpS1870 = _M0L1dS352 / 10;
          int32_t _M0L6_2atmpS1869 = 48 + _M0L6_2atmpS1870;
          int32_t _M0L5d__hiS353 = (uint16_t)_M0L6_2atmpS1869;
          int32_t _M0L6_2atmpS1868 = _M0L1dS352 % 10;
          int32_t _M0L6_2atmpS1867 = 48 + _M0L6_2atmpS1868;
          int32_t _M0L5d__loS354 = (uint16_t)_M0L6_2atmpS1867;
          int32_t _M0L6_2atmpS1863 = _M0L12digit__startS347 + _M0L6offsetS350;
          int32_t _M0L6_2atmpS1862 = _M0L6_2atmpS1863 - 2;
          int32_t _M0L6_2atmpS1865;
          int32_t _M0L6_2atmpS1864;
          int32_t _M0L6_2atmpS1866;
          _M0L6bufferS346[_M0L6_2atmpS1862] = _M0L5d__hiS353;
          _M0L6_2atmpS1865 = _M0L12digit__startS347 + _M0L6offsetS350;
          _M0L6_2atmpS1864 = _M0L6_2atmpS1865 - 1;
          _M0L6bufferS346[_M0L6_2atmpS1864] = _M0L5d__loS354;
          _M0L6_2atmpS1866 = _M0L6offsetS350 - 2;
          _M0L9remainingS349 = _M0L1tS351;
          _M0L6offsetS350 = _M0L6_2atmpS1866;
          continue;
        } else if (_M0L9remainingS349 >= 10) {
          int32_t _M0L6_2atmpS1878 = _M0L9remainingS349 / 10;
          int32_t _M0L6_2atmpS1877 = 48 + _M0L6_2atmpS1878;
          int32_t _M0L5d__hiS356 = (uint16_t)_M0L6_2atmpS1877;
          int32_t _M0L6_2atmpS1876 = _M0L9remainingS349 % 10;
          int32_t _M0L6_2atmpS1875 = 48 + _M0L6_2atmpS1876;
          int32_t _M0L5d__loS357 = (uint16_t)_M0L6_2atmpS1875;
          int32_t _M0L6_2atmpS1872 = _M0L12digit__startS347 + _M0L6offsetS350;
          int32_t _M0L6_2atmpS1871 = _M0L6_2atmpS1872 - 2;
          int32_t _M0L6_2atmpS1874;
          int32_t _M0L6_2atmpS1873;
          _M0L6bufferS346[_M0L6_2atmpS1871] = _M0L5d__hiS356;
          _M0L6_2atmpS1874 = _M0L12digit__startS347 + _M0L6offsetS350;
          _M0L6_2atmpS1873 = _M0L6_2atmpS1874 - 1;
          _M0L6bufferS346[_M0L6_2atmpS1873] = _M0L5d__loS357;
          moonbit_decref(_M0L6bufferS346);
        } else {
          int32_t _M0L6_2atmpS1882 = _M0L12digit__startS347 + _M0L6offsetS350;
          int32_t _M0L6_2atmpS1879 = _M0L6_2atmpS1882 - 1;
          int32_t _M0L6_2atmpS1881 = 48 + _M0L9remainingS349;
          int32_t _M0L6_2atmpS1880 = (uint16_t)_M0L6_2atmpS1881;
          _M0L6bufferS346[_M0L6_2atmpS1879] = _M0L6_2atmpS1880;
          moonbit_decref(_M0L6bufferS346);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS326,
  uint32_t _M0L3numS330,
  int32_t _M0L12digit__startS327,
  int32_t _M0L10total__lenS329,
  int32_t _M0L5radixS320
) {
  uint32_t _M0L4baseS319;
  int32_t _M0L6_2atmpS1829;
  int32_t _M0L6_2atmpS1828;
  #line 57 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS319 = *(uint32_t*)&_M0L5radixS320;
  _M0L6_2atmpS1829 = _M0L5radixS320 - 1;
  _M0L6_2atmpS1828 = _M0L5radixS320 & _M0L6_2atmpS1829;
  if (_M0L6_2atmpS1828 == 0) {
    int32_t _M0L5shiftS321;
    uint32_t _M0L4maskS322;
    int32_t _M0L6_2atmpS1836;
    int32_t _M0L6offsetS323;
    uint32_t _M0L1nS324;
    #line 68 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS321 = moonbit_ctz32(_M0L5radixS320);
    _M0L4maskS322 = _M0L4baseS319 - 1u;
    _M0L6_2atmpS1836 = _M0L10total__lenS329 - _M0L12digit__startS327;
    _M0L6offsetS323 = _M0L6_2atmpS1836;
    _M0L1nS324 = _M0L3numS330;
    while (1) {
      if (_M0L1nS324 > 0u) {
        uint32_t _M0L6_2atmpS1835 = _M0L1nS324 & _M0L4maskS322;
        int32_t _M0L5digitS325 = *(int32_t*)&_M0L6_2atmpS1835;
        int32_t _M0L6_2atmpS1832 = _M0L12digit__startS327 + _M0L6offsetS323;
        int32_t _M0L6_2atmpS1830 = _M0L6_2atmpS1832 - 1;
        int32_t _M0L6_2atmpS1831 =
          ((moonbit_string_t)moonbit_string_literal_49.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS1833;
        uint32_t _M0L6_2atmpS1834;
        _M0L6bufferS326[_M0L6_2atmpS1830] = _M0L6_2atmpS1831;
        _M0L6_2atmpS1833 = _M0L6offsetS323 - 1;
        _M0L6_2atmpS1834 = _M0L1nS324 >> (_M0L5shiftS321 & 31);
        _M0L6offsetS323 = _M0L6_2atmpS1833;
        _M0L1nS324 = _M0L6_2atmpS1834;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS326);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1843 = _M0L10total__lenS329 - _M0L12digit__startS327;
    int32_t _M0L6offsetS331 = _M0L6_2atmpS1843;
    uint32_t _M0L1nS332 = _M0L3numS330;
    while (1) {
      if (_M0L1nS332 > 0u) {
        uint32_t _M0L1qS333 = _M0L1nS332 / _M0L4baseS319;
        uint32_t _M0L6_2atmpS1842 = _M0L1qS333 * _M0L4baseS319;
        uint32_t _M0L6_2atmpS1841 = _M0L1nS332 - _M0L6_2atmpS1842;
        int32_t _M0L5digitS334 = *(int32_t*)&_M0L6_2atmpS1841;
        int32_t _M0L6_2atmpS1839 = _M0L12digit__startS327 + _M0L6offsetS331;
        int32_t _M0L6_2atmpS1837 = _M0L6_2atmpS1839 - 1;
        int32_t _M0L6_2atmpS1838 =
          ((moonbit_string_t)moonbit_string_literal_49.data)[_M0L5digitS334];
        int32_t _M0L6_2atmpS1840;
        _M0L6bufferS326[_M0L6_2atmpS1837] = _M0L6_2atmpS1838;
        _M0L6_2atmpS1840 = _M0L6offsetS331 - 1;
        _M0L6offsetS331 = _M0L6_2atmpS1840;
        _M0L1nS332 = _M0L1qS333;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS326);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS313,
  uint32_t _M0L3numS318,
  int32_t _M0L12digit__startS314,
  int32_t _M0L10total__lenS317
) {
  int32_t _M0L6_2atmpS1827;
  int32_t _M0L6offsetS308;
  uint32_t _M0L1nS309;
  #line 29 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1827 = _M0L10total__lenS317 - _M0L12digit__startS314;
  _M0L6offsetS308 = _M0L6_2atmpS1827;
  _M0L1nS309 = _M0L3numS318;
  while (1) {
    if (_M0L6offsetS308 >= 2) {
      uint32_t _M0L6_2atmpS1824 = _M0L1nS309 & 255u;
      int32_t _M0L9byte__valS310 = *(int32_t*)&_M0L6_2atmpS1824;
      int32_t _M0L2hiS311 = _M0L9byte__valS310 / 16;
      int32_t _M0L2loS312 = _M0L9byte__valS310 % 16;
      int32_t _M0L6_2atmpS1818 = _M0L12digit__startS314 + _M0L6offsetS308;
      int32_t _M0L6_2atmpS1816 = _M0L6_2atmpS1818 - 2;
      int32_t _M0L6_2atmpS1817 =
        ((moonbit_string_t)moonbit_string_literal_49.data)[_M0L2hiS311];
      int32_t _M0L6_2atmpS1821;
      int32_t _M0L6_2atmpS1819;
      int32_t _M0L6_2atmpS1820;
      int32_t _M0L6_2atmpS1822;
      uint32_t _M0L6_2atmpS1823;
      _M0L6bufferS313[_M0L6_2atmpS1816] = _M0L6_2atmpS1817;
      _M0L6_2atmpS1821 = _M0L12digit__startS314 + _M0L6offsetS308;
      _M0L6_2atmpS1819 = _M0L6_2atmpS1821 - 1;
      _M0L6_2atmpS1820
      = ((moonbit_string_t)moonbit_string_literal_49.data)[
        _M0L2loS312
      ];
      _M0L6bufferS313[_M0L6_2atmpS1819] = _M0L6_2atmpS1820;
      _M0L6_2atmpS1822 = _M0L6offsetS308 - 2;
      _M0L6_2atmpS1823 = _M0L1nS309 >> 8;
      _M0L6offsetS308 = _M0L6_2atmpS1822;
      _M0L1nS309 = _M0L6_2atmpS1823;
      continue;
    } else if (_M0L6offsetS308 == 1) {
      uint32_t _M0L6_2atmpS1826 = _M0L1nS309 & 15u;
      int32_t _M0L6nibbleS316 = *(int32_t*)&_M0L6_2atmpS1826;
      int32_t _M0L6_2atmpS1825 =
        ((moonbit_string_t)moonbit_string_literal_49.data)[_M0L6nibbleS316];
      _M0L6bufferS313[_M0L12digit__startS314] = _M0L6_2atmpS1825;
      moonbit_decref(_M0L6bufferS313);
    } else {
      moonbit_decref(_M0L6bufferS313);
    }
    break;
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS307) {
  struct _M0TWEOs* _M0L7_2afuncS306;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS306 = _M0L4selfS307;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS306->code(_M0L7_2afuncS306);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS305
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS304;
  struct _M0TPB6Logger _M0L6_2atmpS1815;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS304 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS304);
  _M0L6_2atmpS1815
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS304
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS305, _M0L6_2atmpS1815);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS304);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS303
) {
  int32_t _result_4203;
  #line 98 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _result_4203 = _M0L4selfS303.$1;
  moonbit_decref(_M0L4selfS303.$0);
  return _result_4203;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS302
) {
  #line 91 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS302.$0;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS298,
  moonbit_string_t _M0L5valueS299,
  int32_t _M0L5startS300,
  int32_t _M0L3lenS301
) {
  int32_t _M0L6_2atmpS1814;
  int64_t _M0L6_2atmpS1813;
  struct _M0TPC16string10StringView _M0L6_2atmpS1812;
  #line 102 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1814 = _M0L5startS300 + _M0L3lenS301;
  _M0L6_2atmpS1813 = (int64_t)_M0L6_2atmpS1814;
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1812
  = _M0MPC16string6String11sub_2einner(_M0L5valueS299, _M0L5startS300, _M0L6_2atmpS1813);
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS298, _M0L6_2atmpS1812);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS291,
  int32_t _M0L5startS297,
  int64_t _M0L3endS293
) {
  int32_t _M0L3lenS290;
  int32_t _M0L3endS292;
  int32_t _M0L5startS296;
  int32_t _if__result_4204;
  #line 653 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS290 = Moonbit_array_length(_M0L4selfS291);
  if (_M0L3endS293 == 4294967296ll) {
    _M0L3endS292 = _M0L3lenS290;
  } else {
    int64_t _M0L7_2aSomeS294 = _M0L3endS293;
    int32_t _M0L6_2aendS295 = (int32_t)_M0L7_2aSomeS294;
    if (_M0L6_2aendS295 < 0) {
      _M0L3endS292 = _M0L3lenS290 + _M0L6_2aendS295;
    } else {
      _M0L3endS292 = _M0L6_2aendS295;
    }
  }
  if (_M0L5startS297 < 0) {
    _M0L5startS296 = _M0L3lenS290 + _M0L5startS297;
  } else {
    _M0L5startS296 = _M0L5startS297;
  }
  if (_M0L5startS296 >= 0) {
    if (_M0L5startS296 <= _M0L3endS292) {
      _if__result_4204 = _M0L3endS292 <= _M0L3lenS290;
    } else {
      _if__result_4204 = 0;
    }
  } else {
    _if__result_4204 = 0;
  }
  if (_if__result_4204) {
    if (_M0L5startS296 < _M0L3lenS290) {
      int32_t _M0L6_2atmpS1809 = _M0L4selfS291[_M0L5startS296];
      int32_t _M0L6_2atmpS1808;
      #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1808
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1809);
      if (!_M0L6_2atmpS1808) {
        
      } else {
        #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS292 < _M0L3lenS290) {
      int32_t _M0L6_2atmpS1811 = _M0L4selfS291[_M0L3endS292];
      int32_t _M0L6_2atmpS1810;
      #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1810
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1811);
      if (!_M0L6_2atmpS1810) {
        
      } else {
        #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS296,
                                                 _M0L3endS292,
                                                 _M0L4selfS291};
  } else {
    moonbit_decref(_M0L4selfS291);
    #line 661 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS287) {
  struct _M0TPB6Hasher* _M0L1hS286;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS286 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS286);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS286, _M0L4selfS287);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS286);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS289
) {
  struct _M0TPB6Hasher* _M0L1hS288;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS288 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS288);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS288, _M0L4selfS289);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS288);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS284) {
  int32_t _M0L4seedS283;
  if (_M0L10seed_2eoptS284 == 4294967296ll) {
    _M0L4seedS283 = 0;
  } else {
    int64_t _M0L7_2aSomeS285 = _M0L10seed_2eoptS284;
    _M0L4seedS283 = (int32_t)_M0L7_2aSomeS285;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS283);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS282) {
  uint32_t _M0L6_2atmpS1807;
  uint32_t _M0L6_2atmpS1806;
  struct _M0TPB6Hasher* _block_4205;
  #line 75 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1807 = *(uint32_t*)&_M0L4seedS282;
  _M0L6_2atmpS1806 = _M0L6_2atmpS1807 + 374761393u;
  _block_4205
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4205)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4205->$0 = _M0L6_2atmpS1806;
  return _block_4205;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS281) {
  uint32_t _M0L6_2atmpS1805;
  #line 435 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1805 = _M0MPB6Hasher9avalanche(_M0L4selfS281);
  return *(int32_t*)&_M0L6_2atmpS1805;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS280) {
  uint32_t _M0Lm3accS279;
  uint32_t _M0L6_2atmpS1794;
  uint32_t _M0L6_2atmpS1796;
  uint32_t _M0L6_2atmpS1795;
  uint32_t _M0L6_2atmpS1797;
  uint32_t _M0L6_2atmpS1798;
  uint32_t _M0L6_2atmpS1800;
  uint32_t _M0L6_2atmpS1799;
  uint32_t _M0L6_2atmpS1801;
  uint32_t _M0L6_2atmpS1802;
  uint32_t _M0L6_2atmpS1804;
  uint32_t _M0L6_2atmpS1803;
  #line 440 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS279 = _M0L4selfS280->$0;
  moonbit_decref(_M0L4selfS280);
  _M0L6_2atmpS1794 = _M0Lm3accS279;
  _M0L6_2atmpS1796 = _M0Lm3accS279;
  _M0L6_2atmpS1795 = _M0L6_2atmpS1796 >> 15;
  _M0Lm3accS279 = _M0L6_2atmpS1794 ^ _M0L6_2atmpS1795;
  _M0L6_2atmpS1797 = _M0Lm3accS279;
  _M0Lm3accS279 = _M0L6_2atmpS1797 * 2246822519u;
  _M0L6_2atmpS1798 = _M0Lm3accS279;
  _M0L6_2atmpS1800 = _M0Lm3accS279;
  _M0L6_2atmpS1799 = _M0L6_2atmpS1800 >> 13;
  _M0Lm3accS279 = _M0L6_2atmpS1798 ^ _M0L6_2atmpS1799;
  _M0L6_2atmpS1801 = _M0Lm3accS279;
  _M0Lm3accS279 = _M0L6_2atmpS1801 * 3266489917u;
  _M0L6_2atmpS1802 = _M0Lm3accS279;
  _M0L6_2atmpS1804 = _M0Lm3accS279;
  _M0L6_2atmpS1803 = _M0L6_2atmpS1804 >> 16;
  _M0Lm3accS279 = _M0L6_2atmpS1802 ^ _M0L6_2atmpS1803;
  return _M0Lm3accS279;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS277,
  moonbit_string_t _M0L1yS278
) {
  int32_t _M0L6_2atmpS1793;
  #line 23 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 24 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1793 = moonbit_val_array_equal(_M0L1xS277, _M0L1yS278);
  moonbit_decref(_M0L1yS278);
  moonbit_decref(_M0L1xS277);
  return !_M0L6_2atmpS1793;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS274,
  int32_t _M0L5valueS273
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS273, _M0L4selfS274);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS276,
  moonbit_string_t _M0L5valueS275
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS275, _M0L4selfS276);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS271,
  int32_t _M0L5valueS272
) {
  uint32_t _M0L6_2atmpS1792;
  #line 187 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1792 = *(uint32_t*)&_M0L5valueS272;
  #line 188 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS271, _M0L6_2atmpS1792);
  return 0;
}

int32_t _M0MPC16uint166UInt1613is__surrogate(int32_t _M0L4selfS270) {
  #line 62 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS270 >= 55296) {
    return _M0L4selfS270 <= 57343;
  } else {
    return 0;
  }
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
    moonbit_string_t _M0L6_2atmpS1790;
    moonbit_string_t _M0L6_2atmpS1789;
    moonbit_string_t _M0L6_2atmpS1788;
    moonbit_string_t _M0L14expect__base64S268;
    moonbit_string_t _M0L6_2atmpS1787;
    moonbit_string_t _M0L6_2atmpS1786;
    moonbit_string_t _M0L6_2atmpS1785;
    moonbit_string_t _M0L14actual__base64S269;
    moonbit_string_t _M0L6_2atmpS1784;
    moonbit_string_t _M0L6_2atmpS1783;
    moonbit_string_t _M0L6_2atmpS1781;
    moonbit_string_t _M0L6_2atmpS1782;
    moonbit_string_t _M0L6_2atmpS1780;
    moonbit_string_t _M0L6_2atmpS1778;
    moonbit_string_t _M0L6_2atmpS1779;
    moonbit_string_t _M0L6_2atmpS1777;
    moonbit_string_t _M0L6_2atmpS1775;
    moonbit_string_t _M0L6_2atmpS1776;
    moonbit_string_t _M0L6_2atmpS1774;
    moonbit_string_t _M0L6_2atmpS1772;
    moonbit_string_t _M0L6_2atmpS1773;
    moonbit_string_t _M0L6_2atmpS1771;
    moonbit_string_t _M0L6_2atmpS1769;
    moonbit_string_t _M0L6_2atmpS1770;
    moonbit_string_t _M0L6_2atmpS1768;
    moonbit_string_t _M0L6_2atmpS1767;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1766;
    struct moonbit_result_0 _result_4206;
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
    _M0L6_2atmpS1790
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS261);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1789
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1790);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1788
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS1789);
    moonbit_decref(_M0L6_2atmpS1789);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14expect__base64S268
    = moonbit_add_string(_M0L6_2atmpS1788, (moonbit_string_t)moonbit_string_literal_50.data);
    moonbit_decref(_M0L6_2atmpS1788);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1787
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS259);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1786
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1787);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1785
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS1786);
    moonbit_decref(_M0L6_2atmpS1786);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14actual__base64S269
    = moonbit_add_string(_M0L6_2atmpS1785, (moonbit_string_t)moonbit_string_literal_50.data);
    moonbit_decref(_M0L6_2atmpS1785);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1784 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS262);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1783
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_51.data, _M0L6_2atmpS1784);
    moonbit_decref(_M0L6_2atmpS1784);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1781
    = moonbit_add_string(_M0L6_2atmpS1783, (moonbit_string_t)moonbit_string_literal_52.data);
    moonbit_decref(_M0L6_2atmpS1783);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1782
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS264);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1780 = moonbit_add_string(_M0L6_2atmpS1781, _M0L6_2atmpS1782);
    moonbit_decref(_M0L6_2atmpS1782);
    moonbit_decref(_M0L6_2atmpS1781);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1778
    = moonbit_add_string(_M0L6_2atmpS1780, (moonbit_string_t)moonbit_string_literal_53.data);
    moonbit_decref(_M0L6_2atmpS1780);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1779
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS266);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1777 = moonbit_add_string(_M0L6_2atmpS1778, _M0L6_2atmpS1779);
    moonbit_decref(_M0L6_2atmpS1779);
    moonbit_decref(_M0L6_2atmpS1778);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1775
    = moonbit_add_string(_M0L6_2atmpS1777, (moonbit_string_t)moonbit_string_literal_54.data);
    moonbit_decref(_M0L6_2atmpS1777);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1776
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS267);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1774 = moonbit_add_string(_M0L6_2atmpS1775, _M0L6_2atmpS1776);
    moonbit_decref(_M0L6_2atmpS1776);
    moonbit_decref(_M0L6_2atmpS1775);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1772
    = moonbit_add_string(_M0L6_2atmpS1774, (moonbit_string_t)moonbit_string_literal_55.data);
    moonbit_decref(_M0L6_2atmpS1774);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1773
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S268);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1771 = moonbit_add_string(_M0L6_2atmpS1772, _M0L6_2atmpS1773);
    moonbit_decref(_M0L6_2atmpS1773);
    moonbit_decref(_M0L6_2atmpS1772);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1769
    = moonbit_add_string(_M0L6_2atmpS1771, (moonbit_string_t)moonbit_string_literal_56.data);
    moonbit_decref(_M0L6_2atmpS1771);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1770
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S269);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1768 = moonbit_add_string(_M0L6_2atmpS1769, _M0L6_2atmpS1770);
    moonbit_decref(_M0L6_2atmpS1770);
    moonbit_decref(_M0L6_2atmpS1769);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1767
    = moonbit_add_string(_M0L6_2atmpS1768, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1768);
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1766
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1766)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1766)->$0
    = _M0L6_2atmpS1767;
    _result_4206.tag = 0;
    _result_4206.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1766;
    return _result_4206;
  } else {
    int32_t _M0L6_2atmpS1791;
    struct moonbit_result_0 _result_4207;
    moonbit_decref(_M0L9args__locS265);
    moonbit_decref(_M0L3locS263);
    moonbit_decref(_M0L7contentS261);
    moonbit_decref(_M0L6actualS259);
    _M0L6_2atmpS1791 = 0;
    _result_4207.tag = 1;
    _result_4207.data.ok = _M0L6_2atmpS1791;
    return _result_4207;
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
      moonbit_string_t* _M0L3bufS1765 = _M0L7_2aselfS251->$0;
      moonbit_string_t _M0L4itemS255 =
        (moonbit_string_t)_M0L3bufS1765[_M0L1iS254];
      int32_t _M0L6_2atmpS1764;
      if (_M0L1iS254 != 0) {
        if (_M0L4itemS255) {
          moonbit_incref(_M0L4itemS255);
        }
        moonbit_incref(_M0L3bufS250);
        #line 130 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, (moonbit_string_t)moonbit_string_literal_57.data);
      } else if (_M0L4itemS255) {
        moonbit_incref(_M0L4itemS255);
      }
      if (_M0L4itemS255 == 0) {
        if (_M0L4itemS255) {
          moonbit_decref(_M0L4itemS255);
        }
        moonbit_incref(_M0L3bufS250);
        #line 133 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, (moonbit_string_t)moonbit_string_literal_58.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS256 = _M0L4itemS255;
        moonbit_string_t _M0L6_2alocS257 = _M0L7_2aSomeS256;
        moonbit_string_t _M0L6_2atmpS1763;
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1763
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS257);
        moonbit_incref(_M0L3bufS250);
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, _M0L6_2atmpS1763);
      }
      _M0L6_2atmpS1764 = _M0L1iS254 + 1;
      _M0L1iS254 = _M0L6_2atmpS1764;
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
  moonbit_string_t _M0L6_2atmpS1762;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1761;
  #line 95 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1762 = _M0L4selfS249;
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1761 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1762);
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1761);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L2sbS247;
  struct _M0TPC16string10StringView _M0L8filenameS1747;
  struct _M0TPC16string10StringView _M0L11start__lineS1750;
  moonbit_string_t _M0L6_2atmpS1749;
  moonbit_string_t _M0L6_2atmpS1748;
  struct _M0TPC16string10StringView _M0L13start__columnS1753;
  moonbit_string_t _M0L6_2atmpS1752;
  moonbit_string_t _M0L6_2atmpS1751;
  struct _M0TPC16string10StringView _M0L9end__lineS1756;
  moonbit_string_t _M0L6_2atmpS1755;
  moonbit_string_t _M0L6_2atmpS1754;
  struct _M0TPC16string10StringView _M0L8_2afieldS3756;
  int32_t _M0L6_2acntS3965;
  struct _M0TPC16string10StringView _M0L11end__columnS1760;
  moonbit_string_t _M0L6_2atmpS1759;
  moonbit_string_t _M0L6_2atmpS1758;
  moonbit_string_t _M0L6_2atmpS1757;
  #line 82 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 83 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L2sbS247);
  #line 84 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, (moonbit_string_t)moonbit_string_literal_59.data);
  _M0L8filenameS1747
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$0_1, _M0L4selfS248->$0_2, _M0L4selfS248->$0_0
  };
  moonbit_incref(_M0L8filenameS1747.$0);
  moonbit_incref(_M0L2sbS247);
  #line 85 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS247, _M0L8filenameS1747);
  _M0L11start__lineS1750
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$1_1, _M0L4selfS248->$1_2, _M0L4selfS248->$1_0
  };
  moonbit_incref(_M0L11start__lineS1750.$0);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1749
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1750);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1748
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_60.data, _M0L6_2atmpS1749);
  moonbit_decref(_M0L6_2atmpS1749);
  moonbit_incref(_M0L2sbS247);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1748);
  _M0L13start__columnS1753
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$2_1, _M0L4selfS248->$2_2, _M0L4selfS248->$2_0
  };
  moonbit_incref(_M0L13start__columnS1753.$0);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1752
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1753);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1751
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_61.data, _M0L6_2atmpS1752);
  moonbit_decref(_M0L6_2atmpS1752);
  moonbit_incref(_M0L2sbS247);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1751);
  _M0L9end__lineS1756
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$3_1, _M0L4selfS248->$3_2, _M0L4selfS248->$3_0
  };
  moonbit_incref(_M0L9end__lineS1756.$0);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1755
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1756);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1754
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_62.data, _M0L6_2atmpS1755);
  moonbit_decref(_M0L6_2atmpS1755);
  moonbit_incref(_M0L2sbS247);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1754);
  _M0L8_2afieldS3756
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$4_1, _M0L4selfS248->$4_2, _M0L4selfS248->$4_0
  };
  _M0L6_2acntS3965 = Moonbit_object_header(_M0L4selfS248)->rc;
  if (_M0L6_2acntS3965 > 1) {
    int32_t _M0L11_2anew__cntS3970 = _M0L6_2acntS3965 - 1;
    Moonbit_object_header(_M0L4selfS248)->rc = _M0L11_2anew__cntS3970;
    moonbit_incref(_M0L8_2afieldS3756.$0);
  } else if (_M0L6_2acntS3965 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3969 =
      (struct _M0TPC16string10StringView){_M0L4selfS248->$3_1,
                                            _M0L4selfS248->$3_2,
                                            _M0L4selfS248->$3_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3968;
    struct _M0TPC16string10StringView _M0L8_2afieldS3967;
    struct _M0TPC16string10StringView _M0L8_2afieldS3966;
    moonbit_decref(_M0L8_2afieldS3969.$0);
    _M0L8_2afieldS3968
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$2_1, _M0L4selfS248->$2_2, _M0L4selfS248->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3968.$0);
    _M0L8_2afieldS3967
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$1_1, _M0L4selfS248->$1_2, _M0L4selfS248->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3967.$0);
    _M0L8_2afieldS3966
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$0_1, _M0L4selfS248->$0_2, _M0L4selfS248->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3966.$0);
    #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS248);
  }
  _M0L11end__columnS1760 = _M0L8_2afieldS3756;
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1759
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1760);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1758
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_63.data, _M0L6_2atmpS1759);
  moonbit_decref(_M0L6_2atmpS1759);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1757
  = moonbit_add_string(_M0L6_2atmpS1758, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1758);
  moonbit_incref(_M0L2sbS247);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1757);
  #line 90 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS247);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS246,
  moonbit_string_t _M0L3strS245
) {
  int32_t _M0L8str__lenS244;
  int32_t _M0L3lenS1742;
  int32_t _M0L6_2atmpS1741;
  uint16_t* _M0L4dataS1743;
  int32_t _M0L3lenS1744;
  int32_t _M0L3lenS1746;
  int32_t _M0L6_2atmpS1745;
  #line 81 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS244 = Moonbit_array_length(_M0L3strS245);
  _M0L3lenS1742 = _M0L4selfS246->$1;
  _M0L6_2atmpS1741 = _M0L3lenS1742 + _M0L8str__lenS244;
  moonbit_incref(_M0L4selfS246);
  #line 83 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS246, _M0L6_2atmpS1741);
  _M0L4dataS1743 = _M0L4selfS246->$0;
  _M0L3lenS1744 = _M0L4selfS246->$1;
  moonbit_incref(_M0L4dataS1743);
  #line 84 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1743, _M0L3lenS1744, _M0L3strS245, 0, _M0L8str__lenS244);
  _M0L3lenS1746 = _M0L4selfS246->$1;
  _M0L6_2atmpS1745 = _M0L3lenS1746 + _M0L8str__lenS244;
  _M0L4selfS246->$1 = _M0L6_2atmpS1745;
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
      int32_t _M0L6_2atmpS1738 = _M0L3strS241[_M0L1iS238];
      int32_t _M0L6_2atmpS1739;
      int32_t _M0L6_2atmpS1740;
      if (
        _M0L1jS239 < 0 || _M0L1jS239 >= Moonbit_array_length(_M0L4selfS240)
      ) {
        #line 75 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS240[_M0L1jS239] = _M0L6_2atmpS1738;
      _M0L6_2atmpS1739 = _M0L1iS238 + 1;
      _M0L6_2atmpS1740 = _M0L1jS239 + 1;
      _M0L1iS238 = _M0L6_2atmpS1739;
      _M0L1jS239 = _M0L6_2atmpS1740;
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
  struct _M0TPB6Logger _M0L6_2atmpS1737;
  #line 17 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1737
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS234
  };
  #line 21 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS233, _M0L6_2atmpS1737);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS178
) {
  int32_t _M0L6_2atmpS1736;
  struct _M0TPC16string10StringView _M0L7_2abindS177;
  moonbit_string_t _M0L7_2adataS179;
  int32_t _M0L8_2astartS180;
  int32_t _M0L6_2atmpS1735;
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
  int32_t _M0L6_2atmpS1694;
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1736 = Moonbit_array_length(_M0L4reprS178);
  _M0L7_2abindS177
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1736, _M0L4reprS178
  };
  moonbit_incref(_M0L7_2abindS177.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS179 = _M0MPC16string10StringView4data(_M0L7_2abindS177);
  moonbit_incref(_M0L7_2abindS177.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS180
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS177);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1735 = _M0MPC16string10StringView6length(_M0L7_2abindS177);
  _M0L6_2aendS181 = _M0L8_2astartS180 + _M0L6_2atmpS1735;
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
  _M0L6_2atmpS1694 = _M0Lm9_2acursorS182;
  if (_M0L6_2atmpS1694 < _M0L6_2aendS181) {
    int32_t _M0L6_2atmpS1695 = _M0Lm9_2acursorS182;
    int32_t _M0L12dispatch__15S205;
    _M0Lm9_2acursorS182 = _M0L6_2atmpS1695 + 1;
    _M0L12dispatch__15S205 = 0;
    loop__label__15_208:;
    while (1) {
      int32_t _M0L6_2atmpS1699;
      int32_t _M0L6_2atmpS1696;
      switch (_M0L12dispatch__15S205) {
        case 6: {
          int32_t _M0L6_2atmpS1702;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1702 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1702 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1704 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS213;
            int32_t _M0L6_2atmpS1703;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS213
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1704);
            _M0L6_2atmpS1703 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1703 + 1;
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
          int32_t _M0L6_2atmpS1705;
          _M0Lm9tag__0__2S192 = _M0Lm9tag__0__1S191;
          _M0Lm9tag__0__1S191 = _M0Lm6tag__0S190;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1705 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1705 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1710 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS215;
            int32_t _M0L6_2atmpS1706;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS215
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1710);
            _M0L6_2atmpS1706 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1706 + 1;
            if (_M0L10next__charS215 < 58) {
              if (_M0L10next__charS215 < 48) {
                goto join_214;
              } else {
                int32_t _M0L6_2atmpS1707;
                _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
                _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
                _M0Lm6tag__2S193 = _M0Lm9_2acursorS182;
                _M0L6_2atmpS1707 = _M0Lm9_2acursorS182;
                if (_M0L6_2atmpS1707 < _M0L6_2aendS181) {
                  int32_t _M0L6_2atmpS1709 = _M0Lm9_2acursorS182;
                  int32_t _M0L10next__charS217;
                  int32_t _M0L6_2atmpS1708;
                  moonbit_incref(_M0L7_2adataS179);
                  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS217
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1709);
                  _M0L6_2atmpS1708 = _M0Lm9_2acursorS182;
                  _M0Lm9_2acursorS182 = _M0L6_2atmpS1708 + 1;
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
          int32_t _M0L6_2atmpS1711;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
          _M0Lm6tag__2S193 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1711 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1711 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1713 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS219;
            int32_t _M0L6_2atmpS1712;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS219
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1713);
            _M0L6_2atmpS1712 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1712 + 1;
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
          int32_t _M0L6_2atmpS1714;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
          _M0Lm6tag__4S196 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1714 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1714 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1716 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS221;
            int32_t _M0L6_2atmpS1715;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS221
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1716);
            _M0L6_2atmpS1715 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1715 + 1;
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
          int32_t _M0L6_2atmpS1717;
          _M0Lm9tag__0__1S191 = _M0Lm6tag__0S190;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1717 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1717 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1719 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS223;
            int32_t _M0L6_2atmpS1718;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS223
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1719);
            _M0L6_2atmpS1718 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1718 + 1;
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
          int32_t _M0L6_2atmpS1720;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__3S197 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1720 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1720 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1728 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS225;
            int32_t _M0L6_2atmpS1721;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS225
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1728);
            _M0L6_2atmpS1721 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1721 + 1;
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
              int32_t _M0L6_2atmpS1722;
              _M0Lm9tag__0__2S192 = _M0Lm9tag__0__1S191;
              _M0Lm9tag__0__1S191 = _M0Lm6tag__0S190;
              _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
              _M0L6_2atmpS1722 = _M0Lm9_2acursorS182;
              if (_M0L6_2atmpS1722 < _M0L6_2aendS181) {
                int32_t _M0L6_2atmpS1727 = _M0Lm9_2acursorS182;
                int32_t _M0L10next__charS227;
                int32_t _M0L6_2atmpS1723;
                moonbit_incref(_M0L7_2adataS179);
                #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                _M0L10next__charS227
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1727);
                _M0L6_2atmpS1723 = _M0Lm9_2acursorS182;
                _M0Lm9_2acursorS182 = _M0L6_2atmpS1723 + 1;
                if (_M0L10next__charS227 < 58) {
                  if (_M0L10next__charS227 < 48) {
                    goto join_226;
                  } else {
                    int32_t _M0L6_2atmpS1724;
                    _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
                    _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
                    _M0Lm6tag__4S196 = _M0Lm9_2acursorS182;
                    _M0L6_2atmpS1724 = _M0Lm9_2acursorS182;
                    if (_M0L6_2atmpS1724 < _M0L6_2aendS181) {
                      int32_t _M0L6_2atmpS1726 = _M0Lm9_2acursorS182;
                      int32_t _M0L10next__charS229;
                      int32_t _M0L6_2atmpS1725;
                      moonbit_incref(_M0L7_2adataS179);
                      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS229
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1726);
                      _M0L6_2atmpS1725 = _M0Lm9_2acursorS182;
                      _M0Lm9_2acursorS182 = _M0L6_2atmpS1725 + 1;
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
          int32_t _M0L6_2atmpS1729;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0Lm6tag__1S194 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1729 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1729 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1731 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS231;
            int32_t _M0L6_2atmpS1730;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS231
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1731);
            _M0L6_2atmpS1730 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1730 + 1;
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
          int32_t _M0L6_2atmpS1732;
          _M0Lm6tag__0S190 = _M0Lm9_2acursorS182;
          _M0L6_2atmpS1732 = _M0Lm9_2acursorS182;
          if (_M0L6_2atmpS1732 < _M0L6_2aendS181) {
            int32_t _M0L6_2atmpS1734 = _M0Lm9_2acursorS182;
            int32_t _M0L10next__charS232;
            int32_t _M0L6_2atmpS1733;
            moonbit_incref(_M0L7_2adataS179);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS232
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1734);
            _M0L6_2atmpS1733 = _M0Lm9_2acursorS182;
            _M0Lm9_2acursorS182 = _M0L6_2atmpS1733 + 1;
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
      _M0L6_2atmpS1699 = _M0Lm9_2acursorS182;
      if (_M0L6_2atmpS1699 < _M0L6_2aendS181) {
        int32_t _M0L6_2atmpS1701 = _M0Lm9_2acursorS182;
        int32_t _M0L10next__charS212;
        int32_t _M0L6_2atmpS1700;
        moonbit_incref(_M0L7_2adataS179);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS212
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1701);
        _M0L6_2atmpS1700 = _M0Lm9_2acursorS182;
        _M0Lm9_2acursorS182 = _M0L6_2atmpS1700 + 1;
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
      _M0L6_2atmpS1696 = _M0Lm9_2acursorS182;
      if (_M0L6_2atmpS1696 < _M0L6_2aendS181) {
        int32_t _M0L6_2atmpS1698 = _M0Lm9_2acursorS182;
        int32_t _M0L10next__charS209;
        int32_t _M0L6_2atmpS1697;
        moonbit_incref(_M0L7_2adataS179);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS209
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS179, _M0L6_2atmpS1698);
        _M0L6_2atmpS1697 = _M0Lm9_2acursorS182;
        _M0Lm9_2acursorS182 = _M0L6_2atmpS1697 + 1;
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
      int32_t _M0L6_2atmpS1693 = _M0Lm20match__tag__saver__0S185;
      int32_t _M0L6_2atmpS1692 = _M0L6_2atmpS1693 + 1;
      int64_t _M0L6_2atmpS1689 = (int64_t)_M0L6_2atmpS1692;
      int32_t _M0L6_2atmpS1691 = _M0Lm20match__tag__saver__1S186;
      int64_t _M0L6_2atmpS1690 = (int64_t)_M0L6_2atmpS1691;
      struct _M0TPC16string10StringView _M0L11start__lineS199;
      int32_t _M0L6_2atmpS1688;
      int32_t _M0L6_2atmpS1687;
      int64_t _M0L6_2atmpS1684;
      int32_t _M0L6_2atmpS1686;
      int64_t _M0L6_2atmpS1685;
      struct _M0TPC16string10StringView _M0L13start__columnS200;
      int64_t _M0L6_2atmpS1681;
      int32_t _M0L6_2atmpS1683;
      int64_t _M0L6_2atmpS1682;
      struct _M0TPC16string10StringView _M0L8filenameS201;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L6_2atmpS1679;
      int64_t _M0L6_2atmpS1676;
      int32_t _M0L6_2atmpS1678;
      int64_t _M0L6_2atmpS1677;
      struct _M0TPC16string10StringView _M0L9end__lineS202;
      int32_t _M0L6_2atmpS1675;
      int32_t _M0L6_2atmpS1674;
      int64_t _M0L6_2atmpS1671;
      int32_t _M0L6_2atmpS1673;
      int64_t _M0L6_2atmpS1672;
      struct _M0TPC16string10StringView _M0L11end__columnS203;
      int32_t _M0L6_2atmpS1670;
      int32_t _M0L6_2atmpS1669;
      int64_t _M0L6_2atmpS1666;
      int32_t _M0L6_2atmpS1668;
      int64_t _M0L6_2atmpS1667;
      struct _M0TPC16string10StringView _M0L6_2atmpS3762;
      struct _M0TPB13SourceLocRepr* _block_4225;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS199
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1689, _M0L6_2atmpS1690);
      _M0L6_2atmpS1688 = _M0Lm20match__tag__saver__1S186;
      _M0L6_2atmpS1687 = _M0L6_2atmpS1688 + 1;
      _M0L6_2atmpS1684 = (int64_t)_M0L6_2atmpS1687;
      _M0L6_2atmpS1686 = _M0Lm20match__tag__saver__2S187;
      _M0L6_2atmpS1685 = (int64_t)_M0L6_2atmpS1686;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS200
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1684, _M0L6_2atmpS1685);
      _M0L6_2atmpS1681 = (int64_t)_M0L8_2astartS180;
      _M0L6_2atmpS1683 = _M0Lm20match__tag__saver__0S185;
      _M0L6_2atmpS1682 = (int64_t)_M0L6_2atmpS1683;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS201
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1681, _M0L6_2atmpS1682);
      _M0L6_2atmpS1680 = _M0Lm20match__tag__saver__2S187;
      _M0L6_2atmpS1679 = _M0L6_2atmpS1680 + 1;
      _M0L6_2atmpS1676 = (int64_t)_M0L6_2atmpS1679;
      _M0L6_2atmpS1678 = _M0Lm20match__tag__saver__3S188;
      _M0L6_2atmpS1677 = (int64_t)_M0L6_2atmpS1678;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS202
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1676, _M0L6_2atmpS1677);
      _M0L6_2atmpS1675 = _M0Lm20match__tag__saver__3S188;
      _M0L6_2atmpS1674 = _M0L6_2atmpS1675 + 1;
      _M0L6_2atmpS1671 = (int64_t)_M0L6_2atmpS1674;
      _M0L6_2atmpS1673 = _M0Lm20match__tag__saver__4S189;
      _M0L6_2atmpS1672 = (int64_t)_M0L6_2atmpS1673;
      moonbit_incref(_M0L7_2adataS179);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS203
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1671, _M0L6_2atmpS1672);
      _M0L6_2atmpS1670 = _M0Lm20match__tag__saver__4S189;
      _M0L6_2atmpS1669 = _M0L6_2atmpS1670 + 1;
      _M0L6_2atmpS1666 = (int64_t)_M0L6_2atmpS1669;
      _M0L6_2atmpS1668 = _M0Lm10match__endS184;
      _M0L6_2atmpS1667 = (int64_t)_M0L6_2atmpS1668;
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6_2atmpS3762
      = _M0MPC16string6String4view(_M0L7_2adataS179, _M0L6_2atmpS1666, _M0L6_2atmpS1667);
      moonbit_decref(_M0L6_2atmpS3762.$0);
      _block_4225
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4225)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 5, 0);
      _block_4225->$0_0 = _M0L8filenameS201.$0;
      _block_4225->$0_1 = _M0L8filenameS201.$1;
      _block_4225->$0_2 = _M0L8filenameS201.$2;
      _block_4225->$1_0 = _M0L11start__lineS199.$0;
      _block_4225->$1_1 = _M0L11start__lineS199.$1;
      _block_4225->$1_2 = _M0L11start__lineS199.$2;
      _block_4225->$2_0 = _M0L13start__columnS200.$0;
      _block_4225->$2_1 = _M0L13start__columnS200.$1;
      _block_4225->$2_2 = _M0L13start__columnS200.$2;
      _block_4225->$3_0 = _M0L9end__lineS202.$0;
      _block_4225->$3_1 = _M0L9end__lineS202.$1;
      _block_4225->$3_2 = _M0L9end__lineS202.$2;
      _block_4225->$4_0 = _M0L11end__columnS203.$0;
      _block_4225->$4_1 = _M0L11end__columnS203.$1;
      _block_4225->$4_2 = _M0L11end__columnS203.$2;
      return _block_4225;
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
  int32_t _M0L6_2atmpS1665;
  struct _M0TPC16string10StringView _M0L6_2atmpS1663;
  struct _M0TPB6Logger _M0L6_2atmpS1664;
  #line 145 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 146 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS174 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS1665 = Moonbit_array_length(_M0L4selfS175);
  _M0L6_2atmpS1663
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1665, _M0L4selfS175
  };
  moonbit_incref(_M0L3bufS174);
  _M0L6_2atmpS1664
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS174
  };
  #line 147 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1663, _M0L6_2atmpS1664, _M0L5quoteS176);
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
    int32_t _M0L6_2atmpS1647;
    int32_t _M0L6_2atmpS1648;
    int32_t _M0L6_2atmpS1649;
    int32_t _tmp_4229;
    int32_t _tmp_4230;
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
        int32_t _M0L6_2atmpS1650;
        int32_t _M0L6_2atmpS1651;
        moonbit_incref(_M0L6_2aenvS167);
        #line 207 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 208 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_64.data);
        _M0L6_2atmpS1650 = _M0L1iS168 + 1;
        _M0L6_2atmpS1651 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1650;
        _M0L3segS169 = _M0L6_2atmpS1651;
        goto _2afor_170;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1652;
        int32_t _M0L6_2atmpS1653;
        moonbit_incref(_M0L6_2aenvS167);
        #line 212 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 213 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_65.data);
        _M0L6_2atmpS1652 = _M0L1iS168 + 1;
        _M0L6_2atmpS1653 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1652;
        _M0L3segS169 = _M0L6_2atmpS1653;
        goto _2afor_170;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1654;
        int32_t _M0L6_2atmpS1655;
        moonbit_incref(_M0L6_2aenvS167);
        #line 217 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 218 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_66.data);
        _M0L6_2atmpS1654 = _M0L1iS168 + 1;
        _M0L6_2atmpS1655 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1654;
        _M0L3segS169 = _M0L6_2atmpS1655;
        goto _2afor_170;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1656;
        int32_t _M0L6_2atmpS1657;
        moonbit_incref(_M0L6_2aenvS167);
        #line 222 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
        if (_M0L6loggerS164.$1) {
          moonbit_incref(_M0L6loggerS164.$1);
        }
        #line 223 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_67.data);
        _M0L6_2atmpS1656 = _M0L1iS168 + 1;
        _M0L6_2atmpS1657 = _M0L1iS168 + 1;
        _M0L1iS168 = _M0L6_2atmpS1656;
        _M0L3segS169 = _M0L6_2atmpS1657;
        goto _2afor_170;
        break;
      }
      default: {
        if (_M0L4codeS171 < 32) {
          int32_t _M0L6_2atmpS1659;
          moonbit_string_t _M0L6_2atmpS1658;
          int32_t _M0L6_2atmpS1660;
          int32_t _M0L6_2atmpS1661;
          moonbit_incref(_M0L6_2aenvS167);
          #line 228 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS167, _M0L3segS169, _M0L1iS168);
          if (_M0L6loggerS164.$1) {
            moonbit_incref(_M0L6loggerS164.$1);
          }
          #line 229 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, (moonbit_string_t)moonbit_string_literal_68.data);
          _M0L6_2atmpS1659 = _M0L4codeS171 & 0xff;
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1658 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1659);
          if (_M0L6loggerS164.$1) {
            moonbit_incref(_M0L6loggerS164.$1);
          }
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS164.$0->$method_0(_M0L6loggerS164.$1, _M0L6_2atmpS1658);
          if (_M0L6loggerS164.$1) {
            moonbit_incref(_M0L6loggerS164.$1);
          }
          #line 231 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, 125);
          _M0L6_2atmpS1660 = _M0L1iS168 + 1;
          _M0L6_2atmpS1661 = _M0L1iS168 + 1;
          _M0L1iS168 = _M0L6_2atmpS1660;
          _M0L3segS169 = _M0L6_2atmpS1661;
          goto _2afor_170;
        } else {
          int32_t _M0L6_2atmpS1662 = _M0L1iS168 + 1;
          int32_t _tmp_4228 = _M0L3segS169;
          _M0L1iS168 = _M0L6_2atmpS1662;
          _M0L3segS169 = _tmp_4228;
          goto _2afor_170;
        }
        break;
      }
    }
    goto joinlet_4227;
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
    _M0L6_2atmpS1647 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS173);
    if (_M0L6loggerS164.$1) {
      moonbit_incref(_M0L6loggerS164.$1);
    }
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS164.$0->$method_3(_M0L6loggerS164.$1, _M0L6_2atmpS1647);
    _M0L6_2atmpS1648 = _M0L1iS168 + 1;
    _M0L6_2atmpS1649 = _M0L1iS168 + 1;
    _M0L1iS168 = _M0L6_2atmpS1648;
    _M0L3segS169 = _M0L6_2atmpS1649;
    continue;
    joinlet_4227:;
    _tmp_4229 = _M0L1iS168;
    _tmp_4230 = _M0L3segS169;
    _M0L1iS168 = _tmp_4229;
    _M0L3segS169 = _tmp_4230;
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
  struct _M0TPB6Logger _M0L8_2afieldS3763;
  int32_t _M0L6_2acntS3971;
  struct _M0TPB6Logger _M0L6loggerS160;
  #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L4selfS158
  = (struct _M0TPC16string10StringView){
    _M0L6_2aenvS159->$1_1, _M0L6_2aenvS159->$1_2, _M0L6_2aenvS159->$1_0
  };
  _M0L8_2afieldS3763
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS159->$0_0, _M0L6_2aenvS159->$0_1
  };
  _M0L6_2acntS3971 = Moonbit_object_header(_M0L6_2aenvS159)->rc;
  if (_M0L6_2acntS3971 > 1) {
    int32_t _M0L11_2anew__cntS3972 = _M0L6_2acntS3971 - 1;
    Moonbit_object_header(_M0L6_2aenvS159)->rc = _M0L11_2anew__cntS3972;
    moonbit_incref(_M0L4selfS158.$0);
    if (_M0L8_2afieldS3763.$1) {
      moonbit_incref(_M0L8_2afieldS3763.$1);
    }
  } else if (_M0L6_2acntS3971 == 1) {
    #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS159);
  }
  _M0L6loggerS160 = _M0L8_2afieldS3763;
  if (_M0L1iS161 > _M0L3segS162) {
    int64_t _M0L6_2atmpS1646 = (int64_t)_M0L1iS161;
    struct _M0TPC16string10StringView _M0L6_2atmpS1645;
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1645
    = _M0MPC16string10StringView11sub_2einner(_M0L4selfS158, _M0L3segS162, _M0L6_2atmpS1646);
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS160.$0->$method_2(_M0L6loggerS160.$1, _M0L6_2atmpS1645);
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
  moonbit_string_t _M0L3strS1642;
  int32_t _M0L5startS1644;
  int32_t _M0L6_2atmpS1643;
  int32_t _result_4231;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1642 = _M0L4selfS156.$0;
  _M0L5startS1644 = _M0L4selfS156.$1;
  _M0L6_2atmpS1643 = _M0L5startS1644 + _M0L5indexS157;
  _result_4231 = _M0L3strS1642[_M0L6_2atmpS1643];
  moonbit_decref(_M0L3strS1642);
  return _result_4231;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView _M0L4selfS149,
  int32_t _M0L5startS155,
  int64_t _M0L3endS151
) {
  moonbit_string_t _M0L3strS1641;
  int32_t _M0L8str__lenS148;
  int32_t _M0L8abs__endS150;
  int32_t _M0L10abs__startS154;
  int32_t _M0L5startS1629;
  int32_t _if__result_4232;
  #line 712 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1641 = _M0L4selfS149.$0;
  _M0L8str__lenS148 = Moonbit_array_length(_M0L3strS1641);
  if (_M0L3endS151 == 4294967296ll) {
    _M0L8abs__endS150 = _M0L4selfS149.$2;
  } else {
    int64_t _M0L7_2aSomeS152 = _M0L3endS151;
    int32_t _M0L6_2aendS153 = (int32_t)_M0L7_2aSomeS152;
    if (_M0L6_2aendS153 < 0) {
      int32_t _M0L3endS1639 = _M0L4selfS149.$2;
      _M0L8abs__endS150 = _M0L3endS1639 + _M0L6_2aendS153;
    } else {
      int32_t _M0L5startS1640 = _M0L4selfS149.$1;
      _M0L8abs__endS150 = _M0L5startS1640 + _M0L6_2aendS153;
    }
  }
  if (_M0L5startS155 < 0) {
    int32_t _M0L3endS1637 = _M0L4selfS149.$2;
    _M0L10abs__startS154 = _M0L3endS1637 + _M0L5startS155;
  } else {
    int32_t _M0L5startS1638 = _M0L4selfS149.$1;
    _M0L10abs__startS154 = _M0L5startS1638 + _M0L5startS155;
  }
  _M0L5startS1629 = _M0L4selfS149.$1;
  if (_M0L10abs__startS154 >= _M0L5startS1629) {
    if (_M0L10abs__startS154 <= _M0L8abs__endS150) {
      int32_t _M0L3endS1628 = _M0L4selfS149.$2;
      _if__result_4232 = _M0L8abs__endS150 <= _M0L3endS1628;
    } else {
      _if__result_4232 = 0;
    }
  } else {
    _if__result_4232 = 0;
  }
  if (_if__result_4232) {
    moonbit_string_t _M0L3strS1636;
    if (_M0L10abs__startS154 < _M0L8str__lenS148) {
      moonbit_string_t _M0L3strS1632 = _M0L4selfS149.$0;
      int32_t _M0L6_2atmpS1631 = _M0L3strS1632[_M0L10abs__startS154];
      int32_t _M0L6_2atmpS1630;
      #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1630
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1631);
      if (!_M0L6_2atmpS1630) {
        
      } else {
        #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L8abs__endS150 < _M0L8str__lenS148) {
      moonbit_string_t _M0L3strS1635 = _M0L4selfS149.$0;
      int32_t _M0L6_2atmpS1634 = _M0L3strS1635[_M0L8abs__endS150];
      int32_t _M0L6_2atmpS1633;
      #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1633
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1634);
      if (!_M0L6_2atmpS1633) {
        
      } else {
        #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    _M0L3strS1636 = _M0L4selfS149.$0;
    return (struct _M0TPC16string10StringView){_M0L10abs__startS154,
                                                 _M0L8abs__endS150,
                                                 _M0L3strS1636};
  } else {
    moonbit_decref(_M0L4selfS149.$0);
    #line 732 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS147
) {
  int32_t _M0L3endS1626;
  int32_t _M0L5startS1627;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1626 = _M0L4selfS147.$2;
  _M0L5startS1627 = _M0L4selfS147.$1;
  moonbit_decref(_M0L4selfS147.$0);
  return _M0L3endS1626 - _M0L5startS1627;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS146) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS145;
  int32_t _M0L6_2atmpS1623;
  int32_t _M0L6_2atmpS1622;
  int32_t _M0L6_2atmpS1625;
  int32_t _M0L6_2atmpS1624;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1621;
  #line 109 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS145 = _M0MPB13StringBuilder11new_2einner(0);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1623 = _M0IPC14byte4BytePB3Div3div(_M0L1bS146, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1622
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1623);
  moonbit_incref(_M0L7_2aselfS145);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS145, _M0L6_2atmpS1622);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1625 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS146, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1624
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1625);
  moonbit_incref(_M0L7_2aselfS145);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS145, _M0L6_2atmpS1624);
  _M0L6_2atmpS1621 = _M0L7_2aselfS145;
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1621);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t _M0L1iS144) {
  #line 110 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS144 < 10) {
    int32_t _M0L6_2atmpS1618;
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1618 = _M0IPC14byte4BytePB3Add3add(_M0L1iS144, 48);
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1618);
  } else {
    int32_t _M0L6_2atmpS1620;
    int32_t _M0L6_2atmpS1619;
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1620 = _M0IPC14byte4BytePB3Add3add(_M0L1iS144, 97);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1619 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1620, 10);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1619);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS142,
  int32_t _M0L4thatS143
) {
  int32_t _M0L6_2atmpS1616;
  int32_t _M0L6_2atmpS1617;
  int32_t _M0L6_2atmpS1615;
  #line 120 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1616 = (int32_t)_M0L4selfS142;
  _M0L6_2atmpS1617 = (int32_t)_M0L4thatS143;
  _M0L6_2atmpS1615 = _M0L6_2atmpS1616 - _M0L6_2atmpS1617;
  return _M0L6_2atmpS1615 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS140,
  int32_t _M0L4thatS141
) {
  int32_t _M0L6_2atmpS1613;
  int32_t _M0L6_2atmpS1614;
  int32_t _M0L6_2atmpS1612;
  #line 67 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1613 = (int32_t)_M0L4selfS140;
  _M0L6_2atmpS1614 = (int32_t)_M0L4thatS141;
  _M0L6_2atmpS1612 = _M0L6_2atmpS1613 % _M0L6_2atmpS1614;
  return _M0L6_2atmpS1612 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS138,
  int32_t _M0L4thatS139
) {
  int32_t _M0L6_2atmpS1610;
  int32_t _M0L6_2atmpS1611;
  int32_t _M0L6_2atmpS1609;
  #line 62 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1610 = (int32_t)_M0L4selfS138;
  _M0L6_2atmpS1611 = (int32_t)_M0L4thatS139;
  _M0L6_2atmpS1609 = _M0L6_2atmpS1610 / _M0L6_2atmpS1611;
  return _M0L6_2atmpS1609 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS136,
  int32_t _M0L4thatS137
) {
  int32_t _M0L6_2atmpS1607;
  int32_t _M0L6_2atmpS1608;
  int32_t _M0L6_2atmpS1606;
  #line 106 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1607 = (int32_t)_M0L4selfS136;
  _M0L6_2atmpS1608 = (int32_t)_M0L4thatS137;
  _M0L6_2atmpS1606 = _M0L6_2atmpS1607 + _M0L6_2atmpS1608;
  return _M0L6_2atmpS1606 & 0xff;
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS130
) {
  int32_t _M0L17codepoint__lengthS129;
  int32_t _M0L6_2atmpS1605;
  moonbit_bytes_t _M0L4dataS131;
  int32_t _M0L1iS132;
  int32_t _M0L12utf16__indexS133;
  #line 102 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS130);
  #line 104 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS129
  = _M0MPC16string6String20char__length_2einner(_M0L1sS130, 0, 4294967296ll);
  _M0L6_2atmpS1605 = _M0L17codepoint__lengthS129 * 4;
  _M0L4dataS131 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1605, 0);
  _M0L1iS132 = 0;
  _M0L12utf16__indexS133 = 0;
  while (1) {
    if (_M0L1iS132 < _M0L17codepoint__lengthS129) {
      int32_t _M0L6_2atmpS1602;
      int32_t _M0L1cS134;
      int32_t _M0L6_2atmpS1603;
      int32_t _M0L6_2atmpS1604;
      moonbit_incref(_M0L1sS130);
      #line 109 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1602
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS130, _M0L12utf16__indexS133);
      _M0L1cS134 = _M0L6_2atmpS1602;
      if (_M0L1cS134 > 65535) {
        int32_t _M0L6_2atmpS1570 = _M0L1iS132 * 4;
        int32_t _M0L6_2atmpS1572 = _M0L1cS134 & 255;
        int32_t _M0L6_2atmpS1571 = _M0L6_2atmpS1572 & 0xff;
        int32_t _M0L6_2atmpS1577;
        int32_t _M0L6_2atmpS1573;
        int32_t _M0L6_2atmpS1576;
        int32_t _M0L6_2atmpS1575;
        int32_t _M0L6_2atmpS1574;
        int32_t _M0L6_2atmpS1582;
        int32_t _M0L6_2atmpS1578;
        int32_t _M0L6_2atmpS1581;
        int32_t _M0L6_2atmpS1580;
        int32_t _M0L6_2atmpS1579;
        int32_t _M0L6_2atmpS1587;
        int32_t _M0L6_2atmpS1583;
        int32_t _M0L6_2atmpS1586;
        int32_t _M0L6_2atmpS1585;
        int32_t _M0L6_2atmpS1584;
        int32_t _M0L6_2atmpS1588;
        int32_t _M0L6_2atmpS1589;
        if (
          _M0L6_2atmpS1570 < 0
          || _M0L6_2atmpS1570 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 111 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1570] = _M0L6_2atmpS1571;
        _M0L6_2atmpS1577 = _M0L1iS132 * 4;
        _M0L6_2atmpS1573 = _M0L6_2atmpS1577 + 1;
        _M0L6_2atmpS1576 = _M0L1cS134 >> 8;
        _M0L6_2atmpS1575 = _M0L6_2atmpS1576 & 255;
        _M0L6_2atmpS1574 = _M0L6_2atmpS1575 & 0xff;
        if (
          _M0L6_2atmpS1573 < 0
          || _M0L6_2atmpS1573 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 112 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1573] = _M0L6_2atmpS1574;
        _M0L6_2atmpS1582 = _M0L1iS132 * 4;
        _M0L6_2atmpS1578 = _M0L6_2atmpS1582 + 2;
        _M0L6_2atmpS1581 = _M0L1cS134 >> 16;
        _M0L6_2atmpS1580 = _M0L6_2atmpS1581 & 255;
        _M0L6_2atmpS1579 = _M0L6_2atmpS1580 & 0xff;
        if (
          _M0L6_2atmpS1578 < 0
          || _M0L6_2atmpS1578 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 113 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1578] = _M0L6_2atmpS1579;
        _M0L6_2atmpS1587 = _M0L1iS132 * 4;
        _M0L6_2atmpS1583 = _M0L6_2atmpS1587 + 3;
        _M0L6_2atmpS1586 = _M0L1cS134 >> 24;
        _M0L6_2atmpS1585 = _M0L6_2atmpS1586 & 255;
        _M0L6_2atmpS1584 = _M0L6_2atmpS1585 & 0xff;
        if (
          _M0L6_2atmpS1583 < 0
          || _M0L6_2atmpS1583 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 114 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1583] = _M0L6_2atmpS1584;
        _M0L6_2atmpS1588 = _M0L1iS132 + 1;
        _M0L6_2atmpS1589 = _M0L12utf16__indexS133 + 2;
        _M0L1iS132 = _M0L6_2atmpS1588;
        _M0L12utf16__indexS133 = _M0L6_2atmpS1589;
        continue;
      } else {
        int32_t _M0L6_2atmpS1590 = _M0L1iS132 * 4;
        int32_t _M0L6_2atmpS1592 = _M0L1cS134 & 255;
        int32_t _M0L6_2atmpS1591 = _M0L6_2atmpS1592 & 0xff;
        int32_t _M0L6_2atmpS1597;
        int32_t _M0L6_2atmpS1593;
        int32_t _M0L6_2atmpS1596;
        int32_t _M0L6_2atmpS1595;
        int32_t _M0L6_2atmpS1594;
        int32_t _M0L6_2atmpS1599;
        int32_t _M0L6_2atmpS1598;
        int32_t _M0L6_2atmpS1601;
        int32_t _M0L6_2atmpS1600;
        if (
          _M0L6_2atmpS1590 < 0
          || _M0L6_2atmpS1590 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 117 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1590] = _M0L6_2atmpS1591;
        _M0L6_2atmpS1597 = _M0L1iS132 * 4;
        _M0L6_2atmpS1593 = _M0L6_2atmpS1597 + 1;
        _M0L6_2atmpS1596 = _M0L1cS134 >> 8;
        _M0L6_2atmpS1595 = _M0L6_2atmpS1596 & 255;
        _M0L6_2atmpS1594 = _M0L6_2atmpS1595 & 0xff;
        if (
          _M0L6_2atmpS1593 < 0
          || _M0L6_2atmpS1593 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 118 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1593] = _M0L6_2atmpS1594;
        _M0L6_2atmpS1599 = _M0L1iS132 * 4;
        _M0L6_2atmpS1598 = _M0L6_2atmpS1599 + 2;
        if (
          _M0L6_2atmpS1598 < 0
          || _M0L6_2atmpS1598 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 119 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1598] = 0;
        _M0L6_2atmpS1601 = _M0L1iS132 * 4;
        _M0L6_2atmpS1600 = _M0L6_2atmpS1601 + 3;
        if (
          _M0L6_2atmpS1600 < 0
          || _M0L6_2atmpS1600 >= Moonbit_array_length(_M0L4dataS131)
        ) {
          #line 120 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS131[_M0L6_2atmpS1600] = 0;
      }
      _M0L6_2atmpS1603 = _M0L1iS132 + 1;
      _M0L6_2atmpS1604 = _M0L12utf16__indexS133 + 1;
      _M0L1iS132 = _M0L6_2atmpS1603;
      _M0L12utf16__indexS133 = _M0L6_2atmpS1604;
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
    int32_t _M0L6_2atmpS1569 = _M0L5indexS127 + 1;
    int32_t _M0L2c2S128 = _M0L4selfS126[_M0L6_2atmpS1569];
    int32_t _M0L6_2atmpS1567;
    int32_t _M0L6_2atmpS1568;
    moonbit_decref(_M0L4selfS126);
    _M0L6_2atmpS1567 = (int32_t)_M0L2c1S125;
    _M0L6_2atmpS1568 = (int32_t)_M0L2c2S128;
    #line 96 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1567, _M0L6_2atmpS1568);
  } else {
    moonbit_decref(_M0L4selfS126);
    #line 98 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S125);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS124) {
  int32_t _M0L6_2atmpS1566;
  #line 68 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS1566 = (int32_t)_M0L4selfS124;
  return _M0L6_2atmpS1566;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS122,
  int32_t _M0L8trailingS123
) {
  int32_t _M0L6_2atmpS1565;
  int32_t _M0L6_2atmpS1564;
  int32_t _M0L6_2atmpS1563;
  int32_t _M0L6_2atmpS1562;
  int32_t _M0L6_2atmpS1561;
  #line 40 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1565 = _M0L7leadingS122 - 55296;
  _M0L6_2atmpS1564 = _M0L6_2atmpS1565 * 1024;
  _M0L6_2atmpS1563 = _M0L6_2atmpS1564 + _M0L8trailingS123;
  _M0L6_2atmpS1562 = _M0L6_2atmpS1563 - 56320;
  _M0L6_2atmpS1561 = _M0L6_2atmpS1562 + 65536;
  return _M0L6_2atmpS1561;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS115,
  int32_t _M0L13start__offsetS116,
  int64_t _M0L11end__offsetS113
) {
  int32_t _M0L11end__offsetS112;
  int32_t _if__result_4234;
  #line 60 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS113 == 4294967296ll) {
    _M0L11end__offsetS112 = Moonbit_array_length(_M0L4selfS115);
  } else {
    int64_t _M0L7_2aSomeS114 = _M0L11end__offsetS113;
    _M0L11end__offsetS112 = (int32_t)_M0L7_2aSomeS114;
  }
  if (_M0L13start__offsetS116 >= 0) {
    if (_M0L13start__offsetS116 <= _M0L11end__offsetS112) {
      int32_t _M0L6_2atmpS1554 = Moonbit_array_length(_M0L4selfS115);
      _if__result_4234 = _M0L11end__offsetS112 <= _M0L6_2atmpS1554;
    } else {
      _if__result_4234 = 0;
    }
  } else {
    _if__result_4234 = 0;
  }
  if (_if__result_4234) {
    int32_t _M0L12utf16__indexS117 = _M0L13start__offsetS116;
    int32_t _M0L11char__countS118 = 0;
    while (1) {
      if (_M0L12utf16__indexS117 < _M0L11end__offsetS112) {
        int32_t _M0L2c1S119 = _M0L4selfS115[_M0L12utf16__indexS117];
        int32_t _if__result_4236;
        int32_t _M0L6_2atmpS1559;
        int32_t _M0L6_2atmpS1560;
        #line 76 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S119)) {
          int32_t _M0L6_2atmpS1555 = _M0L12utf16__indexS117 + 1;
          _if__result_4236 = _M0L6_2atmpS1555 < _M0L11end__offsetS112;
        } else {
          _if__result_4236 = 0;
        }
        if (_if__result_4236) {
          int32_t _M0L6_2atmpS1558 = _M0L12utf16__indexS117 + 1;
          int32_t _M0L2c2S120 = _M0L4selfS115[_M0L6_2atmpS1558];
          #line 78 "/Users/user/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S120)) {
            int32_t _M0L6_2atmpS1556 = _M0L12utf16__indexS117 + 2;
            int32_t _M0L6_2atmpS1557 = _M0L11char__countS118 + 1;
            _M0L12utf16__indexS117 = _M0L6_2atmpS1556;
            _M0L11char__countS118 = _M0L6_2atmpS1557;
            continue;
          } else {
            #line 81 "/Users/user/.moon/lib/core/builtin/string.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_44.data);
          }
        }
        _M0L6_2atmpS1559 = _M0L12utf16__indexS117 + 1;
        _M0L6_2atmpS1560 = _M0L11char__countS118 + 1;
        _M0L12utf16__indexS117 = _M0L6_2atmpS1559;
        _M0L11char__countS118 = _M0L6_2atmpS1560;
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
    return _M0FPC15abort5abortGiE((moonbit_string_t)moonbit_string_literal_69.data);
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
    int32_t _M0L6_2atmpS1506 = _M0L3lenS90 - _M0L3remS92;
    if (_M0L1iS93 < _M0L6_2atmpS1506) {
      int32_t _M0L6_2atmpS1528;
      int32_t _M0L2b0S94;
      int32_t _M0L6_2atmpS1527;
      int32_t _M0L6_2atmpS1526;
      int32_t _M0L2b1S95;
      int32_t _M0L6_2atmpS1525;
      int32_t _M0L6_2atmpS1524;
      int32_t _M0L2b2S96;
      int32_t _M0L6_2atmpS1523;
      int32_t _M0L6_2atmpS1522;
      int32_t _M0L2x0S97;
      int32_t _M0L6_2atmpS1521;
      int32_t _M0L6_2atmpS1518;
      int32_t _M0L6_2atmpS1520;
      int32_t _M0L6_2atmpS1519;
      int32_t _M0L6_2atmpS1517;
      int32_t _M0L2x1S98;
      int32_t _M0L6_2atmpS1516;
      int32_t _M0L6_2atmpS1513;
      int32_t _M0L6_2atmpS1515;
      int32_t _M0L6_2atmpS1514;
      int32_t _M0L6_2atmpS1512;
      int32_t _M0L2x2S99;
      int32_t _M0L6_2atmpS1511;
      int32_t _M0L2x3S100;
      int32_t _M0L6_2atmpS1507;
      int32_t _M0L6_2atmpS1508;
      int32_t _M0L6_2atmpS1509;
      int32_t _M0L6_2atmpS1510;
      int32_t _M0L6_2atmpS1529;
      if (_M0L1iS93 < 0 || _M0L1iS93 >= Moonbit_array_length(_M0L4dataS91)) {
        #line 67 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1528 = (int32_t)_M0L4dataS91[_M0L1iS93];
      _M0L2b0S94 = (int32_t)_M0L6_2atmpS1528;
      _M0L6_2atmpS1527 = _M0L1iS93 + 1;
      if (
        _M0L6_2atmpS1527 < 0
        || _M0L6_2atmpS1527 >= Moonbit_array_length(_M0L4dataS91)
      ) {
        #line 68 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1526 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1527];
      _M0L2b1S95 = (int32_t)_M0L6_2atmpS1526;
      _M0L6_2atmpS1525 = _M0L1iS93 + 2;
      if (
        _M0L6_2atmpS1525 < 0
        || _M0L6_2atmpS1525 >= Moonbit_array_length(_M0L4dataS91)
      ) {
        #line 69 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1524 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1525];
      _M0L2b2S96 = (int32_t)_M0L6_2atmpS1524;
      _M0L6_2atmpS1523 = _M0L2b0S94 & 252;
      _M0L6_2atmpS1522 = _M0L6_2atmpS1523 >> 2;
      if (
        _M0L6_2atmpS1522 < 0
        || _M0L6_2atmpS1522
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 70 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S97 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1522];
      _M0L6_2atmpS1521 = _M0L2b0S94 & 3;
      _M0L6_2atmpS1518 = _M0L6_2atmpS1521 << 4;
      _M0L6_2atmpS1520 = _M0L2b1S95 & 240;
      _M0L6_2atmpS1519 = _M0L6_2atmpS1520 >> 4;
      _M0L6_2atmpS1517 = _M0L6_2atmpS1518 | _M0L6_2atmpS1519;
      if (
        _M0L6_2atmpS1517 < 0
        || _M0L6_2atmpS1517
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 71 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S98 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1517];
      _M0L6_2atmpS1516 = _M0L2b1S95 & 15;
      _M0L6_2atmpS1513 = _M0L6_2atmpS1516 << 2;
      _M0L6_2atmpS1515 = _M0L2b2S96 & 192;
      _M0L6_2atmpS1514 = _M0L6_2atmpS1515 >> 6;
      _M0L6_2atmpS1512 = _M0L6_2atmpS1513 | _M0L6_2atmpS1514;
      if (
        _M0L6_2atmpS1512 < 0
        || _M0L6_2atmpS1512
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 72 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S99 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1512];
      _M0L6_2atmpS1511 = _M0L2b2S96 & 63;
      if (
        _M0L6_2atmpS1511 < 0
        || _M0L6_2atmpS1511
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 73 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S100 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1511];
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1507 = _M0MPC14byte4Byte8to__char(_M0L2x0S97);
      moonbit_incref(_M0L3bufS89);
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1507);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1508 = _M0MPC14byte4Byte8to__char(_M0L2x1S98);
      moonbit_incref(_M0L3bufS89);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1508);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1509 = _M0MPC14byte4Byte8to__char(_M0L2x2S99);
      moonbit_incref(_M0L3bufS89);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1509);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1510 = _M0MPC14byte4Byte8to__char(_M0L2x3S100);
      moonbit_incref(_M0L3bufS89);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1510);
      _M0L6_2atmpS1529 = _M0L1iS93 + 3;
      _M0L1iS93 = _M0L6_2atmpS1529;
      continue;
    }
    break;
  }
  if (_M0L3remS92 == 1) {
    int32_t _M0L6_2atmpS1537 = _M0L3lenS90 - 1;
    int32_t _M0L6_2atmpS1536;
    int32_t _M0L2b0S102;
    int32_t _M0L6_2atmpS1535;
    int32_t _M0L6_2atmpS1534;
    int32_t _M0L2x0S103;
    int32_t _M0L6_2atmpS1533;
    int32_t _M0L6_2atmpS1532;
    int32_t _M0L2x1S104;
    int32_t _M0L6_2atmpS1530;
    int32_t _M0L6_2atmpS1531;
    if (
      _M0L6_2atmpS1537 < 0
      || _M0L6_2atmpS1537 >= Moonbit_array_length(_M0L4dataS91)
    ) {
      #line 80 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1536 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1537];
    moonbit_decref(_M0L4dataS91);
    _M0L2b0S102 = (int32_t)_M0L6_2atmpS1536;
    _M0L6_2atmpS1535 = _M0L2b0S102 & 252;
    _M0L6_2atmpS1534 = _M0L6_2atmpS1535 >> 2;
    if (
      _M0L6_2atmpS1534 < 0
      || _M0L6_2atmpS1534
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 81 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S103 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1534];
    _M0L6_2atmpS1533 = _M0L2b0S102 & 3;
    _M0L6_2atmpS1532 = _M0L6_2atmpS1533 << 4;
    if (
      _M0L6_2atmpS1532 < 0
      || _M0L6_2atmpS1532
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 82 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S104 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1532];
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1530 = _M0MPC14byte4Byte8to__char(_M0L2x0S103);
    moonbit_incref(_M0L3bufS89);
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1530);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1531 = _M0MPC14byte4Byte8to__char(_M0L2x1S104);
    moonbit_incref(_M0L3bufS89);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1531);
    moonbit_incref(_M0L3bufS89);
    #line 85 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, 61);
    moonbit_incref(_M0L3bufS89);
    #line 86 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, 61);
  } else if (_M0L3remS92 == 2) {
    int32_t _M0L6_2atmpS1553 = _M0L3lenS90 - 2;
    int32_t _M0L6_2atmpS1552;
    int32_t _M0L2b0S105;
    int32_t _M0L6_2atmpS1551;
    int32_t _M0L6_2atmpS1550;
    int32_t _M0L2b1S106;
    int32_t _M0L6_2atmpS1549;
    int32_t _M0L6_2atmpS1548;
    int32_t _M0L2x0S107;
    int32_t _M0L6_2atmpS1547;
    int32_t _M0L6_2atmpS1544;
    int32_t _M0L6_2atmpS1546;
    int32_t _M0L6_2atmpS1545;
    int32_t _M0L6_2atmpS1543;
    int32_t _M0L2x1S108;
    int32_t _M0L6_2atmpS1542;
    int32_t _M0L6_2atmpS1541;
    int32_t _M0L2x2S109;
    int32_t _M0L6_2atmpS1538;
    int32_t _M0L6_2atmpS1539;
    int32_t _M0L6_2atmpS1540;
    if (
      _M0L6_2atmpS1553 < 0
      || _M0L6_2atmpS1553 >= Moonbit_array_length(_M0L4dataS91)
    ) {
      #line 88 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1552 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1553];
    _M0L2b0S105 = (int32_t)_M0L6_2atmpS1552;
    _M0L6_2atmpS1551 = _M0L3lenS90 - 1;
    if (
      _M0L6_2atmpS1551 < 0
      || _M0L6_2atmpS1551 >= Moonbit_array_length(_M0L4dataS91)
    ) {
      #line 89 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1550 = (int32_t)_M0L4dataS91[_M0L6_2atmpS1551];
    moonbit_decref(_M0L4dataS91);
    _M0L2b1S106 = (int32_t)_M0L6_2atmpS1550;
    _M0L6_2atmpS1549 = _M0L2b0S105 & 252;
    _M0L6_2atmpS1548 = _M0L6_2atmpS1549 >> 2;
    if (
      _M0L6_2atmpS1548 < 0
      || _M0L6_2atmpS1548
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 90 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S107 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1548];
    _M0L6_2atmpS1547 = _M0L2b0S105 & 3;
    _M0L6_2atmpS1544 = _M0L6_2atmpS1547 << 4;
    _M0L6_2atmpS1546 = _M0L2b1S106 & 240;
    _M0L6_2atmpS1545 = _M0L6_2atmpS1546 >> 4;
    _M0L6_2atmpS1543 = _M0L6_2atmpS1544 | _M0L6_2atmpS1545;
    if (
      _M0L6_2atmpS1543 < 0
      || _M0L6_2atmpS1543
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 91 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S108 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1543];
    _M0L6_2atmpS1542 = _M0L2b1S106 & 15;
    _M0L6_2atmpS1541 = _M0L6_2atmpS1542 << 2;
    if (
      _M0L6_2atmpS1541 < 0
      || _M0L6_2atmpS1541
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 92 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S109 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1541];
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1538 = _M0MPC14byte4Byte8to__char(_M0L2x0S107);
    moonbit_incref(_M0L3bufS89);
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1538);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1539 = _M0MPC14byte4Byte8to__char(_M0L2x1S108);
    moonbit_incref(_M0L3bufS89);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1539);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1540 = _M0MPC14byte4Byte8to__char(_M0L2x2S109);
    moonbit_incref(_M0L3bufS89);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS89, _M0L6_2atmpS1540);
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
    int32_t _M0L3lenS1485 = _M0L4selfS87->$1;
    int32_t _M0L6_2atmpS1484 = _M0L3lenS1485 + 1;
    uint16_t* _M0L4dataS1486;
    int32_t _M0L3lenS1487;
    int32_t _M0L6_2atmpS1488;
    int32_t _M0L3lenS1490;
    int32_t _M0L6_2atmpS1489;
    moonbit_incref(_M0L4selfS87);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS87, _M0L6_2atmpS1484);
    _M0L4dataS1486 = _M0L4selfS87->$0;
    _M0L3lenS1487 = _M0L4selfS87->$1;
    moonbit_incref(_M0L4dataS1486);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1488 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS85);
    if (
      _M0L3lenS1487 < 0
      || _M0L3lenS1487 >= Moonbit_array_length(_M0L4dataS1486)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1486[_M0L3lenS1487] = _M0L6_2atmpS1488;
    moonbit_decref(_M0L4dataS1486);
    _M0L3lenS1490 = _M0L4selfS87->$1;
    _M0L6_2atmpS1489 = _M0L3lenS1490 + 1;
    _M0L4selfS87->$1 = _M0L6_2atmpS1489;
    moonbit_decref(_M0L4selfS87);
  } else if (_M0L4codeS85 <= 1114111u) {
    int32_t _M0L3lenS1492 = _M0L4selfS87->$1;
    int32_t _M0L6_2atmpS1491 = _M0L3lenS1492 + 2;
    uint32_t _M0L4codeS88;
    uint16_t* _M0L4dataS1493;
    int32_t _M0L3lenS1494;
    uint32_t _M0L6_2atmpS1497;
    uint32_t _M0L6_2atmpS1496;
    int32_t _M0L6_2atmpS1495;
    uint16_t* _M0L4dataS1498;
    int32_t _M0L3lenS1503;
    int32_t _M0L6_2atmpS1499;
    uint32_t _M0L6_2atmpS1502;
    uint32_t _M0L6_2atmpS1501;
    int32_t _M0L6_2atmpS1500;
    int32_t _M0L3lenS1505;
    int32_t _M0L6_2atmpS1504;
    moonbit_incref(_M0L4selfS87);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS87, _M0L6_2atmpS1491);
    _M0L4codeS88 = _M0L4codeS85 - 65536u;
    _M0L4dataS1493 = _M0L4selfS87->$0;
    _M0L3lenS1494 = _M0L4selfS87->$1;
    _M0L6_2atmpS1497 = _M0L4codeS88 >> 10;
    _M0L6_2atmpS1496 = 55296u + _M0L6_2atmpS1497;
    moonbit_incref(_M0L4dataS1493);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1495 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1496);
    if (
      _M0L3lenS1494 < 0
      || _M0L3lenS1494 >= Moonbit_array_length(_M0L4dataS1493)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1493[_M0L3lenS1494] = _M0L6_2atmpS1495;
    moonbit_decref(_M0L4dataS1493);
    _M0L4dataS1498 = _M0L4selfS87->$0;
    _M0L3lenS1503 = _M0L4selfS87->$1;
    _M0L6_2atmpS1499 = _M0L3lenS1503 + 1;
    _M0L6_2atmpS1502 = _M0L4codeS88 & 1023u;
    _M0L6_2atmpS1501 = 56320u + _M0L6_2atmpS1502;
    moonbit_incref(_M0L4dataS1498);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1500 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1501);
    if (
      _M0L6_2atmpS1499 < 0
      || _M0L6_2atmpS1499 >= Moonbit_array_length(_M0L4dataS1498)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1498[_M0L6_2atmpS1499] = _M0L6_2atmpS1500;
    moonbit_decref(_M0L4dataS1498);
    _M0L3lenS1505 = _M0L4selfS87->$1;
    _M0L6_2atmpS1504 = _M0L3lenS1505 + 2;
    _M0L4selfS87->$1 = _M0L6_2atmpS1504;
    moonbit_decref(_M0L4selfS87);
  } else {
    moonbit_decref(_M0L4selfS87);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_70.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS79,
  int32_t _M0L8requiredS80
) {
  uint16_t* _M0L4dataS1483;
  int32_t _M0L12current__lenS78;
  int32_t _M0L13enough__spaceS81;
  int32_t _M0L13enough__spaceS82;
  int32_t _M0L6_2atmpS1481;
  uint16_t* _M0L9new__dataS84;
  uint16_t* _M0L4dataS1479;
  int32_t _M0L3lenS1480;
  uint16_t* _M0L6_2aoldS3773;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS1483 = _M0L4selfS79->$0;
  _M0L12current__lenS78 = Moonbit_array_length(_M0L4dataS1483);
  if (_M0L8requiredS80 <= _M0L12current__lenS78) {
    moonbit_decref(_M0L4selfS79);
    return 0;
  }
  _M0L13enough__spaceS82 = _M0L12current__lenS78;
  while (1) {
    if (_M0L13enough__spaceS82 < _M0L8requiredS80) {
      int32_t _M0L6_2atmpS1482 = _M0L13enough__spaceS82 * 2;
      _M0L13enough__spaceS82 = _M0L6_2atmpS1482;
      continue;
    } else {
      _M0L13enough__spaceS81 = _M0L13enough__spaceS82;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1481 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS84
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS81, _M0L6_2atmpS1481);
  _M0L4dataS1479 = _M0L4selfS79->$0;
  _M0L3lenS1480 = _M0L4selfS79->$1;
  moonbit_incref(_M0L4dataS1479);
  moonbit_incref(_M0L9new__dataS84);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS84, 0, _M0L4dataS1479, 0, _M0L3lenS1480);
  _M0L6_2aoldS3773 = _M0L4selfS79->$0;
  moonbit_decref(_M0L6_2aoldS3773);
  _M0L4selfS79->$0 = _M0L9new__dataS84;
  moonbit_decref(_M0L4selfS79);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS77) {
  int32_t _M0L6_2atmpS1478;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1478 = *(int32_t*)&_M0L4selfS77;
  return (uint16_t)_M0L6_2atmpS1478;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS76) {
  int32_t _M0L6_2atmpS1477;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1477 = _M0L4selfS76;
  return *(uint32_t*)&_M0L6_2atmpS1477;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS74
) {
  int32_t _M0L3lenS1469;
  uint16_t* _M0L4dataS1471;
  int32_t _M0L6_2atmpS1470;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1469 = _M0L4selfS74->$1;
  _M0L4dataS1471 = _M0L4selfS74->$0;
  _M0L6_2atmpS1470 = Moonbit_array_length(_M0L4dataS1471);
  if (_M0L3lenS1469 == _M0L6_2atmpS1470) {
    uint16_t* _M0L8_2afieldS3776 = _M0L4selfS74->$0;
    int32_t _M0L6_2acntS3973 = Moonbit_object_header(_M0L4selfS74)->rc;
    uint16_t* _M0L4dataS1472;
    if (_M0L6_2acntS3973 > 1) {
      int32_t _M0L11_2anew__cntS3974 = _M0L6_2acntS3973 - 1;
      Moonbit_object_header(_M0L4selfS74)->rc = _M0L11_2anew__cntS3974;
      moonbit_incref(_M0L8_2afieldS3776);
    } else if (_M0L6_2acntS3973 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS74);
    }
    _M0L4dataS1472 = _M0L8_2afieldS3776;
    return _M0L4dataS1472;
  } else {
    int32_t _M0L3lenS1475 = _M0L4selfS74->$1;
    int32_t _M0L6_2atmpS1476;
    uint16_t* _M0L4dataS75;
    uint16_t* _M0L4dataS1473;
    int32_t _M0L3lenS1474;
    int32_t _M0L6_2acntS3975;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1476 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS75
    = (uint16_t*)moonbit_make_string(_M0L3lenS1475, _M0L6_2atmpS1476);
    _M0L4dataS1473 = _M0L4selfS74->$0;
    _M0L3lenS1474 = _M0L4selfS74->$1;
    _M0L6_2acntS3975 = Moonbit_object_header(_M0L4selfS74)->rc;
    if (_M0L6_2acntS3975 > 1) {
      int32_t _M0L11_2anew__cntS3976 = _M0L6_2acntS3975 - 1;
      Moonbit_object_header(_M0L4selfS74)->rc = _M0L11_2anew__cntS3976;
      moonbit_incref(_M0L4dataS1473);
    } else if (_M0L6_2acntS3975 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS74);
    }
    moonbit_incref(_M0L4dataS75);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS75, 0, _M0L4dataS1473, 0, _M0L3lenS1474);
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
  struct _M0TPB13StringBuilder* _block_4239;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS72 < 1) {
    _M0L7initialS71 = 1;
  } else {
    int32_t _M0L6_2atmpS1468 = _M0L10size__hintS72 + 1;
    _M0L7initialS71 = _M0L6_2atmpS1468 / 2;
  }
  _M0L4dataS73 = (uint16_t*)moonbit_make_string(_M0L7initialS71, 0);
  _block_4239
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4239)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4239->$0 = _M0L4dataS73;
  _block_4239->$1 = 0;
  return _block_4239;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS1467;
  #line 1867 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1467 = (int32_t)_M0L4selfS70;
  return _M0L6_2atmpS1467;
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
  int32_t _if__result_4240;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS19 == _M0L3srcS20) {
    _if__result_4240 = _M0L11dst__offsetS21 < _M0L11src__offsetS22;
  } else {
    _if__result_4240 = 0;
  }
  if (_if__result_4240) {
    int32_t _M0L1iS23 = 0;
    while (1) {
      if (_M0L1iS23 < _M0L3lenS24) {
        int32_t _M0L6_2atmpS1431 = _M0L11dst__offsetS21 + _M0L1iS23;
        int32_t _M0L6_2atmpS1433 = _M0L11src__offsetS22 + _M0L1iS23;
        int32_t _M0L6_2atmpS1432;
        int32_t _M0L6_2atmpS1434;
        if (
          _M0L6_2atmpS1433 < 0
          || _M0L6_2atmpS1433 >= Moonbit_array_length(_M0L3srcS20)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1432 = (int32_t)_M0L3srcS20[_M0L6_2atmpS1433];
        if (
          _M0L6_2atmpS1431 < 0
          || _M0L6_2atmpS1431 >= Moonbit_array_length(_M0L3dstS19)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS19[_M0L6_2atmpS1431] = _M0L6_2atmpS1432;
        _M0L6_2atmpS1434 = _M0L1iS23 + 1;
        _M0L1iS23 = _M0L6_2atmpS1434;
        continue;
      } else {
        moonbit_decref(_M0L3srcS20);
        moonbit_decref(_M0L3dstS19);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1439 = _M0L3lenS24 - 1;
    int32_t _M0L1iS26 = _M0L6_2atmpS1439;
    while (1) {
      if (_M0L1iS26 >= 0) {
        int32_t _M0L6_2atmpS1435 = _M0L11dst__offsetS21 + _M0L1iS26;
        int32_t _M0L6_2atmpS1437 = _M0L11src__offsetS22 + _M0L1iS26;
        int32_t _M0L6_2atmpS1436;
        int32_t _M0L6_2atmpS1438;
        if (
          _M0L6_2atmpS1437 < 0
          || _M0L6_2atmpS1437 >= Moonbit_array_length(_M0L3srcS20)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1436 = (int32_t)_M0L3srcS20[_M0L6_2atmpS1437];
        if (
          _M0L6_2atmpS1435 < 0
          || _M0L6_2atmpS1435 >= Moonbit_array_length(_M0L3dstS19)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS19[_M0L6_2atmpS1435] = _M0L6_2atmpS1436;
        _M0L6_2atmpS1438 = _M0L1iS26 - 1;
        _M0L1iS26 = _M0L6_2atmpS1438;
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
  int32_t _if__result_4243;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS28 == _M0L3srcS29) {
    _if__result_4243 = _M0L11dst__offsetS30 < _M0L11src__offsetS31;
  } else {
    _if__result_4243 = 0;
  }
  if (_if__result_4243) {
    int32_t _M0L1iS32 = 0;
    while (1) {
      if (_M0L1iS32 < _M0L3lenS33) {
        int32_t _M0L6_2atmpS1440 = _M0L11dst__offsetS30 + _M0L1iS32;
        int32_t _M0L6_2atmpS1442 = _M0L11src__offsetS31 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS1441;
        moonbit_string_t _M0L6_2aoldS3779;
        int32_t _M0L6_2atmpS1443;
        if (
          _M0L6_2atmpS1442 < 0
          || _M0L6_2atmpS1442 >= Moonbit_array_length(_M0L3srcS29)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1441 = (moonbit_string_t)_M0L3srcS29[_M0L6_2atmpS1442];
        if (
          _M0L6_2atmpS1440 < 0
          || _M0L6_2atmpS1440 >= Moonbit_array_length(_M0L3dstS28)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3779 = (moonbit_string_t)_M0L3dstS28[_M0L6_2atmpS1440];
        moonbit_incref(_M0L6_2atmpS1441);
        moonbit_decref(_M0L6_2aoldS3779);
        _M0L3dstS28[_M0L6_2atmpS1440] = _M0L6_2atmpS1441;
        _M0L6_2atmpS1443 = _M0L1iS32 + 1;
        _M0L1iS32 = _M0L6_2atmpS1443;
        continue;
      } else {
        moonbit_decref(_M0L3srcS29);
        moonbit_decref(_M0L3dstS28);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1448 = _M0L3lenS33 - 1;
    int32_t _M0L1iS35 = _M0L6_2atmpS1448;
    while (1) {
      if (_M0L1iS35 >= 0) {
        int32_t _M0L6_2atmpS1444 = _M0L11dst__offsetS30 + _M0L1iS35;
        int32_t _M0L6_2atmpS1446 = _M0L11src__offsetS31 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS1445;
        moonbit_string_t _M0L6_2aoldS3781;
        int32_t _M0L6_2atmpS1447;
        if (
          _M0L6_2atmpS1446 < 0
          || _M0L6_2atmpS1446 >= Moonbit_array_length(_M0L3srcS29)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1445 = (moonbit_string_t)_M0L3srcS29[_M0L6_2atmpS1446];
        if (
          _M0L6_2atmpS1444 < 0
          || _M0L6_2atmpS1444 >= Moonbit_array_length(_M0L3dstS28)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3781 = (moonbit_string_t)_M0L3dstS28[_M0L6_2atmpS1444];
        moonbit_incref(_M0L6_2atmpS1445);
        moonbit_decref(_M0L6_2aoldS3781);
        _M0L3dstS28[_M0L6_2atmpS1444] = _M0L6_2atmpS1445;
        _M0L6_2atmpS1447 = _M0L1iS35 - 1;
        _M0L1iS35 = _M0L6_2atmpS1447;
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
  int32_t _if__result_4246;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS37 == _M0L3srcS38) {
    _if__result_4246 = _M0L11dst__offsetS39 < _M0L11src__offsetS40;
  } else {
    _if__result_4246 = 0;
  }
  if (_if__result_4246) {
    int32_t _M0L1iS41 = 0;
    while (1) {
      if (_M0L1iS41 < _M0L3lenS42) {
        int32_t _M0L6_2atmpS1449 = _M0L11dst__offsetS39 + _M0L1iS41;
        int32_t _M0L6_2atmpS1451 = _M0L11src__offsetS40 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS1450;
        struct _M0TUsiE* _M0L6_2aoldS3783;
        int32_t _M0L6_2atmpS1452;
        if (
          _M0L6_2atmpS1451 < 0
          || _M0L6_2atmpS1451 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1450 = (struct _M0TUsiE*)_M0L3srcS38[_M0L6_2atmpS1451];
        if (
          _M0L6_2atmpS1449 < 0
          || _M0L6_2atmpS1449 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3783 = (struct _M0TUsiE*)_M0L3dstS37[_M0L6_2atmpS1449];
        if (_M0L6_2atmpS1450) {
          moonbit_incref(_M0L6_2atmpS1450);
        }
        if (_M0L6_2aoldS3783) {
          moonbit_decref(_M0L6_2aoldS3783);
        }
        _M0L3dstS37[_M0L6_2atmpS1449] = _M0L6_2atmpS1450;
        _M0L6_2atmpS1452 = _M0L1iS41 + 1;
        _M0L1iS41 = _M0L6_2atmpS1452;
        continue;
      } else {
        moonbit_decref(_M0L3srcS38);
        moonbit_decref(_M0L3dstS37);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1457 = _M0L3lenS42 - 1;
    int32_t _M0L1iS44 = _M0L6_2atmpS1457;
    while (1) {
      if (_M0L1iS44 >= 0) {
        int32_t _M0L6_2atmpS1453 = _M0L11dst__offsetS39 + _M0L1iS44;
        int32_t _M0L6_2atmpS1455 = _M0L11src__offsetS40 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS1454;
        struct _M0TUsiE* _M0L6_2aoldS3785;
        int32_t _M0L6_2atmpS1456;
        if (
          _M0L6_2atmpS1455 < 0
          || _M0L6_2atmpS1455 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1454 = (struct _M0TUsiE*)_M0L3srcS38[_M0L6_2atmpS1455];
        if (
          _M0L6_2atmpS1453 < 0
          || _M0L6_2atmpS1453 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3785 = (struct _M0TUsiE*)_M0L3dstS37[_M0L6_2atmpS1453];
        if (_M0L6_2atmpS1454) {
          moonbit_incref(_M0L6_2atmpS1454);
        }
        if (_M0L6_2aoldS3785) {
          moonbit_decref(_M0L6_2aoldS3785);
        }
        _M0L3dstS37[_M0L6_2atmpS1453] = _M0L6_2atmpS1454;
        _M0L6_2atmpS1456 = _M0L1iS44 - 1;
        _M0L1iS44 = _M0L6_2atmpS1456;
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
  int32_t _if__result_4249;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS46 == _M0L3srcS47) {
    _if__result_4249 = _M0L11dst__offsetS48 < _M0L11src__offsetS49;
  } else {
    _if__result_4249 = 0;
  }
  if (_if__result_4249) {
    int32_t _M0L1iS50 = 0;
    while (1) {
      if (_M0L1iS50 < _M0L3lenS51) {
        int32_t _M0L6_2atmpS1458 = _M0L11dst__offsetS48 + _M0L1iS50;
        int32_t _M0L6_2atmpS1460 = _M0L11src__offsetS49 + _M0L1iS50;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS1459;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2aoldS3787;
        int32_t _M0L6_2atmpS1461;
        if (
          _M0L6_2atmpS1460 < 0
          || _M0L6_2atmpS1460 >= Moonbit_array_length(_M0L3srcS47)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1459
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3srcS47[
            _M0L6_2atmpS1460
          ];
        if (
          _M0L6_2atmpS1458 < 0
          || _M0L6_2atmpS1458 >= Moonbit_array_length(_M0L3dstS46)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3787
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3dstS46[
            _M0L6_2atmpS1458
          ];
        if (_M0L6_2atmpS1459) {
          moonbit_incref(_M0L6_2atmpS1459);
        }
        if (_M0L6_2aoldS3787) {
          moonbit_decref(_M0L6_2aoldS3787);
        }
        _M0L3dstS46[_M0L6_2atmpS1458] = _M0L6_2atmpS1459;
        _M0L6_2atmpS1461 = _M0L1iS50 + 1;
        _M0L1iS50 = _M0L6_2atmpS1461;
        continue;
      } else {
        moonbit_decref(_M0L3srcS47);
        moonbit_decref(_M0L3dstS46);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1466 = _M0L3lenS51 - 1;
    int32_t _M0L1iS53 = _M0L6_2atmpS1466;
    while (1) {
      if (_M0L1iS53 >= 0) {
        int32_t _M0L6_2atmpS1462 = _M0L11dst__offsetS48 + _M0L1iS53;
        int32_t _M0L6_2atmpS1464 = _M0L11src__offsetS49 + _M0L1iS53;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2atmpS1463;
        struct _M0TP36mulpjs4mulp6stream4File* _M0L6_2aoldS3789;
        int32_t _M0L6_2atmpS1465;
        if (
          _M0L6_2atmpS1464 < 0
          || _M0L6_2atmpS1464 >= Moonbit_array_length(_M0L3srcS47)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1463
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3srcS47[
            _M0L6_2atmpS1464
          ];
        if (
          _M0L6_2atmpS1462 < 0
          || _M0L6_2atmpS1462 >= Moonbit_array_length(_M0L3dstS46)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3789
        = (struct _M0TP36mulpjs4mulp6stream4File*)_M0L3dstS46[
            _M0L6_2atmpS1462
          ];
        if (_M0L6_2atmpS1463) {
          moonbit_incref(_M0L6_2atmpS1463);
        }
        if (_M0L6_2aoldS3789) {
          moonbit_decref(_M0L6_2aoldS3789);
        }
        _M0L3dstS46[_M0L6_2atmpS1462] = _M0L6_2atmpS1463;
        _M0L6_2atmpS1465 = _M0L1iS53 - 1;
        _M0L1iS53 = _M0L6_2atmpS1465;
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
  uint32_t _M0L3accS1430;
  uint32_t _M0L6_2atmpS1429;
  #line 236 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1430 = _M0L4selfS17->$0;
  _M0L6_2atmpS1429 = _M0L3accS1430 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1429;
  #line 238 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1427;
  uint32_t _M0L6_2atmpS1428;
  uint32_t _M0L6_2atmpS1426;
  uint32_t _M0L6_2atmpS1425;
  uint32_t _M0L6_2atmpS1424;
  #line 451 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1427 = _M0L4selfS15->$0;
  _M0L6_2atmpS1428 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1426 = _M0L3accS1427 + _M0L6_2atmpS1428;
  #line 452 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1425 = _M0FPB4rotl(_M0L6_2atmpS1426, 17);
  _M0L6_2atmpS1424 = _M0L6_2atmpS1425 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1424;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1421;
  int32_t _M0L6_2atmpS1423;
  uint32_t _M0L6_2atmpS1422;
  #line 461 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1421 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1423 = 32 - _M0L1rS14;
  _M0L6_2atmpS1422 = _M0L1xS13 >> (_M0L6_2atmpS1423 & 31);
  return _M0L6_2atmpS1421 | _M0L6_2atmpS1422;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__5208S9,
  struct _M0TPB6Logger _M0L10_2ax__5209S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS3791;
  int32_t _M0L6_2acntS3977;
  moonbit_string_t _M0L15_2a_2aarg__5210S11;
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__5208S9;
  _M0L8_2afieldS3791 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS3977 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS3977 > 1) {
    int32_t _M0L11_2anew__cntS3978 = _M0L6_2acntS3977 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS3978;
    moonbit_incref(_M0L8_2afieldS3791);
  } else if (_M0L6_2acntS3977 == 1) {
    #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__5210S11 = _M0L8_2afieldS3791;
  if (_M0L10_2ax__5209S12.$1) {
    moonbit_incref(_M0L10_2ax__5209S12.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S12.$0->$method_0(_M0L10_2ax__5209S12.$1, (moonbit_string_t)moonbit_string_literal_71.data);
  if (_M0L10_2ax__5209S12.$1) {
    moonbit_incref(_M0L10_2ax__5209S12.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__5209S12, _M0L15_2a_2aarg__5210S11);
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S12.$0->$method_0(_M0L10_2ax__5209S12.$1, (moonbit_string_t)moonbit_string_literal_72.data);
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

struct _M0TP36mulpjs4mulp6stream10ByteStream* _M0FPC15abort5abortGRP36mulpjs4mulp6stream10ByteStreamE(
  moonbit_string_t _M0L3msgS2
) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS2);
  moonbit_decref(_M0L3msgS2);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t _M0L3msgS3) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1360) {
  switch (Moonbit_object_tag(_M0L4_2aeS1360)) {
    case 1: {
      moonbit_decref(_M0L4_2aeS1360);
      return (moonbit_string_t)moonbit_string_literal_73.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1360);
      return (moonbit_string_t)moonbit_string_literal_74.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1360);
      return (moonbit_string_t)moonbit_string_literal_75.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1360);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1360);
      return (moonbit_string_t)moonbit_string_literal_76.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1406
) {
  moonbit_string_t _M0L7_2aselfS1405 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1406;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS1405);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1404,
  struct _M0TPB6Logger _M0L8_2aparamS1403
) {
  moonbit_string_t _M0L7_2aselfS1402 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1404;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS1402, _M0L8_2aparamS1403);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1400
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1401 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1400;
  int32_t _M0L7_2aselfS1399 = _M0L14_2aboxed__selfS1401->$0;
  moonbit_decref(_M0L14_2aboxed__selfS1401);
  return _M0IPC14bool4BoolPB4Show10to__string(_M0L7_2aselfS1399);
}

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1397,
  struct _M0TPB6Logger _M0L8_2aparamS1396
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1398 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1397;
  int32_t _M0L7_2aselfS1395 = _M0L14_2aboxed__selfS1398->$0;
  moonbit_decref(_M0L14_2aboxed__selfS1398);
  _M0IPC14bool4BoolPB4Show6output(_M0L7_2aselfS1395, _M0L8_2aparamS1396);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1394,
  int32_t _M0L8_2aparamS1393
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1392 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1394;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1392, _M0L8_2aparamS1393);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1391,
  struct _M0TPC16string10StringView _M0L8_2aparamS1390
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1389 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1391;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1389, _M0L8_2aparamS1390);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1388,
  moonbit_string_t _M0L8_2aparamS1385,
  int32_t _M0L8_2aparamS1386,
  int32_t _M0L8_2aparamS1387
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1384 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1388;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1384, _M0L8_2aparamS1385, _M0L8_2aparamS1386, _M0L8_2aparamS1387);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1383,
  moonbit_string_t _M0L8_2aparamS1382
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1381 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1383;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1381, _M0L8_2aparamS1382);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1420 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1419;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1418;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1287;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1417;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1416;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1415;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1414;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1286;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1413;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1412;
  _M0L6_2atmpS1420[0] = (moonbit_string_t)moonbit_string_literal_77.data;
  moonbit_incref(_M0FP36mulpjs4mulp6stream59____test__66696c655f646573745f7762746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1419
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1419)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1419->$0
  = _M0FP36mulpjs4mulp6stream59____test__66696c655f646573745f7762746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1419->$1 = _M0L6_2atmpS1420;
  _M0L8_2atupleS1418
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1418)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1418->$0 = 0;
  _M0L8_2atupleS1418->$1 = _M0L8_2atupleS1419;
  _M0L7_2abindS1287
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1287[0] = _M0L8_2atupleS1418;
  _M0L6_2atmpS1417 = _M0L7_2abindS1287;
  _M0L6_2atmpS1416
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1417
  };
  #line 398 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1415
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1416);
  _M0L8_2atupleS1414
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1414)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1414->$0 = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L8_2atupleS1414->$1 = _M0L6_2atmpS1415;
  _M0L7_2abindS1286
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1286[0] = _M0L8_2atupleS1414;
  _M0L6_2atmpS1413 = _M0L7_2abindS1286;
  _M0L6_2atmpS1412
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS1413
  };
  #line 397 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0FP36mulpjs4mulp6stream48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1412);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1411;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1354;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1355;
  int32_t _M0L7_2abindS1356;
  int32_t _M0L2__S1357;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1411
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1354
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1354)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1354->$0 = _M0L6_2atmpS1411;
  _M0L12async__testsS1354->$1 = 0;
  #line 438 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1355
  = _M0FP36mulpjs4mulp6stream52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1356 = _M0L7_2abindS1355->$1;
  _M0L2__S1357 = 0;
  while (1) {
    if (_M0L2__S1357 < _M0L7_2abindS1356) {
      struct _M0TUsiE** _M0L3bufS1410 = _M0L7_2abindS1355->$0;
      struct _M0TUsiE* _M0L3argS1358 =
        (struct _M0TUsiE*)_M0L3bufS1410[_M0L2__S1357];
      moonbit_string_t _M0L6_2atmpS1407 = _M0L3argS1358->$0;
      int32_t _M0L6_2atmpS1408 = _M0L3argS1358->$1;
      int32_t _M0L6_2atmpS1409;
      moonbit_incref(_M0L6_2atmpS1407);
      moonbit_incref(_M0L12async__testsS1354);
      #line 439 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
      _M0FP36mulpjs4mulp6stream44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1354, _M0L6_2atmpS1407, _M0L6_2atmpS1408);
      _M0L6_2atmpS1409 = _M0L2__S1357 + 1;
      _M0L2__S1357 = _M0L6_2atmpS1409;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1355);
    }
    break;
  }
  #line 441 "/Users/user/workspace/github/gulp/mulp/stream/__generated_driver_for_whitebox_test.mbt"
  _M0IP016_24default__implP36mulpjs4mulp6stream28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp6stream34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1354);
  return 0;
}