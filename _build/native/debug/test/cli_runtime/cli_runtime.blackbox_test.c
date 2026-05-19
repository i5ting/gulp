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
struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0KTPB4ShowS4Bool;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp28cli__runtime__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB8MutLocalGORPC16string10StringViewE;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0Y4Bool;

struct _M0TWEOc;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB6Logger;

struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TWEuQRPC15error5Error;

struct _M0TURPB6LoggerRPC16string10StringViewE;

struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__;

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp28cli__runtime__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0KTPB4ShowTPB5ArrayGsE;

struct _M0DTPC15error5Error114mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB8MutLocalGiE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0BTPB4Show;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS6String;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929;

struct _M0TWcERPC16string10StringView;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTPC16option6OptionGOsE4Some;

struct _M0TPB9ArrayViewGsE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS4Bool {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPB8MutLocalGiE* $1;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp28cli__runtime__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TPB8MutLocalGORPC16string10StringViewE {
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

struct _M0Y4Bool {
  int32_t $0;
  
};

struct _M0TWEOc {
  int32_t(* code)(struct _M0TWEOc*);
  
};

struct _M0TWERPC16option6OptionGRPC16string10StringViewE {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  uint16_t* $0;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TPB5ArrayGUsiEE {
  int32_t $1;
  struct _M0TUsiE** $0;
  
};

struct _M0TWRPC15error5ErrorEs {
  moonbit_string_t(* code)(struct _M0TWRPC15error5ErrorEs*, void*);
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0TURPB6LoggerRPC16string10StringViewE {
  int32_t $1_1;
  int32_t $1_2;
  struct _M0BTPB6Logger* $0_0;
  void* $0_1;
  moonbit_string_t $1_0;
  
};

struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWcERPC16string10StringView* $0;
  struct _M0TWEOc* $1;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp28cli__runtime__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowTPB5ArrayGsE {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTPC15error5Error114mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPB8MutLocalGiE {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPB8MutLocalGiE* $0;
  moonbit_string_t $2_0;
  
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

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
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

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
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

struct _M0KTPB4ShowS6String {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TWcERPC16string10StringView {
  struct _M0TPC16string10StringView(* code)(
    struct _M0TWcERPC16string10StringView*,
    int32_t
  );
  
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

struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC16option6OptionGOsE4Some {
  moonbit_string_t $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0TUWEuQRPC15error5ErrorNsE {
  struct _M0TWEuQRPC15error5Error* $0;
  moonbit_string_t* $1;
  
};

struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* $0;
  moonbit_string_t $1_0;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS938(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS929(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testC2141l432(
  struct _M0TWEOc*
);

int32_t _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testC2137l433(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS862(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS857(
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S844(
  int32_t,
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp28cli__runtime__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__5(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__4(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__0(
  
);

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime19compatibility__hint(
  moonbit_string_t,
  struct _M0TPB5ArrayGsE*
);

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime27select__compatible__version(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0FP36mulpjs4mulp12cli__runtime18version__satisfies(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime14minor__version(
  moonbit_string_t
);

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime14major__version(
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp12cli__runtime14version__parts(
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp12cli__runtime16cli__completions(
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp12cli__runtime12known__flags();

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime9cli__help(moonbit_string_t);

moonbit_string_t _M0MP36mulpjs4mulp12cli__runtime10CliRuntime11entry__file(
  struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime*
);

struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0FP36mulpjs4mulp12cli__runtime12cli__runtime(
  moonbit_string_t,
  void*,
  void*
);

struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0FP36mulpjs4mulp12cli__runtime20cli__runtime_2einner(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

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

moonbit_string_t _M0MPC16option6Option6unwrapGsE(moonbit_string_t);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(struct _M0TPB5ArrayGsE*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1641l680(struct _M0TWEOs*);

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

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

void* _M0MPC16string10StringView5splitC1624l1078(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC1620l1075(
  struct _M0TWcERPC16string10StringView*,
  int32_t
);

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t);

moonbit_string_t _M0FPB16char__to__string(int32_t);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TWEOc*,
  struct _M0TWcERPC16string10StringView*
);

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC1613l319(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
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

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

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

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView4iterC1526l208(struct _M0TWEOc*);

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

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView,
  int32_t,
  int64_t
);

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs*);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc*);

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

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc*);

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

int32_t _M0MPB6Hasher13combine__uint(struct _M0TPB6Hasher*, uint32_t);

int32_t _M0MPB6Hasher8consume4(struct _M0TPB6Hasher*, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0IPB7FailurePB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

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

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGRPB5ArrayGsEE(
  void*
);

int32_t _M0IPC15array5ArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGsE(
  void*,
  struct _M0TPB6Logger
);

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    114, 117, 110, 116, 105, 109, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 56, 58, 49, 49, 45, 52, 56, 58, 54, 49, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    48, 46, 50, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    49, 46, 48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    45, 45, 119, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    78, 111, 32, 108, 111, 99, 97, 108, 32, 109, 117, 108, 112, 32, 118, 
    101, 114, 115, 105, 111, 110, 32, 115, 97, 116, 105, 115, 102, 105, 
    101, 115, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 49, 55, 58, 49, 49, 45, 49, 55, 58, 52, 48, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[45]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 44), 
    10, 85, 115, 97, 103, 101, 58, 32, 109, 117, 108, 112, 32, 91, 45, 
    45, 116, 97, 115, 107, 115, 124, 45, 45, 116, 114, 101, 101, 124, 
    45, 45, 119, 97, 116, 99, 104, 93, 32, 91, 116, 97, 115, 107, 93, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 55, 58, 51, 45, 52, 55, 58, 56, 57, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 54, 58, 56, 51, 45, 52, 54, 58, 57, 48, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 53, 51, 58, 51, 45, 53, 57, 58, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    94, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    45, 45, 102, 105, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    94, 48, 46, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 56, 58, 51, 45, 56, 58, 54, 57, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 52, 58, 49, 51, 45, 50, 52, 58, 55, 48, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 54, 58, 51, 45, 52, 54, 58, 57, 49, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_84 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    45, 45, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    45, 45, 109, 117, 108, 112, 102, 105, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    123, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    47, 116, 109, 112, 47, 99, 117, 115, 116, 111, 109, 46, 109, 98, 
    116, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    99, 108, 105, 32, 114, 117, 110, 116, 105, 109, 101, 32, 114, 101, 
    110, 100, 101, 114, 115, 32, 104, 101, 108, 112, 32, 115, 117, 109, 
    109, 97, 114, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    114, 117, 110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 
    98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    59, 32, 97, 118, 97, 105, 108, 97, 98, 108, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 54, 58, 49, 51, 45, 51, 54, 58, 51, 49, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    91, 34, 45, 45, 116, 97, 115, 107, 115, 34, 44, 32, 34, 45, 45, 116, 
    97, 115, 107, 115, 45, 115, 105, 109, 112, 108, 101, 34, 44, 32, 
    34, 45, 45, 116, 114, 101, 101, 34, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    45, 45, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_107 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_78 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    78, 111, 32, 108, 111, 99, 97, 108, 32, 109, 117, 108, 112, 32, 118, 
    101, 114, 115, 105, 111, 110, 32, 115, 97, 116, 105, 115, 102, 105, 
    101, 115, 32, 94, 49, 59, 32, 97, 118, 97, 105, 108, 97, 98, 108, 
    101, 58, 32, 48, 46, 49, 46, 48, 44, 32, 48, 46, 50, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 49, 55, 58, 51, 45, 49, 55, 58, 55, 55, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    45, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    99, 108, 105, 32, 114, 117, 110, 116, 105, 109, 101, 32, 115, 117, 
    103, 103, 101, 115, 116, 115, 32, 99, 111, 109, 112, 108, 101, 116, 
    105, 111, 110, 115, 32, 102, 111, 114, 32, 99, 111, 114, 101, 32, 
    102, 108, 97, 103, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    45, 45, 116, 97, 115, 107, 115, 45, 115, 105, 109, 112, 108, 101, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 57, 58, 53, 45, 51, 57, 58, 50, 53, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    45, 45, 118, 101, 114, 115, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    99, 108, 105, 32, 114, 117, 110, 116, 105, 109, 101, 32, 115, 101, 
    108, 101, 99, 116, 115, 32, 101, 120, 112, 108, 105, 99, 105, 116, 
    32, 109, 117, 108, 112, 102, 105, 108, 101, 32, 98, 101, 102, 111, 
    114, 101, 32, 100, 105, 115, 99, 111, 118, 101, 114, 101, 100, 32, 
    102, 105, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    109, 117, 108, 112, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 52, 58, 51, 45, 51, 55, 58, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 53, 52, 58, 53, 45, 53, 55, 58, 54, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_39 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    45, 45, 104, 101, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    94, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 109, 117, 
    108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 99, 108, 105, 95, 
    114, 117, 110, 116, 105, 109, 101, 34, 44, 32, 34, 102, 105, 108, 
    101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 49, 58, 53, 45, 51, 49, 58, 50, 55, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 48, 58, 49, 51, 45, 52, 48, 58, 51, 49, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    45, 45, 99, 119, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    94, 48, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 53, 58, 53, 45, 51, 53, 58, 50, 55, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    48, 46, 49, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    45, 45, 103, 117, 108, 112, 102, 105, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 97, 112, 112, 
    47, 109, 117, 108, 112, 46, 109, 98, 116, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 56, 58, 51, 45, 52, 49, 58, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 56, 58, 49, 49, 45, 56, 58, 52, 48, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 97, 112, 112, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    45, 118, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_61 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 94, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 49, 55, 58, 53, 48, 45, 49, 55, 58, 55, 54, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    45, 45, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 55, 58, 49, 49, 45, 52, 55, 58, 55, 49, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_79 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 51, 58, 53, 45, 50, 51, 58, 50, 50, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    91, 34, 45, 45, 103, 117, 108, 112, 102, 105, 108, 101, 34, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 45), 
    99, 108, 105, 32, 114, 117, 110, 116, 105, 109, 101, 32, 102, 97, 
    108, 108, 115, 32, 98, 97, 99, 107, 32, 116, 111, 32, 100, 105, 115, 
    99, 111, 118, 101, 114, 101, 100, 32, 109, 117, 108, 112, 102, 105, 
    108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[106]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 105), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 99, 108, 
    105, 95, 114, 117, 110, 116, 105, 109, 101, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 
    116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 
    107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 50, 50, 58, 51, 45, 50, 53, 58, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    91, 34, 45, 118, 34, 44, 32, 34, 45, 84, 34, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 53, 56, 58, 49, 51, 45, 53, 56, 58, 55, 52, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 56, 58, 55, 49, 45, 52, 56, 58, 55, 55, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 56, 58, 51, 45, 52, 56, 58, 55, 56, 64, 109, 117, 108, 112, 
    106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 55, 58, 56, 49, 45, 52, 55, 58, 56, 56, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 48, 58, 51, 45, 51, 51, 58, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 52, 54, 58, 49, 49, 45, 52, 54, 58, 55, 51, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    99, 108, 105, 32, 114, 117, 110, 116, 105, 109, 101, 32, 114, 101, 
    110, 100, 101, 114, 115, 32, 99, 111, 109, 112, 97, 116, 105, 98, 
    105, 108, 105, 116, 121, 32, 104, 105, 110, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[45]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 44), 
    99, 108, 105, 32, 114, 117, 110, 116, 105, 109, 101, 32, 99, 104, 
    101, 99, 107, 115, 32, 99, 111, 109, 112, 97, 116, 105, 98, 108, 
    101, 32, 109, 97, 106, 111, 114, 32, 118, 101, 114, 115, 105, 111, 
    110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 56, 58, 53, 48, 45, 56, 58, 54, 56, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_62 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 46, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    99, 108, 105, 95, 114, 117, 110, 116, 105, 109, 101, 47, 114, 117, 
    110, 116, 105, 109, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 50, 58, 49, 51, 45, 51, 50, 58, 54, 48, 64, 109, 117, 108, 
    112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    109, 117, 108, 112, 32, 48, 46, 49, 46, 48, 10, 85, 115, 97, 103, 
    101, 58, 32, 109, 117, 108, 112, 32, 91, 45, 45, 116, 97, 115, 107, 
    115, 124, 45, 45, 116, 114, 101, 101, 124, 45, 45, 119, 97, 116, 
    99, 104, 93, 32, 91, 116, 97, 115, 107, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[104]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 103), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 99, 108, 
    105, 95, 114, 117, 110, 116, 105, 109, 101, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 
    69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    45, 45, 116, 114, 101, 101, 0
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

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__4_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__4_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWcERPC16string10StringView data;
  
} const _M0MPC16string10StringView5splitC1620l1075$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MPC16string10StringView5splitC1620l1075
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__3_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS938$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS938
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__5_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__5_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__2_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__4_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__4_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__5_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__5_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__0_2edyncall$closure.data;

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

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2177
) {
  return _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2176
) {
  return _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2175
) {
  return _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2174
) {
  return _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2173
) {
  return _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__5();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test55____test__72756e74696d655f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2172
) {
  return _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__4();
}

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS959,
  moonbit_string_t _M0L8filenameS934,
  int32_t _M0L5indexS937
) {
  struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929* _closure_2415;
  struct _M0TWssbEu* _M0L14handle__resultS929;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS938;
  void* _M0L11_2atry__errS953;
  struct moonbit_result_0 _tmp_2417;
  int32_t _handle__error__result_2418;
  int32_t _M0L6_2atmpS2160;
  void* _M0L3errS954;
  moonbit_string_t _M0L4nameS956;
  struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS957;
  moonbit_string_t _M0L8_2afieldS2178;
  int32_t _M0L6_2acntS2335;
  moonbit_string_t _M0L7_2anameS958;
  #line 531 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS934);
  _closure_2415
  = (struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929*)moonbit_malloc(sizeof(struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929));
  Moonbit_object_header(_closure_2415)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929, $1) >> 2, 1, 0);
  _closure_2415->code
  = &_M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS929;
  _closure_2415->$0 = _M0L5indexS937;
  _closure_2415->$1 = _M0L8filenameS934;
  _M0L14handle__resultS929 = (struct _M0TWssbEu*)_closure_2415;
  _M0L17error__to__stringS938
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS938$closure.data;
  moonbit_incref(_M0L12async__testsS959);
  moonbit_incref(_M0L17error__to__stringS938);
  moonbit_incref(_M0L8filenameS934);
  moonbit_incref(_M0L14handle__resultS929);
  #line 565 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _tmp_2417
  = _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS959, _M0L8filenameS934, _M0L5indexS937, _M0L14handle__resultS929, _M0L17error__to__stringS938);
  if (_tmp_2417.tag) {
    int32_t const _M0L5_2aokS2169 = _tmp_2417.data.ok;
    _handle__error__result_2418 = _M0L5_2aokS2169;
  } else {
    void* const _M0L6_2aerrS2170 = _tmp_2417.data.err;
    moonbit_decref(_M0L12async__testsS959);
    moonbit_decref(_M0L17error__to__stringS938);
    moonbit_decref(_M0L8filenameS934);
    _M0L11_2atry__errS953 = _M0L6_2aerrS2170;
    goto join_952;
  }
  if (_handle__error__result_2418) {
    moonbit_decref(_M0L12async__testsS959);
    moonbit_decref(_M0L17error__to__stringS938);
    moonbit_decref(_M0L8filenameS934);
    _M0L6_2atmpS2160 = 1;
  } else {
    struct moonbit_result_0 _tmp_2419;
    int32_t _handle__error__result_2420;
    moonbit_incref(_M0L12async__testsS959);
    moonbit_incref(_M0L17error__to__stringS938);
    moonbit_incref(_M0L8filenameS934);
    moonbit_incref(_M0L14handle__resultS929);
    #line 568 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    _tmp_2419
    = _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS959, _M0L8filenameS934, _M0L5indexS937, _M0L14handle__resultS929, _M0L17error__to__stringS938);
    if (_tmp_2419.tag) {
      int32_t const _M0L5_2aokS2167 = _tmp_2419.data.ok;
      _handle__error__result_2420 = _M0L5_2aokS2167;
    } else {
      void* const _M0L6_2aerrS2168 = _tmp_2419.data.err;
      moonbit_decref(_M0L12async__testsS959);
      moonbit_decref(_M0L17error__to__stringS938);
      moonbit_decref(_M0L8filenameS934);
      _M0L11_2atry__errS953 = _M0L6_2aerrS2168;
      goto join_952;
    }
    if (_handle__error__result_2420) {
      moonbit_decref(_M0L12async__testsS959);
      moonbit_decref(_M0L17error__to__stringS938);
      moonbit_decref(_M0L8filenameS934);
      _M0L6_2atmpS2160 = 1;
    } else {
      struct moonbit_result_0 _tmp_2421;
      int32_t _handle__error__result_2422;
      moonbit_incref(_M0L12async__testsS959);
      moonbit_incref(_M0L17error__to__stringS938);
      moonbit_incref(_M0L8filenameS934);
      moonbit_incref(_M0L14handle__resultS929);
      #line 571 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _tmp_2421
      = _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS959, _M0L8filenameS934, _M0L5indexS937, _M0L14handle__resultS929, _M0L17error__to__stringS938);
      if (_tmp_2421.tag) {
        int32_t const _M0L5_2aokS2165 = _tmp_2421.data.ok;
        _handle__error__result_2422 = _M0L5_2aokS2165;
      } else {
        void* const _M0L6_2aerrS2166 = _tmp_2421.data.err;
        moonbit_decref(_M0L12async__testsS959);
        moonbit_decref(_M0L17error__to__stringS938);
        moonbit_decref(_M0L8filenameS934);
        _M0L11_2atry__errS953 = _M0L6_2aerrS2166;
        goto join_952;
      }
      if (_handle__error__result_2422) {
        moonbit_decref(_M0L12async__testsS959);
        moonbit_decref(_M0L17error__to__stringS938);
        moonbit_decref(_M0L8filenameS934);
        _M0L6_2atmpS2160 = 1;
      } else {
        struct moonbit_result_0 _tmp_2423;
        int32_t _handle__error__result_2424;
        moonbit_incref(_M0L12async__testsS959);
        moonbit_incref(_M0L17error__to__stringS938);
        moonbit_incref(_M0L8filenameS934);
        moonbit_incref(_M0L14handle__resultS929);
        #line 574 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        _tmp_2423
        = _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS959, _M0L8filenameS934, _M0L5indexS937, _M0L14handle__resultS929, _M0L17error__to__stringS938);
        if (_tmp_2423.tag) {
          int32_t const _M0L5_2aokS2163 = _tmp_2423.data.ok;
          _handle__error__result_2424 = _M0L5_2aokS2163;
        } else {
          void* const _M0L6_2aerrS2164 = _tmp_2423.data.err;
          moonbit_decref(_M0L12async__testsS959);
          moonbit_decref(_M0L17error__to__stringS938);
          moonbit_decref(_M0L8filenameS934);
          _M0L11_2atry__errS953 = _M0L6_2aerrS2164;
          goto join_952;
        }
        if (_handle__error__result_2424) {
          moonbit_decref(_M0L12async__testsS959);
          moonbit_decref(_M0L17error__to__stringS938);
          moonbit_decref(_M0L8filenameS934);
          _M0L6_2atmpS2160 = 1;
        } else {
          struct moonbit_result_0 _tmp_2425;
          moonbit_incref(_M0L14handle__resultS929);
          #line 577 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
          _tmp_2425
          = _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS959, _M0L8filenameS934, _M0L5indexS937, _M0L14handle__resultS929, _M0L17error__to__stringS938);
          if (_tmp_2425.tag) {
            int32_t const _M0L5_2aokS2161 = _tmp_2425.data.ok;
            _M0L6_2atmpS2160 = _M0L5_2aokS2161;
          } else {
            void* const _M0L6_2aerrS2162 = _tmp_2425.data.err;
            _M0L11_2atry__errS953 = _M0L6_2aerrS2162;
            goto join_952;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2160) {
    void* _M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2171 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2171)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2171)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS953
    = _M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2171;
    goto join_952;
  } else {
    moonbit_decref(_M0L14handle__resultS929);
  }
  goto joinlet_2416;
  join_952:;
  _M0L3errS954 = _M0L11_2atry__errS953;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS957
  = (struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS954;
  _M0L8_2afieldS2178 = _M0L36_2aMoonBitTestDriverInternalSkipTestS957->$0;
  _M0L6_2acntS2335
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS957)->rc;
  if (_M0L6_2acntS2335 > 1) {
    int32_t _M0L11_2anew__cntS2336 = _M0L6_2acntS2335 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS957)->rc
    = _M0L11_2anew__cntS2336;
    moonbit_incref(_M0L8_2afieldS2178);
  } else if (_M0L6_2acntS2335 == 1) {
    #line 584 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS957);
  }
  _M0L7_2anameS958 = _M0L8_2afieldS2178;
  _M0L4nameS956 = _M0L7_2anameS958;
  goto join_955;
  goto joinlet_2426;
  join_955:;
  #line 585 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS929(_M0L14handle__resultS929, _M0L4nameS956, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2426:;
  joinlet_2416:;
  return 0;
}

moonbit_string_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS938(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2159,
  void* _M0L3errS939
) {
  void* _M0L1eS941;
  moonbit_string_t _M0L1eS943;
  #line 554 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2159);
  switch (Moonbit_object_tag(_M0L3errS939)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS944 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS939;
      moonbit_string_t _M0L8_2afieldS2179 = _M0L10_2aFailureS944->$0;
      int32_t _M0L6_2acntS2337 =
        Moonbit_object_header(_M0L10_2aFailureS944)->rc;
      moonbit_string_t _M0L4_2aeS945;
      if (_M0L6_2acntS2337 > 1) {
        int32_t _M0L11_2anew__cntS2338 = _M0L6_2acntS2337 - 1;
        Moonbit_object_header(_M0L10_2aFailureS944)->rc
        = _M0L11_2anew__cntS2338;
        moonbit_incref(_M0L8_2afieldS2179);
      } else if (_M0L6_2acntS2337 == 1) {
        #line 555 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS944);
      }
      _M0L4_2aeS945 = _M0L8_2afieldS2179;
      _M0L1eS943 = _M0L4_2aeS945;
      goto join_942;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS946 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS939;
      moonbit_string_t _M0L8_2afieldS2180 = _M0L15_2aInspectErrorS946->$0;
      int32_t _M0L6_2acntS2339 =
        Moonbit_object_header(_M0L15_2aInspectErrorS946)->rc;
      moonbit_string_t _M0L4_2aeS947;
      if (_M0L6_2acntS2339 > 1) {
        int32_t _M0L11_2anew__cntS2340 = _M0L6_2acntS2339 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS946)->rc
        = _M0L11_2anew__cntS2340;
        moonbit_incref(_M0L8_2afieldS2180);
      } else if (_M0L6_2acntS2339 == 1) {
        #line 555 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS946);
      }
      _M0L4_2aeS947 = _M0L8_2afieldS2180;
      _M0L1eS943 = _M0L4_2aeS947;
      goto join_942;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS948 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS939;
      moonbit_string_t _M0L8_2afieldS2181 = _M0L16_2aSnapshotErrorS948->$0;
      int32_t _M0L6_2acntS2341 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS948)->rc;
      moonbit_string_t _M0L4_2aeS949;
      if (_M0L6_2acntS2341 > 1) {
        int32_t _M0L11_2anew__cntS2342 = _M0L6_2acntS2341 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS948)->rc
        = _M0L11_2anew__cntS2342;
        moonbit_incref(_M0L8_2afieldS2181);
      } else if (_M0L6_2acntS2341 == 1) {
        #line 555 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS948);
      }
      _M0L4_2aeS949 = _M0L8_2afieldS2181;
      _M0L1eS943 = _M0L4_2aeS949;
      goto join_942;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error114mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS950 =
        (struct _M0DTPC15error5Error114mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS939;
      moonbit_string_t _M0L8_2afieldS2182 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS950->$0;
      int32_t _M0L6_2acntS2343 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS950)->rc;
      moonbit_string_t _M0L4_2aeS951;
      if (_M0L6_2acntS2343 > 1) {
        int32_t _M0L11_2anew__cntS2344 = _M0L6_2acntS2343 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS950)->rc
        = _M0L11_2anew__cntS2344;
        moonbit_incref(_M0L8_2afieldS2182);
      } else if (_M0L6_2acntS2343 == 1) {
        #line 555 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS950);
      }
      _M0L4_2aeS951 = _M0L8_2afieldS2182;
      _M0L1eS943 = _M0L4_2aeS951;
      goto join_942;
      break;
    }
    default: {
      _M0L1eS941 = _M0L3errS939;
      goto join_940;
      break;
    }
  }
  join_942:;
  return _M0L1eS943;
  join_940:;
  #line 560 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS941);
}

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS929(
  struct _M0TWssbEu* _M0L6_2aenvS2145,
  moonbit_string_t _M0L8testnameS930,
  moonbit_string_t _M0L7messageS931,
  int32_t _M0L7skippedS932
) {
  struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929* _M0L14_2acasted__envS2146;
  moonbit_string_t _M0L8filenameS934;
  int32_t _M0L5indexS937;
  int32_t _M0L6_2acntS2345;
  int32_t _if__result_2429;
  moonbit_string_t _M0L10file__nameS933;
  moonbit_string_t _M0L10test__nameS935;
  moonbit_string_t _M0L7messageS936;
  moonbit_string_t _M0L6_2atmpS2158;
  moonbit_string_t _M0L6_2atmpS2157;
  moonbit_string_t _M0L6_2atmpS2155;
  moonbit_string_t _M0L6_2atmpS2156;
  moonbit_string_t _M0L6_2atmpS2154;
  moonbit_string_t _M0L6_2atmpS2152;
  moonbit_string_t _M0L6_2atmpS2153;
  moonbit_string_t _M0L6_2atmpS2151;
  moonbit_string_t _M0L6_2atmpS2149;
  moonbit_string_t _M0L6_2atmpS2150;
  moonbit_string_t _M0L6_2atmpS2148;
  moonbit_string_t _M0L6_2atmpS2147;
  #line 538 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2146
  = (struct _M0R117_24mulpjs_2fmulp_2fcli__runtime__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c929*)_M0L6_2aenvS2145;
  _M0L8filenameS934 = _M0L14_2acasted__envS2146->$1;
  _M0L5indexS937 = _M0L14_2acasted__envS2146->$0;
  _M0L6_2acntS2345 = Moonbit_object_header(_M0L14_2acasted__envS2146)->rc;
  if (_M0L6_2acntS2345 > 1) {
    int32_t _M0L11_2anew__cntS2346 = _M0L6_2acntS2345 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2146)->rc
    = _M0L11_2anew__cntS2346;
    moonbit_incref(_M0L8filenameS934);
  } else if (_M0L6_2acntS2345 == 1) {
    #line 538 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2146);
  }
  if (!_M0L7skippedS932) {
    _if__result_2429 = 1;
  } else {
    _if__result_2429 = 0;
  }
  if (_if__result_2429) {
    
  }
  #line 544 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS933
  = _M0MPC16string6String14escape_2einner(_M0L8filenameS934, 1);
  #line 545 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS935
  = _M0MPC16string6String14escape_2einner(_M0L8testnameS930, 1);
  #line 546 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS936
  = _M0MPC16string6String14escape_2einner(_M0L7messageS931, 1);
  #line 547 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 549 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2158
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS933);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2157
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2158);
  moonbit_decref(_M0L6_2atmpS2158);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2155
  = moonbit_add_string(_M0L6_2atmpS2157, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2157);
  #line 549 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2156 = _M0IPC13int3IntPB4Show10to__string(_M0L5indexS937);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2154 = moonbit_add_string(_M0L6_2atmpS2155, _M0L6_2atmpS2156);
  moonbit_decref(_M0L6_2atmpS2156);
  moonbit_decref(_M0L6_2atmpS2155);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2152
  = moonbit_add_string(_M0L6_2atmpS2154, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2154);
  #line 549 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2153
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS935);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2151 = moonbit_add_string(_M0L6_2atmpS2152, _M0L6_2atmpS2153);
  moonbit_decref(_M0L6_2atmpS2153);
  moonbit_decref(_M0L6_2atmpS2152);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2149
  = moonbit_add_string(_M0L6_2atmpS2151, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2151);
  #line 549 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2150
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS936);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2148 = moonbit_add_string(_M0L6_2atmpS2149, _M0L6_2atmpS2150);
  moonbit_decref(_M0L6_2atmpS2150);
  moonbit_decref(_M0L6_2atmpS2149);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2147
  = moonbit_add_string(_M0L6_2atmpS2148, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2148);
  #line 548 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2147);
  #line 551 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S928,
  moonbit_string_t _M0L8filenameS925,
  int32_t _M0L5indexS919,
  struct _M0TWssbEu* _M0L14handle__resultS915,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS917
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS895;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS924;
  struct _M0TWEuQRPC15error5Error* _M0L1fS897;
  moonbit_string_t* _M0L5attrsS898;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS918;
  moonbit_string_t _M0L4nameS901;
  moonbit_string_t _M0L4nameS899;
  int32_t _M0L6_2atmpS2144;
  struct _M0TWEOs* _M0L5_2aitS903;
  struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__* _closure_2438;
  struct _M0TWEOc* _M0L6_2atmpS2135;
  struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__* _closure_2439;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2136;
  struct moonbit_result_0 _result_2440;
  #line 412 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S928);
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 419 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS924
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS925);
  if (_M0L7_2abindS924 == 0) {
    struct moonbit_result_0 _result_2431;
    if (_M0L7_2abindS924) {
      moonbit_decref(_M0L7_2abindS924);
    }
    moonbit_decref(_M0L17error__to__stringS917);
    moonbit_decref(_M0L14handle__resultS915);
    _result_2431.tag = 1;
    _result_2431.data.ok = 0;
    return _result_2431;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS926 =
      _M0L7_2abindS924;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS927 =
      _M0L7_2aSomeS926;
    _M0L10index__mapS895 = _M0L13_2aindex__mapS927;
    goto join_894;
  }
  join_894:;
  #line 421 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS918
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS895, _M0L5indexS919);
  if (_M0L7_2abindS918 == 0) {
    struct moonbit_result_0 _result_2433;
    if (_M0L7_2abindS918) {
      moonbit_decref(_M0L7_2abindS918);
    }
    moonbit_decref(_M0L17error__to__stringS917);
    moonbit_decref(_M0L14handle__resultS915);
    _result_2433.tag = 1;
    _result_2433.data.ok = 0;
    return _result_2433;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS920 = _M0L7_2abindS918;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS921 = _M0L7_2aSomeS920;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS922 = _M0L4_2axS921->$0;
    moonbit_string_t* _M0L8_2afieldS2185 = _M0L4_2axS921->$1;
    int32_t _M0L6_2acntS2347 = Moonbit_object_header(_M0L4_2axS921)->rc;
    moonbit_string_t* _M0L8_2aattrsS923;
    if (_M0L6_2acntS2347 > 1) {
      int32_t _M0L11_2anew__cntS2348 = _M0L6_2acntS2347 - 1;
      Moonbit_object_header(_M0L4_2axS921)->rc = _M0L11_2anew__cntS2348;
      moonbit_incref(_M0L8_2afieldS2185);
      moonbit_incref(_M0L4_2afS922);
    } else if (_M0L6_2acntS2347 == 1) {
      #line 419 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS921);
    }
    _M0L8_2aattrsS923 = _M0L8_2afieldS2185;
    _M0L1fS897 = _M0L4_2afS922;
    _M0L5attrsS898 = _M0L8_2aattrsS923;
    goto join_896;
  }
  join_896:;
  _M0L6_2atmpS2144 = Moonbit_array_length(_M0L5attrsS898);
  if (_M0L6_2atmpS2144 >= 1) {
    moonbit_string_t _M0L7_2anameS902 = (moonbit_string_t)_M0L5attrsS898[0];
    moonbit_incref(_M0L7_2anameS902);
    _M0L4nameS901 = _M0L7_2anameS902;
    goto join_900;
  } else {
    _M0L4nameS899 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_2434;
  join_900:;
  _M0L4nameS899 = _M0L4nameS901;
  joinlet_2434:;
  #line 422 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS903 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS898);
  while (1) {
    moonbit_string_t _M0L4attrS905;
    moonbit_string_t _M0L7_2abindS912;
    int32_t _M0L6_2atmpS2128;
    int64_t _M0L6_2atmpS2127;
    moonbit_incref(_M0L5_2aitS903);
    #line 424 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS912 = _M0MPB4Iter4nextGsE(_M0L5_2aitS903);
    if (_M0L7_2abindS912 == 0) {
      if (_M0L7_2abindS912) {
        moonbit_decref(_M0L7_2abindS912);
      }
      moonbit_decref(_M0L5_2aitS903);
    } else {
      moonbit_string_t _M0L7_2aSomeS913 = _M0L7_2abindS912;
      moonbit_string_t _M0L7_2aattrS914 = _M0L7_2aSomeS913;
      _M0L4attrS905 = _M0L7_2aattrS914;
      goto join_904;
    }
    goto joinlet_2436;
    join_904:;
    _M0L6_2atmpS2128 = Moonbit_array_length(_M0L4attrS905);
    _M0L6_2atmpS2127 = (int64_t)_M0L6_2atmpS2128;
    moonbit_incref(_M0L4attrS905);
    #line 425 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS905, 5, 0, _M0L6_2atmpS2127)
    ) {
      int32_t _M0L6_2atmpS2134 = _M0L4attrS905[0];
      int32_t _M0L4_2axS906 = _M0L6_2atmpS2134;
      if (_M0L4_2axS906 == 112) {
        int32_t _M0L6_2atmpS2133 = _M0L4attrS905[1];
        int32_t _M0L4_2axS907 = _M0L6_2atmpS2133;
        if (_M0L4_2axS907 == 97) {
          int32_t _M0L6_2atmpS2132 = _M0L4attrS905[2];
          int32_t _M0L4_2axS908 = _M0L6_2atmpS2132;
          if (_M0L4_2axS908 == 110) {
            int32_t _M0L6_2atmpS2131 = _M0L4attrS905[3];
            int32_t _M0L4_2axS909 = _M0L6_2atmpS2131;
            if (_M0L4_2axS909 == 105) {
              int32_t _M0L6_2atmpS2130 = _M0L4attrS905[4];
              int32_t _M0L4_2axS910;
              moonbit_decref(_M0L4attrS905);
              _M0L4_2axS910 = _M0L6_2atmpS2130;
              if (_M0L4_2axS910 == 99) {
                void* _M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2129;
                struct moonbit_result_0 _result_2437;
                moonbit_decref(_M0L17error__to__stringS917);
                moonbit_decref(_M0L14handle__resultS915);
                moonbit_decref(_M0L5_2aitS903);
                moonbit_decref(_M0L1fS897);
                _M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2129
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2129)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2129)->$0
                = _M0L4nameS899;
                _result_2437.tag = 0;
                _result_2437.data.err
                = _M0L116mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2129;
                return _result_2437;
              }
            } else {
              moonbit_decref(_M0L4attrS905);
            }
          } else {
            moonbit_decref(_M0L4attrS905);
          }
        } else {
          moonbit_decref(_M0L4attrS905);
        }
      } else {
        moonbit_decref(_M0L4attrS905);
      }
    } else {
      moonbit_decref(_M0L4attrS905);
    }
    continue;
    joinlet_2436:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS915);
  moonbit_incref(_M0L4nameS899);
  _closure_2438
  = (struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__*)moonbit_malloc(sizeof(struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__));
  Moonbit_object_header(_closure_2438)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__, $0) >> 2, 2, 0);
  _closure_2438->code
  = &_M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testC2141l432;
  _closure_2438->$0 = _M0L14handle__resultS915;
  _closure_2438->$1 = _M0L4nameS899;
  _M0L6_2atmpS2135 = (struct _M0TWEOc*)_closure_2438;
  _closure_2439
  = (struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__*)moonbit_malloc(sizeof(struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__));
  Moonbit_object_header(_closure_2439)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__, $0) >> 2, 3, 0);
  _closure_2439->code
  = &_M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testC2137l433;
  _closure_2439->$0 = _M0L17error__to__stringS917;
  _closure_2439->$1 = _M0L14handle__resultS915;
  _closure_2439->$2 = _M0L4nameS899;
  _M0L6_2atmpS2136 = (struct _M0TWRPC15error5ErrorEu*)_closure_2439;
  #line 430 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS897, _M0L6_2atmpS2135, _M0L6_2atmpS2136);
  _result_2440.tag = 1;
  _result_2440.data.ok = 1;
  return _result_2440;
}

int32_t _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testC2141l432(
  struct _M0TWEOc* _M0L6_2aenvS2142
) {
  struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__* _M0L14_2acasted__envS2143;
  moonbit_string_t _M0L4nameS899;
  struct _M0TWssbEu* _M0L8_2afieldS2187;
  int32_t _M0L6_2acntS2349;
  struct _M0TWssbEu* _M0L14handle__resultS915;
  #line 432 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2143
  = (struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2141__l432__*)_M0L6_2aenvS2142;
  _M0L4nameS899 = _M0L14_2acasted__envS2143->$1;
  _M0L8_2afieldS2187 = _M0L14_2acasted__envS2143->$0;
  _M0L6_2acntS2349 = Moonbit_object_header(_M0L14_2acasted__envS2143)->rc;
  if (_M0L6_2acntS2349 > 1) {
    int32_t _M0L11_2anew__cntS2350 = _M0L6_2acntS2349 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2143)->rc
    = _M0L11_2anew__cntS2350;
    moonbit_incref(_M0L4nameS899);
    moonbit_incref(_M0L8_2afieldS2187);
  } else if (_M0L6_2acntS2349 == 1) {
    #line 432 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2143);
  }
  _M0L14handle__resultS915 = _M0L8_2afieldS2187;
  #line 432 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS915->code(_M0L14handle__resultS915, _M0L4nameS899, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP36mulpjs4mulp28cli__runtime__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testC2137l433(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2138,
  void* _M0L3errS916
) {
  struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__* _M0L14_2acasted__envS2139;
  moonbit_string_t _M0L4nameS899;
  struct _M0TWssbEu* _M0L14handle__resultS915;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2189;
  int32_t _M0L6_2acntS2351;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS917;
  moonbit_string_t _M0L6_2atmpS2140;
  #line 433 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2139
  = (struct _M0R205_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fcli__runtime__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2137__l433__*)_M0L6_2aenvS2138;
  _M0L4nameS899 = _M0L14_2acasted__envS2139->$2;
  _M0L14handle__resultS915 = _M0L14_2acasted__envS2139->$1;
  _M0L8_2afieldS2189 = _M0L14_2acasted__envS2139->$0;
  _M0L6_2acntS2351 = Moonbit_object_header(_M0L14_2acasted__envS2139)->rc;
  if (_M0L6_2acntS2351 > 1) {
    int32_t _M0L11_2anew__cntS2352 = _M0L6_2acntS2351 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2139)->rc
    = _M0L11_2anew__cntS2352;
    moonbit_incref(_M0L4nameS899);
    moonbit_incref(_M0L14handle__resultS915);
    moonbit_incref(_M0L8_2afieldS2189);
  } else if (_M0L6_2acntS2351 == 1) {
    #line 433 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2139);
  }
  _M0L17error__to__stringS917 = _M0L8_2afieldS2189;
  #line 433 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2140
  = _M0L17error__to__stringS917->code(_M0L17error__to__stringS917, _M0L3errS916);
  #line 433 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS915->code(_M0L14handle__resultS915, _M0L4nameS899, _M0L6_2atmpS2140, 0);
  return 0;
}

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS889,
  struct _M0TWEOc* _M0L6on__okS890,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS887
) {
  void* _M0L11_2atry__errS885;
  struct moonbit_result_0 _tmp_2442;
  void* _M0L3errS886;
  #line 375 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  #line 382 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _tmp_2442 = _M0L1fS889->code(_M0L1fS889);
  if (_tmp_2442.tag) {
    int32_t const _M0L5_2aokS2125 = _tmp_2442.data.ok;
    moonbit_decref(_M0L7on__errS887);
  } else {
    void* const _M0L6_2aerrS2126 = _tmp_2442.data.err;
    moonbit_decref(_M0L6on__okS890);
    _M0L11_2atry__errS885 = _M0L6_2aerrS2126;
    goto join_884;
  }
  #line 382 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS890->code(_M0L6on__okS890);
  goto joinlet_2441;
  join_884:;
  _M0L3errS886 = _M0L11_2atry__errS885;
  #line 383 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS887->code(_M0L7on__errS887, _M0L3errS886);
  joinlet_2441:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S844;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS857;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS862;
  struct _M0TUsiE** _M0L6_2atmpS2124;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS869;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS870;
  moonbit_string_t _M0L6_2atmpS2123;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS871;
  int32_t _M0L7_2abindS872;
  int32_t _M0L2__S873;
  #line 193 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S844 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS857
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS862 = 0;
  _M0L6_2atmpS2124 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS869
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS869)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS869->$0 = _M0L6_2atmpS2124;
  _M0L16file__and__indexS869->$1 = 0;
  #line 282 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS870
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS857(_M0L57moonbit__test__driver__internal__get__cli__args__internalS857);
  #line 284 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2123 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS870, 1);
  #line 283 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS871
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS862(_M0L51moonbit__test__driver__internal__split__mbt__stringS862, _M0L6_2atmpS2123, 47);
  _M0L7_2abindS872 = _M0L10test__argsS871->$1;
  _M0L2__S873 = 0;
  while (1) {
    if (_M0L2__S873 < _M0L7_2abindS872) {
      moonbit_string_t* _M0L3bufS2122 = _M0L10test__argsS871->$0;
      moonbit_string_t _M0L3argS874 =
        (moonbit_string_t)_M0L3bufS2122[_M0L2__S873];
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS875;
      moonbit_string_t _M0L4fileS876;
      moonbit_string_t _M0L5rangeS877;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS878;
      moonbit_string_t _M0L6_2atmpS2120;
      int32_t _M0L5startS879;
      moonbit_string_t _M0L6_2atmpS2119;
      int32_t _M0L3endS880;
      int32_t _M0L1iS881;
      int32_t _M0L6_2atmpS2121;
      moonbit_incref(_M0L3argS874);
      #line 288 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS875
      = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS862(_M0L51moonbit__test__driver__internal__split__mbt__stringS862, _M0L3argS874, 58);
      moonbit_incref(_M0L16file__and__rangeS875);
      #line 289 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS876
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS875, 0);
      #line 290 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS877
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS875, 1);
      #line 291 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS878
      = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS862(_M0L51moonbit__test__driver__internal__split__mbt__stringS862, _M0L5rangeS877, 45);
      moonbit_incref(_M0L15start__and__endS878);
      #line 294 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2120
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS878, 0);
      #line 294 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L5startS879
      = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S844(_M0L45moonbit__test__driver__internal__parse__int__S844, _M0L6_2atmpS2120);
      #line 295 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2119
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS878, 1);
      #line 295 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L3endS880
      = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S844(_M0L45moonbit__test__driver__internal__parse__int__S844, _M0L6_2atmpS2119);
      _M0L1iS881 = _M0L5startS879;
      while (1) {
        if (_M0L1iS881 < _M0L3endS880) {
          struct _M0TUsiE* _M0L8_2atupleS2117;
          int32_t _M0L6_2atmpS2118;
          moonbit_incref(_M0L4fileS876);
          _M0L8_2atupleS2117
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2117)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2117->$0 = _M0L4fileS876;
          _M0L8_2atupleS2117->$1 = _M0L1iS881;
          moonbit_incref(_M0L16file__and__indexS869);
          #line 297 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS869, _M0L8_2atupleS2117);
          _M0L6_2atmpS2118 = _M0L1iS881 + 1;
          _M0L1iS881 = _M0L6_2atmpS2118;
          continue;
        } else {
          moonbit_decref(_M0L4fileS876);
        }
        break;
      }
      _M0L6_2atmpS2121 = _M0L2__S873 + 1;
      _M0L2__S873 = _M0L6_2atmpS2121;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS871);
    }
    break;
  }
  return _M0L16file__and__indexS869;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS862(
  int32_t _M0L6_2aenvS2098,
  moonbit_string_t _M0L1sS863,
  int32_t _M0L3sepS864
) {
  moonbit_string_t* _M0L6_2atmpS2116;
  struct _M0TPB5ArrayGsE* _M0L3resS865;
  struct _M0TPB8MutLocalGiE* _M0L1iS866;
  struct _M0TPB8MutLocalGiE* _M0L5startS867;
  int32_t _M0L3valS2111;
  int32_t _M0L6_2atmpS2112;
  #line 261 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2116 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS865
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS865)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS865->$0 = _M0L6_2atmpS2116;
  _M0L3resS865->$1 = 0;
  _M0L1iS866
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS866)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS866->$0 = 0;
  _M0L5startS867
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS867)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5startS867->$0 = 0;
  while (1) {
    int32_t _M0L3valS2099 = _M0L1iS866->$0;
    int32_t _M0L6_2atmpS2100 = Moonbit_array_length(_M0L1sS863);
    if (_M0L3valS2099 < _M0L6_2atmpS2100) {
      int32_t _M0L3valS2103 = _M0L1iS866->$0;
      int32_t _M0L6_2atmpS2102;
      int32_t _M0L6_2atmpS2101;
      int32_t _M0L3valS2110;
      int32_t _M0L6_2atmpS2109;
      if (
        _M0L3valS2103 < 0
        || _M0L3valS2103 >= Moonbit_array_length(_M0L1sS863)
      ) {
        #line 269 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2102 = _M0L1sS863[_M0L3valS2103];
      _M0L6_2atmpS2101 = _M0L6_2atmpS2102;
      if (_M0L6_2atmpS2101 == _M0L3sepS864) {
        int32_t _M0L3valS2105 = _M0L5startS867->$0;
        int32_t _M0L3valS2106 = _M0L1iS866->$0;
        moonbit_string_t _M0L6_2atmpS2104;
        int32_t _M0L3valS2108;
        int32_t _M0L6_2atmpS2107;
        moonbit_incref(_M0L1sS863);
        #line 270 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS2104
        = _M0MPC16string6String17unsafe__substring(_M0L1sS863, _M0L3valS2105, _M0L3valS2106);
        moonbit_incref(_M0L3resS865);
        #line 270 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS865, _M0L6_2atmpS2104);
        _M0L3valS2108 = _M0L1iS866->$0;
        _M0L6_2atmpS2107 = _M0L3valS2108 + 1;
        _M0L5startS867->$0 = _M0L6_2atmpS2107;
      }
      _M0L3valS2110 = _M0L1iS866->$0;
      _M0L6_2atmpS2109 = _M0L3valS2110 + 1;
      _M0L1iS866->$0 = _M0L6_2atmpS2109;
      continue;
    } else {
      moonbit_decref(_M0L1iS866);
    }
    break;
  }
  _M0L3valS2111 = _M0L5startS867->$0;
  _M0L6_2atmpS2112 = Moonbit_array_length(_M0L1sS863);
  if (_M0L3valS2111 < _M0L6_2atmpS2112) {
    int32_t _M0L3valS2114 = _M0L5startS867->$0;
    int32_t _M0L6_2atmpS2115;
    moonbit_string_t _M0L6_2atmpS2113;
    moonbit_decref(_M0L5startS867);
    _M0L6_2atmpS2115 = Moonbit_array_length(_M0L1sS863);
    #line 276 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS2113
    = _M0MPC16string6String17unsafe__substring(_M0L1sS863, _M0L3valS2114, _M0L6_2atmpS2115);
    moonbit_incref(_M0L3resS865);
    #line 276 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS865, _M0L6_2atmpS2113);
  } else {
    moonbit_decref(_M0L5startS867);
    moonbit_decref(_M0L1sS863);
  }
  return _M0L3resS865;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS857(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850
) {
  moonbit_bytes_t* _M0L3tmpS858;
  int32_t _M0L6_2atmpS2097;
  struct _M0TPB5ArrayGsE* _M0L3resS859;
  int32_t _M0L1iS860;
  #line 250 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  #line 253 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS858
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2097 = Moonbit_array_length(_M0L3tmpS858);
  #line 254 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS859 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2097);
  _M0L1iS860 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2093 = Moonbit_array_length(_M0L3tmpS858);
    if (_M0L1iS860 < _M0L6_2atmpS2093) {
      moonbit_bytes_t _M0L6_2atmpS2095;
      moonbit_string_t _M0L6_2atmpS2094;
      int32_t _M0L6_2atmpS2096;
      if (_M0L1iS860 < 0 || _M0L1iS860 >= Moonbit_array_length(_M0L3tmpS858)) {
        #line 256 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2095 = (moonbit_bytes_t)_M0L3tmpS858[_M0L1iS860];
      moonbit_incref(_M0L6_2atmpS2095);
      #line 256 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2094
      = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850, _M0L6_2atmpS2095);
      moonbit_incref(_M0L3resS859);
      #line 256 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS859, _M0L6_2atmpS2094);
      _M0L6_2atmpS2096 = _M0L1iS860 + 1;
      _M0L1iS860 = _M0L6_2atmpS2096;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS858);
    }
    break;
  }
  return _M0L3resS859;
}

moonbit_string_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS850(
  int32_t _M0L6_2aenvS2007,
  moonbit_bytes_t _M0L5bytesS851
) {
  struct _M0TPB13StringBuilder* _M0L3resS852;
  int32_t _M0L3lenS853;
  struct _M0TPB8MutLocalGiE* _M0L1iS854;
  #line 206 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  #line 209 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS852 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS853 = Moonbit_array_length(_M0L5bytesS851);
  _M0L1iS854
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS854)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS854->$0 = 0;
  while (1) {
    int32_t _M0L3valS2008 = _M0L1iS854->$0;
    if (_M0L3valS2008 < _M0L3lenS853) {
      int32_t _M0L3valS2092 = _M0L1iS854->$0;
      int32_t _M0L6_2atmpS2091;
      int32_t _M0L6_2atmpS2090;
      struct _M0TPB8MutLocalGiE* _M0L1cS855;
      int32_t _M0L3valS2009;
      if (
        _M0L3valS2092 < 0
        || _M0L3valS2092 >= Moonbit_array_length(_M0L5bytesS851)
      ) {
        #line 213 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2091 = _M0L5bytesS851[_M0L3valS2092];
      _M0L6_2atmpS2090 = (int32_t)_M0L6_2atmpS2091;
      _M0L1cS855
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L1cS855)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
      _M0L1cS855->$0 = _M0L6_2atmpS2090;
      _M0L3valS2009 = _M0L1cS855->$0;
      if (_M0L3valS2009 < 128) {
        int32_t _M0L3valS2011 = _M0L1cS855->$0;
        int32_t _M0L6_2atmpS2010;
        int32_t _M0L3valS2013;
        int32_t _M0L6_2atmpS2012;
        moonbit_decref(_M0L1cS855);
        _M0L6_2atmpS2010 = _M0L3valS2011;
        moonbit_incref(_M0L3resS852);
        #line 215 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS852, _M0L6_2atmpS2010);
        _M0L3valS2013 = _M0L1iS854->$0;
        _M0L6_2atmpS2012 = _M0L3valS2013 + 1;
        _M0L1iS854->$0 = _M0L6_2atmpS2012;
      } else {
        int32_t _M0L3valS2014 = _M0L1cS855->$0;
        if (_M0L3valS2014 < 224) {
          int32_t _M0L3valS2016 = _M0L1iS854->$0;
          int32_t _M0L6_2atmpS2015 = _M0L3valS2016 + 1;
          int32_t _M0L3valS2025;
          int32_t _M0L6_2atmpS2024;
          int32_t _M0L6_2atmpS2018;
          int32_t _M0L3valS2023;
          int32_t _M0L6_2atmpS2022;
          int32_t _M0L6_2atmpS2021;
          int32_t _M0L6_2atmpS2020;
          int32_t _M0L6_2atmpS2019;
          int32_t _M0L6_2atmpS2017;
          int32_t _M0L3valS2027;
          int32_t _M0L6_2atmpS2026;
          int32_t _M0L3valS2029;
          int32_t _M0L6_2atmpS2028;
          if (_M0L6_2atmpS2015 >= _M0L3lenS853) {
            moonbit_decref(_M0L1cS855);
            moonbit_decref(_M0L1iS854);
            moonbit_decref(_M0L5bytesS851);
            break;
          }
          _M0L3valS2025 = _M0L1cS855->$0;
          _M0L6_2atmpS2024 = _M0L3valS2025 & 31;
          _M0L6_2atmpS2018 = _M0L6_2atmpS2024 << 6;
          _M0L3valS2023 = _M0L1iS854->$0;
          _M0L6_2atmpS2022 = _M0L3valS2023 + 1;
          if (
            _M0L6_2atmpS2022 < 0
            || _M0L6_2atmpS2022 >= Moonbit_array_length(_M0L5bytesS851)
          ) {
            #line 221 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2021 = _M0L5bytesS851[_M0L6_2atmpS2022];
          _M0L6_2atmpS2020 = (int32_t)_M0L6_2atmpS2021;
          _M0L6_2atmpS2019 = _M0L6_2atmpS2020 & 63;
          _M0L6_2atmpS2017 = _M0L6_2atmpS2018 | _M0L6_2atmpS2019;
          _M0L1cS855->$0 = _M0L6_2atmpS2017;
          _M0L3valS2027 = _M0L1cS855->$0;
          moonbit_decref(_M0L1cS855);
          _M0L6_2atmpS2026 = _M0L3valS2027;
          moonbit_incref(_M0L3resS852);
          #line 222 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS852, _M0L6_2atmpS2026);
          _M0L3valS2029 = _M0L1iS854->$0;
          _M0L6_2atmpS2028 = _M0L3valS2029 + 2;
          _M0L1iS854->$0 = _M0L6_2atmpS2028;
        } else {
          int32_t _M0L3valS2030 = _M0L1cS855->$0;
          if (_M0L3valS2030 < 240) {
            int32_t _M0L3valS2032 = _M0L1iS854->$0;
            int32_t _M0L6_2atmpS2031 = _M0L3valS2032 + 2;
            int32_t _M0L3valS2048;
            int32_t _M0L6_2atmpS2047;
            int32_t _M0L6_2atmpS2040;
            int32_t _M0L3valS2046;
            int32_t _M0L6_2atmpS2045;
            int32_t _M0L6_2atmpS2044;
            int32_t _M0L6_2atmpS2043;
            int32_t _M0L6_2atmpS2042;
            int32_t _M0L6_2atmpS2041;
            int32_t _M0L6_2atmpS2034;
            int32_t _M0L3valS2039;
            int32_t _M0L6_2atmpS2038;
            int32_t _M0L6_2atmpS2037;
            int32_t _M0L6_2atmpS2036;
            int32_t _M0L6_2atmpS2035;
            int32_t _M0L6_2atmpS2033;
            int32_t _M0L3valS2050;
            int32_t _M0L6_2atmpS2049;
            int32_t _M0L3valS2052;
            int32_t _M0L6_2atmpS2051;
            if (_M0L6_2atmpS2031 >= _M0L3lenS853) {
              moonbit_decref(_M0L1cS855);
              moonbit_decref(_M0L1iS854);
              moonbit_decref(_M0L5bytesS851);
              break;
            }
            _M0L3valS2048 = _M0L1cS855->$0;
            _M0L6_2atmpS2047 = _M0L3valS2048 & 15;
            _M0L6_2atmpS2040 = _M0L6_2atmpS2047 << 12;
            _M0L3valS2046 = _M0L1iS854->$0;
            _M0L6_2atmpS2045 = _M0L3valS2046 + 1;
            if (
              _M0L6_2atmpS2045 < 0
              || _M0L6_2atmpS2045 >= Moonbit_array_length(_M0L5bytesS851)
            ) {
              #line 229 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2044 = _M0L5bytesS851[_M0L6_2atmpS2045];
            _M0L6_2atmpS2043 = (int32_t)_M0L6_2atmpS2044;
            _M0L6_2atmpS2042 = _M0L6_2atmpS2043 & 63;
            _M0L6_2atmpS2041 = _M0L6_2atmpS2042 << 6;
            _M0L6_2atmpS2034 = _M0L6_2atmpS2040 | _M0L6_2atmpS2041;
            _M0L3valS2039 = _M0L1iS854->$0;
            _M0L6_2atmpS2038 = _M0L3valS2039 + 2;
            if (
              _M0L6_2atmpS2038 < 0
              || _M0L6_2atmpS2038 >= Moonbit_array_length(_M0L5bytesS851)
            ) {
              #line 230 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2037 = _M0L5bytesS851[_M0L6_2atmpS2038];
            _M0L6_2atmpS2036 = (int32_t)_M0L6_2atmpS2037;
            _M0L6_2atmpS2035 = _M0L6_2atmpS2036 & 63;
            _M0L6_2atmpS2033 = _M0L6_2atmpS2034 | _M0L6_2atmpS2035;
            _M0L1cS855->$0 = _M0L6_2atmpS2033;
            _M0L3valS2050 = _M0L1cS855->$0;
            moonbit_decref(_M0L1cS855);
            _M0L6_2atmpS2049 = _M0L3valS2050;
            moonbit_incref(_M0L3resS852);
            #line 231 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS852, _M0L6_2atmpS2049);
            _M0L3valS2052 = _M0L1iS854->$0;
            _M0L6_2atmpS2051 = _M0L3valS2052 + 3;
            _M0L1iS854->$0 = _M0L6_2atmpS2051;
          } else {
            int32_t _M0L3valS2054 = _M0L1iS854->$0;
            int32_t _M0L6_2atmpS2053 = _M0L3valS2054 + 3;
            int32_t _M0L3valS2077;
            int32_t _M0L6_2atmpS2076;
            int32_t _M0L6_2atmpS2069;
            int32_t _M0L3valS2075;
            int32_t _M0L6_2atmpS2074;
            int32_t _M0L6_2atmpS2073;
            int32_t _M0L6_2atmpS2072;
            int32_t _M0L6_2atmpS2071;
            int32_t _M0L6_2atmpS2070;
            int32_t _M0L6_2atmpS2062;
            int32_t _M0L3valS2068;
            int32_t _M0L6_2atmpS2067;
            int32_t _M0L6_2atmpS2066;
            int32_t _M0L6_2atmpS2065;
            int32_t _M0L6_2atmpS2064;
            int32_t _M0L6_2atmpS2063;
            int32_t _M0L6_2atmpS2056;
            int32_t _M0L3valS2061;
            int32_t _M0L6_2atmpS2060;
            int32_t _M0L6_2atmpS2059;
            int32_t _M0L6_2atmpS2058;
            int32_t _M0L6_2atmpS2057;
            int32_t _M0L6_2atmpS2055;
            int32_t _M0L3valS2079;
            int32_t _M0L6_2atmpS2078;
            int32_t _M0L3valS2083;
            int32_t _M0L6_2atmpS2082;
            int32_t _M0L6_2atmpS2081;
            int32_t _M0L6_2atmpS2080;
            int32_t _M0L3valS2087;
            int32_t _M0L6_2atmpS2086;
            int32_t _M0L6_2atmpS2085;
            int32_t _M0L6_2atmpS2084;
            int32_t _M0L3valS2089;
            int32_t _M0L6_2atmpS2088;
            if (_M0L6_2atmpS2053 >= _M0L3lenS853) {
              moonbit_decref(_M0L1cS855);
              moonbit_decref(_M0L1iS854);
              moonbit_decref(_M0L5bytesS851);
              break;
            }
            _M0L3valS2077 = _M0L1cS855->$0;
            _M0L6_2atmpS2076 = _M0L3valS2077 & 7;
            _M0L6_2atmpS2069 = _M0L6_2atmpS2076 << 18;
            _M0L3valS2075 = _M0L1iS854->$0;
            _M0L6_2atmpS2074 = _M0L3valS2075 + 1;
            if (
              _M0L6_2atmpS2074 < 0
              || _M0L6_2atmpS2074 >= Moonbit_array_length(_M0L5bytesS851)
            ) {
              #line 238 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2073 = _M0L5bytesS851[_M0L6_2atmpS2074];
            _M0L6_2atmpS2072 = (int32_t)_M0L6_2atmpS2073;
            _M0L6_2atmpS2071 = _M0L6_2atmpS2072 & 63;
            _M0L6_2atmpS2070 = _M0L6_2atmpS2071 << 12;
            _M0L6_2atmpS2062 = _M0L6_2atmpS2069 | _M0L6_2atmpS2070;
            _M0L3valS2068 = _M0L1iS854->$0;
            _M0L6_2atmpS2067 = _M0L3valS2068 + 2;
            if (
              _M0L6_2atmpS2067 < 0
              || _M0L6_2atmpS2067 >= Moonbit_array_length(_M0L5bytesS851)
            ) {
              #line 239 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2066 = _M0L5bytesS851[_M0L6_2atmpS2067];
            _M0L6_2atmpS2065 = (int32_t)_M0L6_2atmpS2066;
            _M0L6_2atmpS2064 = _M0L6_2atmpS2065 & 63;
            _M0L6_2atmpS2063 = _M0L6_2atmpS2064 << 6;
            _M0L6_2atmpS2056 = _M0L6_2atmpS2062 | _M0L6_2atmpS2063;
            _M0L3valS2061 = _M0L1iS854->$0;
            _M0L6_2atmpS2060 = _M0L3valS2061 + 3;
            if (
              _M0L6_2atmpS2060 < 0
              || _M0L6_2atmpS2060 >= Moonbit_array_length(_M0L5bytesS851)
            ) {
              #line 240 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2059 = _M0L5bytesS851[_M0L6_2atmpS2060];
            _M0L6_2atmpS2058 = (int32_t)_M0L6_2atmpS2059;
            _M0L6_2atmpS2057 = _M0L6_2atmpS2058 & 63;
            _M0L6_2atmpS2055 = _M0L6_2atmpS2056 | _M0L6_2atmpS2057;
            _M0L1cS855->$0 = _M0L6_2atmpS2055;
            _M0L3valS2079 = _M0L1cS855->$0;
            _M0L6_2atmpS2078 = _M0L3valS2079 - 65536;
            _M0L1cS855->$0 = _M0L6_2atmpS2078;
            _M0L3valS2083 = _M0L1cS855->$0;
            _M0L6_2atmpS2082 = _M0L3valS2083 >> 10;
            _M0L6_2atmpS2081 = _M0L6_2atmpS2082 + 55296;
            _M0L6_2atmpS2080 = _M0L6_2atmpS2081;
            moonbit_incref(_M0L3resS852);
            #line 242 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS852, _M0L6_2atmpS2080);
            _M0L3valS2087 = _M0L1cS855->$0;
            moonbit_decref(_M0L1cS855);
            _M0L6_2atmpS2086 = _M0L3valS2087 & 1023;
            _M0L6_2atmpS2085 = _M0L6_2atmpS2086 + 56320;
            _M0L6_2atmpS2084 = _M0L6_2atmpS2085;
            moonbit_incref(_M0L3resS852);
            #line 243 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS852, _M0L6_2atmpS2084);
            _M0L3valS2089 = _M0L1iS854->$0;
            _M0L6_2atmpS2088 = _M0L3valS2089 + 4;
            _M0L1iS854->$0 = _M0L6_2atmpS2088;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS854);
      moonbit_decref(_M0L5bytesS851);
    }
    break;
  }
  #line 247 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS852);
}

int32_t _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S844(
  int32_t _M0L6_2aenvS2000,
  moonbit_string_t _M0L1sS845
) {
  struct _M0TPB8MutLocalGiE* _M0L3resS846;
  int32_t _M0L3lenS847;
  int32_t _M0L1iS848;
  int32_t _result_2449;
  #line 197 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS846
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L3resS846)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L3resS846->$0 = 0;
  _M0L3lenS847 = Moonbit_array_length(_M0L1sS845);
  _M0L1iS848 = 0;
  while (1) {
    if (_M0L1iS848 < _M0L3lenS847) {
      int32_t _M0L3valS2005 = _M0L3resS846->$0;
      int32_t _M0L6_2atmpS2002 = _M0L3valS2005 * 10;
      int32_t _M0L6_2atmpS2004;
      int32_t _M0L6_2atmpS2003;
      int32_t _M0L6_2atmpS2001;
      int32_t _M0L6_2atmpS2006;
      if (_M0L1iS848 < 0 || _M0L1iS848 >= Moonbit_array_length(_M0L1sS845)) {
        #line 201 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2004 = _M0L1sS845[_M0L1iS848];
      _M0L6_2atmpS2003 = _M0L6_2atmpS2004 - 48;
      _M0L6_2atmpS2001 = _M0L6_2atmpS2002 + _M0L6_2atmpS2003;
      _M0L3resS846->$0 = _M0L6_2atmpS2001;
      _M0L6_2atmpS2006 = _M0L1iS848 + 1;
      _M0L1iS848 = _M0L6_2atmpS2006;
      continue;
    } else {
      moonbit_decref(_M0L1sS845);
    }
    break;
  }
  _result_2449 = _M0L3resS846->$0;
  moonbit_decref(_M0L3resS846);
  return _result_2449;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S824,
  moonbit_string_t _M0L12_2adiscard__S825,
  int32_t _M0L12_2adiscard__S826,
  struct _M0TWssbEu* _M0L12_2adiscard__S827,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S828
) {
  struct moonbit_result_0 _result_2450;
  #line 34 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S828);
  moonbit_decref(_M0L12_2adiscard__S827);
  moonbit_decref(_M0L12_2adiscard__S825);
  moonbit_decref(_M0L12_2adiscard__S824);
  _result_2450.tag = 1;
  _result_2450.data.ok = 0;
  return _result_2450;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S829,
  moonbit_string_t _M0L12_2adiscard__S830,
  int32_t _M0L12_2adiscard__S831,
  struct _M0TWssbEu* _M0L12_2adiscard__S832,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S833
) {
  struct moonbit_result_0 _result_2451;
  #line 34 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S833);
  moonbit_decref(_M0L12_2adiscard__S832);
  moonbit_decref(_M0L12_2adiscard__S830);
  moonbit_decref(_M0L12_2adiscard__S829);
  _result_2451.tag = 1;
  _result_2451.data.ok = 0;
  return _result_2451;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S834,
  moonbit_string_t _M0L12_2adiscard__S835,
  int32_t _M0L12_2adiscard__S836,
  struct _M0TWssbEu* _M0L12_2adiscard__S837,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S838
) {
  struct moonbit_result_0 _result_2452;
  #line 34 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S838);
  moonbit_decref(_M0L12_2adiscard__S837);
  moonbit_decref(_M0L12_2adiscard__S835);
  moonbit_decref(_M0L12_2adiscard__S834);
  _result_2452.tag = 1;
  _result_2452.data.ok = 0;
  return _result_2452;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp28cli__runtime__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S839,
  moonbit_string_t _M0L12_2adiscard__S840,
  int32_t _M0L12_2adiscard__S841,
  struct _M0TWssbEu* _M0L12_2adiscard__S842,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S843
) {
  struct moonbit_result_0 _result_2453;
  #line 34 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S843);
  moonbit_decref(_M0L12_2adiscard__S842);
  moonbit_decref(_M0L12_2adiscard__S840);
  moonbit_decref(_M0L12_2adiscard__S839);
  _result_2453.tag = 1;
  _result_2453.data.ok = 0;
  return _result_2453;
}

int32_t _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp28cli__runtime__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S823
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S823);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__5(
  
) {
  moonbit_string_t* _M0L6_2atmpS1999;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1998;
  moonbit_string_t _M0L6_2atmpS1997;
  struct _M0TPB4Show _M0L6_2atmpS1990;
  moonbit_string_t _M0L6_2atmpS1993;
  moonbit_string_t _M0L6_2atmpS1994;
  moonbit_string_t _M0L6_2atmpS1995;
  moonbit_string_t _M0L6_2atmpS1996;
  moonbit_string_t* _M0L6_2atmpS1992;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1991;
  #line 52 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1999 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1999[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1999[1] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1998
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1998)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1998->$0 = _M0L6_2atmpS1999;
  _M0L6_2atmpS1998->$1 = 2;
  #line 54 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1997
  = _M0FP36mulpjs4mulp12cli__runtime19compatibility__hint((moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS1998);
  _M0L6_2atmpS1990
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1997
  };
  _M0L6_2atmpS1993 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS1994 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS1995 = 0;
  _M0L6_2atmpS1996 = 0;
  _M0L6_2atmpS1992 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1992[0] = _M0L6_2atmpS1993;
  _M0L6_2atmpS1992[1] = _M0L6_2atmpS1994;
  _M0L6_2atmpS1992[2] = _M0L6_2atmpS1995;
  _M0L6_2atmpS1992[3] = _M0L6_2atmpS1996;
  _M0L6_2atmpS1991
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1991)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1991->$0 = _M0L6_2atmpS1992;
  _M0L6_2atmpS1991->$1 = 4;
  #line 53 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1990, (moonbit_string_t)moonbit_string_literal_14.data, (moonbit_string_t)moonbit_string_literal_15.data, _M0L6_2atmpS1991);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__4(
  
) {
  moonbit_string_t* _M0L6_2atmpS1963;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1962;
  moonbit_string_t _M0L6_2atmpS1961;
  moonbit_string_t _M0L6_2atmpS1960;
  struct _M0TPB4Show _M0L6_2atmpS1953;
  moonbit_string_t _M0L6_2atmpS1956;
  moonbit_string_t _M0L6_2atmpS1957;
  moonbit_string_t _M0L6_2atmpS1958;
  moonbit_string_t _M0L6_2atmpS1959;
  moonbit_string_t* _M0L6_2atmpS1955;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1954;
  struct moonbit_result_0 _tmp_2454;
  moonbit_string_t* _M0L6_2atmpS1976;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1975;
  moonbit_string_t _M0L6_2atmpS1974;
  moonbit_string_t _M0L6_2atmpS1973;
  struct _M0TPB4Show _M0L6_2atmpS1966;
  moonbit_string_t _M0L6_2atmpS1969;
  moonbit_string_t _M0L6_2atmpS1970;
  moonbit_string_t _M0L6_2atmpS1971;
  moonbit_string_t _M0L6_2atmpS1972;
  moonbit_string_t* _M0L6_2atmpS1968;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1967;
  struct moonbit_result_0 _tmp_2456;
  moonbit_string_t* _M0L6_2atmpS1989;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1988;
  moonbit_string_t _M0L7_2abindS822;
  int32_t _M0L6_2atmpS1986;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1987;
  struct _M0TPB4Show _M0L6_2atmpS1979;
  moonbit_string_t _M0L6_2atmpS1982;
  moonbit_string_t _M0L6_2atmpS1983;
  moonbit_string_t _M0L6_2atmpS1984;
  moonbit_string_t _M0L6_2atmpS1985;
  moonbit_string_t* _M0L6_2atmpS1981;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1980;
  #line 45 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1963 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1963[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1963[1] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1962
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1962)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1962->$0 = _M0L6_2atmpS1963;
  _M0L6_2atmpS1962->$1 = 2;
  #line 46 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1961
  = _M0FP36mulpjs4mulp12cli__runtime27select__compatible__version(_M0L6_2atmpS1962, (moonbit_string_t)moonbit_string_literal_16.data);
  #line 46 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1960 = _M0MPC16option6Option6unwrapGsE(_M0L6_2atmpS1961);
  _M0L6_2atmpS1953
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1960
  };
  _M0L6_2atmpS1956 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS1957 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS1958 = 0;
  _M0L6_2atmpS1959 = 0;
  _M0L6_2atmpS1955 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1955[0] = _M0L6_2atmpS1956;
  _M0L6_2atmpS1955[1] = _M0L6_2atmpS1957;
  _M0L6_2atmpS1955[2] = _M0L6_2atmpS1958;
  _M0L6_2atmpS1955[3] = _M0L6_2atmpS1959;
  _M0L6_2atmpS1954
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1954)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1954->$0 = _M0L6_2atmpS1955;
  _M0L6_2atmpS1954->$1 = 4;
  #line 46 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _tmp_2454
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1953, (moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_19.data, _M0L6_2atmpS1954);
  if (_tmp_2454.tag) {
    int32_t const _M0L5_2aokS1964 = _tmp_2454.data.ok;
  } else {
    void* const _M0L6_2aerrS1965 = _tmp_2454.data.err;
    struct moonbit_result_0 _result_2455;
    _result_2455.tag = 0;
    _result_2455.data.err = _M0L6_2aerrS1965;
    return _result_2455;
  }
  _M0L6_2atmpS1976 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1976[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1976[1] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS1975
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1975)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1975->$0 = _M0L6_2atmpS1976;
  _M0L6_2atmpS1975->$1 = 2;
  #line 47 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1974
  = _M0FP36mulpjs4mulp12cli__runtime27select__compatible__version(_M0L6_2atmpS1975, (moonbit_string_t)moonbit_string_literal_11.data);
  #line 47 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1973 = _M0MPC16option6Option6unwrapGsE(_M0L6_2atmpS1974);
  _M0L6_2atmpS1966
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1973
  };
  _M0L6_2atmpS1969 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS1970 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS1971 = 0;
  _M0L6_2atmpS1972 = 0;
  _M0L6_2atmpS1968 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1968[0] = _M0L6_2atmpS1969;
  _M0L6_2atmpS1968[1] = _M0L6_2atmpS1970;
  _M0L6_2atmpS1968[2] = _M0L6_2atmpS1971;
  _M0L6_2atmpS1968[3] = _M0L6_2atmpS1972;
  _M0L6_2atmpS1967
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1967)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1967->$0 = _M0L6_2atmpS1968;
  _M0L6_2atmpS1967->$1 = 4;
  #line 47 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _tmp_2456
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1966, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS1967);
  if (_tmp_2456.tag) {
    int32_t const _M0L5_2aokS1977 = _tmp_2456.data.ok;
  } else {
    void* const _M0L6_2aerrS1978 = _tmp_2456.data.err;
    struct moonbit_result_0 _result_2457;
    _result_2457.tag = 0;
    _result_2457.data.err = _M0L6_2aerrS1978;
    return _result_2457;
  }
  _M0L6_2atmpS1989 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1989[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1988
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1988)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1988->$0 = _M0L6_2atmpS1989;
  _M0L6_2atmpS1988->$1 = 1;
  #line 48 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L7_2abindS822
  = _M0FP36mulpjs4mulp12cli__runtime27select__compatible__version(_M0L6_2atmpS1988, (moonbit_string_t)moonbit_string_literal_24.data);
  _M0L6_2atmpS1986 = _M0L7_2abindS822 == 0;
  if (_M0L7_2abindS822) {
    moonbit_decref(_M0L7_2abindS822);
  }
  _M0L14_2aboxed__selfS1987
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS1987)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS1987->$0 = _M0L6_2atmpS1986;
  _M0L6_2atmpS1979
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS1987
  };
  _M0L6_2atmpS1982 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS1983 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS1984 = 0;
  _M0L6_2atmpS1985 = 0;
  _M0L6_2atmpS1981 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1981[0] = _M0L6_2atmpS1982;
  _M0L6_2atmpS1981[1] = _M0L6_2atmpS1983;
  _M0L6_2atmpS1981[2] = _M0L6_2atmpS1984;
  _M0L6_2atmpS1981[3] = _M0L6_2atmpS1985;
  _M0L6_2atmpS1980
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1980)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1980->$0 = _M0L6_2atmpS1981;
  _M0L6_2atmpS1980->$1 = 4;
  #line 48 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1979, (moonbit_string_t)moonbit_string_literal_27.data, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS1980);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__3(
  
) {
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1932;
  struct _M0TPB4Show _M0L6_2atmpS1925;
  moonbit_string_t _M0L6_2atmpS1928;
  moonbit_string_t _M0L6_2atmpS1929;
  moonbit_string_t _M0L6_2atmpS1930;
  moonbit_string_t _M0L6_2atmpS1931;
  moonbit_string_t* _M0L6_2atmpS1927;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1926;
  struct moonbit_result_0 _tmp_2458;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1942;
  struct _M0TPB4Show _M0L6_2atmpS1935;
  moonbit_string_t _M0L6_2atmpS1938;
  moonbit_string_t _M0L6_2atmpS1939;
  moonbit_string_t _M0L6_2atmpS1940;
  moonbit_string_t _M0L6_2atmpS1941;
  moonbit_string_t* _M0L6_2atmpS1937;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1936;
  struct moonbit_result_0 _tmp_2460;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1952;
  struct _M0TPB4Show _M0L6_2atmpS1945;
  moonbit_string_t _M0L6_2atmpS1948;
  moonbit_string_t _M0L6_2atmpS1949;
  moonbit_string_t _M0L6_2atmpS1950;
  moonbit_string_t _M0L6_2atmpS1951;
  moonbit_string_t* _M0L6_2atmpS1947;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1946;
  #line 29 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  #line 31 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1932
  = _M0FP36mulpjs4mulp12cli__runtime16cli__completions((moonbit_string_t)moonbit_string_literal_29.data);
  _M0L6_2atmpS1925
  = (struct _M0TPB4Show){
    _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1932
  };
  _M0L6_2atmpS1928 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS1929 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS1930 = 0;
  _M0L6_2atmpS1931 = 0;
  _M0L6_2atmpS1927 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1927[0] = _M0L6_2atmpS1928;
  _M0L6_2atmpS1927[1] = _M0L6_2atmpS1929;
  _M0L6_2atmpS1927[2] = _M0L6_2atmpS1930;
  _M0L6_2atmpS1927[3] = _M0L6_2atmpS1931;
  _M0L6_2atmpS1926
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1926)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1926->$0 = _M0L6_2atmpS1927;
  _M0L6_2atmpS1926->$1 = 4;
  #line 30 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _tmp_2458
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1925, (moonbit_string_t)moonbit_string_literal_32.data, (moonbit_string_t)moonbit_string_literal_33.data, _M0L6_2atmpS1926);
  if (_tmp_2458.tag) {
    int32_t const _M0L5_2aokS1933 = _tmp_2458.data.ok;
  } else {
    void* const _M0L6_2aerrS1934 = _tmp_2458.data.err;
    struct moonbit_result_0 _result_2459;
    _result_2459.tag = 0;
    _result_2459.data.err = _M0L6_2aerrS1934;
    return _result_2459;
  }
  #line 35 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1942
  = _M0FP36mulpjs4mulp12cli__runtime16cli__completions((moonbit_string_t)moonbit_string_literal_34.data);
  _M0L6_2atmpS1935
  = (struct _M0TPB4Show){
    _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1942
  };
  _M0L6_2atmpS1938 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS1939 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS1940 = 0;
  _M0L6_2atmpS1941 = 0;
  _M0L6_2atmpS1937 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1937[0] = _M0L6_2atmpS1938;
  _M0L6_2atmpS1937[1] = _M0L6_2atmpS1939;
  _M0L6_2atmpS1937[2] = _M0L6_2atmpS1940;
  _M0L6_2atmpS1937[3] = _M0L6_2atmpS1941;
  _M0L6_2atmpS1936
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1936)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1936->$0 = _M0L6_2atmpS1937;
  _M0L6_2atmpS1936->$1 = 4;
  #line 34 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _tmp_2460
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1935, (moonbit_string_t)moonbit_string_literal_37.data, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS1936);
  if (_tmp_2460.tag) {
    int32_t const _M0L5_2aokS1943 = _tmp_2460.data.ok;
  } else {
    void* const _M0L6_2aerrS1944 = _tmp_2460.data.err;
    struct moonbit_result_0 _result_2461;
    _result_2461.tag = 0;
    _result_2461.data.err = _M0L6_2aerrS1944;
    return _result_2461;
  }
  #line 39 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1952
  = _M0FP36mulpjs4mulp12cli__runtime16cli__completions((moonbit_string_t)moonbit_string_literal_39.data);
  _M0L6_2atmpS1945
  = (struct _M0TPB4Show){
    _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1952
  };
  _M0L6_2atmpS1948 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS1949 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS1950 = 0;
  _M0L6_2atmpS1951 = 0;
  _M0L6_2atmpS1947 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1947[0] = _M0L6_2atmpS1948;
  _M0L6_2atmpS1947[1] = _M0L6_2atmpS1949;
  _M0L6_2atmpS1947[2] = _M0L6_2atmpS1950;
  _M0L6_2atmpS1947[3] = _M0L6_2atmpS1951;
  _M0L6_2atmpS1946
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1946)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1946->$0 = _M0L6_2atmpS1947;
  _M0L6_2atmpS1946->$1 = 4;
  #line 38 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1945, (moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_43.data, _M0L6_2atmpS1946);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__2(
  
) {
  moonbit_string_t _M0L6_2atmpS1924;
  struct _M0TPB4Show _M0L6_2atmpS1917;
  moonbit_string_t _M0L6_2atmpS1920;
  moonbit_string_t _M0L6_2atmpS1921;
  moonbit_string_t _M0L6_2atmpS1922;
  moonbit_string_t _M0L6_2atmpS1923;
  moonbit_string_t* _M0L6_2atmpS1919;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1918;
  #line 21 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  #line 23 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1924
  = _M0FP36mulpjs4mulp12cli__runtime9cli__help((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L6_2atmpS1917
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1924
  };
  _M0L6_2atmpS1920 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS1921 = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS1922 = 0;
  _M0L6_2atmpS1923 = 0;
  _M0L6_2atmpS1919 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1919[0] = _M0L6_2atmpS1920;
  _M0L6_2atmpS1919[1] = _M0L6_2atmpS1921;
  _M0L6_2atmpS1919[2] = _M0L6_2atmpS1922;
  _M0L6_2atmpS1919[3] = _M0L6_2atmpS1923;
  _M0L6_2atmpS1918
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1918)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1918->$0 = _M0L6_2atmpS1919;
  _M0L6_2atmpS1918->$1 = 4;
  #line 22 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1917, (moonbit_string_t)moonbit_string_literal_46.data, (moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS1918);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__1(
  
) {
  moonbit_string_t _M0L6_2atmpS1916;
  void* _M0L4SomeS1914;
  void* _M0L4NoneS1915;
  struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0L7runtimeS821;
  moonbit_string_t _M0L6_2atmpS1913;
  moonbit_string_t _M0L6_2atmpS1912;
  struct _M0TPB4Show _M0L6_2atmpS1905;
  moonbit_string_t _M0L6_2atmpS1908;
  moonbit_string_t _M0L6_2atmpS1909;
  moonbit_string_t _M0L6_2atmpS1910;
  moonbit_string_t _M0L6_2atmpS1911;
  moonbit_string_t* _M0L6_2atmpS1907;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1906;
  #line 12 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1916 = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L4SomeS1914
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGOsE4Some));
  Moonbit_object_header(_M0L4SomeS1914)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGOsE4Some, $0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGOsE4Some*)_M0L4SomeS1914)->$0
  = _M0L6_2atmpS1916;
  _M0L4NoneS1915
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  #line 13 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L7runtimeS821
  = _M0FP36mulpjs4mulp12cli__runtime12cli__runtime((moonbit_string_t)moonbit_string_literal_49.data, _M0L4SomeS1914, _M0L4NoneS1915);
  #line 17 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1913
  = _M0MP36mulpjs4mulp12cli__runtime10CliRuntime11entry__file(_M0L7runtimeS821);
  #line 17 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1912 = _M0MPC16option6Option6unwrapGsE(_M0L6_2atmpS1913);
  _M0L6_2atmpS1905
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1912
  };
  _M0L6_2atmpS1908 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS1909 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS1910 = 0;
  _M0L6_2atmpS1911 = 0;
  _M0L6_2atmpS1907 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1907[0] = _M0L6_2atmpS1908;
  _M0L6_2atmpS1907[1] = _M0L6_2atmpS1909;
  _M0L6_2atmpS1907[2] = _M0L6_2atmpS1910;
  _M0L6_2atmpS1907[3] = _M0L6_2atmpS1911;
  _M0L6_2atmpS1906
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1906)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1906->$0 = _M0L6_2atmpS1907;
  _M0L6_2atmpS1906->$1 = 4;
  #line 17 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1905, (moonbit_string_t)moonbit_string_literal_48.data, (moonbit_string_t)moonbit_string_literal_52.data, _M0L6_2atmpS1906);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp28cli__runtime__blackbox__test45____test__72756e74696d655f746573742e6d6274__0(
  
) {
  moonbit_string_t _M0L6_2atmpS1903;
  moonbit_string_t _M0L6_2atmpS1904;
  struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0L7runtimeS820;
  moonbit_string_t _M0L6_2atmpS1902;
  moonbit_string_t _M0L6_2atmpS1901;
  struct _M0TPB4Show _M0L6_2atmpS1894;
  moonbit_string_t _M0L6_2atmpS1897;
  moonbit_string_t _M0L6_2atmpS1898;
  moonbit_string_t _M0L6_2atmpS1899;
  moonbit_string_t _M0L6_2atmpS1900;
  moonbit_string_t* _M0L6_2atmpS1896;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1895;
  #line 2 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1903 = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS1904 = (moonbit_string_t)moonbit_string_literal_53.data;
  #line 3 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L7runtimeS820
  = _M0FP36mulpjs4mulp12cli__runtime20cli__runtime_2einner((moonbit_string_t)moonbit_string_literal_49.data, _M0L6_2atmpS1903, _M0L6_2atmpS1904);
  #line 8 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1902
  = _M0MP36mulpjs4mulp12cli__runtime10CliRuntime11entry__file(_M0L7runtimeS820);
  #line 8 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  _M0L6_2atmpS1901 = _M0MPC16option6Option6unwrapGsE(_M0L6_2atmpS1902);
  _M0L6_2atmpS1894
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1901
  };
  _M0L6_2atmpS1897 = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS1898 = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS1899 = 0;
  _M0L6_2atmpS1900 = 0;
  _M0L6_2atmpS1896 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1896[0] = _M0L6_2atmpS1897;
  _M0L6_2atmpS1896[1] = _M0L6_2atmpS1898;
  _M0L6_2atmpS1896[2] = _M0L6_2atmpS1899;
  _M0L6_2atmpS1896[3] = _M0L6_2atmpS1900;
  _M0L6_2atmpS1895
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1895)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1895->$0 = _M0L6_2atmpS1896;
  _M0L6_2atmpS1895->$1 = 4;
  #line 8 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1894, (moonbit_string_t)moonbit_string_literal_53.data, (moonbit_string_t)moonbit_string_literal_56.data, _M0L6_2atmpS1895);
}

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime19compatibility__hint(
  moonbit_string_t _M0L9requestedS817,
  struct _M0TPB5ArrayGsE* _M0L9availableS818
) {
  moonbit_string_t _M0L6_2atmpS1893;
  moonbit_string_t _M0L6_2atmpS1889;
  moonbit_string_t _M0L7_2abindS819;
  int32_t _M0L6_2atmpS1892;
  struct _M0TPC16string10StringView _M0L6_2atmpS1891;
  moonbit_string_t _M0L6_2atmpS1890;
  moonbit_string_t _result_2462;
  #line 123 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  #line 127 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1893
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_57.data, _M0L9requestedS817);
  moonbit_decref(_M0L9requestedS817);
  #line 127 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1889
  = moonbit_add_string(_M0L6_2atmpS1893, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1893);
  _M0L7_2abindS819 = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS1892 = Moonbit_array_length(_M0L7_2abindS819);
  _M0L6_2atmpS1891
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1892, _M0L7_2abindS819
  };
  #line 130 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1890
  = _M0MPC15array5Array4joinGsE(_M0L9availableS818, _M0L6_2atmpS1891);
  #line 127 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _result_2462 = moonbit_add_string(_M0L6_2atmpS1889, _M0L6_2atmpS1890);
  moonbit_decref(_M0L6_2atmpS1890);
  moonbit_decref(_M0L6_2atmpS1889);
  return _result_2462;
}

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime27select__compatible__version(
  struct _M0TPB5ArrayGsE* _M0L8versionsS812,
  moonbit_string_t _M0L5rangeS815
) {
  int32_t _M0L7_2abindS811;
  int32_t _M0L2__S813;
  #line 110 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L7_2abindS811 = _M0L8versionsS812->$1;
  _M0L2__S813 = 0;
  while (1) {
    if (_M0L2__S813 < _M0L7_2abindS811) {
      moonbit_string_t* _M0L3bufS1888 = _M0L8versionsS812->$0;
      moonbit_string_t _M0L7versionS814 =
        (moonbit_string_t)_M0L3bufS1888[_M0L2__S813];
      int32_t _M0L6_2atmpS1887;
      moonbit_incref(_M0L7versionS814);
      moonbit_incref(_M0L5rangeS815);
      moonbit_incref(_M0L7versionS814);
      #line 115 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      if (
        _M0FP36mulpjs4mulp12cli__runtime18version__satisfies(_M0L7versionS814, _M0L5rangeS815)
      ) {
        moonbit_decref(_M0L5rangeS815);
        moonbit_decref(_M0L8versionsS812);
        return _M0L7versionS814;
      } else {
        moonbit_decref(_M0L7versionS814);
      }
      _M0L6_2atmpS1887 = _M0L2__S813 + 1;
      _M0L2__S813 = _M0L6_2atmpS1887;
      continue;
    } else {
      moonbit_decref(_M0L5rangeS815);
      moonbit_decref(_M0L8versionsS812);
    }
    break;
  }
  return 0;
}

int32_t _M0FP36mulpjs4mulp12cli__runtime18version__satisfies(
  moonbit_string_t _M0L7versionS809,
  moonbit_string_t _M0L5rangeS807
) {
  moonbit_string_t _M0L7_2abindS808;
  int32_t _M0L6_2atmpS1878;
  struct _M0TPC16string10StringView _M0L6_2atmpS1877;
  #line 99 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L7_2abindS808 = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS1878 = Moonbit_array_length(_M0L7_2abindS808);
  _M0L6_2atmpS1877
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1878, _M0L7_2abindS808
  };
  moonbit_incref(_M0L5rangeS807);
  #line 100 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  if (_M0MPC16string6String11has__prefix(_M0L5rangeS807, _M0L6_2atmpS1877)) {
    moonbit_string_t _M0L6_2atmpS1879;
    struct _M0TPC16string10StringView _M0L6_2atmpS1881;
    moonbit_string_t _M0L6_2atmpS1880;
    int32_t _result_2464;
    #line 101 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1879
    = _M0FP36mulpjs4mulp12cli__runtime14minor__version(_M0L7versionS809);
    #line 101 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1881
    = _M0MPC16string6String11sub_2einner(_M0L5rangeS807, 1, 4294967296ll);
    #line 101 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1880 = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS1881);
    #line 101 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _result_2464
    = moonbit_val_array_equal(_M0L6_2atmpS1879, _M0L6_2atmpS1880);
    moonbit_decref(_M0L6_2atmpS1880);
    moonbit_decref(_M0L6_2atmpS1879);
    return _result_2464;
  } else {
    moonbit_string_t _M0L7_2abindS810 =
      (moonbit_string_t)moonbit_string_literal_61.data;
    int32_t _M0L6_2atmpS1883 = Moonbit_array_length(_M0L7_2abindS810);
    struct _M0TPC16string10StringView _M0L6_2atmpS1882 =
      (struct _M0TPC16string10StringView){0,
                                            _M0L6_2atmpS1883,
                                            _M0L7_2abindS810};
    moonbit_incref(_M0L5rangeS807);
    #line 102 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    if (_M0MPC16string6String11has__prefix(_M0L5rangeS807, _M0L6_2atmpS1882)) {
      moonbit_string_t _M0L6_2atmpS1884;
      struct _M0TPC16string10StringView _M0L6_2atmpS1886;
      moonbit_string_t _M0L6_2atmpS1885;
      int32_t _result_2465;
      #line 103 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      _M0L6_2atmpS1884
      = _M0FP36mulpjs4mulp12cli__runtime14major__version(_M0L7versionS809);
      #line 103 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      _M0L6_2atmpS1886
      = _M0MPC16string6String11sub_2einner(_M0L5rangeS807, 1, 4294967296ll);
      #line 103 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      _M0L6_2atmpS1885
      = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS1886);
      #line 103 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      _result_2465
      = moonbit_val_array_equal(_M0L6_2atmpS1884, _M0L6_2atmpS1885);
      moonbit_decref(_M0L6_2atmpS1885);
      moonbit_decref(_M0L6_2atmpS1884);
      return _result_2465;
    } else {
      int32_t _result_2466;
      #line 105 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      _result_2466
      = moonbit_val_array_equal(_M0L7versionS809, _M0L5rangeS807);
      moonbit_decref(_M0L5rangeS807);
      moonbit_decref(_M0L7versionS809);
      return _result_2466;
    }
  }
}

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime14minor__version(
  moonbit_string_t _M0L7versionS806
) {
  struct _M0TPB5ArrayGsE* _M0L5partsS805;
  int32_t _M0L6_2atmpS1873;
  #line 89 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  moonbit_incref(_M0L7versionS806);
  #line 90 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L5partsS805
  = _M0FP36mulpjs4mulp12cli__runtime14version__parts(_M0L7versionS806);
  moonbit_incref(_M0L5partsS805);
  #line 91 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1873 = _M0MPC15array5Array6lengthGsE(_M0L5partsS805);
  if (_M0L6_2atmpS1873 >= 2) {
    moonbit_string_t _M0L6_2atmpS1876;
    moonbit_string_t _M0L6_2atmpS1874;
    moonbit_string_t _M0L6_2atmpS1875;
    moonbit_string_t _result_2467;
    moonbit_decref(_M0L7versionS806);
    moonbit_incref(_M0L5partsS805);
    #line 92 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1876 = _M0MPC15array5Array2atGsE(_M0L5partsS805, 0);
    #line 92 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1874
    = moonbit_add_string(_M0L6_2atmpS1876, (moonbit_string_t)moonbit_string_literal_62.data);
    moonbit_decref(_M0L6_2atmpS1876);
    #line 92 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1875 = _M0MPC15array5Array2atGsE(_M0L5partsS805, 1);
    #line 92 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _result_2467 = moonbit_add_string(_M0L6_2atmpS1874, _M0L6_2atmpS1875);
    moonbit_decref(_M0L6_2atmpS1875);
    moonbit_decref(_M0L6_2atmpS1874);
    return _result_2467;
  } else {
    moonbit_decref(_M0L5partsS805);
    return _M0L7versionS806;
  }
}

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime14major__version(
  moonbit_string_t _M0L7versionS804
) {
  struct _M0TPB5ArrayGsE* _M0L5partsS803;
  int32_t _M0L6_2atmpS1872;
  #line 79 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  moonbit_incref(_M0L7versionS804);
  #line 80 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L5partsS803
  = _M0FP36mulpjs4mulp12cli__runtime14version__parts(_M0L7versionS804);
  moonbit_incref(_M0L5partsS803);
  #line 81 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1872 = _M0MPC15array5Array6lengthGsE(_M0L5partsS803);
  if (_M0L6_2atmpS1872 >= 1) {
    moonbit_decref(_M0L7versionS804);
    #line 82 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    return _M0MPC15array5Array2atGsE(_M0L5partsS803, 0);
  } else {
    moonbit_decref(_M0L5partsS803);
    return _M0L7versionS804;
  }
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp12cli__runtime14version__parts(
  moonbit_string_t _M0L7versionS795
) {
  moonbit_string_t* _M0L6_2atmpS1871;
  struct _M0TPB5ArrayGsE* _M0L5partsS793;
  moonbit_string_t _M0L7_2abindS796;
  int32_t _M0L6_2atmpS1870;
  struct _M0TPC16string10StringView _M0L6_2atmpS1869;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L5_2aitS794;
  #line 70 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1871 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L5partsS793
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L5partsS793)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L5partsS793->$0 = _M0L6_2atmpS1871;
  _M0L5partsS793->$1 = 0;
  _M0L7_2abindS796 = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS1870 = Moonbit_array_length(_M0L7_2abindS796);
  _M0L6_2atmpS1869
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1870, _M0L7_2abindS796
  };
  #line 72 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L5_2aitS794
  = _M0MPC16string6String5split(_M0L7versionS795, _M0L6_2atmpS1869);
  while (1) {
    struct _M0TPC16string10StringView _M0L4partS798;
    void* _M0L7_2abindS800;
    moonbit_string_t _M0L6_2atmpS1868;
    moonbit_incref(_M0L5_2aitS794);
    #line 72 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L7_2abindS800
    = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L5_2aitS794);
    switch (Moonbit_object_tag(_M0L7_2abindS800)) {
      case 1: {
        struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS801 =
          (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS800;
        struct _M0TPC16string10StringView _M0L8_2afieldS2197 =
          (struct _M0TPC16string10StringView){_M0L7_2aSomeS801->$0_1,
                                                _M0L7_2aSomeS801->$0_2,
                                                _M0L7_2aSomeS801->$0_0};
        int32_t _M0L6_2acntS2353 =
          Moonbit_object_header(_M0L7_2aSomeS801)->rc;
        struct _M0TPC16string10StringView _M0L7_2apartS802;
        if (_M0L6_2acntS2353 > 1) {
          int32_t _M0L11_2anew__cntS2354 = _M0L6_2acntS2353 - 1;
          Moonbit_object_header(_M0L7_2aSomeS801)->rc
          = _M0L11_2anew__cntS2354;
          moonbit_incref(_M0L8_2afieldS2197.$0);
        } else if (_M0L6_2acntS2353 == 1) {
          #line 72 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
          moonbit_free(_M0L7_2aSomeS801);
        }
        _M0L7_2apartS802 = _M0L8_2afieldS2197;
        _M0L4partS798 = _M0L7_2apartS802;
        goto join_797;
        break;
      }
      default: {
        moonbit_decref(_M0L7_2abindS800);
        moonbit_decref(_M0L5_2aitS794);
        break;
      }
    }
    goto joinlet_2469;
    join_797:;
    #line 73 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0L6_2atmpS1868 = _M0MPC16string10StringView9to__owned(_M0L4partS798);
    moonbit_incref(_M0L5partsS793);
    #line 73 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
    _M0MPC15array5Array4pushGsE(_M0L5partsS793, _M0L6_2atmpS1868);
    continue;
    joinlet_2469:;
    break;
  }
  return _M0L5partsS793;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp12cli__runtime16cli__completions(
  moonbit_string_t _M0L6prefixS789
) {
  moonbit_string_t* _M0L6_2atmpS1867;
  struct _M0TPB5ArrayGsE* _M0L7matchesS783;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS784;
  int32_t _M0L7_2abindS785;
  int32_t _M0L2__S786;
  #line 54 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1867 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L7matchesS783
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L7matchesS783)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L7matchesS783->$0 = _M0L6_2atmpS1867;
  _M0L7matchesS783->$1 = 0;
  #line 56 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L7_2abindS784 = _M0FP36mulpjs4mulp12cli__runtime12known__flags();
  _M0L7_2abindS785 = _M0L7_2abindS784->$1;
  _M0L2__S786 = 0;
  while (1) {
    if (_M0L2__S786 < _M0L7_2abindS785) {
      moonbit_string_t* _M0L3bufS1866 = _M0L7_2abindS784->$0;
      moonbit_string_t _M0L4flagS787 =
        (moonbit_string_t)_M0L3bufS1866[_M0L2__S786];
      int32_t _M0L17same__flag__styleS788;
      int32_t _M0L6_2atmpS1865;
      #line 57 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      if (
        moonbit_val_array_equal(_M0L6prefixS789, (moonbit_string_t)moonbit_string_literal_39.data)
      ) {
        moonbit_string_t _M0L7_2abindS790 =
          (moonbit_string_t)moonbit_string_literal_39.data;
        int32_t _M0L6_2atmpS1862 = Moonbit_array_length(_M0L7_2abindS790);
        struct _M0TPC16string10StringView _M0L6_2atmpS1861 =
          (struct _M0TPC16string10StringView){0,
                                                _M0L6_2atmpS1862,
                                                _M0L7_2abindS790};
        moonbit_incref(_M0L4flagS787);
        moonbit_incref(_M0L4flagS787);
        #line 58 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
        if (
          _M0MPC16string6String11has__prefix(_M0L4flagS787, _M0L6_2atmpS1861)
        ) {
          moonbit_string_t _M0L7_2abindS791 =
            (moonbit_string_t)moonbit_string_literal_63.data;
          int32_t _M0L6_2atmpS1860 = Moonbit_array_length(_M0L7_2abindS791);
          struct _M0TPC16string10StringView _M0L6_2atmpS1859 =
            (struct _M0TPC16string10StringView){0,
                                                  _M0L6_2atmpS1860,
                                                  _M0L7_2abindS791};
          int32_t _M0L6_2atmpS1858;
          moonbit_incref(_M0L4flagS787);
          #line 58 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
          _M0L6_2atmpS1858
          = _M0MPC16string6String11has__prefix(_M0L4flagS787, _M0L6_2atmpS1859);
          _M0L17same__flag__styleS788 = !_M0L6_2atmpS1858;
        } else {
          _M0L17same__flag__styleS788 = 0;
        }
      } else {
        int32_t _M0L6_2atmpS1864 = Moonbit_array_length(_M0L6prefixS789);
        struct _M0TPC16string10StringView _M0L6_2atmpS1863;
        moonbit_incref(_M0L6prefixS789);
        _M0L6_2atmpS1863
        = (struct _M0TPC16string10StringView){
          0, _M0L6_2atmpS1864, _M0L6prefixS789
        };
        moonbit_incref(_M0L4flagS787);
        moonbit_incref(_M0L4flagS787);
        #line 60 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
        _M0L17same__flag__styleS788
        = _M0MPC16string6String11has__prefix(_M0L4flagS787, _M0L6_2atmpS1863);
      }
      if (_M0L17same__flag__styleS788) {
        moonbit_incref(_M0L7matchesS783);
        #line 63 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
        _M0MPC15array5Array4pushGsE(_M0L7matchesS783, _M0L4flagS787);
      } else {
        moonbit_decref(_M0L4flagS787);
      }
      _M0L6_2atmpS1865 = _M0L2__S786 + 1;
      _M0L2__S786 = _M0L6_2atmpS1865;
      continue;
    } else {
      moonbit_decref(_M0L6prefixS789);
      moonbit_decref(_M0L7_2abindS784);
    }
    break;
  }
  return _M0L7matchesS783;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp12cli__runtime12known__flags() {
  moonbit_string_t* _M0L6_2atmpS1857;
  struct _M0TPB5ArrayGsE* _block_2471;
  #line 36 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1857 = (moonbit_string_t*)moonbit_make_ref_array_raw(12);
  _M0L6_2atmpS1857[0] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS1857[1] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS1857[2] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS1857[3] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS1857[4] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS1857[5] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS1857[6] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS1857[7] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS1857[8] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS1857[9] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS1857[10] = (moonbit_string_t)moonbit_string_literal_74.data;
  _M0L6_2atmpS1857[11] = (moonbit_string_t)moonbit_string_literal_75.data;
  _block_2471
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_2471)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _block_2471->$0 = _M0L6_2atmpS1857;
  _block_2471->$1 = 12;
  return _block_2471;
}

moonbit_string_t _M0FP36mulpjs4mulp12cli__runtime9cli__help(
  moonbit_string_t _M0L7versionS782
) {
  moonbit_string_t _M0L6_2atmpS1856;
  moonbit_string_t _result_2472;
  #line 31 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  #line 32 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L6_2atmpS1856
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_76.data, _M0L7versionS782);
  moonbit_decref(_M0L7versionS782);
  #line 32 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _result_2472
  = moonbit_add_string(_M0L6_2atmpS1856, (moonbit_string_t)moonbit_string_literal_77.data);
  moonbit_decref(_M0L6_2atmpS1856);
  return _result_2472;
}

moonbit_string_t _M0MP36mulpjs4mulp12cli__runtime10CliRuntime11entry__file(
  struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0L4selfS779
) {
  moonbit_string_t _M0L4pathS777;
  moonbit_string_t _M0L7_2abindS778;
  #line 23 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _M0L7_2abindS778 = _M0L4selfS779->$2;
  if (_M0L7_2abindS778 == 0) {
    moonbit_string_t _M0L8_2afieldS2200 = _M0L4selfS779->$1;
    int32_t _M0L6_2acntS2355 = Moonbit_object_header(_M0L4selfS779)->rc;
    if (_M0L6_2acntS2355 > 1) {
      int32_t _M0L11_2anew__cntS2358 = _M0L6_2acntS2355 - 1;
      Moonbit_object_header(_M0L4selfS779)->rc = _M0L11_2anew__cntS2358;
      if (_M0L8_2afieldS2200) {
        moonbit_incref(_M0L8_2afieldS2200);
      }
    } else if (_M0L6_2acntS2355 == 1) {
      moonbit_string_t _M0L8_2afieldS2357 = _M0L4selfS779->$2;
      moonbit_string_t _M0L8_2afieldS2356;
      if (_M0L8_2afieldS2357) {
        moonbit_decref(_M0L8_2afieldS2357);
      }
      _M0L8_2afieldS2356 = _M0L4selfS779->$0;
      moonbit_decref(_M0L8_2afieldS2356);
      #line 26 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      moonbit_free(_M0L4selfS779);
    }
    return _M0L8_2afieldS2200;
  } else {
    int32_t _M0L6_2acntS2359 = Moonbit_object_header(_M0L4selfS779)->rc;
    moonbit_string_t _M0L7_2aSomeS780;
    moonbit_string_t _M0L7_2apathS781;
    if (_M0L6_2acntS2359 > 1) {
      int32_t _M0L11_2anew__cntS2362 = _M0L6_2acntS2359 - 1;
      Moonbit_object_header(_M0L4selfS779)->rc = _M0L11_2anew__cntS2362;
      if (_M0L7_2abindS778) {
        moonbit_incref(_M0L7_2abindS778);
      }
    } else if (_M0L6_2acntS2359 == 1) {
      moonbit_string_t _M0L8_2afieldS2361 = _M0L4selfS779->$1;
      moonbit_string_t _M0L8_2afieldS2360;
      if (_M0L8_2afieldS2361) {
        moonbit_decref(_M0L8_2afieldS2361);
      }
      _M0L8_2afieldS2360 = _M0L4selfS779->$0;
      moonbit_decref(_M0L8_2afieldS2360);
      #line 24 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
      moonbit_free(_M0L4selfS779);
    }
    _M0L7_2aSomeS780 = _M0L7_2abindS778;
    _M0L7_2apathS781 = _M0L7_2aSomeS780;
    _M0L4pathS777 = _M0L7_2apathS781;
    goto join_776;
  }
  join_776:;
  return _M0L4pathS777;
}

struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0FP36mulpjs4mulp12cli__runtime12cli__runtime(
  moonbit_string_t _M0L3cwdS775,
  void* _M0L16discovered_2eoptS770,
  void* _M0L14explicit_2eoptS773
) {
  moonbit_string_t _M0L10discoveredS769;
  moonbit_string_t _M0L8explicitS772;
  switch (Moonbit_object_tag(_M0L16discovered_2eoptS770)) {
    case 1: {
      struct _M0DTPC16option6OptionGOsE4Some* _M0L7_2aSomeS771 =
        (struct _M0DTPC16option6OptionGOsE4Some*)_M0L16discovered_2eoptS770;
      moonbit_string_t _M0L8_2afieldS2203 = _M0L7_2aSomeS771->$0;
      int32_t _M0L6_2acntS2363 = Moonbit_object_header(_M0L7_2aSomeS771)->rc;
      if (_M0L6_2acntS2363 > 1) {
        int32_t _M0L11_2anew__cntS2364 = _M0L6_2acntS2363 - 1;
        Moonbit_object_header(_M0L7_2aSomeS771)->rc = _M0L11_2anew__cntS2364;
        if (_M0L8_2afieldS2203) {
          moonbit_incref(_M0L8_2afieldS2203);
        }
      } else if (_M0L6_2acntS2363 == 1) {
        moonbit_free(_M0L7_2aSomeS771);
      }
      _M0L10discoveredS769 = _M0L8_2afieldS2203;
      break;
    }
    default: {
      moonbit_decref(_M0L16discovered_2eoptS770);
      _M0L10discoveredS769 = 0;
      break;
    }
  }
  switch (Moonbit_object_tag(_M0L14explicit_2eoptS773)) {
    case 1: {
      struct _M0DTPC16option6OptionGOsE4Some* _M0L7_2aSomeS774 =
        (struct _M0DTPC16option6OptionGOsE4Some*)_M0L14explicit_2eoptS773;
      moonbit_string_t _M0L8_2afieldS2202 = _M0L7_2aSomeS774->$0;
      int32_t _M0L6_2acntS2365 = Moonbit_object_header(_M0L7_2aSomeS774)->rc;
      if (_M0L6_2acntS2365 > 1) {
        int32_t _M0L11_2anew__cntS2366 = _M0L6_2acntS2365 - 1;
        Moonbit_object_header(_M0L7_2aSomeS774)->rc = _M0L11_2anew__cntS2366;
        if (_M0L8_2afieldS2202) {
          moonbit_incref(_M0L8_2afieldS2202);
        }
      } else if (_M0L6_2acntS2365 == 1) {
        moonbit_free(_M0L7_2aSomeS774);
      }
      _M0L8explicitS772 = _M0L8_2afieldS2202;
      break;
    }
    default: {
      moonbit_decref(_M0L14explicit_2eoptS773);
      _M0L8explicitS772 = 0;
      break;
    }
  }
  return _M0FP36mulpjs4mulp12cli__runtime20cli__runtime_2einner(_M0L3cwdS775, _M0L10discoveredS769, _M0L8explicitS772);
}

struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _M0FP36mulpjs4mulp12cli__runtime20cli__runtime_2einner(
  moonbit_string_t _M0L3cwdS766,
  moonbit_string_t _M0L10discoveredS767,
  moonbit_string_t _M0L8explicitS768
) {
  struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime* _block_2474;
  #line 9 "/Users/user/workspace/github/gulp/mulp/cli_runtime/runtime.mbt"
  _block_2474
  = (struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime));
  Moonbit_object_header(_block_2474)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp12cli__runtime10CliRuntime, $0) >> 2, 3, 0);
  _block_2474->$0 = _M0L3cwdS766;
  _block_2474->$1 = _M0L10discoveredS767;
  _block_2474->$2 = _M0L8explicitS768;
  return _block_2474;
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS764,
  struct _M0TPC16string10StringView _M0L9separatorS765
) {
  moonbit_string_t* _M0L3bufS1854;
  int32_t _M0L3lenS1855;
  int32_t _M0L6_2acntS2367;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1853;
  #line 2070 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS1854 = _M0L4selfS764->$0;
  _M0L3lenS1855 = _M0L4selfS764->$1;
  _M0L6_2acntS2367 = Moonbit_object_header(_M0L4selfS764)->rc;
  if (_M0L6_2acntS2367 > 1) {
    int32_t _M0L11_2anew__cntS2368 = _M0L6_2acntS2367 - 1;
    Moonbit_object_header(_M0L4selfS764)->rc = _M0L11_2anew__cntS2368;
    moonbit_incref(_M0L3bufS1854);
  } else if (_M0L6_2acntS2367 == 1) {
    #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS764);
  }
  _M0L6_2atmpS1853
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1855, _M0L3bufS1854
  };
  #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS1853, _M0L9separatorS765);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS762,
  int32_t _M0L5indexS763
) {
  int32_t _M0L3lenS761;
  int32_t _if__result_2475;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS761 = _M0L4selfS762->$1;
  if (_M0L5indexS763 >= 0) {
    _if__result_2475 = _M0L5indexS763 < _M0L3lenS761;
  } else {
    _if__result_2475 = 0;
  }
  if (_if__result_2475) {
    moonbit_string_t* _M0L6_2atmpS1852;
    moonbit_string_t _M0L6_2atmpS2205;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS1852 = _M0MPC15array5Array6bufferGsE(_M0L4selfS762);
    if (
      _M0L5indexS763 < 0
      || _M0L5indexS763 >= Moonbit_array_length(_M0L6_2atmpS1852)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2205 = (moonbit_string_t)_M0L6_2atmpS1852[_M0L5indexS763];
    moonbit_incref(_M0L6_2atmpS2205);
    moonbit_decref(_M0L6_2atmpS1852);
    return _M0L6_2atmpS2205;
  } else {
    moonbit_decref(_M0L4selfS762);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS736,
  struct _M0TPC16string10StringView _M0L9separatorS748
) {
  int32_t _M0L3endS1831;
  int32_t _M0L5startS1832;
  int32_t _M0L6_2atmpS1830;
  #line 1369 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1831 = _M0L4selfS736.$2;
  _M0L5startS1832 = _M0L4selfS736.$1;
  _M0L6_2atmpS1830 = _M0L3endS1831 - _M0L5startS1832;
  if (_M0L6_2atmpS1830 == 0) {
    moonbit_decref(_M0L9separatorS748.$0);
    moonbit_decref(_M0L4selfS736.$0);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t* _M0L3bufS1850 = _M0L4selfS736.$0;
    int32_t _M0L5startS1851 = _M0L4selfS736.$1;
    moonbit_string_t _M0L5_2ahdS737 =
      (moonbit_string_t)_M0L3bufS1850[_M0L5startS1851];
    moonbit_string_t* _M0L9_2ax__bufS738 = _M0L4selfS736.$0;
    int32_t _M0L5startS1849 = _M0L4selfS736.$1;
    int32_t _M0L11_2ax__startS739 = 1 + _M0L5startS1849;
    int32_t _M0L9_2ax__endS740 = _M0L4selfS736.$2;
    struct _M0TPC16string10StringView _M0L2hdS741;
    int32_t _M0L7_2abindS742;
    int32_t _M0L6_2atmpS1848;
    int32_t _M0L10size__hintS743;
    int32_t _M0L2__S744;
    int32_t _M0L10size__hintS745;
    int32_t _M0L10size__hintS749;
    struct _M0TPB13StringBuilder* _M0L3bufS750;
    moonbit_string_t _M0L3strS1833;
    int32_t _M0L5startS1834;
    int32_t _M0L3endS1836;
    int64_t _M0L6_2atmpS1835;
    moonbit_incref(_M0L5_2ahdS737);
    #line 1376 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS741
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS737);
    _M0L7_2abindS742 = _M0L9_2ax__endS740 - _M0L11_2ax__startS739;
    moonbit_incref(_M0L2hdS741.$0);
    #line 1377 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L6_2atmpS1848 = _M0MPC16string10StringView6length(_M0L2hdS741);
    _M0L2__S744 = 0;
    _M0L10size__hintS745 = _M0L6_2atmpS1848;
    while (1) {
      if (_M0L2__S744 < _M0L7_2abindS742) {
        int32_t _M0L6_2atmpS1847 = _M0L11_2ax__startS739 + _M0L2__S744;
        moonbit_string_t _M0L1sS746 =
          (moonbit_string_t)_M0L9_2ax__bufS738[_M0L6_2atmpS1847];
        int32_t _M0L6_2atmpS1841 = _M0L2__S744 + 1;
        struct _M0TPC16string10StringView _M0L6_2atmpS1846;
        int32_t _M0L6_2atmpS1845;
        int32_t _M0L6_2atmpS1843;
        int32_t _M0L6_2atmpS1844;
        int32_t _M0L6_2atmpS1842;
        moonbit_incref(_M0L1sS746);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS1846
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS746);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS1845
        = _M0MPC16string10StringView6length(_M0L6_2atmpS1846);
        _M0L6_2atmpS1843 = _M0L10size__hintS745 + _M0L6_2atmpS1845;
        moonbit_incref(_M0L9separatorS748.$0);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS1844
        = _M0MPC16string10StringView6length(_M0L9separatorS748);
        _M0L6_2atmpS1842 = _M0L6_2atmpS1843 + _M0L6_2atmpS1844;
        _M0L2__S744 = _M0L6_2atmpS1841;
        _M0L10size__hintS745 = _M0L6_2atmpS1842;
        continue;
      } else {
        _M0L10size__hintS743 = _M0L10size__hintS745;
      }
      break;
    }
    _M0L10size__hintS749 = _M0L10size__hintS743 << 1;
    #line 1383 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS750 = _M0MPB13StringBuilder11new_2einner(_M0L10size__hintS749);
    moonbit_incref(_M0L3bufS750);
    #line 1385 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS750, _M0L2hdS741);
    _M0L3strS1833 = _M0L9separatorS748.$0;
    _M0L5startS1834 = _M0L9separatorS748.$1;
    _M0L3endS1836 = _M0L9separatorS748.$2;
    _M0L6_2atmpS1835 = (int64_t)_M0L3endS1836;
    moonbit_incref(_M0L3strS1833);
    #line 1386 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS1833, 0, _M0L5startS1834, _M0L6_2atmpS1835)
    ) {
      int32_t _M0L7_2abindS751;
      int32_t _M0L2__S752;
      moonbit_decref(_M0L9separatorS748.$0);
      _M0L7_2abindS751 = _M0L9_2ax__endS740 - _M0L11_2ax__startS739;
      _M0L2__S752 = 0;
      while (1) {
        if (_M0L2__S752 < _M0L7_2abindS751) {
          int32_t _M0L6_2atmpS1838 = _M0L11_2ax__startS739 + _M0L2__S752;
          moonbit_string_t _M0L1sS753 =
            (moonbit_string_t)_M0L9_2ax__bufS738[_M0L6_2atmpS1838];
          struct _M0TPC16string10StringView _M0L1sS754;
          int32_t _M0L6_2atmpS1837;
          moonbit_incref(_M0L1sS753);
          #line 1389 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS754
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS753);
          moonbit_incref(_M0L3bufS750);
          #line 1390 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS750, _M0L1sS754);
          _M0L6_2atmpS1837 = _M0L2__S752 + 1;
          _M0L2__S752 = _M0L6_2atmpS1837;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS738);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS756 = _M0L9_2ax__endS740 - _M0L11_2ax__startS739;
      int32_t _M0L2__S757 = 0;
      while (1) {
        if (_M0L2__S757 < _M0L7_2abindS756) {
          int32_t _M0L6_2atmpS1840 = _M0L11_2ax__startS739 + _M0L2__S757;
          moonbit_string_t _M0L1sS758 =
            (moonbit_string_t)_M0L9_2ax__bufS738[_M0L6_2atmpS1840];
          struct _M0TPC16string10StringView _M0L1sS759;
          int32_t _M0L6_2atmpS1839;
          moonbit_incref(_M0L1sS758);
          #line 1394 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS759
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS758);
          moonbit_incref(_M0L3bufS750);
          moonbit_incref(_M0L9separatorS748.$0);
          #line 1395 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS750, _M0L9separatorS748);
          moonbit_incref(_M0L3bufS750);
          #line 1397 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS750, _M0L1sS759);
          _M0L6_2atmpS1839 = _M0L2__S757 + 1;
          _M0L2__S757 = _M0L6_2atmpS1839;
          continue;
        } else {
          moonbit_decref(_M0L9separatorS748.$0);
          moonbit_decref(_M0L9_2ax__bufS738);
        }
        break;
      }
    }
    #line 1400 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS750);
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS735) {
  moonbit_string_t _M0L6_2atmpS1829;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS1829 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS735);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS1829);
  moonbit_decref(_M0L6_2atmpS1829);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS734,
  struct _M0TPB6Hasher* _M0L6hasherS733
) {
  #line 530 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 531 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS733, _M0L4selfS734);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS732,
  struct _M0TPB6Hasher* _M0L6hasherS731
) {
  #line 496 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 497 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS731, _M0L4selfS732);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS729,
  moonbit_string_t _M0L5valueS727
) {
  int32_t _M0L7_2abindS726;
  int32_t _M0L1iS728;
  #line 387 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS726 = Moonbit_array_length(_M0L5valueS727);
  _M0L1iS728 = 0;
  while (1) {
    if (_M0L1iS728 < _M0L7_2abindS726) {
      int32_t _M0L6_2atmpS1827 = _M0L5valueS727[_M0L1iS728];
      int32_t _M0L6_2atmpS1826 = (int32_t)_M0L6_2atmpS1827;
      uint32_t _M0L6_2atmpS1825 = *(uint32_t*)&_M0L6_2atmpS1826;
      int32_t _M0L6_2atmpS1828;
      moonbit_incref(_M0L4selfS729);
      #line 389 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS729, _M0L6_2atmpS1825);
      _M0L6_2atmpS1828 = _M0L1iS728 + 1;
      _M0L1iS728 = _M0L6_2atmpS1828;
      continue;
    } else {
      moonbit_decref(_M0L4selfS729);
      moonbit_decref(_M0L5valueS727);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS724,
  int32_t _M0L3idxS725
) {
  int32_t _result_2480;
  #line 1778 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _result_2480 = _M0L4selfS724[_M0L3idxS725];
  moonbit_decref(_M0L4selfS724);
  return _result_2480;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS711,
  int32_t _M0L3keyS707
) {
  int32_t _M0L4hashS706;
  int32_t _M0L14capacity__maskS1810;
  int32_t _M0L6_2atmpS1809;
  int32_t _M0L1iS708;
  int32_t _M0L3idxS709;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS706 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS707);
  _M0L14capacity__maskS1810 = _M0L4selfS711->$3;
  _M0L6_2atmpS1809 = _M0L4hashS706 & _M0L14capacity__maskS1810;
  _M0L1iS708 = 0;
  _M0L3idxS709 = _M0L6_2atmpS1809;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1808 =
      _M0L4selfS711->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS710;
    if (
      _M0L3idxS709 < 0
      || _M0L3idxS709 >= Moonbit_array_length(_M0L7entriesS1808)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS710
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1808[
        _M0L3idxS709
      ];
    if (_M0L7_2abindS710 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1797;
      if (_M0L7_2abindS710) {
        moonbit_incref(_M0L7_2abindS710);
      }
      moonbit_decref(_M0L4selfS711);
      if (_M0L7_2abindS710) {
        moonbit_decref(_M0L7_2abindS710);
      }
      _M0L6_2atmpS1797 = 0;
      return _M0L6_2atmpS1797;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS712 =
        _M0L7_2abindS710;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS713 =
        _M0L7_2aSomeS712;
      int32_t _M0L4hashS1799 = _M0L8_2aentryS713->$3;
      int32_t _if__result_2482;
      int32_t _M0L3pslS1802;
      int32_t _M0L6_2atmpS1804;
      int32_t _M0L6_2atmpS1806;
      int32_t _M0L14capacity__maskS1807;
      int32_t _M0L6_2atmpS1805;
      if (_M0L4hashS1799 == _M0L4hashS706) {
        int32_t _M0L3keyS1798 = _M0L8_2aentryS713->$4;
        _if__result_2482 = _M0L3keyS1798 == _M0L3keyS707;
      } else {
        _if__result_2482 = 0;
      }
      if (_if__result_2482) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2213;
        int32_t _M0L6_2acntS2369;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1801;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1800;
        moonbit_incref(_M0L8_2aentryS713);
        moonbit_decref(_M0L4selfS711);
        _M0L8_2afieldS2213 = _M0L8_2aentryS713->$5;
        _M0L6_2acntS2369 = Moonbit_object_header(_M0L8_2aentryS713)->rc;
        if (_M0L6_2acntS2369 > 1) {
          int32_t _M0L11_2anew__cntS2371 = _M0L6_2acntS2369 - 1;
          Moonbit_object_header(_M0L8_2aentryS713)->rc
          = _M0L11_2anew__cntS2371;
          moonbit_incref(_M0L8_2afieldS2213);
        } else if (_M0L6_2acntS2369 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2370 =
            _M0L8_2aentryS713->$1;
          if (_M0L8_2afieldS2370) {
            moonbit_decref(_M0L8_2afieldS2370);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS713);
        }
        _M0L5valueS1801 = _M0L8_2afieldS2213;
        _M0L6_2atmpS1800 = _M0L5valueS1801;
        return _M0L6_2atmpS1800;
      } else {
        moonbit_incref(_M0L8_2aentryS713);
      }
      _M0L3pslS1802 = _M0L8_2aentryS713->$2;
      moonbit_decref(_M0L8_2aentryS713);
      if (_M0L1iS708 > _M0L3pslS1802) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1803;
        moonbit_decref(_M0L4selfS711);
        _M0L6_2atmpS1803 = 0;
        return _M0L6_2atmpS1803;
      }
      _M0L6_2atmpS1804 = _M0L1iS708 + 1;
      _M0L6_2atmpS1806 = _M0L3idxS709 + 1;
      _M0L14capacity__maskS1807 = _M0L4selfS711->$3;
      _M0L6_2atmpS1805 = _M0L6_2atmpS1806 & _M0L14capacity__maskS1807;
      _M0L1iS708 = _M0L6_2atmpS1804;
      _M0L3idxS709 = _M0L6_2atmpS1805;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS720,
  moonbit_string_t _M0L3keyS716
) {
  int32_t _M0L4hashS715;
  int32_t _M0L14capacity__maskS1824;
  int32_t _M0L6_2atmpS1823;
  int32_t _M0L1iS717;
  int32_t _M0L3idxS718;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS716);
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS715 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS716);
  _M0L14capacity__maskS1824 = _M0L4selfS720->$3;
  _M0L6_2atmpS1823 = _M0L4hashS715 & _M0L14capacity__maskS1824;
  _M0L1iS717 = 0;
  _M0L3idxS718 = _M0L6_2atmpS1823;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1822 =
      _M0L4selfS720->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS719;
    if (
      _M0L3idxS718 < 0
      || _M0L3idxS718 >= Moonbit_array_length(_M0L7entriesS1822)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS719
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1822[
        _M0L3idxS718
      ];
    if (_M0L7_2abindS719 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1811;
      if (_M0L7_2abindS719) {
        moonbit_incref(_M0L7_2abindS719);
      }
      moonbit_decref(_M0L4selfS720);
      if (_M0L7_2abindS719) {
        moonbit_decref(_M0L7_2abindS719);
      }
      moonbit_decref(_M0L3keyS716);
      _M0L6_2atmpS1811 = 0;
      return _M0L6_2atmpS1811;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS721 =
        _M0L7_2abindS719;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS722 =
        _M0L7_2aSomeS721;
      int32_t _M0L4hashS1813 = _M0L8_2aentryS722->$3;
      int32_t _if__result_2484;
      int32_t _M0L3pslS1816;
      int32_t _M0L6_2atmpS1818;
      int32_t _M0L6_2atmpS1820;
      int32_t _M0L14capacity__maskS1821;
      int32_t _M0L6_2atmpS1819;
      if (_M0L4hashS1813 == _M0L4hashS715) {
        moonbit_string_t _M0L3keyS1812 = _M0L8_2aentryS722->$4;
        #line 220 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_2484
        = moonbit_val_array_equal(_M0L3keyS1812, _M0L3keyS716);
      } else {
        _if__result_2484 = 0;
      }
      if (_if__result_2484) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2216;
        int32_t _M0L6_2acntS2372;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1815;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1814;
        moonbit_incref(_M0L8_2aentryS722);
        moonbit_decref(_M0L4selfS720);
        moonbit_decref(_M0L3keyS716);
        _M0L8_2afieldS2216 = _M0L8_2aentryS722->$5;
        _M0L6_2acntS2372 = Moonbit_object_header(_M0L8_2aentryS722)->rc;
        if (_M0L6_2acntS2372 > 1) {
          int32_t _M0L11_2anew__cntS2375 = _M0L6_2acntS2372 - 1;
          Moonbit_object_header(_M0L8_2aentryS722)->rc
          = _M0L11_2anew__cntS2375;
          moonbit_incref(_M0L8_2afieldS2216);
        } else if (_M0L6_2acntS2372 == 1) {
          moonbit_string_t _M0L8_2afieldS2374 = _M0L8_2aentryS722->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2373;
          moonbit_decref(_M0L8_2afieldS2374);
          _M0L8_2afieldS2373 = _M0L8_2aentryS722->$1;
          if (_M0L8_2afieldS2373) {
            moonbit_decref(_M0L8_2afieldS2373);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS722);
        }
        _M0L5valueS1815 = _M0L8_2afieldS2216;
        _M0L6_2atmpS1814 = _M0L5valueS1815;
        return _M0L6_2atmpS1814;
      } else {
        moonbit_incref(_M0L8_2aentryS722);
      }
      _M0L3pslS1816 = _M0L8_2aentryS722->$2;
      moonbit_decref(_M0L8_2aentryS722);
      if (_M0L1iS717 > _M0L3pslS1816) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1817;
        moonbit_decref(_M0L4selfS720);
        moonbit_decref(_M0L3keyS716);
        _M0L6_2atmpS1817 = 0;
        return _M0L6_2atmpS1817;
      }
      _M0L6_2atmpS1818 = _M0L1iS717 + 1;
      _M0L6_2atmpS1820 = _M0L3idxS718 + 1;
      _M0L14capacity__maskS1821 = _M0L4selfS720->$3;
      _M0L6_2atmpS1819 = _M0L6_2atmpS1820 & _M0L14capacity__maskS1821;
      _M0L1iS717 = _M0L6_2atmpS1818;
      _M0L3idxS718 = _M0L6_2atmpS1819;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS691
) {
  int32_t _M0L6lengthS690;
  int32_t _M0Lm8capacityS692;
  int32_t _M0L6_2atmpS1774;
  int32_t _M0L6_2atmpS1773;
  int32_t _M0L6_2atmpS1784;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS693;
  int32_t _M0L3endS1782;
  int32_t _M0L5startS1783;
  int32_t _M0L7_2abindS694;
  int32_t _M0L2__S695;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS691.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS690
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS691);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS692 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS690);
  _M0L6_2atmpS1774 = _M0Lm8capacityS692;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1773 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1774);
  if (_M0L6lengthS690 > _M0L6_2atmpS1773) {
    int32_t _M0L6_2atmpS1775 = _M0Lm8capacityS692;
    _M0Lm8capacityS692 = _M0L6_2atmpS1775 * 2;
  }
  _M0L6_2atmpS1784 = _M0Lm8capacityS692;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS693
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1784);
  _M0L3endS1782 = _M0L3arrS691.$2;
  _M0L5startS1783 = _M0L3arrS691.$1;
  _M0L7_2abindS694 = _M0L3endS1782 - _M0L5startS1783;
  _M0L2__S695 = 0;
  while (1) {
    if (_M0L2__S695 < _M0L7_2abindS694) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1779 =
        _M0L3arrS691.$0;
      int32_t _M0L5startS1781 = _M0L3arrS691.$1;
      int32_t _M0L6_2atmpS1780 = _M0L5startS1781 + _M0L2__S695;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS696 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1779[
          _M0L6_2atmpS1780
        ];
      moonbit_string_t _M0L6_2atmpS1776 = _M0L1eS696->$0;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1777 =
        _M0L1eS696->$1;
      int32_t _M0L6_2atmpS1778;
      moonbit_incref(_M0L6_2atmpS1777);
      moonbit_incref(_M0L6_2atmpS1776);
      moonbit_incref(_M0L1mS693);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS693, _M0L6_2atmpS1776, _M0L6_2atmpS1777);
      _M0L6_2atmpS1778 = _M0L2__S695 + 1;
      _M0L2__S695 = _M0L6_2atmpS1778;
      continue;
    } else {
      moonbit_decref(_M0L3arrS691.$0);
    }
    break;
  }
  return _M0L1mS693;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS699
) {
  int32_t _M0L6lengthS698;
  int32_t _M0Lm8capacityS700;
  int32_t _M0L6_2atmpS1786;
  int32_t _M0L6_2atmpS1785;
  int32_t _M0L6_2atmpS1796;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS701;
  int32_t _M0L3endS1794;
  int32_t _M0L5startS1795;
  int32_t _M0L7_2abindS702;
  int32_t _M0L2__S703;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS699.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS698
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS699);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS700 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS698);
  _M0L6_2atmpS1786 = _M0Lm8capacityS700;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1785 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1786);
  if (_M0L6lengthS698 > _M0L6_2atmpS1785) {
    int32_t _M0L6_2atmpS1787 = _M0Lm8capacityS700;
    _M0Lm8capacityS700 = _M0L6_2atmpS1787 * 2;
  }
  _M0L6_2atmpS1796 = _M0Lm8capacityS700;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS701
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1796);
  _M0L3endS1794 = _M0L3arrS699.$2;
  _M0L5startS1795 = _M0L3arrS699.$1;
  _M0L7_2abindS702 = _M0L3endS1794 - _M0L5startS1795;
  _M0L2__S703 = 0;
  while (1) {
    if (_M0L2__S703 < _M0L7_2abindS702) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1791 =
        _M0L3arrS699.$0;
      int32_t _M0L5startS1793 = _M0L3arrS699.$1;
      int32_t _M0L6_2atmpS1792 = _M0L5startS1793 + _M0L2__S703;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS704 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1791[
          _M0L6_2atmpS1792
        ];
      int32_t _M0L6_2atmpS1788 = _M0L1eS704->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1789 = _M0L1eS704->$1;
      int32_t _M0L6_2atmpS1790;
      moonbit_incref(_M0L6_2atmpS1789);
      moonbit_incref(_M0L1mS701);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS701, _M0L6_2atmpS1788, _M0L6_2atmpS1789);
      _M0L6_2atmpS1790 = _M0L2__S703 + 1;
      _M0L2__S703 = _M0L6_2atmpS1790;
      continue;
    } else {
      moonbit_decref(_M0L3arrS699.$0);
    }
    break;
  }
  return _M0L1mS701;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS684,
  moonbit_string_t _M0L3keyS685,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS686
) {
  int32_t _M0L6_2atmpS1771;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS685);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1771 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS685);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS684, _M0L3keyS685, _M0L5valueS686, _M0L6_2atmpS1771);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS687,
  int32_t _M0L3keyS688,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS689
) {
  int32_t _M0L6_2atmpS1772;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1772 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS688);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS687, _M0L3keyS688, _M0L5valueS689, _M0L6_2atmpS1772);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS663
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS662;
  int32_t _M0L8capacityS1763;
  int32_t _M0L13new__capacityS664;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1758;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1757;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2231;
  int32_t _M0L6_2atmpS1759;
  int32_t _M0L8capacityS1761;
  int32_t _M0L6_2atmpS1760;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1762;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2230;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1xS665;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS662 = _M0L4selfS663->$5;
  _M0L8capacityS1763 = _M0L4selfS663->$2;
  _M0L13new__capacityS664 = _M0L8capacityS1763 << 1;
  _M0L6_2atmpS1758 = 0;
  _M0L6_2atmpS1757
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS664, _M0L6_2atmpS1758);
  _M0L6_2aoldS2231 = _M0L4selfS663->$0;
  if (_M0L9old__headS662) {
    moonbit_incref(_M0L9old__headS662);
  }
  moonbit_decref(_M0L6_2aoldS2231);
  _M0L4selfS663->$0 = _M0L6_2atmpS1757;
  _M0L4selfS663->$2 = _M0L13new__capacityS664;
  _M0L6_2atmpS1759 = _M0L13new__capacityS664 - 1;
  _M0L4selfS663->$3 = _M0L6_2atmpS1759;
  _M0L8capacityS1761 = _M0L4selfS663->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1760 = _M0FPB21calc__grow__threshold(_M0L8capacityS1761);
  _M0L4selfS663->$4 = _M0L6_2atmpS1760;
  _M0L4selfS663->$1 = 0;
  _M0L6_2atmpS1762 = 0;
  _M0L6_2aoldS2230 = _M0L4selfS663->$5;
  if (_M0L6_2aoldS2230) {
    moonbit_decref(_M0L6_2aoldS2230);
  }
  _M0L4selfS663->$5 = _M0L6_2atmpS1762;
  _M0L4selfS663->$6 = -1;
  _M0L1xS665 = _M0L9old__headS662;
  while (1) {
    if (_M0L1xS665 == 0) {
      if (_M0L1xS665) {
        moonbit_decref(_M0L1xS665);
      }
      moonbit_decref(_M0L4selfS663);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS666 =
        _M0L1xS665;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS667 =
        _M0L7_2aSomeS666;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS668 =
        _M0L4_2axS667->$1;
      moonbit_string_t _M0L6_2akeyS669 = _M0L4_2axS667->$4;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS670 =
        _M0L4_2axS667->$5;
      int32_t _M0L7_2ahashS671 = _M0L4_2axS667->$3;
      int32_t _M0L6_2acntS2376 = Moonbit_object_header(_M0L4_2axS667)->rc;
      if (_M0L6_2acntS2376 > 1) {
        int32_t _M0L11_2anew__cntS2377 = _M0L6_2acntS2376 - 1;
        Moonbit_object_header(_M0L4_2axS667)->rc = _M0L11_2anew__cntS2377;
        moonbit_incref(_M0L8_2avalueS670);
        moonbit_incref(_M0L6_2akeyS669);
        if (_M0L7_2anextS668) {
          moonbit_incref(_M0L7_2anextS668);
        }
      } else if (_M0L6_2acntS2376 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS667);
      }
      moonbit_incref(_M0L4selfS663);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS663, _M0L6_2akeyS669, _M0L8_2avalueS670, _M0L7_2ahashS671);
      _M0L1xS665 = _M0L7_2anextS668;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS674
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS673;
  int32_t _M0L8capacityS1770;
  int32_t _M0L13new__capacityS675;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1765;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1764;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2236;
  int32_t _M0L6_2atmpS1766;
  int32_t _M0L8capacityS1768;
  int32_t _M0L6_2atmpS1767;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1769;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2235;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L1xS676;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS673 = _M0L4selfS674->$5;
  _M0L8capacityS1770 = _M0L4selfS674->$2;
  _M0L13new__capacityS675 = _M0L8capacityS1770 << 1;
  _M0L6_2atmpS1765 = 0;
  _M0L6_2atmpS1764
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS675, _M0L6_2atmpS1765);
  _M0L6_2aoldS2236 = _M0L4selfS674->$0;
  if (_M0L9old__headS673) {
    moonbit_incref(_M0L9old__headS673);
  }
  moonbit_decref(_M0L6_2aoldS2236);
  _M0L4selfS674->$0 = _M0L6_2atmpS1764;
  _M0L4selfS674->$2 = _M0L13new__capacityS675;
  _M0L6_2atmpS1766 = _M0L13new__capacityS675 - 1;
  _M0L4selfS674->$3 = _M0L6_2atmpS1766;
  _M0L8capacityS1768 = _M0L4selfS674->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1767 = _M0FPB21calc__grow__threshold(_M0L8capacityS1768);
  _M0L4selfS674->$4 = _M0L6_2atmpS1767;
  _M0L4selfS674->$1 = 0;
  _M0L6_2atmpS1769 = 0;
  _M0L6_2aoldS2235 = _M0L4selfS674->$5;
  if (_M0L6_2aoldS2235) {
    moonbit_decref(_M0L6_2aoldS2235);
  }
  _M0L4selfS674->$5 = _M0L6_2atmpS1769;
  _M0L4selfS674->$6 = -1;
  _M0L1xS676 = _M0L9old__headS673;
  while (1) {
    if (_M0L1xS676 == 0) {
      if (_M0L1xS676) {
        moonbit_decref(_M0L1xS676);
      }
      moonbit_decref(_M0L4selfS674);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS677 =
        _M0L1xS676;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS678 =
        _M0L7_2aSomeS677;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS679 =
        _M0L4_2axS678->$1;
      int32_t _M0L6_2akeyS680 = _M0L4_2axS678->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS681 =
        _M0L4_2axS678->$5;
      int32_t _M0L7_2ahashS682 = _M0L4_2axS678->$3;
      int32_t _M0L6_2acntS2378 = Moonbit_object_header(_M0L4_2axS678)->rc;
      if (_M0L6_2acntS2378 > 1) {
        int32_t _M0L11_2anew__cntS2379 = _M0L6_2acntS2378 - 1;
        Moonbit_object_header(_M0L4_2axS678)->rc = _M0L11_2anew__cntS2379;
        moonbit_incref(_M0L8_2avalueS681);
        if (_M0L7_2anextS679) {
          moonbit_incref(_M0L7_2anextS679);
        }
      } else if (_M0L6_2acntS2378 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS678);
      }
      moonbit_incref(_M0L4selfS674);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS674, _M0L6_2akeyS680, _M0L8_2avalueS681, _M0L7_2ahashS682);
      _M0L1xS676 = _M0L7_2anextS679;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS633,
  moonbit_string_t _M0L3keyS639,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS640,
  int32_t _M0L4hashS635
) {
  int32_t _M0L14capacity__maskS1738;
  int32_t _M0L6_2atmpS1737;
  int32_t _M0L3pslS630;
  int32_t _M0L3idxS631;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1738 = _M0L4selfS633->$3;
  _M0L6_2atmpS1737 = _M0L4hashS635 & _M0L14capacity__maskS1738;
  _M0L3pslS630 = 0;
  _M0L3idxS631 = _M0L6_2atmpS1737;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1736 =
      _M0L4selfS633->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS632;
    if (
      _M0L3idxS631 < 0
      || _M0L3idxS631 >= Moonbit_array_length(_M0L7entriesS1736)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS632
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1736[
        _M0L3idxS631
      ];
    if (_M0L7_2abindS632 == 0) {
      int32_t _M0L4sizeS1721 = _M0L4selfS633->$1;
      int32_t _M0L8grow__atS1722 = _M0L4selfS633->$4;
      int32_t _M0L7_2abindS636;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS637;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS638;
      if (_M0L4sizeS1721 >= _M0L8grow__atS1722) {
        int32_t _M0L14capacity__maskS1724;
        int32_t _M0L6_2atmpS1723;
        moonbit_incref(_M0L4selfS633);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS633);
        _M0L14capacity__maskS1724 = _M0L4selfS633->$3;
        _M0L6_2atmpS1723 = _M0L4hashS635 & _M0L14capacity__maskS1724;
        _M0L3pslS630 = 0;
        _M0L3idxS631 = _M0L6_2atmpS1723;
        continue;
      }
      _M0L7_2abindS636 = _M0L4selfS633->$6;
      _M0L7_2abindS637 = 0;
      _M0L5entryS638
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS638)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS638->$0 = _M0L7_2abindS636;
      _M0L5entryS638->$1 = _M0L7_2abindS637;
      _M0L5entryS638->$2 = _M0L3pslS630;
      _M0L5entryS638->$3 = _M0L4hashS635;
      _M0L5entryS638->$4 = _M0L3keyS639;
      _M0L5entryS638->$5 = _M0L5valueS640;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS633, _M0L3idxS631, _M0L5entryS638);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS641 =
        _M0L7_2abindS632;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS642 =
        _M0L7_2aSomeS641;
      int32_t _M0L4hashS1726 = _M0L14_2acurr__entryS642->$3;
      int32_t _if__result_2490;
      int32_t _M0L3pslS1727;
      int32_t _M0L6_2atmpS1732;
      int32_t _M0L6_2atmpS1734;
      int32_t _M0L14capacity__maskS1735;
      int32_t _M0L6_2atmpS1733;
      if (_M0L4hashS1726 == _M0L4hashS635) {
        moonbit_string_t _M0L3keyS1725 = _M0L14_2acurr__entryS642->$4;
        #line 134 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_2490
        = moonbit_val_array_equal(_M0L3keyS1725, _M0L3keyS639);
      } else {
        _if__result_2490 = 0;
      }
      if (_if__result_2490) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2238;
        moonbit_incref(_M0L14_2acurr__entryS642);
        moonbit_decref(_M0L3keyS639);
        moonbit_decref(_M0L4selfS633);
        _M0L6_2aoldS2238 = _M0L14_2acurr__entryS642->$5;
        moonbit_decref(_M0L6_2aoldS2238);
        _M0L14_2acurr__entryS642->$5 = _M0L5valueS640;
        moonbit_decref(_M0L14_2acurr__entryS642);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS642);
      }
      _M0L3pslS1727 = _M0L14_2acurr__entryS642->$2;
      if (_M0L3pslS630 > _M0L3pslS1727) {
        int32_t _M0L4sizeS1728 = _M0L4selfS633->$1;
        int32_t _M0L8grow__atS1729 = _M0L4selfS633->$4;
        int32_t _M0L7_2abindS643;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS644;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS645;
        if (_M0L4sizeS1728 >= _M0L8grow__atS1729) {
          int32_t _M0L14capacity__maskS1731;
          int32_t _M0L6_2atmpS1730;
          moonbit_decref(_M0L14_2acurr__entryS642);
          moonbit_incref(_M0L4selfS633);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS633);
          _M0L14capacity__maskS1731 = _M0L4selfS633->$3;
          _M0L6_2atmpS1730 = _M0L4hashS635 & _M0L14capacity__maskS1731;
          _M0L3pslS630 = 0;
          _M0L3idxS631 = _M0L6_2atmpS1730;
          continue;
        }
        moonbit_incref(_M0L4selfS633);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS633, _M0L3idxS631, _M0L14_2acurr__entryS642);
        _M0L7_2abindS643 = _M0L4selfS633->$6;
        _M0L7_2abindS644 = 0;
        _M0L5entryS645
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS645)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS645->$0 = _M0L7_2abindS643;
        _M0L5entryS645->$1 = _M0L7_2abindS644;
        _M0L5entryS645->$2 = _M0L3pslS630;
        _M0L5entryS645->$3 = _M0L4hashS635;
        _M0L5entryS645->$4 = _M0L3keyS639;
        _M0L5entryS645->$5 = _M0L5valueS640;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS633, _M0L3idxS631, _M0L5entryS645);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS642);
      }
      _M0L6_2atmpS1732 = _M0L3pslS630 + 1;
      _M0L6_2atmpS1734 = _M0L3idxS631 + 1;
      _M0L14capacity__maskS1735 = _M0L4selfS633->$3;
      _M0L6_2atmpS1733 = _M0L6_2atmpS1734 & _M0L14capacity__maskS1735;
      _M0L3pslS630 = _M0L6_2atmpS1732;
      _M0L3idxS631 = _M0L6_2atmpS1733;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS649,
  int32_t _M0L3keyS655,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS656,
  int32_t _M0L4hashS651
) {
  int32_t _M0L14capacity__maskS1756;
  int32_t _M0L6_2atmpS1755;
  int32_t _M0L3pslS646;
  int32_t _M0L3idxS647;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1756 = _M0L4selfS649->$3;
  _M0L6_2atmpS1755 = _M0L4hashS651 & _M0L14capacity__maskS1756;
  _M0L3pslS646 = 0;
  _M0L3idxS647 = _M0L6_2atmpS1755;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1754 =
      _M0L4selfS649->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS648;
    if (
      _M0L3idxS647 < 0
      || _M0L3idxS647 >= Moonbit_array_length(_M0L7entriesS1754)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS648
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1754[
        _M0L3idxS647
      ];
    if (_M0L7_2abindS648 == 0) {
      int32_t _M0L4sizeS1739 = _M0L4selfS649->$1;
      int32_t _M0L8grow__atS1740 = _M0L4selfS649->$4;
      int32_t _M0L7_2abindS652;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS653;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS654;
      if (_M0L4sizeS1739 >= _M0L8grow__atS1740) {
        int32_t _M0L14capacity__maskS1742;
        int32_t _M0L6_2atmpS1741;
        moonbit_incref(_M0L4selfS649);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS649);
        _M0L14capacity__maskS1742 = _M0L4selfS649->$3;
        _M0L6_2atmpS1741 = _M0L4hashS651 & _M0L14capacity__maskS1742;
        _M0L3pslS646 = 0;
        _M0L3idxS647 = _M0L6_2atmpS1741;
        continue;
      }
      _M0L7_2abindS652 = _M0L4selfS649->$6;
      _M0L7_2abindS653 = 0;
      _M0L5entryS654
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS654)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS654->$0 = _M0L7_2abindS652;
      _M0L5entryS654->$1 = _M0L7_2abindS653;
      _M0L5entryS654->$2 = _M0L3pslS646;
      _M0L5entryS654->$3 = _M0L4hashS651;
      _M0L5entryS654->$4 = _M0L3keyS655;
      _M0L5entryS654->$5 = _M0L5valueS656;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS649, _M0L3idxS647, _M0L5entryS654);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS657 =
        _M0L7_2abindS648;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS658 =
        _M0L7_2aSomeS657;
      int32_t _M0L4hashS1744 = _M0L14_2acurr__entryS658->$3;
      int32_t _if__result_2492;
      int32_t _M0L3pslS1745;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L6_2atmpS1752;
      int32_t _M0L14capacity__maskS1753;
      int32_t _M0L6_2atmpS1751;
      if (_M0L4hashS1744 == _M0L4hashS651) {
        int32_t _M0L3keyS1743 = _M0L14_2acurr__entryS658->$4;
        _if__result_2492 = _M0L3keyS1743 == _M0L3keyS655;
      } else {
        _if__result_2492 = 0;
      }
      if (_if__result_2492) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2242;
        moonbit_incref(_M0L14_2acurr__entryS658);
        moonbit_decref(_M0L4selfS649);
        _M0L6_2aoldS2242 = _M0L14_2acurr__entryS658->$5;
        moonbit_decref(_M0L6_2aoldS2242);
        _M0L14_2acurr__entryS658->$5 = _M0L5valueS656;
        moonbit_decref(_M0L14_2acurr__entryS658);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS658);
      }
      _M0L3pslS1745 = _M0L14_2acurr__entryS658->$2;
      if (_M0L3pslS646 > _M0L3pslS1745) {
        int32_t _M0L4sizeS1746 = _M0L4selfS649->$1;
        int32_t _M0L8grow__atS1747 = _M0L4selfS649->$4;
        int32_t _M0L7_2abindS659;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS660;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS661;
        if (_M0L4sizeS1746 >= _M0L8grow__atS1747) {
          int32_t _M0L14capacity__maskS1749;
          int32_t _M0L6_2atmpS1748;
          moonbit_decref(_M0L14_2acurr__entryS658);
          moonbit_incref(_M0L4selfS649);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS649);
          _M0L14capacity__maskS1749 = _M0L4selfS649->$3;
          _M0L6_2atmpS1748 = _M0L4hashS651 & _M0L14capacity__maskS1749;
          _M0L3pslS646 = 0;
          _M0L3idxS647 = _M0L6_2atmpS1748;
          continue;
        }
        moonbit_incref(_M0L4selfS649);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS649, _M0L3idxS647, _M0L14_2acurr__entryS658);
        _M0L7_2abindS659 = _M0L4selfS649->$6;
        _M0L7_2abindS660 = 0;
        _M0L5entryS661
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS661)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS661->$0 = _M0L7_2abindS659;
        _M0L5entryS661->$1 = _M0L7_2abindS660;
        _M0L5entryS661->$2 = _M0L3pslS646;
        _M0L5entryS661->$3 = _M0L4hashS651;
        _M0L5entryS661->$4 = _M0L3keyS655;
        _M0L5entryS661->$5 = _M0L5valueS656;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS649, _M0L3idxS647, _M0L5entryS661);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS658);
      }
      _M0L6_2atmpS1750 = _M0L3pslS646 + 1;
      _M0L6_2atmpS1752 = _M0L3idxS647 + 1;
      _M0L14capacity__maskS1753 = _M0L4selfS649->$3;
      _M0L6_2atmpS1751 = _M0L6_2atmpS1752 & _M0L14capacity__maskS1753;
      _M0L3pslS646 = _M0L6_2atmpS1750;
      _M0L3idxS647 = _M0L6_2atmpS1751;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS614,
  int32_t _M0L3idxS619,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS618
) {
  int32_t _M0L3pslS1704;
  int32_t _M0L6_2atmpS1700;
  int32_t _M0L6_2atmpS1702;
  int32_t _M0L14capacity__maskS1703;
  int32_t _M0L6_2atmpS1701;
  int32_t _M0L3pslS610;
  int32_t _M0L3idxS611;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS612;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1704 = _M0L5entryS618->$2;
  _M0L6_2atmpS1700 = _M0L3pslS1704 + 1;
  _M0L6_2atmpS1702 = _M0L3idxS619 + 1;
  _M0L14capacity__maskS1703 = _M0L4selfS614->$3;
  _M0L6_2atmpS1701 = _M0L6_2atmpS1702 & _M0L14capacity__maskS1703;
  _M0L3pslS610 = _M0L6_2atmpS1700;
  _M0L3idxS611 = _M0L6_2atmpS1701;
  _M0L5entryS612 = _M0L5entryS618;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1699 =
      _M0L4selfS614->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS613;
    if (
      _M0L3idxS611 < 0
      || _M0L3idxS611 >= Moonbit_array_length(_M0L7entriesS1699)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS613
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1699[
        _M0L3idxS611
      ];
    if (_M0L7_2abindS613 == 0) {
      _M0L5entryS612->$2 = _M0L3pslS610;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS614, _M0L5entryS612, _M0L3idxS611);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS616 =
        _M0L7_2abindS613;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS617 =
        _M0L7_2aSomeS616;
      int32_t _M0L3pslS1689 = _M0L14_2acurr__entryS617->$2;
      if (_M0L3pslS610 > _M0L3pslS1689) {
        int32_t _M0L3pslS1694;
        int32_t _M0L6_2atmpS1690;
        int32_t _M0L6_2atmpS1692;
        int32_t _M0L14capacity__maskS1693;
        int32_t _M0L6_2atmpS1691;
        _M0L5entryS612->$2 = _M0L3pslS610;
        moonbit_incref(_M0L14_2acurr__entryS617);
        moonbit_incref(_M0L4selfS614);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS614, _M0L5entryS612, _M0L3idxS611);
        _M0L3pslS1694 = _M0L14_2acurr__entryS617->$2;
        _M0L6_2atmpS1690 = _M0L3pslS1694 + 1;
        _M0L6_2atmpS1692 = _M0L3idxS611 + 1;
        _M0L14capacity__maskS1693 = _M0L4selfS614->$3;
        _M0L6_2atmpS1691 = _M0L6_2atmpS1692 & _M0L14capacity__maskS1693;
        _M0L3pslS610 = _M0L6_2atmpS1690;
        _M0L3idxS611 = _M0L6_2atmpS1691;
        _M0L5entryS612 = _M0L14_2acurr__entryS617;
        continue;
      } else {
        int32_t _M0L6_2atmpS1695 = _M0L3pslS610 + 1;
        int32_t _M0L6_2atmpS1697 = _M0L3idxS611 + 1;
        int32_t _M0L14capacity__maskS1698 = _M0L4selfS614->$3;
        int32_t _M0L6_2atmpS1696 =
          _M0L6_2atmpS1697 & _M0L14capacity__maskS1698;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_2494 =
          _M0L5entryS612;
        _M0L3pslS610 = _M0L6_2atmpS1695;
        _M0L3idxS611 = _M0L6_2atmpS1696;
        _M0L5entryS612 = _tmp_2494;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS624,
  int32_t _M0L3idxS629,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS628
) {
  int32_t _M0L3pslS1720;
  int32_t _M0L6_2atmpS1716;
  int32_t _M0L6_2atmpS1718;
  int32_t _M0L14capacity__maskS1719;
  int32_t _M0L6_2atmpS1717;
  int32_t _M0L3pslS620;
  int32_t _M0L3idxS621;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS622;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1720 = _M0L5entryS628->$2;
  _M0L6_2atmpS1716 = _M0L3pslS1720 + 1;
  _M0L6_2atmpS1718 = _M0L3idxS629 + 1;
  _M0L14capacity__maskS1719 = _M0L4selfS624->$3;
  _M0L6_2atmpS1717 = _M0L6_2atmpS1718 & _M0L14capacity__maskS1719;
  _M0L3pslS620 = _M0L6_2atmpS1716;
  _M0L3idxS621 = _M0L6_2atmpS1717;
  _M0L5entryS622 = _M0L5entryS628;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1715 =
      _M0L4selfS624->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS623;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS1715)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS623
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1715[
        _M0L3idxS621
      ];
    if (_M0L7_2abindS623 == 0) {
      _M0L5entryS622->$2 = _M0L3pslS620;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624, _M0L5entryS622, _M0L3idxS621);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS626 =
        _M0L7_2abindS623;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS627 =
        _M0L7_2aSomeS626;
      int32_t _M0L3pslS1705 = _M0L14_2acurr__entryS627->$2;
      if (_M0L3pslS620 > _M0L3pslS1705) {
        int32_t _M0L3pslS1710;
        int32_t _M0L6_2atmpS1706;
        int32_t _M0L6_2atmpS1708;
        int32_t _M0L14capacity__maskS1709;
        int32_t _M0L6_2atmpS1707;
        _M0L5entryS622->$2 = _M0L3pslS620;
        moonbit_incref(_M0L14_2acurr__entryS627);
        moonbit_incref(_M0L4selfS624);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS624, _M0L5entryS622, _M0L3idxS621);
        _M0L3pslS1710 = _M0L14_2acurr__entryS627->$2;
        _M0L6_2atmpS1706 = _M0L3pslS1710 + 1;
        _M0L6_2atmpS1708 = _M0L3idxS621 + 1;
        _M0L14capacity__maskS1709 = _M0L4selfS624->$3;
        _M0L6_2atmpS1707 = _M0L6_2atmpS1708 & _M0L14capacity__maskS1709;
        _M0L3pslS620 = _M0L6_2atmpS1706;
        _M0L3idxS621 = _M0L6_2atmpS1707;
        _M0L5entryS622 = _M0L14_2acurr__entryS627;
        continue;
      } else {
        int32_t _M0L6_2atmpS1711 = _M0L3pslS620 + 1;
        int32_t _M0L6_2atmpS1713 = _M0L3idxS621 + 1;
        int32_t _M0L14capacity__maskS1714 = _M0L4selfS624->$3;
        int32_t _M0L6_2atmpS1712 =
          _M0L6_2atmpS1713 & _M0L14capacity__maskS1714;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_2496 =
          _M0L5entryS622;
        _M0L3pslS620 = _M0L6_2atmpS1711;
        _M0L3idxS621 = _M0L6_2atmpS1712;
        _M0L5entryS622 = _tmp_2496;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS598,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS600,
  int32_t _M0L8new__idxS599
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1685;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1686;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2250;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2249;
  int32_t _M0L6_2acntS2380;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS601;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1685 = _M0L4selfS598->$0;
  moonbit_incref(_M0L5entryS600);
  _M0L6_2atmpS1686 = _M0L5entryS600;
  if (
    _M0L8new__idxS599 < 0
    || _M0L8new__idxS599 >= Moonbit_array_length(_M0L7entriesS1685)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2250
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1685[
      _M0L8new__idxS599
    ];
  if (_M0L6_2aoldS2250) {
    moonbit_decref(_M0L6_2aoldS2250);
  }
  _M0L7entriesS1685[_M0L8new__idxS599] = _M0L6_2atmpS1686;
  _M0L8_2afieldS2249 = _M0L5entryS600->$1;
  _M0L6_2acntS2380 = Moonbit_object_header(_M0L5entryS600)->rc;
  if (_M0L6_2acntS2380 > 1) {
    int32_t _M0L11_2anew__cntS2383 = _M0L6_2acntS2380 - 1;
    Moonbit_object_header(_M0L5entryS600)->rc = _M0L11_2anew__cntS2383;
    if (_M0L8_2afieldS2249) {
      moonbit_incref(_M0L8_2afieldS2249);
    }
  } else if (_M0L6_2acntS2380 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2382 =
      _M0L5entryS600->$5;
    moonbit_string_t _M0L8_2afieldS2381;
    moonbit_decref(_M0L8_2afieldS2382);
    _M0L8_2afieldS2381 = _M0L5entryS600->$4;
    moonbit_decref(_M0L8_2afieldS2381);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS600);
  }
  _M0L7_2abindS601 = _M0L8_2afieldS2249;
  if (_M0L7_2abindS601 == 0) {
    if (_M0L7_2abindS601) {
      moonbit_decref(_M0L7_2abindS601);
    }
    _M0L4selfS598->$6 = _M0L8new__idxS599;
    moonbit_decref(_M0L4selfS598);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS602;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS603;
    moonbit_decref(_M0L4selfS598);
    _M0L7_2aSomeS602 = _M0L7_2abindS601;
    _M0L7_2anextS603 = _M0L7_2aSomeS602;
    _M0L7_2anextS603->$0 = _M0L8new__idxS599;
    moonbit_decref(_M0L7_2anextS603);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS604,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS606,
  int32_t _M0L8new__idxS605
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1687;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1688;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2253;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2252;
  int32_t _M0L6_2acntS2384;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS607;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1687 = _M0L4selfS604->$0;
  moonbit_incref(_M0L5entryS606);
  _M0L6_2atmpS1688 = _M0L5entryS606;
  if (
    _M0L8new__idxS605 < 0
    || _M0L8new__idxS605 >= Moonbit_array_length(_M0L7entriesS1687)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2253
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1687[
      _M0L8new__idxS605
    ];
  if (_M0L6_2aoldS2253) {
    moonbit_decref(_M0L6_2aoldS2253);
  }
  _M0L7entriesS1687[_M0L8new__idxS605] = _M0L6_2atmpS1688;
  _M0L8_2afieldS2252 = _M0L5entryS606->$1;
  _M0L6_2acntS2384 = Moonbit_object_header(_M0L5entryS606)->rc;
  if (_M0L6_2acntS2384 > 1) {
    int32_t _M0L11_2anew__cntS2386 = _M0L6_2acntS2384 - 1;
    Moonbit_object_header(_M0L5entryS606)->rc = _M0L11_2anew__cntS2386;
    if (_M0L8_2afieldS2252) {
      moonbit_incref(_M0L8_2afieldS2252);
    }
  } else if (_M0L6_2acntS2384 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2385 =
      _M0L5entryS606->$5;
    moonbit_decref(_M0L8_2afieldS2385);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS606);
  }
  _M0L7_2abindS607 = _M0L8_2afieldS2252;
  if (_M0L7_2abindS607 == 0) {
    if (_M0L7_2abindS607) {
      moonbit_decref(_M0L7_2abindS607);
    }
    _M0L4selfS604->$6 = _M0L8new__idxS605;
    moonbit_decref(_M0L4selfS604);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS608;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS609;
    moonbit_decref(_M0L4selfS604);
    _M0L7_2aSomeS608 = _M0L7_2abindS607;
    _M0L7_2anextS609 = _M0L7_2aSomeS608;
    _M0L7_2anextS609->$0 = _M0L8new__idxS605;
    moonbit_decref(_M0L7_2anextS609);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS591,
  int32_t _M0L3idxS593,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS592
) {
  int32_t _M0L7_2abindS590;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1672;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1673;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2255;
  int32_t _M0L4sizeS1675;
  int32_t _M0L6_2atmpS1674;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS590 = _M0L4selfS591->$6;
  switch (_M0L7_2abindS590) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1667;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2257;
      moonbit_incref(_M0L5entryS592);
      _M0L6_2atmpS1667 = _M0L5entryS592;
      _M0L6_2aoldS2257 = _M0L4selfS591->$5;
      if (_M0L6_2aoldS2257) {
        moonbit_decref(_M0L6_2aoldS2257);
      }
      _M0L4selfS591->$5 = _M0L6_2atmpS1667;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1671 =
        _M0L4selfS591->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1670;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1668;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1669;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2258;
      if (
        _M0L7_2abindS590 < 0
        || _M0L7_2abindS590 >= Moonbit_array_length(_M0L7entriesS1671)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1670
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1671[
          _M0L7_2abindS590
        ];
      if (_M0L6_2atmpS1670) {
        moonbit_incref(_M0L6_2atmpS1670);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1668
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1670);
      moonbit_incref(_M0L5entryS592);
      _M0L6_2atmpS1669 = _M0L5entryS592;
      _M0L6_2aoldS2258 = _M0L6_2atmpS1668->$1;
      if (_M0L6_2aoldS2258) {
        moonbit_decref(_M0L6_2aoldS2258);
      }
      _M0L6_2atmpS1668->$1 = _M0L6_2atmpS1669;
      moonbit_decref(_M0L6_2atmpS1668);
      break;
    }
  }
  _M0L4selfS591->$6 = _M0L3idxS593;
  _M0L7entriesS1672 = _M0L4selfS591->$0;
  _M0L6_2atmpS1673 = _M0L5entryS592;
  if (
    _M0L3idxS593 < 0
    || _M0L3idxS593 >= Moonbit_array_length(_M0L7entriesS1672)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2255
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1672[
      _M0L3idxS593
    ];
  if (_M0L6_2aoldS2255) {
    moonbit_decref(_M0L6_2aoldS2255);
  }
  _M0L7entriesS1672[_M0L3idxS593] = _M0L6_2atmpS1673;
  _M0L4sizeS1675 = _M0L4selfS591->$1;
  _M0L6_2atmpS1674 = _M0L4sizeS1675 + 1;
  _M0L4selfS591->$1 = _M0L6_2atmpS1674;
  moonbit_decref(_M0L4selfS591);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS595,
  int32_t _M0L3idxS597,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS596
) {
  int32_t _M0L7_2abindS594;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1681;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1682;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2261;
  int32_t _M0L4sizeS1684;
  int32_t _M0L6_2atmpS1683;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS594 = _M0L4selfS595->$6;
  switch (_M0L7_2abindS594) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1676;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2263;
      moonbit_incref(_M0L5entryS596);
      _M0L6_2atmpS1676 = _M0L5entryS596;
      _M0L6_2aoldS2263 = _M0L4selfS595->$5;
      if (_M0L6_2aoldS2263) {
        moonbit_decref(_M0L6_2aoldS2263);
      }
      _M0L4selfS595->$5 = _M0L6_2atmpS1676;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1680 =
        _M0L4selfS595->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1679;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1677;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1678;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2264;
      if (
        _M0L7_2abindS594 < 0
        || _M0L7_2abindS594 >= Moonbit_array_length(_M0L7entriesS1680)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1679
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1680[
          _M0L7_2abindS594
        ];
      if (_M0L6_2atmpS1679) {
        moonbit_incref(_M0L6_2atmpS1679);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1677
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1679);
      moonbit_incref(_M0L5entryS596);
      _M0L6_2atmpS1678 = _M0L5entryS596;
      _M0L6_2aoldS2264 = _M0L6_2atmpS1677->$1;
      if (_M0L6_2aoldS2264) {
        moonbit_decref(_M0L6_2aoldS2264);
      }
      _M0L6_2atmpS1677->$1 = _M0L6_2atmpS1678;
      moonbit_decref(_M0L6_2atmpS1677);
      break;
    }
  }
  _M0L4selfS595->$6 = _M0L3idxS597;
  _M0L7entriesS1681 = _M0L4selfS595->$0;
  _M0L6_2atmpS1682 = _M0L5entryS596;
  if (
    _M0L3idxS597 < 0
    || _M0L3idxS597 >= Moonbit_array_length(_M0L7entriesS1681)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2261
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1681[
      _M0L3idxS597
    ];
  if (_M0L6_2aoldS2261) {
    moonbit_decref(_M0L6_2aoldS2261);
  }
  _M0L7entriesS1681[_M0L3idxS597] = _M0L6_2atmpS1682;
  _M0L4sizeS1684 = _M0L4selfS595->$1;
  _M0L6_2atmpS1683 = _M0L4sizeS1684 + 1;
  _M0L4selfS595->$1 = _M0L6_2atmpS1683;
  moonbit_decref(_M0L4selfS595);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS579
) {
  int32_t _M0L8capacityS578;
  int32_t _M0L7_2abindS580;
  int32_t _M0L7_2abindS581;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1665;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS582;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS583;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_2497;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS578
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS579);
  _M0L7_2abindS580 = _M0L8capacityS578 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS581 = _M0FPB21calc__grow__threshold(_M0L8capacityS578);
  _M0L6_2atmpS1665 = 0;
  _M0L7_2abindS582
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS578, _M0L6_2atmpS1665);
  _M0L7_2abindS583 = 0;
  _block_2497
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_2497)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_2497->$0 = _M0L7_2abindS582;
  _block_2497->$1 = 0;
  _block_2497->$2 = _M0L8capacityS578;
  _block_2497->$3 = _M0L7_2abindS580;
  _block_2497->$4 = _M0L7_2abindS581;
  _block_2497->$5 = _M0L7_2abindS583;
  _block_2497->$6 = -1;
  return _block_2497;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS585
) {
  int32_t _M0L8capacityS584;
  int32_t _M0L7_2abindS586;
  int32_t _M0L7_2abindS587;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1666;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS588;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS589;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_2498;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS584
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS585);
  _M0L7_2abindS586 = _M0L8capacityS584 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS587 = _M0FPB21calc__grow__threshold(_M0L8capacityS584);
  _M0L6_2atmpS1666 = 0;
  _M0L7_2abindS588
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS584, _M0L6_2atmpS1666);
  _M0L7_2abindS589 = 0;
  _block_2498
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_2498)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_2498->$0 = _M0L7_2abindS588;
  _block_2498->$1 = 0;
  _block_2498->$2 = _M0L8capacityS584;
  _block_2498->$3 = _M0L7_2abindS586;
  _block_2498->$4 = _M0L7_2abindS587;
  _block_2498->$5 = _M0L7_2abindS589;
  _block_2498->$6 = -1;
  return _block_2498;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS577) {
  #line 33 "/Users/user/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS577 >= 0) {
    int32_t _M0L6_2atmpS1664;
    int32_t _M0L6_2atmpS1663;
    int32_t _M0L6_2atmpS1662;
    int32_t _M0L6_2atmpS1661;
    if (_M0L4selfS577 <= 1) {
      return 1;
    }
    if (_M0L4selfS577 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1664 = _M0L4selfS577 - 1;
    #line 44 "/Users/user/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS1663 = moonbit_clz32(_M0L6_2atmpS1664);
    _M0L6_2atmpS1662 = _M0L6_2atmpS1663 - 1;
    _M0L6_2atmpS1661 = 2147483647 >> (_M0L6_2atmpS1662 & 31);
    return _M0L6_2atmpS1661 + 1;
  } else {
    #line 34 "/Users/user/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS576) {
  int32_t _M0L6_2atmpS1660;
  #line 510 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1660 = _M0L8capacityS576 * 13;
  return _M0L6_2atmpS1660 / 16;
}

moonbit_string_t _M0MPC16option6Option6unwrapGsE(
  moonbit_string_t _M0L4selfS570
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS570 == 0) {
    if (_M0L4selfS570) {
      moonbit_decref(_M0L4selfS570);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    moonbit_string_t _M0L7_2aSomeS571 = _M0L4selfS570;
    return _M0L7_2aSomeS571;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS572
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS572 == 0) {
    if (_M0L4selfS572) {
      moonbit_decref(_M0L4selfS572);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS573 =
      _M0L4selfS572;
    return _M0L7_2aSomeS573;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS574
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS574 == 0) {
    if (_M0L4selfS574) {
      moonbit_decref(_M0L4selfS574);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS575 =
      _M0L4selfS574;
    return _M0L7_2aSomeS575;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS569
) {
  moonbit_string_t* _M0L6_2atmpS1659;
  #line 165 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS1659 = _M0L4selfS569;
  #line 167 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1659);
}

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS568,
  struct _M0TPB6Logger _M0L6loggerS567
) {
  struct _M0TWEOs* _M0L6_2atmpS1658;
  #line 304 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 305 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1658 = _M0MPC15array5Array4iterGsE(_M0L4selfS568);
  #line 305 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPB6Logger19write__iter_2einnerGsE(_M0L6loggerS567, _M0L6_2atmpS1658, (moonbit_string_t)moonbit_string_literal_78.data, (moonbit_string_t)moonbit_string_literal_79.data, (moonbit_string_t)moonbit_string_literal_59.data, 0);
  return 0;
}

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS566
) {
  moonbit_string_t* _M0L3bufS1656;
  int32_t _M0L3lenS1657;
  int32_t _M0L6_2acntS2387;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1655;
  #line 1656 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS1656 = _M0L4selfS566->$0;
  _M0L3lenS1657 = _M0L4selfS566->$1;
  _M0L6_2acntS2387 = Moonbit_object_header(_M0L4selfS566)->rc;
  if (_M0L6_2acntS2387 > 1) {
    int32_t _M0L11_2anew__cntS2388 = _M0L6_2acntS2387 - 1;
    Moonbit_object_header(_M0L4selfS566)->rc = _M0L11_2anew__cntS2388;
    moonbit_incref(_M0L3bufS1656);
  } else if (_M0L6_2acntS2387 == 1) {
    #line 1658 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS566);
  }
  _M0L6_2atmpS1655
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1657, _M0L3bufS1656
  };
  #line 1658 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1655);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS565
) {
  moonbit_string_t* _M0L6_2atmpS1653;
  int32_t _M0L6_2atmpS1654;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1652;
  #line 1509 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS565);
  _M0L6_2atmpS1653 = _M0L4selfS565;
  _M0L6_2atmpS1654 = Moonbit_array_length(_M0L4selfS565);
  moonbit_decref(_M0L4selfS565);
  _M0L6_2atmpS1652
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1654, _M0L6_2atmpS1653
  };
  #line 1511 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1652);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS563
) {
  struct _M0TPB8MutLocalGiE* _M0L1iS562;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__* _closure_2499;
  struct _M0TWEOs* _M0L6_2atmpS1640;
  #line 677 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS562
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS562)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS562->$0 = 0;
  _closure_2499
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__));
  Moonbit_object_header(_closure_2499)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__, $0_0) >> 2, 2, 0);
  _closure_2499->code = &_M0MPC15array9ArrayView4iterGsEC1641l680;
  _closure_2499->$0_0 = _M0L4selfS563.$0;
  _closure_2499->$0_1 = _M0L4selfS563.$1;
  _closure_2499->$0_2 = _M0L4selfS563.$2;
  _closure_2499->$1 = _M0L1iS562;
  _M0L6_2atmpS1640 = (struct _M0TWEOs*)_closure_2499;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1640);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1641l680(
  struct _M0TWEOs* _M0L6_2aenvS1642
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__* _M0L14_2acasted__envS1643;
  struct _M0TPB8MutLocalGiE* _M0L1iS562;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2270;
  int32_t _M0L6_2acntS2389;
  struct _M0TPB9ArrayViewGsE _M0L4selfS563;
  int32_t _M0L3valS1644;
  int32_t _M0L6_2atmpS1645;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1643
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1641__l680__*)_M0L6_2aenvS1642;
  _M0L1iS562 = _M0L14_2acasted__envS1643->$1;
  _M0L8_2afieldS2270
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1643->$0_1,
      _M0L14_2acasted__envS1643->$0_2,
      _M0L14_2acasted__envS1643->$0_0
  };
  _M0L6_2acntS2389 = Moonbit_object_header(_M0L14_2acasted__envS1643)->rc;
  if (_M0L6_2acntS2389 > 1) {
    int32_t _M0L11_2anew__cntS2390 = _M0L6_2acntS2389 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1643)->rc
    = _M0L11_2anew__cntS2390;
    moonbit_incref(_M0L1iS562);
    moonbit_incref(_M0L8_2afieldS2270.$0);
  } else if (_M0L6_2acntS2389 == 1) {
    #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1643);
  }
  _M0L4selfS563 = _M0L8_2afieldS2270;
  _M0L3valS1644 = _M0L1iS562->$0;
  moonbit_incref(_M0L4selfS563.$0);
  #line 681 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1645 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS563);
  if (_M0L3valS1644 < _M0L6_2atmpS1645) {
    moonbit_string_t* _M0L3bufS1648 = _M0L4selfS563.$0;
    int32_t _M0L5startS1650 = _M0L4selfS563.$1;
    int32_t _M0L3valS1651 = _M0L1iS562->$0;
    int32_t _M0L6_2atmpS1649 = _M0L5startS1650 + _M0L3valS1651;
    moonbit_string_t _M0L6_2atmpS2268 =
      (moonbit_string_t)_M0L3bufS1648[_M0L6_2atmpS1649];
    moonbit_string_t _M0L4elemS564;
    int32_t _M0L3valS1647;
    int32_t _M0L6_2atmpS1646;
    moonbit_incref(_M0L6_2atmpS2268);
    moonbit_decref(_M0L3bufS1648);
    _M0L4elemS564 = _M0L6_2atmpS2268;
    _M0L3valS1647 = _M0L1iS562->$0;
    _M0L6_2atmpS1646 = _M0L3valS1647 + 1;
    _M0L1iS562->$0 = _M0L6_2atmpS1646;
    moonbit_decref(_M0L1iS562);
    return _M0L4elemS564;
  } else {
    moonbit_decref(_M0L4selfS563.$0);
    moonbit_decref(_M0L1iS562);
    return 0;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS560,
  struct _M0TPB6Logger _M0L6loggerS561
) {
  int32_t _M0L6_2atmpS1639;
  struct _M0TPC16string10StringView _M0L6_2atmpS1638;
  #line 244 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1639 = Moonbit_array_length(_M0L4selfS560);
  _M0L6_2atmpS1638
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1639, _M0L4selfS560
  };
  #line 245 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1638, _M0L6loggerS561, 1);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS559) {
  #line 45 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 46 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS559, 10);
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS558,
  struct _M0TPB6Logger _M0L6loggerS557
) {
  moonbit_string_t _M0L6_2atmpS1637;
  #line 26 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1637 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS558);
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS557.$0->$method_0(_M0L6loggerS557.$1, _M0L6_2atmpS1637);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS556) {
  #line 31 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS556) {
    return (moonbit_string_t)moonbit_string_literal_27.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_80.data;
  }
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS555
) {
  int32_t _M0L6_2atmpS1636;
  #line 24 "/Users/user/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS1636 = Moonbit_array_length(_M0L4selfS555);
  return (struct _M0TPC16string10StringView){0,
                                               _M0L6_2atmpS1636,
                                               _M0L4selfS555};
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t _M0L4selfS553,
  struct _M0TPC16string10StringView _M0L3sepS554
) {
  int32_t _M0L6_2atmpS1635;
  struct _M0TPC16string10StringView _M0L6_2atmpS1634;
  #line 1098 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1635 = Moonbit_array_length(_M0L4selfS553);
  _M0L6_2atmpS1634
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1635, _M0L4selfS553
  };
  #line 1099 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC16string10StringView5split(_M0L6_2atmpS1634, _M0L3sepS554);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView _M0L4selfS544,
  struct _M0TPC16string10StringView _M0L3sepS543
) {
  int32_t _M0L8sep__lenS542;
  void* _M0L4SomeS1633;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L9remainingS546;
  struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__* _closure_2500;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS1623;
  #line 1069 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L3sepS543.$0);
  #line 1073 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L8sep__lenS542 = _M0MPC16string10StringView6length(_M0L3sepS543);
  if (_M0L8sep__lenS542 == 0) {
    struct _M0TWEOc* _M0L6_2atmpS1618;
    struct _M0TWcERPC16string10StringView* _M0L6_2atmpS1619;
    moonbit_decref(_M0L3sepS543.$0);
    #line 1075 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS1618 = _M0MPC16string10StringView4iter(_M0L4selfS544);
    _M0L6_2atmpS1619
    = (struct _M0TWcERPC16string10StringView*)&_M0MPC16string10StringView5splitC1620l1075$closure.data;
    #line 1075 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0MPB4Iter3mapGcRPC16string10StringViewE(_M0L6_2atmpS1618, _M0L6_2atmpS1619);
  }
  _M0L4SomeS1633
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS1633)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1633)->$0_0
  = _M0L4selfS544.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1633)->$0_1
  = _M0L4selfS544.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1633)->$0_2
  = _M0L4selfS544.$2;
  _M0L9remainingS546
  = (struct _M0TPB8MutLocalGORPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPC16string10StringViewE));
  Moonbit_object_header(_M0L9remainingS546)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB8MutLocalGORPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L9remainingS546->$0 = _M0L4SomeS1633;
  _closure_2500
  = (struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__*)moonbit_malloc(sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__));
  Moonbit_object_header(_closure_2500)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__, $0) >> 2, 2, 0);
  _closure_2500->code = &_M0MPC16string10StringView5splitC1624l1078;
  _closure_2500->$0 = _M0L9remainingS546;
  _closure_2500->$1_0 = _M0L3sepS543.$0;
  _closure_2500->$1_1 = _M0L3sepS543.$1;
  _closure_2500->$1_2 = _M0L3sepS543.$2;
  _closure_2500->$2 = _M0L8sep__lenS542;
  _M0L6_2atmpS1623
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_2500;
  #line 1078 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS1623);
}

void* _M0MPC16string10StringView5splitC1624l1078(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS1625
) {
  struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__* _M0L14_2acasted__envS1626;
  int32_t _M0L8sep__lenS542;
  struct _M0TPC16string10StringView _M0L3sepS543;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L8_2afieldS2276;
  int32_t _M0L6_2acntS2391;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L9remainingS546;
  void* _M0L7_2abindS547;
  #line 1078 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L14_2acasted__envS1626
  = (struct _M0R44StringView_3a_3asplit_2eanon__u1624__l1078__*)_M0L6_2aenvS1625;
  _M0L8sep__lenS542 = _M0L14_2acasted__envS1626->$2;
  _M0L3sepS543
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1626->$1_1,
      _M0L14_2acasted__envS1626->$1_2,
      _M0L14_2acasted__envS1626->$1_0
  };
  _M0L8_2afieldS2276 = _M0L14_2acasted__envS1626->$0;
  _M0L6_2acntS2391 = Moonbit_object_header(_M0L14_2acasted__envS1626)->rc;
  if (_M0L6_2acntS2391 > 1) {
    int32_t _M0L11_2anew__cntS2392 = _M0L6_2acntS2391 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1626)->rc
    = _M0L11_2anew__cntS2392;
    moonbit_incref(_M0L3sepS543.$0);
    moonbit_incref(_M0L8_2afieldS2276);
  } else if (_M0L6_2acntS2391 == 1) {
    #line 1078 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    moonbit_free(_M0L14_2acasted__envS1626);
  }
  _M0L9remainingS546 = _M0L8_2afieldS2276;
  _M0L7_2abindS547 = _M0L9remainingS546->$0;
  switch (Moonbit_object_tag(_M0L7_2abindS547)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS548 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS547;
      struct _M0TPC16string10StringView _M0L7_2aviewS549 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS548->$0_1,
                                              _M0L7_2aSomeS548->$0_2,
                                              _M0L7_2aSomeS548->$0_0};
      int64_t _M0L7_2abindS550;
      moonbit_incref(_M0L7_2aviewS549.$0);
      moonbit_incref(_M0L7_2aviewS549.$0);
      #line 1080 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
      _M0L7_2abindS550
      = _M0MPC16string10StringView4find(_M0L7_2aviewS549, _M0L3sepS543);
      if (_M0L7_2abindS550 == 4294967296ll) {
        void* _M0L4NoneS1627 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        void* _M0L6_2aoldS2272 = _M0L9remainingS546->$0;
        void* _block_2501;
        moonbit_decref(_M0L6_2aoldS2272);
        _M0L9remainingS546->$0 = _M0L4NoneS1627;
        moonbit_decref(_M0L9remainingS546);
        _block_2501
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_2501)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2501)->$0_0
        = _M0L7_2aviewS549.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2501)->$0_1
        = _M0L7_2aviewS549.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2501)->$0_2
        = _M0L7_2aviewS549.$2;
        return _block_2501;
      } else {
        int64_t _M0L7_2aSomeS551 = _M0L7_2abindS550;
        int32_t _M0L6_2aendS552 = (int32_t)_M0L7_2aSomeS551;
        int32_t _M0L6_2atmpS1630 = _M0L6_2aendS552 + _M0L8sep__lenS542;
        struct _M0TPC16string10StringView _M0L6_2atmpS1629;
        void* _M0L4SomeS1628;
        void* _M0L6_2aoldS2273;
        int64_t _M0L6_2atmpS1632;
        struct _M0TPC16string10StringView _M0L6_2atmpS1631;
        void* _block_2502;
        moonbit_incref(_M0L7_2aviewS549.$0);
        #line 1084 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS1629
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS549, _M0L6_2atmpS1630, 4294967296ll);
        _M0L4SomeS1628
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_M0L4SomeS1628)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1628)->$0_0
        = _M0L6_2atmpS1629.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1628)->$0_1
        = _M0L6_2atmpS1629.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1628)->$0_2
        = _M0L6_2atmpS1629.$2;
        _M0L6_2aoldS2273 = _M0L9remainingS546->$0;
        moonbit_decref(_M0L6_2aoldS2273);
        _M0L9remainingS546->$0 = _M0L4SomeS1628;
        moonbit_decref(_M0L9remainingS546);
        _M0L6_2atmpS1632 = (int64_t)_M0L6_2aendS552;
        #line 1085 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS1631
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS549, 0, _M0L6_2atmpS1632);
        _block_2502
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_2502)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2502)->$0_0
        = _M0L6_2atmpS1631.$0;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2502)->$0_1
        = _M0L6_2atmpS1631.$1;
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2502)->$0_2
        = _M0L6_2atmpS1631.$2;
        return _block_2502;
      }
      break;
    }
    default: {
      moonbit_decref(_M0L9remainingS546);
      moonbit_decref(_M0L3sepS543.$0);
      return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      break;
    }
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC1620l1075(
  struct _M0TWcERPC16string10StringView* _M0L6_2aenvS1621,
  int32_t _M0L1cS545
) {
  moonbit_string_t _M0L6_2atmpS1622;
  #line 1075 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_decref(_M0L6_2aenvS1621);
  #line 1075 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1622 = _M0IPC14char4CharPB4Show10to__string(_M0L1cS545);
  #line 1075 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC16string6String12view_2einner(_M0L6_2atmpS1622, 0, 4294967296ll);
}

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t _M0L4selfS541) {
  #line 435 "/Users/user/.moon/lib/core/builtin/char.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/char.mbt"
  return _M0FPB16char__to__string(_M0L4selfS541);
}

moonbit_string_t _M0FPB16char__to__string(int32_t _M0L4charS540) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS539;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1617;
  #line 441 "/Users/user/.moon/lib/core/builtin/char.mbt"
  #line 443 "/Users/user/.moon/lib/core/builtin/char.mbt"
  _M0L7_2aselfS539 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L7_2aselfS539);
  #line 443 "/Users/user/.moon/lib/core/builtin/char.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS539, _M0L4charS540);
  _M0L6_2atmpS1617 = _M0L7_2aselfS539;
  #line 443 "/Users/user/.moon/lib/core/builtin/char.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1617);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TWEOc* _M0L4selfS535,
  struct _M0TWcERPC16string10StringView* _M0L1fS538
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__* _closure_2503;
  #line 318 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _closure_2503
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__*)moonbit_malloc(sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__));
  Moonbit_object_header(_closure_2503)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__, $0) >> 2, 2, 0);
  _closure_2503->code = &_M0MPB4Iter3mapGcRPC16string10StringViewEC1613l319;
  _closure_2503->$0 = _M0L1fS538;
  _closure_2503->$1 = _M0L4selfS535;
  return (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_2503;
}

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC1613l319(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS1614
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__* _M0L14_2acasted__envS1615;
  struct _M0TWEOc* _M0L4selfS535;
  struct _M0TWcERPC16string10StringView* _M0L8_2afieldS2278;
  int32_t _M0L6_2acntS2393;
  struct _M0TWcERPC16string10StringView* _M0L1fS538;
  int32_t _M0L7_2abindS534;
  #line 319 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L14_2acasted__envS1615
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u1613__l319__*)_M0L6_2aenvS1614;
  _M0L4selfS535 = _M0L14_2acasted__envS1615->$1;
  _M0L8_2afieldS2278 = _M0L14_2acasted__envS1615->$0;
  _M0L6_2acntS2393 = Moonbit_object_header(_M0L14_2acasted__envS1615)->rc;
  if (_M0L6_2acntS2393 > 1) {
    int32_t _M0L11_2anew__cntS2394 = _M0L6_2acntS2393 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1615)->rc
    = _M0L11_2anew__cntS2394;
    moonbit_incref(_M0L4selfS535);
    moonbit_incref(_M0L8_2afieldS2278);
  } else if (_M0L6_2acntS2393 == 1) {
    #line 319 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS1615);
  }
  _M0L1fS538 = _M0L8_2afieldS2278;
  #line 320 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2abindS534 = _M0MPB4Iter4nextGcE(_M0L4selfS535);
  if (_M0L7_2abindS534 == -1) {
    moonbit_decref(_M0L1fS538);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int32_t _M0L7_2aSomeS536 = _M0L7_2abindS534;
    int32_t _M0L4_2axS537 = _M0L7_2aSomeS536;
    struct _M0TPC16string10StringView _M0L6_2atmpS1616;
    void* _block_2504;
    #line 321 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
    _M0L6_2atmpS1616 = _M0L1fS538->code(_M0L1fS538, _M0L4_2axS537);
    _block_2504
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_2504)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2504)->$0_0
    = _M0L6_2atmpS1616.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2504)->$0_1
    = _M0L6_2atmpS1616.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_2504)->$0_2
    = _M0L6_2atmpS1616.$2;
    return _block_2504;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS528,
  moonbit_string_t _M0L5valueS530
) {
  int32_t _M0L3lenS1603;
  moonbit_string_t* _M0L6_2atmpS1605;
  int32_t _M0L6_2atmpS1604;
  int32_t _M0L6lengthS529;
  moonbit_string_t* _M0L3bufS1606;
  moonbit_string_t _M0L6_2aoldS2280;
  int32_t _M0L6_2atmpS1607;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1603 = _M0L4selfS528->$1;
  moonbit_incref(_M0L4selfS528);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1605 = _M0MPC15array5Array6bufferGsE(_M0L4selfS528);
  _M0L6_2atmpS1604 = Moonbit_array_length(_M0L6_2atmpS1605);
  moonbit_decref(_M0L6_2atmpS1605);
  if (_M0L3lenS1603 == _M0L6_2atmpS1604) {
    moonbit_incref(_M0L4selfS528);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS528);
  }
  _M0L6lengthS529 = _M0L4selfS528->$1;
  _M0L3bufS1606 = _M0L4selfS528->$0;
  _M0L6_2aoldS2280 = (moonbit_string_t)_M0L3bufS1606[_M0L6lengthS529];
  moonbit_decref(_M0L6_2aoldS2280);
  _M0L3bufS1606[_M0L6lengthS529] = _M0L5valueS530;
  _M0L6_2atmpS1607 = _M0L6lengthS529 + 1;
  _M0L4selfS528->$1 = _M0L6_2atmpS1607;
  moonbit_decref(_M0L4selfS528);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS531,
  struct _M0TUsiE* _M0L5valueS533
) {
  int32_t _M0L3lenS1608;
  struct _M0TUsiE** _M0L6_2atmpS1610;
  int32_t _M0L6_2atmpS1609;
  int32_t _M0L6lengthS532;
  struct _M0TUsiE** _M0L3bufS1611;
  struct _M0TUsiE* _M0L6_2aoldS2282;
  int32_t _M0L6_2atmpS1612;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1608 = _M0L4selfS531->$1;
  moonbit_incref(_M0L4selfS531);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1610 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS531);
  _M0L6_2atmpS1609 = Moonbit_array_length(_M0L6_2atmpS1610);
  moonbit_decref(_M0L6_2atmpS1610);
  if (_M0L3lenS1608 == _M0L6_2atmpS1609) {
    moonbit_incref(_M0L4selfS531);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS531);
  }
  _M0L6lengthS532 = _M0L4selfS531->$1;
  _M0L3bufS1611 = _M0L4selfS531->$0;
  _M0L6_2aoldS2282 = (struct _M0TUsiE*)_M0L3bufS1611[_M0L6lengthS532];
  if (_M0L6_2aoldS2282) {
    moonbit_decref(_M0L6_2aoldS2282);
  }
  _M0L3bufS1611[_M0L6lengthS532] = _M0L5valueS533;
  _M0L6_2atmpS1612 = _M0L6lengthS532 + 1;
  _M0L4selfS531->$1 = _M0L6_2atmpS1612;
  moonbit_decref(_M0L4selfS531);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS523) {
  int32_t _M0L8old__capS522;
  int32_t _M0L8new__capS524;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS522 = _M0L4selfS523->$1;
  if (_M0L8old__capS522 == 0) {
    _M0L8new__capS524 = 8;
  } else {
    _M0L8new__capS524 = _M0L8old__capS522 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS523, _M0L8new__capS524);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS526
) {
  int32_t _M0L8old__capS525;
  int32_t _M0L8new__capS527;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS525 = _M0L4selfS526->$1;
  if (_M0L8old__capS525 == 0) {
    _M0L8new__capS527 = 8;
  } else {
    _M0L8new__capS527 = _M0L8old__capS525 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS526, _M0L8new__capS527);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS513,
  int32_t _M0L13new__capacityS511
) {
  moonbit_string_t* _M0L8new__bufS510;
  moonbit_string_t* _M0L8old__bufS512;
  int32_t _M0L8old__capS514;
  int32_t _M0L9copy__lenS515;
  moonbit_string_t* _M0L6_2aoldS2284;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS510
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS511, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8old__bufS512 = _M0L4selfS513->$0;
  _M0L8old__capS514 = Moonbit_array_length(_M0L8old__bufS512);
  if (_M0L8old__capS514 < _M0L13new__capacityS511) {
    _M0L9copy__lenS515 = _M0L8old__capS514;
  } else {
    _M0L9copy__lenS515 = _M0L13new__capacityS511;
  }
  moonbit_incref(_M0L8old__bufS512);
  moonbit_incref(_M0L8new__bufS510);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS510, 0, _M0L8old__bufS512, 0, _M0L9copy__lenS515);
  _M0L6_2aoldS2284 = _M0L4selfS513->$0;
  moonbit_decref(_M0L6_2aoldS2284);
  _M0L4selfS513->$0 = _M0L8new__bufS510;
  moonbit_decref(_M0L4selfS513);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS519,
  int32_t _M0L13new__capacityS517
) {
  struct _M0TUsiE** _M0L8new__bufS516;
  struct _M0TUsiE** _M0L8old__bufS518;
  int32_t _M0L8old__capS520;
  int32_t _M0L9copy__lenS521;
  struct _M0TUsiE** _M0L6_2aoldS2286;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS516
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS517, 0);
  _M0L8old__bufS518 = _M0L4selfS519->$0;
  _M0L8old__capS520 = Moonbit_array_length(_M0L8old__bufS518);
  if (_M0L8old__capS520 < _M0L13new__capacityS517) {
    _M0L9copy__lenS521 = _M0L8old__capS520;
  } else {
    _M0L9copy__lenS521 = _M0L13new__capacityS517;
  }
  moonbit_incref(_M0L8old__bufS518);
  moonbit_incref(_M0L8new__bufS516);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS516, 0, _M0L8old__bufS518, 0, _M0L9copy__lenS521);
  _M0L6_2aoldS2286 = _M0L4selfS519->$0;
  moonbit_decref(_M0L6_2aoldS2286);
  _M0L4selfS519->$0 = _M0L8new__bufS516;
  moonbit_decref(_M0L4selfS519);
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS509) {
  int32_t _result_2505;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_2505 = _M0L4selfS509->$1;
  moonbit_decref(_M0L4selfS509);
  return _result_2505;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS507
) {
  moonbit_string_t* _M0L8_2afieldS2288;
  int32_t _M0L6_2acntS2395;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2288 = _M0L4selfS507->$0;
  _M0L6_2acntS2395 = Moonbit_object_header(_M0L4selfS507)->rc;
  if (_M0L6_2acntS2395 > 1) {
    int32_t _M0L11_2anew__cntS2396 = _M0L6_2acntS2395 - 1;
    Moonbit_object_header(_M0L4selfS507)->rc = _M0L11_2anew__cntS2396;
    moonbit_incref(_M0L8_2afieldS2288);
  } else if (_M0L6_2acntS2395 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS507);
  }
  return _M0L8_2afieldS2288;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS508
) {
  struct _M0TUsiE** _M0L8_2afieldS2289;
  int32_t _M0L6_2acntS2397;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2289 = _M0L4selfS508->$0;
  _M0L6_2acntS2397 = Moonbit_object_header(_M0L4selfS508)->rc;
  if (_M0L6_2acntS2397 > 1) {
    int32_t _M0L11_2anew__cntS2398 = _M0L6_2acntS2397 - 1;
    Moonbit_object_header(_M0L4selfS508)->rc = _M0L11_2anew__cntS2398;
    moonbit_incref(_M0L8_2afieldS2289);
  } else if (_M0L6_2acntS2397 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS508);
  }
  return _M0L8_2afieldS2289;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS506
) {
  #line 53 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS506 == 0) {
    moonbit_string_t* _M0L6_2atmpS1601 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2506 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2506)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2506->$0 = _M0L6_2atmpS1601;
    _block_2506->$1 = 0;
    return _block_2506;
  } else {
    moonbit_string_t* _M0L6_2atmpS1602 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS506, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2507 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2507)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2507->$0 = _M0L6_2atmpS1602;
    _block_2507->$1 = 0;
    return _block_2507;
  }
}

int32_t _M0MPC16string6String11has__prefix(
  moonbit_string_t _M0L4selfS504,
  struct _M0TPC16string10StringView _M0L3strS505
) {
  int32_t _M0L6_2atmpS1600;
  struct _M0TPC16string10StringView _M0L6_2atmpS1599;
  #line 298 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1600 = Moonbit_array_length(_M0L4selfS504);
  _M0L6_2atmpS1599
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1600, _M0L4selfS504
  };
  #line 300 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC16string10StringView11has__prefix(_M0L6_2atmpS1599, _M0L3strS505);
}

int32_t _M0MPC16string10StringView11has__prefix(
  struct _M0TPC16string10StringView _M0L4selfS500,
  struct _M0TPC16string10StringView _M0L3strS501
) {
  int64_t _M0L7_2abindS499;
  #line 291 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  #line 293 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS499
  = _M0MPC16string10StringView4find(_M0L4selfS500, _M0L3strS501);
  if (_M0L7_2abindS499 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS502 = _M0L7_2abindS499;
    int32_t _M0L4_2aiS503 = (int32_t)_M0L7_2aSomeS502;
    return _M0L4_2aiS503 == 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS498
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS498;
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS497,
  struct _M0TPC16string10StringView _M0L3strS496
) {
  int32_t _M0L6_2atmpS1598;
  #line 18 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L3strS496.$0);
  #line 19 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS1598 = _M0MPC16string10StringView6length(_M0L3strS496);
  if (_M0L6_2atmpS1598 <= 4) {
    #line 20 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS497, _M0L3strS496);
  } else {
    #line 22 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS497, _M0L3strS496);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS487,
  struct _M0TPC16string10StringView _M0L6needleS489
) {
  int32_t _M0L13haystack__lenS486;
  int32_t _M0L11needle__lenS488;
  #line 31 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L8haystackS487.$0);
  #line 32 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L13haystack__lenS486
  = _M0MPC16string10StringView6length(_M0L8haystackS487);
  moonbit_incref(_M0L6needleS489.$0);
  #line 33 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L11needle__lenS488 = _M0MPC16string10StringView6length(_M0L6needleS489);
  if (_M0L11needle__lenS488 > 0) {
    if (_M0L13haystack__lenS486 >= _M0L11needle__lenS488) {
      int32_t _M0L13needle__firstS490;
      int32_t _M0L12forward__lenS491;
      int32_t _M0L1iS492;
      moonbit_incref(_M0L6needleS489.$0);
      #line 36 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
      _M0L13needle__firstS490
      = _M0MPC16string10StringView11unsafe__get(_M0L6needleS489, 0);
      _M0L12forward__lenS491
      = _M0L13haystack__lenS486 - _M0L11needle__lenS488;
      _M0L1iS492 = 0;
      while (1) {
        if (_M0L1iS492 <= _M0L12forward__lenS491) {
          int32_t _M0L6_2atmpS1591;
          int32_t _M0L1jS494;
          int32_t _M0L6_2atmpS1597;
          moonbit_incref(_M0L8haystackS487.$0);
          #line 39 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1591
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS487, _M0L1iS492);
          if (_M0L6_2atmpS1591 != _M0L13needle__firstS490) {
            int32_t _M0L6_2atmpS1592 = _M0L1iS492 + 1;
            _M0L1iS492 = _M0L6_2atmpS1592;
            continue;
          }
          _M0L1jS494 = 1;
          while (1) {
            if (_M0L1jS494 < _M0L11needle__lenS488) {
              int32_t _M0L6_2atmpS1595 = _M0L1iS492 + _M0L1jS494;
              int32_t _M0L6_2atmpS1593;
              int32_t _M0L6_2atmpS1594;
              int32_t _M0L6_2atmpS1596;
              moonbit_incref(_M0L8haystackS487.$0);
              #line 44 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1593
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS487, _M0L6_2atmpS1595);
              moonbit_incref(_M0L6needleS489.$0);
              #line 44 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1594
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS489, _M0L1jS494);
              if (_M0L6_2atmpS1593 != _M0L6_2atmpS1594) {
                break;
              }
              _M0L6_2atmpS1596 = _M0L1jS494 + 1;
              _M0L1jS494 = _M0L6_2atmpS1596;
              continue;
            } else {
              moonbit_decref(_M0L6needleS489.$0);
              moonbit_decref(_M0L8haystackS487.$0);
              return (int64_t)_M0L1iS492;
            }
            break;
          }
          _M0L6_2atmpS1597 = _M0L1iS492 + 1;
          _M0L1iS492 = _M0L6_2atmpS1597;
          continue;
        } else {
          moonbit_decref(_M0L6needleS489.$0);
          moonbit_decref(_M0L8haystackS487.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS489.$0);
      moonbit_decref(_M0L8haystackS487.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS489.$0);
    moonbit_decref(_M0L8haystackS487.$0);
    return _M0FPB18brute__force__findN6constrS9146;
  }
}

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView _M0L8haystackS474,
  struct _M0TPC16string10StringView _M0L6needleS476
) {
  int32_t _M0L13haystack__lenS473;
  int32_t _M0L11needle__lenS475;
  #line 58 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  moonbit_incref(_M0L8haystackS474.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L13haystack__lenS473
  = _M0MPC16string10StringView6length(_M0L8haystackS474);
  moonbit_incref(_M0L6needleS476.$0);
  #line 63 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
  _M0L11needle__lenS475 = _M0MPC16string10StringView6length(_M0L6needleS476);
  if (_M0L11needle__lenS475 > 0) {
    if (_M0L13haystack__lenS473 >= _M0L11needle__lenS475) {
      int32_t* _M0L11skip__tableS477 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS475);
      int32_t _M0L7_2abindS478 = _M0L11needle__lenS475 - 1;
      int32_t _M0L1iS479 = 0;
      int32_t _M0L1iS481;
      while (1) {
        if (_M0L1iS479 < _M0L7_2abindS478) {
          int32_t _M0L6_2atmpS1577;
          int32_t _M0L6_2atmpS1576;
          int32_t _M0L6_2atmpS1573;
          int32_t _M0L6_2atmpS1575;
          int32_t _M0L6_2atmpS1574;
          int32_t _M0L6_2atmpS1578;
          moonbit_incref(_M0L6needleS476.$0);
          #line 69 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1577
          = _M0MPC16string10StringView11unsafe__get(_M0L6needleS476, _M0L1iS479);
          _M0L6_2atmpS1576 = (int32_t)_M0L6_2atmpS1577;
          _M0L6_2atmpS1573 = _M0L6_2atmpS1576 & 255;
          _M0L6_2atmpS1575 = _M0L11needle__lenS475 - 1;
          _M0L6_2atmpS1574 = _M0L6_2atmpS1575 - _M0L1iS479;
          if (
            _M0L6_2atmpS1573 < 0
            || _M0L6_2atmpS1573
               >= Moonbit_array_length(_M0L11skip__tableS477)
          ) {
            #line 69 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS477[_M0L6_2atmpS1573] = _M0L6_2atmpS1574;
          _M0L6_2atmpS1578 = _M0L1iS479 + 1;
          _M0L1iS479 = _M0L6_2atmpS1578;
          continue;
        }
        break;
      }
      _M0L1iS481 = 0;
      while (1) {
        int32_t _M0L6_2atmpS1579 =
          _M0L13haystack__lenS473 - _M0L11needle__lenS475;
        if (_M0L1iS481 <= _M0L6_2atmpS1579) {
          int32_t _M0L7_2abindS482 = _M0L11needle__lenS475 - 1;
          int32_t _M0L1jS483 = 0;
          int32_t _M0L6_2atmpS1590;
          int32_t _M0L6_2atmpS1589;
          int32_t _M0L6_2atmpS1588;
          int32_t _M0L6_2atmpS1587;
          int32_t _M0L6_2atmpS1586;
          int32_t _M0L6_2atmpS1585;
          int32_t _M0L6_2atmpS1584;
          while (1) {
            if (_M0L1jS483 <= _M0L7_2abindS482) {
              int32_t _M0L6_2atmpS1582 = _M0L1iS481 + _M0L1jS483;
              int32_t _M0L6_2atmpS1580;
              int32_t _M0L6_2atmpS1581;
              int32_t _M0L6_2atmpS1583;
              moonbit_incref(_M0L8haystackS474.$0);
              #line 77 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1580
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS474, _M0L6_2atmpS1582);
              moonbit_incref(_M0L6needleS476.$0);
              #line 77 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
              _M0L6_2atmpS1581
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS476, _M0L1jS483);
              if (_M0L6_2atmpS1580 != _M0L6_2atmpS1581) {
                break;
              }
              _M0L6_2atmpS1583 = _M0L1jS483 + 1;
              _M0L1jS483 = _M0L6_2atmpS1583;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS477);
              moonbit_decref(_M0L6needleS476.$0);
              moonbit_decref(_M0L8haystackS474.$0);
              return (int64_t)_M0L1iS481;
            }
            break;
          }
          _M0L6_2atmpS1590 = _M0L1iS481 + _M0L11needle__lenS475;
          _M0L6_2atmpS1589 = _M0L6_2atmpS1590 - 1;
          moonbit_incref(_M0L8haystackS474.$0);
          #line 74 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS1588
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS474, _M0L6_2atmpS1589);
          _M0L6_2atmpS1587 = (int32_t)_M0L6_2atmpS1588;
          _M0L6_2atmpS1586 = _M0L6_2atmpS1587 & 255;
          if (
            _M0L6_2atmpS1586 < 0
            || _M0L6_2atmpS1586
               >= Moonbit_array_length(_M0L11skip__tableS477)
          ) {
            #line 74 "/Users/user/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1585 = (int32_t)_M0L11skip__tableS477[_M0L6_2atmpS1586];
          _M0L6_2atmpS1584 = _M0L1iS481 + _M0L6_2atmpS1585;
          _M0L1iS481 = _M0L6_2atmpS1584;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS477);
          moonbit_decref(_M0L6needleS476.$0);
          moonbit_decref(_M0L8haystackS474.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS476.$0);
      moonbit_decref(_M0L8haystackS474.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS476.$0);
    moonbit_decref(_M0L8haystackS474.$0);
    return _M0FPB28boyer__moore__horspool__findN6constrS9145;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS472,
  struct _M0TPC16string10StringView _M0L3strS471
) {
  int32_t _M0L8str__lenS470;
  int32_t _M0L3lenS1566;
  int32_t _M0L6_2atmpS1565;
  uint16_t* _M0L4dataS1567;
  int32_t _M0L3lenS1568;
  moonbit_string_t _M0L6_2atmpS1569;
  int32_t _M0L6_2atmpS1570;
  int32_t _M0L3lenS1572;
  int32_t _M0L6_2atmpS1571;
  #line 126 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  moonbit_incref(_M0L3strS471.$0);
  #line 130 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS470 = _M0MPC16string10StringView6length(_M0L3strS471);
  _M0L3lenS1566 = _M0L4selfS472->$1;
  _M0L6_2atmpS1565 = _M0L3lenS1566 + _M0L8str__lenS470;
  moonbit_incref(_M0L4selfS472);
  #line 131 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS472, _M0L6_2atmpS1565);
  _M0L4dataS1567 = _M0L4selfS472->$0;
  _M0L3lenS1568 = _M0L4selfS472->$1;
  moonbit_incref(_M0L4dataS1567);
  moonbit_incref(_M0L3strS471.$0);
  #line 134 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1569 = _M0MPC16string10StringView4data(_M0L3strS471);
  #line 135 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1570 = _M0MPC16string10StringView13start__offset(_M0L3strS471);
  #line 132 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1567, _M0L3lenS1568, _M0L6_2atmpS1569, _M0L6_2atmpS1570, _M0L8str__lenS470);
  _M0L3lenS1572 = _M0L4selfS472->$1;
  _M0L6_2atmpS1571 = _M0L3lenS1572 + _M0L8str__lenS470;
  _M0L4selfS472->$1 = _M0L6_2atmpS1571;
  moonbit_decref(_M0L4selfS472);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS462,
  int32_t _M0L3lenS465,
  int32_t _M0L13start__offsetS469,
  int64_t _M0L11end__offsetS460
) {
  int32_t _M0L11end__offsetS459;
  int32_t _M0L5indexS463;
  int32_t _M0L5countS464;
  #line 441 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS460 == 4294967296ll) {
    _M0L11end__offsetS459 = Moonbit_array_length(_M0L4selfS462);
  } else {
    int64_t _M0L7_2aSomeS461 = _M0L11end__offsetS460;
    _M0L11end__offsetS459 = (int32_t)_M0L7_2aSomeS461;
  }
  _M0L5indexS463 = _M0L13start__offsetS469;
  _M0L5countS464 = 0;
  while (1) {
    int32_t _if__result_2514;
    if (_M0L5indexS463 < _M0L11end__offsetS459) {
      _if__result_2514 = _M0L5countS464 < _M0L3lenS465;
    } else {
      _if__result_2514 = 0;
    }
    if (_if__result_2514) {
      int32_t _M0L2c1S466 = _M0L4selfS462[_M0L5indexS463];
      int32_t _if__result_2515;
      int32_t _M0L6_2atmpS1563;
      int32_t _M0L6_2atmpS1564;
      #line 452 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S466)) {
        int32_t _M0L6_2atmpS1559 = _M0L5indexS463 + 1;
        _if__result_2515 = _M0L6_2atmpS1559 < _M0L11end__offsetS459;
      } else {
        _if__result_2515 = 0;
      }
      if (_if__result_2515) {
        int32_t _M0L6_2atmpS1562 = _M0L5indexS463 + 1;
        int32_t _M0L2c2S467 = _M0L4selfS462[_M0L6_2atmpS1562];
        #line 454 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S467)) {
          int32_t _M0L6_2atmpS1560 = _M0L5indexS463 + 2;
          int32_t _M0L6_2atmpS1561 = _M0L5countS464 + 1;
          _M0L5indexS463 = _M0L6_2atmpS1560;
          _M0L5countS464 = _M0L6_2atmpS1561;
          continue;
        } else {
          #line 457 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_81.data);
        }
      }
      _M0L6_2atmpS1563 = _M0L5indexS463 + 1;
      _M0L6_2atmpS1564 = _M0L5countS464 + 1;
      _M0L5indexS463 = _M0L6_2atmpS1563;
      _M0L5countS464 = _M0L6_2atmpS1564;
      continue;
    } else {
      moonbit_decref(_M0L4selfS462);
      return _M0L5countS464 >= _M0L3lenS465;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS451,
  int32_t _M0L3lenS454,
  int32_t _M0L13start__offsetS458,
  int64_t _M0L11end__offsetS449
) {
  int32_t _M0L11end__offsetS448;
  int32_t _M0L5indexS452;
  int32_t _M0L5countS453;
  #line 413 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS449 == 4294967296ll) {
    _M0L11end__offsetS448 = Moonbit_array_length(_M0L4selfS451);
  } else {
    int64_t _M0L7_2aSomeS450 = _M0L11end__offsetS449;
    _M0L11end__offsetS448 = (int32_t)_M0L7_2aSomeS450;
  }
  _M0L5indexS452 = _M0L13start__offsetS458;
  _M0L5countS453 = 0;
  while (1) {
    int32_t _if__result_2517;
    if (_M0L5indexS452 < _M0L11end__offsetS448) {
      _if__result_2517 = _M0L5countS453 < _M0L3lenS454;
    } else {
      _if__result_2517 = 0;
    }
    if (_if__result_2517) {
      int32_t _M0L2c1S455 = _M0L4selfS451[_M0L5indexS452];
      int32_t _if__result_2518;
      int32_t _M0L6_2atmpS1557;
      int32_t _M0L6_2atmpS1558;
      #line 424 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S455)) {
        int32_t _M0L6_2atmpS1553 = _M0L5indexS452 + 1;
        _if__result_2518 = _M0L6_2atmpS1553 < _M0L11end__offsetS448;
      } else {
        _if__result_2518 = 0;
      }
      if (_if__result_2518) {
        int32_t _M0L6_2atmpS1556 = _M0L5indexS452 + 1;
        int32_t _M0L2c2S456 = _M0L4selfS451[_M0L6_2atmpS1556];
        #line 426 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S456)) {
          int32_t _M0L6_2atmpS1554 = _M0L5indexS452 + 2;
          int32_t _M0L6_2atmpS1555 = _M0L5countS453 + 1;
          _M0L5indexS452 = _M0L6_2atmpS1554;
          _M0L5countS453 = _M0L6_2atmpS1555;
          continue;
        } else {
          #line 429 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_81.data);
        }
      }
      _M0L6_2atmpS1557 = _M0L5indexS452 + 1;
      _M0L6_2atmpS1558 = _M0L5countS453 + 1;
      _M0L5indexS452 = _M0L6_2atmpS1557;
      _M0L5countS453 = _M0L6_2atmpS1558;
      continue;
    } else {
      moonbit_decref(_M0L4selfS451);
      if (_M0L5countS453 == _M0L3lenS454) {
        return _M0L5indexS452 == _M0L11end__offsetS448;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS445
) {
  int32_t _M0L3endS1547;
  int32_t _M0L5startS1548;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1547 = _M0L4selfS445.$2;
  _M0L5startS1548 = _M0L4selfS445.$1;
  moonbit_decref(_M0L4selfS445.$0);
  return _M0L3endS1547 - _M0L5startS1548;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS446
) {
  int32_t _M0L3endS1549;
  int32_t _M0L5startS1550;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1549 = _M0L4selfS446.$2;
  _M0L5startS1550 = _M0L4selfS446.$1;
  moonbit_decref(_M0L4selfS446.$0);
  return _M0L3endS1549 - _M0L5startS1550;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS447
) {
  int32_t _M0L3endS1551;
  int32_t _M0L5startS1552;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1551 = _M0L4selfS447.$2;
  _M0L5startS1552 = _M0L4selfS447.$1;
  moonbit_decref(_M0L4selfS447.$0);
  return _M0L3endS1551 - _M0L5startS1552;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS443,
  int64_t _M0L19start__offset_2eoptS441,
  int64_t _M0L11end__offsetS444
) {
  int32_t _M0L13start__offsetS440;
  if (_M0L19start__offset_2eoptS441 == 4294967296ll) {
    _M0L13start__offsetS440 = 0;
  } else {
    int64_t _M0L7_2aSomeS442 = _M0L19start__offset_2eoptS441;
    _M0L13start__offsetS440 = (int32_t)_M0L7_2aSomeS442;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS443, _M0L13start__offsetS440, _M0L11end__offsetS444);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS438,
  int32_t _M0L13start__offsetS439,
  int64_t _M0L11end__offsetS436
) {
  int32_t _M0L11end__offsetS435;
  int32_t _if__result_2519;
  #line 512 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS436 == 4294967296ll) {
    _M0L11end__offsetS435 = Moonbit_array_length(_M0L4selfS438);
  } else {
    int64_t _M0L7_2aSomeS437 = _M0L11end__offsetS436;
    _M0L11end__offsetS435 = (int32_t)_M0L7_2aSomeS437;
  }
  if (_M0L13start__offsetS439 >= 0) {
    if (_M0L13start__offsetS439 <= _M0L11end__offsetS435) {
      int32_t _M0L6_2atmpS1546 = Moonbit_array_length(_M0L4selfS438);
      _if__result_2519 = _M0L11end__offsetS435 <= _M0L6_2atmpS1546;
    } else {
      _if__result_2519 = 0;
    }
  } else {
    _if__result_2519 = 0;
  }
  if (_if__result_2519) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS439,
                                                 _M0L11end__offsetS435,
                                                 _M0L4selfS438};
  } else {
    moonbit_decref(_M0L4selfS438);
    #line 521 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_82.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS430
) {
  int32_t _M0L5startS429;
  int32_t _M0L3endS431;
  struct _M0TPB8MutLocalGiE* _M0L5indexS432;
  struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__* _closure_2520;
  struct _M0TWEOc* _M0L6_2atmpS1525;
  #line 203 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L5startS429 = _M0L4selfS430.$1;
  _M0L3endS431 = _M0L4selfS430.$2;
  _M0L5indexS432
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5indexS432)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5indexS432->$0 = _M0L5startS429;
  _closure_2520
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__));
  Moonbit_object_header(_closure_2520)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__, $0) >> 2, 2, 0);
  _closure_2520->code = &_M0MPC16string10StringView4iterC1526l208;
  _closure_2520->$0 = _M0L5indexS432;
  _closure_2520->$1 = _M0L3endS431;
  _closure_2520->$2_0 = _M0L4selfS430.$0;
  _closure_2520->$2_1 = _M0L4selfS430.$1;
  _closure_2520->$2_2 = _M0L4selfS430.$2;
  _M0L6_2atmpS1525 = (struct _M0TWEOc*)_closure_2520;
  #line 208 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1525);
}

int32_t _M0MPC16string10StringView4iterC1526l208(
  struct _M0TWEOc* _M0L6_2aenvS1527
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__* _M0L14_2acasted__envS1528;
  struct _M0TPC16string10StringView _M0L4selfS430;
  int32_t _M0L3endS431;
  struct _M0TPB8MutLocalGiE* _M0L8_2afieldS2293;
  int32_t _M0L6_2acntS2399;
  struct _M0TPB8MutLocalGiE* _M0L5indexS432;
  int32_t _M0L3valS1529;
  #line 208 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L14_2acasted__envS1528
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1526__l208__*)_M0L6_2aenvS1527;
  _M0L4selfS430
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1528->$2_1,
      _M0L14_2acasted__envS1528->$2_2,
      _M0L14_2acasted__envS1528->$2_0
  };
  _M0L3endS431 = _M0L14_2acasted__envS1528->$1;
  _M0L8_2afieldS2293 = _M0L14_2acasted__envS1528->$0;
  _M0L6_2acntS2399 = Moonbit_object_header(_M0L14_2acasted__envS1528)->rc;
  if (_M0L6_2acntS2399 > 1) {
    int32_t _M0L11_2anew__cntS2400 = _M0L6_2acntS2399 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1528)->rc
    = _M0L11_2anew__cntS2400;
    moonbit_incref(_M0L4selfS430.$0);
    moonbit_incref(_M0L8_2afieldS2293);
  } else if (_M0L6_2acntS2399 == 1) {
    #line 208 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS1528);
  }
  _M0L5indexS432 = _M0L8_2afieldS2293;
  _M0L3valS1529 = _M0L5indexS432->$0;
  if (_M0L3valS1529 < _M0L3endS431) {
    moonbit_string_t _M0L3strS1544 = _M0L4selfS430.$0;
    int32_t _M0L3valS1545 = _M0L5indexS432->$0;
    int32_t _M0L2c1S433 = _M0L3strS1544[_M0L3valS1545];
    int32_t _if__result_2521;
    int32_t _M0L3valS1542;
    int32_t _M0L6_2atmpS1541;
    int32_t _M0L6_2atmpS1543;
    #line 211 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S433)) {
      int32_t _M0L3valS1532 = _M0L5indexS432->$0;
      int32_t _M0L6_2atmpS1530 = _M0L3valS1532 + 1;
      int32_t _M0L3endS1531 = _M0L4selfS430.$2;
      _if__result_2521 = _M0L6_2atmpS1530 < _M0L3endS1531;
    } else {
      _if__result_2521 = 0;
    }
    if (_if__result_2521) {
      moonbit_string_t _M0L3strS1538 = _M0L4selfS430.$0;
      int32_t _M0L3valS1540 = _M0L5indexS432->$0;
      int32_t _M0L6_2atmpS1539 = _M0L3valS1540 + 1;
      int32_t _M0L2c2S434 = _M0L3strS1538[_M0L6_2atmpS1539];
      moonbit_decref(_M0L3strS1538);
      #line 213 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S434)) {
        int32_t _M0L3valS1534 = _M0L5indexS432->$0;
        int32_t _M0L6_2atmpS1533 = _M0L3valS1534 + 2;
        int32_t _M0L6_2atmpS1536;
        int32_t _M0L6_2atmpS1537;
        int32_t _M0L6_2atmpS1535;
        _M0L5indexS432->$0 = _M0L6_2atmpS1533;
        moonbit_decref(_M0L5indexS432);
        _M0L6_2atmpS1536 = (int32_t)_M0L2c1S433;
        _M0L6_2atmpS1537 = (int32_t)_M0L2c2S434;
        #line 215 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        _M0L6_2atmpS1535
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1536, _M0L6_2atmpS1537);
        return _M0L6_2atmpS1535;
      }
    } else {
      moonbit_decref(_M0L4selfS430.$0);
    }
    _M0L3valS1542 = _M0L5indexS432->$0;
    _M0L6_2atmpS1541 = _M0L3valS1542 + 1;
    _M0L5indexS432->$0 = _M0L6_2atmpS1541;
    moonbit_decref(_M0L5indexS432);
    #line 219 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    _M0L6_2atmpS1543 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S433);
    return _M0L6_2atmpS1543;
  } else {
    moonbit_decref(_M0L5indexS432);
    moonbit_decref(_M0L4selfS430.$0);
    return -1;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS428
) {
  #line 197 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 198 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L4selfS428);
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS427
) {
  moonbit_string_t _M0L3strS1522;
  int32_t _M0L5startS1523;
  int32_t _M0L3endS1524;
  #line 190 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1522 = _M0L4selfS427.$0;
  _M0L5startS1523 = _M0L4selfS427.$1;
  _M0L3endS1524 = _M0L4selfS427.$2;
  #line 193 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1522, _M0L5startS1523, _M0L3endS1524);
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS424,
  int32_t _M0L5startS422,
  int32_t _M0L3endS423
) {
  int32_t _if__result_2522;
  int32_t _M0L3lenS425;
  int32_t _M0L6_2atmpS1520;
  int32_t _M0L6_2atmpS1521;
  moonbit_bytes_t _M0L5bytesS426;
  moonbit_bytes_t _M0L6_2atmpS1519;
  #line 91 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS422 == 0) {
    int32_t _M0L6_2atmpS1518 = Moonbit_array_length(_M0L3strS424);
    _if__result_2522 = _M0L3endS423 == _M0L6_2atmpS1518;
  } else {
    _if__result_2522 = 0;
  }
  if (_if__result_2522) {
    return _M0L3strS424;
  }
  _M0L3lenS425 = _M0L3endS423 - _M0L5startS422;
  _M0L6_2atmpS1520 = _M0L3lenS425 * 2;
  #line 101 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1521 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS426
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1520, _M0L6_2atmpS1521);
  moonbit_incref(_M0L5bytesS426);
  #line 102 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS426, 0, _M0L3strS424, _M0L5startS422, _M0L3lenS425);
  _M0L6_2atmpS1519 = _M0L5bytesS426;
  #line 103 "/Users/user/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1519, 0, 4294967296ll);
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS417,
  int32_t _M0L6offsetS421,
  int64_t _M0L6lengthS419
) {
  int32_t _M0L3lenS416;
  int32_t _M0L6lengthS418;
  int32_t _if__result_2523;
  #line 76 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS416 = Moonbit_array_length(_M0L4selfS417);
  if (_M0L6lengthS419 == 4294967296ll) {
    _M0L6lengthS418 = _M0L3lenS416 - _M0L6offsetS421;
  } else {
    int64_t _M0L7_2aSomeS420 = _M0L6lengthS419;
    _M0L6lengthS418 = (int32_t)_M0L7_2aSomeS420;
  }
  if (_M0L6offsetS421 >= 0) {
    if (_M0L6lengthS418 >= 0) {
      int32_t _M0L6_2atmpS1517 = _M0L6offsetS421 + _M0L6lengthS418;
      _if__result_2523 = _M0L6_2atmpS1517 <= _M0L3lenS416;
    } else {
      _if__result_2523 = 0;
    }
  } else {
    _if__result_2523 = 0;
  }
  if (_if__result_2523) {
    #line 84 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS417, _M0L6offsetS421, _M0L6lengthS418);
  } else {
    moonbit_decref(_M0L4selfS417);
    #line 83 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS408,
  int32_t _M0L13bytes__offsetS403,
  moonbit_string_t _M0L3strS410,
  int32_t _M0L11str__offsetS406,
  int32_t _M0L6lengthS404
) {
  int32_t _M0L6_2atmpS1516;
  int32_t _M0L6_2atmpS1515;
  int32_t _M0L2e1S402;
  int32_t _M0L6_2atmpS1514;
  int32_t _M0L2e2S405;
  int32_t _M0L4len1S407;
  int32_t _M0L4len2S409;
  int32_t _if__result_2524;
  #line 124 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1516 = _M0L6lengthS404 * 2;
  _M0L6_2atmpS1515 = _M0L13bytes__offsetS403 + _M0L6_2atmpS1516;
  _M0L2e1S402 = _M0L6_2atmpS1515 - 1;
  _M0L6_2atmpS1514 = _M0L11str__offsetS406 + _M0L6lengthS404;
  _M0L2e2S405 = _M0L6_2atmpS1514 - 1;
  _M0L4len1S407 = Moonbit_array_length(_M0L4selfS408);
  _M0L4len2S409 = Moonbit_array_length(_M0L3strS410);
  if (_M0L6lengthS404 >= 0) {
    if (_M0L13bytes__offsetS403 >= 0) {
      if (_M0L2e1S402 < _M0L4len1S407) {
        if (_M0L11str__offsetS406 >= 0) {
          _if__result_2524 = _M0L2e2S405 < _M0L4len2S409;
        } else {
          _if__result_2524 = 0;
        }
      } else {
        _if__result_2524 = 0;
      }
    } else {
      _if__result_2524 = 0;
    }
  } else {
    _if__result_2524 = 0;
  }
  if (_if__result_2524) {
    int32_t _M0L16end__str__offsetS411 =
      _M0L11str__offsetS406 + _M0L6lengthS404;
    int32_t _M0L1iS412 = _M0L11str__offsetS406;
    int32_t _M0L1jS413 = _M0L13bytes__offsetS403;
    while (1) {
      if (_M0L1iS412 < _M0L16end__str__offsetS411) {
        int32_t _M0L6_2atmpS1511 = _M0L3strS410[_M0L1iS412];
        int32_t _M0L6_2atmpS1510 = (int32_t)_M0L6_2atmpS1511;
        uint32_t _M0L1cS414 = *(uint32_t*)&_M0L6_2atmpS1510;
        uint32_t _M0L6_2atmpS1506 = _M0L1cS414 & 255u;
        int32_t _M0L6_2atmpS1505;
        int32_t _M0L6_2atmpS1507;
        uint32_t _M0L6_2atmpS1509;
        int32_t _M0L6_2atmpS1508;
        int32_t _M0L6_2atmpS1512;
        int32_t _M0L6_2atmpS1513;
        #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1505 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1506);
        if (
          _M0L1jS413 < 0 || _M0L1jS413 >= Moonbit_array_length(_M0L4selfS408)
        ) {
          #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS408[_M0L1jS413] = _M0L6_2atmpS1505;
        _M0L6_2atmpS1507 = _M0L1jS413 + 1;
        _M0L6_2atmpS1509 = _M0L1cS414 >> 8;
        #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1508 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1509);
        if (
          _M0L6_2atmpS1507 < 0
          || _M0L6_2atmpS1507 >= Moonbit_array_length(_M0L4selfS408)
        ) {
          #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS408[_M0L6_2atmpS1507] = _M0L6_2atmpS1508;
        _M0L6_2atmpS1512 = _M0L1iS412 + 1;
        _M0L6_2atmpS1513 = _M0L1jS413 + 2;
        _M0L1iS412 = _M0L6_2atmpS1512;
        _M0L1jS413 = _M0L6_2atmpS1513;
        continue;
      } else {
        moonbit_decref(_M0L3strS410);
        moonbit_decref(_M0L4selfS408);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS410);
    moonbit_decref(_M0L4selfS408);
    #line 137 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS401) {
  int32_t _M0L6_2atmpS1504;
  #line 2518 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1504 = *(int32_t*)&_M0L4selfS401;
  return _M0L6_2atmpS1504 & 0xff;
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS399,
  struct _M0TPB6Logger _M0L6loggerS400
) {
  #line 166 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 167 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L4selfS399, _M0L6loggerS400, 1);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS397,
  int32_t _M0L13start__offsetS398,
  int64_t _M0L11end__offsetS395
) {
  int32_t _M0L11end__offsetS394;
  int32_t _if__result_2526;
  #line 104 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS395 == 4294967296ll) {
    moonbit_incref(_M0L4selfS397.$0);
    #line 109 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    _M0L11end__offsetS394 = _M0MPC16string10StringView6length(_M0L4selfS397);
  } else {
    int64_t _M0L7_2aSomeS396 = _M0L11end__offsetS395;
    _M0L11end__offsetS394 = (int32_t)_M0L7_2aSomeS396;
  }
  if (_M0L13start__offsetS398 >= 0) {
    if (_M0L13start__offsetS398 <= _M0L11end__offsetS394) {
      int32_t _M0L6_2atmpS1498;
      moonbit_incref(_M0L4selfS397.$0);
      #line 112 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1498 = _M0MPC16string10StringView6length(_M0L4selfS397);
      _if__result_2526 = _M0L11end__offsetS394 <= _M0L6_2atmpS1498;
    } else {
      _if__result_2526 = 0;
    }
  } else {
    _if__result_2526 = 0;
  }
  if (_if__result_2526) {
    moonbit_string_t _M0L3strS1499 = _M0L4selfS397.$0;
    int32_t _M0L5startS1503 = _M0L4selfS397.$1;
    int32_t _M0L6_2atmpS1500 = _M0L5startS1503 + _M0L13start__offsetS398;
    int32_t _M0L5startS1502 = _M0L4selfS397.$1;
    int32_t _M0L6_2atmpS1501 = _M0L5startS1502 + _M0L11end__offsetS394;
    return (struct _M0TPC16string10StringView){_M0L6_2atmpS1500,
                                                 _M0L6_2atmpS1501,
                                                 _M0L3strS1499};
  } else {
    moonbit_decref(_M0L4selfS397.$0);
    #line 113 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_82.data);
  }
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS391) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS391;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS392
) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS392;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS393) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS393;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS375,
  int32_t _M0L5radixS374
) {
  int32_t _if__result_2527;
  int32_t _M0L12is__negativeS376;
  uint32_t _M0L3numS377;
  uint16_t* _M0L6bufferS378;
  #line 209 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS374 < 2) {
    _if__result_2527 = 1;
  } else {
    _if__result_2527 = _M0L5radixS374 > 36;
  }
  if (_if__result_2527) {
    #line 213 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_83.data);
  }
  if (_M0L4selfS375 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L12is__negativeS376 = _M0L4selfS375 < 0;
  if (_M0L12is__negativeS376) {
    int32_t _M0L6_2atmpS1497 = -_M0L4selfS375;
    _M0L3numS377 = *(uint32_t*)&_M0L6_2atmpS1497;
  } else {
    _M0L3numS377 = *(uint32_t*)&_M0L4selfS375;
  }
  switch (_M0L5radixS374) {
    case 10: {
      int32_t _M0L10digit__lenS379;
      int32_t _M0L6_2atmpS1494;
      int32_t _M0L10total__lenS380;
      uint16_t* _M0L6bufferS381;
      int32_t _M0L12digit__startS382;
      #line 235 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS379 = _M0FPB12dec__count32(_M0L3numS377);
      if (_M0L12is__negativeS376) {
        _M0L6_2atmpS1494 = 1;
      } else {
        _M0L6_2atmpS1494 = 0;
      }
      _M0L10total__lenS380 = _M0L10digit__lenS379 + _M0L6_2atmpS1494;
      _M0L6bufferS381
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS380, 0);
      if (_M0L12is__negativeS376) {
        _M0L12digit__startS382 = 1;
      } else {
        _M0L12digit__startS382 = 0;
      }
      moonbit_incref(_M0L6bufferS381);
      #line 239 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS381, _M0L3numS377, _M0L12digit__startS382, _M0L10total__lenS380);
      _M0L6bufferS378 = _M0L6bufferS381;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS383;
      int32_t _M0L6_2atmpS1495;
      int32_t _M0L10total__lenS384;
      uint16_t* _M0L6bufferS385;
      int32_t _M0L12digit__startS386;
      #line 243 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS383 = _M0FPB12hex__count32(_M0L3numS377);
      if (_M0L12is__negativeS376) {
        _M0L6_2atmpS1495 = 1;
      } else {
        _M0L6_2atmpS1495 = 0;
      }
      _M0L10total__lenS384 = _M0L10digit__lenS383 + _M0L6_2atmpS1495;
      _M0L6bufferS385
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS384, 0);
      if (_M0L12is__negativeS376) {
        _M0L12digit__startS386 = 1;
      } else {
        _M0L12digit__startS386 = 0;
      }
      moonbit_incref(_M0L6bufferS385);
      #line 247 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS385, _M0L3numS377, _M0L12digit__startS386, _M0L10total__lenS384);
      _M0L6bufferS378 = _M0L6bufferS385;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS387;
      int32_t _M0L6_2atmpS1496;
      int32_t _M0L10total__lenS388;
      uint16_t* _M0L6bufferS389;
      int32_t _M0L12digit__startS390;
      #line 251 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS387
      = _M0FPB14radix__count32(_M0L3numS377, _M0L5radixS374);
      if (_M0L12is__negativeS376) {
        _M0L6_2atmpS1496 = 1;
      } else {
        _M0L6_2atmpS1496 = 0;
      }
      _M0L10total__lenS388 = _M0L10digit__lenS387 + _M0L6_2atmpS1496;
      _M0L6bufferS389
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS388, 0);
      if (_M0L12is__negativeS376) {
        _M0L12digit__startS390 = 1;
      } else {
        _M0L12digit__startS390 = 0;
      }
      moonbit_incref(_M0L6bufferS389);
      #line 255 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS389, _M0L3numS377, _M0L12digit__startS390, _M0L10total__lenS388, _M0L5radixS374);
      _M0L6bufferS378 = _M0L6bufferS389;
      break;
    }
  }
  if (_M0L12is__negativeS376) {
    _M0L6bufferS378[0] = 45;
  }
  return _M0L6bufferS378;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS368,
  int32_t _M0L5radixS370
) {
  uint32_t _M0L4baseS369;
  uint32_t _M0L3numS371;
  int32_t _M0L5countS372;
  #line 189 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS368 == 0u) {
    return 1;
  }
  _M0L4baseS369 = *(uint32_t*)&_M0L5radixS370;
  _M0L3numS371 = _M0L5valueS368;
  _M0L5countS372 = 0;
  while (1) {
    if (_M0L3numS371 > 0u) {
      uint32_t _M0L6_2atmpS1492 = _M0L3numS371 / _M0L4baseS369;
      int32_t _M0L6_2atmpS1493 = _M0L5countS372 + 1;
      _M0L3numS371 = _M0L6_2atmpS1492;
      _M0L5countS372 = _M0L6_2atmpS1493;
      continue;
    } else {
      return _M0L5countS372;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS366) {
  #line 177 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS366 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS367;
    int32_t _M0L6_2atmpS1491;
    int32_t _M0L6_2atmpS1490;
    #line 182 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS367 = moonbit_clz32(_M0L5valueS366);
    _M0L6_2atmpS1491 = 31 - _M0L14leading__zerosS367;
    _M0L6_2atmpS1490 = _M0L6_2atmpS1491 / 4;
    return _M0L6_2atmpS1490 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS365) {
  #line 143 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS365 >= 100000u) {
    if (_M0L5valueS365 >= 10000000u) {
      if (_M0L5valueS365 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS365 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS365 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS365 >= 1000u) {
    if (_M0L5valueS365 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS365 >= 100u) {
    return 3;
  } else if (_M0L5valueS365 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS351,
  uint32_t _M0L3numS363,
  int32_t _M0L12digit__startS352,
  int32_t _M0L10total__lenS364
) {
  int32_t _M0L6_2atmpS1489;
  uint32_t _M0L3numS341;
  int32_t _M0L6offsetS342;
  #line 88 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1489 = _M0L10total__lenS364 - _M0L12digit__startS352;
  _M0L3numS341 = _M0L3numS363;
  _M0L6offsetS342 = _M0L6_2atmpS1489;
  while (1) {
    if (_M0L3numS341 >= 10000u) {
      uint32_t _M0L1tS343 = _M0L3numS341 / 10000u;
      uint32_t _M0L6_2atmpS1466 = _M0L3numS341 % 10000u;
      int32_t _M0L1rS344 = *(int32_t*)&_M0L6_2atmpS1466;
      int32_t _M0L2d1S345 = _M0L1rS344 / 100;
      int32_t _M0L2d2S346 = _M0L1rS344 % 100;
      int32_t _M0L6_2atmpS1465 = _M0L2d1S345 / 10;
      int32_t _M0L6_2atmpS1464 = 48 + _M0L6_2atmpS1465;
      int32_t _M0L6d1__hiS347 = (uint16_t)_M0L6_2atmpS1464;
      int32_t _M0L6_2atmpS1463 = _M0L2d1S345 % 10;
      int32_t _M0L6_2atmpS1462 = 48 + _M0L6_2atmpS1463;
      int32_t _M0L6d1__loS348 = (uint16_t)_M0L6_2atmpS1462;
      int32_t _M0L6_2atmpS1461 = _M0L2d2S346 / 10;
      int32_t _M0L6_2atmpS1460 = 48 + _M0L6_2atmpS1461;
      int32_t _M0L6d2__hiS349 = (uint16_t)_M0L6_2atmpS1460;
      int32_t _M0L6_2atmpS1459 = _M0L2d2S346 % 10;
      int32_t _M0L6_2atmpS1458 = 48 + _M0L6_2atmpS1459;
      int32_t _M0L6d2__loS350 = (uint16_t)_M0L6_2atmpS1458;
      int32_t _M0L6_2atmpS1450 = _M0L12digit__startS352 + _M0L6offsetS342;
      int32_t _M0L6_2atmpS1449 = _M0L6_2atmpS1450 - 4;
      int32_t _M0L6_2atmpS1452;
      int32_t _M0L6_2atmpS1451;
      int32_t _M0L6_2atmpS1454;
      int32_t _M0L6_2atmpS1453;
      int32_t _M0L6_2atmpS1456;
      int32_t _M0L6_2atmpS1455;
      int32_t _M0L6_2atmpS1457;
      _M0L6bufferS351[_M0L6_2atmpS1449] = _M0L6d1__hiS347;
      _M0L6_2atmpS1452 = _M0L12digit__startS352 + _M0L6offsetS342;
      _M0L6_2atmpS1451 = _M0L6_2atmpS1452 - 3;
      _M0L6bufferS351[_M0L6_2atmpS1451] = _M0L6d1__loS348;
      _M0L6_2atmpS1454 = _M0L12digit__startS352 + _M0L6offsetS342;
      _M0L6_2atmpS1453 = _M0L6_2atmpS1454 - 2;
      _M0L6bufferS351[_M0L6_2atmpS1453] = _M0L6d2__hiS349;
      _M0L6_2atmpS1456 = _M0L12digit__startS352 + _M0L6offsetS342;
      _M0L6_2atmpS1455 = _M0L6_2atmpS1456 - 1;
      _M0L6bufferS351[_M0L6_2atmpS1455] = _M0L6d2__loS350;
      _M0L6_2atmpS1457 = _M0L6offsetS342 - 4;
      _M0L3numS341 = _M0L1tS343;
      _M0L6offsetS342 = _M0L6_2atmpS1457;
      continue;
    } else {
      int32_t _M0L6_2atmpS1488 = *(int32_t*)&_M0L3numS341;
      int32_t _M0L9remainingS354 = _M0L6_2atmpS1488;
      int32_t _M0L6offsetS355 = _M0L6offsetS342;
      while (1) {
        if (_M0L9remainingS354 >= 100) {
          int32_t _M0L1tS356 = _M0L9remainingS354 / 100;
          int32_t _M0L1dS357 = _M0L9remainingS354 % 100;
          int32_t _M0L6_2atmpS1475 = _M0L1dS357 / 10;
          int32_t _M0L6_2atmpS1474 = 48 + _M0L6_2atmpS1475;
          int32_t _M0L5d__hiS358 = (uint16_t)_M0L6_2atmpS1474;
          int32_t _M0L6_2atmpS1473 = _M0L1dS357 % 10;
          int32_t _M0L6_2atmpS1472 = 48 + _M0L6_2atmpS1473;
          int32_t _M0L5d__loS359 = (uint16_t)_M0L6_2atmpS1472;
          int32_t _M0L6_2atmpS1468 = _M0L12digit__startS352 + _M0L6offsetS355;
          int32_t _M0L6_2atmpS1467 = _M0L6_2atmpS1468 - 2;
          int32_t _M0L6_2atmpS1470;
          int32_t _M0L6_2atmpS1469;
          int32_t _M0L6_2atmpS1471;
          _M0L6bufferS351[_M0L6_2atmpS1467] = _M0L5d__hiS358;
          _M0L6_2atmpS1470 = _M0L12digit__startS352 + _M0L6offsetS355;
          _M0L6_2atmpS1469 = _M0L6_2atmpS1470 - 1;
          _M0L6bufferS351[_M0L6_2atmpS1469] = _M0L5d__loS359;
          _M0L6_2atmpS1471 = _M0L6offsetS355 - 2;
          _M0L9remainingS354 = _M0L1tS356;
          _M0L6offsetS355 = _M0L6_2atmpS1471;
          continue;
        } else if (_M0L9remainingS354 >= 10) {
          int32_t _M0L6_2atmpS1483 = _M0L9remainingS354 / 10;
          int32_t _M0L6_2atmpS1482 = 48 + _M0L6_2atmpS1483;
          int32_t _M0L5d__hiS361 = (uint16_t)_M0L6_2atmpS1482;
          int32_t _M0L6_2atmpS1481 = _M0L9remainingS354 % 10;
          int32_t _M0L6_2atmpS1480 = 48 + _M0L6_2atmpS1481;
          int32_t _M0L5d__loS362 = (uint16_t)_M0L6_2atmpS1480;
          int32_t _M0L6_2atmpS1477 = _M0L12digit__startS352 + _M0L6offsetS355;
          int32_t _M0L6_2atmpS1476 = _M0L6_2atmpS1477 - 2;
          int32_t _M0L6_2atmpS1479;
          int32_t _M0L6_2atmpS1478;
          _M0L6bufferS351[_M0L6_2atmpS1476] = _M0L5d__hiS361;
          _M0L6_2atmpS1479 = _M0L12digit__startS352 + _M0L6offsetS355;
          _M0L6_2atmpS1478 = _M0L6_2atmpS1479 - 1;
          _M0L6bufferS351[_M0L6_2atmpS1478] = _M0L5d__loS362;
          moonbit_decref(_M0L6bufferS351);
        } else {
          int32_t _M0L6_2atmpS1487 = _M0L12digit__startS352 + _M0L6offsetS355;
          int32_t _M0L6_2atmpS1484 = _M0L6_2atmpS1487 - 1;
          int32_t _M0L6_2atmpS1486 = 48 + _M0L9remainingS354;
          int32_t _M0L6_2atmpS1485 = (uint16_t)_M0L6_2atmpS1486;
          _M0L6bufferS351[_M0L6_2atmpS1484] = _M0L6_2atmpS1485;
          moonbit_decref(_M0L6bufferS351);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS331,
  uint32_t _M0L3numS335,
  int32_t _M0L12digit__startS332,
  int32_t _M0L10total__lenS334,
  int32_t _M0L5radixS325
) {
  uint32_t _M0L4baseS324;
  int32_t _M0L6_2atmpS1434;
  int32_t _M0L6_2atmpS1433;
  #line 57 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS324 = *(uint32_t*)&_M0L5radixS325;
  _M0L6_2atmpS1434 = _M0L5radixS325 - 1;
  _M0L6_2atmpS1433 = _M0L5radixS325 & _M0L6_2atmpS1434;
  if (_M0L6_2atmpS1433 == 0) {
    int32_t _M0L5shiftS326;
    uint32_t _M0L4maskS327;
    int32_t _M0L6_2atmpS1441;
    int32_t _M0L6offsetS328;
    uint32_t _M0L1nS329;
    #line 68 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS326 = moonbit_ctz32(_M0L5radixS325);
    _M0L4maskS327 = _M0L4baseS324 - 1u;
    _M0L6_2atmpS1441 = _M0L10total__lenS334 - _M0L12digit__startS332;
    _M0L6offsetS328 = _M0L6_2atmpS1441;
    _M0L1nS329 = _M0L3numS335;
    while (1) {
      if (_M0L1nS329 > 0u) {
        uint32_t _M0L6_2atmpS1440 = _M0L1nS329 & _M0L4maskS327;
        int32_t _M0L5digitS330 = *(int32_t*)&_M0L6_2atmpS1440;
        int32_t _M0L6_2atmpS1437 = _M0L12digit__startS332 + _M0L6offsetS328;
        int32_t _M0L6_2atmpS1435 = _M0L6_2atmpS1437 - 1;
        int32_t _M0L6_2atmpS1436 =
          ((moonbit_string_t)moonbit_string_literal_85.data)[_M0L5digitS330];
        int32_t _M0L6_2atmpS1438;
        uint32_t _M0L6_2atmpS1439;
        _M0L6bufferS331[_M0L6_2atmpS1435] = _M0L6_2atmpS1436;
        _M0L6_2atmpS1438 = _M0L6offsetS328 - 1;
        _M0L6_2atmpS1439 = _M0L1nS329 >> (_M0L5shiftS326 & 31);
        _M0L6offsetS328 = _M0L6_2atmpS1438;
        _M0L1nS329 = _M0L6_2atmpS1439;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS331);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1448 = _M0L10total__lenS334 - _M0L12digit__startS332;
    int32_t _M0L6offsetS336 = _M0L6_2atmpS1448;
    uint32_t _M0L1nS337 = _M0L3numS335;
    while (1) {
      if (_M0L1nS337 > 0u) {
        uint32_t _M0L1qS338 = _M0L1nS337 / _M0L4baseS324;
        uint32_t _M0L6_2atmpS1447 = _M0L1qS338 * _M0L4baseS324;
        uint32_t _M0L6_2atmpS1446 = _M0L1nS337 - _M0L6_2atmpS1447;
        int32_t _M0L5digitS339 = *(int32_t*)&_M0L6_2atmpS1446;
        int32_t _M0L6_2atmpS1444 = _M0L12digit__startS332 + _M0L6offsetS336;
        int32_t _M0L6_2atmpS1442 = _M0L6_2atmpS1444 - 1;
        int32_t _M0L6_2atmpS1443 =
          ((moonbit_string_t)moonbit_string_literal_85.data)[_M0L5digitS339];
        int32_t _M0L6_2atmpS1445;
        _M0L6bufferS331[_M0L6_2atmpS1442] = _M0L6_2atmpS1443;
        _M0L6_2atmpS1445 = _M0L6offsetS336 - 1;
        _M0L6offsetS336 = _M0L6_2atmpS1445;
        _M0L1nS337 = _M0L1qS338;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS331);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS318,
  uint32_t _M0L3numS323,
  int32_t _M0L12digit__startS319,
  int32_t _M0L10total__lenS322
) {
  int32_t _M0L6_2atmpS1432;
  int32_t _M0L6offsetS313;
  uint32_t _M0L1nS314;
  #line 29 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1432 = _M0L10total__lenS322 - _M0L12digit__startS319;
  _M0L6offsetS313 = _M0L6_2atmpS1432;
  _M0L1nS314 = _M0L3numS323;
  while (1) {
    if (_M0L6offsetS313 >= 2) {
      uint32_t _M0L6_2atmpS1429 = _M0L1nS314 & 255u;
      int32_t _M0L9byte__valS315 = *(int32_t*)&_M0L6_2atmpS1429;
      int32_t _M0L2hiS316 = _M0L9byte__valS315 / 16;
      int32_t _M0L2loS317 = _M0L9byte__valS315 % 16;
      int32_t _M0L6_2atmpS1423 = _M0L12digit__startS319 + _M0L6offsetS313;
      int32_t _M0L6_2atmpS1421 = _M0L6_2atmpS1423 - 2;
      int32_t _M0L6_2atmpS1422 =
        ((moonbit_string_t)moonbit_string_literal_85.data)[_M0L2hiS316];
      int32_t _M0L6_2atmpS1426;
      int32_t _M0L6_2atmpS1424;
      int32_t _M0L6_2atmpS1425;
      int32_t _M0L6_2atmpS1427;
      uint32_t _M0L6_2atmpS1428;
      _M0L6bufferS318[_M0L6_2atmpS1421] = _M0L6_2atmpS1422;
      _M0L6_2atmpS1426 = _M0L12digit__startS319 + _M0L6offsetS313;
      _M0L6_2atmpS1424 = _M0L6_2atmpS1426 - 1;
      _M0L6_2atmpS1425
      = ((moonbit_string_t)moonbit_string_literal_85.data)[
        _M0L2loS317
      ];
      _M0L6bufferS318[_M0L6_2atmpS1424] = _M0L6_2atmpS1425;
      _M0L6_2atmpS1427 = _M0L6offsetS313 - 2;
      _M0L6_2atmpS1428 = _M0L1nS314 >> 8;
      _M0L6offsetS313 = _M0L6_2atmpS1427;
      _M0L1nS314 = _M0L6_2atmpS1428;
      continue;
    } else if (_M0L6offsetS313 == 1) {
      uint32_t _M0L6_2atmpS1431 = _M0L1nS314 & 15u;
      int32_t _M0L6nibbleS321 = *(int32_t*)&_M0L6_2atmpS1431;
      int32_t _M0L6_2atmpS1430 =
        ((moonbit_string_t)moonbit_string_literal_85.data)[_M0L6nibbleS321];
      _M0L6bufferS318[_M0L12digit__startS319] = _M0L6_2atmpS1430;
      moonbit_decref(_M0L6bufferS318);
    } else {
      moonbit_decref(_M0L6bufferS318);
    }
    break;
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger _M0L4selfS296,
  struct _M0TWEOs* _M0L4iterS300,
  moonbit_string_t _M0L6prefixS297,
  moonbit_string_t _M0L6suffixS312,
  moonbit_string_t _M0L3sepS303,
  int32_t _M0L8trailingS298
) {
  #line 161 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  if (_M0L4selfS296.$1) {
    moonbit_incref(_M0L4selfS296.$1);
  }
  #line 169 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS296.$0->$method_0(_M0L4selfS296.$1, _M0L6prefixS297);
  if (_M0L8trailingS298) {
    while (1) {
      moonbit_string_t _M0L7_2abindS299;
      moonbit_incref(_M0L4iterS300);
      #line 171 "/Users/user/.moon/lib/core/builtin/traits.mbt"
      _M0L7_2abindS299 = _M0MPB4Iter4nextGsE(_M0L4iterS300);
      if (_M0L7_2abindS299 == 0) {
        moonbit_decref(_M0L3sepS303);
        moonbit_decref(_M0L4iterS300);
        if (_M0L7_2abindS299) {
          moonbit_decref(_M0L7_2abindS299);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS301 = _M0L7_2abindS299;
        moonbit_string_t _M0L4_2axS302 = _M0L7_2aSomeS301;
        if (_M0L4selfS296.$1) {
          moonbit_incref(_M0L4selfS296.$1);
        }
        #line 172 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0MPB6Logger13write__objectGsE(_M0L4selfS296, _M0L4_2axS302);
        moonbit_incref(_M0L3sepS303);
        if (_M0L4selfS296.$1) {
          moonbit_incref(_M0L4selfS296.$1);
        }
        #line 173 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0L4selfS296.$0->$method_0(_M0L4selfS296.$1, _M0L3sepS303);
        continue;
      }
      break;
    }
  } else {
    moonbit_string_t _M0L7_2abindS305;
    moonbit_incref(_M0L4iterS300);
    #line 175 "/Users/user/.moon/lib/core/builtin/traits.mbt"
    _M0L7_2abindS305 = _M0MPB4Iter4nextGsE(_M0L4iterS300);
    if (_M0L7_2abindS305 == 0) {
      if (_M0L7_2abindS305) {
        moonbit_decref(_M0L7_2abindS305);
      }
      moonbit_decref(_M0L3sepS303);
      moonbit_decref(_M0L4iterS300);
    } else {
      moonbit_string_t _M0L7_2aSomeS306 = _M0L7_2abindS305;
      moonbit_string_t _M0L4_2axS307 = _M0L7_2aSomeS306;
      if (_M0L4selfS296.$1) {
        moonbit_incref(_M0L4selfS296.$1);
      }
      #line 176 "/Users/user/.moon/lib/core/builtin/traits.mbt"
      _M0MPB6Logger13write__objectGsE(_M0L4selfS296, _M0L4_2axS307);
      while (1) {
        moonbit_string_t _M0L7_2abindS308;
        moonbit_incref(_M0L4iterS300);
        #line 177 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0L7_2abindS308 = _M0MPB4Iter4nextGsE(_M0L4iterS300);
        if (_M0L7_2abindS308 == 0) {
          if (_M0L7_2abindS308) {
            moonbit_decref(_M0L7_2abindS308);
          }
          moonbit_decref(_M0L3sepS303);
          moonbit_decref(_M0L4iterS300);
        } else {
          moonbit_string_t _M0L7_2aSomeS309 = _M0L7_2abindS308;
          moonbit_string_t _M0L4_2axS310 = _M0L7_2aSomeS309;
          moonbit_incref(_M0L3sepS303);
          if (_M0L4selfS296.$1) {
            moonbit_incref(_M0L4selfS296.$1);
          }
          #line 178 "/Users/user/.moon/lib/core/builtin/traits.mbt"
          _M0L4selfS296.$0->$method_0(_M0L4selfS296.$1, _M0L3sepS303);
          if (_M0L4selfS296.$1) {
            moonbit_incref(_M0L4selfS296.$1);
          }
          #line 179 "/Users/user/.moon/lib/core/builtin/traits.mbt"
          _M0MPB6Logger13write__objectGsE(_M0L4selfS296, _M0L4_2axS310);
          continue;
        }
        break;
      }
    }
  }
  #line 182 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS296.$0->$method_0(_M0L4selfS296.$1, _M0L6suffixS312);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS291) {
  struct _M0TWEOs* _M0L7_2afuncS290;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS290 = _M0L4selfS291;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS290->code(_M0L7_2afuncS290);
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS293
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS292;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS292 = _M0L4selfS293;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS292->code(_M0L7_2afuncS292);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS295) {
  struct _M0TWEOc* _M0L7_2afuncS294;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS294 = _M0L4selfS295;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS294->code(_M0L7_2afuncS294);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE* _M0L4selfS287
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS286;
  struct _M0TPB6Logger _M0L6_2atmpS1419;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS286 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS286);
  _M0L6_2atmpS1419
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS286
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L4selfS287, _M0L6_2atmpS1419);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS286);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS289
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS288;
  struct _M0TPB6Logger _M0L6_2atmpS1420;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS288 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS288);
  _M0L6_2atmpS1420
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS288
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS289, _M0L6_2atmpS1420);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS288);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS285
) {
  int32_t _result_2536;
  #line 98 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _result_2536 = _M0L4selfS285.$1;
  moonbit_decref(_M0L4selfS285.$0);
  return _result_2536;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS284
) {
  #line 91 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS284.$0;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS280,
  moonbit_string_t _M0L5valueS281,
  int32_t _M0L5startS282,
  int32_t _M0L3lenS283
) {
  int32_t _M0L6_2atmpS1418;
  int64_t _M0L6_2atmpS1417;
  struct _M0TPC16string10StringView _M0L6_2atmpS1416;
  #line 102 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1418 = _M0L5startS282 + _M0L3lenS283;
  _M0L6_2atmpS1417 = (int64_t)_M0L6_2atmpS1418;
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1416
  = _M0MPC16string6String11sub_2einner(_M0L5valueS281, _M0L5startS282, _M0L6_2atmpS1417);
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS280, _M0L6_2atmpS1416);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS273,
  int32_t _M0L5startS279,
  int64_t _M0L3endS275
) {
  int32_t _M0L3lenS272;
  int32_t _M0L3endS274;
  int32_t _M0L5startS278;
  int32_t _if__result_2537;
  #line 653 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS272 = Moonbit_array_length(_M0L4selfS273);
  if (_M0L3endS275 == 4294967296ll) {
    _M0L3endS274 = _M0L3lenS272;
  } else {
    int64_t _M0L7_2aSomeS276 = _M0L3endS275;
    int32_t _M0L6_2aendS277 = (int32_t)_M0L7_2aSomeS276;
    if (_M0L6_2aendS277 < 0) {
      _M0L3endS274 = _M0L3lenS272 + _M0L6_2aendS277;
    } else {
      _M0L3endS274 = _M0L6_2aendS277;
    }
  }
  if (_M0L5startS279 < 0) {
    _M0L5startS278 = _M0L3lenS272 + _M0L5startS279;
  } else {
    _M0L5startS278 = _M0L5startS279;
  }
  if (_M0L5startS278 >= 0) {
    if (_M0L5startS278 <= _M0L3endS274) {
      _if__result_2537 = _M0L3endS274 <= _M0L3lenS272;
    } else {
      _if__result_2537 = 0;
    }
  } else {
    _if__result_2537 = 0;
  }
  if (_if__result_2537) {
    if (_M0L5startS278 < _M0L3lenS272) {
      int32_t _M0L6_2atmpS1413 = _M0L4selfS273[_M0L5startS278];
      int32_t _M0L6_2atmpS1412;
      #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1412
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1413);
      if (!_M0L6_2atmpS1412) {
        
      } else {
        #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS274 < _M0L3lenS272) {
      int32_t _M0L6_2atmpS1415 = _M0L4selfS273[_M0L3endS274];
      int32_t _M0L6_2atmpS1414;
      #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1414
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1415);
      if (!_M0L6_2atmpS1414) {
        
      } else {
        #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS278,
                                                 _M0L3endS274,
                                                 _M0L4selfS273};
  } else {
    moonbit_decref(_M0L4selfS273);
    #line 661 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS269) {
  struct _M0TPB6Hasher* _M0L1hS268;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS268 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS268);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS268, _M0L4selfS269);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS268);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS271
) {
  struct _M0TPB6Hasher* _M0L1hS270;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS270 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS270);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS270, _M0L4selfS271);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS270);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS266) {
  int32_t _M0L4seedS265;
  if (_M0L10seed_2eoptS266 == 4294967296ll) {
    _M0L4seedS265 = 0;
  } else {
    int64_t _M0L7_2aSomeS267 = _M0L10seed_2eoptS266;
    _M0L4seedS265 = (int32_t)_M0L7_2aSomeS267;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS265);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS264) {
  uint32_t _M0L6_2atmpS1411;
  uint32_t _M0L6_2atmpS1410;
  struct _M0TPB6Hasher* _block_2538;
  #line 75 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1411 = *(uint32_t*)&_M0L4seedS264;
  _M0L6_2atmpS1410 = _M0L6_2atmpS1411 + 374761393u;
  _block_2538
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2538)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2538->$0 = _M0L6_2atmpS1410;
  return _block_2538;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS263) {
  uint32_t _M0L6_2atmpS1409;
  #line 435 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1409 = _M0MPB6Hasher9avalanche(_M0L4selfS263);
  return *(int32_t*)&_M0L6_2atmpS1409;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS262) {
  uint32_t _M0Lm3accS261;
  uint32_t _M0L6_2atmpS1398;
  uint32_t _M0L6_2atmpS1400;
  uint32_t _M0L6_2atmpS1399;
  uint32_t _M0L6_2atmpS1401;
  uint32_t _M0L6_2atmpS1402;
  uint32_t _M0L6_2atmpS1404;
  uint32_t _M0L6_2atmpS1403;
  uint32_t _M0L6_2atmpS1405;
  uint32_t _M0L6_2atmpS1406;
  uint32_t _M0L6_2atmpS1408;
  uint32_t _M0L6_2atmpS1407;
  #line 440 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS261 = _M0L4selfS262->$0;
  moonbit_decref(_M0L4selfS262);
  _M0L6_2atmpS1398 = _M0Lm3accS261;
  _M0L6_2atmpS1400 = _M0Lm3accS261;
  _M0L6_2atmpS1399 = _M0L6_2atmpS1400 >> 15;
  _M0Lm3accS261 = _M0L6_2atmpS1398 ^ _M0L6_2atmpS1399;
  _M0L6_2atmpS1401 = _M0Lm3accS261;
  _M0Lm3accS261 = _M0L6_2atmpS1401 * 2246822519u;
  _M0L6_2atmpS1402 = _M0Lm3accS261;
  _M0L6_2atmpS1404 = _M0Lm3accS261;
  _M0L6_2atmpS1403 = _M0L6_2atmpS1404 >> 13;
  _M0Lm3accS261 = _M0L6_2atmpS1402 ^ _M0L6_2atmpS1403;
  _M0L6_2atmpS1405 = _M0Lm3accS261;
  _M0Lm3accS261 = _M0L6_2atmpS1405 * 3266489917u;
  _M0L6_2atmpS1406 = _M0Lm3accS261;
  _M0L6_2atmpS1408 = _M0Lm3accS261;
  _M0L6_2atmpS1407 = _M0L6_2atmpS1408 >> 16;
  _M0Lm3accS261 = _M0L6_2atmpS1406 ^ _M0L6_2atmpS1407;
  return _M0Lm3accS261;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS259,
  moonbit_string_t _M0L1yS260
) {
  int32_t _M0L6_2atmpS1397;
  #line 23 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 24 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1397 = moonbit_val_array_equal(_M0L1xS259, _M0L1yS260);
  moonbit_decref(_M0L1yS260);
  moonbit_decref(_M0L1xS259);
  return !_M0L6_2atmpS1397;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS256,
  int32_t _M0L5valueS255
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS255, _M0L4selfS256);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS258,
  moonbit_string_t _M0L5valueS257
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS257, _M0L4selfS258);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS253,
  int32_t _M0L5valueS254
) {
  uint32_t _M0L6_2atmpS1396;
  #line 187 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1396 = *(uint32_t*)&_M0L5valueS254;
  #line 188 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS253, _M0L6_2atmpS1396);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS243,
  moonbit_string_t _M0L7contentS244,
  moonbit_string_t _M0L3locS246,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS248
) {
  moonbit_string_t _M0L6actualS242;
  #line 184 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 191 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6actualS242 = _M0L3objS243.$0->$method_1(_M0L3objS243.$1);
  moonbit_incref(_M0L7contentS244);
  moonbit_incref(_M0L6actualS242);
  #line 192 "/Users/user/.moon/lib/core/builtin/console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS242, _M0L7contentS244)
  ) {
    moonbit_string_t _M0L3locS245;
    moonbit_string_t _M0L9args__locS247;
    moonbit_string_t _M0L15expect__escapedS249;
    moonbit_string_t _M0L15actual__escapedS250;
    moonbit_string_t _M0L6_2atmpS1394;
    moonbit_string_t _M0L6_2atmpS1393;
    moonbit_string_t _M0L6_2atmpS1392;
    moonbit_string_t _M0L14expect__base64S251;
    moonbit_string_t _M0L6_2atmpS1391;
    moonbit_string_t _M0L6_2atmpS1390;
    moonbit_string_t _M0L6_2atmpS1389;
    moonbit_string_t _M0L14actual__base64S252;
    moonbit_string_t _M0L6_2atmpS1388;
    moonbit_string_t _M0L6_2atmpS1387;
    moonbit_string_t _M0L6_2atmpS1385;
    moonbit_string_t _M0L6_2atmpS1386;
    moonbit_string_t _M0L6_2atmpS1384;
    moonbit_string_t _M0L6_2atmpS1382;
    moonbit_string_t _M0L6_2atmpS1383;
    moonbit_string_t _M0L6_2atmpS1381;
    moonbit_string_t _M0L6_2atmpS1379;
    moonbit_string_t _M0L6_2atmpS1380;
    moonbit_string_t _M0L6_2atmpS1378;
    moonbit_string_t _M0L6_2atmpS1376;
    moonbit_string_t _M0L6_2atmpS1377;
    moonbit_string_t _M0L6_2atmpS1375;
    moonbit_string_t _M0L6_2atmpS1373;
    moonbit_string_t _M0L6_2atmpS1374;
    moonbit_string_t _M0L6_2atmpS1372;
    moonbit_string_t _M0L6_2atmpS1371;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1370;
    struct moonbit_result_0 _result_2539;
    #line 193 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L3locS245 = _M0MPB9SourceLoc16to__json__string(_M0L3locS246);
    #line 194 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L9args__locS247 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS248);
    moonbit_incref(_M0L7contentS244);
    #line 195 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L15expect__escapedS249
    = _M0MPC16string6String14escape_2einner(_M0L7contentS244, 1);
    moonbit_incref(_M0L6actualS242);
    #line 196 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L15actual__escapedS250
    = _M0MPC16string6String14escape_2einner(_M0L6actualS242, 1);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1394
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS244);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1393
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1394);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1392
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_86.data, _M0L6_2atmpS1393);
    moonbit_decref(_M0L6_2atmpS1393);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14expect__base64S251
    = moonbit_add_string(_M0L6_2atmpS1392, (moonbit_string_t)moonbit_string_literal_86.data);
    moonbit_decref(_M0L6_2atmpS1392);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1391
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS242);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1390
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1391);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1389
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_86.data, _M0L6_2atmpS1390);
    moonbit_decref(_M0L6_2atmpS1390);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14actual__base64S252
    = moonbit_add_string(_M0L6_2atmpS1389, (moonbit_string_t)moonbit_string_literal_86.data);
    moonbit_decref(_M0L6_2atmpS1389);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1388 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS245);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1387
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_87.data, _M0L6_2atmpS1388);
    moonbit_decref(_M0L6_2atmpS1388);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1385
    = moonbit_add_string(_M0L6_2atmpS1387, (moonbit_string_t)moonbit_string_literal_88.data);
    moonbit_decref(_M0L6_2atmpS1387);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1386
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS247);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1384 = moonbit_add_string(_M0L6_2atmpS1385, _M0L6_2atmpS1386);
    moonbit_decref(_M0L6_2atmpS1386);
    moonbit_decref(_M0L6_2atmpS1385);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1382
    = moonbit_add_string(_M0L6_2atmpS1384, (moonbit_string_t)moonbit_string_literal_89.data);
    moonbit_decref(_M0L6_2atmpS1384);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1383
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS249);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1381 = moonbit_add_string(_M0L6_2atmpS1382, _M0L6_2atmpS1383);
    moonbit_decref(_M0L6_2atmpS1383);
    moonbit_decref(_M0L6_2atmpS1382);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1379
    = moonbit_add_string(_M0L6_2atmpS1381, (moonbit_string_t)moonbit_string_literal_90.data);
    moonbit_decref(_M0L6_2atmpS1381);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1380
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS250);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1378 = moonbit_add_string(_M0L6_2atmpS1379, _M0L6_2atmpS1380);
    moonbit_decref(_M0L6_2atmpS1380);
    moonbit_decref(_M0L6_2atmpS1379);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1376
    = moonbit_add_string(_M0L6_2atmpS1378, (moonbit_string_t)moonbit_string_literal_91.data);
    moonbit_decref(_M0L6_2atmpS1378);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1377
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S251);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1375 = moonbit_add_string(_M0L6_2atmpS1376, _M0L6_2atmpS1377);
    moonbit_decref(_M0L6_2atmpS1377);
    moonbit_decref(_M0L6_2atmpS1376);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1373
    = moonbit_add_string(_M0L6_2atmpS1375, (moonbit_string_t)moonbit_string_literal_92.data);
    moonbit_decref(_M0L6_2atmpS1375);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1374
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S252);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1372 = moonbit_add_string(_M0L6_2atmpS1373, _M0L6_2atmpS1374);
    moonbit_decref(_M0L6_2atmpS1374);
    moonbit_decref(_M0L6_2atmpS1373);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1371
    = moonbit_add_string(_M0L6_2atmpS1372, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1372);
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1370
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1370)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1370)->$0
    = _M0L6_2atmpS1371;
    _result_2539.tag = 0;
    _result_2539.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1370;
    return _result_2539;
  } else {
    int32_t _M0L6_2atmpS1395;
    struct moonbit_result_0 _result_2540;
    moonbit_decref(_M0L9args__locS248);
    moonbit_decref(_M0L3locS246);
    moonbit_decref(_M0L7contentS244);
    moonbit_decref(_M0L6actualS242);
    _M0L6_2atmpS1395 = 0;
    _result_2540.tag = 1;
    _result_2540.data.ok = _M0L6_2atmpS1395;
    return _result_2540;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS235
) {
  struct _M0TPB13StringBuilder* _M0L3bufS233;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS234;
  int32_t _M0L7_2abindS236;
  int32_t _M0L1iS237;
  #line 124 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 125 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L3bufS233 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS234 = _M0L4selfS235;
  moonbit_incref(_M0L3bufS233);
  #line 127 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS233, 91);
  _M0L7_2abindS236 = _M0L7_2aselfS234->$1;
  _M0L1iS237 = 0;
  while (1) {
    if (_M0L1iS237 < _M0L7_2abindS236) {
      moonbit_string_t* _M0L3bufS1369 = _M0L7_2aselfS234->$0;
      moonbit_string_t _M0L4itemS238 =
        (moonbit_string_t)_M0L3bufS1369[_M0L1iS237];
      int32_t _M0L6_2atmpS1368;
      if (_M0L1iS237 != 0) {
        if (_M0L4itemS238) {
          moonbit_incref(_M0L4itemS238);
        }
        moonbit_incref(_M0L3bufS233);
        #line 130 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, (moonbit_string_t)moonbit_string_literal_59.data);
      } else if (_M0L4itemS238) {
        moonbit_incref(_M0L4itemS238);
      }
      if (_M0L4itemS238 == 0) {
        if (_M0L4itemS238) {
          moonbit_decref(_M0L4itemS238);
        }
        moonbit_incref(_M0L3bufS233);
        #line 133 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, (moonbit_string_t)moonbit_string_literal_93.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS239 = _M0L4itemS238;
        moonbit_string_t _M0L6_2alocS240 = _M0L7_2aSomeS239;
        moonbit_string_t _M0L6_2atmpS1367;
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1367
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS240);
        moonbit_incref(_M0L3bufS233);
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, _M0L6_2atmpS1367);
      }
      _M0L6_2atmpS1368 = _M0L1iS237 + 1;
      _M0L1iS237 = _M0L6_2atmpS1368;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS234);
    }
    break;
  }
  moonbit_incref(_M0L3bufS233);
  #line 137 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS233, 93);
  #line 138 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS233);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS232
) {
  moonbit_string_t _M0L6_2atmpS1366;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1365;
  #line 95 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1366 = _M0L4selfS232;
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1365 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1366);
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1365);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS231
) {
  struct _M0TPB13StringBuilder* _M0L2sbS230;
  struct _M0TPC16string10StringView _M0L8filenameS1351;
  struct _M0TPC16string10StringView _M0L11start__lineS1354;
  moonbit_string_t _M0L6_2atmpS1353;
  moonbit_string_t _M0L6_2atmpS1352;
  struct _M0TPC16string10StringView _M0L13start__columnS1357;
  moonbit_string_t _M0L6_2atmpS1356;
  moonbit_string_t _M0L6_2atmpS1355;
  struct _M0TPC16string10StringView _M0L9end__lineS1360;
  moonbit_string_t _M0L6_2atmpS1359;
  moonbit_string_t _M0L6_2atmpS1358;
  struct _M0TPC16string10StringView _M0L8_2afieldS2300;
  int32_t _M0L6_2acntS2401;
  struct _M0TPC16string10StringView _M0L11end__columnS1364;
  moonbit_string_t _M0L6_2atmpS1363;
  moonbit_string_t _M0L6_2atmpS1362;
  moonbit_string_t _M0L6_2atmpS1361;
  #line 82 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 83 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS230 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L2sbS230);
  #line 84 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, (moonbit_string_t)moonbit_string_literal_94.data);
  _M0L8filenameS1351
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$0_1, _M0L4selfS231->$0_2, _M0L4selfS231->$0_0
  };
  moonbit_incref(_M0L8filenameS1351.$0);
  moonbit_incref(_M0L2sbS230);
  #line 85 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS230, _M0L8filenameS1351);
  _M0L11start__lineS1354
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$1_1, _M0L4selfS231->$1_2, _M0L4selfS231->$1_0
  };
  moonbit_incref(_M0L11start__lineS1354.$0);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1353
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1354);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1352
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_95.data, _M0L6_2atmpS1353);
  moonbit_decref(_M0L6_2atmpS1353);
  moonbit_incref(_M0L2sbS230);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1352);
  _M0L13start__columnS1357
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$2_1, _M0L4selfS231->$2_2, _M0L4selfS231->$2_0
  };
  moonbit_incref(_M0L13start__columnS1357.$0);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1356
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1357);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1355
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_96.data, _M0L6_2atmpS1356);
  moonbit_decref(_M0L6_2atmpS1356);
  moonbit_incref(_M0L2sbS230);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1355);
  _M0L9end__lineS1360
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$3_1, _M0L4selfS231->$3_2, _M0L4selfS231->$3_0
  };
  moonbit_incref(_M0L9end__lineS1360.$0);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1359
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1360);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1358
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_97.data, _M0L6_2atmpS1359);
  moonbit_decref(_M0L6_2atmpS1359);
  moonbit_incref(_M0L2sbS230);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1358);
  _M0L8_2afieldS2300
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$4_1, _M0L4selfS231->$4_2, _M0L4selfS231->$4_0
  };
  _M0L6_2acntS2401 = Moonbit_object_header(_M0L4selfS231)->rc;
  if (_M0L6_2acntS2401 > 1) {
    int32_t _M0L11_2anew__cntS2406 = _M0L6_2acntS2401 - 1;
    Moonbit_object_header(_M0L4selfS231)->rc = _M0L11_2anew__cntS2406;
    moonbit_incref(_M0L8_2afieldS2300.$0);
  } else if (_M0L6_2acntS2401 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2405 =
      (struct _M0TPC16string10StringView){_M0L4selfS231->$3_1,
                                            _M0L4selfS231->$3_2,
                                            _M0L4selfS231->$3_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2404;
    struct _M0TPC16string10StringView _M0L8_2afieldS2403;
    struct _M0TPC16string10StringView _M0L8_2afieldS2402;
    moonbit_decref(_M0L8_2afieldS2405.$0);
    _M0L8_2afieldS2404
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$2_1, _M0L4selfS231->$2_2, _M0L4selfS231->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2404.$0);
    _M0L8_2afieldS2403
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$1_1, _M0L4selfS231->$1_2, _M0L4selfS231->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2403.$0);
    _M0L8_2afieldS2402
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$0_1, _M0L4selfS231->$0_2, _M0L4selfS231->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2402.$0);
    #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS231);
  }
  _M0L11end__columnS1364 = _M0L8_2afieldS2300;
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1363
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1364);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1362
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_98.data, _M0L6_2atmpS1363);
  moonbit_decref(_M0L6_2atmpS1363);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1361
  = moonbit_add_string(_M0L6_2atmpS1362, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1362);
  moonbit_incref(_M0L2sbS230);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1361);
  #line 90 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS230);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS229,
  moonbit_string_t _M0L3strS228
) {
  int32_t _M0L8str__lenS227;
  int32_t _M0L3lenS1346;
  int32_t _M0L6_2atmpS1345;
  uint16_t* _M0L4dataS1347;
  int32_t _M0L3lenS1348;
  int32_t _M0L3lenS1350;
  int32_t _M0L6_2atmpS1349;
  #line 81 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS227 = Moonbit_array_length(_M0L3strS228);
  _M0L3lenS1346 = _M0L4selfS229->$1;
  _M0L6_2atmpS1345 = _M0L3lenS1346 + _M0L8str__lenS227;
  moonbit_incref(_M0L4selfS229);
  #line 83 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS229, _M0L6_2atmpS1345);
  _M0L4dataS1347 = _M0L4selfS229->$0;
  _M0L3lenS1348 = _M0L4selfS229->$1;
  moonbit_incref(_M0L4dataS1347);
  #line 84 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1347, _M0L3lenS1348, _M0L3strS228, 0, _M0L8str__lenS227);
  _M0L3lenS1350 = _M0L4selfS229->$1;
  _M0L6_2atmpS1349 = _M0L3lenS1350 + _M0L8str__lenS227;
  _M0L4selfS229->$1 = _M0L6_2atmpS1349;
  moonbit_decref(_M0L4selfS229);
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS223,
  int32_t _M0L11dst__offsetS226,
  moonbit_string_t _M0L3strS224,
  int32_t _M0L11str__offsetS219,
  int32_t _M0L3lenS220
) {
  int32_t _M0L16end__str__offsetS218;
  int32_t _M0L1iS221;
  int32_t _M0L1jS222;
  #line 66 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS218 = _M0L11str__offsetS219 + _M0L3lenS220;
  _M0L1iS221 = _M0L11str__offsetS219;
  _M0L1jS222 = _M0L11dst__offsetS226;
  while (1) {
    if (_M0L1iS221 < _M0L16end__str__offsetS218) {
      int32_t _M0L6_2atmpS1342 = _M0L3strS224[_M0L1iS221];
      int32_t _M0L6_2atmpS1343;
      int32_t _M0L6_2atmpS1344;
      if (
        _M0L1jS222 < 0 || _M0L1jS222 >= Moonbit_array_length(_M0L4selfS223)
      ) {
        #line 75 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS223[_M0L1jS222] = _M0L6_2atmpS1342;
      _M0L6_2atmpS1343 = _M0L1iS221 + 1;
      _M0L6_2atmpS1344 = _M0L1jS222 + 1;
      _M0L1iS221 = _M0L6_2atmpS1343;
      _M0L1jS222 = _M0L6_2atmpS1344;
      continue;
    } else {
      moonbit_decref(_M0L3strS224);
      moonbit_decref(_M0L4selfS223);
    }
    break;
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS217,
  struct _M0TPC16string10StringView _M0L3objS216
) {
  struct _M0TPB6Logger _M0L6_2atmpS1341;
  #line 17 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1341
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS217
  };
  #line 21 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS216, _M0L6_2atmpS1341);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS161
) {
  int32_t _M0L6_2atmpS1340;
  struct _M0TPC16string10StringView _M0L7_2abindS160;
  moonbit_string_t _M0L7_2adataS162;
  int32_t _M0L8_2astartS163;
  int32_t _M0L6_2atmpS1339;
  int32_t _M0L6_2aendS164;
  int32_t _M0Lm9_2acursorS165;
  int32_t _M0Lm13accept__stateS166;
  int32_t _M0Lm10match__endS167;
  int32_t _M0Lm20match__tag__saver__0S168;
  int32_t _M0Lm20match__tag__saver__1S169;
  int32_t _M0Lm20match__tag__saver__2S170;
  int32_t _M0Lm20match__tag__saver__3S171;
  int32_t _M0Lm20match__tag__saver__4S172;
  int32_t _M0Lm6tag__0S173;
  int32_t _M0Lm9tag__0__1S174;
  int32_t _M0Lm9tag__0__2S175;
  int32_t _M0Lm6tag__2S176;
  int32_t _M0Lm6tag__1S177;
  int32_t _M0Lm9tag__1__1S178;
  int32_t _M0Lm6tag__4S179;
  int32_t _M0Lm6tag__3S180;
  int32_t _M0L6_2atmpS1298;
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1340 = Moonbit_array_length(_M0L4reprS161);
  _M0L7_2abindS160
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1340, _M0L4reprS161
  };
  moonbit_incref(_M0L7_2abindS160.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS162 = _M0MPC16string10StringView4data(_M0L7_2abindS160);
  moonbit_incref(_M0L7_2abindS160.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS163
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS160);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1339 = _M0MPC16string10StringView6length(_M0L7_2abindS160);
  _M0L6_2aendS164 = _M0L8_2astartS163 + _M0L6_2atmpS1339;
  _M0Lm9_2acursorS165 = _M0L8_2astartS163;
  _M0Lm13accept__stateS166 = -1;
  _M0Lm10match__endS167 = -1;
  _M0Lm20match__tag__saver__0S168 = -1;
  _M0Lm20match__tag__saver__1S169 = -1;
  _M0Lm20match__tag__saver__2S170 = -1;
  _M0Lm20match__tag__saver__3S171 = -1;
  _M0Lm20match__tag__saver__4S172 = -1;
  _M0Lm6tag__0S173 = -1;
  _M0Lm9tag__0__1S174 = -1;
  _M0Lm9tag__0__2S175 = -1;
  _M0Lm6tag__2S176 = -1;
  _M0Lm6tag__1S177 = -1;
  _M0Lm9tag__1__1S178 = -1;
  _M0Lm6tag__4S179 = -1;
  _M0Lm6tag__3S180 = -1;
  _M0L6_2atmpS1298 = _M0Lm9_2acursorS165;
  if (_M0L6_2atmpS1298 < _M0L6_2aendS164) {
    int32_t _M0L6_2atmpS1299 = _M0Lm9_2acursorS165;
    int32_t _M0L12dispatch__15S188;
    _M0Lm9_2acursorS165 = _M0L6_2atmpS1299 + 1;
    _M0L12dispatch__15S188 = 0;
    loop__label__15_191:;
    while (1) {
      int32_t _M0L6_2atmpS1303;
      int32_t _M0L6_2atmpS1300;
      switch (_M0L12dispatch__15S188) {
        case 6: {
          int32_t _M0L6_2atmpS1306;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1306 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1306 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1308 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS196;
            int32_t _M0L6_2atmpS1307;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS196
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1308);
            _M0L6_2atmpS1307 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1307 + 1;
            if (_M0L10next__charS196 == 58) {
              _M0L12dispatch__15S188 = 1;
              goto loop__label__15_191;
            } else {
              _M0L12dispatch__15S188 = 6;
              goto loop__label__15_191;
            }
          } else {
            goto join_193;
          }
          break;
        }
        
        case 3: {
          int32_t _M0L6_2atmpS1309;
          _M0Lm9tag__0__2S175 = _M0Lm9tag__0__1S174;
          _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1309 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1309 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1314 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS198;
            int32_t _M0L6_2atmpS1310;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS198
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1314);
            _M0L6_2atmpS1310 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1310 + 1;
            if (_M0L10next__charS198 < 58) {
              if (_M0L10next__charS198 < 48) {
                goto join_197;
              } else {
                int32_t _M0L6_2atmpS1311;
                _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
                _M0Lm9tag__1__1S178 = _M0Lm6tag__1S177;
                _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
                _M0Lm6tag__2S176 = _M0Lm9_2acursorS165;
                _M0L6_2atmpS1311 = _M0Lm9_2acursorS165;
                if (_M0L6_2atmpS1311 < _M0L6_2aendS164) {
                  int32_t _M0L6_2atmpS1313 = _M0Lm9_2acursorS165;
                  int32_t _M0L10next__charS200;
                  int32_t _M0L6_2atmpS1312;
                  moonbit_incref(_M0L7_2adataS162);
                  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS200
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1313);
                  _M0L6_2atmpS1312 = _M0Lm9_2acursorS165;
                  _M0Lm9_2acursorS165 = _M0L6_2atmpS1312 + 1;
                  if (_M0L10next__charS200 < 48) {
                    if (_M0L10next__charS200 == 45) {
                      goto join_189;
                    } else {
                      goto join_199;
                    }
                  } else if (_M0L10next__charS200 > 57) {
                    if (_M0L10next__charS200 < 59) {
                      _M0L12dispatch__15S188 = 3;
                      goto loop__label__15_191;
                    } else {
                      goto join_199;
                    }
                  } else {
                    _M0L12dispatch__15S188 = 7;
                    goto loop__label__15_191;
                  }
                  join_199:;
                  _M0L12dispatch__15S188 = 0;
                  goto loop__label__15_191;
                } else {
                  goto join_181;
                }
              }
            } else if (_M0L10next__charS198 > 58) {
              goto join_197;
            } else {
              _M0L12dispatch__15S188 = 1;
              goto loop__label__15_191;
            }
            join_197:;
            _M0L12dispatch__15S188 = 0;
            goto loop__label__15_191;
          } else {
            goto join_181;
          }
          break;
        }
        
        case 7: {
          int32_t _M0L6_2atmpS1315;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0Lm6tag__2S176 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1315 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1315 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1317 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS202;
            int32_t _M0L6_2atmpS1316;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS202
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1317);
            _M0L6_2atmpS1316 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1316 + 1;
            if (_M0L10next__charS202 < 48) {
              if (_M0L10next__charS202 == 45) {
                goto join_189;
              } else {
                goto join_201;
              }
            } else if (_M0L10next__charS202 > 57) {
              if (_M0L10next__charS202 < 59) {
                _M0L12dispatch__15S188 = 3;
                goto loop__label__15_191;
              } else {
                goto join_201;
              }
            } else {
              _M0L12dispatch__15S188 = 7;
              goto loop__label__15_191;
            }
            join_201:;
            _M0L12dispatch__15S188 = 0;
            goto loop__label__15_191;
          } else {
            goto join_181;
          }
          break;
        }
        
        case 5: {
          int32_t _M0L6_2atmpS1318;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0Lm6tag__4S179 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1318 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1318 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1320 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS204;
            int32_t _M0L6_2atmpS1319;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS204
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1320);
            _M0L6_2atmpS1319 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1319 + 1;
            if (_M0L10next__charS204 < 59) {
              if (_M0L10next__charS204 < 48) {
                goto join_203;
              } else if (_M0L10next__charS204 > 57) {
                _M0L12dispatch__15S188 = 3;
                goto loop__label__15_191;
              } else {
                _M0L12dispatch__15S188 = 5;
                goto loop__label__15_191;
              }
            } else if (_M0L10next__charS204 > 63) {
              if (_M0L10next__charS204 < 65) {
                goto join_194;
              } else {
                goto join_203;
              }
            } else {
              goto join_203;
            }
            join_203:;
            _M0L12dispatch__15S188 = 0;
            goto loop__label__15_191;
          } else {
            goto join_181;
          }
          break;
        }
        
        case 1: {
          int32_t _M0L6_2atmpS1321;
          _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1321 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1321 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1323 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS206;
            int32_t _M0L6_2atmpS1322;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS206
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1323);
            _M0L6_2atmpS1322 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1322 + 1;
            if (_M0L10next__charS206 < 58) {
              if (_M0L10next__charS206 < 48) {
                goto join_205;
              } else {
                _M0L12dispatch__15S188 = 2;
                goto loop__label__15_191;
              }
            } else if (_M0L10next__charS206 > 58) {
              goto join_205;
            } else {
              _M0L12dispatch__15S188 = 1;
              goto loop__label__15_191;
            }
            join_205:;
            _M0L12dispatch__15S188 = 0;
            goto loop__label__15_191;
          } else {
            goto join_181;
          }
          break;
        }
        
        case 4: {
          int32_t _M0L6_2atmpS1324;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__3S180 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1324 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1324 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1332 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS208;
            int32_t _M0L6_2atmpS1325;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS208
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1332);
            _M0L6_2atmpS1325 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1325 + 1;
            if (_M0L10next__charS208 < 58) {
              if (_M0L10next__charS208 < 48) {
                goto join_207;
              } else {
                _M0L12dispatch__15S188 = 4;
                goto loop__label__15_191;
              }
            } else if (_M0L10next__charS208 > 58) {
              goto join_207;
            } else {
              int32_t _M0L6_2atmpS1326;
              _M0Lm9tag__0__2S175 = _M0Lm9tag__0__1S174;
              _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
              _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
              _M0L6_2atmpS1326 = _M0Lm9_2acursorS165;
              if (_M0L6_2atmpS1326 < _M0L6_2aendS164) {
                int32_t _M0L6_2atmpS1331 = _M0Lm9_2acursorS165;
                int32_t _M0L10next__charS210;
                int32_t _M0L6_2atmpS1327;
                moonbit_incref(_M0L7_2adataS162);
                #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                _M0L10next__charS210
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1331);
                _M0L6_2atmpS1327 = _M0Lm9_2acursorS165;
                _M0Lm9_2acursorS165 = _M0L6_2atmpS1327 + 1;
                if (_M0L10next__charS210 < 58) {
                  if (_M0L10next__charS210 < 48) {
                    goto join_209;
                  } else {
                    int32_t _M0L6_2atmpS1328;
                    _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
                    _M0Lm9tag__1__1S178 = _M0Lm6tag__1S177;
                    _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
                    _M0Lm6tag__4S179 = _M0Lm9_2acursorS165;
                    _M0L6_2atmpS1328 = _M0Lm9_2acursorS165;
                    if (_M0L6_2atmpS1328 < _M0L6_2aendS164) {
                      int32_t _M0L6_2atmpS1330 = _M0Lm9_2acursorS165;
                      int32_t _M0L10next__charS212;
                      int32_t _M0L6_2atmpS1329;
                      moonbit_incref(_M0L7_2adataS162);
                      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS212
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1330);
                      _M0L6_2atmpS1329 = _M0Lm9_2acursorS165;
                      _M0Lm9_2acursorS165 = _M0L6_2atmpS1329 + 1;
                      if (_M0L10next__charS212 < 59) {
                        if (_M0L10next__charS212 < 48) {
                          goto join_211;
                        } else if (_M0L10next__charS212 > 57) {
                          _M0L12dispatch__15S188 = 3;
                          goto loop__label__15_191;
                        } else {
                          _M0L12dispatch__15S188 = 5;
                          goto loop__label__15_191;
                        }
                      } else if (_M0L10next__charS212 > 63) {
                        if (_M0L10next__charS212 < 65) {
                          goto join_194;
                        } else {
                          goto join_211;
                        }
                      } else {
                        goto join_211;
                      }
                      join_211:;
                      _M0L12dispatch__15S188 = 0;
                      goto loop__label__15_191;
                    } else {
                      goto join_181;
                    }
                  }
                } else if (_M0L10next__charS210 > 58) {
                  goto join_209;
                } else {
                  _M0L12dispatch__15S188 = 1;
                  goto loop__label__15_191;
                }
                join_209:;
                _M0L12dispatch__15S188 = 0;
                goto loop__label__15_191;
              } else {
                goto join_181;
              }
            }
            join_207:;
            _M0L12dispatch__15S188 = 0;
            goto loop__label__15_191;
          } else {
            goto join_181;
          }
          break;
        }
        
        case 2: {
          int32_t _M0L6_2atmpS1333;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1333 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1333 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1335 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS214;
            int32_t _M0L6_2atmpS1334;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS214
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1335);
            _M0L6_2atmpS1334 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1334 + 1;
            if (_M0L10next__charS214 < 58) {
              if (_M0L10next__charS214 < 48) {
                goto join_213;
              } else {
                _M0L12dispatch__15S188 = 2;
                goto loop__label__15_191;
              }
            } else if (_M0L10next__charS214 > 58) {
              goto join_213;
            } else {
              _M0L12dispatch__15S188 = 3;
              goto loop__label__15_191;
            }
            join_213:;
            _M0L12dispatch__15S188 = 0;
            goto loop__label__15_191;
          } else {
            goto join_181;
          }
          break;
        }
        
        case 0: {
          int32_t _M0L6_2atmpS1336;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1336 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1336 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1338 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS215;
            int32_t _M0L6_2atmpS1337;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS215
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1338);
            _M0L6_2atmpS1337 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1337 + 1;
            if (_M0L10next__charS215 == 58) {
              _M0L12dispatch__15S188 = 1;
              goto loop__label__15_191;
            } else {
              _M0L12dispatch__15S188 = 0;
              goto loop__label__15_191;
            }
          } else {
            goto join_181;
          }
          break;
        }
        default: {
          goto join_181;
          break;
        }
      }
      join_194:;
      _M0Lm9tag__0__1S174 = _M0Lm9tag__0__2S175;
      _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
      _M0Lm6tag__1S177 = _M0Lm9tag__1__1S178;
      _M0L6_2atmpS1303 = _M0Lm9_2acursorS165;
      if (_M0L6_2atmpS1303 < _M0L6_2aendS164) {
        int32_t _M0L6_2atmpS1305 = _M0Lm9_2acursorS165;
        int32_t _M0L10next__charS195;
        int32_t _M0L6_2atmpS1304;
        moonbit_incref(_M0L7_2adataS162);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS195
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1305);
        _M0L6_2atmpS1304 = _M0Lm9_2acursorS165;
        _M0Lm9_2acursorS165 = _M0L6_2atmpS1304 + 1;
        if (_M0L10next__charS195 == 58) {
          _M0L12dispatch__15S188 = 1;
          continue;
        } else {
          _M0L12dispatch__15S188 = 6;
          continue;
        }
      } else {
        goto join_193;
      }
      join_193:;
      _M0Lm6tag__0S173 = _M0Lm9tag__0__1S174;
      _M0Lm20match__tag__saver__0S168 = _M0Lm6tag__0S173;
      _M0Lm20match__tag__saver__1S169 = _M0Lm6tag__1S177;
      _M0Lm20match__tag__saver__2S170 = _M0Lm6tag__2S176;
      _M0Lm20match__tag__saver__3S171 = _M0Lm6tag__3S180;
      _M0Lm20match__tag__saver__4S172 = _M0Lm6tag__4S179;
      _M0Lm13accept__stateS166 = 0;
      _M0Lm10match__endS167 = _M0Lm9_2acursorS165;
      goto join_181;
      join_189:;
      _M0Lm9tag__0__1S174 = _M0Lm9tag__0__2S175;
      _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
      _M0Lm6tag__1S177 = _M0Lm9tag__1__1S178;
      _M0L6_2atmpS1300 = _M0Lm9_2acursorS165;
      if (_M0L6_2atmpS1300 < _M0L6_2aendS164) {
        int32_t _M0L6_2atmpS1302 = _M0Lm9_2acursorS165;
        int32_t _M0L10next__charS192;
        int32_t _M0L6_2atmpS1301;
        moonbit_incref(_M0L7_2adataS162);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS192
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1302);
        _M0L6_2atmpS1301 = _M0Lm9_2acursorS165;
        _M0Lm9_2acursorS165 = _M0L6_2atmpS1301 + 1;
        if (_M0L10next__charS192 < 58) {
          if (_M0L10next__charS192 < 48) {
            goto join_190;
          } else {
            _M0L12dispatch__15S188 = 4;
            continue;
          }
        } else if (_M0L10next__charS192 > 58) {
          goto join_190;
        } else {
          _M0L12dispatch__15S188 = 1;
          continue;
        }
        join_190:;
        _M0L12dispatch__15S188 = 0;
        continue;
      } else {
        goto join_181;
      }
      break;
    }
  } else {
    goto join_181;
  }
  join_181:;
  switch (_M0Lm13accept__stateS166) {
    case 0: {
      int32_t _M0L6_2atmpS1297 = _M0Lm20match__tag__saver__0S168;
      int32_t _M0L6_2atmpS1296 = _M0L6_2atmpS1297 + 1;
      int64_t _M0L6_2atmpS1293 = (int64_t)_M0L6_2atmpS1296;
      int32_t _M0L6_2atmpS1295 = _M0Lm20match__tag__saver__1S169;
      int64_t _M0L6_2atmpS1294 = (int64_t)_M0L6_2atmpS1295;
      struct _M0TPC16string10StringView _M0L11start__lineS182;
      int32_t _M0L6_2atmpS1292;
      int32_t _M0L6_2atmpS1291;
      int64_t _M0L6_2atmpS1288;
      int32_t _M0L6_2atmpS1290;
      int64_t _M0L6_2atmpS1289;
      struct _M0TPC16string10StringView _M0L13start__columnS183;
      int64_t _M0L6_2atmpS1285;
      int32_t _M0L6_2atmpS1287;
      int64_t _M0L6_2atmpS1286;
      struct _M0TPC16string10StringView _M0L8filenameS184;
      int32_t _M0L6_2atmpS1284;
      int32_t _M0L6_2atmpS1283;
      int64_t _M0L6_2atmpS1280;
      int32_t _M0L6_2atmpS1282;
      int64_t _M0L6_2atmpS1281;
      struct _M0TPC16string10StringView _M0L9end__lineS185;
      int32_t _M0L6_2atmpS1279;
      int32_t _M0L6_2atmpS1278;
      int64_t _M0L6_2atmpS1275;
      int32_t _M0L6_2atmpS1277;
      int64_t _M0L6_2atmpS1276;
      struct _M0TPC16string10StringView _M0L11end__columnS186;
      int32_t _M0L6_2atmpS1274;
      int32_t _M0L6_2atmpS1273;
      int64_t _M0L6_2atmpS1270;
      int32_t _M0L6_2atmpS1272;
      int64_t _M0L6_2atmpS1271;
      struct _M0TPC16string10StringView _M0L6_2atmpS2306;
      struct _M0TPB13SourceLocRepr* _block_2558;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS182
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1293, _M0L6_2atmpS1294);
      _M0L6_2atmpS1292 = _M0Lm20match__tag__saver__1S169;
      _M0L6_2atmpS1291 = _M0L6_2atmpS1292 + 1;
      _M0L6_2atmpS1288 = (int64_t)_M0L6_2atmpS1291;
      _M0L6_2atmpS1290 = _M0Lm20match__tag__saver__2S170;
      _M0L6_2atmpS1289 = (int64_t)_M0L6_2atmpS1290;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS183
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1288, _M0L6_2atmpS1289);
      _M0L6_2atmpS1285 = (int64_t)_M0L8_2astartS163;
      _M0L6_2atmpS1287 = _M0Lm20match__tag__saver__0S168;
      _M0L6_2atmpS1286 = (int64_t)_M0L6_2atmpS1287;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS184
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1285, _M0L6_2atmpS1286);
      _M0L6_2atmpS1284 = _M0Lm20match__tag__saver__2S170;
      _M0L6_2atmpS1283 = _M0L6_2atmpS1284 + 1;
      _M0L6_2atmpS1280 = (int64_t)_M0L6_2atmpS1283;
      _M0L6_2atmpS1282 = _M0Lm20match__tag__saver__3S171;
      _M0L6_2atmpS1281 = (int64_t)_M0L6_2atmpS1282;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS185
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1280, _M0L6_2atmpS1281);
      _M0L6_2atmpS1279 = _M0Lm20match__tag__saver__3S171;
      _M0L6_2atmpS1278 = _M0L6_2atmpS1279 + 1;
      _M0L6_2atmpS1275 = (int64_t)_M0L6_2atmpS1278;
      _M0L6_2atmpS1277 = _M0Lm20match__tag__saver__4S172;
      _M0L6_2atmpS1276 = (int64_t)_M0L6_2atmpS1277;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS186
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1275, _M0L6_2atmpS1276);
      _M0L6_2atmpS1274 = _M0Lm20match__tag__saver__4S172;
      _M0L6_2atmpS1273 = _M0L6_2atmpS1274 + 1;
      _M0L6_2atmpS1270 = (int64_t)_M0L6_2atmpS1273;
      _M0L6_2atmpS1272 = _M0Lm10match__endS167;
      _M0L6_2atmpS1271 = (int64_t)_M0L6_2atmpS1272;
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6_2atmpS2306
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1270, _M0L6_2atmpS1271);
      moonbit_decref(_M0L6_2atmpS2306.$0);
      _block_2558
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2558)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 5, 0);
      _block_2558->$0_0 = _M0L8filenameS184.$0;
      _block_2558->$0_1 = _M0L8filenameS184.$1;
      _block_2558->$0_2 = _M0L8filenameS184.$2;
      _block_2558->$1_0 = _M0L11start__lineS182.$0;
      _block_2558->$1_1 = _M0L11start__lineS182.$1;
      _block_2558->$1_2 = _M0L11start__lineS182.$2;
      _block_2558->$2_0 = _M0L13start__columnS183.$0;
      _block_2558->$2_1 = _M0L13start__columnS183.$1;
      _block_2558->$2_2 = _M0L13start__columnS183.$2;
      _block_2558->$3_0 = _M0L9end__lineS185.$0;
      _block_2558->$3_1 = _M0L9end__lineS185.$1;
      _block_2558->$3_2 = _M0L9end__lineS185.$2;
      _block_2558->$4_0 = _M0L11end__columnS186.$0;
      _block_2558->$4_1 = _M0L11end__columnS186.$1;
      _block_2558->$4_2 = _M0L11end__columnS186.$2;
      return _block_2558;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS162);
      #line 77 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC16string6String14escape_2einner(
  moonbit_string_t _M0L4selfS158,
  int32_t _M0L5quoteS159
) {
  struct _M0TPB13StringBuilder* _M0L3bufS157;
  int32_t _M0L6_2atmpS1269;
  struct _M0TPC16string10StringView _M0L6_2atmpS1267;
  struct _M0TPB6Logger _M0L6_2atmpS1268;
  #line 145 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 146 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS157 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS1269 = Moonbit_array_length(_M0L4selfS158);
  _M0L6_2atmpS1267
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1269, _M0L4selfS158
  };
  moonbit_incref(_M0L3bufS157);
  _M0L6_2atmpS1268
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS157
  };
  #line 147 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1267, _M0L6_2atmpS1268, _M0L5quoteS159);
  #line 148 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS157);
}

int32_t _M0MPC16string10StringView18escape__to_2einner(
  struct _M0TPC16string10StringView _M0L4selfS149,
  struct _M0TPB6Logger _M0L6loggerS147,
  int32_t _M0L5quoteS146
) {
  int32_t _M0L3lenS148;
  struct _M0TURPB6LoggerRPC16string10StringViewE* _M0L6_2aenvS150;
  int32_t _M0L1iS151;
  int32_t _M0L3segS152;
  #line 179 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L5quoteS146) {
    if (_M0L6loggerS147.$1) {
      moonbit_incref(_M0L6loggerS147.$1);
    }
    #line 185 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, 34);
  }
  moonbit_incref(_M0L4selfS149.$0);
  #line 187 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3lenS148 = _M0MPC16string10StringView6length(_M0L4selfS149);
  if (_M0L6loggerS147.$1) {
    moonbit_incref(_M0L6loggerS147.$1);
  }
  moonbit_incref(_M0L4selfS149.$0);
  _M0L6_2aenvS150
  = (struct _M0TURPB6LoggerRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TURPB6LoggerRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2aenvS150)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPB6LoggerRPC16string10StringViewE, $0_0) >> 2, 3, 0);
  _M0L6_2aenvS150->$0_0 = _M0L6loggerS147.$0;
  _M0L6_2aenvS150->$0_1 = _M0L6loggerS147.$1;
  _M0L6_2aenvS150->$1_0 = _M0L4selfS149.$0;
  _M0L6_2aenvS150->$1_1 = _M0L4selfS149.$1;
  _M0L6_2aenvS150->$1_2 = _M0L4selfS149.$2;
  _M0L1iS151 = 0;
  _M0L3segS152 = 0;
  _2afor_153:;
  while (1) {
    int32_t _M0L4codeS154;
    int32_t _M0L1cS156;
    int32_t _M0L6_2atmpS1251;
    int32_t _M0L6_2atmpS1252;
    int32_t _M0L6_2atmpS1253;
    int32_t _tmp_2562;
    int32_t _tmp_2563;
    if (_M0L1iS151 >= _M0L3lenS148) {
      moonbit_decref(_M0L4selfS149.$0);
      #line 195 "/Users/user/.moon/lib/core/builtin/show.mbt"
      _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
      break;
    }
    moonbit_incref(_M0L4selfS149.$0);
    #line 198 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L4codeS154
    = _M0MPC16string10StringView11unsafe__get(_M0L4selfS149, _M0L1iS151);
    switch (_M0L4codeS154) {
      case 34: {
        _M0L1cS156 = _M0L4codeS154;
        goto join_155;
        break;
      }
      
      case 92: {
        _M0L1cS156 = _M0L4codeS154;
        goto join_155;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1254;
        int32_t _M0L6_2atmpS1255;
        moonbit_incref(_M0L6_2aenvS150);
        #line 207 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 208 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_99.data);
        _M0L6_2atmpS1254 = _M0L1iS151 + 1;
        _M0L6_2atmpS1255 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1254;
        _M0L3segS152 = _M0L6_2atmpS1255;
        goto _2afor_153;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1256;
        int32_t _M0L6_2atmpS1257;
        moonbit_incref(_M0L6_2aenvS150);
        #line 212 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 213 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_100.data);
        _M0L6_2atmpS1256 = _M0L1iS151 + 1;
        _M0L6_2atmpS1257 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1256;
        _M0L3segS152 = _M0L6_2atmpS1257;
        goto _2afor_153;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1258;
        int32_t _M0L6_2atmpS1259;
        moonbit_incref(_M0L6_2aenvS150);
        #line 217 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 218 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_101.data);
        _M0L6_2atmpS1258 = _M0L1iS151 + 1;
        _M0L6_2atmpS1259 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1258;
        _M0L3segS152 = _M0L6_2atmpS1259;
        goto _2afor_153;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1260;
        int32_t _M0L6_2atmpS1261;
        moonbit_incref(_M0L6_2aenvS150);
        #line 222 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 223 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_102.data);
        _M0L6_2atmpS1260 = _M0L1iS151 + 1;
        _M0L6_2atmpS1261 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1260;
        _M0L3segS152 = _M0L6_2atmpS1261;
        goto _2afor_153;
        break;
      }
      default: {
        if (_M0L4codeS154 < 32) {
          int32_t _M0L6_2atmpS1263;
          moonbit_string_t _M0L6_2atmpS1262;
          int32_t _M0L6_2atmpS1264;
          int32_t _M0L6_2atmpS1265;
          moonbit_incref(_M0L6_2aenvS150);
          #line 228 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 229 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_103.data);
          _M0L6_2atmpS1263 = _M0L4codeS154 & 0xff;
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1262 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1263);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, _M0L6_2atmpS1262);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 231 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, 125);
          _M0L6_2atmpS1264 = _M0L1iS151 + 1;
          _M0L6_2atmpS1265 = _M0L1iS151 + 1;
          _M0L1iS151 = _M0L6_2atmpS1264;
          _M0L3segS152 = _M0L6_2atmpS1265;
          goto _2afor_153;
        } else {
          int32_t _M0L6_2atmpS1266 = _M0L1iS151 + 1;
          int32_t _tmp_2561 = _M0L3segS152;
          _M0L1iS151 = _M0L6_2atmpS1266;
          _M0L3segS152 = _tmp_2561;
          goto _2afor_153;
        }
        break;
      }
    }
    goto joinlet_2560;
    join_155:;
    moonbit_incref(_M0L6_2aenvS150);
    #line 201 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
    if (_M0L6loggerS147.$1) {
      moonbit_incref(_M0L6loggerS147.$1);
    }
    #line 202 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, 92);
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1251 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS156);
    if (_M0L6loggerS147.$1) {
      moonbit_incref(_M0L6loggerS147.$1);
    }
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, _M0L6_2atmpS1251);
    _M0L6_2atmpS1252 = _M0L1iS151 + 1;
    _M0L6_2atmpS1253 = _M0L1iS151 + 1;
    _M0L1iS151 = _M0L6_2atmpS1252;
    _M0L3segS152 = _M0L6_2atmpS1253;
    continue;
    joinlet_2560:;
    _tmp_2562 = _M0L1iS151;
    _tmp_2563 = _M0L3segS152;
    _M0L1iS151 = _tmp_2562;
    _M0L3segS152 = _tmp_2563;
    continue;
    break;
  }
  if (_M0L5quoteS146) {
    #line 239 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, 34);
  } else if (_M0L6loggerS147.$1) {
    moonbit_decref(_M0L6loggerS147.$1);
  }
  return 0;
}

int32_t _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(
  struct _M0TURPB6LoggerRPC16string10StringViewE* _M0L6_2aenvS142,
  int32_t _M0L3segS145,
  int32_t _M0L1iS144
) {
  struct _M0TPC16string10StringView _M0L4selfS141;
  struct _M0TPB6Logger _M0L8_2afieldS2307;
  int32_t _M0L6_2acntS2407;
  struct _M0TPB6Logger _M0L6loggerS143;
  #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L4selfS141
  = (struct _M0TPC16string10StringView){
    _M0L6_2aenvS142->$1_1, _M0L6_2aenvS142->$1_2, _M0L6_2aenvS142->$1_0
  };
  _M0L8_2afieldS2307
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS142->$0_0, _M0L6_2aenvS142->$0_1
  };
  _M0L6_2acntS2407 = Moonbit_object_header(_M0L6_2aenvS142)->rc;
  if (_M0L6_2acntS2407 > 1) {
    int32_t _M0L11_2anew__cntS2408 = _M0L6_2acntS2407 - 1;
    Moonbit_object_header(_M0L6_2aenvS142)->rc = _M0L11_2anew__cntS2408;
    moonbit_incref(_M0L4selfS141.$0);
    if (_M0L8_2afieldS2307.$1) {
      moonbit_incref(_M0L8_2afieldS2307.$1);
    }
  } else if (_M0L6_2acntS2407 == 1) {
    #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS142);
  }
  _M0L6loggerS143 = _M0L8_2afieldS2307;
  if (_M0L1iS144 > _M0L3segS145) {
    int64_t _M0L6_2atmpS1250 = (int64_t)_M0L1iS144;
    struct _M0TPC16string10StringView _M0L6_2atmpS1249;
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1249
    = _M0MPC16string10StringView11sub_2einner(_M0L4selfS141, _M0L3segS145, _M0L6_2atmpS1250);
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS143.$0->$method_2(_M0L6loggerS143.$1, _M0L6_2atmpS1249);
  } else {
    if (_M0L6loggerS143.$1) {
      moonbit_decref(_M0L6loggerS143.$1);
    }
    moonbit_decref(_M0L4selfS141.$0);
  }
  return 0;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS139,
  int32_t _M0L5indexS140
) {
  moonbit_string_t _M0L3strS1246;
  int32_t _M0L5startS1248;
  int32_t _M0L6_2atmpS1247;
  int32_t _result_2564;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1246 = _M0L4selfS139.$0;
  _M0L5startS1248 = _M0L4selfS139.$1;
  _M0L6_2atmpS1247 = _M0L5startS1248 + _M0L5indexS140;
  _result_2564 = _M0L3strS1246[_M0L6_2atmpS1247];
  moonbit_decref(_M0L3strS1246);
  return _result_2564;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView _M0L4selfS132,
  int32_t _M0L5startS138,
  int64_t _M0L3endS134
) {
  moonbit_string_t _M0L3strS1245;
  int32_t _M0L8str__lenS131;
  int32_t _M0L8abs__endS133;
  int32_t _M0L10abs__startS137;
  int32_t _M0L5startS1233;
  int32_t _if__result_2565;
  #line 712 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1245 = _M0L4selfS132.$0;
  _M0L8str__lenS131 = Moonbit_array_length(_M0L3strS1245);
  if (_M0L3endS134 == 4294967296ll) {
    _M0L8abs__endS133 = _M0L4selfS132.$2;
  } else {
    int64_t _M0L7_2aSomeS135 = _M0L3endS134;
    int32_t _M0L6_2aendS136 = (int32_t)_M0L7_2aSomeS135;
    if (_M0L6_2aendS136 < 0) {
      int32_t _M0L3endS1243 = _M0L4selfS132.$2;
      _M0L8abs__endS133 = _M0L3endS1243 + _M0L6_2aendS136;
    } else {
      int32_t _M0L5startS1244 = _M0L4selfS132.$1;
      _M0L8abs__endS133 = _M0L5startS1244 + _M0L6_2aendS136;
    }
  }
  if (_M0L5startS138 < 0) {
    int32_t _M0L3endS1241 = _M0L4selfS132.$2;
    _M0L10abs__startS137 = _M0L3endS1241 + _M0L5startS138;
  } else {
    int32_t _M0L5startS1242 = _M0L4selfS132.$1;
    _M0L10abs__startS137 = _M0L5startS1242 + _M0L5startS138;
  }
  _M0L5startS1233 = _M0L4selfS132.$1;
  if (_M0L10abs__startS137 >= _M0L5startS1233) {
    if (_M0L10abs__startS137 <= _M0L8abs__endS133) {
      int32_t _M0L3endS1232 = _M0L4selfS132.$2;
      _if__result_2565 = _M0L8abs__endS133 <= _M0L3endS1232;
    } else {
      _if__result_2565 = 0;
    }
  } else {
    _if__result_2565 = 0;
  }
  if (_if__result_2565) {
    moonbit_string_t _M0L3strS1240;
    if (_M0L10abs__startS137 < _M0L8str__lenS131) {
      moonbit_string_t _M0L3strS1236 = _M0L4selfS132.$0;
      int32_t _M0L6_2atmpS1235 = _M0L3strS1236[_M0L10abs__startS137];
      int32_t _M0L6_2atmpS1234;
      #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1234
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1235);
      if (!_M0L6_2atmpS1234) {
        
      } else {
        #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L8abs__endS133 < _M0L8str__lenS131) {
      moonbit_string_t _M0L3strS1239 = _M0L4selfS132.$0;
      int32_t _M0L6_2atmpS1238 = _M0L3strS1239[_M0L8abs__endS133];
      int32_t _M0L6_2atmpS1237;
      #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1237
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1238);
      if (!_M0L6_2atmpS1237) {
        
      } else {
        #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    _M0L3strS1240 = _M0L4selfS132.$0;
    return (struct _M0TPC16string10StringView){_M0L10abs__startS137,
                                                 _M0L8abs__endS133,
                                                 _M0L3strS1240};
  } else {
    moonbit_decref(_M0L4selfS132.$0);
    #line 732 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS130
) {
  int32_t _M0L3endS1230;
  int32_t _M0L5startS1231;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1230 = _M0L4selfS130.$2;
  _M0L5startS1231 = _M0L4selfS130.$1;
  moonbit_decref(_M0L4selfS130.$0);
  return _M0L3endS1230 - _M0L5startS1231;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS129) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS128;
  int32_t _M0L6_2atmpS1227;
  int32_t _M0L6_2atmpS1226;
  int32_t _M0L6_2atmpS1229;
  int32_t _M0L6_2atmpS1228;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1225;
  #line 109 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS128 = _M0MPB13StringBuilder11new_2einner(0);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1227 = _M0IPC14byte4BytePB3Div3div(_M0L1bS129, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1226
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1227);
  moonbit_incref(_M0L7_2aselfS128);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS128, _M0L6_2atmpS1226);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1229 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS129, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1228
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1229);
  moonbit_incref(_M0L7_2aselfS128);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS128, _M0L6_2atmpS1228);
  _M0L6_2atmpS1225 = _M0L7_2aselfS128;
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1225);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t _M0L1iS127) {
  #line 110 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS127 < 10) {
    int32_t _M0L6_2atmpS1222;
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1222 = _M0IPC14byte4BytePB3Add3add(_M0L1iS127, 48);
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1222);
  } else {
    int32_t _M0L6_2atmpS1224;
    int32_t _M0L6_2atmpS1223;
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1224 = _M0IPC14byte4BytePB3Add3add(_M0L1iS127, 97);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1223 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1224, 10);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1223);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS125,
  int32_t _M0L4thatS126
) {
  int32_t _M0L6_2atmpS1220;
  int32_t _M0L6_2atmpS1221;
  int32_t _M0L6_2atmpS1219;
  #line 120 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1220 = (int32_t)_M0L4selfS125;
  _M0L6_2atmpS1221 = (int32_t)_M0L4thatS126;
  _M0L6_2atmpS1219 = _M0L6_2atmpS1220 - _M0L6_2atmpS1221;
  return _M0L6_2atmpS1219 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS123,
  int32_t _M0L4thatS124
) {
  int32_t _M0L6_2atmpS1217;
  int32_t _M0L6_2atmpS1218;
  int32_t _M0L6_2atmpS1216;
  #line 67 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1217 = (int32_t)_M0L4selfS123;
  _M0L6_2atmpS1218 = (int32_t)_M0L4thatS124;
  _M0L6_2atmpS1216 = _M0L6_2atmpS1217 % _M0L6_2atmpS1218;
  return _M0L6_2atmpS1216 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS121,
  int32_t _M0L4thatS122
) {
  int32_t _M0L6_2atmpS1214;
  int32_t _M0L6_2atmpS1215;
  int32_t _M0L6_2atmpS1213;
  #line 62 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1214 = (int32_t)_M0L4selfS121;
  _M0L6_2atmpS1215 = (int32_t)_M0L4thatS122;
  _M0L6_2atmpS1213 = _M0L6_2atmpS1214 / _M0L6_2atmpS1215;
  return _M0L6_2atmpS1213 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS119,
  int32_t _M0L4thatS120
) {
  int32_t _M0L6_2atmpS1211;
  int32_t _M0L6_2atmpS1212;
  int32_t _M0L6_2atmpS1210;
  #line 106 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1211 = (int32_t)_M0L4selfS119;
  _M0L6_2atmpS1212 = (int32_t)_M0L4thatS120;
  _M0L6_2atmpS1210 = _M0L6_2atmpS1211 + _M0L6_2atmpS1212;
  return _M0L6_2atmpS1210 & 0xff;
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS113
) {
  int32_t _M0L17codepoint__lengthS112;
  int32_t _M0L6_2atmpS1209;
  moonbit_bytes_t _M0L4dataS114;
  int32_t _M0L1iS115;
  int32_t _M0L12utf16__indexS116;
  #line 102 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS113);
  #line 104 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS112
  = _M0MPC16string6String20char__length_2einner(_M0L1sS113, 0, 4294967296ll);
  _M0L6_2atmpS1209 = _M0L17codepoint__lengthS112 * 4;
  _M0L4dataS114 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1209, 0);
  _M0L1iS115 = 0;
  _M0L12utf16__indexS116 = 0;
  while (1) {
    if (_M0L1iS115 < _M0L17codepoint__lengthS112) {
      int32_t _M0L6_2atmpS1206;
      int32_t _M0L1cS117;
      int32_t _M0L6_2atmpS1207;
      int32_t _M0L6_2atmpS1208;
      moonbit_incref(_M0L1sS113);
      #line 109 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1206
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS113, _M0L12utf16__indexS116);
      _M0L1cS117 = _M0L6_2atmpS1206;
      if (_M0L1cS117 > 65535) {
        int32_t _M0L6_2atmpS1174 = _M0L1iS115 * 4;
        int32_t _M0L6_2atmpS1176 = _M0L1cS117 & 255;
        int32_t _M0L6_2atmpS1175 = _M0L6_2atmpS1176 & 0xff;
        int32_t _M0L6_2atmpS1181;
        int32_t _M0L6_2atmpS1177;
        int32_t _M0L6_2atmpS1180;
        int32_t _M0L6_2atmpS1179;
        int32_t _M0L6_2atmpS1178;
        int32_t _M0L6_2atmpS1186;
        int32_t _M0L6_2atmpS1182;
        int32_t _M0L6_2atmpS1185;
        int32_t _M0L6_2atmpS1184;
        int32_t _M0L6_2atmpS1183;
        int32_t _M0L6_2atmpS1191;
        int32_t _M0L6_2atmpS1187;
        int32_t _M0L6_2atmpS1190;
        int32_t _M0L6_2atmpS1189;
        int32_t _M0L6_2atmpS1188;
        int32_t _M0L6_2atmpS1192;
        int32_t _M0L6_2atmpS1193;
        if (
          _M0L6_2atmpS1174 < 0
          || _M0L6_2atmpS1174 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 111 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1174] = _M0L6_2atmpS1175;
        _M0L6_2atmpS1181 = _M0L1iS115 * 4;
        _M0L6_2atmpS1177 = _M0L6_2atmpS1181 + 1;
        _M0L6_2atmpS1180 = _M0L1cS117 >> 8;
        _M0L6_2atmpS1179 = _M0L6_2atmpS1180 & 255;
        _M0L6_2atmpS1178 = _M0L6_2atmpS1179 & 0xff;
        if (
          _M0L6_2atmpS1177 < 0
          || _M0L6_2atmpS1177 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 112 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1177] = _M0L6_2atmpS1178;
        _M0L6_2atmpS1186 = _M0L1iS115 * 4;
        _M0L6_2atmpS1182 = _M0L6_2atmpS1186 + 2;
        _M0L6_2atmpS1185 = _M0L1cS117 >> 16;
        _M0L6_2atmpS1184 = _M0L6_2atmpS1185 & 255;
        _M0L6_2atmpS1183 = _M0L6_2atmpS1184 & 0xff;
        if (
          _M0L6_2atmpS1182 < 0
          || _M0L6_2atmpS1182 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 113 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1182] = _M0L6_2atmpS1183;
        _M0L6_2atmpS1191 = _M0L1iS115 * 4;
        _M0L6_2atmpS1187 = _M0L6_2atmpS1191 + 3;
        _M0L6_2atmpS1190 = _M0L1cS117 >> 24;
        _M0L6_2atmpS1189 = _M0L6_2atmpS1190 & 255;
        _M0L6_2atmpS1188 = _M0L6_2atmpS1189 & 0xff;
        if (
          _M0L6_2atmpS1187 < 0
          || _M0L6_2atmpS1187 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 114 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1187] = _M0L6_2atmpS1188;
        _M0L6_2atmpS1192 = _M0L1iS115 + 1;
        _M0L6_2atmpS1193 = _M0L12utf16__indexS116 + 2;
        _M0L1iS115 = _M0L6_2atmpS1192;
        _M0L12utf16__indexS116 = _M0L6_2atmpS1193;
        continue;
      } else {
        int32_t _M0L6_2atmpS1194 = _M0L1iS115 * 4;
        int32_t _M0L6_2atmpS1196 = _M0L1cS117 & 255;
        int32_t _M0L6_2atmpS1195 = _M0L6_2atmpS1196 & 0xff;
        int32_t _M0L6_2atmpS1201;
        int32_t _M0L6_2atmpS1197;
        int32_t _M0L6_2atmpS1200;
        int32_t _M0L6_2atmpS1199;
        int32_t _M0L6_2atmpS1198;
        int32_t _M0L6_2atmpS1203;
        int32_t _M0L6_2atmpS1202;
        int32_t _M0L6_2atmpS1205;
        int32_t _M0L6_2atmpS1204;
        if (
          _M0L6_2atmpS1194 < 0
          || _M0L6_2atmpS1194 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 117 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1194] = _M0L6_2atmpS1195;
        _M0L6_2atmpS1201 = _M0L1iS115 * 4;
        _M0L6_2atmpS1197 = _M0L6_2atmpS1201 + 1;
        _M0L6_2atmpS1200 = _M0L1cS117 >> 8;
        _M0L6_2atmpS1199 = _M0L6_2atmpS1200 & 255;
        _M0L6_2atmpS1198 = _M0L6_2atmpS1199 & 0xff;
        if (
          _M0L6_2atmpS1197 < 0
          || _M0L6_2atmpS1197 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 118 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1197] = _M0L6_2atmpS1198;
        _M0L6_2atmpS1203 = _M0L1iS115 * 4;
        _M0L6_2atmpS1202 = _M0L6_2atmpS1203 + 2;
        if (
          _M0L6_2atmpS1202 < 0
          || _M0L6_2atmpS1202 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 119 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1202] = 0;
        _M0L6_2atmpS1205 = _M0L1iS115 * 4;
        _M0L6_2atmpS1204 = _M0L6_2atmpS1205 + 3;
        if (
          _M0L6_2atmpS1204 < 0
          || _M0L6_2atmpS1204 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 120 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1204] = 0;
      }
      _M0L6_2atmpS1207 = _M0L1iS115 + 1;
      _M0L6_2atmpS1208 = _M0L12utf16__indexS116 + 1;
      _M0L1iS115 = _M0L6_2atmpS1207;
      _M0L12utf16__indexS116 = _M0L6_2atmpS1208;
      continue;
    } else {
      moonbit_decref(_M0L1sS113);
    }
    break;
  }
  #line 123 "/Users/user/.moon/lib/core/builtin/console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS114);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS109,
  int32_t _M0L5indexS110
) {
  int32_t _M0L2c1S108;
  #line 91 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
  _M0L2c1S108 = _M0L4selfS109[_M0L5indexS110];
  #line 94 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S108)) {
    int32_t _M0L6_2atmpS1173 = _M0L5indexS110 + 1;
    int32_t _M0L2c2S111 = _M0L4selfS109[_M0L6_2atmpS1173];
    int32_t _M0L6_2atmpS1171;
    int32_t _M0L6_2atmpS1172;
    moonbit_decref(_M0L4selfS109);
    _M0L6_2atmpS1171 = (int32_t)_M0L2c1S108;
    _M0L6_2atmpS1172 = (int32_t)_M0L2c2S111;
    #line 96 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1171, _M0L6_2atmpS1172);
  } else {
    moonbit_decref(_M0L4selfS109);
    #line 98 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S108);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS107) {
  int32_t _M0L6_2atmpS1170;
  #line 68 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS1170 = (int32_t)_M0L4selfS107;
  return _M0L6_2atmpS1170;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS105,
  int32_t _M0L8trailingS106
) {
  int32_t _M0L6_2atmpS1169;
  int32_t _M0L6_2atmpS1168;
  int32_t _M0L6_2atmpS1167;
  int32_t _M0L6_2atmpS1166;
  int32_t _M0L6_2atmpS1165;
  #line 40 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1169 = _M0L7leadingS105 - 55296;
  _M0L6_2atmpS1168 = _M0L6_2atmpS1169 * 1024;
  _M0L6_2atmpS1167 = _M0L6_2atmpS1168 + _M0L8trailingS106;
  _M0L6_2atmpS1166 = _M0L6_2atmpS1167 - 56320;
  _M0L6_2atmpS1165 = _M0L6_2atmpS1166 + 65536;
  return _M0L6_2atmpS1165;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS98,
  int32_t _M0L13start__offsetS99,
  int64_t _M0L11end__offsetS96
) {
  int32_t _M0L11end__offsetS95;
  int32_t _if__result_2567;
  #line 60 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS96 == 4294967296ll) {
    _M0L11end__offsetS95 = Moonbit_array_length(_M0L4selfS98);
  } else {
    int64_t _M0L7_2aSomeS97 = _M0L11end__offsetS96;
    _M0L11end__offsetS95 = (int32_t)_M0L7_2aSomeS97;
  }
  if (_M0L13start__offsetS99 >= 0) {
    if (_M0L13start__offsetS99 <= _M0L11end__offsetS95) {
      int32_t _M0L6_2atmpS1158 = Moonbit_array_length(_M0L4selfS98);
      _if__result_2567 = _M0L11end__offsetS95 <= _M0L6_2atmpS1158;
    } else {
      _if__result_2567 = 0;
    }
  } else {
    _if__result_2567 = 0;
  }
  if (_if__result_2567) {
    int32_t _M0L12utf16__indexS100 = _M0L13start__offsetS99;
    int32_t _M0L11char__countS101 = 0;
    while (1) {
      if (_M0L12utf16__indexS100 < _M0L11end__offsetS95) {
        int32_t _M0L2c1S102 = _M0L4selfS98[_M0L12utf16__indexS100];
        int32_t _if__result_2569;
        int32_t _M0L6_2atmpS1163;
        int32_t _M0L6_2atmpS1164;
        #line 76 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S102)) {
          int32_t _M0L6_2atmpS1159 = _M0L12utf16__indexS100 + 1;
          _if__result_2569 = _M0L6_2atmpS1159 < _M0L11end__offsetS95;
        } else {
          _if__result_2569 = 0;
        }
        if (_if__result_2569) {
          int32_t _M0L6_2atmpS1162 = _M0L12utf16__indexS100 + 1;
          int32_t _M0L2c2S103 = _M0L4selfS98[_M0L6_2atmpS1162];
          #line 78 "/Users/user/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S103)) {
            int32_t _M0L6_2atmpS1160 = _M0L12utf16__indexS100 + 2;
            int32_t _M0L6_2atmpS1161 = _M0L11char__countS101 + 1;
            _M0L12utf16__indexS100 = _M0L6_2atmpS1160;
            _M0L11char__countS101 = _M0L6_2atmpS1161;
            continue;
          } else {
            #line 81 "/Users/user/.moon/lib/core/builtin/string.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_81.data);
          }
        }
        _M0L6_2atmpS1163 = _M0L12utf16__indexS100 + 1;
        _M0L6_2atmpS1164 = _M0L11char__countS101 + 1;
        _M0L12utf16__indexS100 = _M0L6_2atmpS1163;
        _M0L11char__countS101 = _M0L6_2atmpS1164;
        continue;
      } else {
        moonbit_decref(_M0L4selfS98);
        return _M0L11char__countS101;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS98);
    #line 70 "/Users/user/.moon/lib/core/builtin/string.mbt"
    return _M0FPC15abort5abortGiE((moonbit_string_t)moonbit_string_literal_104.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS94) {
  #line 45 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS94 >= 56320) {
    return _M0L4selfS94 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS93) {
  #line 28 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS93 >= 55296) {
    return _M0L4selfS93 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS74) {
  struct _M0TPB13StringBuilder* _M0L3bufS72;
  int32_t _M0L3lenS73;
  int32_t _M0L3remS75;
  int32_t _M0L1iS76;
  #line 61 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 63 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L3bufS72 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS73 = Moonbit_array_length(_M0L4dataS74);
  _M0L3remS75 = _M0L3lenS73 % 3;
  _M0L1iS76 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1110 = _M0L3lenS73 - _M0L3remS75;
    if (_M0L1iS76 < _M0L6_2atmpS1110) {
      int32_t _M0L6_2atmpS1132;
      int32_t _M0L2b0S77;
      int32_t _M0L6_2atmpS1131;
      int32_t _M0L6_2atmpS1130;
      int32_t _M0L2b1S78;
      int32_t _M0L6_2atmpS1129;
      int32_t _M0L6_2atmpS1128;
      int32_t _M0L2b2S79;
      int32_t _M0L6_2atmpS1127;
      int32_t _M0L6_2atmpS1126;
      int32_t _M0L2x0S80;
      int32_t _M0L6_2atmpS1125;
      int32_t _M0L6_2atmpS1122;
      int32_t _M0L6_2atmpS1124;
      int32_t _M0L6_2atmpS1123;
      int32_t _M0L6_2atmpS1121;
      int32_t _M0L2x1S81;
      int32_t _M0L6_2atmpS1120;
      int32_t _M0L6_2atmpS1117;
      int32_t _M0L6_2atmpS1119;
      int32_t _M0L6_2atmpS1118;
      int32_t _M0L6_2atmpS1116;
      int32_t _M0L2x2S82;
      int32_t _M0L6_2atmpS1115;
      int32_t _M0L2x3S83;
      int32_t _M0L6_2atmpS1111;
      int32_t _M0L6_2atmpS1112;
      int32_t _M0L6_2atmpS1113;
      int32_t _M0L6_2atmpS1114;
      int32_t _M0L6_2atmpS1133;
      if (_M0L1iS76 < 0 || _M0L1iS76 >= Moonbit_array_length(_M0L4dataS74)) {
        #line 67 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1132 = (int32_t)_M0L4dataS74[_M0L1iS76];
      _M0L2b0S77 = (int32_t)_M0L6_2atmpS1132;
      _M0L6_2atmpS1131 = _M0L1iS76 + 1;
      if (
        _M0L6_2atmpS1131 < 0
        || _M0L6_2atmpS1131 >= Moonbit_array_length(_M0L4dataS74)
      ) {
        #line 68 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1130 = (int32_t)_M0L4dataS74[_M0L6_2atmpS1131];
      _M0L2b1S78 = (int32_t)_M0L6_2atmpS1130;
      _M0L6_2atmpS1129 = _M0L1iS76 + 2;
      if (
        _M0L6_2atmpS1129 < 0
        || _M0L6_2atmpS1129 >= Moonbit_array_length(_M0L4dataS74)
      ) {
        #line 69 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1128 = (int32_t)_M0L4dataS74[_M0L6_2atmpS1129];
      _M0L2b2S79 = (int32_t)_M0L6_2atmpS1128;
      _M0L6_2atmpS1127 = _M0L2b0S77 & 252;
      _M0L6_2atmpS1126 = _M0L6_2atmpS1127 >> 2;
      if (
        _M0L6_2atmpS1126 < 0
        || _M0L6_2atmpS1126
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 70 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S80 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1126];
      _M0L6_2atmpS1125 = _M0L2b0S77 & 3;
      _M0L6_2atmpS1122 = _M0L6_2atmpS1125 << 4;
      _M0L6_2atmpS1124 = _M0L2b1S78 & 240;
      _M0L6_2atmpS1123 = _M0L6_2atmpS1124 >> 4;
      _M0L6_2atmpS1121 = _M0L6_2atmpS1122 | _M0L6_2atmpS1123;
      if (
        _M0L6_2atmpS1121 < 0
        || _M0L6_2atmpS1121
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 71 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S81 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1121];
      _M0L6_2atmpS1120 = _M0L2b1S78 & 15;
      _M0L6_2atmpS1117 = _M0L6_2atmpS1120 << 2;
      _M0L6_2atmpS1119 = _M0L2b2S79 & 192;
      _M0L6_2atmpS1118 = _M0L6_2atmpS1119 >> 6;
      _M0L6_2atmpS1116 = _M0L6_2atmpS1117 | _M0L6_2atmpS1118;
      if (
        _M0L6_2atmpS1116 < 0
        || _M0L6_2atmpS1116
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 72 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S82 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1116];
      _M0L6_2atmpS1115 = _M0L2b2S79 & 63;
      if (
        _M0L6_2atmpS1115 < 0
        || _M0L6_2atmpS1115
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 73 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S83 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1115];
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1111 = _M0MPC14byte4Byte8to__char(_M0L2x0S80);
      moonbit_incref(_M0L3bufS72);
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1111);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1112 = _M0MPC14byte4Byte8to__char(_M0L2x1S81);
      moonbit_incref(_M0L3bufS72);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1112);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1113 = _M0MPC14byte4Byte8to__char(_M0L2x2S82);
      moonbit_incref(_M0L3bufS72);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1113);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1114 = _M0MPC14byte4Byte8to__char(_M0L2x3S83);
      moonbit_incref(_M0L3bufS72);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1114);
      _M0L6_2atmpS1133 = _M0L1iS76 + 3;
      _M0L1iS76 = _M0L6_2atmpS1133;
      continue;
    }
    break;
  }
  if (_M0L3remS75 == 1) {
    int32_t _M0L6_2atmpS1141 = _M0L3lenS73 - 1;
    int32_t _M0L6_2atmpS1140;
    int32_t _M0L2b0S85;
    int32_t _M0L6_2atmpS1139;
    int32_t _M0L6_2atmpS1138;
    int32_t _M0L2x0S86;
    int32_t _M0L6_2atmpS1137;
    int32_t _M0L6_2atmpS1136;
    int32_t _M0L2x1S87;
    int32_t _M0L6_2atmpS1134;
    int32_t _M0L6_2atmpS1135;
    if (
      _M0L6_2atmpS1141 < 0
      || _M0L6_2atmpS1141 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 80 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1140 = (int32_t)_M0L4dataS74[_M0L6_2atmpS1141];
    moonbit_decref(_M0L4dataS74);
    _M0L2b0S85 = (int32_t)_M0L6_2atmpS1140;
    _M0L6_2atmpS1139 = _M0L2b0S85 & 252;
    _M0L6_2atmpS1138 = _M0L6_2atmpS1139 >> 2;
    if (
      _M0L6_2atmpS1138 < 0
      || _M0L6_2atmpS1138
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 81 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S86 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1138];
    _M0L6_2atmpS1137 = _M0L2b0S85 & 3;
    _M0L6_2atmpS1136 = _M0L6_2atmpS1137 << 4;
    if (
      _M0L6_2atmpS1136 < 0
      || _M0L6_2atmpS1136
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 82 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S87 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1136];
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1134 = _M0MPC14byte4Byte8to__char(_M0L2x0S86);
    moonbit_incref(_M0L3bufS72);
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1134);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1135 = _M0MPC14byte4Byte8to__char(_M0L2x1S87);
    moonbit_incref(_M0L3bufS72);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1135);
    moonbit_incref(_M0L3bufS72);
    #line 85 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
    moonbit_incref(_M0L3bufS72);
    #line 86 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
  } else if (_M0L3remS75 == 2) {
    int32_t _M0L6_2atmpS1157 = _M0L3lenS73 - 2;
    int32_t _M0L6_2atmpS1156;
    int32_t _M0L2b0S88;
    int32_t _M0L6_2atmpS1155;
    int32_t _M0L6_2atmpS1154;
    int32_t _M0L2b1S89;
    int32_t _M0L6_2atmpS1153;
    int32_t _M0L6_2atmpS1152;
    int32_t _M0L2x0S90;
    int32_t _M0L6_2atmpS1151;
    int32_t _M0L6_2atmpS1148;
    int32_t _M0L6_2atmpS1150;
    int32_t _M0L6_2atmpS1149;
    int32_t _M0L6_2atmpS1147;
    int32_t _M0L2x1S91;
    int32_t _M0L6_2atmpS1146;
    int32_t _M0L6_2atmpS1145;
    int32_t _M0L2x2S92;
    int32_t _M0L6_2atmpS1142;
    int32_t _M0L6_2atmpS1143;
    int32_t _M0L6_2atmpS1144;
    if (
      _M0L6_2atmpS1157 < 0
      || _M0L6_2atmpS1157 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 88 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1156 = (int32_t)_M0L4dataS74[_M0L6_2atmpS1157];
    _M0L2b0S88 = (int32_t)_M0L6_2atmpS1156;
    _M0L6_2atmpS1155 = _M0L3lenS73 - 1;
    if (
      _M0L6_2atmpS1155 < 0
      || _M0L6_2atmpS1155 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 89 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1154 = (int32_t)_M0L4dataS74[_M0L6_2atmpS1155];
    moonbit_decref(_M0L4dataS74);
    _M0L2b1S89 = (int32_t)_M0L6_2atmpS1154;
    _M0L6_2atmpS1153 = _M0L2b0S88 & 252;
    _M0L6_2atmpS1152 = _M0L6_2atmpS1153 >> 2;
    if (
      _M0L6_2atmpS1152 < 0
      || _M0L6_2atmpS1152
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 90 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S90 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1152];
    _M0L6_2atmpS1151 = _M0L2b0S88 & 3;
    _M0L6_2atmpS1148 = _M0L6_2atmpS1151 << 4;
    _M0L6_2atmpS1150 = _M0L2b1S89 & 240;
    _M0L6_2atmpS1149 = _M0L6_2atmpS1150 >> 4;
    _M0L6_2atmpS1147 = _M0L6_2atmpS1148 | _M0L6_2atmpS1149;
    if (
      _M0L6_2atmpS1147 < 0
      || _M0L6_2atmpS1147
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 91 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S91 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1147];
    _M0L6_2atmpS1146 = _M0L2b1S89 & 15;
    _M0L6_2atmpS1145 = _M0L6_2atmpS1146 << 2;
    if (
      _M0L6_2atmpS1145 < 0
      || _M0L6_2atmpS1145
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 92 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S92 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1145];
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1142 = _M0MPC14byte4Byte8to__char(_M0L2x0S90);
    moonbit_incref(_M0L3bufS72);
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1142);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1143 = _M0MPC14byte4Byte8to__char(_M0L2x1S91);
    moonbit_incref(_M0L3bufS72);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1143);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1144 = _M0MPC14byte4Byte8to__char(_M0L2x2S92);
    moonbit_incref(_M0L3bufS72);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS1144);
    moonbit_incref(_M0L3bufS72);
    #line 96 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
  } else {
    moonbit_decref(_M0L4dataS74);
  }
  #line 98 "/Users/user/.moon/lib/core/builtin/console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS72);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS70,
  int32_t _M0L2chS69
) {
  uint32_t _M0L4codeS68;
  #line 90 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 91 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS68 = _M0MPC14char4Char8to__uint(_M0L2chS69);
  if (_M0L4codeS68 <= 65535u) {
    int32_t _M0L3lenS1089 = _M0L4selfS70->$1;
    int32_t _M0L6_2atmpS1088 = _M0L3lenS1089 + 1;
    uint16_t* _M0L4dataS1090;
    int32_t _M0L3lenS1091;
    int32_t _M0L6_2atmpS1092;
    int32_t _M0L3lenS1094;
    int32_t _M0L6_2atmpS1093;
    moonbit_incref(_M0L4selfS70);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS70, _M0L6_2atmpS1088);
    _M0L4dataS1090 = _M0L4selfS70->$0;
    _M0L3lenS1091 = _M0L4selfS70->$1;
    moonbit_incref(_M0L4dataS1090);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1092 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS68);
    if (
      _M0L3lenS1091 < 0
      || _M0L3lenS1091 >= Moonbit_array_length(_M0L4dataS1090)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1090[_M0L3lenS1091] = _M0L6_2atmpS1092;
    moonbit_decref(_M0L4dataS1090);
    _M0L3lenS1094 = _M0L4selfS70->$1;
    _M0L6_2atmpS1093 = _M0L3lenS1094 + 1;
    _M0L4selfS70->$1 = _M0L6_2atmpS1093;
    moonbit_decref(_M0L4selfS70);
  } else if (_M0L4codeS68 <= 1114111u) {
    int32_t _M0L3lenS1096 = _M0L4selfS70->$1;
    int32_t _M0L6_2atmpS1095 = _M0L3lenS1096 + 2;
    uint32_t _M0L4codeS71;
    uint16_t* _M0L4dataS1097;
    int32_t _M0L3lenS1098;
    uint32_t _M0L6_2atmpS1101;
    uint32_t _M0L6_2atmpS1100;
    int32_t _M0L6_2atmpS1099;
    uint16_t* _M0L4dataS1102;
    int32_t _M0L3lenS1107;
    int32_t _M0L6_2atmpS1103;
    uint32_t _M0L6_2atmpS1106;
    uint32_t _M0L6_2atmpS1105;
    int32_t _M0L6_2atmpS1104;
    int32_t _M0L3lenS1109;
    int32_t _M0L6_2atmpS1108;
    moonbit_incref(_M0L4selfS70);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS70, _M0L6_2atmpS1095);
    _M0L4codeS71 = _M0L4codeS68 - 65536u;
    _M0L4dataS1097 = _M0L4selfS70->$0;
    _M0L3lenS1098 = _M0L4selfS70->$1;
    _M0L6_2atmpS1101 = _M0L4codeS71 >> 10;
    _M0L6_2atmpS1100 = 55296u + _M0L6_2atmpS1101;
    moonbit_incref(_M0L4dataS1097);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1099 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1100);
    if (
      _M0L3lenS1098 < 0
      || _M0L3lenS1098 >= Moonbit_array_length(_M0L4dataS1097)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1097[_M0L3lenS1098] = _M0L6_2atmpS1099;
    moonbit_decref(_M0L4dataS1097);
    _M0L4dataS1102 = _M0L4selfS70->$0;
    _M0L3lenS1107 = _M0L4selfS70->$1;
    _M0L6_2atmpS1103 = _M0L3lenS1107 + 1;
    _M0L6_2atmpS1106 = _M0L4codeS71 & 1023u;
    _M0L6_2atmpS1105 = 56320u + _M0L6_2atmpS1106;
    moonbit_incref(_M0L4dataS1102);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1104 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1105);
    if (
      _M0L6_2atmpS1103 < 0
      || _M0L6_2atmpS1103 >= Moonbit_array_length(_M0L4dataS1102)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1102[_M0L6_2atmpS1103] = _M0L6_2atmpS1104;
    moonbit_decref(_M0L4dataS1102);
    _M0L3lenS1109 = _M0L4selfS70->$1;
    _M0L6_2atmpS1108 = _M0L3lenS1109 + 2;
    _M0L4selfS70->$1 = _M0L6_2atmpS1108;
    moonbit_decref(_M0L4selfS70);
  } else {
    moonbit_decref(_M0L4selfS70);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_105.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS62,
  int32_t _M0L8requiredS63
) {
  uint16_t* _M0L4dataS1087;
  int32_t _M0L12current__lenS61;
  int32_t _M0L13enough__spaceS64;
  int32_t _M0L13enough__spaceS65;
  int32_t _M0L6_2atmpS1085;
  uint16_t* _M0L9new__dataS67;
  uint16_t* _M0L4dataS1083;
  int32_t _M0L3lenS1084;
  uint16_t* _M0L6_2aoldS2317;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS1087 = _M0L4selfS62->$0;
  _M0L12current__lenS61 = Moonbit_array_length(_M0L4dataS1087);
  if (_M0L8requiredS63 <= _M0L12current__lenS61) {
    moonbit_decref(_M0L4selfS62);
    return 0;
  }
  _M0L13enough__spaceS65 = _M0L12current__lenS61;
  while (1) {
    if (_M0L13enough__spaceS65 < _M0L8requiredS63) {
      int32_t _M0L6_2atmpS1086 = _M0L13enough__spaceS65 * 2;
      _M0L13enough__spaceS65 = _M0L6_2atmpS1086;
      continue;
    } else {
      _M0L13enough__spaceS64 = _M0L13enough__spaceS65;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1085 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS67
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS64, _M0L6_2atmpS1085);
  _M0L4dataS1083 = _M0L4selfS62->$0;
  _M0L3lenS1084 = _M0L4selfS62->$1;
  moonbit_incref(_M0L4dataS1083);
  moonbit_incref(_M0L9new__dataS67);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS67, 0, _M0L4dataS1083, 0, _M0L3lenS1084);
  _M0L6_2aoldS2317 = _M0L4selfS62->$0;
  moonbit_decref(_M0L6_2aoldS2317);
  _M0L4selfS62->$0 = _M0L9new__dataS67;
  moonbit_decref(_M0L4selfS62);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS60) {
  int32_t _M0L6_2atmpS1082;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1082 = *(int32_t*)&_M0L4selfS60;
  return (uint16_t)_M0L6_2atmpS1082;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS1081;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1081 = _M0L4selfS59;
  return *(uint32_t*)&_M0L6_2atmpS1081;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS57
) {
  int32_t _M0L3lenS1073;
  uint16_t* _M0L4dataS1075;
  int32_t _M0L6_2atmpS1074;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1073 = _M0L4selfS57->$1;
  _M0L4dataS1075 = _M0L4selfS57->$0;
  _M0L6_2atmpS1074 = Moonbit_array_length(_M0L4dataS1075);
  if (_M0L3lenS1073 == _M0L6_2atmpS1074) {
    uint16_t* _M0L8_2afieldS2320 = _M0L4selfS57->$0;
    int32_t _M0L6_2acntS2409 = Moonbit_object_header(_M0L4selfS57)->rc;
    uint16_t* _M0L4dataS1076;
    if (_M0L6_2acntS2409 > 1) {
      int32_t _M0L11_2anew__cntS2410 = _M0L6_2acntS2409 - 1;
      Moonbit_object_header(_M0L4selfS57)->rc = _M0L11_2anew__cntS2410;
      moonbit_incref(_M0L8_2afieldS2320);
    } else if (_M0L6_2acntS2409 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS57);
    }
    _M0L4dataS1076 = _M0L8_2afieldS2320;
    return _M0L4dataS1076;
  } else {
    int32_t _M0L3lenS1079 = _M0L4selfS57->$1;
    int32_t _M0L6_2atmpS1080;
    uint16_t* _M0L4dataS58;
    uint16_t* _M0L4dataS1077;
    int32_t _M0L3lenS1078;
    int32_t _M0L6_2acntS2411;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1080 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS58
    = (uint16_t*)moonbit_make_string(_M0L3lenS1079, _M0L6_2atmpS1080);
    _M0L4dataS1077 = _M0L4selfS57->$0;
    _M0L3lenS1078 = _M0L4selfS57->$1;
    _M0L6_2acntS2411 = Moonbit_object_header(_M0L4selfS57)->rc;
    if (_M0L6_2acntS2411 > 1) {
      int32_t _M0L11_2anew__cntS2412 = _M0L6_2acntS2411 - 1;
      Moonbit_object_header(_M0L4selfS57)->rc = _M0L11_2anew__cntS2412;
      moonbit_incref(_M0L4dataS1077);
    } else if (_M0L6_2acntS2411 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS57);
    }
    moonbit_incref(_M0L4dataS58);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS58, 0, _M0L4dataS1077, 0, _M0L3lenS1078);
    return _M0L4dataS58;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 153 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS55
) {
  int32_t _M0L7initialS54;
  uint16_t* _M0L4dataS56;
  struct _M0TPB13StringBuilder* _block_2572;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS55 < 1) {
    _M0L7initialS54 = 1;
  } else {
    int32_t _M0L6_2atmpS1072 = _M0L10size__hintS55 + 1;
    _M0L7initialS54 = _M0L6_2atmpS1072 / 2;
  }
  _M0L4dataS56 = (uint16_t*)moonbit_make_string(_M0L7initialS54, 0);
  _block_2572
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2572)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2572->$0 = _M0L4dataS56;
  _block_2572->$1 = 0;
  return _block_2572;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS53) {
  int32_t _M0L6_2atmpS1071;
  #line 1867 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1071 = (int32_t)_M0L4selfS53;
  return _M0L6_2atmpS1071;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS43,
  int32_t _M0L11dst__offsetS44,
  moonbit_string_t* _M0L3srcS45,
  int32_t _M0L11src__offsetS46,
  int32_t _M0L3lenS47
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS43, _M0L11dst__offsetS44, _M0L3srcS45, _M0L11src__offsetS46, _M0L3lenS47);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS48,
  int32_t _M0L11dst__offsetS49,
  struct _M0TUsiE** _M0L3srcS50,
  int32_t _M0L11src__offsetS51,
  int32_t _M0L3lenS52
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS48, _M0L11dst__offsetS49, _M0L3srcS50, _M0L11src__offsetS51, _M0L3lenS52);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS16,
  int32_t _M0L11dst__offsetS18,
  uint16_t* _M0L3srcS17,
  int32_t _M0L11src__offsetS19,
  int32_t _M0L3lenS21
) {
  int32_t _if__result_2573;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS16 == _M0L3srcS17) {
    _if__result_2573 = _M0L11dst__offsetS18 < _M0L11src__offsetS19;
  } else {
    _if__result_2573 = 0;
  }
  if (_if__result_2573) {
    int32_t _M0L1iS20 = 0;
    while (1) {
      if (_M0L1iS20 < _M0L3lenS21) {
        int32_t _M0L6_2atmpS1044 = _M0L11dst__offsetS18 + _M0L1iS20;
        int32_t _M0L6_2atmpS1046 = _M0L11src__offsetS19 + _M0L1iS20;
        int32_t _M0L6_2atmpS1045;
        int32_t _M0L6_2atmpS1047;
        if (
          _M0L6_2atmpS1046 < 0
          || _M0L6_2atmpS1046 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1045 = (int32_t)_M0L3srcS17[_M0L6_2atmpS1046];
        if (
          _M0L6_2atmpS1044 < 0
          || _M0L6_2atmpS1044 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS1044] = _M0L6_2atmpS1045;
        _M0L6_2atmpS1047 = _M0L1iS20 + 1;
        _M0L1iS20 = _M0L6_2atmpS1047;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1052 = _M0L3lenS21 - 1;
    int32_t _M0L1iS23 = _M0L6_2atmpS1052;
    while (1) {
      if (_M0L1iS23 >= 0) {
        int32_t _M0L6_2atmpS1048 = _M0L11dst__offsetS18 + _M0L1iS23;
        int32_t _M0L6_2atmpS1050 = _M0L11src__offsetS19 + _M0L1iS23;
        int32_t _M0L6_2atmpS1049;
        int32_t _M0L6_2atmpS1051;
        if (
          _M0L6_2atmpS1050 < 0
          || _M0L6_2atmpS1050 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1049 = (int32_t)_M0L3srcS17[_M0L6_2atmpS1050];
        if (
          _M0L6_2atmpS1048 < 0
          || _M0L6_2atmpS1048 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS1048] = _M0L6_2atmpS1049;
        _M0L6_2atmpS1051 = _M0L1iS23 - 1;
        _M0L1iS23 = _M0L6_2atmpS1051;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS25,
  int32_t _M0L11dst__offsetS27,
  moonbit_string_t* _M0L3srcS26,
  int32_t _M0L11src__offsetS28,
  int32_t _M0L3lenS30
) {
  int32_t _if__result_2576;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS25 == _M0L3srcS26) {
    _if__result_2576 = _M0L11dst__offsetS27 < _M0L11src__offsetS28;
  } else {
    _if__result_2576 = 0;
  }
  if (_if__result_2576) {
    int32_t _M0L1iS29 = 0;
    while (1) {
      if (_M0L1iS29 < _M0L3lenS30) {
        int32_t _M0L6_2atmpS1053 = _M0L11dst__offsetS27 + _M0L1iS29;
        int32_t _M0L6_2atmpS1055 = _M0L11src__offsetS28 + _M0L1iS29;
        moonbit_string_t _M0L6_2atmpS1054;
        moonbit_string_t _M0L6_2aoldS2323;
        int32_t _M0L6_2atmpS1056;
        if (
          _M0L6_2atmpS1055 < 0
          || _M0L6_2atmpS1055 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1054 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS1055];
        if (
          _M0L6_2atmpS1053 < 0
          || _M0L6_2atmpS1053 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2323 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS1053];
        moonbit_incref(_M0L6_2atmpS1054);
        moonbit_decref(_M0L6_2aoldS2323);
        _M0L3dstS25[_M0L6_2atmpS1053] = _M0L6_2atmpS1054;
        _M0L6_2atmpS1056 = _M0L1iS29 + 1;
        _M0L1iS29 = _M0L6_2atmpS1056;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1061 = _M0L3lenS30 - 1;
    int32_t _M0L1iS32 = _M0L6_2atmpS1061;
    while (1) {
      if (_M0L1iS32 >= 0) {
        int32_t _M0L6_2atmpS1057 = _M0L11dst__offsetS27 + _M0L1iS32;
        int32_t _M0L6_2atmpS1059 = _M0L11src__offsetS28 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS1058;
        moonbit_string_t _M0L6_2aoldS2325;
        int32_t _M0L6_2atmpS1060;
        if (
          _M0L6_2atmpS1059 < 0
          || _M0L6_2atmpS1059 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1058 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS1059];
        if (
          _M0L6_2atmpS1057 < 0
          || _M0L6_2atmpS1057 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2325 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS1057];
        moonbit_incref(_M0L6_2atmpS1058);
        moonbit_decref(_M0L6_2aoldS2325);
        _M0L3dstS25[_M0L6_2atmpS1057] = _M0L6_2atmpS1058;
        _M0L6_2atmpS1060 = _M0L1iS32 - 1;
        _M0L1iS32 = _M0L6_2atmpS1060;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS34,
  int32_t _M0L11dst__offsetS36,
  struct _M0TUsiE** _M0L3srcS35,
  int32_t _M0L11src__offsetS37,
  int32_t _M0L3lenS39
) {
  int32_t _if__result_2579;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS34 == _M0L3srcS35) {
    _if__result_2579 = _M0L11dst__offsetS36 < _M0L11src__offsetS37;
  } else {
    _if__result_2579 = 0;
  }
  if (_if__result_2579) {
    int32_t _M0L1iS38 = 0;
    while (1) {
      if (_M0L1iS38 < _M0L3lenS39) {
        int32_t _M0L6_2atmpS1062 = _M0L11dst__offsetS36 + _M0L1iS38;
        int32_t _M0L6_2atmpS1064 = _M0L11src__offsetS37 + _M0L1iS38;
        struct _M0TUsiE* _M0L6_2atmpS1063;
        struct _M0TUsiE* _M0L6_2aoldS2327;
        int32_t _M0L6_2atmpS1065;
        if (
          _M0L6_2atmpS1064 < 0
          || _M0L6_2atmpS1064 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1063 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS1064];
        if (
          _M0L6_2atmpS1062 < 0
          || _M0L6_2atmpS1062 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2327 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS1062];
        if (_M0L6_2atmpS1063) {
          moonbit_incref(_M0L6_2atmpS1063);
        }
        if (_M0L6_2aoldS2327) {
          moonbit_decref(_M0L6_2aoldS2327);
        }
        _M0L3dstS34[_M0L6_2atmpS1062] = _M0L6_2atmpS1063;
        _M0L6_2atmpS1065 = _M0L1iS38 + 1;
        _M0L1iS38 = _M0L6_2atmpS1065;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1070 = _M0L3lenS39 - 1;
    int32_t _M0L1iS41 = _M0L6_2atmpS1070;
    while (1) {
      if (_M0L1iS41 >= 0) {
        int32_t _M0L6_2atmpS1066 = _M0L11dst__offsetS36 + _M0L1iS41;
        int32_t _M0L6_2atmpS1068 = _M0L11src__offsetS37 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS1067;
        struct _M0TUsiE* _M0L6_2aoldS2329;
        int32_t _M0L6_2atmpS1069;
        if (
          _M0L6_2atmpS1068 < 0
          || _M0L6_2atmpS1068 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1067 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS1068];
        if (
          _M0L6_2atmpS1066 < 0
          || _M0L6_2atmpS1066 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2329 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS1066];
        if (_M0L6_2atmpS1067) {
          moonbit_incref(_M0L6_2atmpS1067);
        }
        if (_M0L6_2aoldS2329) {
          moonbit_decref(_M0L6_2aoldS2329);
        }
        _M0L3dstS34[_M0L6_2atmpS1066] = _M0L6_2atmpS1067;
        _M0L6_2atmpS1069 = _M0L1iS41 - 1;
        _M0L1iS41 = _M0L6_2atmpS1069;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5valueS15
) {
  uint32_t _M0L3accS1043;
  uint32_t _M0L6_2atmpS1042;
  #line 236 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1043 = _M0L4selfS14->$0;
  _M0L6_2atmpS1042 = _M0L3accS1043 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1042;
  #line 238 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS1040;
  uint32_t _M0L6_2atmpS1041;
  uint32_t _M0L6_2atmpS1039;
  uint32_t _M0L6_2atmpS1038;
  uint32_t _M0L6_2atmpS1037;
  #line 451 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1040 = _M0L4selfS12->$0;
  _M0L6_2atmpS1041 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS1039 = _M0L3accS1040 + _M0L6_2atmpS1041;
  #line 452 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1038 = _M0FPB4rotl(_M0L6_2atmpS1039, 17);
  _M0L6_2atmpS1037 = _M0L6_2atmpS1038 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS1037;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS1034;
  int32_t _M0L6_2atmpS1036;
  uint32_t _M0L6_2atmpS1035;
  #line 461 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1034 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS1036 = 32 - _M0L1rS11;
  _M0L6_2atmpS1035 = _M0L1xS10 >> (_M0L6_2atmpS1036 & 31);
  return _M0L6_2atmpS1034 | _M0L6_2atmpS1035;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__5208S6,
  struct _M0TPB6Logger _M0L10_2ax__5209S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS2331;
  int32_t _M0L6_2acntS2413;
  moonbit_string_t _M0L15_2a_2aarg__5210S8;
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__5208S6;
  _M0L8_2afieldS2331 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS2413 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS2413 > 1) {
    int32_t _M0L11_2anew__cntS2414 = _M0L6_2acntS2413 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS2414;
    moonbit_incref(_M0L8_2afieldS2331);
  } else if (_M0L6_2acntS2413 == 1) {
    #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__5210S8 = _M0L8_2afieldS2331;
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_106.data);
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__5209S9, _M0L15_2a_2aarg__5210S8);
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_107.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS5,
  moonbit_string_t _M0L3objS4
) {
  #line 155 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 156 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS4, _M0L4selfS5);
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

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS2) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS2);
  moonbit_decref(_M0L3msgS2);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "/Users/user/.moon/lib/core/abort/abort.mbt"
  #line 49 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "/Users/user/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS966) {
  switch (Moonbit_object_tag(_M0L4_2aeS966)) {
    case 1: {
      moonbit_decref(_M0L4_2aeS966);
      return (moonbit_string_t)moonbit_string_literal_108.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS966);
      return (moonbit_string_t)moonbit_string_literal_109.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS966);
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS966);
      return (moonbit_string_t)moonbit_string_literal_110.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS966);
      return (moonbit_string_t)moonbit_string_literal_111.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1000
) {
  moonbit_string_t _M0L7_2aselfS999 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1000;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS999);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS998,
  struct _M0TPB6Logger _M0L8_2aparamS997
) {
  moonbit_string_t _M0L7_2aselfS996 = (moonbit_string_t)_M0L11_2aobj__ptrS998;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS996, _M0L8_2aparamS997);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS994
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS995 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS994;
  int32_t _M0L7_2aselfS993 = _M0L14_2aboxed__selfS995->$0;
  moonbit_decref(_M0L14_2aboxed__selfS995);
  return _M0IPC14bool4BoolPB4Show10to__string(_M0L7_2aselfS993);
}

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS991,
  struct _M0TPB6Logger _M0L8_2aparamS990
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS992 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS991;
  int32_t _M0L7_2aselfS989 = _M0L14_2aboxed__selfS992->$0;
  moonbit_decref(_M0L14_2aboxed__selfS992);
  _M0IPC14bool4BoolPB4Show6output(_M0L7_2aselfS989, _M0L8_2aparamS990);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS988,
  int32_t _M0L8_2aparamS987
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS986 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS988;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS986, _M0L8_2aparamS987);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS985,
  struct _M0TPC16string10StringView _M0L8_2aparamS984
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS983 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS985;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS983, _M0L8_2aparamS984);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS982,
  moonbit_string_t _M0L8_2aparamS979,
  int32_t _M0L8_2aparamS980,
  int32_t _M0L8_2aparamS981
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS978 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS982;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS978, _M0L8_2aparamS979, _M0L8_2aparamS980, _M0L8_2aparamS981);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS977,
  moonbit_string_t _M0L8_2aparamS976
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS975 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS977;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS975, _M0L8_2aparamS976);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGRPB5ArrayGsEE(
  void* _M0L11_2aobj__ptrS974
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS973 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS974;
  return _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(_M0L7_2aselfS973);
}

int32_t _M0IPC15array5ArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGsE(
  void* _M0L11_2aobj__ptrS972,
  struct _M0TPB6Logger _M0L8_2aparamS971
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS970 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS972;
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L7_2aselfS970, _M0L8_2aparamS971);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1033 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1032;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1016;
  moonbit_string_t* _M0L6_2atmpS1031;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1030;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1017;
  moonbit_string_t* _M0L6_2atmpS1029;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1028;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1018;
  moonbit_string_t* _M0L6_2atmpS1027;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1026;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1019;
  moonbit_string_t* _M0L6_2atmpS1025;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1024;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1020;
  moonbit_string_t* _M0L6_2atmpS1023;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1022;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1021;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS892;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1015;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1014;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1013;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1008;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS893;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1012;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1011;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1010;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1009;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS891;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1007;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1006;
  _M0L6_2atmpS1033[0] = (moonbit_string_t)moonbit_string_literal_112.data;
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1032
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1032)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1032->$0
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1032->$1 = _M0L6_2atmpS1033;
  _M0L8_2atupleS1016
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1016)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1016->$0 = 0;
  _M0L8_2atupleS1016->$1 = _M0L8_2atupleS1032;
  _M0L6_2atmpS1031 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1031[0] = (moonbit_string_t)moonbit_string_literal_113.data;
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1030
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1030)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1030->$0
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1030->$1 = _M0L6_2atmpS1031;
  _M0L8_2atupleS1017
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1017)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1017->$0 = 1;
  _M0L8_2atupleS1017->$1 = _M0L8_2atupleS1030;
  _M0L6_2atmpS1029 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1029[0] = (moonbit_string_t)moonbit_string_literal_114.data;
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1028
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1028)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1028->$0
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1028->$1 = _M0L6_2atmpS1029;
  _M0L8_2atupleS1018
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1018)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1018->$0 = 2;
  _M0L8_2atupleS1018->$1 = _M0L8_2atupleS1028;
  _M0L6_2atmpS1027 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1027[0] = (moonbit_string_t)moonbit_string_literal_115.data;
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1026
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1026)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1026->$0
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1026->$1 = _M0L6_2atmpS1027;
  _M0L8_2atupleS1019
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1019)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1019->$0 = 3;
  _M0L8_2atupleS1019->$1 = _M0L8_2atupleS1026;
  _M0L6_2atmpS1025 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1025[0] = (moonbit_string_t)moonbit_string_literal_116.data;
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__4_2eclo);
  _M0L8_2atupleS1024
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1024)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1024->$0
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__4_2eclo;
  _M0L8_2atupleS1024->$1 = _M0L6_2atmpS1025;
  _M0L8_2atupleS1020
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1020)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1020->$0 = 4;
  _M0L8_2atupleS1020->$1 = _M0L8_2atupleS1024;
  _M0L6_2atmpS1023 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1023[0] = (moonbit_string_t)moonbit_string_literal_117.data;
  moonbit_incref(_M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__5_2eclo);
  _M0L8_2atupleS1022
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1022)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1022->$0
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test51____test__72756e74696d655f746573742e6d6274__5_2eclo;
  _M0L8_2atupleS1022->$1 = _M0L6_2atmpS1023;
  _M0L8_2atupleS1021
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1021)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1021->$0 = 5;
  _M0L8_2atupleS1021->$1 = _M0L8_2atupleS1022;
  _M0L7_2abindS892
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(6);
  _M0L7_2abindS892[0] = _M0L8_2atupleS1016;
  _M0L7_2abindS892[1] = _M0L8_2atupleS1017;
  _M0L7_2abindS892[2] = _M0L8_2atupleS1018;
  _M0L7_2abindS892[3] = _M0L8_2atupleS1019;
  _M0L7_2abindS892[4] = _M0L8_2atupleS1020;
  _M0L7_2abindS892[5] = _M0L8_2atupleS1021;
  _M0L6_2atmpS1015 = _M0L7_2abindS892;
  _M0L6_2atmpS1014
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 6, _M0L6_2atmpS1015
  };
  #line 398 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1013
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1014);
  _M0L8_2atupleS1008
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1008)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1008->$0 = (moonbit_string_t)moonbit_string_literal_118.data;
  _M0L8_2atupleS1008->$1 = _M0L6_2atmpS1013;
  _M0L7_2abindS893
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1012 = _M0L7_2abindS893;
  _M0L6_2atmpS1011
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1012
  };
  #line 406 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1010
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1011);
  _M0L8_2atupleS1009
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1009)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1009->$0 = (moonbit_string_t)moonbit_string_literal_119.data;
  _M0L8_2atupleS1009->$1 = _M0L6_2atmpS1010;
  _M0L7_2abindS891
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS891[0] = _M0L8_2atupleS1008;
  _M0L7_2abindS891[1] = _M0L8_2atupleS1009;
  _M0L6_2atmpS1007 = _M0L7_2abindS891;
  _M0L6_2atmpS1006
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1007
  };
  #line 397 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp28cli__runtime__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1006);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1005;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS960;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS961;
  int32_t _M0L7_2abindS962;
  int32_t _M0L2__S963;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1005
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS960
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS960)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS960->$0 = _M0L6_2atmpS1005;
  _M0L12async__testsS960->$1 = 0;
  #line 445 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS961
  = _M0FP36mulpjs4mulp28cli__runtime__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS962 = _M0L7_2abindS961->$1;
  _M0L2__S963 = 0;
  while (1) {
    if (_M0L2__S963 < _M0L7_2abindS962) {
      struct _M0TUsiE** _M0L3bufS1004 = _M0L7_2abindS961->$0;
      struct _M0TUsiE* _M0L3argS964 =
        (struct _M0TUsiE*)_M0L3bufS1004[_M0L2__S963];
      moonbit_string_t _M0L6_2atmpS1001 = _M0L3argS964->$0;
      int32_t _M0L6_2atmpS1002 = _M0L3argS964->$1;
      int32_t _M0L6_2atmpS1003;
      moonbit_incref(_M0L6_2atmpS1001);
      moonbit_incref(_M0L12async__testsS960);
      #line 446 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
      _M0FP36mulpjs4mulp28cli__runtime__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS960, _M0L6_2atmpS1001, _M0L6_2atmpS1002);
      _M0L6_2atmpS1003 = _M0L2__S963 + 1;
      _M0L2__S963 = _M0L6_2atmpS1003;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS961);
    }
    break;
  }
  #line 448 "/Users/user/workspace/github/gulp/mulp/cli_runtime/__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP36mulpjs4mulp28cli__runtime__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp28cli__runtime__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS960);
  return 0;
}