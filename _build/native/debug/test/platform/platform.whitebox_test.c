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
struct _M0DTPC15error5Error94mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0KTPB4ShowS4Bool;

struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0Y4Bool;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp8platform33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp8platform33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWEuQRPC15error5Error;

struct _M0TURPB6LoggerRPC16string10StringViewE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB8MutLocalGiE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TP36mulpjs4mulp4core17CancellationToken;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0BTPB4Show;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TP36mulpjs4mulp8platform13SignalWatcher;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0DTPC15error5Error94mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

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

struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TPB13StringBuilder {
  int32_t $1;
  uint16_t* $0;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp8platform33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp8platform33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPB8MutLocalGiE* $1;
  
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

struct _M0TP36mulpjs4mulp4core17CancellationToken {
  int32_t $0;
  
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

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
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

struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TP36mulpjs4mulp8platform13SignalWatcher {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* $0;
  
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

struct moonbit_result_0 _M0FP36mulpjs4mulp8platform57____test__7369676e616c5f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN17error__to__stringS759(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN14handle__resultS750(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testC1708l425(
  struct _M0TWEu*
);

int32_t _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testC1704l426(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP36mulpjs4mulp8platform45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS684(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS679(
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S666(
  int32_t,
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP36mulpjs4mulp8platform28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp8platform34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp8platform47____test__7369676e616c5f7762746573742e6d6274__0(
  
);

int64_t _M0MP36mulpjs4mulp8platform13SignalWatcher4poll(
  struct _M0TP36mulpjs4mulp8platform13SignalWatcher*
);

#define _M0FP36mulpjs4mulp8platform22take__platform__signal mulp_take_signal

struct _M0TP36mulpjs4mulp8platform13SignalWatcher* _M0FP36mulpjs4mulp8platform15signal__watcher(
  struct _M0TP36mulpjs4mulp4core17CancellationToken*
);

int32_t _M0FP36mulpjs4mulp8platform35install__platform__signal__handlers();

int32_t _M0FP36mulpjs4mulp8platform34raise__platform__signal__for__test(
  int32_t
);

int32_t _M0FP36mulpjs4mulp8platform39raise__platform__signal__for__test__ffi(
  int32_t
);

int32_t _M0MP36mulpjs4mulp4core17CancellationToken6cancel(
  struct _M0TP36mulpjs4mulp4core17CancellationToken*
);

struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0FP36mulpjs4mulp4core24new__cancellation__token(
  
);

int32_t _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(
  struct _M0TP36mulpjs4mulp4core17CancellationToken*
);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

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

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1348l680(struct _M0TWEOs*);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t);

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

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
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

void mulp_raise_signal_for_test(int32_t);

void mulp_install_signal_handlers();

int32_t mulp_take_signal();

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_27 =
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
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 56, 
    58, 53, 52, 45, 49, 56, 58, 54, 48, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 57, 
    58, 52, 49, 45, 49, 57, 58, 52, 55, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    115, 105, 103, 110, 97, 108, 32, 119, 97, 116, 99, 104, 101, 114, 
    32, 116, 117, 114, 110, 115, 32, 105, 110, 116, 101, 114, 114, 117, 
    112, 116, 32, 105, 110, 116, 111, 32, 99, 97, 110, 99, 101, 108, 
    108, 97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 56, 
    58, 51, 45, 49, 56, 58, 54, 49, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 
    58, 51, 45, 50, 48, 58, 53, 48, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 
    58, 52, 51, 45, 50, 48, 58, 52, 57, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_23 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    115, 105, 103, 110, 97, 108, 95, 119, 98, 116, 101, 115, 116, 46, 
    109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    123, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 56, 
    58, 49, 49, 45, 49, 56, 58, 52, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 57, 
    58, 49, 49, 45, 49, 57, 58, 51, 49, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[89]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 88), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 112, 108, 
    97, 116, 102, 111, 114, 109, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 
    84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_47 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 57, 
    58, 51, 45, 49, 57, 58, 52, 56, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_25 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[87]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 86), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 112, 108, 
    97, 116, 102, 111, 114, 109, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 
    111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 
    114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 
    111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    112, 108, 97, 116, 102, 111, 114, 109, 47, 115, 105, 103, 110, 97, 
    108, 95, 119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 
    58, 49, 49, 45, 50, 48, 58, 51, 51, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 109, 117, 
    108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 112, 108, 97, 116, 
    102, 111, 114, 109, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 
    109, 101, 34, 58, 32, 0
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

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp8platform57____test__7369676e616c5f7762746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp8platform57____test__7369676e616c5f7762746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN17error__to__stringS759$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN17error__to__stringS759
  };

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp8platform53____test__7369676e616c5f7762746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp8platform57____test__7369676e616c5f7762746573742e6d6274__0_2edyncall$closure.data;

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

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1826 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP36mulpjs4mulp8platform48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP36mulpjs4mulp8platform57____test__7369676e616c5f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1739
) {
  return _M0FP36mulpjs4mulp8platform47____test__7369676e616c5f7762746573742e6d6274__0();
}

int32_t _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS780,
  moonbit_string_t _M0L8filenameS755,
  int32_t _M0L5indexS758
) {
  struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750* _closure_1928;
  struct _M0TWssbEu* _M0L14handle__resultS750;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS759;
  void* _M0L11_2atry__errS774;
  struct moonbit_result_0 _tmp_1930;
  int32_t _handle__error__result_1931;
  int32_t _M0L6_2atmpS1727;
  void* _M0L3errS775;
  moonbit_string_t _M0L4nameS777;
  struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS778;
  moonbit_string_t _M0L8_2afieldS1740;
  int32_t _M0L6_2acntS1868;
  moonbit_string_t _M0L7_2anameS779;
  #line 524 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_incref(_M0L8filenameS755);
  _closure_1928
  = (struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750*)moonbit_malloc(sizeof(struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750));
  Moonbit_object_header(_closure_1928)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750, $1) >> 2, 1, 0);
  _closure_1928->code
  = &_M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN14handle__resultS750;
  _closure_1928->$0 = _M0L5indexS758;
  _closure_1928->$1 = _M0L8filenameS755;
  _M0L14handle__resultS750 = (struct _M0TWssbEu*)_closure_1928;
  _M0L17error__to__stringS759
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN17error__to__stringS759$closure.data;
  moonbit_incref(_M0L12async__testsS780);
  moonbit_incref(_M0L17error__to__stringS759);
  moonbit_incref(_M0L8filenameS755);
  moonbit_incref(_M0L14handle__resultS750);
  #line 558 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _tmp_1930
  = _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__test(_M0L12async__testsS780, _M0L8filenameS755, _M0L5indexS758, _M0L14handle__resultS750, _M0L17error__to__stringS759);
  if (_tmp_1930.tag) {
    int32_t const _M0L5_2aokS1736 = _tmp_1930.data.ok;
    _handle__error__result_1931 = _M0L5_2aokS1736;
  } else {
    void* const _M0L6_2aerrS1737 = _tmp_1930.data.err;
    moonbit_decref(_M0L12async__testsS780);
    moonbit_decref(_M0L17error__to__stringS759);
    moonbit_decref(_M0L8filenameS755);
    _M0L11_2atry__errS774 = _M0L6_2aerrS1737;
    goto join_773;
  }
  if (_handle__error__result_1931) {
    moonbit_decref(_M0L12async__testsS780);
    moonbit_decref(_M0L17error__to__stringS759);
    moonbit_decref(_M0L8filenameS755);
    _M0L6_2atmpS1727 = 1;
  } else {
    struct moonbit_result_0 _tmp_1932;
    int32_t _handle__error__result_1933;
    moonbit_incref(_M0L12async__testsS780);
    moonbit_incref(_M0L17error__to__stringS759);
    moonbit_incref(_M0L8filenameS755);
    moonbit_incref(_M0L14handle__resultS750);
    #line 561 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    _tmp_1932
    = _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS780, _M0L8filenameS755, _M0L5indexS758, _M0L14handle__resultS750, _M0L17error__to__stringS759);
    if (_tmp_1932.tag) {
      int32_t const _M0L5_2aokS1734 = _tmp_1932.data.ok;
      _handle__error__result_1933 = _M0L5_2aokS1734;
    } else {
      void* const _M0L6_2aerrS1735 = _tmp_1932.data.err;
      moonbit_decref(_M0L12async__testsS780);
      moonbit_decref(_M0L17error__to__stringS759);
      moonbit_decref(_M0L8filenameS755);
      _M0L11_2atry__errS774 = _M0L6_2aerrS1735;
      goto join_773;
    }
    if (_handle__error__result_1933) {
      moonbit_decref(_M0L12async__testsS780);
      moonbit_decref(_M0L17error__to__stringS759);
      moonbit_decref(_M0L8filenameS755);
      _M0L6_2atmpS1727 = 1;
    } else {
      struct moonbit_result_0 _tmp_1934;
      int32_t _handle__error__result_1935;
      moonbit_incref(_M0L12async__testsS780);
      moonbit_incref(_M0L17error__to__stringS759);
      moonbit_incref(_M0L8filenameS755);
      moonbit_incref(_M0L14handle__resultS750);
      #line 564 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _tmp_1934
      = _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS780, _M0L8filenameS755, _M0L5indexS758, _M0L14handle__resultS750, _M0L17error__to__stringS759);
      if (_tmp_1934.tag) {
        int32_t const _M0L5_2aokS1732 = _tmp_1934.data.ok;
        _handle__error__result_1935 = _M0L5_2aokS1732;
      } else {
        void* const _M0L6_2aerrS1733 = _tmp_1934.data.err;
        moonbit_decref(_M0L12async__testsS780);
        moonbit_decref(_M0L17error__to__stringS759);
        moonbit_decref(_M0L8filenameS755);
        _M0L11_2atry__errS774 = _M0L6_2aerrS1733;
        goto join_773;
      }
      if (_handle__error__result_1935) {
        moonbit_decref(_M0L12async__testsS780);
        moonbit_decref(_M0L17error__to__stringS759);
        moonbit_decref(_M0L8filenameS755);
        _M0L6_2atmpS1727 = 1;
      } else {
        struct moonbit_result_0 _tmp_1936;
        int32_t _handle__error__result_1937;
        moonbit_incref(_M0L12async__testsS780);
        moonbit_incref(_M0L17error__to__stringS759);
        moonbit_incref(_M0L8filenameS755);
        moonbit_incref(_M0L14handle__resultS750);
        #line 567 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        _tmp_1936
        = _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS780, _M0L8filenameS755, _M0L5indexS758, _M0L14handle__resultS750, _M0L17error__to__stringS759);
        if (_tmp_1936.tag) {
          int32_t const _M0L5_2aokS1730 = _tmp_1936.data.ok;
          _handle__error__result_1937 = _M0L5_2aokS1730;
        } else {
          void* const _M0L6_2aerrS1731 = _tmp_1936.data.err;
          moonbit_decref(_M0L12async__testsS780);
          moonbit_decref(_M0L17error__to__stringS759);
          moonbit_decref(_M0L8filenameS755);
          _M0L11_2atry__errS774 = _M0L6_2aerrS1731;
          goto join_773;
        }
        if (_handle__error__result_1937) {
          moonbit_decref(_M0L12async__testsS780);
          moonbit_decref(_M0L17error__to__stringS759);
          moonbit_decref(_M0L8filenameS755);
          _M0L6_2atmpS1727 = 1;
        } else {
          struct moonbit_result_0 _tmp_1938;
          moonbit_incref(_M0L14handle__resultS750);
          #line 570 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
          _tmp_1938
          = _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS780, _M0L8filenameS755, _M0L5indexS758, _M0L14handle__resultS750, _M0L17error__to__stringS759);
          if (_tmp_1938.tag) {
            int32_t const _M0L5_2aokS1728 = _tmp_1938.data.ok;
            _M0L6_2atmpS1727 = _M0L5_2aokS1728;
          } else {
            void* const _M0L6_2aerrS1729 = _tmp_1938.data.err;
            _M0L11_2atry__errS774 = _M0L6_2aerrS1729;
            goto join_773;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1727) {
    void* _M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1738 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1738)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1738)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS774
    = _M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1738;
    goto join_773;
  } else {
    moonbit_decref(_M0L14handle__resultS750);
  }
  goto joinlet_1929;
  join_773:;
  _M0L3errS775 = _M0L11_2atry__errS774;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS778
  = (struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS775;
  _M0L8_2afieldS1740 = _M0L36_2aMoonBitTestDriverInternalSkipTestS778->$0;
  _M0L6_2acntS1868
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS778)->rc;
  if (_M0L6_2acntS1868 > 1) {
    int32_t _M0L11_2anew__cntS1869 = _M0L6_2acntS1868 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS778)->rc
    = _M0L11_2anew__cntS1869;
    moonbit_incref(_M0L8_2afieldS1740);
  } else if (_M0L6_2acntS1868 == 1) {
    #line 577 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS778);
  }
  _M0L7_2anameS779 = _M0L8_2afieldS1740;
  _M0L4nameS777 = _M0L7_2anameS779;
  goto join_776;
  goto joinlet_1939;
  join_776:;
  #line 578 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN14handle__resultS750(_M0L14handle__resultS750, _M0L4nameS777, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_1939:;
  joinlet_1929:;
  return 0;
}

moonbit_string_t _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN17error__to__stringS759(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1726,
  void* _M0L3errS760
) {
  void* _M0L1eS762;
  moonbit_string_t _M0L1eS764;
  #line 547 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L6_2aenvS1726);
  switch (Moonbit_object_tag(_M0L3errS760)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS765 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS760;
      moonbit_string_t _M0L8_2afieldS1741 = _M0L10_2aFailureS765->$0;
      int32_t _M0L6_2acntS1870 =
        Moonbit_object_header(_M0L10_2aFailureS765)->rc;
      moonbit_string_t _M0L4_2aeS766;
      if (_M0L6_2acntS1870 > 1) {
        int32_t _M0L11_2anew__cntS1871 = _M0L6_2acntS1870 - 1;
        Moonbit_object_header(_M0L10_2aFailureS765)->rc
        = _M0L11_2anew__cntS1871;
        moonbit_incref(_M0L8_2afieldS1741);
      } else if (_M0L6_2acntS1870 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L10_2aFailureS765);
      }
      _M0L4_2aeS766 = _M0L8_2afieldS1741;
      _M0L1eS764 = _M0L4_2aeS766;
      goto join_763;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS767 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS760;
      moonbit_string_t _M0L8_2afieldS1742 = _M0L15_2aInspectErrorS767->$0;
      int32_t _M0L6_2acntS1872 =
        Moonbit_object_header(_M0L15_2aInspectErrorS767)->rc;
      moonbit_string_t _M0L4_2aeS768;
      if (_M0L6_2acntS1872 > 1) {
        int32_t _M0L11_2anew__cntS1873 = _M0L6_2acntS1872 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS767)->rc
        = _M0L11_2anew__cntS1873;
        moonbit_incref(_M0L8_2afieldS1742);
      } else if (_M0L6_2acntS1872 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS767);
      }
      _M0L4_2aeS768 = _M0L8_2afieldS1742;
      _M0L1eS764 = _M0L4_2aeS768;
      goto join_763;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS769 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS760;
      moonbit_string_t _M0L8_2afieldS1743 = _M0L16_2aSnapshotErrorS769->$0;
      int32_t _M0L6_2acntS1874 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS769)->rc;
      moonbit_string_t _M0L4_2aeS770;
      if (_M0L6_2acntS1874 > 1) {
        int32_t _M0L11_2anew__cntS1875 = _M0L6_2acntS1874 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS769)->rc
        = _M0L11_2anew__cntS1875;
        moonbit_incref(_M0L8_2afieldS1743);
      } else if (_M0L6_2acntS1874 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS769);
      }
      _M0L4_2aeS770 = _M0L8_2afieldS1743;
      _M0L1eS764 = _M0L4_2aeS770;
      goto join_763;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error94mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS771 =
        (struct _M0DTPC15error5Error94mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS760;
      moonbit_string_t _M0L8_2afieldS1744 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS771->$0;
      int32_t _M0L6_2acntS1876 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS771)->rc;
      moonbit_string_t _M0L4_2aeS772;
      if (_M0L6_2acntS1876 > 1) {
        int32_t _M0L11_2anew__cntS1877 = _M0L6_2acntS1876 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS771)->rc
        = _M0L11_2anew__cntS1877;
        moonbit_incref(_M0L8_2afieldS1744);
      } else if (_M0L6_2acntS1876 == 1) {
        #line 548 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS771);
      }
      _M0L4_2aeS772 = _M0L8_2afieldS1744;
      _M0L1eS764 = _M0L4_2aeS772;
      goto join_763;
      break;
    }
    default: {
      _M0L1eS762 = _M0L3errS760;
      goto join_761;
      break;
    }
  }
  join_763:;
  return _M0L1eS764;
  join_761:;
  #line 553 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS762);
}

int32_t _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__executeN14handle__resultS750(
  struct _M0TWssbEu* _M0L6_2aenvS1712,
  moonbit_string_t _M0L8testnameS751,
  moonbit_string_t _M0L7messageS752,
  int32_t _M0L7skippedS753
) {
  struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750* _M0L14_2acasted__envS1713;
  moonbit_string_t _M0L8filenameS755;
  int32_t _M0L5indexS758;
  int32_t _M0L6_2acntS1878;
  int32_t _if__result_1942;
  moonbit_string_t _M0L10file__nameS754;
  moonbit_string_t _M0L10test__nameS756;
  moonbit_string_t _M0L7messageS757;
  moonbit_string_t _M0L6_2atmpS1725;
  moonbit_string_t _M0L6_2atmpS1724;
  moonbit_string_t _M0L6_2atmpS1722;
  moonbit_string_t _M0L6_2atmpS1723;
  moonbit_string_t _M0L6_2atmpS1721;
  moonbit_string_t _M0L6_2atmpS1719;
  moonbit_string_t _M0L6_2atmpS1720;
  moonbit_string_t _M0L6_2atmpS1718;
  moonbit_string_t _M0L6_2atmpS1716;
  moonbit_string_t _M0L6_2atmpS1717;
  moonbit_string_t _M0L6_2atmpS1715;
  moonbit_string_t _M0L6_2atmpS1714;
  #line 531 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS1713
  = (struct _M0R97_24mulpjs_2fmulp_2fplatform_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c750*)_M0L6_2aenvS1712;
  _M0L8filenameS755 = _M0L14_2acasted__envS1713->$1;
  _M0L5indexS758 = _M0L14_2acasted__envS1713->$0;
  _M0L6_2acntS1878 = Moonbit_object_header(_M0L14_2acasted__envS1713)->rc;
  if (_M0L6_2acntS1878 > 1) {
    int32_t _M0L11_2anew__cntS1879 = _M0L6_2acntS1878 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1713)->rc
    = _M0L11_2anew__cntS1879;
    moonbit_incref(_M0L8filenameS755);
  } else if (_M0L6_2acntS1878 == 1) {
    #line 531 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1713);
  }
  if (!_M0L7skippedS753) {
    _if__result_1942 = 1;
  } else {
    _if__result_1942 = 0;
  }
  if (_if__result_1942) {
    
  }
  #line 537 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L10file__nameS754
  = _M0MPC16string6String14escape_2einner(_M0L8filenameS755, 1);
  #line 538 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L10test__nameS756
  = _M0MPC16string6String14escape_2einner(_M0L8testnameS751, 1);
  #line 539 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L7messageS757
  = _M0MPC16string6String14escape_2einner(_M0L7messageS752, 1);
  #line 540 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 542 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1725
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS754);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1724
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1725);
  moonbit_decref(_M0L6_2atmpS1725);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1722
  = moonbit_add_string(_M0L6_2atmpS1724, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1724);
  #line 542 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1723 = _M0IPC13int3IntPB4Show10to__string(_M0L5indexS758);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1721 = moonbit_add_string(_M0L6_2atmpS1722, _M0L6_2atmpS1723);
  moonbit_decref(_M0L6_2atmpS1723);
  moonbit_decref(_M0L6_2atmpS1722);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1719
  = moonbit_add_string(_M0L6_2atmpS1721, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1721);
  #line 542 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1720
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS756);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1718 = moonbit_add_string(_M0L6_2atmpS1719, _M0L6_2atmpS1720);
  moonbit_decref(_M0L6_2atmpS1720);
  moonbit_decref(_M0L6_2atmpS1719);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1716
  = moonbit_add_string(_M0L6_2atmpS1718, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1718);
  #line 542 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1717
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS757);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1715 = moonbit_add_string(_M0L6_2atmpS1716, _M0L6_2atmpS1717);
  moonbit_decref(_M0L6_2atmpS1717);
  moonbit_decref(_M0L6_2atmpS1716);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1714
  = moonbit_add_string(_M0L6_2atmpS1715, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1715);
  #line 541 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1714);
  #line 544 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S749,
  moonbit_string_t _M0L8filenameS746,
  int32_t _M0L5indexS740,
  struct _M0TWssbEu* _M0L14handle__resultS736,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS738
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS716;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS745;
  struct _M0TWEuQRPC15error5Error* _M0L1fS718;
  moonbit_string_t* _M0L5attrsS719;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS739;
  moonbit_string_t _M0L4nameS722;
  moonbit_string_t _M0L4nameS720;
  int32_t _M0L6_2atmpS1711;
  struct _M0TWEOs* _M0L5_2aitS724;
  struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__* _closure_1951;
  struct _M0TWEu* _M0L6_2atmpS1702;
  struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__* _closure_1952;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS1703;
  struct moonbit_result_0 _result_1953;
  #line 405 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S749);
  moonbit_incref(_M0FP36mulpjs4mulp8platform48moonbit__test__driver__internal__no__args__tests);
  #line 412 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS745
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP36mulpjs4mulp8platform48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS746);
  if (_M0L7_2abindS745 == 0) {
    struct moonbit_result_0 _result_1944;
    if (_M0L7_2abindS745) {
      moonbit_decref(_M0L7_2abindS745);
    }
    moonbit_decref(_M0L17error__to__stringS738);
    moonbit_decref(_M0L14handle__resultS736);
    _result_1944.tag = 1;
    _result_1944.data.ok = 0;
    return _result_1944;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS747 =
      _M0L7_2abindS745;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS748 =
      _M0L7_2aSomeS747;
    _M0L10index__mapS716 = _M0L13_2aindex__mapS748;
    goto join_715;
  }
  join_715:;
  #line 414 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS739
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS716, _M0L5indexS740);
  if (_M0L7_2abindS739 == 0) {
    struct moonbit_result_0 _result_1946;
    if (_M0L7_2abindS739) {
      moonbit_decref(_M0L7_2abindS739);
    }
    moonbit_decref(_M0L17error__to__stringS738);
    moonbit_decref(_M0L14handle__resultS736);
    _result_1946.tag = 1;
    _result_1946.data.ok = 0;
    return _result_1946;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS741 = _M0L7_2abindS739;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS742 = _M0L7_2aSomeS741;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS743 = _M0L4_2axS742->$0;
    moonbit_string_t* _M0L8_2afieldS1747 = _M0L4_2axS742->$1;
    int32_t _M0L6_2acntS1880 = Moonbit_object_header(_M0L4_2axS742)->rc;
    moonbit_string_t* _M0L8_2aattrsS744;
    if (_M0L6_2acntS1880 > 1) {
      int32_t _M0L11_2anew__cntS1881 = _M0L6_2acntS1880 - 1;
      Moonbit_object_header(_M0L4_2axS742)->rc = _M0L11_2anew__cntS1881;
      moonbit_incref(_M0L8_2afieldS1747);
      moonbit_incref(_M0L4_2afS743);
    } else if (_M0L6_2acntS1880 == 1) {
      #line 412 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      moonbit_free(_M0L4_2axS742);
    }
    _M0L8_2aattrsS744 = _M0L8_2afieldS1747;
    _M0L1fS718 = _M0L4_2afS743;
    _M0L5attrsS719 = _M0L8_2aattrsS744;
    goto join_717;
  }
  join_717:;
  _M0L6_2atmpS1711 = Moonbit_array_length(_M0L5attrsS719);
  if (_M0L6_2atmpS1711 >= 1) {
    moonbit_string_t _M0L7_2anameS723 = (moonbit_string_t)_M0L5attrsS719[0];
    moonbit_incref(_M0L7_2anameS723);
    _M0L4nameS722 = _M0L7_2anameS723;
    goto join_721;
  } else {
    _M0L4nameS720 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_1947;
  join_721:;
  _M0L4nameS720 = _M0L4nameS722;
  joinlet_1947:;
  #line 415 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L5_2aitS724 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS719);
  while (1) {
    moonbit_string_t _M0L4attrS726;
    moonbit_string_t _M0L7_2abindS733;
    int32_t _M0L6_2atmpS1695;
    int64_t _M0L6_2atmpS1694;
    moonbit_incref(_M0L5_2aitS724);
    #line 417 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    _M0L7_2abindS733 = _M0MPB4Iter4nextGsE(_M0L5_2aitS724);
    if (_M0L7_2abindS733 == 0) {
      if (_M0L7_2abindS733) {
        moonbit_decref(_M0L7_2abindS733);
      }
      moonbit_decref(_M0L5_2aitS724);
    } else {
      moonbit_string_t _M0L7_2aSomeS734 = _M0L7_2abindS733;
      moonbit_string_t _M0L7_2aattrS735 = _M0L7_2aSomeS734;
      _M0L4attrS726 = _M0L7_2aattrS735;
      goto join_725;
    }
    goto joinlet_1949;
    join_725:;
    _M0L6_2atmpS1695 = Moonbit_array_length(_M0L4attrS726);
    _M0L6_2atmpS1694 = (int64_t)_M0L6_2atmpS1695;
    moonbit_incref(_M0L4attrS726);
    #line 418 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS726, 5, 0, _M0L6_2atmpS1694)
    ) {
      int32_t _M0L6_2atmpS1701 = _M0L4attrS726[0];
      int32_t _M0L4_2axS727 = _M0L6_2atmpS1701;
      if (_M0L4_2axS727 == 112) {
        int32_t _M0L6_2atmpS1700 = _M0L4attrS726[1];
        int32_t _M0L4_2axS728 = _M0L6_2atmpS1700;
        if (_M0L4_2axS728 == 97) {
          int32_t _M0L6_2atmpS1699 = _M0L4attrS726[2];
          int32_t _M0L4_2axS729 = _M0L6_2atmpS1699;
          if (_M0L4_2axS729 == 110) {
            int32_t _M0L6_2atmpS1698 = _M0L4attrS726[3];
            int32_t _M0L4_2axS730 = _M0L6_2atmpS1698;
            if (_M0L4_2axS730 == 105) {
              int32_t _M0L6_2atmpS1697 = _M0L4attrS726[4];
              int32_t _M0L4_2axS731;
              moonbit_decref(_M0L4attrS726);
              _M0L4_2axS731 = _M0L6_2atmpS1697;
              if (_M0L4_2axS731 == 99) {
                void* _M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1696;
                struct moonbit_result_0 _result_1950;
                moonbit_decref(_M0L17error__to__stringS738);
                moonbit_decref(_M0L14handle__resultS736);
                moonbit_decref(_M0L5_2aitS724);
                moonbit_decref(_M0L1fS718);
                _M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1696
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1696)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1696)->$0
                = _M0L4nameS720;
                _result_1950.tag = 0;
                _result_1950.data.err
                = _M0L96mulpjs_2fmulp_2fplatform_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1696;
                return _result_1950;
              }
            } else {
              moonbit_decref(_M0L4attrS726);
            }
          } else {
            moonbit_decref(_M0L4attrS726);
          }
        } else {
          moonbit_decref(_M0L4attrS726);
        }
      } else {
        moonbit_decref(_M0L4attrS726);
      }
    } else {
      moonbit_decref(_M0L4attrS726);
    }
    continue;
    joinlet_1949:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS736);
  moonbit_incref(_M0L4nameS720);
  _closure_1951
  = (struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__*)moonbit_malloc(sizeof(struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__));
  Moonbit_object_header(_closure_1951)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__, $0) >> 2, 2, 0);
  _closure_1951->code
  = &_M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testC1708l425;
  _closure_1951->$0 = _M0L14handle__resultS736;
  _closure_1951->$1 = _M0L4nameS720;
  _M0L6_2atmpS1702 = (struct _M0TWEu*)_closure_1951;
  _closure_1952
  = (struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__*)moonbit_malloc(sizeof(struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__));
  Moonbit_object_header(_closure_1952)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__, $0) >> 2, 3, 0);
  _closure_1952->code
  = &_M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testC1704l426;
  _closure_1952->$0 = _M0L17error__to__stringS738;
  _closure_1952->$1 = _M0L14handle__resultS736;
  _closure_1952->$2 = _M0L4nameS720;
  _M0L6_2atmpS1703 = (struct _M0TWRPC15error5ErrorEu*)_closure_1952;
  #line 423 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0FP36mulpjs4mulp8platform45moonbit__test__driver__internal__catch__error(_M0L1fS718, _M0L6_2atmpS1702, _M0L6_2atmpS1703);
  _result_1953.tag = 1;
  _result_1953.data.ok = 1;
  return _result_1953;
}

int32_t _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testC1708l425(
  struct _M0TWEu* _M0L6_2aenvS1709
) {
  struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__* _M0L14_2acasted__envS1710;
  moonbit_string_t _M0L4nameS720;
  struct _M0TWssbEu* _M0L8_2afieldS1749;
  int32_t _M0L6_2acntS1882;
  struct _M0TWssbEu* _M0L14handle__resultS736;
  #line 425 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS1710
  = (struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1708__l425__*)_M0L6_2aenvS1709;
  _M0L4nameS720 = _M0L14_2acasted__envS1710->$1;
  _M0L8_2afieldS1749 = _M0L14_2acasted__envS1710->$0;
  _M0L6_2acntS1882 = Moonbit_object_header(_M0L14_2acasted__envS1710)->rc;
  if (_M0L6_2acntS1882 > 1) {
    int32_t _M0L11_2anew__cntS1883 = _M0L6_2acntS1882 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1710)->rc
    = _M0L11_2anew__cntS1883;
    moonbit_incref(_M0L4nameS720);
    moonbit_incref(_M0L8_2afieldS1749);
  } else if (_M0L6_2acntS1882 == 1) {
    #line 425 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1710);
  }
  _M0L14handle__resultS736 = _M0L8_2afieldS1749;
  #line 425 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS736->code(_M0L14handle__resultS736, _M0L4nameS720, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP36mulpjs4mulp8platform41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testC1704l426(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS1705,
  void* _M0L3errS737
) {
  struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__* _M0L14_2acasted__envS1706;
  moonbit_string_t _M0L4nameS720;
  struct _M0TWssbEu* _M0L14handle__resultS736;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS1751;
  int32_t _M0L6_2acntS1884;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS738;
  moonbit_string_t _M0L6_2atmpS1707;
  #line 426 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS1706
  = (struct _M0R165_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplatform_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1704__l426__*)_M0L6_2aenvS1705;
  _M0L4nameS720 = _M0L14_2acasted__envS1706->$2;
  _M0L14handle__resultS736 = _M0L14_2acasted__envS1706->$1;
  _M0L8_2afieldS1751 = _M0L14_2acasted__envS1706->$0;
  _M0L6_2acntS1884 = Moonbit_object_header(_M0L14_2acasted__envS1706)->rc;
  if (_M0L6_2acntS1884 > 1) {
    int32_t _M0L11_2anew__cntS1885 = _M0L6_2acntS1884 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1706)->rc
    = _M0L11_2anew__cntS1885;
    moonbit_incref(_M0L4nameS720);
    moonbit_incref(_M0L14handle__resultS736);
    moonbit_incref(_M0L8_2afieldS1751);
  } else if (_M0L6_2acntS1884 == 1) {
    #line 426 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1706);
  }
  _M0L17error__to__stringS738 = _M0L8_2afieldS1751;
  #line 426 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1707
  = _M0L17error__to__stringS738->code(_M0L17error__to__stringS738, _M0L3errS737);
  #line 426 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS736->code(_M0L14handle__resultS736, _M0L4nameS720, _M0L6_2atmpS1707, 0);
  return 0;
}

int32_t _M0FP36mulpjs4mulp8platform45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS711,
  struct _M0TWEu* _M0L6on__okS712,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS709
) {
  void* _M0L11_2atry__errS707;
  struct moonbit_result_0 _tmp_1955;
  void* _M0L3errS708;
  #line 375 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  #line 382 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _tmp_1955 = _M0L1fS711->code(_M0L1fS711);
  if (_tmp_1955.tag) {
    int32_t const _M0L5_2aokS1692 = _tmp_1955.data.ok;
    moonbit_decref(_M0L7on__errS709);
  } else {
    void* const _M0L6_2aerrS1693 = _tmp_1955.data.err;
    moonbit_decref(_M0L6on__okS712);
    _M0L11_2atry__errS707 = _M0L6_2aerrS1693;
    goto join_706;
  }
  #line 382 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6on__okS712->code(_M0L6on__okS712);
  goto joinlet_1954;
  join_706:;
  _M0L3errS708 = _M0L11_2atry__errS707;
  #line 383 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L7on__errS709->code(_M0L7on__errS709, _M0L3errS708);
  joinlet_1954:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S666;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS679;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS684;
  struct _M0TUsiE** _M0L6_2atmpS1691;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS691;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS692;
  moonbit_string_t _M0L6_2atmpS1690;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS693;
  int32_t _M0L7_2abindS694;
  int32_t _M0L2__S695;
  #line 193 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S666 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS679
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS684 = 0;
  _M0L6_2atmpS1691 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS691
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS691)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS691->$0 = _M0L6_2atmpS1691;
  _M0L16file__and__indexS691->$1 = 0;
  #line 282 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L9cli__argsS692
  = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS679(_M0L57moonbit__test__driver__internal__get__cli__args__internalS679);
  #line 284 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1690 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS692, 1);
  #line 283 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L10test__argsS693
  = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS684(_M0L51moonbit__test__driver__internal__split__mbt__stringS684, _M0L6_2atmpS1690, 47);
  _M0L7_2abindS694 = _M0L10test__argsS693->$1;
  _M0L2__S695 = 0;
  while (1) {
    if (_M0L2__S695 < _M0L7_2abindS694) {
      moonbit_string_t* _M0L3bufS1689 = _M0L10test__argsS693->$0;
      moonbit_string_t _M0L3argS696 =
        (moonbit_string_t)_M0L3bufS1689[_M0L2__S695];
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS697;
      moonbit_string_t _M0L4fileS698;
      moonbit_string_t _M0L5rangeS699;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS700;
      moonbit_string_t _M0L6_2atmpS1687;
      int32_t _M0L5startS701;
      moonbit_string_t _M0L6_2atmpS1686;
      int32_t _M0L3endS702;
      int32_t _M0L1iS703;
      int32_t _M0L6_2atmpS1688;
      moonbit_incref(_M0L3argS696);
      #line 288 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L16file__and__rangeS697
      = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS684(_M0L51moonbit__test__driver__internal__split__mbt__stringS684, _M0L3argS696, 58);
      moonbit_incref(_M0L16file__and__rangeS697);
      #line 289 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L4fileS698
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS697, 0);
      #line 290 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L5rangeS699
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS697, 1);
      #line 291 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L15start__and__endS700
      = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS684(_M0L51moonbit__test__driver__internal__split__mbt__stringS684, _M0L5rangeS699, 45);
      moonbit_incref(_M0L15start__and__endS700);
      #line 294 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS1687
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS700, 0);
      #line 294 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L5startS701
      = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S666(_M0L45moonbit__test__driver__internal__parse__int__S666, _M0L6_2atmpS1687);
      #line 295 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS1686
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS700, 1);
      #line 295 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L3endS702
      = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S666(_M0L45moonbit__test__driver__internal__parse__int__S666, _M0L6_2atmpS1686);
      _M0L1iS703 = _M0L5startS701;
      while (1) {
        if (_M0L1iS703 < _M0L3endS702) {
          struct _M0TUsiE* _M0L8_2atupleS1684;
          int32_t _M0L6_2atmpS1685;
          moonbit_incref(_M0L4fileS698);
          _M0L8_2atupleS1684
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1684)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1684->$0 = _M0L4fileS698;
          _M0L8_2atupleS1684->$1 = _M0L1iS703;
          moonbit_incref(_M0L16file__and__indexS691);
          #line 297 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS691, _M0L8_2atupleS1684);
          _M0L6_2atmpS1685 = _M0L1iS703 + 1;
          _M0L1iS703 = _M0L6_2atmpS1685;
          continue;
        } else {
          moonbit_decref(_M0L4fileS698);
        }
        break;
      }
      _M0L6_2atmpS1688 = _M0L2__S695 + 1;
      _M0L2__S695 = _M0L6_2atmpS1688;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS693);
    }
    break;
  }
  return _M0L16file__and__indexS691;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS684(
  int32_t _M0L6_2aenvS1665,
  moonbit_string_t _M0L1sS685,
  int32_t _M0L3sepS686
) {
  moonbit_string_t* _M0L6_2atmpS1683;
  struct _M0TPB5ArrayGsE* _M0L3resS687;
  struct _M0TPB8MutLocalGiE* _M0L1iS688;
  struct _M0TPB8MutLocalGiE* _M0L5startS689;
  int32_t _M0L3valS1678;
  int32_t _M0L6_2atmpS1679;
  #line 261 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1683 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS687
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS687)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS687->$0 = _M0L6_2atmpS1683;
  _M0L3resS687->$1 = 0;
  _M0L1iS688
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS688)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS688->$0 = 0;
  _M0L5startS689
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS689)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5startS689->$0 = 0;
  while (1) {
    int32_t _M0L3valS1666 = _M0L1iS688->$0;
    int32_t _M0L6_2atmpS1667 = Moonbit_array_length(_M0L1sS685);
    if (_M0L3valS1666 < _M0L6_2atmpS1667) {
      int32_t _M0L3valS1670 = _M0L1iS688->$0;
      int32_t _M0L6_2atmpS1669;
      int32_t _M0L6_2atmpS1668;
      int32_t _M0L3valS1677;
      int32_t _M0L6_2atmpS1676;
      if (
        _M0L3valS1670 < 0
        || _M0L3valS1670 >= Moonbit_array_length(_M0L1sS685)
      ) {
        #line 269 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1669 = _M0L1sS685[_M0L3valS1670];
      _M0L6_2atmpS1668 = _M0L6_2atmpS1669;
      if (_M0L6_2atmpS1668 == _M0L3sepS686) {
        int32_t _M0L3valS1672 = _M0L5startS689->$0;
        int32_t _M0L3valS1673 = _M0L1iS688->$0;
        moonbit_string_t _M0L6_2atmpS1671;
        int32_t _M0L3valS1675;
        int32_t _M0L6_2atmpS1674;
        moonbit_incref(_M0L1sS685);
        #line 270 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        _M0L6_2atmpS1671
        = _M0MPC16string6String17unsafe__substring(_M0L1sS685, _M0L3valS1672, _M0L3valS1673);
        moonbit_incref(_M0L3resS687);
        #line 270 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS687, _M0L6_2atmpS1671);
        _M0L3valS1675 = _M0L1iS688->$0;
        _M0L6_2atmpS1674 = _M0L3valS1675 + 1;
        _M0L5startS689->$0 = _M0L6_2atmpS1674;
      }
      _M0L3valS1677 = _M0L1iS688->$0;
      _M0L6_2atmpS1676 = _M0L3valS1677 + 1;
      _M0L1iS688->$0 = _M0L6_2atmpS1676;
      continue;
    } else {
      moonbit_decref(_M0L1iS688);
    }
    break;
  }
  _M0L3valS1678 = _M0L5startS689->$0;
  _M0L6_2atmpS1679 = Moonbit_array_length(_M0L1sS685);
  if (_M0L3valS1678 < _M0L6_2atmpS1679) {
    int32_t _M0L3valS1681 = _M0L5startS689->$0;
    int32_t _M0L6_2atmpS1682;
    moonbit_string_t _M0L6_2atmpS1680;
    moonbit_decref(_M0L5startS689);
    _M0L6_2atmpS1682 = Moonbit_array_length(_M0L1sS685);
    #line 276 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    _M0L6_2atmpS1680
    = _M0MPC16string6String17unsafe__substring(_M0L1sS685, _M0L3valS1681, _M0L6_2atmpS1682);
    moonbit_incref(_M0L3resS687);
    #line 276 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS687, _M0L6_2atmpS1680);
  } else {
    moonbit_decref(_M0L5startS689);
    moonbit_decref(_M0L1sS685);
  }
  return _M0L3resS687;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS679(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672
) {
  moonbit_bytes_t* _M0L3tmpS680;
  int32_t _M0L6_2atmpS1664;
  struct _M0TPB5ArrayGsE* _M0L3resS681;
  int32_t _M0L1iS682;
  #line 250 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  #line 253 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L3tmpS680
  = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1664 = Moonbit_array_length(_M0L3tmpS680);
  #line 254 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L3resS681 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1664);
  _M0L1iS682 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1660 = Moonbit_array_length(_M0L3tmpS680);
    if (_M0L1iS682 < _M0L6_2atmpS1660) {
      moonbit_bytes_t _M0L6_2atmpS1662;
      moonbit_string_t _M0L6_2atmpS1661;
      int32_t _M0L6_2atmpS1663;
      if (_M0L1iS682 < 0 || _M0L1iS682 >= Moonbit_array_length(_M0L3tmpS680)) {
        #line 256 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1662 = (moonbit_bytes_t)_M0L3tmpS680[_M0L1iS682];
      moonbit_incref(_M0L6_2atmpS1662);
      #line 256 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS1661
      = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672, _M0L6_2atmpS1662);
      moonbit_incref(_M0L3resS681);
      #line 256 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS681, _M0L6_2atmpS1661);
      _M0L6_2atmpS1663 = _M0L1iS682 + 1;
      _M0L1iS682 = _M0L6_2atmpS1663;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS680);
    }
    break;
  }
  return _M0L3resS681;
}

moonbit_string_t _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS672(
  int32_t _M0L6_2aenvS1574,
  moonbit_bytes_t _M0L5bytesS673
) {
  struct _M0TPB13StringBuilder* _M0L3resS674;
  int32_t _M0L3lenS675;
  struct _M0TPB8MutLocalGiE* _M0L1iS676;
  #line 206 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  #line 209 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L3resS674 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS675 = Moonbit_array_length(_M0L5bytesS673);
  _M0L1iS676
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS676)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS676->$0 = 0;
  while (1) {
    int32_t _M0L3valS1575 = _M0L1iS676->$0;
    if (_M0L3valS1575 < _M0L3lenS675) {
      int32_t _M0L3valS1659 = _M0L1iS676->$0;
      int32_t _M0L6_2atmpS1658;
      int32_t _M0L6_2atmpS1657;
      struct _M0TPB8MutLocalGiE* _M0L1cS677;
      int32_t _M0L3valS1576;
      if (
        _M0L3valS1659 < 0
        || _M0L3valS1659 >= Moonbit_array_length(_M0L5bytesS673)
      ) {
        #line 213 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1658 = _M0L5bytesS673[_M0L3valS1659];
      _M0L6_2atmpS1657 = (int32_t)_M0L6_2atmpS1658;
      _M0L1cS677
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L1cS677)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
      _M0L1cS677->$0 = _M0L6_2atmpS1657;
      _M0L3valS1576 = _M0L1cS677->$0;
      if (_M0L3valS1576 < 128) {
        int32_t _M0L3valS1578 = _M0L1cS677->$0;
        int32_t _M0L6_2atmpS1577;
        int32_t _M0L3valS1580;
        int32_t _M0L6_2atmpS1579;
        moonbit_decref(_M0L1cS677);
        _M0L6_2atmpS1577 = _M0L3valS1578;
        moonbit_incref(_M0L3resS674);
        #line 215 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS674, _M0L6_2atmpS1577);
        _M0L3valS1580 = _M0L1iS676->$0;
        _M0L6_2atmpS1579 = _M0L3valS1580 + 1;
        _M0L1iS676->$0 = _M0L6_2atmpS1579;
      } else {
        int32_t _M0L3valS1581 = _M0L1cS677->$0;
        if (_M0L3valS1581 < 224) {
          int32_t _M0L3valS1583 = _M0L1iS676->$0;
          int32_t _M0L6_2atmpS1582 = _M0L3valS1583 + 1;
          int32_t _M0L3valS1592;
          int32_t _M0L6_2atmpS1591;
          int32_t _M0L6_2atmpS1585;
          int32_t _M0L3valS1590;
          int32_t _M0L6_2atmpS1589;
          int32_t _M0L6_2atmpS1588;
          int32_t _M0L6_2atmpS1587;
          int32_t _M0L6_2atmpS1586;
          int32_t _M0L6_2atmpS1584;
          int32_t _M0L3valS1594;
          int32_t _M0L6_2atmpS1593;
          int32_t _M0L3valS1596;
          int32_t _M0L6_2atmpS1595;
          if (_M0L6_2atmpS1582 >= _M0L3lenS675) {
            moonbit_decref(_M0L1cS677);
            moonbit_decref(_M0L1iS676);
            moonbit_decref(_M0L5bytesS673);
            break;
          }
          _M0L3valS1592 = _M0L1cS677->$0;
          _M0L6_2atmpS1591 = _M0L3valS1592 & 31;
          _M0L6_2atmpS1585 = _M0L6_2atmpS1591 << 6;
          _M0L3valS1590 = _M0L1iS676->$0;
          _M0L6_2atmpS1589 = _M0L3valS1590 + 1;
          if (
            _M0L6_2atmpS1589 < 0
            || _M0L6_2atmpS1589 >= Moonbit_array_length(_M0L5bytesS673)
          ) {
            #line 221 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1588 = _M0L5bytesS673[_M0L6_2atmpS1589];
          _M0L6_2atmpS1587 = (int32_t)_M0L6_2atmpS1588;
          _M0L6_2atmpS1586 = _M0L6_2atmpS1587 & 63;
          _M0L6_2atmpS1584 = _M0L6_2atmpS1585 | _M0L6_2atmpS1586;
          _M0L1cS677->$0 = _M0L6_2atmpS1584;
          _M0L3valS1594 = _M0L1cS677->$0;
          moonbit_decref(_M0L1cS677);
          _M0L6_2atmpS1593 = _M0L3valS1594;
          moonbit_incref(_M0L3resS674);
          #line 222 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS674, _M0L6_2atmpS1593);
          _M0L3valS1596 = _M0L1iS676->$0;
          _M0L6_2atmpS1595 = _M0L3valS1596 + 2;
          _M0L1iS676->$0 = _M0L6_2atmpS1595;
        } else {
          int32_t _M0L3valS1597 = _M0L1cS677->$0;
          if (_M0L3valS1597 < 240) {
            int32_t _M0L3valS1599 = _M0L1iS676->$0;
            int32_t _M0L6_2atmpS1598 = _M0L3valS1599 + 2;
            int32_t _M0L3valS1615;
            int32_t _M0L6_2atmpS1614;
            int32_t _M0L6_2atmpS1607;
            int32_t _M0L3valS1613;
            int32_t _M0L6_2atmpS1612;
            int32_t _M0L6_2atmpS1611;
            int32_t _M0L6_2atmpS1610;
            int32_t _M0L6_2atmpS1609;
            int32_t _M0L6_2atmpS1608;
            int32_t _M0L6_2atmpS1601;
            int32_t _M0L3valS1606;
            int32_t _M0L6_2atmpS1605;
            int32_t _M0L6_2atmpS1604;
            int32_t _M0L6_2atmpS1603;
            int32_t _M0L6_2atmpS1602;
            int32_t _M0L6_2atmpS1600;
            int32_t _M0L3valS1617;
            int32_t _M0L6_2atmpS1616;
            int32_t _M0L3valS1619;
            int32_t _M0L6_2atmpS1618;
            if (_M0L6_2atmpS1598 >= _M0L3lenS675) {
              moonbit_decref(_M0L1cS677);
              moonbit_decref(_M0L1iS676);
              moonbit_decref(_M0L5bytesS673);
              break;
            }
            _M0L3valS1615 = _M0L1cS677->$0;
            _M0L6_2atmpS1614 = _M0L3valS1615 & 15;
            _M0L6_2atmpS1607 = _M0L6_2atmpS1614 << 12;
            _M0L3valS1613 = _M0L1iS676->$0;
            _M0L6_2atmpS1612 = _M0L3valS1613 + 1;
            if (
              _M0L6_2atmpS1612 < 0
              || _M0L6_2atmpS1612 >= Moonbit_array_length(_M0L5bytesS673)
            ) {
              #line 229 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1611 = _M0L5bytesS673[_M0L6_2atmpS1612];
            _M0L6_2atmpS1610 = (int32_t)_M0L6_2atmpS1611;
            _M0L6_2atmpS1609 = _M0L6_2atmpS1610 & 63;
            _M0L6_2atmpS1608 = _M0L6_2atmpS1609 << 6;
            _M0L6_2atmpS1601 = _M0L6_2atmpS1607 | _M0L6_2atmpS1608;
            _M0L3valS1606 = _M0L1iS676->$0;
            _M0L6_2atmpS1605 = _M0L3valS1606 + 2;
            if (
              _M0L6_2atmpS1605 < 0
              || _M0L6_2atmpS1605 >= Moonbit_array_length(_M0L5bytesS673)
            ) {
              #line 230 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1604 = _M0L5bytesS673[_M0L6_2atmpS1605];
            _M0L6_2atmpS1603 = (int32_t)_M0L6_2atmpS1604;
            _M0L6_2atmpS1602 = _M0L6_2atmpS1603 & 63;
            _M0L6_2atmpS1600 = _M0L6_2atmpS1601 | _M0L6_2atmpS1602;
            _M0L1cS677->$0 = _M0L6_2atmpS1600;
            _M0L3valS1617 = _M0L1cS677->$0;
            moonbit_decref(_M0L1cS677);
            _M0L6_2atmpS1616 = _M0L3valS1617;
            moonbit_incref(_M0L3resS674);
            #line 231 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS674, _M0L6_2atmpS1616);
            _M0L3valS1619 = _M0L1iS676->$0;
            _M0L6_2atmpS1618 = _M0L3valS1619 + 3;
            _M0L1iS676->$0 = _M0L6_2atmpS1618;
          } else {
            int32_t _M0L3valS1621 = _M0L1iS676->$0;
            int32_t _M0L6_2atmpS1620 = _M0L3valS1621 + 3;
            int32_t _M0L3valS1644;
            int32_t _M0L6_2atmpS1643;
            int32_t _M0L6_2atmpS1636;
            int32_t _M0L3valS1642;
            int32_t _M0L6_2atmpS1641;
            int32_t _M0L6_2atmpS1640;
            int32_t _M0L6_2atmpS1639;
            int32_t _M0L6_2atmpS1638;
            int32_t _M0L6_2atmpS1637;
            int32_t _M0L6_2atmpS1629;
            int32_t _M0L3valS1635;
            int32_t _M0L6_2atmpS1634;
            int32_t _M0L6_2atmpS1633;
            int32_t _M0L6_2atmpS1632;
            int32_t _M0L6_2atmpS1631;
            int32_t _M0L6_2atmpS1630;
            int32_t _M0L6_2atmpS1623;
            int32_t _M0L3valS1628;
            int32_t _M0L6_2atmpS1627;
            int32_t _M0L6_2atmpS1626;
            int32_t _M0L6_2atmpS1625;
            int32_t _M0L6_2atmpS1624;
            int32_t _M0L6_2atmpS1622;
            int32_t _M0L3valS1646;
            int32_t _M0L6_2atmpS1645;
            int32_t _M0L3valS1650;
            int32_t _M0L6_2atmpS1649;
            int32_t _M0L6_2atmpS1648;
            int32_t _M0L6_2atmpS1647;
            int32_t _M0L3valS1654;
            int32_t _M0L6_2atmpS1653;
            int32_t _M0L6_2atmpS1652;
            int32_t _M0L6_2atmpS1651;
            int32_t _M0L3valS1656;
            int32_t _M0L6_2atmpS1655;
            if (_M0L6_2atmpS1620 >= _M0L3lenS675) {
              moonbit_decref(_M0L1cS677);
              moonbit_decref(_M0L1iS676);
              moonbit_decref(_M0L5bytesS673);
              break;
            }
            _M0L3valS1644 = _M0L1cS677->$0;
            _M0L6_2atmpS1643 = _M0L3valS1644 & 7;
            _M0L6_2atmpS1636 = _M0L6_2atmpS1643 << 18;
            _M0L3valS1642 = _M0L1iS676->$0;
            _M0L6_2atmpS1641 = _M0L3valS1642 + 1;
            if (
              _M0L6_2atmpS1641 < 0
              || _M0L6_2atmpS1641 >= Moonbit_array_length(_M0L5bytesS673)
            ) {
              #line 238 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1640 = _M0L5bytesS673[_M0L6_2atmpS1641];
            _M0L6_2atmpS1639 = (int32_t)_M0L6_2atmpS1640;
            _M0L6_2atmpS1638 = _M0L6_2atmpS1639 & 63;
            _M0L6_2atmpS1637 = _M0L6_2atmpS1638 << 12;
            _M0L6_2atmpS1629 = _M0L6_2atmpS1636 | _M0L6_2atmpS1637;
            _M0L3valS1635 = _M0L1iS676->$0;
            _M0L6_2atmpS1634 = _M0L3valS1635 + 2;
            if (
              _M0L6_2atmpS1634 < 0
              || _M0L6_2atmpS1634 >= Moonbit_array_length(_M0L5bytesS673)
            ) {
              #line 239 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1633 = _M0L5bytesS673[_M0L6_2atmpS1634];
            _M0L6_2atmpS1632 = (int32_t)_M0L6_2atmpS1633;
            _M0L6_2atmpS1631 = _M0L6_2atmpS1632 & 63;
            _M0L6_2atmpS1630 = _M0L6_2atmpS1631 << 6;
            _M0L6_2atmpS1623 = _M0L6_2atmpS1629 | _M0L6_2atmpS1630;
            _M0L3valS1628 = _M0L1iS676->$0;
            _M0L6_2atmpS1627 = _M0L3valS1628 + 3;
            if (
              _M0L6_2atmpS1627 < 0
              || _M0L6_2atmpS1627 >= Moonbit_array_length(_M0L5bytesS673)
            ) {
              #line 240 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1626 = _M0L5bytesS673[_M0L6_2atmpS1627];
            _M0L6_2atmpS1625 = (int32_t)_M0L6_2atmpS1626;
            _M0L6_2atmpS1624 = _M0L6_2atmpS1625 & 63;
            _M0L6_2atmpS1622 = _M0L6_2atmpS1623 | _M0L6_2atmpS1624;
            _M0L1cS677->$0 = _M0L6_2atmpS1622;
            _M0L3valS1646 = _M0L1cS677->$0;
            _M0L6_2atmpS1645 = _M0L3valS1646 - 65536;
            _M0L1cS677->$0 = _M0L6_2atmpS1645;
            _M0L3valS1650 = _M0L1cS677->$0;
            _M0L6_2atmpS1649 = _M0L3valS1650 >> 10;
            _M0L6_2atmpS1648 = _M0L6_2atmpS1649 + 55296;
            _M0L6_2atmpS1647 = _M0L6_2atmpS1648;
            moonbit_incref(_M0L3resS674);
            #line 242 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS674, _M0L6_2atmpS1647);
            _M0L3valS1654 = _M0L1cS677->$0;
            moonbit_decref(_M0L1cS677);
            _M0L6_2atmpS1653 = _M0L3valS1654 & 1023;
            _M0L6_2atmpS1652 = _M0L6_2atmpS1653 + 56320;
            _M0L6_2atmpS1651 = _M0L6_2atmpS1652;
            moonbit_incref(_M0L3resS674);
            #line 243 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS674, _M0L6_2atmpS1651);
            _M0L3valS1656 = _M0L1iS676->$0;
            _M0L6_2atmpS1655 = _M0L3valS1656 + 4;
            _M0L1iS676->$0 = _M0L6_2atmpS1655;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS676);
      moonbit_decref(_M0L5bytesS673);
    }
    break;
  }
  #line 247 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS674);
}

int32_t _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S666(
  int32_t _M0L6_2aenvS1567,
  moonbit_string_t _M0L1sS667
) {
  struct _M0TPB8MutLocalGiE* _M0L3resS668;
  int32_t _M0L3lenS669;
  int32_t _M0L1iS670;
  int32_t _result_1962;
  #line 197 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L3resS668
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L3resS668)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L3resS668->$0 = 0;
  _M0L3lenS669 = Moonbit_array_length(_M0L1sS667);
  _M0L1iS670 = 0;
  while (1) {
    if (_M0L1iS670 < _M0L3lenS669) {
      int32_t _M0L3valS1572 = _M0L3resS668->$0;
      int32_t _M0L6_2atmpS1569 = _M0L3valS1572 * 10;
      int32_t _M0L6_2atmpS1571;
      int32_t _M0L6_2atmpS1570;
      int32_t _M0L6_2atmpS1568;
      int32_t _M0L6_2atmpS1573;
      if (_M0L1iS670 < 0 || _M0L1iS670 >= Moonbit_array_length(_M0L1sS667)) {
        #line 201 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1571 = _M0L1sS667[_M0L1iS670];
      _M0L6_2atmpS1570 = _M0L6_2atmpS1571 - 48;
      _M0L6_2atmpS1568 = _M0L6_2atmpS1569 + _M0L6_2atmpS1570;
      _M0L3resS668->$0 = _M0L6_2atmpS1568;
      _M0L6_2atmpS1573 = _M0L1iS670 + 1;
      _M0L1iS670 = _M0L6_2atmpS1573;
      continue;
    } else {
      moonbit_decref(_M0L1sS667);
    }
    break;
  }
  _result_1962 = _M0L3resS668->$0;
  moonbit_decref(_M0L3resS668);
  return _result_1962;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S646,
  moonbit_string_t _M0L12_2adiscard__S647,
  int32_t _M0L12_2adiscard__S648,
  struct _M0TWssbEu* _M0L12_2adiscard__S649,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S650
) {
  struct moonbit_result_0 _result_1963;
  #line 34 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S650);
  moonbit_decref(_M0L12_2adiscard__S649);
  moonbit_decref(_M0L12_2adiscard__S647);
  moonbit_decref(_M0L12_2adiscard__S646);
  _result_1963.tag = 1;
  _result_1963.data.ok = 0;
  return _result_1963;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S651,
  moonbit_string_t _M0L12_2adiscard__S652,
  int32_t _M0L12_2adiscard__S653,
  struct _M0TWssbEu* _M0L12_2adiscard__S654,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S655
) {
  struct moonbit_result_0 _result_1964;
  #line 34 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S655);
  moonbit_decref(_M0L12_2adiscard__S654);
  moonbit_decref(_M0L12_2adiscard__S652);
  moonbit_decref(_M0L12_2adiscard__S651);
  _result_1964.tag = 1;
  _result_1964.data.ok = 0;
  return _result_1964;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S656,
  moonbit_string_t _M0L12_2adiscard__S657,
  int32_t _M0L12_2adiscard__S658,
  struct _M0TWssbEu* _M0L12_2adiscard__S659,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S660
) {
  struct moonbit_result_0 _result_1965;
  #line 34 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S660);
  moonbit_decref(_M0L12_2adiscard__S659);
  moonbit_decref(_M0L12_2adiscard__S657);
  moonbit_decref(_M0L12_2adiscard__S656);
  _result_1965.tag = 1;
  _result_1965.data.ok = 0;
  return _result_1965;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp8platform21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp8platform50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S661,
  moonbit_string_t _M0L12_2adiscard__S662,
  int32_t _M0L12_2adiscard__S663,
  struct _M0TWssbEu* _M0L12_2adiscard__S664,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S665
) {
  struct moonbit_result_0 _result_1966;
  #line 34 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S665);
  moonbit_decref(_M0L12_2adiscard__S664);
  moonbit_decref(_M0L12_2adiscard__S662);
  moonbit_decref(_M0L12_2adiscard__S661);
  _result_1966.tag = 1;
  _result_1966.data.ok = 0;
  return _result_1966;
}

int32_t _M0IP016_24default__implP36mulpjs4mulp8platform28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp8platform34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S645
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S645);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp8platform47____test__7369676e616c5f7762746573742e6d6274__0(
  
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L5tokenS639;
  struct _M0TP36mulpjs4mulp8platform13SignalWatcher* _M0L7watcherS640;
  int64_t _M0L7_2abindS641;
  int32_t _M0L6_2atmpS1543;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1544;
  struct _M0TPB4Show _M0L6_2atmpS1536;
  moonbit_string_t _M0L6_2atmpS1539;
  moonbit_string_t _M0L6_2atmpS1540;
  moonbit_string_t _M0L6_2atmpS1541;
  moonbit_string_t _M0L6_2atmpS1542;
  moonbit_string_t* _M0L6_2atmpS1538;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1537;
  struct moonbit_result_0 _tmp_1967;
  int32_t _M0L6_2atmpS1554;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1555;
  struct _M0TPB4Show _M0L6_2atmpS1547;
  moonbit_string_t _M0L6_2atmpS1550;
  moonbit_string_t _M0L6_2atmpS1551;
  moonbit_string_t _M0L6_2atmpS1552;
  moonbit_string_t _M0L6_2atmpS1553;
  moonbit_string_t* _M0L6_2atmpS1549;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1548;
  struct moonbit_result_0 _tmp_1969;
  int64_t _M0L7_2abindS644;
  int32_t _M0L6_2atmpS1565;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1566;
  struct _M0TPB4Show _M0L6_2atmpS1558;
  moonbit_string_t _M0L6_2atmpS1561;
  moonbit_string_t _M0L6_2atmpS1562;
  moonbit_string_t _M0L6_2atmpS1563;
  moonbit_string_t _M0L6_2atmpS1564;
  moonbit_string_t* _M0L6_2atmpS1560;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1559;
  #line 14 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  #line 15 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0L5tokenS639 = _M0FP36mulpjs4mulp4core24new__cancellation__token();
  moonbit_incref(_M0L5tokenS639);
  #line 16 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0L7watcherS640
  = _M0FP36mulpjs4mulp8platform15signal__watcher(_M0L5tokenS639);
  #line 17 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0FP36mulpjs4mulp8platform34raise__platform__signal__for__test(0);
  moonbit_incref(_M0L7watcherS640);
  #line 18 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0L7_2abindS641
  = _M0MP36mulpjs4mulp8platform13SignalWatcher4poll(_M0L7watcherS640);
  if (_M0L7_2abindS641 == 4294967296ll) {
    _M0L6_2atmpS1543 = 0;
  } else {
    int64_t _M0L7_2aSomeS642 = _M0L7_2abindS641;
    int32_t _M0L4_2axS643 = (int32_t)_M0L7_2aSomeS642;
    switch (_M0L4_2axS643) {
      case 0: {
        _M0L6_2atmpS1543 = 1;
        break;
      }
      default: {
        _M0L6_2atmpS1543 = 0;
        break;
      }
    }
  }
  _M0L14_2aboxed__selfS1544
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS1544)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS1544->$0 = _M0L6_2atmpS1543;
  _M0L6_2atmpS1536
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS1544
  };
  _M0L6_2atmpS1539 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1540 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1541 = 0;
  _M0L6_2atmpS1542 = 0;
  _M0L6_2atmpS1538 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1538[0] = _M0L6_2atmpS1539;
  _M0L6_2atmpS1538[1] = _M0L6_2atmpS1540;
  _M0L6_2atmpS1538[2] = _M0L6_2atmpS1541;
  _M0L6_2atmpS1538[3] = _M0L6_2atmpS1542;
  _M0L6_2atmpS1537
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1537)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1537->$0 = _M0L6_2atmpS1538;
  _M0L6_2atmpS1537->$1 = 4;
  #line 18 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _tmp_1967
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1536, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS1537);
  if (_tmp_1967.tag) {
    int32_t const _M0L5_2aokS1545 = _tmp_1967.data.ok;
  } else {
    void* const _M0L6_2aerrS1546 = _tmp_1967.data.err;
    struct moonbit_result_0 _result_1968;
    moonbit_decref(_M0L7watcherS640);
    moonbit_decref(_M0L5tokenS639);
    _result_1968.tag = 0;
    _result_1968.data.err = _M0L6_2aerrS1546;
    return _result_1968;
  }
  #line 19 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0L6_2atmpS1554
  = _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(_M0L5tokenS639);
  _M0L14_2aboxed__selfS1555
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS1555)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS1555->$0 = _M0L6_2atmpS1554;
  _M0L6_2atmpS1547
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS1555
  };
  _M0L6_2atmpS1550 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS1551 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS1552 = 0;
  _M0L6_2atmpS1553 = 0;
  _M0L6_2atmpS1549 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1549[0] = _M0L6_2atmpS1550;
  _M0L6_2atmpS1549[1] = _M0L6_2atmpS1551;
  _M0L6_2atmpS1549[2] = _M0L6_2atmpS1552;
  _M0L6_2atmpS1549[3] = _M0L6_2atmpS1553;
  _M0L6_2atmpS1548
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1548)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1548->$0 = _M0L6_2atmpS1549;
  _M0L6_2atmpS1548->$1 = 4;
  #line 19 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _tmp_1969
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1547, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_15.data, _M0L6_2atmpS1548);
  if (_tmp_1969.tag) {
    int32_t const _M0L5_2aokS1556 = _tmp_1969.data.ok;
  } else {
    void* const _M0L6_2aerrS1557 = _tmp_1969.data.err;
    struct moonbit_result_0 _result_1970;
    moonbit_decref(_M0L7watcherS640);
    _result_1970.tag = 0;
    _result_1970.data.err = _M0L6_2aerrS1557;
    return _result_1970;
  }
  #line 20 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0L7_2abindS644
  = _M0MP36mulpjs4mulp8platform13SignalWatcher4poll(_M0L7watcherS640);
  _M0L6_2atmpS1565 = _M0L7_2abindS644 == 4294967296ll;
  _M0L14_2aboxed__selfS1566
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS1566)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS1566->$0 = _M0L6_2atmpS1565;
  _M0L6_2atmpS1558
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS1566
  };
  _M0L6_2atmpS1561 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS1562 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS1563 = 0;
  _M0L6_2atmpS1564 = 0;
  _M0L6_2atmpS1560 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1560[0] = _M0L6_2atmpS1561;
  _M0L6_2atmpS1560[1] = _M0L6_2atmpS1562;
  _M0L6_2atmpS1560[2] = _M0L6_2atmpS1563;
  _M0L6_2atmpS1560[3] = _M0L6_2atmpS1564;
  _M0L6_2atmpS1559
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1559)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1559->$0 = _M0L6_2atmpS1560;
  _M0L6_2atmpS1559->$1 = 4;
  #line 20 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1558, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS1559);
}

int64_t _M0MP36mulpjs4mulp8platform13SignalWatcher4poll(
  struct _M0TP36mulpjs4mulp8platform13SignalWatcher* _M0L4selfS638
) {
  int32_t _M0L7_2abindS637;
  #line 19 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
  #line 20 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
  _M0L7_2abindS637 = _M0FP36mulpjs4mulp8platform22take__platform__signal();
  switch (_M0L7_2abindS637) {
    case 2: {
      struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L8_2afieldS1757 =
        _M0L4selfS638->$0;
      int32_t _M0L6_2acntS1886 = Moonbit_object_header(_M0L4selfS638)->rc;
      struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS1534;
      if (_M0L6_2acntS1886 > 1) {
        int32_t _M0L11_2anew__cntS1887 = _M0L6_2acntS1886 - 1;
        Moonbit_object_header(_M0L4selfS638)->rc = _M0L11_2anew__cntS1887;
        moonbit_incref(_M0L8_2afieldS1757);
      } else if (_M0L6_2acntS1886 == 1) {
        #line 22 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
        moonbit_free(_M0L4selfS638);
      }
      _M0L12cancellationS1534 = _M0L8_2afieldS1757;
      #line 22 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
      _M0MP36mulpjs4mulp4core17CancellationToken6cancel(_M0L12cancellationS1534);
      return 0ll;
      break;
    }
    
    case 15: {
      struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L8_2afieldS1758 =
        _M0L4selfS638->$0;
      int32_t _M0L6_2acntS1888 = Moonbit_object_header(_M0L4selfS638)->rc;
      struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS1535;
      if (_M0L6_2acntS1888 > 1) {
        int32_t _M0L11_2anew__cntS1889 = _M0L6_2acntS1888 - 1;
        Moonbit_object_header(_M0L4selfS638)->rc = _M0L11_2anew__cntS1889;
        moonbit_incref(_M0L8_2afieldS1758);
      } else if (_M0L6_2acntS1888 == 1) {
        #line 26 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
        moonbit_free(_M0L4selfS638);
      }
      _M0L12cancellationS1535 = _M0L8_2afieldS1758;
      #line 26 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
      _M0MP36mulpjs4mulp4core17CancellationToken6cancel(_M0L12cancellationS1535);
      return 1ll;
      break;
    }
    default: {
      moonbit_decref(_M0L4selfS638);
      return 4294967296ll;
      break;
    }
  }
}

struct _M0TP36mulpjs4mulp8platform13SignalWatcher* _M0FP36mulpjs4mulp8platform15signal__watcher(
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L12cancellationS636
) {
  struct _M0TP36mulpjs4mulp8platform13SignalWatcher* _block_1971;
  #line 13 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
  #line 14 "/Users/user/workspace/github/gulp/mulp/platform/signal.mbt"
  _M0FP36mulpjs4mulp8platform35install__platform__signal__handlers();
  _block_1971
  = (struct _M0TP36mulpjs4mulp8platform13SignalWatcher*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp8platform13SignalWatcher));
  Moonbit_object_header(_block_1971)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp8platform13SignalWatcher, $0) >> 2, 1, 0);
  _block_1971->$0 = _M0L12cancellationS636;
  return _block_1971;
}

int32_t _M0FP36mulpjs4mulp8platform35install__platform__signal__handlers() {
  mulp_install_signal_handlers();
  return 0;
}

int32_t _M0FP36mulpjs4mulp8platform34raise__platform__signal__for__test(
  int32_t _M0L6signalS635
) {
  int32_t _M0L4codeS634;
  #line 2 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  switch (_M0L6signalS635) {
    case 0: {
      _M0L4codeS634 = 2;
      break;
    }
    default: {
      _M0L4codeS634 = 15;
      break;
    }
  }
  #line 7 "/Users/user/workspace/github/gulp/mulp/platform/signal_wbtest.mbt"
  _M0FP36mulpjs4mulp8platform39raise__platform__signal__for__test__ffi(_M0L4codeS634);
  return 0;
}

int32_t _M0FP36mulpjs4mulp8platform39raise__platform__signal__for__test__ffi(
  int32_t _M0L8_2aparamS791
) {
  mulp_raise_signal_for_test(_M0L8_2aparamS791);
  return 0;
}

int32_t _M0MP36mulpjs4mulp4core17CancellationToken6cancel(
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L4selfS633
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _M0L4selfS633->$0 = 1;
  moonbit_decref(_M0L4selfS633);
  return 0;
}

struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0FP36mulpjs4mulp4core24new__cancellation__token(
  
) {
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _block_1972;
  #line 7 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _block_1972
  = (struct _M0TP36mulpjs4mulp4core17CancellationToken*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp4core17CancellationToken));
  Moonbit_object_header(_block_1972)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP36mulpjs4mulp4core17CancellationToken) >> 2, 0, 0);
  _block_1972->$0 = 0;
  return _block_1972;
}

int32_t _M0MP36mulpjs4mulp4core17CancellationToken13is__cancelled(
  struct _M0TP36mulpjs4mulp4core17CancellationToken* _M0L4selfS632
) {
  int32_t _result_1973;
  #line 17 "/Users/user/workspace/github/gulp/mulp/core/context.mbt"
  _result_1973 = _M0L4selfS632->$0;
  moonbit_decref(_M0L4selfS632);
  return _result_1973;
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS630,
  int32_t _M0L5indexS631
) {
  int32_t _M0L3lenS629;
  int32_t _if__result_1974;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS629 = _M0L4selfS630->$1;
  if (_M0L5indexS631 >= 0) {
    _if__result_1974 = _M0L5indexS631 < _M0L3lenS629;
  } else {
    _if__result_1974 = 0;
  }
  if (_if__result_1974) {
    moonbit_string_t* _M0L6_2atmpS1533;
    moonbit_string_t _M0L6_2atmpS1759;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS1533 = _M0MPC15array5Array6bufferGsE(_M0L4selfS630);
    if (
      _M0L5indexS631 < 0
      || _M0L5indexS631 >= Moonbit_array_length(_M0L6_2atmpS1533)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1759 = (moonbit_string_t)_M0L6_2atmpS1533[_M0L5indexS631];
    moonbit_incref(_M0L6_2atmpS1759);
    moonbit_decref(_M0L6_2atmpS1533);
    return _M0L6_2atmpS1759;
  } else {
    moonbit_decref(_M0L4selfS630);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS628) {
  moonbit_string_t _M0L6_2atmpS1532;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS1532 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS628);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS1532);
  moonbit_decref(_M0L6_2atmpS1532);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS627,
  struct _M0TPB6Hasher* _M0L6hasherS626
) {
  #line 530 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 531 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS626, _M0L4selfS627);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS625,
  struct _M0TPB6Hasher* _M0L6hasherS624
) {
  #line 496 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 497 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS624, _M0L4selfS625);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS622,
  moonbit_string_t _M0L5valueS620
) {
  int32_t _M0L7_2abindS619;
  int32_t _M0L1iS621;
  #line 387 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS619 = Moonbit_array_length(_M0L5valueS620);
  _M0L1iS621 = 0;
  while (1) {
    if (_M0L1iS621 < _M0L7_2abindS619) {
      int32_t _M0L6_2atmpS1530 = _M0L5valueS620[_M0L1iS621];
      int32_t _M0L6_2atmpS1529 = (int32_t)_M0L6_2atmpS1530;
      uint32_t _M0L6_2atmpS1528 = *(uint32_t*)&_M0L6_2atmpS1529;
      int32_t _M0L6_2atmpS1531;
      moonbit_incref(_M0L4selfS622);
      #line 389 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS622, _M0L6_2atmpS1528);
      _M0L6_2atmpS1531 = _M0L1iS621 + 1;
      _M0L1iS621 = _M0L6_2atmpS1531;
      continue;
    } else {
      moonbit_decref(_M0L4selfS622);
      moonbit_decref(_M0L5valueS620);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS617,
  int32_t _M0L3idxS618
) {
  int32_t _result_1976;
  #line 1778 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _result_1976 = _M0L4selfS617[_M0L3idxS618];
  moonbit_decref(_M0L4selfS617);
  return _result_1976;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS604,
  int32_t _M0L3keyS600
) {
  int32_t _M0L4hashS599;
  int32_t _M0L14capacity__maskS1513;
  int32_t _M0L6_2atmpS1512;
  int32_t _M0L1iS601;
  int32_t _M0L3idxS602;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS599 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS600);
  _M0L14capacity__maskS1513 = _M0L4selfS604->$3;
  _M0L6_2atmpS1512 = _M0L4hashS599 & _M0L14capacity__maskS1513;
  _M0L1iS601 = 0;
  _M0L3idxS602 = _M0L6_2atmpS1512;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1511 =
      _M0L4selfS604->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS603;
    if (
      _M0L3idxS602 < 0
      || _M0L3idxS602 >= Moonbit_array_length(_M0L7entriesS1511)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS603
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1511[
        _M0L3idxS602
      ];
    if (_M0L7_2abindS603 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1500;
      if (_M0L7_2abindS603) {
        moonbit_incref(_M0L7_2abindS603);
      }
      moonbit_decref(_M0L4selfS604);
      if (_M0L7_2abindS603) {
        moonbit_decref(_M0L7_2abindS603);
      }
      _M0L6_2atmpS1500 = 0;
      return _M0L6_2atmpS1500;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS605 =
        _M0L7_2abindS603;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS606 =
        _M0L7_2aSomeS605;
      int32_t _M0L4hashS1502 = _M0L8_2aentryS606->$3;
      int32_t _if__result_1978;
      int32_t _M0L3pslS1505;
      int32_t _M0L6_2atmpS1507;
      int32_t _M0L6_2atmpS1509;
      int32_t _M0L14capacity__maskS1510;
      int32_t _M0L6_2atmpS1508;
      if (_M0L4hashS1502 == _M0L4hashS599) {
        int32_t _M0L3keyS1501 = _M0L8_2aentryS606->$4;
        _if__result_1978 = _M0L3keyS1501 == _M0L3keyS600;
      } else {
        _if__result_1978 = 0;
      }
      if (_if__result_1978) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1760;
        int32_t _M0L6_2acntS1890;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1504;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1503;
        moonbit_incref(_M0L8_2aentryS606);
        moonbit_decref(_M0L4selfS604);
        _M0L8_2afieldS1760 = _M0L8_2aentryS606->$5;
        _M0L6_2acntS1890 = Moonbit_object_header(_M0L8_2aentryS606)->rc;
        if (_M0L6_2acntS1890 > 1) {
          int32_t _M0L11_2anew__cntS1892 = _M0L6_2acntS1890 - 1;
          Moonbit_object_header(_M0L8_2aentryS606)->rc
          = _M0L11_2anew__cntS1892;
          moonbit_incref(_M0L8_2afieldS1760);
        } else if (_M0L6_2acntS1890 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1891 =
            _M0L8_2aentryS606->$1;
          if (_M0L8_2afieldS1891) {
            moonbit_decref(_M0L8_2afieldS1891);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS606);
        }
        _M0L5valueS1504 = _M0L8_2afieldS1760;
        _M0L6_2atmpS1503 = _M0L5valueS1504;
        return _M0L6_2atmpS1503;
      } else {
        moonbit_incref(_M0L8_2aentryS606);
      }
      _M0L3pslS1505 = _M0L8_2aentryS606->$2;
      moonbit_decref(_M0L8_2aentryS606);
      if (_M0L1iS601 > _M0L3pslS1505) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1506;
        moonbit_decref(_M0L4selfS604);
        _M0L6_2atmpS1506 = 0;
        return _M0L6_2atmpS1506;
      }
      _M0L6_2atmpS1507 = _M0L1iS601 + 1;
      _M0L6_2atmpS1509 = _M0L3idxS602 + 1;
      _M0L14capacity__maskS1510 = _M0L4selfS604->$3;
      _M0L6_2atmpS1508 = _M0L6_2atmpS1509 & _M0L14capacity__maskS1510;
      _M0L1iS601 = _M0L6_2atmpS1507;
      _M0L3idxS602 = _M0L6_2atmpS1508;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS613,
  moonbit_string_t _M0L3keyS609
) {
  int32_t _M0L4hashS608;
  int32_t _M0L14capacity__maskS1527;
  int32_t _M0L6_2atmpS1526;
  int32_t _M0L1iS610;
  int32_t _M0L3idxS611;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS609);
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS608 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS609);
  _M0L14capacity__maskS1527 = _M0L4selfS613->$3;
  _M0L6_2atmpS1526 = _M0L4hashS608 & _M0L14capacity__maskS1527;
  _M0L1iS610 = 0;
  _M0L3idxS611 = _M0L6_2atmpS1526;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1525 =
      _M0L4selfS613->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS612;
    if (
      _M0L3idxS611 < 0
      || _M0L3idxS611 >= Moonbit_array_length(_M0L7entriesS1525)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS612
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1525[
        _M0L3idxS611
      ];
    if (_M0L7_2abindS612 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1514;
      if (_M0L7_2abindS612) {
        moonbit_incref(_M0L7_2abindS612);
      }
      moonbit_decref(_M0L4selfS613);
      if (_M0L7_2abindS612) {
        moonbit_decref(_M0L7_2abindS612);
      }
      moonbit_decref(_M0L3keyS609);
      _M0L6_2atmpS1514 = 0;
      return _M0L6_2atmpS1514;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS614 =
        _M0L7_2abindS612;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS615 =
        _M0L7_2aSomeS614;
      int32_t _M0L4hashS1516 = _M0L8_2aentryS615->$3;
      int32_t _if__result_1980;
      int32_t _M0L3pslS1519;
      int32_t _M0L6_2atmpS1521;
      int32_t _M0L6_2atmpS1523;
      int32_t _M0L14capacity__maskS1524;
      int32_t _M0L6_2atmpS1522;
      if (_M0L4hashS1516 == _M0L4hashS608) {
        moonbit_string_t _M0L3keyS1515 = _M0L8_2aentryS615->$4;
        #line 220 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_1980
        = moonbit_val_array_equal(_M0L3keyS1515, _M0L3keyS609);
      } else {
        _if__result_1980 = 0;
      }
      if (_if__result_1980) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1763;
        int32_t _M0L6_2acntS1893;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1518;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1517;
        moonbit_incref(_M0L8_2aentryS615);
        moonbit_decref(_M0L4selfS613);
        moonbit_decref(_M0L3keyS609);
        _M0L8_2afieldS1763 = _M0L8_2aentryS615->$5;
        _M0L6_2acntS1893 = Moonbit_object_header(_M0L8_2aentryS615)->rc;
        if (_M0L6_2acntS1893 > 1) {
          int32_t _M0L11_2anew__cntS1896 = _M0L6_2acntS1893 - 1;
          Moonbit_object_header(_M0L8_2aentryS615)->rc
          = _M0L11_2anew__cntS1896;
          moonbit_incref(_M0L8_2afieldS1763);
        } else if (_M0L6_2acntS1893 == 1) {
          moonbit_string_t _M0L8_2afieldS1895 = _M0L8_2aentryS615->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1894;
          moonbit_decref(_M0L8_2afieldS1895);
          _M0L8_2afieldS1894 = _M0L8_2aentryS615->$1;
          if (_M0L8_2afieldS1894) {
            moonbit_decref(_M0L8_2afieldS1894);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS615);
        }
        _M0L5valueS1518 = _M0L8_2afieldS1763;
        _M0L6_2atmpS1517 = _M0L5valueS1518;
        return _M0L6_2atmpS1517;
      } else {
        moonbit_incref(_M0L8_2aentryS615);
      }
      _M0L3pslS1519 = _M0L8_2aentryS615->$2;
      moonbit_decref(_M0L8_2aentryS615);
      if (_M0L1iS610 > _M0L3pslS1519) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1520;
        moonbit_decref(_M0L4selfS613);
        moonbit_decref(_M0L3keyS609);
        _M0L6_2atmpS1520 = 0;
        return _M0L6_2atmpS1520;
      }
      _M0L6_2atmpS1521 = _M0L1iS610 + 1;
      _M0L6_2atmpS1523 = _M0L3idxS611 + 1;
      _M0L14capacity__maskS1524 = _M0L4selfS613->$3;
      _M0L6_2atmpS1522 = _M0L6_2atmpS1523 & _M0L14capacity__maskS1524;
      _M0L1iS610 = _M0L6_2atmpS1521;
      _M0L3idxS611 = _M0L6_2atmpS1522;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS584
) {
  int32_t _M0L6lengthS583;
  int32_t _M0Lm8capacityS585;
  int32_t _M0L6_2atmpS1477;
  int32_t _M0L6_2atmpS1476;
  int32_t _M0L6_2atmpS1487;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS586;
  int32_t _M0L3endS1485;
  int32_t _M0L5startS1486;
  int32_t _M0L7_2abindS587;
  int32_t _M0L2__S588;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS584.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS583
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS584);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS585 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS583);
  _M0L6_2atmpS1477 = _M0Lm8capacityS585;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1476 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1477);
  if (_M0L6lengthS583 > _M0L6_2atmpS1476) {
    int32_t _M0L6_2atmpS1478 = _M0Lm8capacityS585;
    _M0Lm8capacityS585 = _M0L6_2atmpS1478 * 2;
  }
  _M0L6_2atmpS1487 = _M0Lm8capacityS585;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS586
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1487);
  _M0L3endS1485 = _M0L3arrS584.$2;
  _M0L5startS1486 = _M0L3arrS584.$1;
  _M0L7_2abindS587 = _M0L3endS1485 - _M0L5startS1486;
  _M0L2__S588 = 0;
  while (1) {
    if (_M0L2__S588 < _M0L7_2abindS587) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1482 =
        _M0L3arrS584.$0;
      int32_t _M0L5startS1484 = _M0L3arrS584.$1;
      int32_t _M0L6_2atmpS1483 = _M0L5startS1484 + _M0L2__S588;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS589 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1482[
          _M0L6_2atmpS1483
        ];
      moonbit_string_t _M0L6_2atmpS1479 = _M0L1eS589->$0;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1480 =
        _M0L1eS589->$1;
      int32_t _M0L6_2atmpS1481;
      moonbit_incref(_M0L6_2atmpS1480);
      moonbit_incref(_M0L6_2atmpS1479);
      moonbit_incref(_M0L1mS586);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS586, _M0L6_2atmpS1479, _M0L6_2atmpS1480);
      _M0L6_2atmpS1481 = _M0L2__S588 + 1;
      _M0L2__S588 = _M0L6_2atmpS1481;
      continue;
    } else {
      moonbit_decref(_M0L3arrS584.$0);
    }
    break;
  }
  return _M0L1mS586;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS592
) {
  int32_t _M0L6lengthS591;
  int32_t _M0Lm8capacityS593;
  int32_t _M0L6_2atmpS1489;
  int32_t _M0L6_2atmpS1488;
  int32_t _M0L6_2atmpS1499;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS594;
  int32_t _M0L3endS1497;
  int32_t _M0L5startS1498;
  int32_t _M0L7_2abindS595;
  int32_t _M0L2__S596;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS592.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS591
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS592);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS593 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS591);
  _M0L6_2atmpS1489 = _M0Lm8capacityS593;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1488 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1489);
  if (_M0L6lengthS591 > _M0L6_2atmpS1488) {
    int32_t _M0L6_2atmpS1490 = _M0Lm8capacityS593;
    _M0Lm8capacityS593 = _M0L6_2atmpS1490 * 2;
  }
  _M0L6_2atmpS1499 = _M0Lm8capacityS593;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS594
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1499);
  _M0L3endS1497 = _M0L3arrS592.$2;
  _M0L5startS1498 = _M0L3arrS592.$1;
  _M0L7_2abindS595 = _M0L3endS1497 - _M0L5startS1498;
  _M0L2__S596 = 0;
  while (1) {
    if (_M0L2__S596 < _M0L7_2abindS595) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1494 =
        _M0L3arrS592.$0;
      int32_t _M0L5startS1496 = _M0L3arrS592.$1;
      int32_t _M0L6_2atmpS1495 = _M0L5startS1496 + _M0L2__S596;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS597 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1494[
          _M0L6_2atmpS1495
        ];
      int32_t _M0L6_2atmpS1491 = _M0L1eS597->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1492 = _M0L1eS597->$1;
      int32_t _M0L6_2atmpS1493;
      moonbit_incref(_M0L6_2atmpS1492);
      moonbit_incref(_M0L1mS594);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS594, _M0L6_2atmpS1491, _M0L6_2atmpS1492);
      _M0L6_2atmpS1493 = _M0L2__S596 + 1;
      _M0L2__S596 = _M0L6_2atmpS1493;
      continue;
    } else {
      moonbit_decref(_M0L3arrS592.$0);
    }
    break;
  }
  return _M0L1mS594;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS577,
  moonbit_string_t _M0L3keyS578,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS579
) {
  int32_t _M0L6_2atmpS1474;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS578);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1474 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS578);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS577, _M0L3keyS578, _M0L5valueS579, _M0L6_2atmpS1474);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS580,
  int32_t _M0L3keyS581,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS582
) {
  int32_t _M0L6_2atmpS1475;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1475 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS581);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS580, _M0L3keyS581, _M0L5valueS582, _M0L6_2atmpS1475);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS556
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS555;
  int32_t _M0L8capacityS1466;
  int32_t _M0L13new__capacityS557;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1461;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1460;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS1778;
  int32_t _M0L6_2atmpS1462;
  int32_t _M0L8capacityS1464;
  int32_t _M0L6_2atmpS1463;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1465;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1777;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1xS558;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS555 = _M0L4selfS556->$5;
  _M0L8capacityS1466 = _M0L4selfS556->$2;
  _M0L13new__capacityS557 = _M0L8capacityS1466 << 1;
  _M0L6_2atmpS1461 = 0;
  _M0L6_2atmpS1460
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS557, _M0L6_2atmpS1461);
  _M0L6_2aoldS1778 = _M0L4selfS556->$0;
  if (_M0L9old__headS555) {
    moonbit_incref(_M0L9old__headS555);
  }
  moonbit_decref(_M0L6_2aoldS1778);
  _M0L4selfS556->$0 = _M0L6_2atmpS1460;
  _M0L4selfS556->$2 = _M0L13new__capacityS557;
  _M0L6_2atmpS1462 = _M0L13new__capacityS557 - 1;
  _M0L4selfS556->$3 = _M0L6_2atmpS1462;
  _M0L8capacityS1464 = _M0L4selfS556->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1463 = _M0FPB21calc__grow__threshold(_M0L8capacityS1464);
  _M0L4selfS556->$4 = _M0L6_2atmpS1463;
  _M0L4selfS556->$1 = 0;
  _M0L6_2atmpS1465 = 0;
  _M0L6_2aoldS1777 = _M0L4selfS556->$5;
  if (_M0L6_2aoldS1777) {
    moonbit_decref(_M0L6_2aoldS1777);
  }
  _M0L4selfS556->$5 = _M0L6_2atmpS1465;
  _M0L4selfS556->$6 = -1;
  _M0L1xS558 = _M0L9old__headS555;
  while (1) {
    if (_M0L1xS558 == 0) {
      if (_M0L1xS558) {
        moonbit_decref(_M0L1xS558);
      }
      moonbit_decref(_M0L4selfS556);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS559 =
        _M0L1xS558;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS560 =
        _M0L7_2aSomeS559;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS561 =
        _M0L4_2axS560->$1;
      moonbit_string_t _M0L6_2akeyS562 = _M0L4_2axS560->$4;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS563 =
        _M0L4_2axS560->$5;
      int32_t _M0L7_2ahashS564 = _M0L4_2axS560->$3;
      int32_t _M0L6_2acntS1897 = Moonbit_object_header(_M0L4_2axS560)->rc;
      if (_M0L6_2acntS1897 > 1) {
        int32_t _M0L11_2anew__cntS1898 = _M0L6_2acntS1897 - 1;
        Moonbit_object_header(_M0L4_2axS560)->rc = _M0L11_2anew__cntS1898;
        moonbit_incref(_M0L8_2avalueS563);
        moonbit_incref(_M0L6_2akeyS562);
        if (_M0L7_2anextS561) {
          moonbit_incref(_M0L7_2anextS561);
        }
      } else if (_M0L6_2acntS1897 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS560);
      }
      moonbit_incref(_M0L4selfS556);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS556, _M0L6_2akeyS562, _M0L8_2avalueS563, _M0L7_2ahashS564);
      _M0L1xS558 = _M0L7_2anextS561;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS567
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS566;
  int32_t _M0L8capacityS1473;
  int32_t _M0L13new__capacityS568;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1468;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1467;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS1783;
  int32_t _M0L6_2atmpS1469;
  int32_t _M0L8capacityS1471;
  int32_t _M0L6_2atmpS1470;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1472;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1782;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L1xS569;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS566 = _M0L4selfS567->$5;
  _M0L8capacityS1473 = _M0L4selfS567->$2;
  _M0L13new__capacityS568 = _M0L8capacityS1473 << 1;
  _M0L6_2atmpS1468 = 0;
  _M0L6_2atmpS1467
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS568, _M0L6_2atmpS1468);
  _M0L6_2aoldS1783 = _M0L4selfS567->$0;
  if (_M0L9old__headS566) {
    moonbit_incref(_M0L9old__headS566);
  }
  moonbit_decref(_M0L6_2aoldS1783);
  _M0L4selfS567->$0 = _M0L6_2atmpS1467;
  _M0L4selfS567->$2 = _M0L13new__capacityS568;
  _M0L6_2atmpS1469 = _M0L13new__capacityS568 - 1;
  _M0L4selfS567->$3 = _M0L6_2atmpS1469;
  _M0L8capacityS1471 = _M0L4selfS567->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1470 = _M0FPB21calc__grow__threshold(_M0L8capacityS1471);
  _M0L4selfS567->$4 = _M0L6_2atmpS1470;
  _M0L4selfS567->$1 = 0;
  _M0L6_2atmpS1472 = 0;
  _M0L6_2aoldS1782 = _M0L4selfS567->$5;
  if (_M0L6_2aoldS1782) {
    moonbit_decref(_M0L6_2aoldS1782);
  }
  _M0L4selfS567->$5 = _M0L6_2atmpS1472;
  _M0L4selfS567->$6 = -1;
  _M0L1xS569 = _M0L9old__headS566;
  while (1) {
    if (_M0L1xS569 == 0) {
      if (_M0L1xS569) {
        moonbit_decref(_M0L1xS569);
      }
      moonbit_decref(_M0L4selfS567);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS570 =
        _M0L1xS569;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS571 =
        _M0L7_2aSomeS570;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS572 =
        _M0L4_2axS571->$1;
      int32_t _M0L6_2akeyS573 = _M0L4_2axS571->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS574 =
        _M0L4_2axS571->$5;
      int32_t _M0L7_2ahashS575 = _M0L4_2axS571->$3;
      int32_t _M0L6_2acntS1899 = Moonbit_object_header(_M0L4_2axS571)->rc;
      if (_M0L6_2acntS1899 > 1) {
        int32_t _M0L11_2anew__cntS1900 = _M0L6_2acntS1899 - 1;
        Moonbit_object_header(_M0L4_2axS571)->rc = _M0L11_2anew__cntS1900;
        moonbit_incref(_M0L8_2avalueS574);
        if (_M0L7_2anextS572) {
          moonbit_incref(_M0L7_2anextS572);
        }
      } else if (_M0L6_2acntS1899 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS571);
      }
      moonbit_incref(_M0L4selfS567);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS567, _M0L6_2akeyS573, _M0L8_2avalueS574, _M0L7_2ahashS575);
      _M0L1xS569 = _M0L7_2anextS572;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS526,
  moonbit_string_t _M0L3keyS532,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS533,
  int32_t _M0L4hashS528
) {
  int32_t _M0L14capacity__maskS1441;
  int32_t _M0L6_2atmpS1440;
  int32_t _M0L3pslS523;
  int32_t _M0L3idxS524;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1441 = _M0L4selfS526->$3;
  _M0L6_2atmpS1440 = _M0L4hashS528 & _M0L14capacity__maskS1441;
  _M0L3pslS523 = 0;
  _M0L3idxS524 = _M0L6_2atmpS1440;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1439 =
      _M0L4selfS526->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS525;
    if (
      _M0L3idxS524 < 0
      || _M0L3idxS524 >= Moonbit_array_length(_M0L7entriesS1439)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS525
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1439[
        _M0L3idxS524
      ];
    if (_M0L7_2abindS525 == 0) {
      int32_t _M0L4sizeS1424 = _M0L4selfS526->$1;
      int32_t _M0L8grow__atS1425 = _M0L4selfS526->$4;
      int32_t _M0L7_2abindS529;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS530;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS531;
      if (_M0L4sizeS1424 >= _M0L8grow__atS1425) {
        int32_t _M0L14capacity__maskS1427;
        int32_t _M0L6_2atmpS1426;
        moonbit_incref(_M0L4selfS526);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526);
        _M0L14capacity__maskS1427 = _M0L4selfS526->$3;
        _M0L6_2atmpS1426 = _M0L4hashS528 & _M0L14capacity__maskS1427;
        _M0L3pslS523 = 0;
        _M0L3idxS524 = _M0L6_2atmpS1426;
        continue;
      }
      _M0L7_2abindS529 = _M0L4selfS526->$6;
      _M0L7_2abindS530 = 0;
      _M0L5entryS531
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS531)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS531->$0 = _M0L7_2abindS529;
      _M0L5entryS531->$1 = _M0L7_2abindS530;
      _M0L5entryS531->$2 = _M0L3pslS523;
      _M0L5entryS531->$3 = _M0L4hashS528;
      _M0L5entryS531->$4 = _M0L3keyS532;
      _M0L5entryS531->$5 = _M0L5valueS533;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526, _M0L3idxS524, _M0L5entryS531);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS534 =
        _M0L7_2abindS525;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS535 =
        _M0L7_2aSomeS534;
      int32_t _M0L4hashS1429 = _M0L14_2acurr__entryS535->$3;
      int32_t _if__result_1986;
      int32_t _M0L3pslS1430;
      int32_t _M0L6_2atmpS1435;
      int32_t _M0L6_2atmpS1437;
      int32_t _M0L14capacity__maskS1438;
      int32_t _M0L6_2atmpS1436;
      if (_M0L4hashS1429 == _M0L4hashS528) {
        moonbit_string_t _M0L3keyS1428 = _M0L14_2acurr__entryS535->$4;
        #line 134 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_1986
        = moonbit_val_array_equal(_M0L3keyS1428, _M0L3keyS532);
      } else {
        _if__result_1986 = 0;
      }
      if (_if__result_1986) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1785;
        moonbit_incref(_M0L14_2acurr__entryS535);
        moonbit_decref(_M0L3keyS532);
        moonbit_decref(_M0L4selfS526);
        _M0L6_2aoldS1785 = _M0L14_2acurr__entryS535->$5;
        moonbit_decref(_M0L6_2aoldS1785);
        _M0L14_2acurr__entryS535->$5 = _M0L5valueS533;
        moonbit_decref(_M0L14_2acurr__entryS535);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS535);
      }
      _M0L3pslS1430 = _M0L14_2acurr__entryS535->$2;
      if (_M0L3pslS523 > _M0L3pslS1430) {
        int32_t _M0L4sizeS1431 = _M0L4selfS526->$1;
        int32_t _M0L8grow__atS1432 = _M0L4selfS526->$4;
        int32_t _M0L7_2abindS536;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS537;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS538;
        if (_M0L4sizeS1431 >= _M0L8grow__atS1432) {
          int32_t _M0L14capacity__maskS1434;
          int32_t _M0L6_2atmpS1433;
          moonbit_decref(_M0L14_2acurr__entryS535);
          moonbit_incref(_M0L4selfS526);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526);
          _M0L14capacity__maskS1434 = _M0L4selfS526->$3;
          _M0L6_2atmpS1433 = _M0L4hashS528 & _M0L14capacity__maskS1434;
          _M0L3pslS523 = 0;
          _M0L3idxS524 = _M0L6_2atmpS1433;
          continue;
        }
        moonbit_incref(_M0L4selfS526);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526, _M0L3idxS524, _M0L14_2acurr__entryS535);
        _M0L7_2abindS536 = _M0L4selfS526->$6;
        _M0L7_2abindS537 = 0;
        _M0L5entryS538
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS538)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS538->$0 = _M0L7_2abindS536;
        _M0L5entryS538->$1 = _M0L7_2abindS537;
        _M0L5entryS538->$2 = _M0L3pslS523;
        _M0L5entryS538->$3 = _M0L4hashS528;
        _M0L5entryS538->$4 = _M0L3keyS532;
        _M0L5entryS538->$5 = _M0L5valueS533;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526, _M0L3idxS524, _M0L5entryS538);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS535);
      }
      _M0L6_2atmpS1435 = _M0L3pslS523 + 1;
      _M0L6_2atmpS1437 = _M0L3idxS524 + 1;
      _M0L14capacity__maskS1438 = _M0L4selfS526->$3;
      _M0L6_2atmpS1436 = _M0L6_2atmpS1437 & _M0L14capacity__maskS1438;
      _M0L3pslS523 = _M0L6_2atmpS1435;
      _M0L3idxS524 = _M0L6_2atmpS1436;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS542,
  int32_t _M0L3keyS548,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS549,
  int32_t _M0L4hashS544
) {
  int32_t _M0L14capacity__maskS1459;
  int32_t _M0L6_2atmpS1458;
  int32_t _M0L3pslS539;
  int32_t _M0L3idxS540;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1459 = _M0L4selfS542->$3;
  _M0L6_2atmpS1458 = _M0L4hashS544 & _M0L14capacity__maskS1459;
  _M0L3pslS539 = 0;
  _M0L3idxS540 = _M0L6_2atmpS1458;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1457 =
      _M0L4selfS542->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS541;
    if (
      _M0L3idxS540 < 0
      || _M0L3idxS540 >= Moonbit_array_length(_M0L7entriesS1457)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS541
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1457[
        _M0L3idxS540
      ];
    if (_M0L7_2abindS541 == 0) {
      int32_t _M0L4sizeS1442 = _M0L4selfS542->$1;
      int32_t _M0L8grow__atS1443 = _M0L4selfS542->$4;
      int32_t _M0L7_2abindS545;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS546;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS547;
      if (_M0L4sizeS1442 >= _M0L8grow__atS1443) {
        int32_t _M0L14capacity__maskS1445;
        int32_t _M0L6_2atmpS1444;
        moonbit_incref(_M0L4selfS542);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS542);
        _M0L14capacity__maskS1445 = _M0L4selfS542->$3;
        _M0L6_2atmpS1444 = _M0L4hashS544 & _M0L14capacity__maskS1445;
        _M0L3pslS539 = 0;
        _M0L3idxS540 = _M0L6_2atmpS1444;
        continue;
      }
      _M0L7_2abindS545 = _M0L4selfS542->$6;
      _M0L7_2abindS546 = 0;
      _M0L5entryS547
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS547)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS547->$0 = _M0L7_2abindS545;
      _M0L5entryS547->$1 = _M0L7_2abindS546;
      _M0L5entryS547->$2 = _M0L3pslS539;
      _M0L5entryS547->$3 = _M0L4hashS544;
      _M0L5entryS547->$4 = _M0L3keyS548;
      _M0L5entryS547->$5 = _M0L5valueS549;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS542, _M0L3idxS540, _M0L5entryS547);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS550 =
        _M0L7_2abindS541;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS551 =
        _M0L7_2aSomeS550;
      int32_t _M0L4hashS1447 = _M0L14_2acurr__entryS551->$3;
      int32_t _if__result_1988;
      int32_t _M0L3pslS1448;
      int32_t _M0L6_2atmpS1453;
      int32_t _M0L6_2atmpS1455;
      int32_t _M0L14capacity__maskS1456;
      int32_t _M0L6_2atmpS1454;
      if (_M0L4hashS1447 == _M0L4hashS544) {
        int32_t _M0L3keyS1446 = _M0L14_2acurr__entryS551->$4;
        _if__result_1988 = _M0L3keyS1446 == _M0L3keyS548;
      } else {
        _if__result_1988 = 0;
      }
      if (_if__result_1988) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS1789;
        moonbit_incref(_M0L14_2acurr__entryS551);
        moonbit_decref(_M0L4selfS542);
        _M0L6_2aoldS1789 = _M0L14_2acurr__entryS551->$5;
        moonbit_decref(_M0L6_2aoldS1789);
        _M0L14_2acurr__entryS551->$5 = _M0L5valueS549;
        moonbit_decref(_M0L14_2acurr__entryS551);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS551);
      }
      _M0L3pslS1448 = _M0L14_2acurr__entryS551->$2;
      if (_M0L3pslS539 > _M0L3pslS1448) {
        int32_t _M0L4sizeS1449 = _M0L4selfS542->$1;
        int32_t _M0L8grow__atS1450 = _M0L4selfS542->$4;
        int32_t _M0L7_2abindS552;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS553;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS554;
        if (_M0L4sizeS1449 >= _M0L8grow__atS1450) {
          int32_t _M0L14capacity__maskS1452;
          int32_t _M0L6_2atmpS1451;
          moonbit_decref(_M0L14_2acurr__entryS551);
          moonbit_incref(_M0L4selfS542);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS542);
          _M0L14capacity__maskS1452 = _M0L4selfS542->$3;
          _M0L6_2atmpS1451 = _M0L4hashS544 & _M0L14capacity__maskS1452;
          _M0L3pslS539 = 0;
          _M0L3idxS540 = _M0L6_2atmpS1451;
          continue;
        }
        moonbit_incref(_M0L4selfS542);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS542, _M0L3idxS540, _M0L14_2acurr__entryS551);
        _M0L7_2abindS552 = _M0L4selfS542->$6;
        _M0L7_2abindS553 = 0;
        _M0L5entryS554
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS554)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS554->$0 = _M0L7_2abindS552;
        _M0L5entryS554->$1 = _M0L7_2abindS553;
        _M0L5entryS554->$2 = _M0L3pslS539;
        _M0L5entryS554->$3 = _M0L4hashS544;
        _M0L5entryS554->$4 = _M0L3keyS548;
        _M0L5entryS554->$5 = _M0L5valueS549;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS542, _M0L3idxS540, _M0L5entryS554);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS551);
      }
      _M0L6_2atmpS1453 = _M0L3pslS539 + 1;
      _M0L6_2atmpS1455 = _M0L3idxS540 + 1;
      _M0L14capacity__maskS1456 = _M0L4selfS542->$3;
      _M0L6_2atmpS1454 = _M0L6_2atmpS1455 & _M0L14capacity__maskS1456;
      _M0L3pslS539 = _M0L6_2atmpS1453;
      _M0L3idxS540 = _M0L6_2atmpS1454;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS507,
  int32_t _M0L3idxS512,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS511
) {
  int32_t _M0L3pslS1407;
  int32_t _M0L6_2atmpS1403;
  int32_t _M0L6_2atmpS1405;
  int32_t _M0L14capacity__maskS1406;
  int32_t _M0L6_2atmpS1404;
  int32_t _M0L3pslS503;
  int32_t _M0L3idxS504;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS505;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1407 = _M0L5entryS511->$2;
  _M0L6_2atmpS1403 = _M0L3pslS1407 + 1;
  _M0L6_2atmpS1405 = _M0L3idxS512 + 1;
  _M0L14capacity__maskS1406 = _M0L4selfS507->$3;
  _M0L6_2atmpS1404 = _M0L6_2atmpS1405 & _M0L14capacity__maskS1406;
  _M0L3pslS503 = _M0L6_2atmpS1403;
  _M0L3idxS504 = _M0L6_2atmpS1404;
  _M0L5entryS505 = _M0L5entryS511;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1402 =
      _M0L4selfS507->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS506;
    if (
      _M0L3idxS504 < 0
      || _M0L3idxS504 >= Moonbit_array_length(_M0L7entriesS1402)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS506
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1402[
        _M0L3idxS504
      ];
    if (_M0L7_2abindS506 == 0) {
      _M0L5entryS505->$2 = _M0L3pslS503;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS507, _M0L5entryS505, _M0L3idxS504);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS509 =
        _M0L7_2abindS506;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS510 =
        _M0L7_2aSomeS509;
      int32_t _M0L3pslS1392 = _M0L14_2acurr__entryS510->$2;
      if (_M0L3pslS503 > _M0L3pslS1392) {
        int32_t _M0L3pslS1397;
        int32_t _M0L6_2atmpS1393;
        int32_t _M0L6_2atmpS1395;
        int32_t _M0L14capacity__maskS1396;
        int32_t _M0L6_2atmpS1394;
        _M0L5entryS505->$2 = _M0L3pslS503;
        moonbit_incref(_M0L14_2acurr__entryS510);
        moonbit_incref(_M0L4selfS507);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS507, _M0L5entryS505, _M0L3idxS504);
        _M0L3pslS1397 = _M0L14_2acurr__entryS510->$2;
        _M0L6_2atmpS1393 = _M0L3pslS1397 + 1;
        _M0L6_2atmpS1395 = _M0L3idxS504 + 1;
        _M0L14capacity__maskS1396 = _M0L4selfS507->$3;
        _M0L6_2atmpS1394 = _M0L6_2atmpS1395 & _M0L14capacity__maskS1396;
        _M0L3pslS503 = _M0L6_2atmpS1393;
        _M0L3idxS504 = _M0L6_2atmpS1394;
        _M0L5entryS505 = _M0L14_2acurr__entryS510;
        continue;
      } else {
        int32_t _M0L6_2atmpS1398 = _M0L3pslS503 + 1;
        int32_t _M0L6_2atmpS1400 = _M0L3idxS504 + 1;
        int32_t _M0L14capacity__maskS1401 = _M0L4selfS507->$3;
        int32_t _M0L6_2atmpS1399 =
          _M0L6_2atmpS1400 & _M0L14capacity__maskS1401;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_1990 =
          _M0L5entryS505;
        _M0L3pslS503 = _M0L6_2atmpS1398;
        _M0L3idxS504 = _M0L6_2atmpS1399;
        _M0L5entryS505 = _tmp_1990;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS517,
  int32_t _M0L3idxS522,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS521
) {
  int32_t _M0L3pslS1423;
  int32_t _M0L6_2atmpS1419;
  int32_t _M0L6_2atmpS1421;
  int32_t _M0L14capacity__maskS1422;
  int32_t _M0L6_2atmpS1420;
  int32_t _M0L3pslS513;
  int32_t _M0L3idxS514;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS515;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1423 = _M0L5entryS521->$2;
  _M0L6_2atmpS1419 = _M0L3pslS1423 + 1;
  _M0L6_2atmpS1421 = _M0L3idxS522 + 1;
  _M0L14capacity__maskS1422 = _M0L4selfS517->$3;
  _M0L6_2atmpS1420 = _M0L6_2atmpS1421 & _M0L14capacity__maskS1422;
  _M0L3pslS513 = _M0L6_2atmpS1419;
  _M0L3idxS514 = _M0L6_2atmpS1420;
  _M0L5entryS515 = _M0L5entryS521;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1418 =
      _M0L4selfS517->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS516;
    if (
      _M0L3idxS514 < 0
      || _M0L3idxS514 >= Moonbit_array_length(_M0L7entriesS1418)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS516
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1418[
        _M0L3idxS514
      ];
    if (_M0L7_2abindS516 == 0) {
      _M0L5entryS515->$2 = _M0L3pslS513;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS517, _M0L5entryS515, _M0L3idxS514);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS519 =
        _M0L7_2abindS516;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS520 =
        _M0L7_2aSomeS519;
      int32_t _M0L3pslS1408 = _M0L14_2acurr__entryS520->$2;
      if (_M0L3pslS513 > _M0L3pslS1408) {
        int32_t _M0L3pslS1413;
        int32_t _M0L6_2atmpS1409;
        int32_t _M0L6_2atmpS1411;
        int32_t _M0L14capacity__maskS1412;
        int32_t _M0L6_2atmpS1410;
        _M0L5entryS515->$2 = _M0L3pslS513;
        moonbit_incref(_M0L14_2acurr__entryS520);
        moonbit_incref(_M0L4selfS517);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS517, _M0L5entryS515, _M0L3idxS514);
        _M0L3pslS1413 = _M0L14_2acurr__entryS520->$2;
        _M0L6_2atmpS1409 = _M0L3pslS1413 + 1;
        _M0L6_2atmpS1411 = _M0L3idxS514 + 1;
        _M0L14capacity__maskS1412 = _M0L4selfS517->$3;
        _M0L6_2atmpS1410 = _M0L6_2atmpS1411 & _M0L14capacity__maskS1412;
        _M0L3pslS513 = _M0L6_2atmpS1409;
        _M0L3idxS514 = _M0L6_2atmpS1410;
        _M0L5entryS515 = _M0L14_2acurr__entryS520;
        continue;
      } else {
        int32_t _M0L6_2atmpS1414 = _M0L3pslS513 + 1;
        int32_t _M0L6_2atmpS1416 = _M0L3idxS514 + 1;
        int32_t _M0L14capacity__maskS1417 = _M0L4selfS517->$3;
        int32_t _M0L6_2atmpS1415 =
          _M0L6_2atmpS1416 & _M0L14capacity__maskS1417;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_1992 =
          _M0L5entryS515;
        _M0L3pslS513 = _M0L6_2atmpS1414;
        _M0L3idxS514 = _M0L6_2atmpS1415;
        _M0L5entryS515 = _tmp_1992;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS491,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS493,
  int32_t _M0L8new__idxS492
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1388;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1389;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1797;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1796;
  int32_t _M0L6_2acntS1901;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS494;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1388 = _M0L4selfS491->$0;
  moonbit_incref(_M0L5entryS493);
  _M0L6_2atmpS1389 = _M0L5entryS493;
  if (
    _M0L8new__idxS492 < 0
    || _M0L8new__idxS492 >= Moonbit_array_length(_M0L7entriesS1388)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1797
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1388[
      _M0L8new__idxS492
    ];
  if (_M0L6_2aoldS1797) {
    moonbit_decref(_M0L6_2aoldS1797);
  }
  _M0L7entriesS1388[_M0L8new__idxS492] = _M0L6_2atmpS1389;
  _M0L8_2afieldS1796 = _M0L5entryS493->$1;
  _M0L6_2acntS1901 = Moonbit_object_header(_M0L5entryS493)->rc;
  if (_M0L6_2acntS1901 > 1) {
    int32_t _M0L11_2anew__cntS1904 = _M0L6_2acntS1901 - 1;
    Moonbit_object_header(_M0L5entryS493)->rc = _M0L11_2anew__cntS1904;
    if (_M0L8_2afieldS1796) {
      moonbit_incref(_M0L8_2afieldS1796);
    }
  } else if (_M0L6_2acntS1901 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1903 =
      _M0L5entryS493->$5;
    moonbit_string_t _M0L8_2afieldS1902;
    moonbit_decref(_M0L8_2afieldS1903);
    _M0L8_2afieldS1902 = _M0L5entryS493->$4;
    moonbit_decref(_M0L8_2afieldS1902);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS493);
  }
  _M0L7_2abindS494 = _M0L8_2afieldS1796;
  if (_M0L7_2abindS494 == 0) {
    if (_M0L7_2abindS494) {
      moonbit_decref(_M0L7_2abindS494);
    }
    _M0L4selfS491->$6 = _M0L8new__idxS492;
    moonbit_decref(_M0L4selfS491);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS495;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS496;
    moonbit_decref(_M0L4selfS491);
    _M0L7_2aSomeS495 = _M0L7_2abindS494;
    _M0L7_2anextS496 = _M0L7_2aSomeS495;
    _M0L7_2anextS496->$0 = _M0L8new__idxS492;
    moonbit_decref(_M0L7_2anextS496);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS497,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS499,
  int32_t _M0L8new__idxS498
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1390;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1391;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1800;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1799;
  int32_t _M0L6_2acntS1905;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS500;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1390 = _M0L4selfS497->$0;
  moonbit_incref(_M0L5entryS499);
  _M0L6_2atmpS1391 = _M0L5entryS499;
  if (
    _M0L8new__idxS498 < 0
    || _M0L8new__idxS498 >= Moonbit_array_length(_M0L7entriesS1390)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1800
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1390[
      _M0L8new__idxS498
    ];
  if (_M0L6_2aoldS1800) {
    moonbit_decref(_M0L6_2aoldS1800);
  }
  _M0L7entriesS1390[_M0L8new__idxS498] = _M0L6_2atmpS1391;
  _M0L8_2afieldS1799 = _M0L5entryS499->$1;
  _M0L6_2acntS1905 = Moonbit_object_header(_M0L5entryS499)->rc;
  if (_M0L6_2acntS1905 > 1) {
    int32_t _M0L11_2anew__cntS1907 = _M0L6_2acntS1905 - 1;
    Moonbit_object_header(_M0L5entryS499)->rc = _M0L11_2anew__cntS1907;
    if (_M0L8_2afieldS1799) {
      moonbit_incref(_M0L8_2afieldS1799);
    }
  } else if (_M0L6_2acntS1905 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1906 =
      _M0L5entryS499->$5;
    moonbit_decref(_M0L8_2afieldS1906);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS499);
  }
  _M0L7_2abindS500 = _M0L8_2afieldS1799;
  if (_M0L7_2abindS500 == 0) {
    if (_M0L7_2abindS500) {
      moonbit_decref(_M0L7_2abindS500);
    }
    _M0L4selfS497->$6 = _M0L8new__idxS498;
    moonbit_decref(_M0L4selfS497);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS501;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS502;
    moonbit_decref(_M0L4selfS497);
    _M0L7_2aSomeS501 = _M0L7_2abindS500;
    _M0L7_2anextS502 = _M0L7_2aSomeS501;
    _M0L7_2anextS502->$0 = _M0L8new__idxS498;
    moonbit_decref(_M0L7_2anextS502);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS484,
  int32_t _M0L3idxS486,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS485
) {
  int32_t _M0L7_2abindS483;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1375;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1376;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1802;
  int32_t _M0L4sizeS1378;
  int32_t _M0L6_2atmpS1377;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS483 = _M0L4selfS484->$6;
  switch (_M0L7_2abindS483) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1370;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1804;
      moonbit_incref(_M0L5entryS485);
      _M0L6_2atmpS1370 = _M0L5entryS485;
      _M0L6_2aoldS1804 = _M0L4selfS484->$5;
      if (_M0L6_2aoldS1804) {
        moonbit_decref(_M0L6_2aoldS1804);
      }
      _M0L4selfS484->$5 = _M0L6_2atmpS1370;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1374 =
        _M0L4selfS484->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1373;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1371;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1372;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1805;
      if (
        _M0L7_2abindS483 < 0
        || _M0L7_2abindS483 >= Moonbit_array_length(_M0L7entriesS1374)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1373
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1374[
          _M0L7_2abindS483
        ];
      if (_M0L6_2atmpS1373) {
        moonbit_incref(_M0L6_2atmpS1373);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1371
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1373);
      moonbit_incref(_M0L5entryS485);
      _M0L6_2atmpS1372 = _M0L5entryS485;
      _M0L6_2aoldS1805 = _M0L6_2atmpS1371->$1;
      if (_M0L6_2aoldS1805) {
        moonbit_decref(_M0L6_2aoldS1805);
      }
      _M0L6_2atmpS1371->$1 = _M0L6_2atmpS1372;
      moonbit_decref(_M0L6_2atmpS1371);
      break;
    }
  }
  _M0L4selfS484->$6 = _M0L3idxS486;
  _M0L7entriesS1375 = _M0L4selfS484->$0;
  _M0L6_2atmpS1376 = _M0L5entryS485;
  if (
    _M0L3idxS486 < 0
    || _M0L3idxS486 >= Moonbit_array_length(_M0L7entriesS1375)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1802
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1375[
      _M0L3idxS486
    ];
  if (_M0L6_2aoldS1802) {
    moonbit_decref(_M0L6_2aoldS1802);
  }
  _M0L7entriesS1375[_M0L3idxS486] = _M0L6_2atmpS1376;
  _M0L4sizeS1378 = _M0L4selfS484->$1;
  _M0L6_2atmpS1377 = _M0L4sizeS1378 + 1;
  _M0L4selfS484->$1 = _M0L6_2atmpS1377;
  moonbit_decref(_M0L4selfS484);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS488,
  int32_t _M0L3idxS490,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS489
) {
  int32_t _M0L7_2abindS487;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1384;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1385;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1808;
  int32_t _M0L4sizeS1387;
  int32_t _M0L6_2atmpS1386;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS487 = _M0L4selfS488->$6;
  switch (_M0L7_2abindS487) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1379;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1810;
      moonbit_incref(_M0L5entryS489);
      _M0L6_2atmpS1379 = _M0L5entryS489;
      _M0L6_2aoldS1810 = _M0L4selfS488->$5;
      if (_M0L6_2aoldS1810) {
        moonbit_decref(_M0L6_2aoldS1810);
      }
      _M0L4selfS488->$5 = _M0L6_2atmpS1379;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1383 =
        _M0L4selfS488->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1382;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1380;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1381;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1811;
      if (
        _M0L7_2abindS487 < 0
        || _M0L7_2abindS487 >= Moonbit_array_length(_M0L7entriesS1383)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1382
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1383[
          _M0L7_2abindS487
        ];
      if (_M0L6_2atmpS1382) {
        moonbit_incref(_M0L6_2atmpS1382);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1380
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1382);
      moonbit_incref(_M0L5entryS489);
      _M0L6_2atmpS1381 = _M0L5entryS489;
      _M0L6_2aoldS1811 = _M0L6_2atmpS1380->$1;
      if (_M0L6_2aoldS1811) {
        moonbit_decref(_M0L6_2aoldS1811);
      }
      _M0L6_2atmpS1380->$1 = _M0L6_2atmpS1381;
      moonbit_decref(_M0L6_2atmpS1380);
      break;
    }
  }
  _M0L4selfS488->$6 = _M0L3idxS490;
  _M0L7entriesS1384 = _M0L4selfS488->$0;
  _M0L6_2atmpS1385 = _M0L5entryS489;
  if (
    _M0L3idxS490 < 0
    || _M0L3idxS490 >= Moonbit_array_length(_M0L7entriesS1384)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1808
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1384[
      _M0L3idxS490
    ];
  if (_M0L6_2aoldS1808) {
    moonbit_decref(_M0L6_2aoldS1808);
  }
  _M0L7entriesS1384[_M0L3idxS490] = _M0L6_2atmpS1385;
  _M0L4sizeS1387 = _M0L4selfS488->$1;
  _M0L6_2atmpS1386 = _M0L4sizeS1387 + 1;
  _M0L4selfS488->$1 = _M0L6_2atmpS1386;
  moonbit_decref(_M0L4selfS488);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS472
) {
  int32_t _M0L8capacityS471;
  int32_t _M0L7_2abindS473;
  int32_t _M0L7_2abindS474;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1368;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS475;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS476;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_1993;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS471
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS472);
  _M0L7_2abindS473 = _M0L8capacityS471 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS474 = _M0FPB21calc__grow__threshold(_M0L8capacityS471);
  _M0L6_2atmpS1368 = 0;
  _M0L7_2abindS475
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS471, _M0L6_2atmpS1368);
  _M0L7_2abindS476 = 0;
  _block_1993
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_1993)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_1993->$0 = _M0L7_2abindS475;
  _block_1993->$1 = 0;
  _block_1993->$2 = _M0L8capacityS471;
  _block_1993->$3 = _M0L7_2abindS473;
  _block_1993->$4 = _M0L7_2abindS474;
  _block_1993->$5 = _M0L7_2abindS476;
  _block_1993->$6 = -1;
  return _block_1993;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS478
) {
  int32_t _M0L8capacityS477;
  int32_t _M0L7_2abindS479;
  int32_t _M0L7_2abindS480;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1369;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS481;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS482;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_1994;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS477
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS478);
  _M0L7_2abindS479 = _M0L8capacityS477 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS480 = _M0FPB21calc__grow__threshold(_M0L8capacityS477);
  _M0L6_2atmpS1369 = 0;
  _M0L7_2abindS481
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS477, _M0L6_2atmpS1369);
  _M0L7_2abindS482 = 0;
  _block_1994
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_1994)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_1994->$0 = _M0L7_2abindS481;
  _block_1994->$1 = 0;
  _block_1994->$2 = _M0L8capacityS477;
  _block_1994->$3 = _M0L7_2abindS479;
  _block_1994->$4 = _M0L7_2abindS480;
  _block_1994->$5 = _M0L7_2abindS482;
  _block_1994->$6 = -1;
  return _block_1994;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS470) {
  #line 33 "/Users/user/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS470 >= 0) {
    int32_t _M0L6_2atmpS1367;
    int32_t _M0L6_2atmpS1366;
    int32_t _M0L6_2atmpS1365;
    int32_t _M0L6_2atmpS1364;
    if (_M0L4selfS470 <= 1) {
      return 1;
    }
    if (_M0L4selfS470 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1367 = _M0L4selfS470 - 1;
    #line 44 "/Users/user/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS1366 = moonbit_clz32(_M0L6_2atmpS1367);
    _M0L6_2atmpS1365 = _M0L6_2atmpS1366 - 1;
    _M0L6_2atmpS1364 = 2147483647 >> (_M0L6_2atmpS1365 & 31);
    return _M0L6_2atmpS1364 + 1;
  } else {
    #line 34 "/Users/user/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS469) {
  int32_t _M0L6_2atmpS1363;
  #line 510 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1363 = _M0L8capacityS469 * 13;
  return _M0L6_2atmpS1363 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS465
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS465 == 0) {
    if (_M0L4selfS465) {
      moonbit_decref(_M0L4selfS465);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS466 =
      _M0L4selfS465;
    return _M0L7_2aSomeS466;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS467
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS467 == 0) {
    if (_M0L4selfS467) {
      moonbit_decref(_M0L4selfS467);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS468 =
      _M0L4selfS467;
    return _M0L7_2aSomeS468;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS464
) {
  moonbit_string_t* _M0L6_2atmpS1362;
  #line 165 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS1362 = _M0L4selfS464;
  #line 167 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1362);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS463
) {
  moonbit_string_t* _M0L6_2atmpS1360;
  int32_t _M0L6_2atmpS1361;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1359;
  #line 1509 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS463);
  _M0L6_2atmpS1360 = _M0L4selfS463;
  _M0L6_2atmpS1361 = Moonbit_array_length(_M0L4selfS463);
  moonbit_decref(_M0L4selfS463);
  _M0L6_2atmpS1359
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1361, _M0L6_2atmpS1360
  };
  #line 1511 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1359);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS461
) {
  struct _M0TPB8MutLocalGiE* _M0L1iS460;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__* _closure_1995;
  struct _M0TWEOs* _M0L6_2atmpS1347;
  #line 677 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS460
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS460)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS460->$0 = 0;
  _closure_1995
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__));
  Moonbit_object_header(_closure_1995)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__, $0_0) >> 2, 2, 0);
  _closure_1995->code = &_M0MPC15array9ArrayView4iterGsEC1348l680;
  _closure_1995->$0_0 = _M0L4selfS461.$0;
  _closure_1995->$0_1 = _M0L4selfS461.$1;
  _closure_1995->$0_2 = _M0L4selfS461.$2;
  _closure_1995->$1 = _M0L1iS460;
  _M0L6_2atmpS1347 = (struct _M0TWEOs*)_closure_1995;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1347);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1348l680(
  struct _M0TWEOs* _M0L6_2aenvS1349
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__* _M0L14_2acasted__envS1350;
  struct _M0TPB8MutLocalGiE* _M0L1iS460;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS1816;
  int32_t _M0L6_2acntS1908;
  struct _M0TPB9ArrayViewGsE _M0L4selfS461;
  int32_t _M0L3valS1351;
  int32_t _M0L6_2atmpS1352;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1350
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1348__l680__*)_M0L6_2aenvS1349;
  _M0L1iS460 = _M0L14_2acasted__envS1350->$1;
  _M0L8_2afieldS1816
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1350->$0_1,
      _M0L14_2acasted__envS1350->$0_2,
      _M0L14_2acasted__envS1350->$0_0
  };
  _M0L6_2acntS1908 = Moonbit_object_header(_M0L14_2acasted__envS1350)->rc;
  if (_M0L6_2acntS1908 > 1) {
    int32_t _M0L11_2anew__cntS1909 = _M0L6_2acntS1908 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1350)->rc
    = _M0L11_2anew__cntS1909;
    moonbit_incref(_M0L1iS460);
    moonbit_incref(_M0L8_2afieldS1816.$0);
  } else if (_M0L6_2acntS1908 == 1) {
    #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1350);
  }
  _M0L4selfS461 = _M0L8_2afieldS1816;
  _M0L3valS1351 = _M0L1iS460->$0;
  moonbit_incref(_M0L4selfS461.$0);
  #line 681 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1352 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS461);
  if (_M0L3valS1351 < _M0L6_2atmpS1352) {
    moonbit_string_t* _M0L3bufS1355 = _M0L4selfS461.$0;
    int32_t _M0L5startS1357 = _M0L4selfS461.$1;
    int32_t _M0L3valS1358 = _M0L1iS460->$0;
    int32_t _M0L6_2atmpS1356 = _M0L5startS1357 + _M0L3valS1358;
    moonbit_string_t _M0L6_2atmpS1814 =
      (moonbit_string_t)_M0L3bufS1355[_M0L6_2atmpS1356];
    moonbit_string_t _M0L4elemS462;
    int32_t _M0L3valS1354;
    int32_t _M0L6_2atmpS1353;
    moonbit_incref(_M0L6_2atmpS1814);
    moonbit_decref(_M0L3bufS1355);
    _M0L4elemS462 = _M0L6_2atmpS1814;
    _M0L3valS1354 = _M0L1iS460->$0;
    _M0L6_2atmpS1353 = _M0L3valS1354 + 1;
    _M0L1iS460->$0 = _M0L6_2atmpS1353;
    moonbit_decref(_M0L1iS460);
    return _M0L4elemS462;
  } else {
    moonbit_decref(_M0L4selfS461.$0);
    moonbit_decref(_M0L1iS460);
    return 0;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS458,
  struct _M0TPB6Logger _M0L6loggerS459
) {
  int32_t _M0L6_2atmpS1346;
  struct _M0TPC16string10StringView _M0L6_2atmpS1345;
  #line 244 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1346 = Moonbit_array_length(_M0L4selfS458);
  _M0L6_2atmpS1345
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1346, _M0L4selfS458
  };
  #line 245 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1345, _M0L6loggerS459, 1);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS457) {
  #line 45 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 46 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS457, 10);
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS456,
  struct _M0TPB6Logger _M0L6loggerS455
) {
  moonbit_string_t _M0L6_2atmpS1344;
  #line 26 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1344 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS456);
  #line 27 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS455.$0->$method_0(_M0L6loggerS455.$1, _M0L6_2atmpS1344);
  return 0;
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS454) {
  #line 31 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS454) {
    return (moonbit_string_t)moonbit_string_literal_11.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_19.data;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS448,
  moonbit_string_t _M0L5valueS450
) {
  int32_t _M0L3lenS1334;
  moonbit_string_t* _M0L6_2atmpS1336;
  int32_t _M0L6_2atmpS1335;
  int32_t _M0L6lengthS449;
  moonbit_string_t* _M0L3bufS1337;
  moonbit_string_t _M0L6_2aoldS1818;
  int32_t _M0L6_2atmpS1338;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1334 = _M0L4selfS448->$1;
  moonbit_incref(_M0L4selfS448);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1336 = _M0MPC15array5Array6bufferGsE(_M0L4selfS448);
  _M0L6_2atmpS1335 = Moonbit_array_length(_M0L6_2atmpS1336);
  moonbit_decref(_M0L6_2atmpS1336);
  if (_M0L3lenS1334 == _M0L6_2atmpS1335) {
    moonbit_incref(_M0L4selfS448);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS448);
  }
  _M0L6lengthS449 = _M0L4selfS448->$1;
  _M0L3bufS1337 = _M0L4selfS448->$0;
  _M0L6_2aoldS1818 = (moonbit_string_t)_M0L3bufS1337[_M0L6lengthS449];
  moonbit_decref(_M0L6_2aoldS1818);
  _M0L3bufS1337[_M0L6lengthS449] = _M0L5valueS450;
  _M0L6_2atmpS1338 = _M0L6lengthS449 + 1;
  _M0L4selfS448->$1 = _M0L6_2atmpS1338;
  moonbit_decref(_M0L4selfS448);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS451,
  struct _M0TUsiE* _M0L5valueS453
) {
  int32_t _M0L3lenS1339;
  struct _M0TUsiE** _M0L6_2atmpS1341;
  int32_t _M0L6_2atmpS1340;
  int32_t _M0L6lengthS452;
  struct _M0TUsiE** _M0L3bufS1342;
  struct _M0TUsiE* _M0L6_2aoldS1820;
  int32_t _M0L6_2atmpS1343;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1339 = _M0L4selfS451->$1;
  moonbit_incref(_M0L4selfS451);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1341 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS451);
  _M0L6_2atmpS1340 = Moonbit_array_length(_M0L6_2atmpS1341);
  moonbit_decref(_M0L6_2atmpS1341);
  if (_M0L3lenS1339 == _M0L6_2atmpS1340) {
    moonbit_incref(_M0L4selfS451);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS451);
  }
  _M0L6lengthS452 = _M0L4selfS451->$1;
  _M0L3bufS1342 = _M0L4selfS451->$0;
  _M0L6_2aoldS1820 = (struct _M0TUsiE*)_M0L3bufS1342[_M0L6lengthS452];
  if (_M0L6_2aoldS1820) {
    moonbit_decref(_M0L6_2aoldS1820);
  }
  _M0L3bufS1342[_M0L6lengthS452] = _M0L5valueS453;
  _M0L6_2atmpS1343 = _M0L6lengthS452 + 1;
  _M0L4selfS451->$1 = _M0L6_2atmpS1343;
  moonbit_decref(_M0L4selfS451);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS443) {
  int32_t _M0L8old__capS442;
  int32_t _M0L8new__capS444;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS442 = _M0L4selfS443->$1;
  if (_M0L8old__capS442 == 0) {
    _M0L8new__capS444 = 8;
  } else {
    _M0L8new__capS444 = _M0L8old__capS442 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS443, _M0L8new__capS444);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS446
) {
  int32_t _M0L8old__capS445;
  int32_t _M0L8new__capS447;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS445 = _M0L4selfS446->$1;
  if (_M0L8old__capS445 == 0) {
    _M0L8new__capS447 = 8;
  } else {
    _M0L8new__capS447 = _M0L8old__capS445 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS446, _M0L8new__capS447);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS433,
  int32_t _M0L13new__capacityS431
) {
  moonbit_string_t* _M0L8new__bufS430;
  moonbit_string_t* _M0L8old__bufS432;
  int32_t _M0L8old__capS434;
  int32_t _M0L9copy__lenS435;
  moonbit_string_t* _M0L6_2aoldS1822;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS430
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS431, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8old__bufS432 = _M0L4selfS433->$0;
  _M0L8old__capS434 = Moonbit_array_length(_M0L8old__bufS432);
  if (_M0L8old__capS434 < _M0L13new__capacityS431) {
    _M0L9copy__lenS435 = _M0L8old__capS434;
  } else {
    _M0L9copy__lenS435 = _M0L13new__capacityS431;
  }
  moonbit_incref(_M0L8old__bufS432);
  moonbit_incref(_M0L8new__bufS430);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS430, 0, _M0L8old__bufS432, 0, _M0L9copy__lenS435);
  _M0L6_2aoldS1822 = _M0L4selfS433->$0;
  moonbit_decref(_M0L6_2aoldS1822);
  _M0L4selfS433->$0 = _M0L8new__bufS430;
  moonbit_decref(_M0L4selfS433);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS439,
  int32_t _M0L13new__capacityS437
) {
  struct _M0TUsiE** _M0L8new__bufS436;
  struct _M0TUsiE** _M0L8old__bufS438;
  int32_t _M0L8old__capS440;
  int32_t _M0L9copy__lenS441;
  struct _M0TUsiE** _M0L6_2aoldS1824;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS436
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS437, 0);
  _M0L8old__bufS438 = _M0L4selfS439->$0;
  _M0L8old__capS440 = Moonbit_array_length(_M0L8old__bufS438);
  if (_M0L8old__capS440 < _M0L13new__capacityS437) {
    _M0L9copy__lenS441 = _M0L8old__capS440;
  } else {
    _M0L9copy__lenS441 = _M0L13new__capacityS437;
  }
  moonbit_incref(_M0L8old__bufS438);
  moonbit_incref(_M0L8new__bufS436);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS436, 0, _M0L8old__bufS438, 0, _M0L9copy__lenS441);
  _M0L6_2aoldS1824 = _M0L4selfS439->$0;
  moonbit_decref(_M0L6_2aoldS1824);
  _M0L4selfS439->$0 = _M0L8new__bufS436;
  moonbit_decref(_M0L4selfS439);
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS428
) {
  moonbit_string_t* _M0L8_2afieldS1826;
  int32_t _M0L6_2acntS1910;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS1826 = _M0L4selfS428->$0;
  _M0L6_2acntS1910 = Moonbit_object_header(_M0L4selfS428)->rc;
  if (_M0L6_2acntS1910 > 1) {
    int32_t _M0L11_2anew__cntS1911 = _M0L6_2acntS1910 - 1;
    Moonbit_object_header(_M0L4selfS428)->rc = _M0L11_2anew__cntS1911;
    moonbit_incref(_M0L8_2afieldS1826);
  } else if (_M0L6_2acntS1910 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS428);
  }
  return _M0L8_2afieldS1826;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS429
) {
  struct _M0TUsiE** _M0L8_2afieldS1827;
  int32_t _M0L6_2acntS1912;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS1827 = _M0L4selfS429->$0;
  _M0L6_2acntS1912 = Moonbit_object_header(_M0L4selfS429)->rc;
  if (_M0L6_2acntS1912 > 1) {
    int32_t _M0L11_2anew__cntS1913 = _M0L6_2acntS1912 - 1;
    Moonbit_object_header(_M0L4selfS429)->rc = _M0L11_2anew__cntS1913;
    moonbit_incref(_M0L8_2afieldS1827);
  } else if (_M0L6_2acntS1912 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS429);
  }
  return _M0L8_2afieldS1827;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS427
) {
  #line 53 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS427 == 0) {
    moonbit_string_t* _M0L6_2atmpS1332 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_1996 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1996)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1996->$0 = _M0L6_2atmpS1332;
    _block_1996->$1 = 0;
    return _block_1996;
  } else {
    moonbit_string_t* _M0L6_2atmpS1333 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS427, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_1997 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1997)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1997->$0 = _M0L6_2atmpS1333;
    _block_1997->$1 = 0;
    return _block_1997;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS426
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS426;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS425,
  struct _M0TPC16string10StringView _M0L3strS424
) {
  int32_t _M0L8str__lenS423;
  int32_t _M0L3lenS1325;
  int32_t _M0L6_2atmpS1324;
  uint16_t* _M0L4dataS1326;
  int32_t _M0L3lenS1327;
  moonbit_string_t _M0L6_2atmpS1328;
  int32_t _M0L6_2atmpS1329;
  int32_t _M0L3lenS1331;
  int32_t _M0L6_2atmpS1330;
  #line 126 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  moonbit_incref(_M0L3strS424.$0);
  #line 130 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS423 = _M0MPC16string10StringView6length(_M0L3strS424);
  _M0L3lenS1325 = _M0L4selfS425->$1;
  _M0L6_2atmpS1324 = _M0L3lenS1325 + _M0L8str__lenS423;
  moonbit_incref(_M0L4selfS425);
  #line 131 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS425, _M0L6_2atmpS1324);
  _M0L4dataS1326 = _M0L4selfS425->$0;
  _M0L3lenS1327 = _M0L4selfS425->$1;
  moonbit_incref(_M0L4dataS1326);
  moonbit_incref(_M0L3strS424.$0);
  #line 134 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1328 = _M0MPC16string10StringView4data(_M0L3strS424);
  #line 135 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1329 = _M0MPC16string10StringView13start__offset(_M0L3strS424);
  #line 132 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1326, _M0L3lenS1327, _M0L6_2atmpS1328, _M0L6_2atmpS1329, _M0L8str__lenS423);
  _M0L3lenS1331 = _M0L4selfS425->$1;
  _M0L6_2atmpS1330 = _M0L3lenS1331 + _M0L8str__lenS423;
  _M0L4selfS425->$1 = _M0L6_2atmpS1330;
  moonbit_decref(_M0L4selfS425);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS415,
  int32_t _M0L3lenS418,
  int32_t _M0L13start__offsetS422,
  int64_t _M0L11end__offsetS413
) {
  int32_t _M0L11end__offsetS412;
  int32_t _M0L5indexS416;
  int32_t _M0L5countS417;
  #line 441 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS413 == 4294967296ll) {
    _M0L11end__offsetS412 = Moonbit_array_length(_M0L4selfS415);
  } else {
    int64_t _M0L7_2aSomeS414 = _M0L11end__offsetS413;
    _M0L11end__offsetS412 = (int32_t)_M0L7_2aSomeS414;
  }
  _M0L5indexS416 = _M0L13start__offsetS422;
  _M0L5countS417 = 0;
  while (1) {
    int32_t _if__result_1999;
    if (_M0L5indexS416 < _M0L11end__offsetS412) {
      _if__result_1999 = _M0L5countS417 < _M0L3lenS418;
    } else {
      _if__result_1999 = 0;
    }
    if (_if__result_1999) {
      int32_t _M0L2c1S419 = _M0L4selfS415[_M0L5indexS416];
      int32_t _if__result_2000;
      int32_t _M0L6_2atmpS1322;
      int32_t _M0L6_2atmpS1323;
      #line 452 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S419)) {
        int32_t _M0L6_2atmpS1318 = _M0L5indexS416 + 1;
        _if__result_2000 = _M0L6_2atmpS1318 < _M0L11end__offsetS412;
      } else {
        _if__result_2000 = 0;
      }
      if (_if__result_2000) {
        int32_t _M0L6_2atmpS1321 = _M0L5indexS416 + 1;
        int32_t _M0L2c2S420 = _M0L4selfS415[_M0L6_2atmpS1321];
        #line 454 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S420)) {
          int32_t _M0L6_2atmpS1319 = _M0L5indexS416 + 2;
          int32_t _M0L6_2atmpS1320 = _M0L5countS417 + 1;
          _M0L5indexS416 = _M0L6_2atmpS1319;
          _M0L5countS417 = _M0L6_2atmpS1320;
          continue;
        } else {
          #line 457 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_20.data);
        }
      }
      _M0L6_2atmpS1322 = _M0L5indexS416 + 1;
      _M0L6_2atmpS1323 = _M0L5countS417 + 1;
      _M0L5indexS416 = _M0L6_2atmpS1322;
      _M0L5countS417 = _M0L6_2atmpS1323;
      continue;
    } else {
      moonbit_decref(_M0L4selfS415);
      return _M0L5countS417 >= _M0L3lenS418;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS409
) {
  int32_t _M0L3endS1312;
  int32_t _M0L5startS1313;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1312 = _M0L4selfS409.$2;
  _M0L5startS1313 = _M0L4selfS409.$1;
  moonbit_decref(_M0L4selfS409.$0);
  return _M0L3endS1312 - _M0L5startS1313;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS410
) {
  int32_t _M0L3endS1314;
  int32_t _M0L5startS1315;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1314 = _M0L4selfS410.$2;
  _M0L5startS1315 = _M0L4selfS410.$1;
  moonbit_decref(_M0L4selfS410.$0);
  return _M0L3endS1314 - _M0L5startS1315;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS411
) {
  int32_t _M0L3endS1316;
  int32_t _M0L5startS1317;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1316 = _M0L4selfS411.$2;
  _M0L5startS1317 = _M0L4selfS411.$1;
  moonbit_decref(_M0L4selfS411.$0);
  return _M0L3endS1316 - _M0L5startS1317;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS407,
  int64_t _M0L19start__offset_2eoptS405,
  int64_t _M0L11end__offsetS408
) {
  int32_t _M0L13start__offsetS404;
  if (_M0L19start__offset_2eoptS405 == 4294967296ll) {
    _M0L13start__offsetS404 = 0;
  } else {
    int64_t _M0L7_2aSomeS406 = _M0L19start__offset_2eoptS405;
    _M0L13start__offsetS404 = (int32_t)_M0L7_2aSomeS406;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS407, _M0L13start__offsetS404, _M0L11end__offsetS408);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS402,
  int32_t _M0L13start__offsetS403,
  int64_t _M0L11end__offsetS400
) {
  int32_t _M0L11end__offsetS399;
  int32_t _if__result_2001;
  #line 512 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS400 == 4294967296ll) {
    _M0L11end__offsetS399 = Moonbit_array_length(_M0L4selfS402);
  } else {
    int64_t _M0L7_2aSomeS401 = _M0L11end__offsetS400;
    _M0L11end__offsetS399 = (int32_t)_M0L7_2aSomeS401;
  }
  if (_M0L13start__offsetS403 >= 0) {
    if (_M0L13start__offsetS403 <= _M0L11end__offsetS399) {
      int32_t _M0L6_2atmpS1311 = Moonbit_array_length(_M0L4selfS402);
      _if__result_2001 = _M0L11end__offsetS399 <= _M0L6_2atmpS1311;
    } else {
      _if__result_2001 = 0;
    }
  } else {
    _if__result_2001 = 0;
  }
  if (_if__result_2001) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS403,
                                                 _M0L11end__offsetS399,
                                                 _M0L4selfS402};
  } else {
    moonbit_decref(_M0L4selfS402);
    #line 521 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_21.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS398
) {
  #line 197 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 198 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L4selfS398);
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS397
) {
  moonbit_string_t _M0L3strS1308;
  int32_t _M0L5startS1309;
  int32_t _M0L3endS1310;
  #line 190 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1308 = _M0L4selfS397.$0;
  _M0L5startS1309 = _M0L4selfS397.$1;
  _M0L3endS1310 = _M0L4selfS397.$2;
  #line 193 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1308, _M0L5startS1309, _M0L3endS1310);
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS394,
  int32_t _M0L5startS392,
  int32_t _M0L3endS393
) {
  int32_t _if__result_2002;
  int32_t _M0L3lenS395;
  int32_t _M0L6_2atmpS1306;
  int32_t _M0L6_2atmpS1307;
  moonbit_bytes_t _M0L5bytesS396;
  moonbit_bytes_t _M0L6_2atmpS1305;
  #line 91 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS392 == 0) {
    int32_t _M0L6_2atmpS1304 = Moonbit_array_length(_M0L3strS394);
    _if__result_2002 = _M0L3endS393 == _M0L6_2atmpS1304;
  } else {
    _if__result_2002 = 0;
  }
  if (_if__result_2002) {
    return _M0L3strS394;
  }
  _M0L3lenS395 = _M0L3endS393 - _M0L5startS392;
  _M0L6_2atmpS1306 = _M0L3lenS395 * 2;
  #line 101 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1307 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS396
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1306, _M0L6_2atmpS1307);
  moonbit_incref(_M0L5bytesS396);
  #line 102 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS396, 0, _M0L3strS394, _M0L5startS392, _M0L3lenS395);
  _M0L6_2atmpS1305 = _M0L5bytesS396;
  #line 103 "/Users/user/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1305, 0, 4294967296ll);
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS387,
  int32_t _M0L6offsetS391,
  int64_t _M0L6lengthS389
) {
  int32_t _M0L3lenS386;
  int32_t _M0L6lengthS388;
  int32_t _if__result_2003;
  #line 76 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS386 = Moonbit_array_length(_M0L4selfS387);
  if (_M0L6lengthS389 == 4294967296ll) {
    _M0L6lengthS388 = _M0L3lenS386 - _M0L6offsetS391;
  } else {
    int64_t _M0L7_2aSomeS390 = _M0L6lengthS389;
    _M0L6lengthS388 = (int32_t)_M0L7_2aSomeS390;
  }
  if (_M0L6offsetS391 >= 0) {
    if (_M0L6lengthS388 >= 0) {
      int32_t _M0L6_2atmpS1303 = _M0L6offsetS391 + _M0L6lengthS388;
      _if__result_2003 = _M0L6_2atmpS1303 <= _M0L3lenS386;
    } else {
      _if__result_2003 = 0;
    }
  } else {
    _if__result_2003 = 0;
  }
  if (_if__result_2003) {
    #line 84 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS387, _M0L6offsetS391, _M0L6lengthS388);
  } else {
    moonbit_decref(_M0L4selfS387);
    #line 83 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS378,
  int32_t _M0L13bytes__offsetS373,
  moonbit_string_t _M0L3strS380,
  int32_t _M0L11str__offsetS376,
  int32_t _M0L6lengthS374
) {
  int32_t _M0L6_2atmpS1302;
  int32_t _M0L6_2atmpS1301;
  int32_t _M0L2e1S372;
  int32_t _M0L6_2atmpS1300;
  int32_t _M0L2e2S375;
  int32_t _M0L4len1S377;
  int32_t _M0L4len2S379;
  int32_t _if__result_2004;
  #line 124 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1302 = _M0L6lengthS374 * 2;
  _M0L6_2atmpS1301 = _M0L13bytes__offsetS373 + _M0L6_2atmpS1302;
  _M0L2e1S372 = _M0L6_2atmpS1301 - 1;
  _M0L6_2atmpS1300 = _M0L11str__offsetS376 + _M0L6lengthS374;
  _M0L2e2S375 = _M0L6_2atmpS1300 - 1;
  _M0L4len1S377 = Moonbit_array_length(_M0L4selfS378);
  _M0L4len2S379 = Moonbit_array_length(_M0L3strS380);
  if (_M0L6lengthS374 >= 0) {
    if (_M0L13bytes__offsetS373 >= 0) {
      if (_M0L2e1S372 < _M0L4len1S377) {
        if (_M0L11str__offsetS376 >= 0) {
          _if__result_2004 = _M0L2e2S375 < _M0L4len2S379;
        } else {
          _if__result_2004 = 0;
        }
      } else {
        _if__result_2004 = 0;
      }
    } else {
      _if__result_2004 = 0;
    }
  } else {
    _if__result_2004 = 0;
  }
  if (_if__result_2004) {
    int32_t _M0L16end__str__offsetS381 =
      _M0L11str__offsetS376 + _M0L6lengthS374;
    int32_t _M0L1iS382 = _M0L11str__offsetS376;
    int32_t _M0L1jS383 = _M0L13bytes__offsetS373;
    while (1) {
      if (_M0L1iS382 < _M0L16end__str__offsetS381) {
        int32_t _M0L6_2atmpS1297 = _M0L3strS380[_M0L1iS382];
        int32_t _M0L6_2atmpS1296 = (int32_t)_M0L6_2atmpS1297;
        uint32_t _M0L1cS384 = *(uint32_t*)&_M0L6_2atmpS1296;
        uint32_t _M0L6_2atmpS1292 = _M0L1cS384 & 255u;
        int32_t _M0L6_2atmpS1291;
        int32_t _M0L6_2atmpS1293;
        uint32_t _M0L6_2atmpS1295;
        int32_t _M0L6_2atmpS1294;
        int32_t _M0L6_2atmpS1298;
        int32_t _M0L6_2atmpS1299;
        #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1291 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1292);
        if (
          _M0L1jS383 < 0 || _M0L1jS383 >= Moonbit_array_length(_M0L4selfS378)
        ) {
          #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS378[_M0L1jS383] = _M0L6_2atmpS1291;
        _M0L6_2atmpS1293 = _M0L1jS383 + 1;
        _M0L6_2atmpS1295 = _M0L1cS384 >> 8;
        #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1294 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1295);
        if (
          _M0L6_2atmpS1293 < 0
          || _M0L6_2atmpS1293 >= Moonbit_array_length(_M0L4selfS378)
        ) {
          #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS378[_M0L6_2atmpS1293] = _M0L6_2atmpS1294;
        _M0L6_2atmpS1298 = _M0L1iS382 + 1;
        _M0L6_2atmpS1299 = _M0L1jS383 + 2;
        _M0L1iS382 = _M0L6_2atmpS1298;
        _M0L1jS383 = _M0L6_2atmpS1299;
        continue;
      } else {
        moonbit_decref(_M0L3strS380);
        moonbit_decref(_M0L4selfS378);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS380);
    moonbit_decref(_M0L4selfS378);
    #line 137 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS371) {
  int32_t _M0L6_2atmpS1290;
  #line 2518 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1290 = *(int32_t*)&_M0L4selfS371;
  return _M0L6_2atmpS1290 & 0xff;
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS369,
  struct _M0TPB6Logger _M0L6loggerS370
) {
  #line 166 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 167 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L4selfS369, _M0L6loggerS370, 1);
  return 0;
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS368) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS368;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_2006;
  int32_t _M0L12is__negativeS353;
  uint32_t _M0L3numS354;
  uint16_t* _M0L6bufferS355;
  #line 209 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_2006 = 1;
  } else {
    _if__result_2006 = _M0L5radixS351 > 36;
  }
  if (_if__result_2006) {
    #line 213 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_22.data);
  }
  if (_M0L4selfS352 == 0) {
    return (moonbit_string_t)moonbit_string_literal_23.data;
  }
  _M0L12is__negativeS353 = _M0L4selfS352 < 0;
  if (_M0L12is__negativeS353) {
    int32_t _M0L6_2atmpS1289 = -_M0L4selfS352;
    _M0L3numS354 = *(uint32_t*)&_M0L6_2atmpS1289;
  } else {
    _M0L3numS354 = *(uint32_t*)&_M0L4selfS352;
  }
  switch (_M0L5radixS351) {
    case 10: {
      int32_t _M0L10digit__lenS356;
      int32_t _M0L6_2atmpS1286;
      int32_t _M0L10total__lenS357;
      uint16_t* _M0L6bufferS358;
      int32_t _M0L12digit__startS359;
      #line 235 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS356 = _M0FPB12dec__count32(_M0L3numS354);
      if (_M0L12is__negativeS353) {
        _M0L6_2atmpS1286 = 1;
      } else {
        _M0L6_2atmpS1286 = 0;
      }
      _M0L10total__lenS357 = _M0L10digit__lenS356 + _M0L6_2atmpS1286;
      _M0L6bufferS358
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS357, 0);
      if (_M0L12is__negativeS353) {
        _M0L12digit__startS359 = 1;
      } else {
        _M0L12digit__startS359 = 0;
      }
      moonbit_incref(_M0L6bufferS358);
      #line 239 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS358, _M0L3numS354, _M0L12digit__startS359, _M0L10total__lenS357);
      _M0L6bufferS355 = _M0L6bufferS358;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS360;
      int32_t _M0L6_2atmpS1287;
      int32_t _M0L10total__lenS361;
      uint16_t* _M0L6bufferS362;
      int32_t _M0L12digit__startS363;
      #line 243 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS360 = _M0FPB12hex__count32(_M0L3numS354);
      if (_M0L12is__negativeS353) {
        _M0L6_2atmpS1287 = 1;
      } else {
        _M0L6_2atmpS1287 = 0;
      }
      _M0L10total__lenS361 = _M0L10digit__lenS360 + _M0L6_2atmpS1287;
      _M0L6bufferS362
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS361, 0);
      if (_M0L12is__negativeS353) {
        _M0L12digit__startS363 = 1;
      } else {
        _M0L12digit__startS363 = 0;
      }
      moonbit_incref(_M0L6bufferS362);
      #line 247 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS362, _M0L3numS354, _M0L12digit__startS363, _M0L10total__lenS361);
      _M0L6bufferS355 = _M0L6bufferS362;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS364;
      int32_t _M0L6_2atmpS1288;
      int32_t _M0L10total__lenS365;
      uint16_t* _M0L6bufferS366;
      int32_t _M0L12digit__startS367;
      #line 251 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS364
      = _M0FPB14radix__count32(_M0L3numS354, _M0L5radixS351);
      if (_M0L12is__negativeS353) {
        _M0L6_2atmpS1288 = 1;
      } else {
        _M0L6_2atmpS1288 = 0;
      }
      _M0L10total__lenS365 = _M0L10digit__lenS364 + _M0L6_2atmpS1288;
      _M0L6bufferS366
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS365, 0);
      if (_M0L12is__negativeS353) {
        _M0L12digit__startS367 = 1;
      } else {
        _M0L12digit__startS367 = 0;
      }
      moonbit_incref(_M0L6bufferS366);
      #line 255 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS366, _M0L3numS354, _M0L12digit__startS367, _M0L10total__lenS365, _M0L5radixS351);
      _M0L6bufferS355 = _M0L6bufferS366;
      break;
    }
  }
  if (_M0L12is__negativeS353) {
    _M0L6bufferS355[0] = 45;
  }
  return _M0L6bufferS355;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS345,
  int32_t _M0L5radixS347
) {
  uint32_t _M0L4baseS346;
  uint32_t _M0L3numS348;
  int32_t _M0L5countS349;
  #line 189 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS345 == 0u) {
    return 1;
  }
  _M0L4baseS346 = *(uint32_t*)&_M0L5radixS347;
  _M0L3numS348 = _M0L5valueS345;
  _M0L5countS349 = 0;
  while (1) {
    if (_M0L3numS348 > 0u) {
      uint32_t _M0L6_2atmpS1284 = _M0L3numS348 / _M0L4baseS346;
      int32_t _M0L6_2atmpS1285 = _M0L5countS349 + 1;
      _M0L3numS348 = _M0L6_2atmpS1284;
      _M0L5countS349 = _M0L6_2atmpS1285;
      continue;
    } else {
      return _M0L5countS349;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS343) {
  #line 177 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS343 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS344;
    int32_t _M0L6_2atmpS1283;
    int32_t _M0L6_2atmpS1282;
    #line 182 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS344 = moonbit_clz32(_M0L5valueS343);
    _M0L6_2atmpS1283 = 31 - _M0L14leading__zerosS344;
    _M0L6_2atmpS1282 = _M0L6_2atmpS1283 / 4;
    return _M0L6_2atmpS1282 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS342) {
  #line 143 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS342 >= 100000u) {
    if (_M0L5valueS342 >= 10000000u) {
      if (_M0L5valueS342 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS342 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS342 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS342 >= 1000u) {
    if (_M0L5valueS342 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS342 >= 100u) {
    return 3;
  } else if (_M0L5valueS342 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS328,
  uint32_t _M0L3numS340,
  int32_t _M0L12digit__startS329,
  int32_t _M0L10total__lenS341
) {
  int32_t _M0L6_2atmpS1281;
  uint32_t _M0L3numS318;
  int32_t _M0L6offsetS319;
  #line 88 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1281 = _M0L10total__lenS341 - _M0L12digit__startS329;
  _M0L3numS318 = _M0L3numS340;
  _M0L6offsetS319 = _M0L6_2atmpS1281;
  while (1) {
    if (_M0L3numS318 >= 10000u) {
      uint32_t _M0L1tS320 = _M0L3numS318 / 10000u;
      uint32_t _M0L6_2atmpS1258 = _M0L3numS318 % 10000u;
      int32_t _M0L1rS321 = *(int32_t*)&_M0L6_2atmpS1258;
      int32_t _M0L2d1S322 = _M0L1rS321 / 100;
      int32_t _M0L2d2S323 = _M0L1rS321 % 100;
      int32_t _M0L6_2atmpS1257 = _M0L2d1S322 / 10;
      int32_t _M0L6_2atmpS1256 = 48 + _M0L6_2atmpS1257;
      int32_t _M0L6d1__hiS324 = (uint16_t)_M0L6_2atmpS1256;
      int32_t _M0L6_2atmpS1255 = _M0L2d1S322 % 10;
      int32_t _M0L6_2atmpS1254 = 48 + _M0L6_2atmpS1255;
      int32_t _M0L6d1__loS325 = (uint16_t)_M0L6_2atmpS1254;
      int32_t _M0L6_2atmpS1253 = _M0L2d2S323 / 10;
      int32_t _M0L6_2atmpS1252 = 48 + _M0L6_2atmpS1253;
      int32_t _M0L6d2__hiS326 = (uint16_t)_M0L6_2atmpS1252;
      int32_t _M0L6_2atmpS1251 = _M0L2d2S323 % 10;
      int32_t _M0L6_2atmpS1250 = 48 + _M0L6_2atmpS1251;
      int32_t _M0L6d2__loS327 = (uint16_t)_M0L6_2atmpS1250;
      int32_t _M0L6_2atmpS1242 = _M0L12digit__startS329 + _M0L6offsetS319;
      int32_t _M0L6_2atmpS1241 = _M0L6_2atmpS1242 - 4;
      int32_t _M0L6_2atmpS1244;
      int32_t _M0L6_2atmpS1243;
      int32_t _M0L6_2atmpS1246;
      int32_t _M0L6_2atmpS1245;
      int32_t _M0L6_2atmpS1248;
      int32_t _M0L6_2atmpS1247;
      int32_t _M0L6_2atmpS1249;
      _M0L6bufferS328[_M0L6_2atmpS1241] = _M0L6d1__hiS324;
      _M0L6_2atmpS1244 = _M0L12digit__startS329 + _M0L6offsetS319;
      _M0L6_2atmpS1243 = _M0L6_2atmpS1244 - 3;
      _M0L6bufferS328[_M0L6_2atmpS1243] = _M0L6d1__loS325;
      _M0L6_2atmpS1246 = _M0L12digit__startS329 + _M0L6offsetS319;
      _M0L6_2atmpS1245 = _M0L6_2atmpS1246 - 2;
      _M0L6bufferS328[_M0L6_2atmpS1245] = _M0L6d2__hiS326;
      _M0L6_2atmpS1248 = _M0L12digit__startS329 + _M0L6offsetS319;
      _M0L6_2atmpS1247 = _M0L6_2atmpS1248 - 1;
      _M0L6bufferS328[_M0L6_2atmpS1247] = _M0L6d2__loS327;
      _M0L6_2atmpS1249 = _M0L6offsetS319 - 4;
      _M0L3numS318 = _M0L1tS320;
      _M0L6offsetS319 = _M0L6_2atmpS1249;
      continue;
    } else {
      int32_t _M0L6_2atmpS1280 = *(int32_t*)&_M0L3numS318;
      int32_t _M0L9remainingS331 = _M0L6_2atmpS1280;
      int32_t _M0L6offsetS332 = _M0L6offsetS319;
      while (1) {
        if (_M0L9remainingS331 >= 100) {
          int32_t _M0L1tS333 = _M0L9remainingS331 / 100;
          int32_t _M0L1dS334 = _M0L9remainingS331 % 100;
          int32_t _M0L6_2atmpS1267 = _M0L1dS334 / 10;
          int32_t _M0L6_2atmpS1266 = 48 + _M0L6_2atmpS1267;
          int32_t _M0L5d__hiS335 = (uint16_t)_M0L6_2atmpS1266;
          int32_t _M0L6_2atmpS1265 = _M0L1dS334 % 10;
          int32_t _M0L6_2atmpS1264 = 48 + _M0L6_2atmpS1265;
          int32_t _M0L5d__loS336 = (uint16_t)_M0L6_2atmpS1264;
          int32_t _M0L6_2atmpS1260 = _M0L12digit__startS329 + _M0L6offsetS332;
          int32_t _M0L6_2atmpS1259 = _M0L6_2atmpS1260 - 2;
          int32_t _M0L6_2atmpS1262;
          int32_t _M0L6_2atmpS1261;
          int32_t _M0L6_2atmpS1263;
          _M0L6bufferS328[_M0L6_2atmpS1259] = _M0L5d__hiS335;
          _M0L6_2atmpS1262 = _M0L12digit__startS329 + _M0L6offsetS332;
          _M0L6_2atmpS1261 = _M0L6_2atmpS1262 - 1;
          _M0L6bufferS328[_M0L6_2atmpS1261] = _M0L5d__loS336;
          _M0L6_2atmpS1263 = _M0L6offsetS332 - 2;
          _M0L9remainingS331 = _M0L1tS333;
          _M0L6offsetS332 = _M0L6_2atmpS1263;
          continue;
        } else if (_M0L9remainingS331 >= 10) {
          int32_t _M0L6_2atmpS1275 = _M0L9remainingS331 / 10;
          int32_t _M0L6_2atmpS1274 = 48 + _M0L6_2atmpS1275;
          int32_t _M0L5d__hiS338 = (uint16_t)_M0L6_2atmpS1274;
          int32_t _M0L6_2atmpS1273 = _M0L9remainingS331 % 10;
          int32_t _M0L6_2atmpS1272 = 48 + _M0L6_2atmpS1273;
          int32_t _M0L5d__loS339 = (uint16_t)_M0L6_2atmpS1272;
          int32_t _M0L6_2atmpS1269 = _M0L12digit__startS329 + _M0L6offsetS332;
          int32_t _M0L6_2atmpS1268 = _M0L6_2atmpS1269 - 2;
          int32_t _M0L6_2atmpS1271;
          int32_t _M0L6_2atmpS1270;
          _M0L6bufferS328[_M0L6_2atmpS1268] = _M0L5d__hiS338;
          _M0L6_2atmpS1271 = _M0L12digit__startS329 + _M0L6offsetS332;
          _M0L6_2atmpS1270 = _M0L6_2atmpS1271 - 1;
          _M0L6bufferS328[_M0L6_2atmpS1270] = _M0L5d__loS339;
          moonbit_decref(_M0L6bufferS328);
        } else {
          int32_t _M0L6_2atmpS1279 = _M0L12digit__startS329 + _M0L6offsetS332;
          int32_t _M0L6_2atmpS1276 = _M0L6_2atmpS1279 - 1;
          int32_t _M0L6_2atmpS1278 = 48 + _M0L9remainingS331;
          int32_t _M0L6_2atmpS1277 = (uint16_t)_M0L6_2atmpS1278;
          _M0L6bufferS328[_M0L6_2atmpS1276] = _M0L6_2atmpS1277;
          moonbit_decref(_M0L6bufferS328);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS308,
  uint32_t _M0L3numS312,
  int32_t _M0L12digit__startS309,
  int32_t _M0L10total__lenS311,
  int32_t _M0L5radixS302
) {
  uint32_t _M0L4baseS301;
  int32_t _M0L6_2atmpS1226;
  int32_t _M0L6_2atmpS1225;
  #line 57 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS301 = *(uint32_t*)&_M0L5radixS302;
  _M0L6_2atmpS1226 = _M0L5radixS302 - 1;
  _M0L6_2atmpS1225 = _M0L5radixS302 & _M0L6_2atmpS1226;
  if (_M0L6_2atmpS1225 == 0) {
    int32_t _M0L5shiftS303;
    uint32_t _M0L4maskS304;
    int32_t _M0L6_2atmpS1233;
    int32_t _M0L6offsetS305;
    uint32_t _M0L1nS306;
    #line 68 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS303 = moonbit_ctz32(_M0L5radixS302);
    _M0L4maskS304 = _M0L4baseS301 - 1u;
    _M0L6_2atmpS1233 = _M0L10total__lenS311 - _M0L12digit__startS309;
    _M0L6offsetS305 = _M0L6_2atmpS1233;
    _M0L1nS306 = _M0L3numS312;
    while (1) {
      if (_M0L1nS306 > 0u) {
        uint32_t _M0L6_2atmpS1232 = _M0L1nS306 & _M0L4maskS304;
        int32_t _M0L5digitS307 = *(int32_t*)&_M0L6_2atmpS1232;
        int32_t _M0L6_2atmpS1229 = _M0L12digit__startS309 + _M0L6offsetS305;
        int32_t _M0L6_2atmpS1227 = _M0L6_2atmpS1229 - 1;
        int32_t _M0L6_2atmpS1228 =
          ((moonbit_string_t)moonbit_string_literal_24.data)[_M0L5digitS307];
        int32_t _M0L6_2atmpS1230;
        uint32_t _M0L6_2atmpS1231;
        _M0L6bufferS308[_M0L6_2atmpS1227] = _M0L6_2atmpS1228;
        _M0L6_2atmpS1230 = _M0L6offsetS305 - 1;
        _M0L6_2atmpS1231 = _M0L1nS306 >> (_M0L5shiftS303 & 31);
        _M0L6offsetS305 = _M0L6_2atmpS1230;
        _M0L1nS306 = _M0L6_2atmpS1231;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS308);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1240 = _M0L10total__lenS311 - _M0L12digit__startS309;
    int32_t _M0L6offsetS313 = _M0L6_2atmpS1240;
    uint32_t _M0L1nS314 = _M0L3numS312;
    while (1) {
      if (_M0L1nS314 > 0u) {
        uint32_t _M0L1qS315 = _M0L1nS314 / _M0L4baseS301;
        uint32_t _M0L6_2atmpS1239 = _M0L1qS315 * _M0L4baseS301;
        uint32_t _M0L6_2atmpS1238 = _M0L1nS314 - _M0L6_2atmpS1239;
        int32_t _M0L5digitS316 = *(int32_t*)&_M0L6_2atmpS1238;
        int32_t _M0L6_2atmpS1236 = _M0L12digit__startS309 + _M0L6offsetS313;
        int32_t _M0L6_2atmpS1234 = _M0L6_2atmpS1236 - 1;
        int32_t _M0L6_2atmpS1235 =
          ((moonbit_string_t)moonbit_string_literal_24.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS1237;
        _M0L6bufferS308[_M0L6_2atmpS1234] = _M0L6_2atmpS1235;
        _M0L6_2atmpS1237 = _M0L6offsetS313 - 1;
        _M0L6offsetS313 = _M0L6_2atmpS1237;
        _M0L1nS314 = _M0L1qS315;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS308);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS295,
  uint32_t _M0L3numS300,
  int32_t _M0L12digit__startS296,
  int32_t _M0L10total__lenS299
) {
  int32_t _M0L6_2atmpS1224;
  int32_t _M0L6offsetS290;
  uint32_t _M0L1nS291;
  #line 29 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1224 = _M0L10total__lenS299 - _M0L12digit__startS296;
  _M0L6offsetS290 = _M0L6_2atmpS1224;
  _M0L1nS291 = _M0L3numS300;
  while (1) {
    if (_M0L6offsetS290 >= 2) {
      uint32_t _M0L6_2atmpS1221 = _M0L1nS291 & 255u;
      int32_t _M0L9byte__valS292 = *(int32_t*)&_M0L6_2atmpS1221;
      int32_t _M0L2hiS293 = _M0L9byte__valS292 / 16;
      int32_t _M0L2loS294 = _M0L9byte__valS292 % 16;
      int32_t _M0L6_2atmpS1215 = _M0L12digit__startS296 + _M0L6offsetS290;
      int32_t _M0L6_2atmpS1213 = _M0L6_2atmpS1215 - 2;
      int32_t _M0L6_2atmpS1214 =
        ((moonbit_string_t)moonbit_string_literal_24.data)[_M0L2hiS293];
      int32_t _M0L6_2atmpS1218;
      int32_t _M0L6_2atmpS1216;
      int32_t _M0L6_2atmpS1217;
      int32_t _M0L6_2atmpS1219;
      uint32_t _M0L6_2atmpS1220;
      _M0L6bufferS295[_M0L6_2atmpS1213] = _M0L6_2atmpS1214;
      _M0L6_2atmpS1218 = _M0L12digit__startS296 + _M0L6offsetS290;
      _M0L6_2atmpS1216 = _M0L6_2atmpS1218 - 1;
      _M0L6_2atmpS1217
      = ((moonbit_string_t)moonbit_string_literal_24.data)[
        _M0L2loS294
      ];
      _M0L6bufferS295[_M0L6_2atmpS1216] = _M0L6_2atmpS1217;
      _M0L6_2atmpS1219 = _M0L6offsetS290 - 2;
      _M0L6_2atmpS1220 = _M0L1nS291 >> 8;
      _M0L6offsetS290 = _M0L6_2atmpS1219;
      _M0L1nS291 = _M0L6_2atmpS1220;
      continue;
    } else if (_M0L6offsetS290 == 1) {
      uint32_t _M0L6_2atmpS1223 = _M0L1nS291 & 15u;
      int32_t _M0L6nibbleS298 = *(int32_t*)&_M0L6_2atmpS1223;
      int32_t _M0L6_2atmpS1222 =
        ((moonbit_string_t)moonbit_string_literal_24.data)[_M0L6nibbleS298];
      _M0L6bufferS295[_M0L12digit__startS296] = _M0L6_2atmpS1222;
      moonbit_decref(_M0L6bufferS295);
    } else {
      moonbit_decref(_M0L6bufferS295);
    }
    break;
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS289) {
  struct _M0TWEOs* _M0L7_2afuncS288;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS288 = _M0L4selfS289;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS288->code(_M0L7_2afuncS288);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS287
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS286;
  struct _M0TPB6Logger _M0L6_2atmpS1212;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS286 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS286);
  _M0L6_2atmpS1212
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS286
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS287, _M0L6_2atmpS1212);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS286);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS285
) {
  int32_t _result_2013;
  #line 98 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _result_2013 = _M0L4selfS285.$1;
  moonbit_decref(_M0L4selfS285.$0);
  return _result_2013;
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
  int32_t _M0L6_2atmpS1211;
  int64_t _M0L6_2atmpS1210;
  struct _M0TPC16string10StringView _M0L6_2atmpS1209;
  #line 102 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1211 = _M0L5startS282 + _M0L3lenS283;
  _M0L6_2atmpS1210 = (int64_t)_M0L6_2atmpS1211;
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1209
  = _M0MPC16string6String11sub_2einner(_M0L5valueS281, _M0L5startS282, _M0L6_2atmpS1210);
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS280, _M0L6_2atmpS1209);
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
  int32_t _if__result_2014;
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
      _if__result_2014 = _M0L3endS274 <= _M0L3lenS272;
    } else {
      _if__result_2014 = 0;
    }
  } else {
    _if__result_2014 = 0;
  }
  if (_if__result_2014) {
    if (_M0L5startS278 < _M0L3lenS272) {
      int32_t _M0L6_2atmpS1206 = _M0L4selfS273[_M0L5startS278];
      int32_t _M0L6_2atmpS1205;
      #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1205
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1206);
      if (!_M0L6_2atmpS1205) {
        
      } else {
        #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS274 < _M0L3lenS272) {
      int32_t _M0L6_2atmpS1208 = _M0L4selfS273[_M0L3endS274];
      int32_t _M0L6_2atmpS1207;
      #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1207
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1208);
      if (!_M0L6_2atmpS1207) {
        
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
  uint32_t _M0L6_2atmpS1204;
  uint32_t _M0L6_2atmpS1203;
  struct _M0TPB6Hasher* _block_2015;
  #line 75 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1204 = *(uint32_t*)&_M0L4seedS264;
  _M0L6_2atmpS1203 = _M0L6_2atmpS1204 + 374761393u;
  _block_2015
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2015)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2015->$0 = _M0L6_2atmpS1203;
  return _block_2015;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS263) {
  uint32_t _M0L6_2atmpS1202;
  #line 435 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1202 = _M0MPB6Hasher9avalanche(_M0L4selfS263);
  return *(int32_t*)&_M0L6_2atmpS1202;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS262) {
  uint32_t _M0Lm3accS261;
  uint32_t _M0L6_2atmpS1191;
  uint32_t _M0L6_2atmpS1193;
  uint32_t _M0L6_2atmpS1192;
  uint32_t _M0L6_2atmpS1194;
  uint32_t _M0L6_2atmpS1195;
  uint32_t _M0L6_2atmpS1197;
  uint32_t _M0L6_2atmpS1196;
  uint32_t _M0L6_2atmpS1198;
  uint32_t _M0L6_2atmpS1199;
  uint32_t _M0L6_2atmpS1201;
  uint32_t _M0L6_2atmpS1200;
  #line 440 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS261 = _M0L4selfS262->$0;
  moonbit_decref(_M0L4selfS262);
  _M0L6_2atmpS1191 = _M0Lm3accS261;
  _M0L6_2atmpS1193 = _M0Lm3accS261;
  _M0L6_2atmpS1192 = _M0L6_2atmpS1193 >> 15;
  _M0Lm3accS261 = _M0L6_2atmpS1191 ^ _M0L6_2atmpS1192;
  _M0L6_2atmpS1194 = _M0Lm3accS261;
  _M0Lm3accS261 = _M0L6_2atmpS1194 * 2246822519u;
  _M0L6_2atmpS1195 = _M0Lm3accS261;
  _M0L6_2atmpS1197 = _M0Lm3accS261;
  _M0L6_2atmpS1196 = _M0L6_2atmpS1197 >> 13;
  _M0Lm3accS261 = _M0L6_2atmpS1195 ^ _M0L6_2atmpS1196;
  _M0L6_2atmpS1198 = _M0Lm3accS261;
  _M0Lm3accS261 = _M0L6_2atmpS1198 * 3266489917u;
  _M0L6_2atmpS1199 = _M0Lm3accS261;
  _M0L6_2atmpS1201 = _M0Lm3accS261;
  _M0L6_2atmpS1200 = _M0L6_2atmpS1201 >> 16;
  _M0Lm3accS261 = _M0L6_2atmpS1199 ^ _M0L6_2atmpS1200;
  return _M0Lm3accS261;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS259,
  moonbit_string_t _M0L1yS260
) {
  int32_t _M0L6_2atmpS1190;
  #line 23 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 24 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1190 = moonbit_val_array_equal(_M0L1xS259, _M0L1yS260);
  moonbit_decref(_M0L1yS260);
  moonbit_decref(_M0L1xS259);
  return !_M0L6_2atmpS1190;
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
  uint32_t _M0L6_2atmpS1189;
  #line 187 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1189 = *(uint32_t*)&_M0L5valueS254;
  #line 188 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS253, _M0L6_2atmpS1189);
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
    moonbit_string_t _M0L6_2atmpS1187;
    moonbit_string_t _M0L6_2atmpS1186;
    moonbit_string_t _M0L6_2atmpS1185;
    moonbit_string_t _M0L14expect__base64S251;
    moonbit_string_t _M0L6_2atmpS1184;
    moonbit_string_t _M0L6_2atmpS1183;
    moonbit_string_t _M0L6_2atmpS1182;
    moonbit_string_t _M0L14actual__base64S252;
    moonbit_string_t _M0L6_2atmpS1181;
    moonbit_string_t _M0L6_2atmpS1180;
    moonbit_string_t _M0L6_2atmpS1178;
    moonbit_string_t _M0L6_2atmpS1179;
    moonbit_string_t _M0L6_2atmpS1177;
    moonbit_string_t _M0L6_2atmpS1175;
    moonbit_string_t _M0L6_2atmpS1176;
    moonbit_string_t _M0L6_2atmpS1174;
    moonbit_string_t _M0L6_2atmpS1172;
    moonbit_string_t _M0L6_2atmpS1173;
    moonbit_string_t _M0L6_2atmpS1171;
    moonbit_string_t _M0L6_2atmpS1169;
    moonbit_string_t _M0L6_2atmpS1170;
    moonbit_string_t _M0L6_2atmpS1168;
    moonbit_string_t _M0L6_2atmpS1166;
    moonbit_string_t _M0L6_2atmpS1167;
    moonbit_string_t _M0L6_2atmpS1165;
    moonbit_string_t _M0L6_2atmpS1164;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1163;
    struct moonbit_result_0 _result_2016;
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
    _M0L6_2atmpS1187
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS244);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1186
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1187);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1185
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_25.data, _M0L6_2atmpS1186);
    moonbit_decref(_M0L6_2atmpS1186);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14expect__base64S251
    = moonbit_add_string(_M0L6_2atmpS1185, (moonbit_string_t)moonbit_string_literal_25.data);
    moonbit_decref(_M0L6_2atmpS1185);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1184
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS242);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1183
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1184);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1182
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_25.data, _M0L6_2atmpS1183);
    moonbit_decref(_M0L6_2atmpS1183);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14actual__base64S252
    = moonbit_add_string(_M0L6_2atmpS1182, (moonbit_string_t)moonbit_string_literal_25.data);
    moonbit_decref(_M0L6_2atmpS1182);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1181 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS245);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1180
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS1181);
    moonbit_decref(_M0L6_2atmpS1181);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1178
    = moonbit_add_string(_M0L6_2atmpS1180, (moonbit_string_t)moonbit_string_literal_27.data);
    moonbit_decref(_M0L6_2atmpS1180);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1179
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS247);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1177 = moonbit_add_string(_M0L6_2atmpS1178, _M0L6_2atmpS1179);
    moonbit_decref(_M0L6_2atmpS1179);
    moonbit_decref(_M0L6_2atmpS1178);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1175
    = moonbit_add_string(_M0L6_2atmpS1177, (moonbit_string_t)moonbit_string_literal_28.data);
    moonbit_decref(_M0L6_2atmpS1177);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1176
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS249);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1174 = moonbit_add_string(_M0L6_2atmpS1175, _M0L6_2atmpS1176);
    moonbit_decref(_M0L6_2atmpS1176);
    moonbit_decref(_M0L6_2atmpS1175);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1172
    = moonbit_add_string(_M0L6_2atmpS1174, (moonbit_string_t)moonbit_string_literal_29.data);
    moonbit_decref(_M0L6_2atmpS1174);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1173
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS250);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1171 = moonbit_add_string(_M0L6_2atmpS1172, _M0L6_2atmpS1173);
    moonbit_decref(_M0L6_2atmpS1173);
    moonbit_decref(_M0L6_2atmpS1172);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1169
    = moonbit_add_string(_M0L6_2atmpS1171, (moonbit_string_t)moonbit_string_literal_30.data);
    moonbit_decref(_M0L6_2atmpS1171);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1170
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S251);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1168 = moonbit_add_string(_M0L6_2atmpS1169, _M0L6_2atmpS1170);
    moonbit_decref(_M0L6_2atmpS1170);
    moonbit_decref(_M0L6_2atmpS1169);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1166
    = moonbit_add_string(_M0L6_2atmpS1168, (moonbit_string_t)moonbit_string_literal_31.data);
    moonbit_decref(_M0L6_2atmpS1168);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1167
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S252);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1165 = moonbit_add_string(_M0L6_2atmpS1166, _M0L6_2atmpS1167);
    moonbit_decref(_M0L6_2atmpS1167);
    moonbit_decref(_M0L6_2atmpS1166);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1164
    = moonbit_add_string(_M0L6_2atmpS1165, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1165);
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1163
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1163)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1163)->$0
    = _M0L6_2atmpS1164;
    _result_2016.tag = 0;
    _result_2016.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1163;
    return _result_2016;
  } else {
    int32_t _M0L6_2atmpS1188;
    struct moonbit_result_0 _result_2017;
    moonbit_decref(_M0L9args__locS248);
    moonbit_decref(_M0L3locS246);
    moonbit_decref(_M0L7contentS244);
    moonbit_decref(_M0L6actualS242);
    _M0L6_2atmpS1188 = 0;
    _result_2017.tag = 1;
    _result_2017.data.ok = _M0L6_2atmpS1188;
    return _result_2017;
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
      moonbit_string_t* _M0L3bufS1162 = _M0L7_2aselfS234->$0;
      moonbit_string_t _M0L4itemS238 =
        (moonbit_string_t)_M0L3bufS1162[_M0L1iS237];
      int32_t _M0L6_2atmpS1161;
      if (_M0L1iS237 != 0) {
        if (_M0L4itemS238) {
          moonbit_incref(_M0L4itemS238);
        }
        moonbit_incref(_M0L3bufS233);
        #line 130 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, (moonbit_string_t)moonbit_string_literal_32.data);
      } else if (_M0L4itemS238) {
        moonbit_incref(_M0L4itemS238);
      }
      if (_M0L4itemS238 == 0) {
        if (_M0L4itemS238) {
          moonbit_decref(_M0L4itemS238);
        }
        moonbit_incref(_M0L3bufS233);
        #line 133 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, (moonbit_string_t)moonbit_string_literal_33.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS239 = _M0L4itemS238;
        moonbit_string_t _M0L6_2alocS240 = _M0L7_2aSomeS239;
        moonbit_string_t _M0L6_2atmpS1160;
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1160
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS240);
        moonbit_incref(_M0L3bufS233);
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, _M0L6_2atmpS1160);
      }
      _M0L6_2atmpS1161 = _M0L1iS237 + 1;
      _M0L1iS237 = _M0L6_2atmpS1161;
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
  moonbit_string_t _M0L6_2atmpS1159;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1158;
  #line 95 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1159 = _M0L4selfS232;
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1158 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1159);
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1158);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS231
) {
  struct _M0TPB13StringBuilder* _M0L2sbS230;
  struct _M0TPC16string10StringView _M0L8filenameS1144;
  struct _M0TPC16string10StringView _M0L11start__lineS1147;
  moonbit_string_t _M0L6_2atmpS1146;
  moonbit_string_t _M0L6_2atmpS1145;
  struct _M0TPC16string10StringView _M0L13start__columnS1150;
  moonbit_string_t _M0L6_2atmpS1149;
  moonbit_string_t _M0L6_2atmpS1148;
  struct _M0TPC16string10StringView _M0L9end__lineS1153;
  moonbit_string_t _M0L6_2atmpS1152;
  moonbit_string_t _M0L6_2atmpS1151;
  struct _M0TPC16string10StringView _M0L8_2afieldS1833;
  int32_t _M0L6_2acntS1914;
  struct _M0TPC16string10StringView _M0L11end__columnS1157;
  moonbit_string_t _M0L6_2atmpS1156;
  moonbit_string_t _M0L6_2atmpS1155;
  moonbit_string_t _M0L6_2atmpS1154;
  #line 82 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 83 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS230 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L2sbS230);
  #line 84 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, (moonbit_string_t)moonbit_string_literal_34.data);
  _M0L8filenameS1144
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$0_1, _M0L4selfS231->$0_2, _M0L4selfS231->$0_0
  };
  moonbit_incref(_M0L8filenameS1144.$0);
  moonbit_incref(_M0L2sbS230);
  #line 85 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS230, _M0L8filenameS1144);
  _M0L11start__lineS1147
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$1_1, _M0L4selfS231->$1_2, _M0L4selfS231->$1_0
  };
  moonbit_incref(_M0L11start__lineS1147.$0);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1146
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1147);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1145
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS1146);
  moonbit_decref(_M0L6_2atmpS1146);
  moonbit_incref(_M0L2sbS230);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1145);
  _M0L13start__columnS1150
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$2_1, _M0L4selfS231->$2_2, _M0L4selfS231->$2_0
  };
  moonbit_incref(_M0L13start__columnS1150.$0);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1149
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1150);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1148
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS1149);
  moonbit_decref(_M0L6_2atmpS1149);
  moonbit_incref(_M0L2sbS230);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1148);
  _M0L9end__lineS1153
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$3_1, _M0L4selfS231->$3_2, _M0L4selfS231->$3_0
  };
  moonbit_incref(_M0L9end__lineS1153.$0);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1152
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1153);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1151
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS1152);
  moonbit_decref(_M0L6_2atmpS1152);
  moonbit_incref(_M0L2sbS230);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1151);
  _M0L8_2afieldS1833
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$4_1, _M0L4selfS231->$4_2, _M0L4selfS231->$4_0
  };
  _M0L6_2acntS1914 = Moonbit_object_header(_M0L4selfS231)->rc;
  if (_M0L6_2acntS1914 > 1) {
    int32_t _M0L11_2anew__cntS1919 = _M0L6_2acntS1914 - 1;
    Moonbit_object_header(_M0L4selfS231)->rc = _M0L11_2anew__cntS1919;
    moonbit_incref(_M0L8_2afieldS1833.$0);
  } else if (_M0L6_2acntS1914 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS1918 =
      (struct _M0TPC16string10StringView){_M0L4selfS231->$3_1,
                                            _M0L4selfS231->$3_2,
                                            _M0L4selfS231->$3_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS1917;
    struct _M0TPC16string10StringView _M0L8_2afieldS1916;
    struct _M0TPC16string10StringView _M0L8_2afieldS1915;
    moonbit_decref(_M0L8_2afieldS1918.$0);
    _M0L8_2afieldS1917
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$2_1, _M0L4selfS231->$2_2, _M0L4selfS231->$2_0
    };
    moonbit_decref(_M0L8_2afieldS1917.$0);
    _M0L8_2afieldS1916
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$1_1, _M0L4selfS231->$1_2, _M0L4selfS231->$1_0
    };
    moonbit_decref(_M0L8_2afieldS1916.$0);
    _M0L8_2afieldS1915
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$0_1, _M0L4selfS231->$0_2, _M0L4selfS231->$0_0
    };
    moonbit_decref(_M0L8_2afieldS1915.$0);
    #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS231);
  }
  _M0L11end__columnS1157 = _M0L8_2afieldS1833;
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1156
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1157);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1155
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS1156);
  moonbit_decref(_M0L6_2atmpS1156);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1154
  = moonbit_add_string(_M0L6_2atmpS1155, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1155);
  moonbit_incref(_M0L2sbS230);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1154);
  #line 90 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS230);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS229,
  moonbit_string_t _M0L3strS228
) {
  int32_t _M0L8str__lenS227;
  int32_t _M0L3lenS1139;
  int32_t _M0L6_2atmpS1138;
  uint16_t* _M0L4dataS1140;
  int32_t _M0L3lenS1141;
  int32_t _M0L3lenS1143;
  int32_t _M0L6_2atmpS1142;
  #line 81 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS227 = Moonbit_array_length(_M0L3strS228);
  _M0L3lenS1139 = _M0L4selfS229->$1;
  _M0L6_2atmpS1138 = _M0L3lenS1139 + _M0L8str__lenS227;
  moonbit_incref(_M0L4selfS229);
  #line 83 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS229, _M0L6_2atmpS1138);
  _M0L4dataS1140 = _M0L4selfS229->$0;
  _M0L3lenS1141 = _M0L4selfS229->$1;
  moonbit_incref(_M0L4dataS1140);
  #line 84 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1140, _M0L3lenS1141, _M0L3strS228, 0, _M0L8str__lenS227);
  _M0L3lenS1143 = _M0L4selfS229->$1;
  _M0L6_2atmpS1142 = _M0L3lenS1143 + _M0L8str__lenS227;
  _M0L4selfS229->$1 = _M0L6_2atmpS1142;
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
      int32_t _M0L6_2atmpS1135 = _M0L3strS224[_M0L1iS221];
      int32_t _M0L6_2atmpS1136;
      int32_t _M0L6_2atmpS1137;
      if (
        _M0L1jS222 < 0 || _M0L1jS222 >= Moonbit_array_length(_M0L4selfS223)
      ) {
        #line 75 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS223[_M0L1jS222] = _M0L6_2atmpS1135;
      _M0L6_2atmpS1136 = _M0L1iS221 + 1;
      _M0L6_2atmpS1137 = _M0L1jS222 + 1;
      _M0L1iS221 = _M0L6_2atmpS1136;
      _M0L1jS222 = _M0L6_2atmpS1137;
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
  struct _M0TPB6Logger _M0L6_2atmpS1134;
  #line 17 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1134
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS217
  };
  #line 21 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS216, _M0L6_2atmpS1134);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS161
) {
  int32_t _M0L6_2atmpS1133;
  struct _M0TPC16string10StringView _M0L7_2abindS160;
  moonbit_string_t _M0L7_2adataS162;
  int32_t _M0L8_2astartS163;
  int32_t _M0L6_2atmpS1132;
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
  int32_t _M0L6_2atmpS1091;
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1133 = Moonbit_array_length(_M0L4reprS161);
  _M0L7_2abindS160
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1133, _M0L4reprS161
  };
  moonbit_incref(_M0L7_2abindS160.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS162 = _M0MPC16string10StringView4data(_M0L7_2abindS160);
  moonbit_incref(_M0L7_2abindS160.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS163
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS160);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1132 = _M0MPC16string10StringView6length(_M0L7_2abindS160);
  _M0L6_2aendS164 = _M0L8_2astartS163 + _M0L6_2atmpS1132;
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
  _M0L6_2atmpS1091 = _M0Lm9_2acursorS165;
  if (_M0L6_2atmpS1091 < _M0L6_2aendS164) {
    int32_t _M0L6_2atmpS1092 = _M0Lm9_2acursorS165;
    int32_t _M0L12dispatch__15S188;
    _M0Lm9_2acursorS165 = _M0L6_2atmpS1092 + 1;
    _M0L12dispatch__15S188 = 0;
    loop__label__15_191:;
    while (1) {
      int32_t _M0L6_2atmpS1096;
      int32_t _M0L6_2atmpS1093;
      switch (_M0L12dispatch__15S188) {
        case 6: {
          int32_t _M0L6_2atmpS1099;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1099 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1099 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1101 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS196;
            int32_t _M0L6_2atmpS1100;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS196
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1101);
            _M0L6_2atmpS1100 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1100 + 1;
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
          int32_t _M0L6_2atmpS1102;
          _M0Lm9tag__0__2S175 = _M0Lm9tag__0__1S174;
          _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1102 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1102 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1107 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS198;
            int32_t _M0L6_2atmpS1103;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS198
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1107);
            _M0L6_2atmpS1103 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1103 + 1;
            if (_M0L10next__charS198 < 58) {
              if (_M0L10next__charS198 < 48) {
                goto join_197;
              } else {
                int32_t _M0L6_2atmpS1104;
                _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
                _M0Lm9tag__1__1S178 = _M0Lm6tag__1S177;
                _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
                _M0Lm6tag__2S176 = _M0Lm9_2acursorS165;
                _M0L6_2atmpS1104 = _M0Lm9_2acursorS165;
                if (_M0L6_2atmpS1104 < _M0L6_2aendS164) {
                  int32_t _M0L6_2atmpS1106 = _M0Lm9_2acursorS165;
                  int32_t _M0L10next__charS200;
                  int32_t _M0L6_2atmpS1105;
                  moonbit_incref(_M0L7_2adataS162);
                  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS200
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1106);
                  _M0L6_2atmpS1105 = _M0Lm9_2acursorS165;
                  _M0Lm9_2acursorS165 = _M0L6_2atmpS1105 + 1;
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
          int32_t _M0L6_2atmpS1108;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0Lm6tag__2S176 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1108 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1108 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1110 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS202;
            int32_t _M0L6_2atmpS1109;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS202
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1110);
            _M0L6_2atmpS1109 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1109 + 1;
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
          int32_t _M0L6_2atmpS1111;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0Lm6tag__4S179 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1111 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1111 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1113 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS204;
            int32_t _M0L6_2atmpS1112;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS204
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1113);
            _M0L6_2atmpS1112 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1112 + 1;
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
          int32_t _M0L6_2atmpS1114;
          _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1114 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1114 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1116 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS206;
            int32_t _M0L6_2atmpS1115;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS206
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1116);
            _M0L6_2atmpS1115 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1115 + 1;
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
          int32_t _M0L6_2atmpS1117;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__3S180 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1117 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1117 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1125 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS208;
            int32_t _M0L6_2atmpS1118;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS208
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1125);
            _M0L6_2atmpS1118 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1118 + 1;
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
              int32_t _M0L6_2atmpS1119;
              _M0Lm9tag__0__2S175 = _M0Lm9tag__0__1S174;
              _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
              _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
              _M0L6_2atmpS1119 = _M0Lm9_2acursorS165;
              if (_M0L6_2atmpS1119 < _M0L6_2aendS164) {
                int32_t _M0L6_2atmpS1124 = _M0Lm9_2acursorS165;
                int32_t _M0L10next__charS210;
                int32_t _M0L6_2atmpS1120;
                moonbit_incref(_M0L7_2adataS162);
                #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                _M0L10next__charS210
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1124);
                _M0L6_2atmpS1120 = _M0Lm9_2acursorS165;
                _M0Lm9_2acursorS165 = _M0L6_2atmpS1120 + 1;
                if (_M0L10next__charS210 < 58) {
                  if (_M0L10next__charS210 < 48) {
                    goto join_209;
                  } else {
                    int32_t _M0L6_2atmpS1121;
                    _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
                    _M0Lm9tag__1__1S178 = _M0Lm6tag__1S177;
                    _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
                    _M0Lm6tag__4S179 = _M0Lm9_2acursorS165;
                    _M0L6_2atmpS1121 = _M0Lm9_2acursorS165;
                    if (_M0L6_2atmpS1121 < _M0L6_2aendS164) {
                      int32_t _M0L6_2atmpS1123 = _M0Lm9_2acursorS165;
                      int32_t _M0L10next__charS212;
                      int32_t _M0L6_2atmpS1122;
                      moonbit_incref(_M0L7_2adataS162);
                      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS212
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1123);
                      _M0L6_2atmpS1122 = _M0Lm9_2acursorS165;
                      _M0Lm9_2acursorS165 = _M0L6_2atmpS1122 + 1;
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
          int32_t _M0L6_2atmpS1126;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1126 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1126 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1128 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS214;
            int32_t _M0L6_2atmpS1127;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS214
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1128);
            _M0L6_2atmpS1127 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1127 + 1;
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
          int32_t _M0L6_2atmpS1129;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1129 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1129 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1131 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS215;
            int32_t _M0L6_2atmpS1130;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS215
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1131);
            _M0L6_2atmpS1130 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1130 + 1;
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
      _M0L6_2atmpS1096 = _M0Lm9_2acursorS165;
      if (_M0L6_2atmpS1096 < _M0L6_2aendS164) {
        int32_t _M0L6_2atmpS1098 = _M0Lm9_2acursorS165;
        int32_t _M0L10next__charS195;
        int32_t _M0L6_2atmpS1097;
        moonbit_incref(_M0L7_2adataS162);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS195
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1098);
        _M0L6_2atmpS1097 = _M0Lm9_2acursorS165;
        _M0Lm9_2acursorS165 = _M0L6_2atmpS1097 + 1;
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
      _M0L6_2atmpS1093 = _M0Lm9_2acursorS165;
      if (_M0L6_2atmpS1093 < _M0L6_2aendS164) {
        int32_t _M0L6_2atmpS1095 = _M0Lm9_2acursorS165;
        int32_t _M0L10next__charS192;
        int32_t _M0L6_2atmpS1094;
        moonbit_incref(_M0L7_2adataS162);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS192
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1095);
        _M0L6_2atmpS1094 = _M0Lm9_2acursorS165;
        _M0Lm9_2acursorS165 = _M0L6_2atmpS1094 + 1;
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
      int32_t _M0L6_2atmpS1090 = _M0Lm20match__tag__saver__0S168;
      int32_t _M0L6_2atmpS1089 = _M0L6_2atmpS1090 + 1;
      int64_t _M0L6_2atmpS1086 = (int64_t)_M0L6_2atmpS1089;
      int32_t _M0L6_2atmpS1088 = _M0Lm20match__tag__saver__1S169;
      int64_t _M0L6_2atmpS1087 = (int64_t)_M0L6_2atmpS1088;
      struct _M0TPC16string10StringView _M0L11start__lineS182;
      int32_t _M0L6_2atmpS1085;
      int32_t _M0L6_2atmpS1084;
      int64_t _M0L6_2atmpS1081;
      int32_t _M0L6_2atmpS1083;
      int64_t _M0L6_2atmpS1082;
      struct _M0TPC16string10StringView _M0L13start__columnS183;
      int64_t _M0L6_2atmpS1078;
      int32_t _M0L6_2atmpS1080;
      int64_t _M0L6_2atmpS1079;
      struct _M0TPC16string10StringView _M0L8filenameS184;
      int32_t _M0L6_2atmpS1077;
      int32_t _M0L6_2atmpS1076;
      int64_t _M0L6_2atmpS1073;
      int32_t _M0L6_2atmpS1075;
      int64_t _M0L6_2atmpS1074;
      struct _M0TPC16string10StringView _M0L9end__lineS185;
      int32_t _M0L6_2atmpS1072;
      int32_t _M0L6_2atmpS1071;
      int64_t _M0L6_2atmpS1068;
      int32_t _M0L6_2atmpS1070;
      int64_t _M0L6_2atmpS1069;
      struct _M0TPC16string10StringView _M0L11end__columnS186;
      int32_t _M0L6_2atmpS1067;
      int32_t _M0L6_2atmpS1066;
      int64_t _M0L6_2atmpS1063;
      int32_t _M0L6_2atmpS1065;
      int64_t _M0L6_2atmpS1064;
      struct _M0TPC16string10StringView _M0L6_2atmpS1839;
      struct _M0TPB13SourceLocRepr* _block_2035;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS182
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1086, _M0L6_2atmpS1087);
      _M0L6_2atmpS1085 = _M0Lm20match__tag__saver__1S169;
      _M0L6_2atmpS1084 = _M0L6_2atmpS1085 + 1;
      _M0L6_2atmpS1081 = (int64_t)_M0L6_2atmpS1084;
      _M0L6_2atmpS1083 = _M0Lm20match__tag__saver__2S170;
      _M0L6_2atmpS1082 = (int64_t)_M0L6_2atmpS1083;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS183
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1081, _M0L6_2atmpS1082);
      _M0L6_2atmpS1078 = (int64_t)_M0L8_2astartS163;
      _M0L6_2atmpS1080 = _M0Lm20match__tag__saver__0S168;
      _M0L6_2atmpS1079 = (int64_t)_M0L6_2atmpS1080;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS184
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1078, _M0L6_2atmpS1079);
      _M0L6_2atmpS1077 = _M0Lm20match__tag__saver__2S170;
      _M0L6_2atmpS1076 = _M0L6_2atmpS1077 + 1;
      _M0L6_2atmpS1073 = (int64_t)_M0L6_2atmpS1076;
      _M0L6_2atmpS1075 = _M0Lm20match__tag__saver__3S171;
      _M0L6_2atmpS1074 = (int64_t)_M0L6_2atmpS1075;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS185
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1073, _M0L6_2atmpS1074);
      _M0L6_2atmpS1072 = _M0Lm20match__tag__saver__3S171;
      _M0L6_2atmpS1071 = _M0L6_2atmpS1072 + 1;
      _M0L6_2atmpS1068 = (int64_t)_M0L6_2atmpS1071;
      _M0L6_2atmpS1070 = _M0Lm20match__tag__saver__4S172;
      _M0L6_2atmpS1069 = (int64_t)_M0L6_2atmpS1070;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS186
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1068, _M0L6_2atmpS1069);
      _M0L6_2atmpS1067 = _M0Lm20match__tag__saver__4S172;
      _M0L6_2atmpS1066 = _M0L6_2atmpS1067 + 1;
      _M0L6_2atmpS1063 = (int64_t)_M0L6_2atmpS1066;
      _M0L6_2atmpS1065 = _M0Lm10match__endS167;
      _M0L6_2atmpS1064 = (int64_t)_M0L6_2atmpS1065;
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6_2atmpS1839
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1063, _M0L6_2atmpS1064);
      moonbit_decref(_M0L6_2atmpS1839.$0);
      _block_2035
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2035)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 5, 0);
      _block_2035->$0_0 = _M0L8filenameS184.$0;
      _block_2035->$0_1 = _M0L8filenameS184.$1;
      _block_2035->$0_2 = _M0L8filenameS184.$2;
      _block_2035->$1_0 = _M0L11start__lineS182.$0;
      _block_2035->$1_1 = _M0L11start__lineS182.$1;
      _block_2035->$1_2 = _M0L11start__lineS182.$2;
      _block_2035->$2_0 = _M0L13start__columnS183.$0;
      _block_2035->$2_1 = _M0L13start__columnS183.$1;
      _block_2035->$2_2 = _M0L13start__columnS183.$2;
      _block_2035->$3_0 = _M0L9end__lineS185.$0;
      _block_2035->$3_1 = _M0L9end__lineS185.$1;
      _block_2035->$3_2 = _M0L9end__lineS185.$2;
      _block_2035->$4_0 = _M0L11end__columnS186.$0;
      _block_2035->$4_1 = _M0L11end__columnS186.$1;
      _block_2035->$4_2 = _M0L11end__columnS186.$2;
      return _block_2035;
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
  int32_t _M0L6_2atmpS1062;
  struct _M0TPC16string10StringView _M0L6_2atmpS1060;
  struct _M0TPB6Logger _M0L6_2atmpS1061;
  #line 145 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 146 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS157 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS1062 = Moonbit_array_length(_M0L4selfS158);
  _M0L6_2atmpS1060
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1062, _M0L4selfS158
  };
  moonbit_incref(_M0L3bufS157);
  _M0L6_2atmpS1061
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS157
  };
  #line 147 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1060, _M0L6_2atmpS1061, _M0L5quoteS159);
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
    int32_t _M0L6_2atmpS1044;
    int32_t _M0L6_2atmpS1045;
    int32_t _M0L6_2atmpS1046;
    int32_t _tmp_2039;
    int32_t _tmp_2040;
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
        int32_t _M0L6_2atmpS1047;
        int32_t _M0L6_2atmpS1048;
        moonbit_incref(_M0L6_2aenvS150);
        #line 207 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 208 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_39.data);
        _M0L6_2atmpS1047 = _M0L1iS151 + 1;
        _M0L6_2atmpS1048 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1047;
        _M0L3segS152 = _M0L6_2atmpS1048;
        goto _2afor_153;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1049;
        int32_t _M0L6_2atmpS1050;
        moonbit_incref(_M0L6_2aenvS150);
        #line 212 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 213 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_40.data);
        _M0L6_2atmpS1049 = _M0L1iS151 + 1;
        _M0L6_2atmpS1050 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1049;
        _M0L3segS152 = _M0L6_2atmpS1050;
        goto _2afor_153;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1051;
        int32_t _M0L6_2atmpS1052;
        moonbit_incref(_M0L6_2aenvS150);
        #line 217 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 218 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_41.data);
        _M0L6_2atmpS1051 = _M0L1iS151 + 1;
        _M0L6_2atmpS1052 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1051;
        _M0L3segS152 = _M0L6_2atmpS1052;
        goto _2afor_153;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1053;
        int32_t _M0L6_2atmpS1054;
        moonbit_incref(_M0L6_2aenvS150);
        #line 222 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 223 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_42.data);
        _M0L6_2atmpS1053 = _M0L1iS151 + 1;
        _M0L6_2atmpS1054 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1053;
        _M0L3segS152 = _M0L6_2atmpS1054;
        goto _2afor_153;
        break;
      }
      default: {
        if (_M0L4codeS154 < 32) {
          int32_t _M0L6_2atmpS1056;
          moonbit_string_t _M0L6_2atmpS1055;
          int32_t _M0L6_2atmpS1057;
          int32_t _M0L6_2atmpS1058;
          moonbit_incref(_M0L6_2aenvS150);
          #line 228 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 229 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_43.data);
          _M0L6_2atmpS1056 = _M0L4codeS154 & 0xff;
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1055 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1056);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, _M0L6_2atmpS1055);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 231 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, 125);
          _M0L6_2atmpS1057 = _M0L1iS151 + 1;
          _M0L6_2atmpS1058 = _M0L1iS151 + 1;
          _M0L1iS151 = _M0L6_2atmpS1057;
          _M0L3segS152 = _M0L6_2atmpS1058;
          goto _2afor_153;
        } else {
          int32_t _M0L6_2atmpS1059 = _M0L1iS151 + 1;
          int32_t _tmp_2038 = _M0L3segS152;
          _M0L1iS151 = _M0L6_2atmpS1059;
          _M0L3segS152 = _tmp_2038;
          goto _2afor_153;
        }
        break;
      }
    }
    goto joinlet_2037;
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
    _M0L6_2atmpS1044 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS156);
    if (_M0L6loggerS147.$1) {
      moonbit_incref(_M0L6loggerS147.$1);
    }
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, _M0L6_2atmpS1044);
    _M0L6_2atmpS1045 = _M0L1iS151 + 1;
    _M0L6_2atmpS1046 = _M0L1iS151 + 1;
    _M0L1iS151 = _M0L6_2atmpS1045;
    _M0L3segS152 = _M0L6_2atmpS1046;
    continue;
    joinlet_2037:;
    _tmp_2039 = _M0L1iS151;
    _tmp_2040 = _M0L3segS152;
    _M0L1iS151 = _tmp_2039;
    _M0L3segS152 = _tmp_2040;
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
  struct _M0TPB6Logger _M0L8_2afieldS1840;
  int32_t _M0L6_2acntS1920;
  struct _M0TPB6Logger _M0L6loggerS143;
  #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L4selfS141
  = (struct _M0TPC16string10StringView){
    _M0L6_2aenvS142->$1_1, _M0L6_2aenvS142->$1_2, _M0L6_2aenvS142->$1_0
  };
  _M0L8_2afieldS1840
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS142->$0_0, _M0L6_2aenvS142->$0_1
  };
  _M0L6_2acntS1920 = Moonbit_object_header(_M0L6_2aenvS142)->rc;
  if (_M0L6_2acntS1920 > 1) {
    int32_t _M0L11_2anew__cntS1921 = _M0L6_2acntS1920 - 1;
    Moonbit_object_header(_M0L6_2aenvS142)->rc = _M0L11_2anew__cntS1921;
    moonbit_incref(_M0L4selfS141.$0);
    if (_M0L8_2afieldS1840.$1) {
      moonbit_incref(_M0L8_2afieldS1840.$1);
    }
  } else if (_M0L6_2acntS1920 == 1) {
    #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS142);
  }
  _M0L6loggerS143 = _M0L8_2afieldS1840;
  if (_M0L1iS144 > _M0L3segS145) {
    int64_t _M0L6_2atmpS1043 = (int64_t)_M0L1iS144;
    struct _M0TPC16string10StringView _M0L6_2atmpS1042;
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1042
    = _M0MPC16string10StringView11sub_2einner(_M0L4selfS141, _M0L3segS145, _M0L6_2atmpS1043);
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS143.$0->$method_2(_M0L6loggerS143.$1, _M0L6_2atmpS1042);
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
  moonbit_string_t _M0L3strS1039;
  int32_t _M0L5startS1041;
  int32_t _M0L6_2atmpS1040;
  int32_t _result_2041;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1039 = _M0L4selfS139.$0;
  _M0L5startS1041 = _M0L4selfS139.$1;
  _M0L6_2atmpS1040 = _M0L5startS1041 + _M0L5indexS140;
  _result_2041 = _M0L3strS1039[_M0L6_2atmpS1040];
  moonbit_decref(_M0L3strS1039);
  return _result_2041;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView _M0L4selfS132,
  int32_t _M0L5startS138,
  int64_t _M0L3endS134
) {
  moonbit_string_t _M0L3strS1038;
  int32_t _M0L8str__lenS131;
  int32_t _M0L8abs__endS133;
  int32_t _M0L10abs__startS137;
  int32_t _M0L5startS1026;
  int32_t _if__result_2042;
  #line 712 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1038 = _M0L4selfS132.$0;
  _M0L8str__lenS131 = Moonbit_array_length(_M0L3strS1038);
  if (_M0L3endS134 == 4294967296ll) {
    _M0L8abs__endS133 = _M0L4selfS132.$2;
  } else {
    int64_t _M0L7_2aSomeS135 = _M0L3endS134;
    int32_t _M0L6_2aendS136 = (int32_t)_M0L7_2aSomeS135;
    if (_M0L6_2aendS136 < 0) {
      int32_t _M0L3endS1036 = _M0L4selfS132.$2;
      _M0L8abs__endS133 = _M0L3endS1036 + _M0L6_2aendS136;
    } else {
      int32_t _M0L5startS1037 = _M0L4selfS132.$1;
      _M0L8abs__endS133 = _M0L5startS1037 + _M0L6_2aendS136;
    }
  }
  if (_M0L5startS138 < 0) {
    int32_t _M0L3endS1034 = _M0L4selfS132.$2;
    _M0L10abs__startS137 = _M0L3endS1034 + _M0L5startS138;
  } else {
    int32_t _M0L5startS1035 = _M0L4selfS132.$1;
    _M0L10abs__startS137 = _M0L5startS1035 + _M0L5startS138;
  }
  _M0L5startS1026 = _M0L4selfS132.$1;
  if (_M0L10abs__startS137 >= _M0L5startS1026) {
    if (_M0L10abs__startS137 <= _M0L8abs__endS133) {
      int32_t _M0L3endS1025 = _M0L4selfS132.$2;
      _if__result_2042 = _M0L8abs__endS133 <= _M0L3endS1025;
    } else {
      _if__result_2042 = 0;
    }
  } else {
    _if__result_2042 = 0;
  }
  if (_if__result_2042) {
    moonbit_string_t _M0L3strS1033;
    if (_M0L10abs__startS137 < _M0L8str__lenS131) {
      moonbit_string_t _M0L3strS1029 = _M0L4selfS132.$0;
      int32_t _M0L6_2atmpS1028 = _M0L3strS1029[_M0L10abs__startS137];
      int32_t _M0L6_2atmpS1027;
      #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1027
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1028);
      if (!_M0L6_2atmpS1027) {
        
      } else {
        #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L8abs__endS133 < _M0L8str__lenS131) {
      moonbit_string_t _M0L3strS1032 = _M0L4selfS132.$0;
      int32_t _M0L6_2atmpS1031 = _M0L3strS1032[_M0L8abs__endS133];
      int32_t _M0L6_2atmpS1030;
      #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1030
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1031);
      if (!_M0L6_2atmpS1030) {
        
      } else {
        #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    _M0L3strS1033 = _M0L4selfS132.$0;
    return (struct _M0TPC16string10StringView){_M0L10abs__startS137,
                                                 _M0L8abs__endS133,
                                                 _M0L3strS1033};
  } else {
    moonbit_decref(_M0L4selfS132.$0);
    #line 732 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS130
) {
  int32_t _M0L3endS1023;
  int32_t _M0L5startS1024;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1023 = _M0L4selfS130.$2;
  _M0L5startS1024 = _M0L4selfS130.$1;
  moonbit_decref(_M0L4selfS130.$0);
  return _M0L3endS1023 - _M0L5startS1024;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS129) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS128;
  int32_t _M0L6_2atmpS1020;
  int32_t _M0L6_2atmpS1019;
  int32_t _M0L6_2atmpS1022;
  int32_t _M0L6_2atmpS1021;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1018;
  #line 109 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS128 = _M0MPB13StringBuilder11new_2einner(0);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1020 = _M0IPC14byte4BytePB3Div3div(_M0L1bS129, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1019
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1020);
  moonbit_incref(_M0L7_2aselfS128);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS128, _M0L6_2atmpS1019);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1022 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS129, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1021
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1022);
  moonbit_incref(_M0L7_2aselfS128);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS128, _M0L6_2atmpS1021);
  _M0L6_2atmpS1018 = _M0L7_2aselfS128;
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1018);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t _M0L1iS127) {
  #line 110 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS127 < 10) {
    int32_t _M0L6_2atmpS1015;
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1015 = _M0IPC14byte4BytePB3Add3add(_M0L1iS127, 48);
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1015);
  } else {
    int32_t _M0L6_2atmpS1017;
    int32_t _M0L6_2atmpS1016;
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1017 = _M0IPC14byte4BytePB3Add3add(_M0L1iS127, 97);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1016 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1017, 10);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1016);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS125,
  int32_t _M0L4thatS126
) {
  int32_t _M0L6_2atmpS1013;
  int32_t _M0L6_2atmpS1014;
  int32_t _M0L6_2atmpS1012;
  #line 120 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1013 = (int32_t)_M0L4selfS125;
  _M0L6_2atmpS1014 = (int32_t)_M0L4thatS126;
  _M0L6_2atmpS1012 = _M0L6_2atmpS1013 - _M0L6_2atmpS1014;
  return _M0L6_2atmpS1012 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS123,
  int32_t _M0L4thatS124
) {
  int32_t _M0L6_2atmpS1010;
  int32_t _M0L6_2atmpS1011;
  int32_t _M0L6_2atmpS1009;
  #line 67 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1010 = (int32_t)_M0L4selfS123;
  _M0L6_2atmpS1011 = (int32_t)_M0L4thatS124;
  _M0L6_2atmpS1009 = _M0L6_2atmpS1010 % _M0L6_2atmpS1011;
  return _M0L6_2atmpS1009 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS121,
  int32_t _M0L4thatS122
) {
  int32_t _M0L6_2atmpS1007;
  int32_t _M0L6_2atmpS1008;
  int32_t _M0L6_2atmpS1006;
  #line 62 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1007 = (int32_t)_M0L4selfS121;
  _M0L6_2atmpS1008 = (int32_t)_M0L4thatS122;
  _M0L6_2atmpS1006 = _M0L6_2atmpS1007 / _M0L6_2atmpS1008;
  return _M0L6_2atmpS1006 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS119,
  int32_t _M0L4thatS120
) {
  int32_t _M0L6_2atmpS1004;
  int32_t _M0L6_2atmpS1005;
  int32_t _M0L6_2atmpS1003;
  #line 106 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1004 = (int32_t)_M0L4selfS119;
  _M0L6_2atmpS1005 = (int32_t)_M0L4thatS120;
  _M0L6_2atmpS1003 = _M0L6_2atmpS1004 + _M0L6_2atmpS1005;
  return _M0L6_2atmpS1003 & 0xff;
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS113
) {
  int32_t _M0L17codepoint__lengthS112;
  int32_t _M0L6_2atmpS1002;
  moonbit_bytes_t _M0L4dataS114;
  int32_t _M0L1iS115;
  int32_t _M0L12utf16__indexS116;
  #line 102 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS113);
  #line 104 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS112
  = _M0MPC16string6String20char__length_2einner(_M0L1sS113, 0, 4294967296ll);
  _M0L6_2atmpS1002 = _M0L17codepoint__lengthS112 * 4;
  _M0L4dataS114 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1002, 0);
  _M0L1iS115 = 0;
  _M0L12utf16__indexS116 = 0;
  while (1) {
    if (_M0L1iS115 < _M0L17codepoint__lengthS112) {
      int32_t _M0L6_2atmpS999;
      int32_t _M0L1cS117;
      int32_t _M0L6_2atmpS1000;
      int32_t _M0L6_2atmpS1001;
      moonbit_incref(_M0L1sS113);
      #line 109 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS999
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS113, _M0L12utf16__indexS116);
      _M0L1cS117 = _M0L6_2atmpS999;
      if (_M0L1cS117 > 65535) {
        int32_t _M0L6_2atmpS967 = _M0L1iS115 * 4;
        int32_t _M0L6_2atmpS969 = _M0L1cS117 & 255;
        int32_t _M0L6_2atmpS968 = _M0L6_2atmpS969 & 0xff;
        int32_t _M0L6_2atmpS974;
        int32_t _M0L6_2atmpS970;
        int32_t _M0L6_2atmpS973;
        int32_t _M0L6_2atmpS972;
        int32_t _M0L6_2atmpS971;
        int32_t _M0L6_2atmpS979;
        int32_t _M0L6_2atmpS975;
        int32_t _M0L6_2atmpS978;
        int32_t _M0L6_2atmpS977;
        int32_t _M0L6_2atmpS976;
        int32_t _M0L6_2atmpS984;
        int32_t _M0L6_2atmpS980;
        int32_t _M0L6_2atmpS983;
        int32_t _M0L6_2atmpS982;
        int32_t _M0L6_2atmpS981;
        int32_t _M0L6_2atmpS985;
        int32_t _M0L6_2atmpS986;
        if (
          _M0L6_2atmpS967 < 0
          || _M0L6_2atmpS967 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 111 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS967] = _M0L6_2atmpS968;
        _M0L6_2atmpS974 = _M0L1iS115 * 4;
        _M0L6_2atmpS970 = _M0L6_2atmpS974 + 1;
        _M0L6_2atmpS973 = _M0L1cS117 >> 8;
        _M0L6_2atmpS972 = _M0L6_2atmpS973 & 255;
        _M0L6_2atmpS971 = _M0L6_2atmpS972 & 0xff;
        if (
          _M0L6_2atmpS970 < 0
          || _M0L6_2atmpS970 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 112 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS970] = _M0L6_2atmpS971;
        _M0L6_2atmpS979 = _M0L1iS115 * 4;
        _M0L6_2atmpS975 = _M0L6_2atmpS979 + 2;
        _M0L6_2atmpS978 = _M0L1cS117 >> 16;
        _M0L6_2atmpS977 = _M0L6_2atmpS978 & 255;
        _M0L6_2atmpS976 = _M0L6_2atmpS977 & 0xff;
        if (
          _M0L6_2atmpS975 < 0
          || _M0L6_2atmpS975 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 113 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS975] = _M0L6_2atmpS976;
        _M0L6_2atmpS984 = _M0L1iS115 * 4;
        _M0L6_2atmpS980 = _M0L6_2atmpS984 + 3;
        _M0L6_2atmpS983 = _M0L1cS117 >> 24;
        _M0L6_2atmpS982 = _M0L6_2atmpS983 & 255;
        _M0L6_2atmpS981 = _M0L6_2atmpS982 & 0xff;
        if (
          _M0L6_2atmpS980 < 0
          || _M0L6_2atmpS980 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 114 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS980] = _M0L6_2atmpS981;
        _M0L6_2atmpS985 = _M0L1iS115 + 1;
        _M0L6_2atmpS986 = _M0L12utf16__indexS116 + 2;
        _M0L1iS115 = _M0L6_2atmpS985;
        _M0L12utf16__indexS116 = _M0L6_2atmpS986;
        continue;
      } else {
        int32_t _M0L6_2atmpS987 = _M0L1iS115 * 4;
        int32_t _M0L6_2atmpS989 = _M0L1cS117 & 255;
        int32_t _M0L6_2atmpS988 = _M0L6_2atmpS989 & 0xff;
        int32_t _M0L6_2atmpS994;
        int32_t _M0L6_2atmpS990;
        int32_t _M0L6_2atmpS993;
        int32_t _M0L6_2atmpS992;
        int32_t _M0L6_2atmpS991;
        int32_t _M0L6_2atmpS996;
        int32_t _M0L6_2atmpS995;
        int32_t _M0L6_2atmpS998;
        int32_t _M0L6_2atmpS997;
        if (
          _M0L6_2atmpS987 < 0
          || _M0L6_2atmpS987 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 117 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS987] = _M0L6_2atmpS988;
        _M0L6_2atmpS994 = _M0L1iS115 * 4;
        _M0L6_2atmpS990 = _M0L6_2atmpS994 + 1;
        _M0L6_2atmpS993 = _M0L1cS117 >> 8;
        _M0L6_2atmpS992 = _M0L6_2atmpS993 & 255;
        _M0L6_2atmpS991 = _M0L6_2atmpS992 & 0xff;
        if (
          _M0L6_2atmpS990 < 0
          || _M0L6_2atmpS990 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 118 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS990] = _M0L6_2atmpS991;
        _M0L6_2atmpS996 = _M0L1iS115 * 4;
        _M0L6_2atmpS995 = _M0L6_2atmpS996 + 2;
        if (
          _M0L6_2atmpS995 < 0
          || _M0L6_2atmpS995 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 119 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS995] = 0;
        _M0L6_2atmpS998 = _M0L1iS115 * 4;
        _M0L6_2atmpS997 = _M0L6_2atmpS998 + 3;
        if (
          _M0L6_2atmpS997 < 0
          || _M0L6_2atmpS997 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 120 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS997] = 0;
      }
      _M0L6_2atmpS1000 = _M0L1iS115 + 1;
      _M0L6_2atmpS1001 = _M0L12utf16__indexS116 + 1;
      _M0L1iS115 = _M0L6_2atmpS1000;
      _M0L12utf16__indexS116 = _M0L6_2atmpS1001;
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
    int32_t _M0L6_2atmpS966 = _M0L5indexS110 + 1;
    int32_t _M0L2c2S111 = _M0L4selfS109[_M0L6_2atmpS966];
    int32_t _M0L6_2atmpS964;
    int32_t _M0L6_2atmpS965;
    moonbit_decref(_M0L4selfS109);
    _M0L6_2atmpS964 = (int32_t)_M0L2c1S108;
    _M0L6_2atmpS965 = (int32_t)_M0L2c2S111;
    #line 96 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS964, _M0L6_2atmpS965);
  } else {
    moonbit_decref(_M0L4selfS109);
    #line 98 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S108);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS107) {
  int32_t _M0L6_2atmpS963;
  #line 68 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS963 = (int32_t)_M0L4selfS107;
  return _M0L6_2atmpS963;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS105,
  int32_t _M0L8trailingS106
) {
  int32_t _M0L6_2atmpS962;
  int32_t _M0L6_2atmpS961;
  int32_t _M0L6_2atmpS960;
  int32_t _M0L6_2atmpS959;
  int32_t _M0L6_2atmpS958;
  #line 40 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS962 = _M0L7leadingS105 - 55296;
  _M0L6_2atmpS961 = _M0L6_2atmpS962 * 1024;
  _M0L6_2atmpS960 = _M0L6_2atmpS961 + _M0L8trailingS106;
  _M0L6_2atmpS959 = _M0L6_2atmpS960 - 56320;
  _M0L6_2atmpS958 = _M0L6_2atmpS959 + 65536;
  return _M0L6_2atmpS958;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS98,
  int32_t _M0L13start__offsetS99,
  int64_t _M0L11end__offsetS96
) {
  int32_t _M0L11end__offsetS95;
  int32_t _if__result_2044;
  #line 60 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS96 == 4294967296ll) {
    _M0L11end__offsetS95 = Moonbit_array_length(_M0L4selfS98);
  } else {
    int64_t _M0L7_2aSomeS97 = _M0L11end__offsetS96;
    _M0L11end__offsetS95 = (int32_t)_M0L7_2aSomeS97;
  }
  if (_M0L13start__offsetS99 >= 0) {
    if (_M0L13start__offsetS99 <= _M0L11end__offsetS95) {
      int32_t _M0L6_2atmpS951 = Moonbit_array_length(_M0L4selfS98);
      _if__result_2044 = _M0L11end__offsetS95 <= _M0L6_2atmpS951;
    } else {
      _if__result_2044 = 0;
    }
  } else {
    _if__result_2044 = 0;
  }
  if (_if__result_2044) {
    int32_t _M0L12utf16__indexS100 = _M0L13start__offsetS99;
    int32_t _M0L11char__countS101 = 0;
    while (1) {
      if (_M0L12utf16__indexS100 < _M0L11end__offsetS95) {
        int32_t _M0L2c1S102 = _M0L4selfS98[_M0L12utf16__indexS100];
        int32_t _if__result_2046;
        int32_t _M0L6_2atmpS956;
        int32_t _M0L6_2atmpS957;
        #line 76 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S102)) {
          int32_t _M0L6_2atmpS952 = _M0L12utf16__indexS100 + 1;
          _if__result_2046 = _M0L6_2atmpS952 < _M0L11end__offsetS95;
        } else {
          _if__result_2046 = 0;
        }
        if (_if__result_2046) {
          int32_t _M0L6_2atmpS955 = _M0L12utf16__indexS100 + 1;
          int32_t _M0L2c2S103 = _M0L4selfS98[_M0L6_2atmpS955];
          #line 78 "/Users/user/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S103)) {
            int32_t _M0L6_2atmpS953 = _M0L12utf16__indexS100 + 2;
            int32_t _M0L6_2atmpS954 = _M0L11char__countS101 + 1;
            _M0L12utf16__indexS100 = _M0L6_2atmpS953;
            _M0L11char__countS101 = _M0L6_2atmpS954;
            continue;
          } else {
            #line 81 "/Users/user/.moon/lib/core/builtin/string.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_20.data);
          }
        }
        _M0L6_2atmpS956 = _M0L12utf16__indexS100 + 1;
        _M0L6_2atmpS957 = _M0L11char__countS101 + 1;
        _M0L12utf16__indexS100 = _M0L6_2atmpS956;
        _M0L11char__countS101 = _M0L6_2atmpS957;
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
    return _M0FPC15abort5abortGiE((moonbit_string_t)moonbit_string_literal_44.data);
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
    int32_t _M0L6_2atmpS903 = _M0L3lenS73 - _M0L3remS75;
    if (_M0L1iS76 < _M0L6_2atmpS903) {
      int32_t _M0L6_2atmpS925;
      int32_t _M0L2b0S77;
      int32_t _M0L6_2atmpS924;
      int32_t _M0L6_2atmpS923;
      int32_t _M0L2b1S78;
      int32_t _M0L6_2atmpS922;
      int32_t _M0L6_2atmpS921;
      int32_t _M0L2b2S79;
      int32_t _M0L6_2atmpS920;
      int32_t _M0L6_2atmpS919;
      int32_t _M0L2x0S80;
      int32_t _M0L6_2atmpS918;
      int32_t _M0L6_2atmpS915;
      int32_t _M0L6_2atmpS917;
      int32_t _M0L6_2atmpS916;
      int32_t _M0L6_2atmpS914;
      int32_t _M0L2x1S81;
      int32_t _M0L6_2atmpS913;
      int32_t _M0L6_2atmpS910;
      int32_t _M0L6_2atmpS912;
      int32_t _M0L6_2atmpS911;
      int32_t _M0L6_2atmpS909;
      int32_t _M0L2x2S82;
      int32_t _M0L6_2atmpS908;
      int32_t _M0L2x3S83;
      int32_t _M0L6_2atmpS904;
      int32_t _M0L6_2atmpS905;
      int32_t _M0L6_2atmpS906;
      int32_t _M0L6_2atmpS907;
      int32_t _M0L6_2atmpS926;
      if (_M0L1iS76 < 0 || _M0L1iS76 >= Moonbit_array_length(_M0L4dataS74)) {
        #line 67 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS925 = (int32_t)_M0L4dataS74[_M0L1iS76];
      _M0L2b0S77 = (int32_t)_M0L6_2atmpS925;
      _M0L6_2atmpS924 = _M0L1iS76 + 1;
      if (
        _M0L6_2atmpS924 < 0
        || _M0L6_2atmpS924 >= Moonbit_array_length(_M0L4dataS74)
      ) {
        #line 68 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS923 = (int32_t)_M0L4dataS74[_M0L6_2atmpS924];
      _M0L2b1S78 = (int32_t)_M0L6_2atmpS923;
      _M0L6_2atmpS922 = _M0L1iS76 + 2;
      if (
        _M0L6_2atmpS922 < 0
        || _M0L6_2atmpS922 >= Moonbit_array_length(_M0L4dataS74)
      ) {
        #line 69 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS921 = (int32_t)_M0L4dataS74[_M0L6_2atmpS922];
      _M0L2b2S79 = (int32_t)_M0L6_2atmpS921;
      _M0L6_2atmpS920 = _M0L2b0S77 & 252;
      _M0L6_2atmpS919 = _M0L6_2atmpS920 >> 2;
      if (
        _M0L6_2atmpS919 < 0
        || _M0L6_2atmpS919
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 70 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S80 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS919];
      _M0L6_2atmpS918 = _M0L2b0S77 & 3;
      _M0L6_2atmpS915 = _M0L6_2atmpS918 << 4;
      _M0L6_2atmpS917 = _M0L2b1S78 & 240;
      _M0L6_2atmpS916 = _M0L6_2atmpS917 >> 4;
      _M0L6_2atmpS914 = _M0L6_2atmpS915 | _M0L6_2atmpS916;
      if (
        _M0L6_2atmpS914 < 0
        || _M0L6_2atmpS914
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 71 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S81 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS914];
      _M0L6_2atmpS913 = _M0L2b1S78 & 15;
      _M0L6_2atmpS910 = _M0L6_2atmpS913 << 2;
      _M0L6_2atmpS912 = _M0L2b2S79 & 192;
      _M0L6_2atmpS911 = _M0L6_2atmpS912 >> 6;
      _M0L6_2atmpS909 = _M0L6_2atmpS910 | _M0L6_2atmpS911;
      if (
        _M0L6_2atmpS909 < 0
        || _M0L6_2atmpS909
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 72 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S82 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS909];
      _M0L6_2atmpS908 = _M0L2b2S79 & 63;
      if (
        _M0L6_2atmpS908 < 0
        || _M0L6_2atmpS908
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 73 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S83 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS908];
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS904 = _M0MPC14byte4Byte8to__char(_M0L2x0S80);
      moonbit_incref(_M0L3bufS72);
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS904);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS905 = _M0MPC14byte4Byte8to__char(_M0L2x1S81);
      moonbit_incref(_M0L3bufS72);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS905);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS906 = _M0MPC14byte4Byte8to__char(_M0L2x2S82);
      moonbit_incref(_M0L3bufS72);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS906);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS907 = _M0MPC14byte4Byte8to__char(_M0L2x3S83);
      moonbit_incref(_M0L3bufS72);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS907);
      _M0L6_2atmpS926 = _M0L1iS76 + 3;
      _M0L1iS76 = _M0L6_2atmpS926;
      continue;
    }
    break;
  }
  if (_M0L3remS75 == 1) {
    int32_t _M0L6_2atmpS934 = _M0L3lenS73 - 1;
    int32_t _M0L6_2atmpS933;
    int32_t _M0L2b0S85;
    int32_t _M0L6_2atmpS932;
    int32_t _M0L6_2atmpS931;
    int32_t _M0L2x0S86;
    int32_t _M0L6_2atmpS930;
    int32_t _M0L6_2atmpS929;
    int32_t _M0L2x1S87;
    int32_t _M0L6_2atmpS927;
    int32_t _M0L6_2atmpS928;
    if (
      _M0L6_2atmpS934 < 0
      || _M0L6_2atmpS934 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 80 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS933 = (int32_t)_M0L4dataS74[_M0L6_2atmpS934];
    moonbit_decref(_M0L4dataS74);
    _M0L2b0S85 = (int32_t)_M0L6_2atmpS933;
    _M0L6_2atmpS932 = _M0L2b0S85 & 252;
    _M0L6_2atmpS931 = _M0L6_2atmpS932 >> 2;
    if (
      _M0L6_2atmpS931 < 0
      || _M0L6_2atmpS931
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 81 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S86 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS931];
    _M0L6_2atmpS930 = _M0L2b0S85 & 3;
    _M0L6_2atmpS929 = _M0L6_2atmpS930 << 4;
    if (
      _M0L6_2atmpS929 < 0
      || _M0L6_2atmpS929
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 82 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S87 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS929];
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS927 = _M0MPC14byte4Byte8to__char(_M0L2x0S86);
    moonbit_incref(_M0L3bufS72);
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS927);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS928 = _M0MPC14byte4Byte8to__char(_M0L2x1S87);
    moonbit_incref(_M0L3bufS72);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS928);
    moonbit_incref(_M0L3bufS72);
    #line 85 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
    moonbit_incref(_M0L3bufS72);
    #line 86 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
  } else if (_M0L3remS75 == 2) {
    int32_t _M0L6_2atmpS950 = _M0L3lenS73 - 2;
    int32_t _M0L6_2atmpS949;
    int32_t _M0L2b0S88;
    int32_t _M0L6_2atmpS948;
    int32_t _M0L6_2atmpS947;
    int32_t _M0L2b1S89;
    int32_t _M0L6_2atmpS946;
    int32_t _M0L6_2atmpS945;
    int32_t _M0L2x0S90;
    int32_t _M0L6_2atmpS944;
    int32_t _M0L6_2atmpS941;
    int32_t _M0L6_2atmpS943;
    int32_t _M0L6_2atmpS942;
    int32_t _M0L6_2atmpS940;
    int32_t _M0L2x1S91;
    int32_t _M0L6_2atmpS939;
    int32_t _M0L6_2atmpS938;
    int32_t _M0L2x2S92;
    int32_t _M0L6_2atmpS935;
    int32_t _M0L6_2atmpS936;
    int32_t _M0L6_2atmpS937;
    if (
      _M0L6_2atmpS950 < 0
      || _M0L6_2atmpS950 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 88 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS949 = (int32_t)_M0L4dataS74[_M0L6_2atmpS950];
    _M0L2b0S88 = (int32_t)_M0L6_2atmpS949;
    _M0L6_2atmpS948 = _M0L3lenS73 - 1;
    if (
      _M0L6_2atmpS948 < 0
      || _M0L6_2atmpS948 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 89 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS947 = (int32_t)_M0L4dataS74[_M0L6_2atmpS948];
    moonbit_decref(_M0L4dataS74);
    _M0L2b1S89 = (int32_t)_M0L6_2atmpS947;
    _M0L6_2atmpS946 = _M0L2b0S88 & 252;
    _M0L6_2atmpS945 = _M0L6_2atmpS946 >> 2;
    if (
      _M0L6_2atmpS945 < 0
      || _M0L6_2atmpS945
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 90 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S90 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS945];
    _M0L6_2atmpS944 = _M0L2b0S88 & 3;
    _M0L6_2atmpS941 = _M0L6_2atmpS944 << 4;
    _M0L6_2atmpS943 = _M0L2b1S89 & 240;
    _M0L6_2atmpS942 = _M0L6_2atmpS943 >> 4;
    _M0L6_2atmpS940 = _M0L6_2atmpS941 | _M0L6_2atmpS942;
    if (
      _M0L6_2atmpS940 < 0
      || _M0L6_2atmpS940
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 91 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S91 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS940];
    _M0L6_2atmpS939 = _M0L2b1S89 & 15;
    _M0L6_2atmpS938 = _M0L6_2atmpS939 << 2;
    if (
      _M0L6_2atmpS938 < 0
      || _M0L6_2atmpS938
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 92 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S92 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS938];
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS935 = _M0MPC14byte4Byte8to__char(_M0L2x0S90);
    moonbit_incref(_M0L3bufS72);
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS935);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS936 = _M0MPC14byte4Byte8to__char(_M0L2x1S91);
    moonbit_incref(_M0L3bufS72);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS936);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS937 = _M0MPC14byte4Byte8to__char(_M0L2x2S92);
    moonbit_incref(_M0L3bufS72);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS937);
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
    int32_t _M0L3lenS882 = _M0L4selfS70->$1;
    int32_t _M0L6_2atmpS881 = _M0L3lenS882 + 1;
    uint16_t* _M0L4dataS883;
    int32_t _M0L3lenS884;
    int32_t _M0L6_2atmpS885;
    int32_t _M0L3lenS887;
    int32_t _M0L6_2atmpS886;
    moonbit_incref(_M0L4selfS70);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS70, _M0L6_2atmpS881);
    _M0L4dataS883 = _M0L4selfS70->$0;
    _M0L3lenS884 = _M0L4selfS70->$1;
    moonbit_incref(_M0L4dataS883);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS885 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS68);
    if (
      _M0L3lenS884 < 0 || _M0L3lenS884 >= Moonbit_array_length(_M0L4dataS883)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS883[_M0L3lenS884] = _M0L6_2atmpS885;
    moonbit_decref(_M0L4dataS883);
    _M0L3lenS887 = _M0L4selfS70->$1;
    _M0L6_2atmpS886 = _M0L3lenS887 + 1;
    _M0L4selfS70->$1 = _M0L6_2atmpS886;
    moonbit_decref(_M0L4selfS70);
  } else if (_M0L4codeS68 <= 1114111u) {
    int32_t _M0L3lenS889 = _M0L4selfS70->$1;
    int32_t _M0L6_2atmpS888 = _M0L3lenS889 + 2;
    uint32_t _M0L4codeS71;
    uint16_t* _M0L4dataS890;
    int32_t _M0L3lenS891;
    uint32_t _M0L6_2atmpS894;
    uint32_t _M0L6_2atmpS893;
    int32_t _M0L6_2atmpS892;
    uint16_t* _M0L4dataS895;
    int32_t _M0L3lenS900;
    int32_t _M0L6_2atmpS896;
    uint32_t _M0L6_2atmpS899;
    uint32_t _M0L6_2atmpS898;
    int32_t _M0L6_2atmpS897;
    int32_t _M0L3lenS902;
    int32_t _M0L6_2atmpS901;
    moonbit_incref(_M0L4selfS70);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS70, _M0L6_2atmpS888);
    _M0L4codeS71 = _M0L4codeS68 - 65536u;
    _M0L4dataS890 = _M0L4selfS70->$0;
    _M0L3lenS891 = _M0L4selfS70->$1;
    _M0L6_2atmpS894 = _M0L4codeS71 >> 10;
    _M0L6_2atmpS893 = 55296u + _M0L6_2atmpS894;
    moonbit_incref(_M0L4dataS890);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS892 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS893);
    if (
      _M0L3lenS891 < 0 || _M0L3lenS891 >= Moonbit_array_length(_M0L4dataS890)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS890[_M0L3lenS891] = _M0L6_2atmpS892;
    moonbit_decref(_M0L4dataS890);
    _M0L4dataS895 = _M0L4selfS70->$0;
    _M0L3lenS900 = _M0L4selfS70->$1;
    _M0L6_2atmpS896 = _M0L3lenS900 + 1;
    _M0L6_2atmpS899 = _M0L4codeS71 & 1023u;
    _M0L6_2atmpS898 = 56320u + _M0L6_2atmpS899;
    moonbit_incref(_M0L4dataS895);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS897 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS898);
    if (
      _M0L6_2atmpS896 < 0
      || _M0L6_2atmpS896 >= Moonbit_array_length(_M0L4dataS895)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS895[_M0L6_2atmpS896] = _M0L6_2atmpS897;
    moonbit_decref(_M0L4dataS895);
    _M0L3lenS902 = _M0L4selfS70->$1;
    _M0L6_2atmpS901 = _M0L3lenS902 + 2;
    _M0L4selfS70->$1 = _M0L6_2atmpS901;
    moonbit_decref(_M0L4selfS70);
  } else {
    moonbit_decref(_M0L4selfS70);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_45.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS62,
  int32_t _M0L8requiredS63
) {
  uint16_t* _M0L4dataS880;
  int32_t _M0L12current__lenS61;
  int32_t _M0L13enough__spaceS64;
  int32_t _M0L13enough__spaceS65;
  int32_t _M0L6_2atmpS878;
  uint16_t* _M0L9new__dataS67;
  uint16_t* _M0L4dataS876;
  int32_t _M0L3lenS877;
  uint16_t* _M0L6_2aoldS1850;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS880 = _M0L4selfS62->$0;
  _M0L12current__lenS61 = Moonbit_array_length(_M0L4dataS880);
  if (_M0L8requiredS63 <= _M0L12current__lenS61) {
    moonbit_decref(_M0L4selfS62);
    return 0;
  }
  _M0L13enough__spaceS65 = _M0L12current__lenS61;
  while (1) {
    if (_M0L13enough__spaceS65 < _M0L8requiredS63) {
      int32_t _M0L6_2atmpS879 = _M0L13enough__spaceS65 * 2;
      _M0L13enough__spaceS65 = _M0L6_2atmpS879;
      continue;
    } else {
      _M0L13enough__spaceS64 = _M0L13enough__spaceS65;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS878 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS67
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS64, _M0L6_2atmpS878);
  _M0L4dataS876 = _M0L4selfS62->$0;
  _M0L3lenS877 = _M0L4selfS62->$1;
  moonbit_incref(_M0L4dataS876);
  moonbit_incref(_M0L9new__dataS67);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS67, 0, _M0L4dataS876, 0, _M0L3lenS877);
  _M0L6_2aoldS1850 = _M0L4selfS62->$0;
  moonbit_decref(_M0L6_2aoldS1850);
  _M0L4selfS62->$0 = _M0L9new__dataS67;
  moonbit_decref(_M0L4selfS62);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS60) {
  int32_t _M0L6_2atmpS875;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS875 = *(int32_t*)&_M0L4selfS60;
  return (uint16_t)_M0L6_2atmpS875;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS874;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS874 = _M0L4selfS59;
  return *(uint32_t*)&_M0L6_2atmpS874;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS57
) {
  int32_t _M0L3lenS866;
  uint16_t* _M0L4dataS868;
  int32_t _M0L6_2atmpS867;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS866 = _M0L4selfS57->$1;
  _M0L4dataS868 = _M0L4selfS57->$0;
  _M0L6_2atmpS867 = Moonbit_array_length(_M0L4dataS868);
  if (_M0L3lenS866 == _M0L6_2atmpS867) {
    uint16_t* _M0L8_2afieldS1853 = _M0L4selfS57->$0;
    int32_t _M0L6_2acntS1922 = Moonbit_object_header(_M0L4selfS57)->rc;
    uint16_t* _M0L4dataS869;
    if (_M0L6_2acntS1922 > 1) {
      int32_t _M0L11_2anew__cntS1923 = _M0L6_2acntS1922 - 1;
      Moonbit_object_header(_M0L4selfS57)->rc = _M0L11_2anew__cntS1923;
      moonbit_incref(_M0L8_2afieldS1853);
    } else if (_M0L6_2acntS1922 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS57);
    }
    _M0L4dataS869 = _M0L8_2afieldS1853;
    return _M0L4dataS869;
  } else {
    int32_t _M0L3lenS872 = _M0L4selfS57->$1;
    int32_t _M0L6_2atmpS873;
    uint16_t* _M0L4dataS58;
    uint16_t* _M0L4dataS870;
    int32_t _M0L3lenS871;
    int32_t _M0L6_2acntS1924;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS873 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS58
    = (uint16_t*)moonbit_make_string(_M0L3lenS872, _M0L6_2atmpS873);
    _M0L4dataS870 = _M0L4selfS57->$0;
    _M0L3lenS871 = _M0L4selfS57->$1;
    _M0L6_2acntS1924 = Moonbit_object_header(_M0L4selfS57)->rc;
    if (_M0L6_2acntS1924 > 1) {
      int32_t _M0L11_2anew__cntS1925 = _M0L6_2acntS1924 - 1;
      Moonbit_object_header(_M0L4selfS57)->rc = _M0L11_2anew__cntS1925;
      moonbit_incref(_M0L4dataS870);
    } else if (_M0L6_2acntS1924 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS57);
    }
    moonbit_incref(_M0L4dataS58);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS58, 0, _M0L4dataS870, 0, _M0L3lenS871);
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
  struct _M0TPB13StringBuilder* _block_2049;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS55 < 1) {
    _M0L7initialS54 = 1;
  } else {
    int32_t _M0L6_2atmpS865 = _M0L10size__hintS55 + 1;
    _M0L7initialS54 = _M0L6_2atmpS865 / 2;
  }
  _M0L4dataS56 = (uint16_t*)moonbit_make_string(_M0L7initialS54, 0);
  _block_2049
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2049)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2049->$0 = _M0L4dataS56;
  _block_2049->$1 = 0;
  return _block_2049;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS53) {
  int32_t _M0L6_2atmpS864;
  #line 1867 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS864 = (int32_t)_M0L4selfS53;
  return _M0L6_2atmpS864;
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
  int32_t _if__result_2050;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS16 == _M0L3srcS17) {
    _if__result_2050 = _M0L11dst__offsetS18 < _M0L11src__offsetS19;
  } else {
    _if__result_2050 = 0;
  }
  if (_if__result_2050) {
    int32_t _M0L1iS20 = 0;
    while (1) {
      if (_M0L1iS20 < _M0L3lenS21) {
        int32_t _M0L6_2atmpS837 = _M0L11dst__offsetS18 + _M0L1iS20;
        int32_t _M0L6_2atmpS839 = _M0L11src__offsetS19 + _M0L1iS20;
        int32_t _M0L6_2atmpS838;
        int32_t _M0L6_2atmpS840;
        if (
          _M0L6_2atmpS839 < 0
          || _M0L6_2atmpS839 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS838 = (int32_t)_M0L3srcS17[_M0L6_2atmpS839];
        if (
          _M0L6_2atmpS837 < 0
          || _M0L6_2atmpS837 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS837] = _M0L6_2atmpS838;
        _M0L6_2atmpS840 = _M0L1iS20 + 1;
        _M0L1iS20 = _M0L6_2atmpS840;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS845 = _M0L3lenS21 - 1;
    int32_t _M0L1iS23 = _M0L6_2atmpS845;
    while (1) {
      if (_M0L1iS23 >= 0) {
        int32_t _M0L6_2atmpS841 = _M0L11dst__offsetS18 + _M0L1iS23;
        int32_t _M0L6_2atmpS843 = _M0L11src__offsetS19 + _M0L1iS23;
        int32_t _M0L6_2atmpS842;
        int32_t _M0L6_2atmpS844;
        if (
          _M0L6_2atmpS843 < 0
          || _M0L6_2atmpS843 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS842 = (int32_t)_M0L3srcS17[_M0L6_2atmpS843];
        if (
          _M0L6_2atmpS841 < 0
          || _M0L6_2atmpS841 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS841] = _M0L6_2atmpS842;
        _M0L6_2atmpS844 = _M0L1iS23 - 1;
        _M0L1iS23 = _M0L6_2atmpS844;
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
  int32_t _if__result_2053;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS25 == _M0L3srcS26) {
    _if__result_2053 = _M0L11dst__offsetS27 < _M0L11src__offsetS28;
  } else {
    _if__result_2053 = 0;
  }
  if (_if__result_2053) {
    int32_t _M0L1iS29 = 0;
    while (1) {
      if (_M0L1iS29 < _M0L3lenS30) {
        int32_t _M0L6_2atmpS846 = _M0L11dst__offsetS27 + _M0L1iS29;
        int32_t _M0L6_2atmpS848 = _M0L11src__offsetS28 + _M0L1iS29;
        moonbit_string_t _M0L6_2atmpS847;
        moonbit_string_t _M0L6_2aoldS1856;
        int32_t _M0L6_2atmpS849;
        if (
          _M0L6_2atmpS848 < 0
          || _M0L6_2atmpS848 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS847 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS848];
        if (
          _M0L6_2atmpS846 < 0
          || _M0L6_2atmpS846 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1856 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS846];
        moonbit_incref(_M0L6_2atmpS847);
        moonbit_decref(_M0L6_2aoldS1856);
        _M0L3dstS25[_M0L6_2atmpS846] = _M0L6_2atmpS847;
        _M0L6_2atmpS849 = _M0L1iS29 + 1;
        _M0L1iS29 = _M0L6_2atmpS849;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS854 = _M0L3lenS30 - 1;
    int32_t _M0L1iS32 = _M0L6_2atmpS854;
    while (1) {
      if (_M0L1iS32 >= 0) {
        int32_t _M0L6_2atmpS850 = _M0L11dst__offsetS27 + _M0L1iS32;
        int32_t _M0L6_2atmpS852 = _M0L11src__offsetS28 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS851;
        moonbit_string_t _M0L6_2aoldS1858;
        int32_t _M0L6_2atmpS853;
        if (
          _M0L6_2atmpS852 < 0
          || _M0L6_2atmpS852 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS851 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS852];
        if (
          _M0L6_2atmpS850 < 0
          || _M0L6_2atmpS850 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1858 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS850];
        moonbit_incref(_M0L6_2atmpS851);
        moonbit_decref(_M0L6_2aoldS1858);
        _M0L3dstS25[_M0L6_2atmpS850] = _M0L6_2atmpS851;
        _M0L6_2atmpS853 = _M0L1iS32 - 1;
        _M0L1iS32 = _M0L6_2atmpS853;
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
  int32_t _if__result_2056;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS34 == _M0L3srcS35) {
    _if__result_2056 = _M0L11dst__offsetS36 < _M0L11src__offsetS37;
  } else {
    _if__result_2056 = 0;
  }
  if (_if__result_2056) {
    int32_t _M0L1iS38 = 0;
    while (1) {
      if (_M0L1iS38 < _M0L3lenS39) {
        int32_t _M0L6_2atmpS855 = _M0L11dst__offsetS36 + _M0L1iS38;
        int32_t _M0L6_2atmpS857 = _M0L11src__offsetS37 + _M0L1iS38;
        struct _M0TUsiE* _M0L6_2atmpS856;
        struct _M0TUsiE* _M0L6_2aoldS1860;
        int32_t _M0L6_2atmpS858;
        if (
          _M0L6_2atmpS857 < 0
          || _M0L6_2atmpS857 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS856 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS857];
        if (
          _M0L6_2atmpS855 < 0
          || _M0L6_2atmpS855 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1860 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS855];
        if (_M0L6_2atmpS856) {
          moonbit_incref(_M0L6_2atmpS856);
        }
        if (_M0L6_2aoldS1860) {
          moonbit_decref(_M0L6_2aoldS1860);
        }
        _M0L3dstS34[_M0L6_2atmpS855] = _M0L6_2atmpS856;
        _M0L6_2atmpS858 = _M0L1iS38 + 1;
        _M0L1iS38 = _M0L6_2atmpS858;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS863 = _M0L3lenS39 - 1;
    int32_t _M0L1iS41 = _M0L6_2atmpS863;
    while (1) {
      if (_M0L1iS41 >= 0) {
        int32_t _M0L6_2atmpS859 = _M0L11dst__offsetS36 + _M0L1iS41;
        int32_t _M0L6_2atmpS861 = _M0L11src__offsetS37 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS860;
        struct _M0TUsiE* _M0L6_2aoldS1862;
        int32_t _M0L6_2atmpS862;
        if (
          _M0L6_2atmpS861 < 0
          || _M0L6_2atmpS861 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS860 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS861];
        if (
          _M0L6_2atmpS859 < 0
          || _M0L6_2atmpS859 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1862 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS859];
        if (_M0L6_2atmpS860) {
          moonbit_incref(_M0L6_2atmpS860);
        }
        if (_M0L6_2aoldS1862) {
          moonbit_decref(_M0L6_2aoldS1862);
        }
        _M0L3dstS34[_M0L6_2atmpS859] = _M0L6_2atmpS860;
        _M0L6_2atmpS862 = _M0L1iS41 - 1;
        _M0L1iS41 = _M0L6_2atmpS862;
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
  uint32_t _M0L3accS836;
  uint32_t _M0L6_2atmpS835;
  #line 236 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS836 = _M0L4selfS14->$0;
  _M0L6_2atmpS835 = _M0L3accS836 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS835;
  #line 238 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS833;
  uint32_t _M0L6_2atmpS834;
  uint32_t _M0L6_2atmpS832;
  uint32_t _M0L6_2atmpS831;
  uint32_t _M0L6_2atmpS830;
  #line 451 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS833 = _M0L4selfS12->$0;
  _M0L6_2atmpS834 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS832 = _M0L3accS833 + _M0L6_2atmpS834;
  #line 452 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS831 = _M0FPB4rotl(_M0L6_2atmpS832, 17);
  _M0L6_2atmpS830 = _M0L6_2atmpS831 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS830;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS827;
  int32_t _M0L6_2atmpS829;
  uint32_t _M0L6_2atmpS828;
  #line 461 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS827 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS829 = 32 - _M0L1rS11;
  _M0L6_2atmpS828 = _M0L1xS10 >> (_M0L6_2atmpS829 & 31);
  return _M0L6_2atmpS827 | _M0L6_2atmpS828;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__5208S6,
  struct _M0TPB6Logger _M0L10_2ax__5209S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS1864;
  int32_t _M0L6_2acntS1926;
  moonbit_string_t _M0L15_2a_2aarg__5210S8;
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__5208S6;
  _M0L8_2afieldS1864 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS1926 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS1926 > 1) {
    int32_t _M0L11_2anew__cntS1927 = _M0L6_2acntS1926 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS1927;
    moonbit_incref(_M0L8_2afieldS1864);
  } else if (_M0L6_2acntS1926 == 1) {
    #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__5210S8 = _M0L8_2afieldS1864;
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_46.data);
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__5209S9, _M0L15_2a_2aarg__5210S8);
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_47.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS787) {
  switch (Moonbit_object_tag(_M0L4_2aeS787)) {
    case 4: {
      moonbit_decref(_M0L4_2aeS787);
      return (moonbit_string_t)moonbit_string_literal_48.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS787);
      return (moonbit_string_t)moonbit_string_literal_49.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS787);
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS787);
      return (moonbit_string_t)moonbit_string_literal_50.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS787);
      return (moonbit_string_t)moonbit_string_literal_51.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC14bool4BoolPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS811
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS812 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS811;
  int32_t _M0L7_2aselfS810 = _M0L14_2aboxed__selfS812->$0;
  moonbit_decref(_M0L14_2aboxed__selfS812);
  return _M0IPC14bool4BoolPB4Show10to__string(_M0L7_2aselfS810);
}

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS808,
  struct _M0TPB6Logger _M0L8_2aparamS807
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS809 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS808;
  int32_t _M0L7_2aselfS806 = _M0L14_2aboxed__selfS809->$0;
  moonbit_decref(_M0L14_2aboxed__selfS809);
  _M0IPC14bool4BoolPB4Show6output(_M0L7_2aselfS806, _M0L8_2aparamS807);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS805,
  int32_t _M0L8_2aparamS804
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS803 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS805;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS803, _M0L8_2aparamS804);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS802,
  struct _M0TPC16string10StringView _M0L8_2aparamS801
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS800 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS802;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS800, _M0L8_2aparamS801);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS799,
  moonbit_string_t _M0L8_2aparamS796,
  int32_t _M0L8_2aparamS797,
  int32_t _M0L8_2aparamS798
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS795 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS799;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS795, _M0L8_2aparamS796, _M0L8_2aparamS797, _M0L8_2aparamS798);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS794,
  moonbit_string_t _M0L8_2aparamS793
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS792 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS794;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS792, _M0L8_2aparamS793);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS826 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS825;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS824;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS714;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS823;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS822;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS821;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS820;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS713;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS819;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS818;
  _M0L6_2atmpS826[0] = (moonbit_string_t)moonbit_string_literal_52.data;
  moonbit_incref(_M0FP36mulpjs4mulp8platform53____test__7369676e616c5f7762746573742e6d6274__0_2eclo);
  _M0L8_2atupleS825
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS825)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS825->$0
  = _M0FP36mulpjs4mulp8platform53____test__7369676e616c5f7762746573742e6d6274__0_2eclo;
  _M0L8_2atupleS825->$1 = _M0L6_2atmpS826;
  _M0L8_2atupleS824
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS824)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS824->$0 = 0;
  _M0L8_2atupleS824->$1 = _M0L8_2atupleS825;
  _M0L7_2abindS714
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS714[0] = _M0L8_2atupleS824;
  _M0L6_2atmpS823 = _M0L7_2abindS714;
  _M0L6_2atmpS822
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS823
  };
  #line 398 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS821
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS822);
  _M0L8_2atupleS820
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS820)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS820->$0 = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L8_2atupleS820->$1 = _M0L6_2atmpS821;
  _M0L7_2abindS713
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS713[0] = _M0L8_2atupleS820;
  _M0L6_2atmpS819 = _M0L7_2abindS713;
  _M0L6_2atmpS818
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS819
  };
  #line 397 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0FP36mulpjs4mulp8platform48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS818);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS817;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS781;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS782;
  int32_t _M0L7_2abindS783;
  int32_t _M0L2__S784;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS817
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS781
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS781)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS781->$0 = _M0L6_2atmpS817;
  _M0L12async__testsS781->$1 = 0;
  #line 438 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS782
  = _M0FP36mulpjs4mulp8platform52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS783 = _M0L7_2abindS782->$1;
  _M0L2__S784 = 0;
  while (1) {
    if (_M0L2__S784 < _M0L7_2abindS783) {
      struct _M0TUsiE** _M0L3bufS816 = _M0L7_2abindS782->$0;
      struct _M0TUsiE* _M0L3argS785 =
        (struct _M0TUsiE*)_M0L3bufS816[_M0L2__S784];
      moonbit_string_t _M0L6_2atmpS813 = _M0L3argS785->$0;
      int32_t _M0L6_2atmpS814 = _M0L3argS785->$1;
      int32_t _M0L6_2atmpS815;
      moonbit_incref(_M0L6_2atmpS813);
      moonbit_incref(_M0L12async__testsS781);
      #line 439 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
      _M0FP36mulpjs4mulp8platform44moonbit__test__driver__internal__do__execute(_M0L12async__testsS781, _M0L6_2atmpS813, _M0L6_2atmpS814);
      _M0L6_2atmpS815 = _M0L2__S784 + 1;
      _M0L2__S784 = _M0L6_2atmpS815;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS782);
    }
    break;
  }
  #line 441 "/Users/user/workspace/github/gulp/mulp/platform/__generated_driver_for_whitebox_test.mbt"
  _M0IP016_24default__implP36mulpjs4mulp8platform28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp8platform34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS781);
  return 0;
}