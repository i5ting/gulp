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
struct _M0DTPC15error5Error118mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0TURPB6LoggerRPC16string10StringViewE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0KTPB4ShowTPB5ArrayGsE;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB8MutLocalGiE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32plugin__examples__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32plugin__examples__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0BTPB4Show;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS6String;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0DTPC15error5Error118mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
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

struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0TPB13StringBuilder {
  int32_t $1;
  uint16_t* $0;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPB8MutLocalGiE* $1;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowTPB5ArrayGsE {
  struct _M0BTPB4Show* $0;
  void* $1;
  
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

struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand {
  moonbit_string_t $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32plugin__examples__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp32plugin__examples__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
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

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS779(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS770(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testC1751l428(
  struct _M0TWEu*
);

int32_t _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testC1747l429(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS703(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS698(
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S685(
  int32_t,
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp32plugin__examples__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test59____test__6275696c645f77726170706572735f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test59____test__6275696c645f77726170706572735f746573742e6d6274__0(
  
);

struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0FP36mulpjs4mulp16plugin__examples26typescript__build__wrapper(
  moonbit_string_t
);

struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0FP36mulpjs4mulp16plugin__examples23moonbit__build__wrapper(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand4args(
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand*
);

moonbit_string_t _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand7program(
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand*
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

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(struct _M0TPB5ArrayGsE*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1377l680(struct _M0TWEOs*);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

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

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 109, 117, 
    108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 112, 108, 117, 103, 
    105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 115, 34, 44, 32, 
    34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    112, 108, 117, 103, 105, 110, 32, 101, 120, 97, 109, 112, 108, 101, 
    115, 32, 98, 117, 105, 108, 100, 32, 109, 111, 111, 110, 98, 105, 
    116, 32, 98, 117, 105, 108, 100, 32, 119, 114, 97, 112, 112, 101, 
    114, 32, 99, 111, 109, 109, 97, 110, 100, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    116, 115, 99, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_38 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    110, 97, 116, 105, 118, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    123, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[38]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 37), 
    91, 34, 116, 115, 99, 34, 44, 32, 34, 45, 45, 112, 114, 111, 106, 
    101, 99, 116, 34, 44, 32, 34, 116, 115, 99, 111, 110, 102, 105, 103, 
    46, 106, 115, 111, 110, 34, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 49, 58, 
    51, 45, 49, 49, 58, 52, 52, 64, 109, 117, 108, 112, 106, 115, 47, 
    109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_61 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_32 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    45, 45, 116, 97, 114, 103, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    98, 117, 105, 108, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 51, 
    45, 52, 58, 52, 53, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 
    108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    110, 112, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    112, 108, 117, 103, 105, 110, 32, 101, 120, 97, 109, 112, 108, 101, 
    115, 32, 98, 117, 105, 108, 100, 32, 116, 121, 112, 101, 115, 99, 
    114, 105, 112, 116, 32, 119, 114, 97, 112, 112, 101, 114, 32, 99, 
    111, 109, 109, 97, 110, 100, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_50 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 58, 51, 
    53, 45, 53, 58, 57, 49, 64, 109, 117, 108, 112, 106, 115, 47, 109, 
    117, 108, 112, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[110]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 109), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 112, 108, 
    117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 115, 95, 
    98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 
    84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[63]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 62), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 
    51, 45, 49, 50, 58, 56, 49, 64, 109, 117, 108, 112, 106, 115, 47, 
    109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_42 =
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
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 114, 115, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    109, 111, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 114, 115, 
    95, 116, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 58, 51, 
    45, 53, 58, 57, 50, 64, 109, 117, 108, 112, 106, 115, 47, 109, 117, 
    108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 
    49, 49, 45, 49, 50, 58, 50, 53, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 58, 49, 
    49, 45, 53, 58, 50, 53, 64, 109, 117, 108, 112, 106, 115, 47, 109, 
    117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 51, 
    56, 45, 52, 58, 52, 52, 64, 109, 117, 108, 112, 106, 115, 47, 109, 
    117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 49, 58, 
    49, 49, 45, 49, 49, 58, 50, 56, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 49, 58, 
    51, 56, 45, 49, 49, 58, 52, 51, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    116, 115, 99, 111, 110, 102, 105, 103, 46, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 
    51, 53, 45, 49, 50, 58, 56, 48, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[108]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 107), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 112, 108, 
    117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 115, 95, 
    98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 
    114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_33 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    45, 45, 112, 114, 111, 106, 101, 99, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    112, 108, 117, 103, 105, 110, 95, 101, 120, 97, 109, 112, 108, 101, 
    115, 47, 98, 117, 105, 108, 100, 95, 119, 114, 97, 112, 112, 101, 
    114, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 49, 
    49, 45, 52, 58, 50, 56, 64, 109, 117, 108, 112, 106, 115, 47, 109, 
    117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_40 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[47]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 46), 
    91, 34, 98, 117, 105, 108, 100, 34, 44, 32, 34, 45, 45, 116, 97, 
    114, 103, 101, 116, 34, 44, 32, 34, 110, 97, 116, 105, 118, 101, 
    34, 44, 32, 34, 109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 
    112, 34, 93, 0
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
} const _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS779$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS779
  };

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test65____test__6275696c645f77726170706572735f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test65____test__6275696c645f77726170706572735f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__1_2edyncall$closure.data;

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

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1783
) {
  return _M0FP36mulpjs4mulp32plugin__examples__blackbox__test59____test__6275696c645f77726170706572735f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test69____test__6275696c645f77726170706572735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1782
) {
  return _M0FP36mulpjs4mulp32plugin__examples__blackbox__test59____test__6275696c645f77726170706572735f746573742e6d6274__0();
}

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS800,
  moonbit_string_t _M0L8filenameS775,
  int32_t _M0L5indexS778
) {
  struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770* _closure_1979;
  struct _M0TWssbEu* _M0L14handle__resultS770;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS779;
  void* _M0L11_2atry__errS794;
  struct moonbit_result_0 _tmp_1981;
  int32_t _handle__error__result_1982;
  int32_t _M0L6_2atmpS1770;
  void* _M0L3errS795;
  moonbit_string_t _M0L4nameS797;
  struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS798;
  moonbit_string_t _M0L8_2afieldS1784;
  int32_t _M0L6_2acntS1915;
  moonbit_string_t _M0L7_2anameS799;
  #line 527 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS775);
  _closure_1979
  = (struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770*)moonbit_malloc(sizeof(struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770));
  Moonbit_object_header(_closure_1979)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770, $1) >> 2, 1, 0);
  _closure_1979->code
  = &_M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS770;
  _closure_1979->$0 = _M0L5indexS778;
  _closure_1979->$1 = _M0L8filenameS775;
  _M0L14handle__resultS770 = (struct _M0TWssbEu*)_closure_1979;
  _M0L17error__to__stringS779
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS779$closure.data;
  moonbit_incref(_M0L12async__testsS800);
  moonbit_incref(_M0L17error__to__stringS779);
  moonbit_incref(_M0L8filenameS775);
  moonbit_incref(_M0L14handle__resultS770);
  #line 561 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _tmp_1981
  = _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS800, _M0L8filenameS775, _M0L5indexS778, _M0L14handle__resultS770, _M0L17error__to__stringS779);
  if (_tmp_1981.tag) {
    int32_t const _M0L5_2aokS1779 = _tmp_1981.data.ok;
    _handle__error__result_1982 = _M0L5_2aokS1779;
  } else {
    void* const _M0L6_2aerrS1780 = _tmp_1981.data.err;
    moonbit_decref(_M0L12async__testsS800);
    moonbit_decref(_M0L17error__to__stringS779);
    moonbit_decref(_M0L8filenameS775);
    _M0L11_2atry__errS794 = _M0L6_2aerrS1780;
    goto join_793;
  }
  if (_handle__error__result_1982) {
    moonbit_decref(_M0L12async__testsS800);
    moonbit_decref(_M0L17error__to__stringS779);
    moonbit_decref(_M0L8filenameS775);
    _M0L6_2atmpS1770 = 1;
  } else {
    struct moonbit_result_0 _tmp_1983;
    int32_t _handle__error__result_1984;
    moonbit_incref(_M0L12async__testsS800);
    moonbit_incref(_M0L17error__to__stringS779);
    moonbit_incref(_M0L8filenameS775);
    moonbit_incref(_M0L14handle__resultS770);
    #line 564 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    _tmp_1983
    = _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS800, _M0L8filenameS775, _M0L5indexS778, _M0L14handle__resultS770, _M0L17error__to__stringS779);
    if (_tmp_1983.tag) {
      int32_t const _M0L5_2aokS1777 = _tmp_1983.data.ok;
      _handle__error__result_1984 = _M0L5_2aokS1777;
    } else {
      void* const _M0L6_2aerrS1778 = _tmp_1983.data.err;
      moonbit_decref(_M0L12async__testsS800);
      moonbit_decref(_M0L17error__to__stringS779);
      moonbit_decref(_M0L8filenameS775);
      _M0L11_2atry__errS794 = _M0L6_2aerrS1778;
      goto join_793;
    }
    if (_handle__error__result_1984) {
      moonbit_decref(_M0L12async__testsS800);
      moonbit_decref(_M0L17error__to__stringS779);
      moonbit_decref(_M0L8filenameS775);
      _M0L6_2atmpS1770 = 1;
    } else {
      struct moonbit_result_0 _tmp_1985;
      int32_t _handle__error__result_1986;
      moonbit_incref(_M0L12async__testsS800);
      moonbit_incref(_M0L17error__to__stringS779);
      moonbit_incref(_M0L8filenameS775);
      moonbit_incref(_M0L14handle__resultS770);
      #line 567 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _tmp_1985
      = _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS800, _M0L8filenameS775, _M0L5indexS778, _M0L14handle__resultS770, _M0L17error__to__stringS779);
      if (_tmp_1985.tag) {
        int32_t const _M0L5_2aokS1775 = _tmp_1985.data.ok;
        _handle__error__result_1986 = _M0L5_2aokS1775;
      } else {
        void* const _M0L6_2aerrS1776 = _tmp_1985.data.err;
        moonbit_decref(_M0L12async__testsS800);
        moonbit_decref(_M0L17error__to__stringS779);
        moonbit_decref(_M0L8filenameS775);
        _M0L11_2atry__errS794 = _M0L6_2aerrS1776;
        goto join_793;
      }
      if (_handle__error__result_1986) {
        moonbit_decref(_M0L12async__testsS800);
        moonbit_decref(_M0L17error__to__stringS779);
        moonbit_decref(_M0L8filenameS775);
        _M0L6_2atmpS1770 = 1;
      } else {
        struct moonbit_result_0 _tmp_1987;
        int32_t _handle__error__result_1988;
        moonbit_incref(_M0L12async__testsS800);
        moonbit_incref(_M0L17error__to__stringS779);
        moonbit_incref(_M0L8filenameS775);
        moonbit_incref(_M0L14handle__resultS770);
        #line 570 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        _tmp_1987
        = _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS800, _M0L8filenameS775, _M0L5indexS778, _M0L14handle__resultS770, _M0L17error__to__stringS779);
        if (_tmp_1987.tag) {
          int32_t const _M0L5_2aokS1773 = _tmp_1987.data.ok;
          _handle__error__result_1988 = _M0L5_2aokS1773;
        } else {
          void* const _M0L6_2aerrS1774 = _tmp_1987.data.err;
          moonbit_decref(_M0L12async__testsS800);
          moonbit_decref(_M0L17error__to__stringS779);
          moonbit_decref(_M0L8filenameS775);
          _M0L11_2atry__errS794 = _M0L6_2aerrS1774;
          goto join_793;
        }
        if (_handle__error__result_1988) {
          moonbit_decref(_M0L12async__testsS800);
          moonbit_decref(_M0L17error__to__stringS779);
          moonbit_decref(_M0L8filenameS775);
          _M0L6_2atmpS1770 = 1;
        } else {
          struct moonbit_result_0 _tmp_1989;
          moonbit_incref(_M0L14handle__resultS770);
          #line 573 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
          _tmp_1989
          = _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS800, _M0L8filenameS775, _M0L5indexS778, _M0L14handle__resultS770, _M0L17error__to__stringS779);
          if (_tmp_1989.tag) {
            int32_t const _M0L5_2aokS1771 = _tmp_1989.data.ok;
            _M0L6_2atmpS1770 = _M0L5_2aokS1771;
          } else {
            void* const _M0L6_2aerrS1772 = _tmp_1989.data.err;
            _M0L11_2atry__errS794 = _M0L6_2aerrS1772;
            goto join_793;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1770) {
    void* _M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1781 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1781)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1781)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS794
    = _M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1781;
    goto join_793;
  } else {
    moonbit_decref(_M0L14handle__resultS770);
  }
  goto joinlet_1980;
  join_793:;
  _M0L3errS795 = _M0L11_2atry__errS794;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS798
  = (struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS795;
  _M0L8_2afieldS1784 = _M0L36_2aMoonBitTestDriverInternalSkipTestS798->$0;
  _M0L6_2acntS1915
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS798)->rc;
  if (_M0L6_2acntS1915 > 1) {
    int32_t _M0L11_2anew__cntS1916 = _M0L6_2acntS1915 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS798)->rc
    = _M0L11_2anew__cntS1916;
    moonbit_incref(_M0L8_2afieldS1784);
  } else if (_M0L6_2acntS1915 == 1) {
    #line 580 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS798);
  }
  _M0L7_2anameS799 = _M0L8_2afieldS1784;
  _M0L4nameS797 = _M0L7_2anameS799;
  goto join_796;
  goto joinlet_1990;
  join_796:;
  #line 581 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS770(_M0L14handle__resultS770, _M0L4nameS797, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_1990:;
  joinlet_1980:;
  return 0;
}

moonbit_string_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS779(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1769,
  void* _M0L3errS780
) {
  void* _M0L1eS782;
  moonbit_string_t _M0L1eS784;
  #line 550 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS1769);
  switch (Moonbit_object_tag(_M0L3errS780)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS785 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS780;
      moonbit_string_t _M0L8_2afieldS1785 = _M0L10_2aFailureS785->$0;
      int32_t _M0L6_2acntS1917 =
        Moonbit_object_header(_M0L10_2aFailureS785)->rc;
      moonbit_string_t _M0L4_2aeS786;
      if (_M0L6_2acntS1917 > 1) {
        int32_t _M0L11_2anew__cntS1918 = _M0L6_2acntS1917 - 1;
        Moonbit_object_header(_M0L10_2aFailureS785)->rc
        = _M0L11_2anew__cntS1918;
        moonbit_incref(_M0L8_2afieldS1785);
      } else if (_M0L6_2acntS1917 == 1) {
        #line 551 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS785);
      }
      _M0L4_2aeS786 = _M0L8_2afieldS1785;
      _M0L1eS784 = _M0L4_2aeS786;
      goto join_783;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS787 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS780;
      moonbit_string_t _M0L8_2afieldS1786 = _M0L15_2aInspectErrorS787->$0;
      int32_t _M0L6_2acntS1919 =
        Moonbit_object_header(_M0L15_2aInspectErrorS787)->rc;
      moonbit_string_t _M0L4_2aeS788;
      if (_M0L6_2acntS1919 > 1) {
        int32_t _M0L11_2anew__cntS1920 = _M0L6_2acntS1919 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS787)->rc
        = _M0L11_2anew__cntS1920;
        moonbit_incref(_M0L8_2afieldS1786);
      } else if (_M0L6_2acntS1919 == 1) {
        #line 551 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS787);
      }
      _M0L4_2aeS788 = _M0L8_2afieldS1786;
      _M0L1eS784 = _M0L4_2aeS788;
      goto join_783;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS789 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS780;
      moonbit_string_t _M0L8_2afieldS1787 = _M0L16_2aSnapshotErrorS789->$0;
      int32_t _M0L6_2acntS1921 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS789)->rc;
      moonbit_string_t _M0L4_2aeS790;
      if (_M0L6_2acntS1921 > 1) {
        int32_t _M0L11_2anew__cntS1922 = _M0L6_2acntS1921 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS789)->rc
        = _M0L11_2anew__cntS1922;
        moonbit_incref(_M0L8_2afieldS1787);
      } else if (_M0L6_2acntS1921 == 1) {
        #line 551 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS789);
      }
      _M0L4_2aeS790 = _M0L8_2afieldS1787;
      _M0L1eS784 = _M0L4_2aeS790;
      goto join_783;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error118mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS791 =
        (struct _M0DTPC15error5Error118mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS780;
      moonbit_string_t _M0L8_2afieldS1788 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS791->$0;
      int32_t _M0L6_2acntS1923 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS791)->rc;
      moonbit_string_t _M0L4_2aeS792;
      if (_M0L6_2acntS1923 > 1) {
        int32_t _M0L11_2anew__cntS1924 = _M0L6_2acntS1923 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS791)->rc
        = _M0L11_2anew__cntS1924;
        moonbit_incref(_M0L8_2afieldS1788);
      } else if (_M0L6_2acntS1923 == 1) {
        #line 551 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS791);
      }
      _M0L4_2aeS792 = _M0L8_2afieldS1788;
      _M0L1eS784 = _M0L4_2aeS792;
      goto join_783;
      break;
    }
    default: {
      _M0L1eS782 = _M0L3errS780;
      goto join_781;
      break;
    }
  }
  join_783:;
  return _M0L1eS784;
  join_781:;
  #line 556 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS782);
}

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS770(
  struct _M0TWssbEu* _M0L6_2aenvS1755,
  moonbit_string_t _M0L8testnameS771,
  moonbit_string_t _M0L7messageS772,
  int32_t _M0L7skippedS773
) {
  struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770* _M0L14_2acasted__envS1756;
  moonbit_string_t _M0L8filenameS775;
  int32_t _M0L5indexS778;
  int32_t _M0L6_2acntS1925;
  int32_t _if__result_1993;
  moonbit_string_t _M0L10file__nameS774;
  moonbit_string_t _M0L10test__nameS776;
  moonbit_string_t _M0L7messageS777;
  moonbit_string_t _M0L6_2atmpS1768;
  moonbit_string_t _M0L6_2atmpS1767;
  moonbit_string_t _M0L6_2atmpS1765;
  moonbit_string_t _M0L6_2atmpS1766;
  moonbit_string_t _M0L6_2atmpS1764;
  moonbit_string_t _M0L6_2atmpS1762;
  moonbit_string_t _M0L6_2atmpS1763;
  moonbit_string_t _M0L6_2atmpS1761;
  moonbit_string_t _M0L6_2atmpS1759;
  moonbit_string_t _M0L6_2atmpS1760;
  moonbit_string_t _M0L6_2atmpS1758;
  moonbit_string_t _M0L6_2atmpS1757;
  #line 534 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS1756
  = (struct _M0R121_24mulpjs_2fmulp_2fplugin__examples__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c770*)_M0L6_2aenvS1755;
  _M0L8filenameS775 = _M0L14_2acasted__envS1756->$1;
  _M0L5indexS778 = _M0L14_2acasted__envS1756->$0;
  _M0L6_2acntS1925 = Moonbit_object_header(_M0L14_2acasted__envS1756)->rc;
  if (_M0L6_2acntS1925 > 1) {
    int32_t _M0L11_2anew__cntS1926 = _M0L6_2acntS1925 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1756)->rc
    = _M0L11_2anew__cntS1926;
    moonbit_incref(_M0L8filenameS775);
  } else if (_M0L6_2acntS1925 == 1) {
    #line 534 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1756);
  }
  if (!_M0L7skippedS773) {
    _if__result_1993 = 1;
  } else {
    _if__result_1993 = 0;
  }
  if (_if__result_1993) {
    
  }
  #line 540 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS774
  = _M0MPC16string6String14escape_2einner(_M0L8filenameS775, 1);
  #line 541 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS776
  = _M0MPC16string6String14escape_2einner(_M0L8testnameS771, 1);
  #line 542 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS777
  = _M0MPC16string6String14escape_2einner(_M0L7messageS772, 1);
  #line 543 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 545 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1768
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS774);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1767
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1768);
  moonbit_decref(_M0L6_2atmpS1768);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1765
  = moonbit_add_string(_M0L6_2atmpS1767, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1767);
  #line 545 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1766 = _M0IPC13int3IntPB4Show10to__string(_M0L5indexS778);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1764 = moonbit_add_string(_M0L6_2atmpS1765, _M0L6_2atmpS1766);
  moonbit_decref(_M0L6_2atmpS1766);
  moonbit_decref(_M0L6_2atmpS1765);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1762
  = moonbit_add_string(_M0L6_2atmpS1764, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1764);
  #line 545 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1763
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS776);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1761 = moonbit_add_string(_M0L6_2atmpS1762, _M0L6_2atmpS1763);
  moonbit_decref(_M0L6_2atmpS1763);
  moonbit_decref(_M0L6_2atmpS1762);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1759
  = moonbit_add_string(_M0L6_2atmpS1761, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1761);
  #line 545 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1760
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS777);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1758 = moonbit_add_string(_M0L6_2atmpS1759, _M0L6_2atmpS1760);
  moonbit_decref(_M0L6_2atmpS1760);
  moonbit_decref(_M0L6_2atmpS1759);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1757
  = moonbit_add_string(_M0L6_2atmpS1758, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1758);
  #line 544 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1757);
  #line 547 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S769,
  moonbit_string_t _M0L8filenameS766,
  int32_t _M0L5indexS760,
  struct _M0TWssbEu* _M0L14handle__resultS756,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS758
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS736;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS765;
  struct _M0TWEuQRPC15error5Error* _M0L1fS738;
  moonbit_string_t* _M0L5attrsS739;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS759;
  moonbit_string_t _M0L4nameS742;
  moonbit_string_t _M0L4nameS740;
  int32_t _M0L6_2atmpS1754;
  struct _M0TWEOs* _M0L5_2aitS744;
  struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__* _closure_2002;
  struct _M0TWEu* _M0L6_2atmpS1745;
  struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__* _closure_2003;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS1746;
  struct moonbit_result_0 _result_2004;
  #line 408 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S769);
  moonbit_incref(_M0FP36mulpjs4mulp32plugin__examples__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 415 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS765
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP36mulpjs4mulp32plugin__examples__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS766);
  if (_M0L7_2abindS765 == 0) {
    struct moonbit_result_0 _result_1995;
    if (_M0L7_2abindS765) {
      moonbit_decref(_M0L7_2abindS765);
    }
    moonbit_decref(_M0L17error__to__stringS758);
    moonbit_decref(_M0L14handle__resultS756);
    _result_1995.tag = 1;
    _result_1995.data.ok = 0;
    return _result_1995;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS767 =
      _M0L7_2abindS765;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS768 =
      _M0L7_2aSomeS767;
    _M0L10index__mapS736 = _M0L13_2aindex__mapS768;
    goto join_735;
  }
  join_735:;
  #line 417 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS759
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS736, _M0L5indexS760);
  if (_M0L7_2abindS759 == 0) {
    struct moonbit_result_0 _result_1997;
    if (_M0L7_2abindS759) {
      moonbit_decref(_M0L7_2abindS759);
    }
    moonbit_decref(_M0L17error__to__stringS758);
    moonbit_decref(_M0L14handle__resultS756);
    _result_1997.tag = 1;
    _result_1997.data.ok = 0;
    return _result_1997;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS761 = _M0L7_2abindS759;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS762 = _M0L7_2aSomeS761;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS763 = _M0L4_2axS762->$0;
    moonbit_string_t* _M0L8_2afieldS1791 = _M0L4_2axS762->$1;
    int32_t _M0L6_2acntS1927 = Moonbit_object_header(_M0L4_2axS762)->rc;
    moonbit_string_t* _M0L8_2aattrsS764;
    if (_M0L6_2acntS1927 > 1) {
      int32_t _M0L11_2anew__cntS1928 = _M0L6_2acntS1927 - 1;
      Moonbit_object_header(_M0L4_2axS762)->rc = _M0L11_2anew__cntS1928;
      moonbit_incref(_M0L8_2afieldS1791);
      moonbit_incref(_M0L4_2afS763);
    } else if (_M0L6_2acntS1927 == 1) {
      #line 415 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS762);
    }
    _M0L8_2aattrsS764 = _M0L8_2afieldS1791;
    _M0L1fS738 = _M0L4_2afS763;
    _M0L5attrsS739 = _M0L8_2aattrsS764;
    goto join_737;
  }
  join_737:;
  _M0L6_2atmpS1754 = Moonbit_array_length(_M0L5attrsS739);
  if (_M0L6_2atmpS1754 >= 1) {
    moonbit_string_t _M0L7_2anameS743 = (moonbit_string_t)_M0L5attrsS739[0];
    moonbit_incref(_M0L7_2anameS743);
    _M0L4nameS742 = _M0L7_2anameS743;
    goto join_741;
  } else {
    _M0L4nameS740 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_1998;
  join_741:;
  _M0L4nameS740 = _M0L4nameS742;
  joinlet_1998:;
  #line 418 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS744 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS739);
  while (1) {
    moonbit_string_t _M0L4attrS746;
    moonbit_string_t _M0L7_2abindS753;
    int32_t _M0L6_2atmpS1738;
    int64_t _M0L6_2atmpS1737;
    moonbit_incref(_M0L5_2aitS744);
    #line 420 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS753 = _M0MPB4Iter4nextGsE(_M0L5_2aitS744);
    if (_M0L7_2abindS753 == 0) {
      if (_M0L7_2abindS753) {
        moonbit_decref(_M0L7_2abindS753);
      }
      moonbit_decref(_M0L5_2aitS744);
    } else {
      moonbit_string_t _M0L7_2aSomeS754 = _M0L7_2abindS753;
      moonbit_string_t _M0L7_2aattrS755 = _M0L7_2aSomeS754;
      _M0L4attrS746 = _M0L7_2aattrS755;
      goto join_745;
    }
    goto joinlet_2000;
    join_745:;
    _M0L6_2atmpS1738 = Moonbit_array_length(_M0L4attrS746);
    _M0L6_2atmpS1737 = (int64_t)_M0L6_2atmpS1738;
    moonbit_incref(_M0L4attrS746);
    #line 421 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS746, 5, 0, _M0L6_2atmpS1737)
    ) {
      int32_t _M0L6_2atmpS1744 = _M0L4attrS746[0];
      int32_t _M0L4_2axS747 = _M0L6_2atmpS1744;
      if (_M0L4_2axS747 == 112) {
        int32_t _M0L6_2atmpS1743 = _M0L4attrS746[1];
        int32_t _M0L4_2axS748 = _M0L6_2atmpS1743;
        if (_M0L4_2axS748 == 97) {
          int32_t _M0L6_2atmpS1742 = _M0L4attrS746[2];
          int32_t _M0L4_2axS749 = _M0L6_2atmpS1742;
          if (_M0L4_2axS749 == 110) {
            int32_t _M0L6_2atmpS1741 = _M0L4attrS746[3];
            int32_t _M0L4_2axS750 = _M0L6_2atmpS1741;
            if (_M0L4_2axS750 == 105) {
              int32_t _M0L6_2atmpS1740 = _M0L4attrS746[4];
              int32_t _M0L4_2axS751;
              moonbit_decref(_M0L4attrS746);
              _M0L4_2axS751 = _M0L6_2atmpS1740;
              if (_M0L4_2axS751 == 99) {
                void* _M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1739;
                struct moonbit_result_0 _result_2001;
                moonbit_decref(_M0L17error__to__stringS758);
                moonbit_decref(_M0L14handle__resultS756);
                moonbit_decref(_M0L5_2aitS744);
                moonbit_decref(_M0L1fS738);
                _M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1739
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1739)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1739)->$0
                = _M0L4nameS740;
                _result_2001.tag = 0;
                _result_2001.data.err
                = _M0L120mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1739;
                return _result_2001;
              }
            } else {
              moonbit_decref(_M0L4attrS746);
            }
          } else {
            moonbit_decref(_M0L4attrS746);
          }
        } else {
          moonbit_decref(_M0L4attrS746);
        }
      } else {
        moonbit_decref(_M0L4attrS746);
      }
    } else {
      moonbit_decref(_M0L4attrS746);
    }
    continue;
    joinlet_2000:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS756);
  moonbit_incref(_M0L4nameS740);
  _closure_2002
  = (struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__*)moonbit_malloc(sizeof(struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__));
  Moonbit_object_header(_closure_2002)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__, $0) >> 2, 2, 0);
  _closure_2002->code
  = &_M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testC1751l428;
  _closure_2002->$0 = _M0L14handle__resultS756;
  _closure_2002->$1 = _M0L4nameS740;
  _M0L6_2atmpS1745 = (struct _M0TWEu*)_closure_2002;
  _closure_2003
  = (struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__*)moonbit_malloc(sizeof(struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__));
  Moonbit_object_header(_closure_2003)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__, $0) >> 2, 3, 0);
  _closure_2003->code
  = &_M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testC1747l429;
  _closure_2003->$0 = _M0L17error__to__stringS758;
  _closure_2003->$1 = _M0L14handle__resultS756;
  _closure_2003->$2 = _M0L4nameS740;
  _M0L6_2atmpS1746 = (struct _M0TWRPC15error5ErrorEu*)_closure_2003;
  #line 426 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp32plugin__examples__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS738, _M0L6_2atmpS1745, _M0L6_2atmpS1746);
  _result_2004.tag = 1;
  _result_2004.data.ok = 1;
  return _result_2004;
}

int32_t _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testC1751l428(
  struct _M0TWEu* _M0L6_2aenvS1752
) {
  struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__* _M0L14_2acasted__envS1753;
  moonbit_string_t _M0L4nameS740;
  struct _M0TWssbEu* _M0L8_2afieldS1793;
  int32_t _M0L6_2acntS1929;
  struct _M0TWssbEu* _M0L14handle__resultS756;
  #line 428 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS1753
  = (struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1751__l428__*)_M0L6_2aenvS1752;
  _M0L4nameS740 = _M0L14_2acasted__envS1753->$1;
  _M0L8_2afieldS1793 = _M0L14_2acasted__envS1753->$0;
  _M0L6_2acntS1929 = Moonbit_object_header(_M0L14_2acasted__envS1753)->rc;
  if (_M0L6_2acntS1929 > 1) {
    int32_t _M0L11_2anew__cntS1930 = _M0L6_2acntS1929 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1753)->rc
    = _M0L11_2anew__cntS1930;
    moonbit_incref(_M0L4nameS740);
    moonbit_incref(_M0L8_2afieldS1793);
  } else if (_M0L6_2acntS1929 == 1) {
    #line 428 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1753);
  }
  _M0L14handle__resultS756 = _M0L8_2afieldS1793;
  #line 428 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS756->code(_M0L14handle__resultS756, _M0L4nameS740, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP36mulpjs4mulp32plugin__examples__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testC1747l429(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS1748,
  void* _M0L3errS757
) {
  struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__* _M0L14_2acasted__envS1749;
  moonbit_string_t _M0L4nameS740;
  struct _M0TWssbEu* _M0L14handle__resultS756;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS1795;
  int32_t _M0L6_2acntS1931;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS758;
  moonbit_string_t _M0L6_2atmpS1750;
  #line 429 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS1749
  = (struct _M0R213_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2fplugin__examples__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1747__l429__*)_M0L6_2aenvS1748;
  _M0L4nameS740 = _M0L14_2acasted__envS1749->$2;
  _M0L14handle__resultS756 = _M0L14_2acasted__envS1749->$1;
  _M0L8_2afieldS1795 = _M0L14_2acasted__envS1749->$0;
  _M0L6_2acntS1931 = Moonbit_object_header(_M0L14_2acasted__envS1749)->rc;
  if (_M0L6_2acntS1931 > 1) {
    int32_t _M0L11_2anew__cntS1932 = _M0L6_2acntS1931 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1749)->rc
    = _M0L11_2anew__cntS1932;
    moonbit_incref(_M0L4nameS740);
    moonbit_incref(_M0L14handle__resultS756);
    moonbit_incref(_M0L8_2afieldS1795);
  } else if (_M0L6_2acntS1931 == 1) {
    #line 429 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1749);
  }
  _M0L17error__to__stringS758 = _M0L8_2afieldS1795;
  #line 429 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1750
  = _M0L17error__to__stringS758->code(_M0L17error__to__stringS758, _M0L3errS757);
  #line 429 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS756->code(_M0L14handle__resultS756, _M0L4nameS740, _M0L6_2atmpS1750, 0);
  return 0;
}

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS730,
  struct _M0TWEu* _M0L6on__okS731,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS728
) {
  void* _M0L11_2atry__errS726;
  struct moonbit_result_0 _tmp_2006;
  void* _M0L3errS727;
  #line 375 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  #line 382 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _tmp_2006 = _M0L1fS730->code(_M0L1fS730);
  if (_tmp_2006.tag) {
    int32_t const _M0L5_2aokS1735 = _tmp_2006.data.ok;
    moonbit_decref(_M0L7on__errS728);
  } else {
    void* const _M0L6_2aerrS1736 = _tmp_2006.data.err;
    moonbit_decref(_M0L6on__okS731);
    _M0L11_2atry__errS726 = _M0L6_2aerrS1736;
    goto join_725;
  }
  #line 382 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS731->code(_M0L6on__okS731);
  goto joinlet_2005;
  join_725:;
  _M0L3errS727 = _M0L11_2atry__errS726;
  #line 383 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS728->code(_M0L7on__errS728, _M0L3errS727);
  joinlet_2005:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S685;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS698;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS703;
  struct _M0TUsiE** _M0L6_2atmpS1734;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS710;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS711;
  moonbit_string_t _M0L6_2atmpS1733;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS712;
  int32_t _M0L7_2abindS713;
  int32_t _M0L2__S714;
  #line 193 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S685 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS698
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS703 = 0;
  _M0L6_2atmpS1734 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS710
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS710)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS710->$0 = _M0L6_2atmpS1734;
  _M0L16file__and__indexS710->$1 = 0;
  #line 282 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS711
  = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS698(_M0L57moonbit__test__driver__internal__get__cli__args__internalS698);
  #line 284 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1733 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS711, 1);
  #line 283 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS712
  = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS703(_M0L51moonbit__test__driver__internal__split__mbt__stringS703, _M0L6_2atmpS1733, 47);
  _M0L7_2abindS713 = _M0L10test__argsS712->$1;
  _M0L2__S714 = 0;
  while (1) {
    if (_M0L2__S714 < _M0L7_2abindS713) {
      moonbit_string_t* _M0L3bufS1732 = _M0L10test__argsS712->$0;
      moonbit_string_t _M0L3argS715 =
        (moonbit_string_t)_M0L3bufS1732[_M0L2__S714];
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS716;
      moonbit_string_t _M0L4fileS717;
      moonbit_string_t _M0L5rangeS718;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS719;
      moonbit_string_t _M0L6_2atmpS1730;
      int32_t _M0L5startS720;
      moonbit_string_t _M0L6_2atmpS1729;
      int32_t _M0L3endS721;
      int32_t _M0L1iS722;
      int32_t _M0L6_2atmpS1731;
      moonbit_incref(_M0L3argS715);
      #line 288 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS716
      = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS703(_M0L51moonbit__test__driver__internal__split__mbt__stringS703, _M0L3argS715, 58);
      moonbit_incref(_M0L16file__and__rangeS716);
      #line 289 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS717
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS716, 0);
      #line 290 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS718
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS716, 1);
      #line 291 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS719
      = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS703(_M0L51moonbit__test__driver__internal__split__mbt__stringS703, _M0L5rangeS718, 45);
      moonbit_incref(_M0L15start__and__endS719);
      #line 294 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1730
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS719, 0);
      #line 294 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L5startS720
      = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S685(_M0L45moonbit__test__driver__internal__parse__int__S685, _M0L6_2atmpS1730);
      #line 295 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1729
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS719, 1);
      #line 295 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L3endS721
      = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S685(_M0L45moonbit__test__driver__internal__parse__int__S685, _M0L6_2atmpS1729);
      _M0L1iS722 = _M0L5startS720;
      while (1) {
        if (_M0L1iS722 < _M0L3endS721) {
          struct _M0TUsiE* _M0L8_2atupleS1727;
          int32_t _M0L6_2atmpS1728;
          moonbit_incref(_M0L4fileS717);
          _M0L8_2atupleS1727
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1727)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1727->$0 = _M0L4fileS717;
          _M0L8_2atupleS1727->$1 = _M0L1iS722;
          moonbit_incref(_M0L16file__and__indexS710);
          #line 297 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS710, _M0L8_2atupleS1727);
          _M0L6_2atmpS1728 = _M0L1iS722 + 1;
          _M0L1iS722 = _M0L6_2atmpS1728;
          continue;
        } else {
          moonbit_decref(_M0L4fileS717);
        }
        break;
      }
      _M0L6_2atmpS1731 = _M0L2__S714 + 1;
      _M0L2__S714 = _M0L6_2atmpS1731;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS712);
    }
    break;
  }
  return _M0L16file__and__indexS710;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS703(
  int32_t _M0L6_2aenvS1708,
  moonbit_string_t _M0L1sS704,
  int32_t _M0L3sepS705
) {
  moonbit_string_t* _M0L6_2atmpS1726;
  struct _M0TPB5ArrayGsE* _M0L3resS706;
  struct _M0TPB8MutLocalGiE* _M0L1iS707;
  struct _M0TPB8MutLocalGiE* _M0L5startS708;
  int32_t _M0L3valS1721;
  int32_t _M0L6_2atmpS1722;
  #line 261 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1726 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS706
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS706)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS706->$0 = _M0L6_2atmpS1726;
  _M0L3resS706->$1 = 0;
  _M0L1iS707
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS707)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS707->$0 = 0;
  _M0L5startS708
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS708)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5startS708->$0 = 0;
  while (1) {
    int32_t _M0L3valS1709 = _M0L1iS707->$0;
    int32_t _M0L6_2atmpS1710 = Moonbit_array_length(_M0L1sS704);
    if (_M0L3valS1709 < _M0L6_2atmpS1710) {
      int32_t _M0L3valS1713 = _M0L1iS707->$0;
      int32_t _M0L6_2atmpS1712;
      int32_t _M0L6_2atmpS1711;
      int32_t _M0L3valS1720;
      int32_t _M0L6_2atmpS1719;
      if (
        _M0L3valS1713 < 0
        || _M0L3valS1713 >= Moonbit_array_length(_M0L1sS704)
      ) {
        #line 269 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1712 = _M0L1sS704[_M0L3valS1713];
      _M0L6_2atmpS1711 = _M0L6_2atmpS1712;
      if (_M0L6_2atmpS1711 == _M0L3sepS705) {
        int32_t _M0L3valS1715 = _M0L5startS708->$0;
        int32_t _M0L3valS1716 = _M0L1iS707->$0;
        moonbit_string_t _M0L6_2atmpS1714;
        int32_t _M0L3valS1718;
        int32_t _M0L6_2atmpS1717;
        moonbit_incref(_M0L1sS704);
        #line 270 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS1714
        = _M0MPC16string6String17unsafe__substring(_M0L1sS704, _M0L3valS1715, _M0L3valS1716);
        moonbit_incref(_M0L3resS706);
        #line 270 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS706, _M0L6_2atmpS1714);
        _M0L3valS1718 = _M0L1iS707->$0;
        _M0L6_2atmpS1717 = _M0L3valS1718 + 1;
        _M0L5startS708->$0 = _M0L6_2atmpS1717;
      }
      _M0L3valS1720 = _M0L1iS707->$0;
      _M0L6_2atmpS1719 = _M0L3valS1720 + 1;
      _M0L1iS707->$0 = _M0L6_2atmpS1719;
      continue;
    } else {
      moonbit_decref(_M0L1iS707);
    }
    break;
  }
  _M0L3valS1721 = _M0L5startS708->$0;
  _M0L6_2atmpS1722 = Moonbit_array_length(_M0L1sS704);
  if (_M0L3valS1721 < _M0L6_2atmpS1722) {
    int32_t _M0L3valS1724 = _M0L5startS708->$0;
    int32_t _M0L6_2atmpS1725;
    moonbit_string_t _M0L6_2atmpS1723;
    moonbit_decref(_M0L5startS708);
    _M0L6_2atmpS1725 = Moonbit_array_length(_M0L1sS704);
    #line 276 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS1723
    = _M0MPC16string6String17unsafe__substring(_M0L1sS704, _M0L3valS1724, _M0L6_2atmpS1725);
    moonbit_incref(_M0L3resS706);
    #line 276 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS706, _M0L6_2atmpS1723);
  } else {
    moonbit_decref(_M0L5startS708);
    moonbit_decref(_M0L1sS704);
  }
  return _M0L3resS706;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS698(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691
) {
  moonbit_bytes_t* _M0L3tmpS699;
  int32_t _M0L6_2atmpS1707;
  struct _M0TPB5ArrayGsE* _M0L3resS700;
  int32_t _M0L1iS701;
  #line 250 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  #line 253 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS699
  = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1707 = Moonbit_array_length(_M0L3tmpS699);
  #line 254 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS700 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1707);
  _M0L1iS701 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1703 = Moonbit_array_length(_M0L3tmpS699);
    if (_M0L1iS701 < _M0L6_2atmpS1703) {
      moonbit_bytes_t _M0L6_2atmpS1705;
      moonbit_string_t _M0L6_2atmpS1704;
      int32_t _M0L6_2atmpS1706;
      if (_M0L1iS701 < 0 || _M0L1iS701 >= Moonbit_array_length(_M0L3tmpS699)) {
        #line 256 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1705 = (moonbit_bytes_t)_M0L3tmpS699[_M0L1iS701];
      moonbit_incref(_M0L6_2atmpS1705);
      #line 256 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1704
      = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691, _M0L6_2atmpS1705);
      moonbit_incref(_M0L3resS700);
      #line 256 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS700, _M0L6_2atmpS1704);
      _M0L6_2atmpS1706 = _M0L1iS701 + 1;
      _M0L1iS701 = _M0L6_2atmpS1706;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS699);
    }
    break;
  }
  return _M0L3resS700;
}

moonbit_string_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS691(
  int32_t _M0L6_2aenvS1617,
  moonbit_bytes_t _M0L5bytesS692
) {
  struct _M0TPB13StringBuilder* _M0L3resS693;
  int32_t _M0L3lenS694;
  struct _M0TPB8MutLocalGiE* _M0L1iS695;
  #line 206 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  #line 209 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS693 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS694 = Moonbit_array_length(_M0L5bytesS692);
  _M0L1iS695
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS695)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS695->$0 = 0;
  while (1) {
    int32_t _M0L3valS1618 = _M0L1iS695->$0;
    if (_M0L3valS1618 < _M0L3lenS694) {
      int32_t _M0L3valS1702 = _M0L1iS695->$0;
      int32_t _M0L6_2atmpS1701;
      int32_t _M0L6_2atmpS1700;
      struct _M0TPB8MutLocalGiE* _M0L1cS696;
      int32_t _M0L3valS1619;
      if (
        _M0L3valS1702 < 0
        || _M0L3valS1702 >= Moonbit_array_length(_M0L5bytesS692)
      ) {
        #line 213 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1701 = _M0L5bytesS692[_M0L3valS1702];
      _M0L6_2atmpS1700 = (int32_t)_M0L6_2atmpS1701;
      _M0L1cS696
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L1cS696)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
      _M0L1cS696->$0 = _M0L6_2atmpS1700;
      _M0L3valS1619 = _M0L1cS696->$0;
      if (_M0L3valS1619 < 128) {
        int32_t _M0L3valS1621 = _M0L1cS696->$0;
        int32_t _M0L6_2atmpS1620;
        int32_t _M0L3valS1623;
        int32_t _M0L6_2atmpS1622;
        moonbit_decref(_M0L1cS696);
        _M0L6_2atmpS1620 = _M0L3valS1621;
        moonbit_incref(_M0L3resS693);
        #line 215 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS693, _M0L6_2atmpS1620);
        _M0L3valS1623 = _M0L1iS695->$0;
        _M0L6_2atmpS1622 = _M0L3valS1623 + 1;
        _M0L1iS695->$0 = _M0L6_2atmpS1622;
      } else {
        int32_t _M0L3valS1624 = _M0L1cS696->$0;
        if (_M0L3valS1624 < 224) {
          int32_t _M0L3valS1626 = _M0L1iS695->$0;
          int32_t _M0L6_2atmpS1625 = _M0L3valS1626 + 1;
          int32_t _M0L3valS1635;
          int32_t _M0L6_2atmpS1634;
          int32_t _M0L6_2atmpS1628;
          int32_t _M0L3valS1633;
          int32_t _M0L6_2atmpS1632;
          int32_t _M0L6_2atmpS1631;
          int32_t _M0L6_2atmpS1630;
          int32_t _M0L6_2atmpS1629;
          int32_t _M0L6_2atmpS1627;
          int32_t _M0L3valS1637;
          int32_t _M0L6_2atmpS1636;
          int32_t _M0L3valS1639;
          int32_t _M0L6_2atmpS1638;
          if (_M0L6_2atmpS1625 >= _M0L3lenS694) {
            moonbit_decref(_M0L1cS696);
            moonbit_decref(_M0L1iS695);
            moonbit_decref(_M0L5bytesS692);
            break;
          }
          _M0L3valS1635 = _M0L1cS696->$0;
          _M0L6_2atmpS1634 = _M0L3valS1635 & 31;
          _M0L6_2atmpS1628 = _M0L6_2atmpS1634 << 6;
          _M0L3valS1633 = _M0L1iS695->$0;
          _M0L6_2atmpS1632 = _M0L3valS1633 + 1;
          if (
            _M0L6_2atmpS1632 < 0
            || _M0L6_2atmpS1632 >= Moonbit_array_length(_M0L5bytesS692)
          ) {
            #line 221 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1631 = _M0L5bytesS692[_M0L6_2atmpS1632];
          _M0L6_2atmpS1630 = (int32_t)_M0L6_2atmpS1631;
          _M0L6_2atmpS1629 = _M0L6_2atmpS1630 & 63;
          _M0L6_2atmpS1627 = _M0L6_2atmpS1628 | _M0L6_2atmpS1629;
          _M0L1cS696->$0 = _M0L6_2atmpS1627;
          _M0L3valS1637 = _M0L1cS696->$0;
          moonbit_decref(_M0L1cS696);
          _M0L6_2atmpS1636 = _M0L3valS1637;
          moonbit_incref(_M0L3resS693);
          #line 222 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS693, _M0L6_2atmpS1636);
          _M0L3valS1639 = _M0L1iS695->$0;
          _M0L6_2atmpS1638 = _M0L3valS1639 + 2;
          _M0L1iS695->$0 = _M0L6_2atmpS1638;
        } else {
          int32_t _M0L3valS1640 = _M0L1cS696->$0;
          if (_M0L3valS1640 < 240) {
            int32_t _M0L3valS1642 = _M0L1iS695->$0;
            int32_t _M0L6_2atmpS1641 = _M0L3valS1642 + 2;
            int32_t _M0L3valS1658;
            int32_t _M0L6_2atmpS1657;
            int32_t _M0L6_2atmpS1650;
            int32_t _M0L3valS1656;
            int32_t _M0L6_2atmpS1655;
            int32_t _M0L6_2atmpS1654;
            int32_t _M0L6_2atmpS1653;
            int32_t _M0L6_2atmpS1652;
            int32_t _M0L6_2atmpS1651;
            int32_t _M0L6_2atmpS1644;
            int32_t _M0L3valS1649;
            int32_t _M0L6_2atmpS1648;
            int32_t _M0L6_2atmpS1647;
            int32_t _M0L6_2atmpS1646;
            int32_t _M0L6_2atmpS1645;
            int32_t _M0L6_2atmpS1643;
            int32_t _M0L3valS1660;
            int32_t _M0L6_2atmpS1659;
            int32_t _M0L3valS1662;
            int32_t _M0L6_2atmpS1661;
            if (_M0L6_2atmpS1641 >= _M0L3lenS694) {
              moonbit_decref(_M0L1cS696);
              moonbit_decref(_M0L1iS695);
              moonbit_decref(_M0L5bytesS692);
              break;
            }
            _M0L3valS1658 = _M0L1cS696->$0;
            _M0L6_2atmpS1657 = _M0L3valS1658 & 15;
            _M0L6_2atmpS1650 = _M0L6_2atmpS1657 << 12;
            _M0L3valS1656 = _M0L1iS695->$0;
            _M0L6_2atmpS1655 = _M0L3valS1656 + 1;
            if (
              _M0L6_2atmpS1655 < 0
              || _M0L6_2atmpS1655 >= Moonbit_array_length(_M0L5bytesS692)
            ) {
              #line 229 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1654 = _M0L5bytesS692[_M0L6_2atmpS1655];
            _M0L6_2atmpS1653 = (int32_t)_M0L6_2atmpS1654;
            _M0L6_2atmpS1652 = _M0L6_2atmpS1653 & 63;
            _M0L6_2atmpS1651 = _M0L6_2atmpS1652 << 6;
            _M0L6_2atmpS1644 = _M0L6_2atmpS1650 | _M0L6_2atmpS1651;
            _M0L3valS1649 = _M0L1iS695->$0;
            _M0L6_2atmpS1648 = _M0L3valS1649 + 2;
            if (
              _M0L6_2atmpS1648 < 0
              || _M0L6_2atmpS1648 >= Moonbit_array_length(_M0L5bytesS692)
            ) {
              #line 230 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1647 = _M0L5bytesS692[_M0L6_2atmpS1648];
            _M0L6_2atmpS1646 = (int32_t)_M0L6_2atmpS1647;
            _M0L6_2atmpS1645 = _M0L6_2atmpS1646 & 63;
            _M0L6_2atmpS1643 = _M0L6_2atmpS1644 | _M0L6_2atmpS1645;
            _M0L1cS696->$0 = _M0L6_2atmpS1643;
            _M0L3valS1660 = _M0L1cS696->$0;
            moonbit_decref(_M0L1cS696);
            _M0L6_2atmpS1659 = _M0L3valS1660;
            moonbit_incref(_M0L3resS693);
            #line 231 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS693, _M0L6_2atmpS1659);
            _M0L3valS1662 = _M0L1iS695->$0;
            _M0L6_2atmpS1661 = _M0L3valS1662 + 3;
            _M0L1iS695->$0 = _M0L6_2atmpS1661;
          } else {
            int32_t _M0L3valS1664 = _M0L1iS695->$0;
            int32_t _M0L6_2atmpS1663 = _M0L3valS1664 + 3;
            int32_t _M0L3valS1687;
            int32_t _M0L6_2atmpS1686;
            int32_t _M0L6_2atmpS1679;
            int32_t _M0L3valS1685;
            int32_t _M0L6_2atmpS1684;
            int32_t _M0L6_2atmpS1683;
            int32_t _M0L6_2atmpS1682;
            int32_t _M0L6_2atmpS1681;
            int32_t _M0L6_2atmpS1680;
            int32_t _M0L6_2atmpS1672;
            int32_t _M0L3valS1678;
            int32_t _M0L6_2atmpS1677;
            int32_t _M0L6_2atmpS1676;
            int32_t _M0L6_2atmpS1675;
            int32_t _M0L6_2atmpS1674;
            int32_t _M0L6_2atmpS1673;
            int32_t _M0L6_2atmpS1666;
            int32_t _M0L3valS1671;
            int32_t _M0L6_2atmpS1670;
            int32_t _M0L6_2atmpS1669;
            int32_t _M0L6_2atmpS1668;
            int32_t _M0L6_2atmpS1667;
            int32_t _M0L6_2atmpS1665;
            int32_t _M0L3valS1689;
            int32_t _M0L6_2atmpS1688;
            int32_t _M0L3valS1693;
            int32_t _M0L6_2atmpS1692;
            int32_t _M0L6_2atmpS1691;
            int32_t _M0L6_2atmpS1690;
            int32_t _M0L3valS1697;
            int32_t _M0L6_2atmpS1696;
            int32_t _M0L6_2atmpS1695;
            int32_t _M0L6_2atmpS1694;
            int32_t _M0L3valS1699;
            int32_t _M0L6_2atmpS1698;
            if (_M0L6_2atmpS1663 >= _M0L3lenS694) {
              moonbit_decref(_M0L1cS696);
              moonbit_decref(_M0L1iS695);
              moonbit_decref(_M0L5bytesS692);
              break;
            }
            _M0L3valS1687 = _M0L1cS696->$0;
            _M0L6_2atmpS1686 = _M0L3valS1687 & 7;
            _M0L6_2atmpS1679 = _M0L6_2atmpS1686 << 18;
            _M0L3valS1685 = _M0L1iS695->$0;
            _M0L6_2atmpS1684 = _M0L3valS1685 + 1;
            if (
              _M0L6_2atmpS1684 < 0
              || _M0L6_2atmpS1684 >= Moonbit_array_length(_M0L5bytesS692)
            ) {
              #line 238 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1683 = _M0L5bytesS692[_M0L6_2atmpS1684];
            _M0L6_2atmpS1682 = (int32_t)_M0L6_2atmpS1683;
            _M0L6_2atmpS1681 = _M0L6_2atmpS1682 & 63;
            _M0L6_2atmpS1680 = _M0L6_2atmpS1681 << 12;
            _M0L6_2atmpS1672 = _M0L6_2atmpS1679 | _M0L6_2atmpS1680;
            _M0L3valS1678 = _M0L1iS695->$0;
            _M0L6_2atmpS1677 = _M0L3valS1678 + 2;
            if (
              _M0L6_2atmpS1677 < 0
              || _M0L6_2atmpS1677 >= Moonbit_array_length(_M0L5bytesS692)
            ) {
              #line 239 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1676 = _M0L5bytesS692[_M0L6_2atmpS1677];
            _M0L6_2atmpS1675 = (int32_t)_M0L6_2atmpS1676;
            _M0L6_2atmpS1674 = _M0L6_2atmpS1675 & 63;
            _M0L6_2atmpS1673 = _M0L6_2atmpS1674 << 6;
            _M0L6_2atmpS1666 = _M0L6_2atmpS1672 | _M0L6_2atmpS1673;
            _M0L3valS1671 = _M0L1iS695->$0;
            _M0L6_2atmpS1670 = _M0L3valS1671 + 3;
            if (
              _M0L6_2atmpS1670 < 0
              || _M0L6_2atmpS1670 >= Moonbit_array_length(_M0L5bytesS692)
            ) {
              #line 240 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1669 = _M0L5bytesS692[_M0L6_2atmpS1670];
            _M0L6_2atmpS1668 = (int32_t)_M0L6_2atmpS1669;
            _M0L6_2atmpS1667 = _M0L6_2atmpS1668 & 63;
            _M0L6_2atmpS1665 = _M0L6_2atmpS1666 | _M0L6_2atmpS1667;
            _M0L1cS696->$0 = _M0L6_2atmpS1665;
            _M0L3valS1689 = _M0L1cS696->$0;
            _M0L6_2atmpS1688 = _M0L3valS1689 - 65536;
            _M0L1cS696->$0 = _M0L6_2atmpS1688;
            _M0L3valS1693 = _M0L1cS696->$0;
            _M0L6_2atmpS1692 = _M0L3valS1693 >> 10;
            _M0L6_2atmpS1691 = _M0L6_2atmpS1692 + 55296;
            _M0L6_2atmpS1690 = _M0L6_2atmpS1691;
            moonbit_incref(_M0L3resS693);
            #line 242 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS693, _M0L6_2atmpS1690);
            _M0L3valS1697 = _M0L1cS696->$0;
            moonbit_decref(_M0L1cS696);
            _M0L6_2atmpS1696 = _M0L3valS1697 & 1023;
            _M0L6_2atmpS1695 = _M0L6_2atmpS1696 + 56320;
            _M0L6_2atmpS1694 = _M0L6_2atmpS1695;
            moonbit_incref(_M0L3resS693);
            #line 243 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS693, _M0L6_2atmpS1694);
            _M0L3valS1699 = _M0L1iS695->$0;
            _M0L6_2atmpS1698 = _M0L3valS1699 + 4;
            _M0L1iS695->$0 = _M0L6_2atmpS1698;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS695);
      moonbit_decref(_M0L5bytesS692);
    }
    break;
  }
  #line 247 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS693);
}

int32_t _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S685(
  int32_t _M0L6_2aenvS1610,
  moonbit_string_t _M0L1sS686
) {
  struct _M0TPB8MutLocalGiE* _M0L3resS687;
  int32_t _M0L3lenS688;
  int32_t _M0L1iS689;
  int32_t _result_2013;
  #line 197 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS687
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L3resS687)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L3resS687->$0 = 0;
  _M0L3lenS688 = Moonbit_array_length(_M0L1sS686);
  _M0L1iS689 = 0;
  while (1) {
    if (_M0L1iS689 < _M0L3lenS688) {
      int32_t _M0L3valS1615 = _M0L3resS687->$0;
      int32_t _M0L6_2atmpS1612 = _M0L3valS1615 * 10;
      int32_t _M0L6_2atmpS1614;
      int32_t _M0L6_2atmpS1613;
      int32_t _M0L6_2atmpS1611;
      int32_t _M0L6_2atmpS1616;
      if (_M0L1iS689 < 0 || _M0L1iS689 >= Moonbit_array_length(_M0L1sS686)) {
        #line 201 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1614 = _M0L1sS686[_M0L1iS689];
      _M0L6_2atmpS1613 = _M0L6_2atmpS1614 - 48;
      _M0L6_2atmpS1611 = _M0L6_2atmpS1612 + _M0L6_2atmpS1613;
      _M0L3resS687->$0 = _M0L6_2atmpS1611;
      _M0L6_2atmpS1616 = _M0L1iS689 + 1;
      _M0L1iS689 = _M0L6_2atmpS1616;
      continue;
    } else {
      moonbit_decref(_M0L1sS686);
    }
    break;
  }
  _result_2013 = _M0L3resS687->$0;
  moonbit_decref(_M0L3resS687);
  return _result_2013;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S665,
  moonbit_string_t _M0L12_2adiscard__S666,
  int32_t _M0L12_2adiscard__S667,
  struct _M0TWssbEu* _M0L12_2adiscard__S668,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S669
) {
  struct moonbit_result_0 _result_2014;
  #line 34 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S669);
  moonbit_decref(_M0L12_2adiscard__S668);
  moonbit_decref(_M0L12_2adiscard__S666);
  moonbit_decref(_M0L12_2adiscard__S665);
  _result_2014.tag = 1;
  _result_2014.data.ok = 0;
  return _result_2014;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S670,
  moonbit_string_t _M0L12_2adiscard__S671,
  int32_t _M0L12_2adiscard__S672,
  struct _M0TWssbEu* _M0L12_2adiscard__S673,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S674
) {
  struct moonbit_result_0 _result_2015;
  #line 34 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S674);
  moonbit_decref(_M0L12_2adiscard__S673);
  moonbit_decref(_M0L12_2adiscard__S671);
  moonbit_decref(_M0L12_2adiscard__S670);
  _result_2015.tag = 1;
  _result_2015.data.ok = 0;
  return _result_2015;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S675,
  moonbit_string_t _M0L12_2adiscard__S676,
  int32_t _M0L12_2adiscard__S677,
  struct _M0TWssbEu* _M0L12_2adiscard__S678,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S679
) {
  struct moonbit_result_0 _result_2016;
  #line 34 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S679);
  moonbit_decref(_M0L12_2adiscard__S678);
  moonbit_decref(_M0L12_2adiscard__S676);
  moonbit_decref(_M0L12_2adiscard__S675);
  _result_2016.tag = 1;
  _result_2016.data.ok = 0;
  return _result_2016;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp32plugin__examples__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S680,
  moonbit_string_t _M0L12_2adiscard__S681,
  int32_t _M0L12_2adiscard__S682,
  struct _M0TWssbEu* _M0L12_2adiscard__S683,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S684
) {
  struct moonbit_result_0 _result_2017;
  #line 34 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S684);
  moonbit_decref(_M0L12_2adiscard__S683);
  moonbit_decref(_M0L12_2adiscard__S681);
  moonbit_decref(_M0L12_2adiscard__S680);
  _result_2017.tag = 1;
  _result_2017.data.ok = 0;
  return _result_2017;
}

int32_t _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp32plugin__examples__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S664
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S664);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test59____test__6275696c645f77726170706572735f746573742e6d6274__1(
  
) {
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0L7commandS663;
  moonbit_string_t _M0L6_2atmpS1599;
  struct _M0TPB4Show _M0L6_2atmpS1592;
  moonbit_string_t _M0L6_2atmpS1595;
  moonbit_string_t _M0L6_2atmpS1596;
  moonbit_string_t _M0L6_2atmpS1597;
  moonbit_string_t _M0L6_2atmpS1598;
  moonbit_string_t* _M0L6_2atmpS1594;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1593;
  struct moonbit_result_0 _tmp_2018;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1609;
  struct _M0TPB4Show _M0L6_2atmpS1602;
  moonbit_string_t _M0L6_2atmpS1605;
  moonbit_string_t _M0L6_2atmpS1606;
  moonbit_string_t _M0L6_2atmpS1607;
  moonbit_string_t _M0L6_2atmpS1608;
  moonbit_string_t* _M0L6_2atmpS1604;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1603;
  #line 9 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  #line 10 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _M0L7commandS663
  = _M0FP36mulpjs4mulp16plugin__examples26typescript__build__wrapper((moonbit_string_t)moonbit_string_literal_9.data);
  moonbit_incref(_M0L7commandS663);
  #line 11 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _M0L6_2atmpS1599
  = _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand7program(_M0L7commandS663);
  _M0L6_2atmpS1592
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1599
  };
  _M0L6_2atmpS1595 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1596 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS1597 = 0;
  _M0L6_2atmpS1598 = 0;
  _M0L6_2atmpS1594 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1594[0] = _M0L6_2atmpS1595;
  _M0L6_2atmpS1594[1] = _M0L6_2atmpS1596;
  _M0L6_2atmpS1594[2] = _M0L6_2atmpS1597;
  _M0L6_2atmpS1594[3] = _M0L6_2atmpS1598;
  _M0L6_2atmpS1593
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1593)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1593->$0 = _M0L6_2atmpS1594;
  _M0L6_2atmpS1593->$1 = 4;
  #line 11 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _tmp_2018
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1592, (moonbit_string_t)moonbit_string_literal_12.data, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS1593);
  if (_tmp_2018.tag) {
    int32_t const _M0L5_2aokS1600 = _tmp_2018.data.ok;
  } else {
    void* const _M0L6_2aerrS1601 = _tmp_2018.data.err;
    struct moonbit_result_0 _result_2019;
    moonbit_decref(_M0L7commandS663);
    _result_2019.tag = 0;
    _result_2019.data.err = _M0L6_2aerrS1601;
    return _result_2019;
  }
  #line 12 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _M0L6_2atmpS1609
  = _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand4args(_M0L7commandS663);
  _M0L6_2atmpS1602
  = (struct _M0TPB4Show){
    _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1609
  };
  _M0L6_2atmpS1605 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS1606 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS1607 = 0;
  _M0L6_2atmpS1608 = 0;
  _M0L6_2atmpS1604 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1604[0] = _M0L6_2atmpS1605;
  _M0L6_2atmpS1604[1] = _M0L6_2atmpS1606;
  _M0L6_2atmpS1604[2] = _M0L6_2atmpS1607;
  _M0L6_2atmpS1604[3] = _M0L6_2atmpS1608;
  _M0L6_2atmpS1603
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1603)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1603->$0 = _M0L6_2atmpS1604;
  _M0L6_2atmpS1603->$1 = 4;
  #line 12 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1602, (moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS1603);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp32plugin__examples__blackbox__test59____test__6275696c645f77726170706572735f746573742e6d6274__0(
  
) {
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0L7commandS662;
  moonbit_string_t _M0L6_2atmpS1581;
  struct _M0TPB4Show _M0L6_2atmpS1574;
  moonbit_string_t _M0L6_2atmpS1577;
  moonbit_string_t _M0L6_2atmpS1578;
  moonbit_string_t _M0L6_2atmpS1579;
  moonbit_string_t _M0L6_2atmpS1580;
  moonbit_string_t* _M0L6_2atmpS1576;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1575;
  struct moonbit_result_0 _tmp_2020;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1591;
  struct _M0TPB4Show _M0L6_2atmpS1584;
  moonbit_string_t _M0L6_2atmpS1587;
  moonbit_string_t _M0L6_2atmpS1588;
  moonbit_string_t _M0L6_2atmpS1589;
  moonbit_string_t _M0L6_2atmpS1590;
  moonbit_string_t* _M0L6_2atmpS1586;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1585;
  #line 2 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  #line 3 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _M0L7commandS662
  = _M0FP36mulpjs4mulp16plugin__examples23moonbit__build__wrapper((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_19.data);
  moonbit_incref(_M0L7commandS662);
  #line 4 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _M0L6_2atmpS1581
  = _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand7program(_M0L7commandS662);
  _M0L6_2atmpS1574
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1581
  };
  _M0L6_2atmpS1577 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS1578 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS1579 = 0;
  _M0L6_2atmpS1580 = 0;
  _M0L6_2atmpS1576 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1576[0] = _M0L6_2atmpS1577;
  _M0L6_2atmpS1576[1] = _M0L6_2atmpS1578;
  _M0L6_2atmpS1576[2] = _M0L6_2atmpS1579;
  _M0L6_2atmpS1576[3] = _M0L6_2atmpS1580;
  _M0L6_2atmpS1575
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1575)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1575->$0 = _M0L6_2atmpS1576;
  _M0L6_2atmpS1575->$1 = 4;
  #line 4 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _tmp_2020
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1574, (moonbit_string_t)moonbit_string_literal_22.data, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS1575);
  if (_tmp_2020.tag) {
    int32_t const _M0L5_2aokS1582 = _tmp_2020.data.ok;
  } else {
    void* const _M0L6_2aerrS1583 = _tmp_2020.data.err;
    struct moonbit_result_0 _result_2021;
    moonbit_decref(_M0L7commandS662);
    _result_2021.tag = 0;
    _result_2021.data.err = _M0L6_2aerrS1583;
    return _result_2021;
  }
  #line 5 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  _M0L6_2atmpS1591
  = _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand4args(_M0L7commandS662);
  _M0L6_2atmpS1584
  = (struct _M0TPB4Show){
    _M0FP0121moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1591
  };
  _M0L6_2atmpS1587 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS1588 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS1589 = 0;
  _M0L6_2atmpS1590 = 0;
  _M0L6_2atmpS1586 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1586[0] = _M0L6_2atmpS1587;
  _M0L6_2atmpS1586[1] = _M0L6_2atmpS1588;
  _M0L6_2atmpS1586[2] = _M0L6_2atmpS1589;
  _M0L6_2atmpS1586[3] = _M0L6_2atmpS1590;
  _M0L6_2atmpS1585
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1585)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1585->$0 = _M0L6_2atmpS1586;
  _M0L6_2atmpS1585->$1 = 4;
  #line 5 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1584, (moonbit_string_t)moonbit_string_literal_26.data, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS1585);
}

struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0FP36mulpjs4mulp16plugin__examples26typescript__build__wrapper(
  moonbit_string_t _M0L7projectS661
) {
  moonbit_string_t* _M0L6_2atmpS1573;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1572;
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _block_2022;
  #line 33 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
  _M0L6_2atmpS1573 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS1573[0] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS1573[1] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS1573[2] = _M0L7projectS661;
  _M0L6_2atmpS1572
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1572)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1572->$0 = _M0L6_2atmpS1573;
  _M0L6_2atmpS1572->$1 = 3;
  _block_2022
  = (struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand));
  Moonbit_object_header(_block_2022)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand, $0) >> 2, 2, 0);
  _block_2022->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _block_2022->$1 = _M0L6_2atmpS1572;
  return _block_2022;
}

struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0FP36mulpjs4mulp16plugin__examples23moonbit__build__wrapper(
  moonbit_string_t _M0L6targetS659,
  moonbit_string_t _M0L13package__nameS660
) {
  moonbit_string_t* _M0L6_2atmpS1571;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS1570;
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _block_2023;
  #line 22 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
  _M0L6_2atmpS1571 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1571[0] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS1571[1] = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS1571[2] = _M0L6targetS659;
  _M0L6_2atmpS1571[3] = _M0L13package__nameS660;
  _M0L6_2atmpS1570
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS1570)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1570->$0 = _M0L6_2atmpS1571;
  _M0L6_2atmpS1570->$1 = 4;
  _block_2023
  = (struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand));
  Moonbit_object_header(_block_2023)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand, $0) >> 2, 2, 0);
  _block_2023->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _block_2023->$1 = _M0L6_2atmpS1570;
  return _block_2023;
}

struct _M0TPB5ArrayGsE* _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand4args(
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0L4selfS654
) {
  moonbit_string_t* _M0L6_2atmpS1569;
  struct _M0TPB5ArrayGsE* _M0L6copiedS652;
  struct _M0TPB5ArrayGsE* _M0L8_2afieldS1803;
  int32_t _M0L6_2acntS1933;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS653;
  int32_t _M0L7_2abindS655;
  int32_t _M0L2__S656;
  #line 13 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
  _M0L6_2atmpS1569 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6copiedS652
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6copiedS652)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6copiedS652->$0 = _M0L6_2atmpS1569;
  _M0L6copiedS652->$1 = 0;
  _M0L8_2afieldS1803 = _M0L4selfS654->$1;
  _M0L6_2acntS1933 = Moonbit_object_header(_M0L4selfS654)->rc;
  if (_M0L6_2acntS1933 > 1) {
    int32_t _M0L11_2anew__cntS1935 = _M0L6_2acntS1933 - 1;
    Moonbit_object_header(_M0L4selfS654)->rc = _M0L11_2anew__cntS1935;
    moonbit_incref(_M0L8_2afieldS1803);
  } else if (_M0L6_2acntS1933 == 1) {
    moonbit_string_t _M0L8_2afieldS1934 = _M0L4selfS654->$0;
    moonbit_decref(_M0L8_2afieldS1934);
    #line 15 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
    moonbit_free(_M0L4selfS654);
  }
  _M0L7_2abindS653 = _M0L8_2afieldS1803;
  _M0L7_2abindS655 = _M0L7_2abindS653->$1;
  _M0L2__S656 = 0;
  while (1) {
    if (_M0L2__S656 < _M0L7_2abindS655) {
      moonbit_string_t* _M0L3bufS1568 = _M0L7_2abindS653->$0;
      moonbit_string_t _M0L3argS657 =
        (moonbit_string_t)_M0L3bufS1568[_M0L2__S656];
      int32_t _M0L6_2atmpS1567;
      moonbit_incref(_M0L3argS657);
      moonbit_incref(_M0L6copiedS652);
      #line 16 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6copiedS652, _M0L3argS657);
      _M0L6_2atmpS1567 = _M0L2__S656 + 1;
      _M0L2__S656 = _M0L6_2atmpS1567;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS653);
    }
    break;
  }
  return _M0L6copiedS652;
}

moonbit_string_t _M0MP36mulpjs4mulp16plugin__examples19BuildWrapperCommand7program(
  struct _M0TP36mulpjs4mulp16plugin__examples19BuildWrapperCommand* _M0L4selfS651
) {
  moonbit_string_t _M0L8_2afieldS1804;
  int32_t _M0L6_2acntS1936;
  #line 8 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
  _M0L8_2afieldS1804 = _M0L4selfS651->$0;
  _M0L6_2acntS1936 = Moonbit_object_header(_M0L4selfS651)->rc;
  if (_M0L6_2acntS1936 > 1) {
    int32_t _M0L11_2anew__cntS1938 = _M0L6_2acntS1936 - 1;
    Moonbit_object_header(_M0L4selfS651)->rc = _M0L11_2anew__cntS1938;
    moonbit_incref(_M0L8_2afieldS1804);
  } else if (_M0L6_2acntS1936 == 1) {
    struct _M0TPB5ArrayGsE* _M0L8_2afieldS1937 = _M0L4selfS651->$1;
    moonbit_decref(_M0L8_2afieldS1937);
    #line 9 "/Users/user/workspace/github/gulp/mulp/plugin_examples/build_wrappers.mbt"
    moonbit_free(_M0L4selfS651);
  }
  return _M0L8_2afieldS1804;
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS649,
  int32_t _M0L5indexS650
) {
  int32_t _M0L3lenS648;
  int32_t _if__result_2025;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS648 = _M0L4selfS649->$1;
  if (_M0L5indexS650 >= 0) {
    _if__result_2025 = _M0L5indexS650 < _M0L3lenS648;
  } else {
    _if__result_2025 = 0;
  }
  if (_if__result_2025) {
    moonbit_string_t* _M0L6_2atmpS1566;
    moonbit_string_t _M0L6_2atmpS1805;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS1566 = _M0MPC15array5Array6bufferGsE(_M0L4selfS649);
    if (
      _M0L5indexS650 < 0
      || _M0L5indexS650 >= Moonbit_array_length(_M0L6_2atmpS1566)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1805 = (moonbit_string_t)_M0L6_2atmpS1566[_M0L5indexS650];
    moonbit_incref(_M0L6_2atmpS1805);
    moonbit_decref(_M0L6_2atmpS1566);
    return _M0L6_2atmpS1805;
  } else {
    moonbit_decref(_M0L4selfS649);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS647) {
  moonbit_string_t _M0L6_2atmpS1565;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS1565 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS647);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS1565);
  moonbit_decref(_M0L6_2atmpS1565);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS646,
  struct _M0TPB6Hasher* _M0L6hasherS645
) {
  #line 530 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 531 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS645, _M0L4selfS646);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS644,
  struct _M0TPB6Hasher* _M0L6hasherS643
) {
  #line 496 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 497 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS643, _M0L4selfS644);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS641,
  moonbit_string_t _M0L5valueS639
) {
  int32_t _M0L7_2abindS638;
  int32_t _M0L1iS640;
  #line 387 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS638 = Moonbit_array_length(_M0L5valueS639);
  _M0L1iS640 = 0;
  while (1) {
    if (_M0L1iS640 < _M0L7_2abindS638) {
      int32_t _M0L6_2atmpS1563 = _M0L5valueS639[_M0L1iS640];
      int32_t _M0L6_2atmpS1562 = (int32_t)_M0L6_2atmpS1563;
      uint32_t _M0L6_2atmpS1561 = *(uint32_t*)&_M0L6_2atmpS1562;
      int32_t _M0L6_2atmpS1564;
      moonbit_incref(_M0L4selfS641);
      #line 389 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS641, _M0L6_2atmpS1561);
      _M0L6_2atmpS1564 = _M0L1iS640 + 1;
      _M0L1iS640 = _M0L6_2atmpS1564;
      continue;
    } else {
      moonbit_decref(_M0L4selfS641);
      moonbit_decref(_M0L5valueS639);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS636,
  int32_t _M0L3idxS637
) {
  int32_t _result_2027;
  #line 1778 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _result_2027 = _M0L4selfS636[_M0L3idxS637];
  moonbit_decref(_M0L4selfS636);
  return _result_2027;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS623,
  int32_t _M0L3keyS619
) {
  int32_t _M0L4hashS618;
  int32_t _M0L14capacity__maskS1546;
  int32_t _M0L6_2atmpS1545;
  int32_t _M0L1iS620;
  int32_t _M0L3idxS621;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS618 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS619);
  _M0L14capacity__maskS1546 = _M0L4selfS623->$3;
  _M0L6_2atmpS1545 = _M0L4hashS618 & _M0L14capacity__maskS1546;
  _M0L1iS620 = 0;
  _M0L3idxS621 = _M0L6_2atmpS1545;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1544 =
      _M0L4selfS623->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS622;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS1544)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS622
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1544[
        _M0L3idxS621
      ];
    if (_M0L7_2abindS622 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1533;
      if (_M0L7_2abindS622) {
        moonbit_incref(_M0L7_2abindS622);
      }
      moonbit_decref(_M0L4selfS623);
      if (_M0L7_2abindS622) {
        moonbit_decref(_M0L7_2abindS622);
      }
      _M0L6_2atmpS1533 = 0;
      return _M0L6_2atmpS1533;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS624 =
        _M0L7_2abindS622;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS625 =
        _M0L7_2aSomeS624;
      int32_t _M0L4hashS1535 = _M0L8_2aentryS625->$3;
      int32_t _if__result_2029;
      int32_t _M0L3pslS1538;
      int32_t _M0L6_2atmpS1540;
      int32_t _M0L6_2atmpS1542;
      int32_t _M0L14capacity__maskS1543;
      int32_t _M0L6_2atmpS1541;
      if (_M0L4hashS1535 == _M0L4hashS618) {
        int32_t _M0L3keyS1534 = _M0L8_2aentryS625->$4;
        _if__result_2029 = _M0L3keyS1534 == _M0L3keyS619;
      } else {
        _if__result_2029 = 0;
      }
      if (_if__result_2029) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1806;
        int32_t _M0L6_2acntS1939;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1537;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1536;
        moonbit_incref(_M0L8_2aentryS625);
        moonbit_decref(_M0L4selfS623);
        _M0L8_2afieldS1806 = _M0L8_2aentryS625->$5;
        _M0L6_2acntS1939 = Moonbit_object_header(_M0L8_2aentryS625)->rc;
        if (_M0L6_2acntS1939 > 1) {
          int32_t _M0L11_2anew__cntS1941 = _M0L6_2acntS1939 - 1;
          Moonbit_object_header(_M0L8_2aentryS625)->rc
          = _M0L11_2anew__cntS1941;
          moonbit_incref(_M0L8_2afieldS1806);
        } else if (_M0L6_2acntS1939 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1940 =
            _M0L8_2aentryS625->$1;
          if (_M0L8_2afieldS1940) {
            moonbit_decref(_M0L8_2afieldS1940);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS625);
        }
        _M0L5valueS1537 = _M0L8_2afieldS1806;
        _M0L6_2atmpS1536 = _M0L5valueS1537;
        return _M0L6_2atmpS1536;
      } else {
        moonbit_incref(_M0L8_2aentryS625);
      }
      _M0L3pslS1538 = _M0L8_2aentryS625->$2;
      moonbit_decref(_M0L8_2aentryS625);
      if (_M0L1iS620 > _M0L3pslS1538) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1539;
        moonbit_decref(_M0L4selfS623);
        _M0L6_2atmpS1539 = 0;
        return _M0L6_2atmpS1539;
      }
      _M0L6_2atmpS1540 = _M0L1iS620 + 1;
      _M0L6_2atmpS1542 = _M0L3idxS621 + 1;
      _M0L14capacity__maskS1543 = _M0L4selfS623->$3;
      _M0L6_2atmpS1541 = _M0L6_2atmpS1542 & _M0L14capacity__maskS1543;
      _M0L1iS620 = _M0L6_2atmpS1540;
      _M0L3idxS621 = _M0L6_2atmpS1541;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS632,
  moonbit_string_t _M0L3keyS628
) {
  int32_t _M0L4hashS627;
  int32_t _M0L14capacity__maskS1560;
  int32_t _M0L6_2atmpS1559;
  int32_t _M0L1iS629;
  int32_t _M0L3idxS630;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS628);
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS627 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS628);
  _M0L14capacity__maskS1560 = _M0L4selfS632->$3;
  _M0L6_2atmpS1559 = _M0L4hashS627 & _M0L14capacity__maskS1560;
  _M0L1iS629 = 0;
  _M0L3idxS630 = _M0L6_2atmpS1559;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1558 =
      _M0L4selfS632->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS631;
    if (
      _M0L3idxS630 < 0
      || _M0L3idxS630 >= Moonbit_array_length(_M0L7entriesS1558)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS631
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1558[
        _M0L3idxS630
      ];
    if (_M0L7_2abindS631 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1547;
      if (_M0L7_2abindS631) {
        moonbit_incref(_M0L7_2abindS631);
      }
      moonbit_decref(_M0L4selfS632);
      if (_M0L7_2abindS631) {
        moonbit_decref(_M0L7_2abindS631);
      }
      moonbit_decref(_M0L3keyS628);
      _M0L6_2atmpS1547 = 0;
      return _M0L6_2atmpS1547;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS633 =
        _M0L7_2abindS631;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS634 =
        _M0L7_2aSomeS633;
      int32_t _M0L4hashS1549 = _M0L8_2aentryS634->$3;
      int32_t _if__result_2031;
      int32_t _M0L3pslS1552;
      int32_t _M0L6_2atmpS1554;
      int32_t _M0L6_2atmpS1556;
      int32_t _M0L14capacity__maskS1557;
      int32_t _M0L6_2atmpS1555;
      if (_M0L4hashS1549 == _M0L4hashS627) {
        moonbit_string_t _M0L3keyS1548 = _M0L8_2aentryS634->$4;
        #line 220 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_2031
        = moonbit_val_array_equal(_M0L3keyS1548, _M0L3keyS628);
      } else {
        _if__result_2031 = 0;
      }
      if (_if__result_2031) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1809;
        int32_t _M0L6_2acntS1942;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1551;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1550;
        moonbit_incref(_M0L8_2aentryS634);
        moonbit_decref(_M0L4selfS632);
        moonbit_decref(_M0L3keyS628);
        _M0L8_2afieldS1809 = _M0L8_2aentryS634->$5;
        _M0L6_2acntS1942 = Moonbit_object_header(_M0L8_2aentryS634)->rc;
        if (_M0L6_2acntS1942 > 1) {
          int32_t _M0L11_2anew__cntS1945 = _M0L6_2acntS1942 - 1;
          Moonbit_object_header(_M0L8_2aentryS634)->rc
          = _M0L11_2anew__cntS1945;
          moonbit_incref(_M0L8_2afieldS1809);
        } else if (_M0L6_2acntS1942 == 1) {
          moonbit_string_t _M0L8_2afieldS1944 = _M0L8_2aentryS634->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1943;
          moonbit_decref(_M0L8_2afieldS1944);
          _M0L8_2afieldS1943 = _M0L8_2aentryS634->$1;
          if (_M0L8_2afieldS1943) {
            moonbit_decref(_M0L8_2afieldS1943);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS634);
        }
        _M0L5valueS1551 = _M0L8_2afieldS1809;
        _M0L6_2atmpS1550 = _M0L5valueS1551;
        return _M0L6_2atmpS1550;
      } else {
        moonbit_incref(_M0L8_2aentryS634);
      }
      _M0L3pslS1552 = _M0L8_2aentryS634->$2;
      moonbit_decref(_M0L8_2aentryS634);
      if (_M0L1iS629 > _M0L3pslS1552) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1553;
        moonbit_decref(_M0L4selfS632);
        moonbit_decref(_M0L3keyS628);
        _M0L6_2atmpS1553 = 0;
        return _M0L6_2atmpS1553;
      }
      _M0L6_2atmpS1554 = _M0L1iS629 + 1;
      _M0L6_2atmpS1556 = _M0L3idxS630 + 1;
      _M0L14capacity__maskS1557 = _M0L4selfS632->$3;
      _M0L6_2atmpS1555 = _M0L6_2atmpS1556 & _M0L14capacity__maskS1557;
      _M0L1iS629 = _M0L6_2atmpS1554;
      _M0L3idxS630 = _M0L6_2atmpS1555;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS603
) {
  int32_t _M0L6lengthS602;
  int32_t _M0Lm8capacityS604;
  int32_t _M0L6_2atmpS1510;
  int32_t _M0L6_2atmpS1509;
  int32_t _M0L6_2atmpS1520;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS605;
  int32_t _M0L3endS1518;
  int32_t _M0L5startS1519;
  int32_t _M0L7_2abindS606;
  int32_t _M0L2__S607;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS603.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS602
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS603);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS604 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS602);
  _M0L6_2atmpS1510 = _M0Lm8capacityS604;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1509 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1510);
  if (_M0L6lengthS602 > _M0L6_2atmpS1509) {
    int32_t _M0L6_2atmpS1511 = _M0Lm8capacityS604;
    _M0Lm8capacityS604 = _M0L6_2atmpS1511 * 2;
  }
  _M0L6_2atmpS1520 = _M0Lm8capacityS604;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS605
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1520);
  _M0L3endS1518 = _M0L3arrS603.$2;
  _M0L5startS1519 = _M0L3arrS603.$1;
  _M0L7_2abindS606 = _M0L3endS1518 - _M0L5startS1519;
  _M0L2__S607 = 0;
  while (1) {
    if (_M0L2__S607 < _M0L7_2abindS606) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1515 =
        _M0L3arrS603.$0;
      int32_t _M0L5startS1517 = _M0L3arrS603.$1;
      int32_t _M0L6_2atmpS1516 = _M0L5startS1517 + _M0L2__S607;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS608 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1515[
          _M0L6_2atmpS1516
        ];
      moonbit_string_t _M0L6_2atmpS1512 = _M0L1eS608->$0;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1513 =
        _M0L1eS608->$1;
      int32_t _M0L6_2atmpS1514;
      moonbit_incref(_M0L6_2atmpS1513);
      moonbit_incref(_M0L6_2atmpS1512);
      moonbit_incref(_M0L1mS605);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS605, _M0L6_2atmpS1512, _M0L6_2atmpS1513);
      _M0L6_2atmpS1514 = _M0L2__S607 + 1;
      _M0L2__S607 = _M0L6_2atmpS1514;
      continue;
    } else {
      moonbit_decref(_M0L3arrS603.$0);
    }
    break;
  }
  return _M0L1mS605;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS611
) {
  int32_t _M0L6lengthS610;
  int32_t _M0Lm8capacityS612;
  int32_t _M0L6_2atmpS1522;
  int32_t _M0L6_2atmpS1521;
  int32_t _M0L6_2atmpS1532;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS613;
  int32_t _M0L3endS1530;
  int32_t _M0L5startS1531;
  int32_t _M0L7_2abindS614;
  int32_t _M0L2__S615;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS611.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS610
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS611);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS612 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS610);
  _M0L6_2atmpS1522 = _M0Lm8capacityS612;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1521 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1522);
  if (_M0L6lengthS610 > _M0L6_2atmpS1521) {
    int32_t _M0L6_2atmpS1523 = _M0Lm8capacityS612;
    _M0Lm8capacityS612 = _M0L6_2atmpS1523 * 2;
  }
  _M0L6_2atmpS1532 = _M0Lm8capacityS612;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS613
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1532);
  _M0L3endS1530 = _M0L3arrS611.$2;
  _M0L5startS1531 = _M0L3arrS611.$1;
  _M0L7_2abindS614 = _M0L3endS1530 - _M0L5startS1531;
  _M0L2__S615 = 0;
  while (1) {
    if (_M0L2__S615 < _M0L7_2abindS614) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1527 =
        _M0L3arrS611.$0;
      int32_t _M0L5startS1529 = _M0L3arrS611.$1;
      int32_t _M0L6_2atmpS1528 = _M0L5startS1529 + _M0L2__S615;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS616 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1527[
          _M0L6_2atmpS1528
        ];
      int32_t _M0L6_2atmpS1524 = _M0L1eS616->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1525 = _M0L1eS616->$1;
      int32_t _M0L6_2atmpS1526;
      moonbit_incref(_M0L6_2atmpS1525);
      moonbit_incref(_M0L1mS613);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS613, _M0L6_2atmpS1524, _M0L6_2atmpS1525);
      _M0L6_2atmpS1526 = _M0L2__S615 + 1;
      _M0L2__S615 = _M0L6_2atmpS1526;
      continue;
    } else {
      moonbit_decref(_M0L3arrS611.$0);
    }
    break;
  }
  return _M0L1mS613;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS596,
  moonbit_string_t _M0L3keyS597,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS598
) {
  int32_t _M0L6_2atmpS1507;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS597);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1507 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS597);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS596, _M0L3keyS597, _M0L5valueS598, _M0L6_2atmpS1507);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS599,
  int32_t _M0L3keyS600,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS601
) {
  int32_t _M0L6_2atmpS1508;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1508 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS600);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS599, _M0L3keyS600, _M0L5valueS601, _M0L6_2atmpS1508);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS575
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS574;
  int32_t _M0L8capacityS1499;
  int32_t _M0L13new__capacityS576;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1494;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1493;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS1824;
  int32_t _M0L6_2atmpS1495;
  int32_t _M0L8capacityS1497;
  int32_t _M0L6_2atmpS1496;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1498;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1823;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1xS577;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS574 = _M0L4selfS575->$5;
  _M0L8capacityS1499 = _M0L4selfS575->$2;
  _M0L13new__capacityS576 = _M0L8capacityS1499 << 1;
  _M0L6_2atmpS1494 = 0;
  _M0L6_2atmpS1493
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS576, _M0L6_2atmpS1494);
  _M0L6_2aoldS1824 = _M0L4selfS575->$0;
  if (_M0L9old__headS574) {
    moonbit_incref(_M0L9old__headS574);
  }
  moonbit_decref(_M0L6_2aoldS1824);
  _M0L4selfS575->$0 = _M0L6_2atmpS1493;
  _M0L4selfS575->$2 = _M0L13new__capacityS576;
  _M0L6_2atmpS1495 = _M0L13new__capacityS576 - 1;
  _M0L4selfS575->$3 = _M0L6_2atmpS1495;
  _M0L8capacityS1497 = _M0L4selfS575->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1496 = _M0FPB21calc__grow__threshold(_M0L8capacityS1497);
  _M0L4selfS575->$4 = _M0L6_2atmpS1496;
  _M0L4selfS575->$1 = 0;
  _M0L6_2atmpS1498 = 0;
  _M0L6_2aoldS1823 = _M0L4selfS575->$5;
  if (_M0L6_2aoldS1823) {
    moonbit_decref(_M0L6_2aoldS1823);
  }
  _M0L4selfS575->$5 = _M0L6_2atmpS1498;
  _M0L4selfS575->$6 = -1;
  _M0L1xS577 = _M0L9old__headS574;
  while (1) {
    if (_M0L1xS577 == 0) {
      if (_M0L1xS577) {
        moonbit_decref(_M0L1xS577);
      }
      moonbit_decref(_M0L4selfS575);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS578 =
        _M0L1xS577;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS579 =
        _M0L7_2aSomeS578;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS580 =
        _M0L4_2axS579->$1;
      moonbit_string_t _M0L6_2akeyS581 = _M0L4_2axS579->$4;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS582 =
        _M0L4_2axS579->$5;
      int32_t _M0L7_2ahashS583 = _M0L4_2axS579->$3;
      int32_t _M0L6_2acntS1946 = Moonbit_object_header(_M0L4_2axS579)->rc;
      if (_M0L6_2acntS1946 > 1) {
        int32_t _M0L11_2anew__cntS1947 = _M0L6_2acntS1946 - 1;
        Moonbit_object_header(_M0L4_2axS579)->rc = _M0L11_2anew__cntS1947;
        moonbit_incref(_M0L8_2avalueS582);
        moonbit_incref(_M0L6_2akeyS581);
        if (_M0L7_2anextS580) {
          moonbit_incref(_M0L7_2anextS580);
        }
      } else if (_M0L6_2acntS1946 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS579);
      }
      moonbit_incref(_M0L4selfS575);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS575, _M0L6_2akeyS581, _M0L8_2avalueS582, _M0L7_2ahashS583);
      _M0L1xS577 = _M0L7_2anextS580;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS586
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS585;
  int32_t _M0L8capacityS1506;
  int32_t _M0L13new__capacityS587;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1501;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1500;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS1829;
  int32_t _M0L6_2atmpS1502;
  int32_t _M0L8capacityS1504;
  int32_t _M0L6_2atmpS1503;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1505;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1828;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L1xS588;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS585 = _M0L4selfS586->$5;
  _M0L8capacityS1506 = _M0L4selfS586->$2;
  _M0L13new__capacityS587 = _M0L8capacityS1506 << 1;
  _M0L6_2atmpS1501 = 0;
  _M0L6_2atmpS1500
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS587, _M0L6_2atmpS1501);
  _M0L6_2aoldS1829 = _M0L4selfS586->$0;
  if (_M0L9old__headS585) {
    moonbit_incref(_M0L9old__headS585);
  }
  moonbit_decref(_M0L6_2aoldS1829);
  _M0L4selfS586->$0 = _M0L6_2atmpS1500;
  _M0L4selfS586->$2 = _M0L13new__capacityS587;
  _M0L6_2atmpS1502 = _M0L13new__capacityS587 - 1;
  _M0L4selfS586->$3 = _M0L6_2atmpS1502;
  _M0L8capacityS1504 = _M0L4selfS586->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1503 = _M0FPB21calc__grow__threshold(_M0L8capacityS1504);
  _M0L4selfS586->$4 = _M0L6_2atmpS1503;
  _M0L4selfS586->$1 = 0;
  _M0L6_2atmpS1505 = 0;
  _M0L6_2aoldS1828 = _M0L4selfS586->$5;
  if (_M0L6_2aoldS1828) {
    moonbit_decref(_M0L6_2aoldS1828);
  }
  _M0L4selfS586->$5 = _M0L6_2atmpS1505;
  _M0L4selfS586->$6 = -1;
  _M0L1xS588 = _M0L9old__headS585;
  while (1) {
    if (_M0L1xS588 == 0) {
      if (_M0L1xS588) {
        moonbit_decref(_M0L1xS588);
      }
      moonbit_decref(_M0L4selfS586);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS589 =
        _M0L1xS588;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS590 =
        _M0L7_2aSomeS589;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS591 =
        _M0L4_2axS590->$1;
      int32_t _M0L6_2akeyS592 = _M0L4_2axS590->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS593 =
        _M0L4_2axS590->$5;
      int32_t _M0L7_2ahashS594 = _M0L4_2axS590->$3;
      int32_t _M0L6_2acntS1948 = Moonbit_object_header(_M0L4_2axS590)->rc;
      if (_M0L6_2acntS1948 > 1) {
        int32_t _M0L11_2anew__cntS1949 = _M0L6_2acntS1948 - 1;
        Moonbit_object_header(_M0L4_2axS590)->rc = _M0L11_2anew__cntS1949;
        moonbit_incref(_M0L8_2avalueS593);
        if (_M0L7_2anextS591) {
          moonbit_incref(_M0L7_2anextS591);
        }
      } else if (_M0L6_2acntS1948 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS590);
      }
      moonbit_incref(_M0L4selfS586);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS586, _M0L6_2akeyS592, _M0L8_2avalueS593, _M0L7_2ahashS594);
      _M0L1xS588 = _M0L7_2anextS591;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS545,
  moonbit_string_t _M0L3keyS551,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS552,
  int32_t _M0L4hashS547
) {
  int32_t _M0L14capacity__maskS1474;
  int32_t _M0L6_2atmpS1473;
  int32_t _M0L3pslS542;
  int32_t _M0L3idxS543;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1474 = _M0L4selfS545->$3;
  _M0L6_2atmpS1473 = _M0L4hashS547 & _M0L14capacity__maskS1474;
  _M0L3pslS542 = 0;
  _M0L3idxS543 = _M0L6_2atmpS1473;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1472 =
      _M0L4selfS545->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS544;
    if (
      _M0L3idxS543 < 0
      || _M0L3idxS543 >= Moonbit_array_length(_M0L7entriesS1472)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS544
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1472[
        _M0L3idxS543
      ];
    if (_M0L7_2abindS544 == 0) {
      int32_t _M0L4sizeS1457 = _M0L4selfS545->$1;
      int32_t _M0L8grow__atS1458 = _M0L4selfS545->$4;
      int32_t _M0L7_2abindS548;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS549;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS550;
      if (_M0L4sizeS1457 >= _M0L8grow__atS1458) {
        int32_t _M0L14capacity__maskS1460;
        int32_t _M0L6_2atmpS1459;
        moonbit_incref(_M0L4selfS545);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS545);
        _M0L14capacity__maskS1460 = _M0L4selfS545->$3;
        _M0L6_2atmpS1459 = _M0L4hashS547 & _M0L14capacity__maskS1460;
        _M0L3pslS542 = 0;
        _M0L3idxS543 = _M0L6_2atmpS1459;
        continue;
      }
      _M0L7_2abindS548 = _M0L4selfS545->$6;
      _M0L7_2abindS549 = 0;
      _M0L5entryS550
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS550)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS550->$0 = _M0L7_2abindS548;
      _M0L5entryS550->$1 = _M0L7_2abindS549;
      _M0L5entryS550->$2 = _M0L3pslS542;
      _M0L5entryS550->$3 = _M0L4hashS547;
      _M0L5entryS550->$4 = _M0L3keyS551;
      _M0L5entryS550->$5 = _M0L5valueS552;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS545, _M0L3idxS543, _M0L5entryS550);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS553 =
        _M0L7_2abindS544;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS554 =
        _M0L7_2aSomeS553;
      int32_t _M0L4hashS1462 = _M0L14_2acurr__entryS554->$3;
      int32_t _if__result_2037;
      int32_t _M0L3pslS1463;
      int32_t _M0L6_2atmpS1468;
      int32_t _M0L6_2atmpS1470;
      int32_t _M0L14capacity__maskS1471;
      int32_t _M0L6_2atmpS1469;
      if (_M0L4hashS1462 == _M0L4hashS547) {
        moonbit_string_t _M0L3keyS1461 = _M0L14_2acurr__entryS554->$4;
        #line 134 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_2037
        = moonbit_val_array_equal(_M0L3keyS1461, _M0L3keyS551);
      } else {
        _if__result_2037 = 0;
      }
      if (_if__result_2037) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1831;
        moonbit_incref(_M0L14_2acurr__entryS554);
        moonbit_decref(_M0L3keyS551);
        moonbit_decref(_M0L4selfS545);
        _M0L6_2aoldS1831 = _M0L14_2acurr__entryS554->$5;
        moonbit_decref(_M0L6_2aoldS1831);
        _M0L14_2acurr__entryS554->$5 = _M0L5valueS552;
        moonbit_decref(_M0L14_2acurr__entryS554);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS554);
      }
      _M0L3pslS1463 = _M0L14_2acurr__entryS554->$2;
      if (_M0L3pslS542 > _M0L3pslS1463) {
        int32_t _M0L4sizeS1464 = _M0L4selfS545->$1;
        int32_t _M0L8grow__atS1465 = _M0L4selfS545->$4;
        int32_t _M0L7_2abindS555;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS556;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS557;
        if (_M0L4sizeS1464 >= _M0L8grow__atS1465) {
          int32_t _M0L14capacity__maskS1467;
          int32_t _M0L6_2atmpS1466;
          moonbit_decref(_M0L14_2acurr__entryS554);
          moonbit_incref(_M0L4selfS545);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS545);
          _M0L14capacity__maskS1467 = _M0L4selfS545->$3;
          _M0L6_2atmpS1466 = _M0L4hashS547 & _M0L14capacity__maskS1467;
          _M0L3pslS542 = 0;
          _M0L3idxS543 = _M0L6_2atmpS1466;
          continue;
        }
        moonbit_incref(_M0L4selfS545);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS545, _M0L3idxS543, _M0L14_2acurr__entryS554);
        _M0L7_2abindS555 = _M0L4selfS545->$6;
        _M0L7_2abindS556 = 0;
        _M0L5entryS557
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS557)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS557->$0 = _M0L7_2abindS555;
        _M0L5entryS557->$1 = _M0L7_2abindS556;
        _M0L5entryS557->$2 = _M0L3pslS542;
        _M0L5entryS557->$3 = _M0L4hashS547;
        _M0L5entryS557->$4 = _M0L3keyS551;
        _M0L5entryS557->$5 = _M0L5valueS552;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS545, _M0L3idxS543, _M0L5entryS557);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS554);
      }
      _M0L6_2atmpS1468 = _M0L3pslS542 + 1;
      _M0L6_2atmpS1470 = _M0L3idxS543 + 1;
      _M0L14capacity__maskS1471 = _M0L4selfS545->$3;
      _M0L6_2atmpS1469 = _M0L6_2atmpS1470 & _M0L14capacity__maskS1471;
      _M0L3pslS542 = _M0L6_2atmpS1468;
      _M0L3idxS543 = _M0L6_2atmpS1469;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS561,
  int32_t _M0L3keyS567,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS568,
  int32_t _M0L4hashS563
) {
  int32_t _M0L14capacity__maskS1492;
  int32_t _M0L6_2atmpS1491;
  int32_t _M0L3pslS558;
  int32_t _M0L3idxS559;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1492 = _M0L4selfS561->$3;
  _M0L6_2atmpS1491 = _M0L4hashS563 & _M0L14capacity__maskS1492;
  _M0L3pslS558 = 0;
  _M0L3idxS559 = _M0L6_2atmpS1491;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1490 =
      _M0L4selfS561->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS560;
    if (
      _M0L3idxS559 < 0
      || _M0L3idxS559 >= Moonbit_array_length(_M0L7entriesS1490)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS560
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1490[
        _M0L3idxS559
      ];
    if (_M0L7_2abindS560 == 0) {
      int32_t _M0L4sizeS1475 = _M0L4selfS561->$1;
      int32_t _M0L8grow__atS1476 = _M0L4selfS561->$4;
      int32_t _M0L7_2abindS564;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS565;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS566;
      if (_M0L4sizeS1475 >= _M0L8grow__atS1476) {
        int32_t _M0L14capacity__maskS1478;
        int32_t _M0L6_2atmpS1477;
        moonbit_incref(_M0L4selfS561);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS561);
        _M0L14capacity__maskS1478 = _M0L4selfS561->$3;
        _M0L6_2atmpS1477 = _M0L4hashS563 & _M0L14capacity__maskS1478;
        _M0L3pslS558 = 0;
        _M0L3idxS559 = _M0L6_2atmpS1477;
        continue;
      }
      _M0L7_2abindS564 = _M0L4selfS561->$6;
      _M0L7_2abindS565 = 0;
      _M0L5entryS566
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS566)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS566->$0 = _M0L7_2abindS564;
      _M0L5entryS566->$1 = _M0L7_2abindS565;
      _M0L5entryS566->$2 = _M0L3pslS558;
      _M0L5entryS566->$3 = _M0L4hashS563;
      _M0L5entryS566->$4 = _M0L3keyS567;
      _M0L5entryS566->$5 = _M0L5valueS568;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS561, _M0L3idxS559, _M0L5entryS566);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS569 =
        _M0L7_2abindS560;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS570 =
        _M0L7_2aSomeS569;
      int32_t _M0L4hashS1480 = _M0L14_2acurr__entryS570->$3;
      int32_t _if__result_2039;
      int32_t _M0L3pslS1481;
      int32_t _M0L6_2atmpS1486;
      int32_t _M0L6_2atmpS1488;
      int32_t _M0L14capacity__maskS1489;
      int32_t _M0L6_2atmpS1487;
      if (_M0L4hashS1480 == _M0L4hashS563) {
        int32_t _M0L3keyS1479 = _M0L14_2acurr__entryS570->$4;
        _if__result_2039 = _M0L3keyS1479 == _M0L3keyS567;
      } else {
        _if__result_2039 = 0;
      }
      if (_if__result_2039) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS1835;
        moonbit_incref(_M0L14_2acurr__entryS570);
        moonbit_decref(_M0L4selfS561);
        _M0L6_2aoldS1835 = _M0L14_2acurr__entryS570->$5;
        moonbit_decref(_M0L6_2aoldS1835);
        _M0L14_2acurr__entryS570->$5 = _M0L5valueS568;
        moonbit_decref(_M0L14_2acurr__entryS570);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS570);
      }
      _M0L3pslS1481 = _M0L14_2acurr__entryS570->$2;
      if (_M0L3pslS558 > _M0L3pslS1481) {
        int32_t _M0L4sizeS1482 = _M0L4selfS561->$1;
        int32_t _M0L8grow__atS1483 = _M0L4selfS561->$4;
        int32_t _M0L7_2abindS571;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS572;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS573;
        if (_M0L4sizeS1482 >= _M0L8grow__atS1483) {
          int32_t _M0L14capacity__maskS1485;
          int32_t _M0L6_2atmpS1484;
          moonbit_decref(_M0L14_2acurr__entryS570);
          moonbit_incref(_M0L4selfS561);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS561);
          _M0L14capacity__maskS1485 = _M0L4selfS561->$3;
          _M0L6_2atmpS1484 = _M0L4hashS563 & _M0L14capacity__maskS1485;
          _M0L3pslS558 = 0;
          _M0L3idxS559 = _M0L6_2atmpS1484;
          continue;
        }
        moonbit_incref(_M0L4selfS561);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS561, _M0L3idxS559, _M0L14_2acurr__entryS570);
        _M0L7_2abindS571 = _M0L4selfS561->$6;
        _M0L7_2abindS572 = 0;
        _M0L5entryS573
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS573)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS573->$0 = _M0L7_2abindS571;
        _M0L5entryS573->$1 = _M0L7_2abindS572;
        _M0L5entryS573->$2 = _M0L3pslS558;
        _M0L5entryS573->$3 = _M0L4hashS563;
        _M0L5entryS573->$4 = _M0L3keyS567;
        _M0L5entryS573->$5 = _M0L5valueS568;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS561, _M0L3idxS559, _M0L5entryS573);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS570);
      }
      _M0L6_2atmpS1486 = _M0L3pslS558 + 1;
      _M0L6_2atmpS1488 = _M0L3idxS559 + 1;
      _M0L14capacity__maskS1489 = _M0L4selfS561->$3;
      _M0L6_2atmpS1487 = _M0L6_2atmpS1488 & _M0L14capacity__maskS1489;
      _M0L3pslS558 = _M0L6_2atmpS1486;
      _M0L3idxS559 = _M0L6_2atmpS1487;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS526,
  int32_t _M0L3idxS531,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS530
) {
  int32_t _M0L3pslS1440;
  int32_t _M0L6_2atmpS1436;
  int32_t _M0L6_2atmpS1438;
  int32_t _M0L14capacity__maskS1439;
  int32_t _M0L6_2atmpS1437;
  int32_t _M0L3pslS522;
  int32_t _M0L3idxS523;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS524;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1440 = _M0L5entryS530->$2;
  _M0L6_2atmpS1436 = _M0L3pslS1440 + 1;
  _M0L6_2atmpS1438 = _M0L3idxS531 + 1;
  _M0L14capacity__maskS1439 = _M0L4selfS526->$3;
  _M0L6_2atmpS1437 = _M0L6_2atmpS1438 & _M0L14capacity__maskS1439;
  _M0L3pslS522 = _M0L6_2atmpS1436;
  _M0L3idxS523 = _M0L6_2atmpS1437;
  _M0L5entryS524 = _M0L5entryS530;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1435 =
      _M0L4selfS526->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS525;
    if (
      _M0L3idxS523 < 0
      || _M0L3idxS523 >= Moonbit_array_length(_M0L7entriesS1435)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS525
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1435[
        _M0L3idxS523
      ];
    if (_M0L7_2abindS525 == 0) {
      _M0L5entryS524->$2 = _M0L3pslS522;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526, _M0L5entryS524, _M0L3idxS523);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS528 =
        _M0L7_2abindS525;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS529 =
        _M0L7_2aSomeS528;
      int32_t _M0L3pslS1425 = _M0L14_2acurr__entryS529->$2;
      if (_M0L3pslS522 > _M0L3pslS1425) {
        int32_t _M0L3pslS1430;
        int32_t _M0L6_2atmpS1426;
        int32_t _M0L6_2atmpS1428;
        int32_t _M0L14capacity__maskS1429;
        int32_t _M0L6_2atmpS1427;
        _M0L5entryS524->$2 = _M0L3pslS522;
        moonbit_incref(_M0L14_2acurr__entryS529);
        moonbit_incref(_M0L4selfS526);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS526, _M0L5entryS524, _M0L3idxS523);
        _M0L3pslS1430 = _M0L14_2acurr__entryS529->$2;
        _M0L6_2atmpS1426 = _M0L3pslS1430 + 1;
        _M0L6_2atmpS1428 = _M0L3idxS523 + 1;
        _M0L14capacity__maskS1429 = _M0L4selfS526->$3;
        _M0L6_2atmpS1427 = _M0L6_2atmpS1428 & _M0L14capacity__maskS1429;
        _M0L3pslS522 = _M0L6_2atmpS1426;
        _M0L3idxS523 = _M0L6_2atmpS1427;
        _M0L5entryS524 = _M0L14_2acurr__entryS529;
        continue;
      } else {
        int32_t _M0L6_2atmpS1431 = _M0L3pslS522 + 1;
        int32_t _M0L6_2atmpS1433 = _M0L3idxS523 + 1;
        int32_t _M0L14capacity__maskS1434 = _M0L4selfS526->$3;
        int32_t _M0L6_2atmpS1432 =
          _M0L6_2atmpS1433 & _M0L14capacity__maskS1434;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_2041 =
          _M0L5entryS524;
        _M0L3pslS522 = _M0L6_2atmpS1431;
        _M0L3idxS523 = _M0L6_2atmpS1432;
        _M0L5entryS524 = _tmp_2041;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS536,
  int32_t _M0L3idxS541,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS540
) {
  int32_t _M0L3pslS1456;
  int32_t _M0L6_2atmpS1452;
  int32_t _M0L6_2atmpS1454;
  int32_t _M0L14capacity__maskS1455;
  int32_t _M0L6_2atmpS1453;
  int32_t _M0L3pslS532;
  int32_t _M0L3idxS533;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS534;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1456 = _M0L5entryS540->$2;
  _M0L6_2atmpS1452 = _M0L3pslS1456 + 1;
  _M0L6_2atmpS1454 = _M0L3idxS541 + 1;
  _M0L14capacity__maskS1455 = _M0L4selfS536->$3;
  _M0L6_2atmpS1453 = _M0L6_2atmpS1454 & _M0L14capacity__maskS1455;
  _M0L3pslS532 = _M0L6_2atmpS1452;
  _M0L3idxS533 = _M0L6_2atmpS1453;
  _M0L5entryS534 = _M0L5entryS540;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1451 =
      _M0L4selfS536->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS535;
    if (
      _M0L3idxS533 < 0
      || _M0L3idxS533 >= Moonbit_array_length(_M0L7entriesS1451)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS535
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1451[
        _M0L3idxS533
      ];
    if (_M0L7_2abindS535 == 0) {
      _M0L5entryS534->$2 = _M0L3pslS532;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS536, _M0L5entryS534, _M0L3idxS533);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS538 =
        _M0L7_2abindS535;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS539 =
        _M0L7_2aSomeS538;
      int32_t _M0L3pslS1441 = _M0L14_2acurr__entryS539->$2;
      if (_M0L3pslS532 > _M0L3pslS1441) {
        int32_t _M0L3pslS1446;
        int32_t _M0L6_2atmpS1442;
        int32_t _M0L6_2atmpS1444;
        int32_t _M0L14capacity__maskS1445;
        int32_t _M0L6_2atmpS1443;
        _M0L5entryS534->$2 = _M0L3pslS532;
        moonbit_incref(_M0L14_2acurr__entryS539);
        moonbit_incref(_M0L4selfS536);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS536, _M0L5entryS534, _M0L3idxS533);
        _M0L3pslS1446 = _M0L14_2acurr__entryS539->$2;
        _M0L6_2atmpS1442 = _M0L3pslS1446 + 1;
        _M0L6_2atmpS1444 = _M0L3idxS533 + 1;
        _M0L14capacity__maskS1445 = _M0L4selfS536->$3;
        _M0L6_2atmpS1443 = _M0L6_2atmpS1444 & _M0L14capacity__maskS1445;
        _M0L3pslS532 = _M0L6_2atmpS1442;
        _M0L3idxS533 = _M0L6_2atmpS1443;
        _M0L5entryS534 = _M0L14_2acurr__entryS539;
        continue;
      } else {
        int32_t _M0L6_2atmpS1447 = _M0L3pslS532 + 1;
        int32_t _M0L6_2atmpS1449 = _M0L3idxS533 + 1;
        int32_t _M0L14capacity__maskS1450 = _M0L4selfS536->$3;
        int32_t _M0L6_2atmpS1448 =
          _M0L6_2atmpS1449 & _M0L14capacity__maskS1450;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_2043 =
          _M0L5entryS534;
        _M0L3pslS532 = _M0L6_2atmpS1447;
        _M0L3idxS533 = _M0L6_2atmpS1448;
        _M0L5entryS534 = _tmp_2043;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS510,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS512,
  int32_t _M0L8new__idxS511
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1421;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1422;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1843;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1842;
  int32_t _M0L6_2acntS1950;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS513;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1421 = _M0L4selfS510->$0;
  moonbit_incref(_M0L5entryS512);
  _M0L6_2atmpS1422 = _M0L5entryS512;
  if (
    _M0L8new__idxS511 < 0
    || _M0L8new__idxS511 >= Moonbit_array_length(_M0L7entriesS1421)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1843
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1421[
      _M0L8new__idxS511
    ];
  if (_M0L6_2aoldS1843) {
    moonbit_decref(_M0L6_2aoldS1843);
  }
  _M0L7entriesS1421[_M0L8new__idxS511] = _M0L6_2atmpS1422;
  _M0L8_2afieldS1842 = _M0L5entryS512->$1;
  _M0L6_2acntS1950 = Moonbit_object_header(_M0L5entryS512)->rc;
  if (_M0L6_2acntS1950 > 1) {
    int32_t _M0L11_2anew__cntS1953 = _M0L6_2acntS1950 - 1;
    Moonbit_object_header(_M0L5entryS512)->rc = _M0L11_2anew__cntS1953;
    if (_M0L8_2afieldS1842) {
      moonbit_incref(_M0L8_2afieldS1842);
    }
  } else if (_M0L6_2acntS1950 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1952 =
      _M0L5entryS512->$5;
    moonbit_string_t _M0L8_2afieldS1951;
    moonbit_decref(_M0L8_2afieldS1952);
    _M0L8_2afieldS1951 = _M0L5entryS512->$4;
    moonbit_decref(_M0L8_2afieldS1951);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS512);
  }
  _M0L7_2abindS513 = _M0L8_2afieldS1842;
  if (_M0L7_2abindS513 == 0) {
    if (_M0L7_2abindS513) {
      moonbit_decref(_M0L7_2abindS513);
    }
    _M0L4selfS510->$6 = _M0L8new__idxS511;
    moonbit_decref(_M0L4selfS510);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS514;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS515;
    moonbit_decref(_M0L4selfS510);
    _M0L7_2aSomeS514 = _M0L7_2abindS513;
    _M0L7_2anextS515 = _M0L7_2aSomeS514;
    _M0L7_2anextS515->$0 = _M0L8new__idxS511;
    moonbit_decref(_M0L7_2anextS515);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS516,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS518,
  int32_t _M0L8new__idxS517
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1423;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1424;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1846;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1845;
  int32_t _M0L6_2acntS1954;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS519;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1423 = _M0L4selfS516->$0;
  moonbit_incref(_M0L5entryS518);
  _M0L6_2atmpS1424 = _M0L5entryS518;
  if (
    _M0L8new__idxS517 < 0
    || _M0L8new__idxS517 >= Moonbit_array_length(_M0L7entriesS1423)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1846
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1423[
      _M0L8new__idxS517
    ];
  if (_M0L6_2aoldS1846) {
    moonbit_decref(_M0L6_2aoldS1846);
  }
  _M0L7entriesS1423[_M0L8new__idxS517] = _M0L6_2atmpS1424;
  _M0L8_2afieldS1845 = _M0L5entryS518->$1;
  _M0L6_2acntS1954 = Moonbit_object_header(_M0L5entryS518)->rc;
  if (_M0L6_2acntS1954 > 1) {
    int32_t _M0L11_2anew__cntS1956 = _M0L6_2acntS1954 - 1;
    Moonbit_object_header(_M0L5entryS518)->rc = _M0L11_2anew__cntS1956;
    if (_M0L8_2afieldS1845) {
      moonbit_incref(_M0L8_2afieldS1845);
    }
  } else if (_M0L6_2acntS1954 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1955 =
      _M0L5entryS518->$5;
    moonbit_decref(_M0L8_2afieldS1955);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS518);
  }
  _M0L7_2abindS519 = _M0L8_2afieldS1845;
  if (_M0L7_2abindS519 == 0) {
    if (_M0L7_2abindS519) {
      moonbit_decref(_M0L7_2abindS519);
    }
    _M0L4selfS516->$6 = _M0L8new__idxS517;
    moonbit_decref(_M0L4selfS516);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS520;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS521;
    moonbit_decref(_M0L4selfS516);
    _M0L7_2aSomeS520 = _M0L7_2abindS519;
    _M0L7_2anextS521 = _M0L7_2aSomeS520;
    _M0L7_2anextS521->$0 = _M0L8new__idxS517;
    moonbit_decref(_M0L7_2anextS521);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS503,
  int32_t _M0L3idxS505,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS504
) {
  int32_t _M0L7_2abindS502;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1408;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1409;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1848;
  int32_t _M0L4sizeS1411;
  int32_t _M0L6_2atmpS1410;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS502 = _M0L4selfS503->$6;
  switch (_M0L7_2abindS502) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1403;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1850;
      moonbit_incref(_M0L5entryS504);
      _M0L6_2atmpS1403 = _M0L5entryS504;
      _M0L6_2aoldS1850 = _M0L4selfS503->$5;
      if (_M0L6_2aoldS1850) {
        moonbit_decref(_M0L6_2aoldS1850);
      }
      _M0L4selfS503->$5 = _M0L6_2atmpS1403;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1407 =
        _M0L4selfS503->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1406;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1404;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1405;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1851;
      if (
        _M0L7_2abindS502 < 0
        || _M0L7_2abindS502 >= Moonbit_array_length(_M0L7entriesS1407)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1406
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1407[
          _M0L7_2abindS502
        ];
      if (_M0L6_2atmpS1406) {
        moonbit_incref(_M0L6_2atmpS1406);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1404
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1406);
      moonbit_incref(_M0L5entryS504);
      _M0L6_2atmpS1405 = _M0L5entryS504;
      _M0L6_2aoldS1851 = _M0L6_2atmpS1404->$1;
      if (_M0L6_2aoldS1851) {
        moonbit_decref(_M0L6_2aoldS1851);
      }
      _M0L6_2atmpS1404->$1 = _M0L6_2atmpS1405;
      moonbit_decref(_M0L6_2atmpS1404);
      break;
    }
  }
  _M0L4selfS503->$6 = _M0L3idxS505;
  _M0L7entriesS1408 = _M0L4selfS503->$0;
  _M0L6_2atmpS1409 = _M0L5entryS504;
  if (
    _M0L3idxS505 < 0
    || _M0L3idxS505 >= Moonbit_array_length(_M0L7entriesS1408)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1848
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1408[
      _M0L3idxS505
    ];
  if (_M0L6_2aoldS1848) {
    moonbit_decref(_M0L6_2aoldS1848);
  }
  _M0L7entriesS1408[_M0L3idxS505] = _M0L6_2atmpS1409;
  _M0L4sizeS1411 = _M0L4selfS503->$1;
  _M0L6_2atmpS1410 = _M0L4sizeS1411 + 1;
  _M0L4selfS503->$1 = _M0L6_2atmpS1410;
  moonbit_decref(_M0L4selfS503);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS507,
  int32_t _M0L3idxS509,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS508
) {
  int32_t _M0L7_2abindS506;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1417;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1418;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1854;
  int32_t _M0L4sizeS1420;
  int32_t _M0L6_2atmpS1419;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS506 = _M0L4selfS507->$6;
  switch (_M0L7_2abindS506) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1412;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1856;
      moonbit_incref(_M0L5entryS508);
      _M0L6_2atmpS1412 = _M0L5entryS508;
      _M0L6_2aoldS1856 = _M0L4selfS507->$5;
      if (_M0L6_2aoldS1856) {
        moonbit_decref(_M0L6_2aoldS1856);
      }
      _M0L4selfS507->$5 = _M0L6_2atmpS1412;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1416 =
        _M0L4selfS507->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1415;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1413;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1414;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1857;
      if (
        _M0L7_2abindS506 < 0
        || _M0L7_2abindS506 >= Moonbit_array_length(_M0L7entriesS1416)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1415
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1416[
          _M0L7_2abindS506
        ];
      if (_M0L6_2atmpS1415) {
        moonbit_incref(_M0L6_2atmpS1415);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1413
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1415);
      moonbit_incref(_M0L5entryS508);
      _M0L6_2atmpS1414 = _M0L5entryS508;
      _M0L6_2aoldS1857 = _M0L6_2atmpS1413->$1;
      if (_M0L6_2aoldS1857) {
        moonbit_decref(_M0L6_2aoldS1857);
      }
      _M0L6_2atmpS1413->$1 = _M0L6_2atmpS1414;
      moonbit_decref(_M0L6_2atmpS1413);
      break;
    }
  }
  _M0L4selfS507->$6 = _M0L3idxS509;
  _M0L7entriesS1417 = _M0L4selfS507->$0;
  _M0L6_2atmpS1418 = _M0L5entryS508;
  if (
    _M0L3idxS509 < 0
    || _M0L3idxS509 >= Moonbit_array_length(_M0L7entriesS1417)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1854
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1417[
      _M0L3idxS509
    ];
  if (_M0L6_2aoldS1854) {
    moonbit_decref(_M0L6_2aoldS1854);
  }
  _M0L7entriesS1417[_M0L3idxS509] = _M0L6_2atmpS1418;
  _M0L4sizeS1420 = _M0L4selfS507->$1;
  _M0L6_2atmpS1419 = _M0L4sizeS1420 + 1;
  _M0L4selfS507->$1 = _M0L6_2atmpS1419;
  moonbit_decref(_M0L4selfS507);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS491
) {
  int32_t _M0L8capacityS490;
  int32_t _M0L7_2abindS492;
  int32_t _M0L7_2abindS493;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1401;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS494;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS495;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_2044;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS490
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS491);
  _M0L7_2abindS492 = _M0L8capacityS490 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS493 = _M0FPB21calc__grow__threshold(_M0L8capacityS490);
  _M0L6_2atmpS1401 = 0;
  _M0L7_2abindS494
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS490, _M0L6_2atmpS1401);
  _M0L7_2abindS495 = 0;
  _block_2044
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_2044)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_2044->$0 = _M0L7_2abindS494;
  _block_2044->$1 = 0;
  _block_2044->$2 = _M0L8capacityS490;
  _block_2044->$3 = _M0L7_2abindS492;
  _block_2044->$4 = _M0L7_2abindS493;
  _block_2044->$5 = _M0L7_2abindS495;
  _block_2044->$6 = -1;
  return _block_2044;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS497
) {
  int32_t _M0L8capacityS496;
  int32_t _M0L7_2abindS498;
  int32_t _M0L7_2abindS499;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1402;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS500;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS501;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_2045;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS496
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS497);
  _M0L7_2abindS498 = _M0L8capacityS496 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS499 = _M0FPB21calc__grow__threshold(_M0L8capacityS496);
  _M0L6_2atmpS1402 = 0;
  _M0L7_2abindS500
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS496, _M0L6_2atmpS1402);
  _M0L7_2abindS501 = 0;
  _block_2045
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_2045)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_2045->$0 = _M0L7_2abindS500;
  _block_2045->$1 = 0;
  _block_2045->$2 = _M0L8capacityS496;
  _block_2045->$3 = _M0L7_2abindS498;
  _block_2045->$4 = _M0L7_2abindS499;
  _block_2045->$5 = _M0L7_2abindS501;
  _block_2045->$6 = -1;
  return _block_2045;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS489) {
  #line 33 "/Users/user/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS489 >= 0) {
    int32_t _M0L6_2atmpS1400;
    int32_t _M0L6_2atmpS1399;
    int32_t _M0L6_2atmpS1398;
    int32_t _M0L6_2atmpS1397;
    if (_M0L4selfS489 <= 1) {
      return 1;
    }
    if (_M0L4selfS489 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1400 = _M0L4selfS489 - 1;
    #line 44 "/Users/user/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS1399 = moonbit_clz32(_M0L6_2atmpS1400);
    _M0L6_2atmpS1398 = _M0L6_2atmpS1399 - 1;
    _M0L6_2atmpS1397 = 2147483647 >> (_M0L6_2atmpS1398 & 31);
    return _M0L6_2atmpS1397 + 1;
  } else {
    #line 34 "/Users/user/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS488) {
  int32_t _M0L6_2atmpS1396;
  #line 510 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1396 = _M0L8capacityS488 * 13;
  return _M0L6_2atmpS1396 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS484
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS484 == 0) {
    if (_M0L4selfS484) {
      moonbit_decref(_M0L4selfS484);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS485 =
      _M0L4selfS484;
    return _M0L7_2aSomeS485;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS486
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS486 == 0) {
    if (_M0L4selfS486) {
      moonbit_decref(_M0L4selfS486);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS487 =
      _M0L4selfS486;
    return _M0L7_2aSomeS487;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS483
) {
  moonbit_string_t* _M0L6_2atmpS1395;
  #line 165 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS1395 = _M0L4selfS483;
  #line 167 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1395);
}

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS482,
  struct _M0TPB6Logger _M0L6loggerS481
) {
  struct _M0TWEOs* _M0L6_2atmpS1394;
  #line 304 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 305 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1394 = _M0MPC15array5Array4iterGsE(_M0L4selfS482);
  #line 305 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPB6Logger19write__iter_2einnerGsE(_M0L6loggerS481, _M0L6_2atmpS1394, (moonbit_string_t)moonbit_string_literal_32.data, (moonbit_string_t)moonbit_string_literal_33.data, (moonbit_string_t)moonbit_string_literal_34.data, 0);
  return 0;
}

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS480
) {
  moonbit_string_t* _M0L3bufS1392;
  int32_t _M0L3lenS1393;
  int32_t _M0L6_2acntS1957;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1391;
  #line 1656 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS1392 = _M0L4selfS480->$0;
  _M0L3lenS1393 = _M0L4selfS480->$1;
  _M0L6_2acntS1957 = Moonbit_object_header(_M0L4selfS480)->rc;
  if (_M0L6_2acntS1957 > 1) {
    int32_t _M0L11_2anew__cntS1958 = _M0L6_2acntS1957 - 1;
    Moonbit_object_header(_M0L4selfS480)->rc = _M0L11_2anew__cntS1958;
    moonbit_incref(_M0L3bufS1392);
  } else if (_M0L6_2acntS1957 == 1) {
    #line 1658 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS480);
  }
  _M0L6_2atmpS1391
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1393, _M0L3bufS1392
  };
  #line 1658 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1391);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS479
) {
  moonbit_string_t* _M0L6_2atmpS1389;
  int32_t _M0L6_2atmpS1390;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1388;
  #line 1509 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS479);
  _M0L6_2atmpS1389 = _M0L4selfS479;
  _M0L6_2atmpS1390 = Moonbit_array_length(_M0L4selfS479);
  moonbit_decref(_M0L4selfS479);
  _M0L6_2atmpS1388
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1390, _M0L6_2atmpS1389
  };
  #line 1511 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1388);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS477
) {
  struct _M0TPB8MutLocalGiE* _M0L1iS476;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__* _closure_2046;
  struct _M0TWEOs* _M0L6_2atmpS1376;
  #line 677 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS476
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS476)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS476->$0 = 0;
  _closure_2046
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__));
  Moonbit_object_header(_closure_2046)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__, $0_0) >> 2, 2, 0);
  _closure_2046->code = &_M0MPC15array9ArrayView4iterGsEC1377l680;
  _closure_2046->$0_0 = _M0L4selfS477.$0;
  _closure_2046->$0_1 = _M0L4selfS477.$1;
  _closure_2046->$0_2 = _M0L4selfS477.$2;
  _closure_2046->$1 = _M0L1iS476;
  _M0L6_2atmpS1376 = (struct _M0TWEOs*)_closure_2046;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1376);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1377l680(
  struct _M0TWEOs* _M0L6_2aenvS1378
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__* _M0L14_2acasted__envS1379;
  struct _M0TPB8MutLocalGiE* _M0L1iS476;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS1863;
  int32_t _M0L6_2acntS1959;
  struct _M0TPB9ArrayViewGsE _M0L4selfS477;
  int32_t _M0L3valS1380;
  int32_t _M0L6_2atmpS1381;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1379
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1377__l680__*)_M0L6_2aenvS1378;
  _M0L1iS476 = _M0L14_2acasted__envS1379->$1;
  _M0L8_2afieldS1863
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1379->$0_1,
      _M0L14_2acasted__envS1379->$0_2,
      _M0L14_2acasted__envS1379->$0_0
  };
  _M0L6_2acntS1959 = Moonbit_object_header(_M0L14_2acasted__envS1379)->rc;
  if (_M0L6_2acntS1959 > 1) {
    int32_t _M0L11_2anew__cntS1960 = _M0L6_2acntS1959 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1379)->rc
    = _M0L11_2anew__cntS1960;
    moonbit_incref(_M0L1iS476);
    moonbit_incref(_M0L8_2afieldS1863.$0);
  } else if (_M0L6_2acntS1959 == 1) {
    #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1379);
  }
  _M0L4selfS477 = _M0L8_2afieldS1863;
  _M0L3valS1380 = _M0L1iS476->$0;
  moonbit_incref(_M0L4selfS477.$0);
  #line 681 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1381 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS477);
  if (_M0L3valS1380 < _M0L6_2atmpS1381) {
    moonbit_string_t* _M0L3bufS1384 = _M0L4selfS477.$0;
    int32_t _M0L5startS1386 = _M0L4selfS477.$1;
    int32_t _M0L3valS1387 = _M0L1iS476->$0;
    int32_t _M0L6_2atmpS1385 = _M0L5startS1386 + _M0L3valS1387;
    moonbit_string_t _M0L6_2atmpS1861 =
      (moonbit_string_t)_M0L3bufS1384[_M0L6_2atmpS1385];
    moonbit_string_t _M0L4elemS478;
    int32_t _M0L3valS1383;
    int32_t _M0L6_2atmpS1382;
    moonbit_incref(_M0L6_2atmpS1861);
    moonbit_decref(_M0L3bufS1384);
    _M0L4elemS478 = _M0L6_2atmpS1861;
    _M0L3valS1383 = _M0L1iS476->$0;
    _M0L6_2atmpS1382 = _M0L3valS1383 + 1;
    _M0L1iS476->$0 = _M0L6_2atmpS1382;
    moonbit_decref(_M0L1iS476);
    return _M0L4elemS478;
  } else {
    moonbit_decref(_M0L4selfS477.$0);
    moonbit_decref(_M0L1iS476);
    return 0;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS474,
  struct _M0TPB6Logger _M0L6loggerS475
) {
  int32_t _M0L6_2atmpS1375;
  struct _M0TPC16string10StringView _M0L6_2atmpS1374;
  #line 244 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1375 = Moonbit_array_length(_M0L4selfS474);
  _M0L6_2atmpS1374
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1375, _M0L4selfS474
  };
  #line 245 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1374, _M0L6loggerS475, 1);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS473) {
  #line 45 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 46 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS473, 10);
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS467,
  moonbit_string_t _M0L5valueS469
) {
  int32_t _M0L3lenS1364;
  moonbit_string_t* _M0L6_2atmpS1366;
  int32_t _M0L6_2atmpS1365;
  int32_t _M0L6lengthS468;
  moonbit_string_t* _M0L3bufS1367;
  moonbit_string_t _M0L6_2aoldS1865;
  int32_t _M0L6_2atmpS1368;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1364 = _M0L4selfS467->$1;
  moonbit_incref(_M0L4selfS467);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1366 = _M0MPC15array5Array6bufferGsE(_M0L4selfS467);
  _M0L6_2atmpS1365 = Moonbit_array_length(_M0L6_2atmpS1366);
  moonbit_decref(_M0L6_2atmpS1366);
  if (_M0L3lenS1364 == _M0L6_2atmpS1365) {
    moonbit_incref(_M0L4selfS467);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS467);
  }
  _M0L6lengthS468 = _M0L4selfS467->$1;
  _M0L3bufS1367 = _M0L4selfS467->$0;
  _M0L6_2aoldS1865 = (moonbit_string_t)_M0L3bufS1367[_M0L6lengthS468];
  moonbit_decref(_M0L6_2aoldS1865);
  _M0L3bufS1367[_M0L6lengthS468] = _M0L5valueS469;
  _M0L6_2atmpS1368 = _M0L6lengthS468 + 1;
  _M0L4selfS467->$1 = _M0L6_2atmpS1368;
  moonbit_decref(_M0L4selfS467);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS470,
  struct _M0TUsiE* _M0L5valueS472
) {
  int32_t _M0L3lenS1369;
  struct _M0TUsiE** _M0L6_2atmpS1371;
  int32_t _M0L6_2atmpS1370;
  int32_t _M0L6lengthS471;
  struct _M0TUsiE** _M0L3bufS1372;
  struct _M0TUsiE* _M0L6_2aoldS1867;
  int32_t _M0L6_2atmpS1373;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1369 = _M0L4selfS470->$1;
  moonbit_incref(_M0L4selfS470);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1371 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS470);
  _M0L6_2atmpS1370 = Moonbit_array_length(_M0L6_2atmpS1371);
  moonbit_decref(_M0L6_2atmpS1371);
  if (_M0L3lenS1369 == _M0L6_2atmpS1370) {
    moonbit_incref(_M0L4selfS470);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS470);
  }
  _M0L6lengthS471 = _M0L4selfS470->$1;
  _M0L3bufS1372 = _M0L4selfS470->$0;
  _M0L6_2aoldS1867 = (struct _M0TUsiE*)_M0L3bufS1372[_M0L6lengthS471];
  if (_M0L6_2aoldS1867) {
    moonbit_decref(_M0L6_2aoldS1867);
  }
  _M0L3bufS1372[_M0L6lengthS471] = _M0L5valueS472;
  _M0L6_2atmpS1373 = _M0L6lengthS471 + 1;
  _M0L4selfS470->$1 = _M0L6_2atmpS1373;
  moonbit_decref(_M0L4selfS470);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS462) {
  int32_t _M0L8old__capS461;
  int32_t _M0L8new__capS463;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS461 = _M0L4selfS462->$1;
  if (_M0L8old__capS461 == 0) {
    _M0L8new__capS463 = 8;
  } else {
    _M0L8new__capS463 = _M0L8old__capS461 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS462, _M0L8new__capS463);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS465
) {
  int32_t _M0L8old__capS464;
  int32_t _M0L8new__capS466;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS464 = _M0L4selfS465->$1;
  if (_M0L8old__capS464 == 0) {
    _M0L8new__capS466 = 8;
  } else {
    _M0L8new__capS466 = _M0L8old__capS464 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS465, _M0L8new__capS466);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS452,
  int32_t _M0L13new__capacityS450
) {
  moonbit_string_t* _M0L8new__bufS449;
  moonbit_string_t* _M0L8old__bufS451;
  int32_t _M0L8old__capS453;
  int32_t _M0L9copy__lenS454;
  moonbit_string_t* _M0L6_2aoldS1869;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS449
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS450, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8old__bufS451 = _M0L4selfS452->$0;
  _M0L8old__capS453 = Moonbit_array_length(_M0L8old__bufS451);
  if (_M0L8old__capS453 < _M0L13new__capacityS450) {
    _M0L9copy__lenS454 = _M0L8old__capS453;
  } else {
    _M0L9copy__lenS454 = _M0L13new__capacityS450;
  }
  moonbit_incref(_M0L8old__bufS451);
  moonbit_incref(_M0L8new__bufS449);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS449, 0, _M0L8old__bufS451, 0, _M0L9copy__lenS454);
  _M0L6_2aoldS1869 = _M0L4selfS452->$0;
  moonbit_decref(_M0L6_2aoldS1869);
  _M0L4selfS452->$0 = _M0L8new__bufS449;
  moonbit_decref(_M0L4selfS452);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS458,
  int32_t _M0L13new__capacityS456
) {
  struct _M0TUsiE** _M0L8new__bufS455;
  struct _M0TUsiE** _M0L8old__bufS457;
  int32_t _M0L8old__capS459;
  int32_t _M0L9copy__lenS460;
  struct _M0TUsiE** _M0L6_2aoldS1871;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS455
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS456, 0);
  _M0L8old__bufS457 = _M0L4selfS458->$0;
  _M0L8old__capS459 = Moonbit_array_length(_M0L8old__bufS457);
  if (_M0L8old__capS459 < _M0L13new__capacityS456) {
    _M0L9copy__lenS460 = _M0L8old__capS459;
  } else {
    _M0L9copy__lenS460 = _M0L13new__capacityS456;
  }
  moonbit_incref(_M0L8old__bufS457);
  moonbit_incref(_M0L8new__bufS455);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS455, 0, _M0L8old__bufS457, 0, _M0L9copy__lenS460);
  _M0L6_2aoldS1871 = _M0L4selfS458->$0;
  moonbit_decref(_M0L6_2aoldS1871);
  _M0L4selfS458->$0 = _M0L8new__bufS455;
  moonbit_decref(_M0L4selfS458);
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS447
) {
  moonbit_string_t* _M0L8_2afieldS1873;
  int32_t _M0L6_2acntS1961;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS1873 = _M0L4selfS447->$0;
  _M0L6_2acntS1961 = Moonbit_object_header(_M0L4selfS447)->rc;
  if (_M0L6_2acntS1961 > 1) {
    int32_t _M0L11_2anew__cntS1962 = _M0L6_2acntS1961 - 1;
    Moonbit_object_header(_M0L4selfS447)->rc = _M0L11_2anew__cntS1962;
    moonbit_incref(_M0L8_2afieldS1873);
  } else if (_M0L6_2acntS1961 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS447);
  }
  return _M0L8_2afieldS1873;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS448
) {
  struct _M0TUsiE** _M0L8_2afieldS1874;
  int32_t _M0L6_2acntS1963;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS1874 = _M0L4selfS448->$0;
  _M0L6_2acntS1963 = Moonbit_object_header(_M0L4selfS448)->rc;
  if (_M0L6_2acntS1963 > 1) {
    int32_t _M0L11_2anew__cntS1964 = _M0L6_2acntS1963 - 1;
    Moonbit_object_header(_M0L4selfS448)->rc = _M0L11_2anew__cntS1964;
    moonbit_incref(_M0L8_2afieldS1874);
  } else if (_M0L6_2acntS1963 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS448);
  }
  return _M0L8_2afieldS1874;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS446
) {
  #line 53 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS446 == 0) {
    moonbit_string_t* _M0L6_2atmpS1362 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2047 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2047)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2047->$0 = _M0L6_2atmpS1362;
    _block_2047->$1 = 0;
    return _block_2047;
  } else {
    moonbit_string_t* _M0L6_2atmpS1363 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS446, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2048 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2048)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2048->$0 = _M0L6_2atmpS1363;
    _block_2048->$1 = 0;
    return _block_2048;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS445
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS445;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS444,
  struct _M0TPC16string10StringView _M0L3strS443
) {
  int32_t _M0L8str__lenS442;
  int32_t _M0L3lenS1355;
  int32_t _M0L6_2atmpS1354;
  uint16_t* _M0L4dataS1356;
  int32_t _M0L3lenS1357;
  moonbit_string_t _M0L6_2atmpS1358;
  int32_t _M0L6_2atmpS1359;
  int32_t _M0L3lenS1361;
  int32_t _M0L6_2atmpS1360;
  #line 126 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  moonbit_incref(_M0L3strS443.$0);
  #line 130 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS442 = _M0MPC16string10StringView6length(_M0L3strS443);
  _M0L3lenS1355 = _M0L4selfS444->$1;
  _M0L6_2atmpS1354 = _M0L3lenS1355 + _M0L8str__lenS442;
  moonbit_incref(_M0L4selfS444);
  #line 131 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS444, _M0L6_2atmpS1354);
  _M0L4dataS1356 = _M0L4selfS444->$0;
  _M0L3lenS1357 = _M0L4selfS444->$1;
  moonbit_incref(_M0L4dataS1356);
  moonbit_incref(_M0L3strS443.$0);
  #line 134 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1358 = _M0MPC16string10StringView4data(_M0L3strS443);
  #line 135 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1359 = _M0MPC16string10StringView13start__offset(_M0L3strS443);
  #line 132 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1356, _M0L3lenS1357, _M0L6_2atmpS1358, _M0L6_2atmpS1359, _M0L8str__lenS442);
  _M0L3lenS1361 = _M0L4selfS444->$1;
  _M0L6_2atmpS1360 = _M0L3lenS1361 + _M0L8str__lenS442;
  _M0L4selfS444->$1 = _M0L6_2atmpS1360;
  moonbit_decref(_M0L4selfS444);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS434,
  int32_t _M0L3lenS437,
  int32_t _M0L13start__offsetS441,
  int64_t _M0L11end__offsetS432
) {
  int32_t _M0L11end__offsetS431;
  int32_t _M0L5indexS435;
  int32_t _M0L5countS436;
  #line 441 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS432 == 4294967296ll) {
    _M0L11end__offsetS431 = Moonbit_array_length(_M0L4selfS434);
  } else {
    int64_t _M0L7_2aSomeS433 = _M0L11end__offsetS432;
    _M0L11end__offsetS431 = (int32_t)_M0L7_2aSomeS433;
  }
  _M0L5indexS435 = _M0L13start__offsetS441;
  _M0L5countS436 = 0;
  while (1) {
    int32_t _if__result_2050;
    if (_M0L5indexS435 < _M0L11end__offsetS431) {
      _if__result_2050 = _M0L5countS436 < _M0L3lenS437;
    } else {
      _if__result_2050 = 0;
    }
    if (_if__result_2050) {
      int32_t _M0L2c1S438 = _M0L4selfS434[_M0L5indexS435];
      int32_t _if__result_2051;
      int32_t _M0L6_2atmpS1352;
      int32_t _M0L6_2atmpS1353;
      #line 452 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S438)) {
        int32_t _M0L6_2atmpS1348 = _M0L5indexS435 + 1;
        _if__result_2051 = _M0L6_2atmpS1348 < _M0L11end__offsetS431;
      } else {
        _if__result_2051 = 0;
      }
      if (_if__result_2051) {
        int32_t _M0L6_2atmpS1351 = _M0L5indexS435 + 1;
        int32_t _M0L2c2S439 = _M0L4selfS434[_M0L6_2atmpS1351];
        #line 454 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S439)) {
          int32_t _M0L6_2atmpS1349 = _M0L5indexS435 + 2;
          int32_t _M0L6_2atmpS1350 = _M0L5countS436 + 1;
          _M0L5indexS435 = _M0L6_2atmpS1349;
          _M0L5countS436 = _M0L6_2atmpS1350;
          continue;
        } else {
          #line 457 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_35.data);
        }
      }
      _M0L6_2atmpS1352 = _M0L5indexS435 + 1;
      _M0L6_2atmpS1353 = _M0L5countS436 + 1;
      _M0L5indexS435 = _M0L6_2atmpS1352;
      _M0L5countS436 = _M0L6_2atmpS1353;
      continue;
    } else {
      moonbit_decref(_M0L4selfS434);
      return _M0L5countS436 >= _M0L3lenS437;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS428
) {
  int32_t _M0L3endS1342;
  int32_t _M0L5startS1343;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1342 = _M0L4selfS428.$2;
  _M0L5startS1343 = _M0L4selfS428.$1;
  moonbit_decref(_M0L4selfS428.$0);
  return _M0L3endS1342 - _M0L5startS1343;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS429
) {
  int32_t _M0L3endS1344;
  int32_t _M0L5startS1345;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1344 = _M0L4selfS429.$2;
  _M0L5startS1345 = _M0L4selfS429.$1;
  moonbit_decref(_M0L4selfS429.$0);
  return _M0L3endS1344 - _M0L5startS1345;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS430
) {
  int32_t _M0L3endS1346;
  int32_t _M0L5startS1347;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1346 = _M0L4selfS430.$2;
  _M0L5startS1347 = _M0L4selfS430.$1;
  moonbit_decref(_M0L4selfS430.$0);
  return _M0L3endS1346 - _M0L5startS1347;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS426,
  int64_t _M0L19start__offset_2eoptS424,
  int64_t _M0L11end__offsetS427
) {
  int32_t _M0L13start__offsetS423;
  if (_M0L19start__offset_2eoptS424 == 4294967296ll) {
    _M0L13start__offsetS423 = 0;
  } else {
    int64_t _M0L7_2aSomeS425 = _M0L19start__offset_2eoptS424;
    _M0L13start__offsetS423 = (int32_t)_M0L7_2aSomeS425;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS426, _M0L13start__offsetS423, _M0L11end__offsetS427);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS421,
  int32_t _M0L13start__offsetS422,
  int64_t _M0L11end__offsetS419
) {
  int32_t _M0L11end__offsetS418;
  int32_t _if__result_2052;
  #line 512 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS419 == 4294967296ll) {
    _M0L11end__offsetS418 = Moonbit_array_length(_M0L4selfS421);
  } else {
    int64_t _M0L7_2aSomeS420 = _M0L11end__offsetS419;
    _M0L11end__offsetS418 = (int32_t)_M0L7_2aSomeS420;
  }
  if (_M0L13start__offsetS422 >= 0) {
    if (_M0L13start__offsetS422 <= _M0L11end__offsetS418) {
      int32_t _M0L6_2atmpS1341 = Moonbit_array_length(_M0L4selfS421);
      _if__result_2052 = _M0L11end__offsetS418 <= _M0L6_2atmpS1341;
    } else {
      _if__result_2052 = 0;
    }
  } else {
    _if__result_2052 = 0;
  }
  if (_if__result_2052) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS422,
                                                 _M0L11end__offsetS418,
                                                 _M0L4selfS421};
  } else {
    moonbit_decref(_M0L4selfS421);
    #line 521 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_36.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS417
) {
  #line 197 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 198 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L4selfS417);
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS416
) {
  moonbit_string_t _M0L3strS1338;
  int32_t _M0L5startS1339;
  int32_t _M0L3endS1340;
  #line 190 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1338 = _M0L4selfS416.$0;
  _M0L5startS1339 = _M0L4selfS416.$1;
  _M0L3endS1340 = _M0L4selfS416.$2;
  #line 193 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1338, _M0L5startS1339, _M0L3endS1340);
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS413,
  int32_t _M0L5startS411,
  int32_t _M0L3endS412
) {
  int32_t _if__result_2053;
  int32_t _M0L3lenS414;
  int32_t _M0L6_2atmpS1336;
  int32_t _M0L6_2atmpS1337;
  moonbit_bytes_t _M0L5bytesS415;
  moonbit_bytes_t _M0L6_2atmpS1335;
  #line 91 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS411 == 0) {
    int32_t _M0L6_2atmpS1334 = Moonbit_array_length(_M0L3strS413);
    _if__result_2053 = _M0L3endS412 == _M0L6_2atmpS1334;
  } else {
    _if__result_2053 = 0;
  }
  if (_if__result_2053) {
    return _M0L3strS413;
  }
  _M0L3lenS414 = _M0L3endS412 - _M0L5startS411;
  _M0L6_2atmpS1336 = _M0L3lenS414 * 2;
  #line 101 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1337 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS415
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1336, _M0L6_2atmpS1337);
  moonbit_incref(_M0L5bytesS415);
  #line 102 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS415, 0, _M0L3strS413, _M0L5startS411, _M0L3lenS414);
  _M0L6_2atmpS1335 = _M0L5bytesS415;
  #line 103 "/Users/user/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1335, 0, 4294967296ll);
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS406,
  int32_t _M0L6offsetS410,
  int64_t _M0L6lengthS408
) {
  int32_t _M0L3lenS405;
  int32_t _M0L6lengthS407;
  int32_t _if__result_2054;
  #line 76 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS405 = Moonbit_array_length(_M0L4selfS406);
  if (_M0L6lengthS408 == 4294967296ll) {
    _M0L6lengthS407 = _M0L3lenS405 - _M0L6offsetS410;
  } else {
    int64_t _M0L7_2aSomeS409 = _M0L6lengthS408;
    _M0L6lengthS407 = (int32_t)_M0L7_2aSomeS409;
  }
  if (_M0L6offsetS410 >= 0) {
    if (_M0L6lengthS407 >= 0) {
      int32_t _M0L6_2atmpS1333 = _M0L6offsetS410 + _M0L6lengthS407;
      _if__result_2054 = _M0L6_2atmpS1333 <= _M0L3lenS405;
    } else {
      _if__result_2054 = 0;
    }
  } else {
    _if__result_2054 = 0;
  }
  if (_if__result_2054) {
    #line 84 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS406, _M0L6offsetS410, _M0L6lengthS407);
  } else {
    moonbit_decref(_M0L4selfS406);
    #line 83 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS397,
  int32_t _M0L13bytes__offsetS392,
  moonbit_string_t _M0L3strS399,
  int32_t _M0L11str__offsetS395,
  int32_t _M0L6lengthS393
) {
  int32_t _M0L6_2atmpS1332;
  int32_t _M0L6_2atmpS1331;
  int32_t _M0L2e1S391;
  int32_t _M0L6_2atmpS1330;
  int32_t _M0L2e2S394;
  int32_t _M0L4len1S396;
  int32_t _M0L4len2S398;
  int32_t _if__result_2055;
  #line 124 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1332 = _M0L6lengthS393 * 2;
  _M0L6_2atmpS1331 = _M0L13bytes__offsetS392 + _M0L6_2atmpS1332;
  _M0L2e1S391 = _M0L6_2atmpS1331 - 1;
  _M0L6_2atmpS1330 = _M0L11str__offsetS395 + _M0L6lengthS393;
  _M0L2e2S394 = _M0L6_2atmpS1330 - 1;
  _M0L4len1S396 = Moonbit_array_length(_M0L4selfS397);
  _M0L4len2S398 = Moonbit_array_length(_M0L3strS399);
  if (_M0L6lengthS393 >= 0) {
    if (_M0L13bytes__offsetS392 >= 0) {
      if (_M0L2e1S391 < _M0L4len1S396) {
        if (_M0L11str__offsetS395 >= 0) {
          _if__result_2055 = _M0L2e2S394 < _M0L4len2S398;
        } else {
          _if__result_2055 = 0;
        }
      } else {
        _if__result_2055 = 0;
      }
    } else {
      _if__result_2055 = 0;
    }
  } else {
    _if__result_2055 = 0;
  }
  if (_if__result_2055) {
    int32_t _M0L16end__str__offsetS400 =
      _M0L11str__offsetS395 + _M0L6lengthS393;
    int32_t _M0L1iS401 = _M0L11str__offsetS395;
    int32_t _M0L1jS402 = _M0L13bytes__offsetS392;
    while (1) {
      if (_M0L1iS401 < _M0L16end__str__offsetS400) {
        int32_t _M0L6_2atmpS1327 = _M0L3strS399[_M0L1iS401];
        int32_t _M0L6_2atmpS1326 = (int32_t)_M0L6_2atmpS1327;
        uint32_t _M0L1cS403 = *(uint32_t*)&_M0L6_2atmpS1326;
        uint32_t _M0L6_2atmpS1322 = _M0L1cS403 & 255u;
        int32_t _M0L6_2atmpS1321;
        int32_t _M0L6_2atmpS1323;
        uint32_t _M0L6_2atmpS1325;
        int32_t _M0L6_2atmpS1324;
        int32_t _M0L6_2atmpS1328;
        int32_t _M0L6_2atmpS1329;
        #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1321 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1322);
        if (
          _M0L1jS402 < 0 || _M0L1jS402 >= Moonbit_array_length(_M0L4selfS397)
        ) {
          #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS397[_M0L1jS402] = _M0L6_2atmpS1321;
        _M0L6_2atmpS1323 = _M0L1jS402 + 1;
        _M0L6_2atmpS1325 = _M0L1cS403 >> 8;
        #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1324 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1325);
        if (
          _M0L6_2atmpS1323 < 0
          || _M0L6_2atmpS1323 >= Moonbit_array_length(_M0L4selfS397)
        ) {
          #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS397[_M0L6_2atmpS1323] = _M0L6_2atmpS1324;
        _M0L6_2atmpS1328 = _M0L1iS401 + 1;
        _M0L6_2atmpS1329 = _M0L1jS402 + 2;
        _M0L1iS401 = _M0L6_2atmpS1328;
        _M0L1jS402 = _M0L6_2atmpS1329;
        continue;
      } else {
        moonbit_decref(_M0L3strS399);
        moonbit_decref(_M0L4selfS397);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS399);
    moonbit_decref(_M0L4selfS397);
    #line 137 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS390) {
  int32_t _M0L6_2atmpS1320;
  #line 2518 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1320 = *(int32_t*)&_M0L4selfS390;
  return _M0L6_2atmpS1320 & 0xff;
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS388,
  struct _M0TPB6Logger _M0L6loggerS389
) {
  #line 166 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 167 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L4selfS388, _M0L6loggerS389, 1);
  return 0;
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS387) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS387;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS371,
  int32_t _M0L5radixS370
) {
  int32_t _if__result_2057;
  int32_t _M0L12is__negativeS372;
  uint32_t _M0L3numS373;
  uint16_t* _M0L6bufferS374;
  #line 209 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS370 < 2) {
    _if__result_2057 = 1;
  } else {
    _if__result_2057 = _M0L5radixS370 > 36;
  }
  if (_if__result_2057) {
    #line 213 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_37.data);
  }
  if (_M0L4selfS371 == 0) {
    return (moonbit_string_t)moonbit_string_literal_38.data;
  }
  _M0L12is__negativeS372 = _M0L4selfS371 < 0;
  if (_M0L12is__negativeS372) {
    int32_t _M0L6_2atmpS1319 = -_M0L4selfS371;
    _M0L3numS373 = *(uint32_t*)&_M0L6_2atmpS1319;
  } else {
    _M0L3numS373 = *(uint32_t*)&_M0L4selfS371;
  }
  switch (_M0L5radixS370) {
    case 10: {
      int32_t _M0L10digit__lenS375;
      int32_t _M0L6_2atmpS1316;
      int32_t _M0L10total__lenS376;
      uint16_t* _M0L6bufferS377;
      int32_t _M0L12digit__startS378;
      #line 235 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS375 = _M0FPB12dec__count32(_M0L3numS373);
      if (_M0L12is__negativeS372) {
        _M0L6_2atmpS1316 = 1;
      } else {
        _M0L6_2atmpS1316 = 0;
      }
      _M0L10total__lenS376 = _M0L10digit__lenS375 + _M0L6_2atmpS1316;
      _M0L6bufferS377
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS376, 0);
      if (_M0L12is__negativeS372) {
        _M0L12digit__startS378 = 1;
      } else {
        _M0L12digit__startS378 = 0;
      }
      moonbit_incref(_M0L6bufferS377);
      #line 239 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS377, _M0L3numS373, _M0L12digit__startS378, _M0L10total__lenS376);
      _M0L6bufferS374 = _M0L6bufferS377;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS379;
      int32_t _M0L6_2atmpS1317;
      int32_t _M0L10total__lenS380;
      uint16_t* _M0L6bufferS381;
      int32_t _M0L12digit__startS382;
      #line 243 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS379 = _M0FPB12hex__count32(_M0L3numS373);
      if (_M0L12is__negativeS372) {
        _M0L6_2atmpS1317 = 1;
      } else {
        _M0L6_2atmpS1317 = 0;
      }
      _M0L10total__lenS380 = _M0L10digit__lenS379 + _M0L6_2atmpS1317;
      _M0L6bufferS381
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS380, 0);
      if (_M0L12is__negativeS372) {
        _M0L12digit__startS382 = 1;
      } else {
        _M0L12digit__startS382 = 0;
      }
      moonbit_incref(_M0L6bufferS381);
      #line 247 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS381, _M0L3numS373, _M0L12digit__startS382, _M0L10total__lenS380);
      _M0L6bufferS374 = _M0L6bufferS381;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS383;
      int32_t _M0L6_2atmpS1318;
      int32_t _M0L10total__lenS384;
      uint16_t* _M0L6bufferS385;
      int32_t _M0L12digit__startS386;
      #line 251 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS383
      = _M0FPB14radix__count32(_M0L3numS373, _M0L5radixS370);
      if (_M0L12is__negativeS372) {
        _M0L6_2atmpS1318 = 1;
      } else {
        _M0L6_2atmpS1318 = 0;
      }
      _M0L10total__lenS384 = _M0L10digit__lenS383 + _M0L6_2atmpS1318;
      _M0L6bufferS385
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS384, 0);
      if (_M0L12is__negativeS372) {
        _M0L12digit__startS386 = 1;
      } else {
        _M0L12digit__startS386 = 0;
      }
      moonbit_incref(_M0L6bufferS385);
      #line 255 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS385, _M0L3numS373, _M0L12digit__startS386, _M0L10total__lenS384, _M0L5radixS370);
      _M0L6bufferS374 = _M0L6bufferS385;
      break;
    }
  }
  if (_M0L12is__negativeS372) {
    _M0L6bufferS374[0] = 45;
  }
  return _M0L6bufferS374;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS364,
  int32_t _M0L5radixS366
) {
  uint32_t _M0L4baseS365;
  uint32_t _M0L3numS367;
  int32_t _M0L5countS368;
  #line 189 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS364 == 0u) {
    return 1;
  }
  _M0L4baseS365 = *(uint32_t*)&_M0L5radixS366;
  _M0L3numS367 = _M0L5valueS364;
  _M0L5countS368 = 0;
  while (1) {
    if (_M0L3numS367 > 0u) {
      uint32_t _M0L6_2atmpS1314 = _M0L3numS367 / _M0L4baseS365;
      int32_t _M0L6_2atmpS1315 = _M0L5countS368 + 1;
      _M0L3numS367 = _M0L6_2atmpS1314;
      _M0L5countS368 = _M0L6_2atmpS1315;
      continue;
    } else {
      return _M0L5countS368;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS362) {
  #line 177 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS362 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS363;
    int32_t _M0L6_2atmpS1313;
    int32_t _M0L6_2atmpS1312;
    #line 182 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS363 = moonbit_clz32(_M0L5valueS362);
    _M0L6_2atmpS1313 = 31 - _M0L14leading__zerosS363;
    _M0L6_2atmpS1312 = _M0L6_2atmpS1313 / 4;
    return _M0L6_2atmpS1312 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS361) {
  #line 143 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS361 >= 100000u) {
    if (_M0L5valueS361 >= 10000000u) {
      if (_M0L5valueS361 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS361 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS361 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS361 >= 1000u) {
    if (_M0L5valueS361 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS361 >= 100u) {
    return 3;
  } else if (_M0L5valueS361 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS347,
  uint32_t _M0L3numS359,
  int32_t _M0L12digit__startS348,
  int32_t _M0L10total__lenS360
) {
  int32_t _M0L6_2atmpS1311;
  uint32_t _M0L3numS337;
  int32_t _M0L6offsetS338;
  #line 88 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1311 = _M0L10total__lenS360 - _M0L12digit__startS348;
  _M0L3numS337 = _M0L3numS359;
  _M0L6offsetS338 = _M0L6_2atmpS1311;
  while (1) {
    if (_M0L3numS337 >= 10000u) {
      uint32_t _M0L1tS339 = _M0L3numS337 / 10000u;
      uint32_t _M0L6_2atmpS1288 = _M0L3numS337 % 10000u;
      int32_t _M0L1rS340 = *(int32_t*)&_M0L6_2atmpS1288;
      int32_t _M0L2d1S341 = _M0L1rS340 / 100;
      int32_t _M0L2d2S342 = _M0L1rS340 % 100;
      int32_t _M0L6_2atmpS1287 = _M0L2d1S341 / 10;
      int32_t _M0L6_2atmpS1286 = 48 + _M0L6_2atmpS1287;
      int32_t _M0L6d1__hiS343 = (uint16_t)_M0L6_2atmpS1286;
      int32_t _M0L6_2atmpS1285 = _M0L2d1S341 % 10;
      int32_t _M0L6_2atmpS1284 = 48 + _M0L6_2atmpS1285;
      int32_t _M0L6d1__loS344 = (uint16_t)_M0L6_2atmpS1284;
      int32_t _M0L6_2atmpS1283 = _M0L2d2S342 / 10;
      int32_t _M0L6_2atmpS1282 = 48 + _M0L6_2atmpS1283;
      int32_t _M0L6d2__hiS345 = (uint16_t)_M0L6_2atmpS1282;
      int32_t _M0L6_2atmpS1281 = _M0L2d2S342 % 10;
      int32_t _M0L6_2atmpS1280 = 48 + _M0L6_2atmpS1281;
      int32_t _M0L6d2__loS346 = (uint16_t)_M0L6_2atmpS1280;
      int32_t _M0L6_2atmpS1272 = _M0L12digit__startS348 + _M0L6offsetS338;
      int32_t _M0L6_2atmpS1271 = _M0L6_2atmpS1272 - 4;
      int32_t _M0L6_2atmpS1274;
      int32_t _M0L6_2atmpS1273;
      int32_t _M0L6_2atmpS1276;
      int32_t _M0L6_2atmpS1275;
      int32_t _M0L6_2atmpS1278;
      int32_t _M0L6_2atmpS1277;
      int32_t _M0L6_2atmpS1279;
      _M0L6bufferS347[_M0L6_2atmpS1271] = _M0L6d1__hiS343;
      _M0L6_2atmpS1274 = _M0L12digit__startS348 + _M0L6offsetS338;
      _M0L6_2atmpS1273 = _M0L6_2atmpS1274 - 3;
      _M0L6bufferS347[_M0L6_2atmpS1273] = _M0L6d1__loS344;
      _M0L6_2atmpS1276 = _M0L12digit__startS348 + _M0L6offsetS338;
      _M0L6_2atmpS1275 = _M0L6_2atmpS1276 - 2;
      _M0L6bufferS347[_M0L6_2atmpS1275] = _M0L6d2__hiS345;
      _M0L6_2atmpS1278 = _M0L12digit__startS348 + _M0L6offsetS338;
      _M0L6_2atmpS1277 = _M0L6_2atmpS1278 - 1;
      _M0L6bufferS347[_M0L6_2atmpS1277] = _M0L6d2__loS346;
      _M0L6_2atmpS1279 = _M0L6offsetS338 - 4;
      _M0L3numS337 = _M0L1tS339;
      _M0L6offsetS338 = _M0L6_2atmpS1279;
      continue;
    } else {
      int32_t _M0L6_2atmpS1310 = *(int32_t*)&_M0L3numS337;
      int32_t _M0L9remainingS350 = _M0L6_2atmpS1310;
      int32_t _M0L6offsetS351 = _M0L6offsetS338;
      while (1) {
        if (_M0L9remainingS350 >= 100) {
          int32_t _M0L1tS352 = _M0L9remainingS350 / 100;
          int32_t _M0L1dS353 = _M0L9remainingS350 % 100;
          int32_t _M0L6_2atmpS1297 = _M0L1dS353 / 10;
          int32_t _M0L6_2atmpS1296 = 48 + _M0L6_2atmpS1297;
          int32_t _M0L5d__hiS354 = (uint16_t)_M0L6_2atmpS1296;
          int32_t _M0L6_2atmpS1295 = _M0L1dS353 % 10;
          int32_t _M0L6_2atmpS1294 = 48 + _M0L6_2atmpS1295;
          int32_t _M0L5d__loS355 = (uint16_t)_M0L6_2atmpS1294;
          int32_t _M0L6_2atmpS1290 = _M0L12digit__startS348 + _M0L6offsetS351;
          int32_t _M0L6_2atmpS1289 = _M0L6_2atmpS1290 - 2;
          int32_t _M0L6_2atmpS1292;
          int32_t _M0L6_2atmpS1291;
          int32_t _M0L6_2atmpS1293;
          _M0L6bufferS347[_M0L6_2atmpS1289] = _M0L5d__hiS354;
          _M0L6_2atmpS1292 = _M0L12digit__startS348 + _M0L6offsetS351;
          _M0L6_2atmpS1291 = _M0L6_2atmpS1292 - 1;
          _M0L6bufferS347[_M0L6_2atmpS1291] = _M0L5d__loS355;
          _M0L6_2atmpS1293 = _M0L6offsetS351 - 2;
          _M0L9remainingS350 = _M0L1tS352;
          _M0L6offsetS351 = _M0L6_2atmpS1293;
          continue;
        } else if (_M0L9remainingS350 >= 10) {
          int32_t _M0L6_2atmpS1305 = _M0L9remainingS350 / 10;
          int32_t _M0L6_2atmpS1304 = 48 + _M0L6_2atmpS1305;
          int32_t _M0L5d__hiS357 = (uint16_t)_M0L6_2atmpS1304;
          int32_t _M0L6_2atmpS1303 = _M0L9remainingS350 % 10;
          int32_t _M0L6_2atmpS1302 = 48 + _M0L6_2atmpS1303;
          int32_t _M0L5d__loS358 = (uint16_t)_M0L6_2atmpS1302;
          int32_t _M0L6_2atmpS1299 = _M0L12digit__startS348 + _M0L6offsetS351;
          int32_t _M0L6_2atmpS1298 = _M0L6_2atmpS1299 - 2;
          int32_t _M0L6_2atmpS1301;
          int32_t _M0L6_2atmpS1300;
          _M0L6bufferS347[_M0L6_2atmpS1298] = _M0L5d__hiS357;
          _M0L6_2atmpS1301 = _M0L12digit__startS348 + _M0L6offsetS351;
          _M0L6_2atmpS1300 = _M0L6_2atmpS1301 - 1;
          _M0L6bufferS347[_M0L6_2atmpS1300] = _M0L5d__loS358;
          moonbit_decref(_M0L6bufferS347);
        } else {
          int32_t _M0L6_2atmpS1309 = _M0L12digit__startS348 + _M0L6offsetS351;
          int32_t _M0L6_2atmpS1306 = _M0L6_2atmpS1309 - 1;
          int32_t _M0L6_2atmpS1308 = 48 + _M0L9remainingS350;
          int32_t _M0L6_2atmpS1307 = (uint16_t)_M0L6_2atmpS1308;
          _M0L6bufferS347[_M0L6_2atmpS1306] = _M0L6_2atmpS1307;
          moonbit_decref(_M0L6bufferS347);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS327,
  uint32_t _M0L3numS331,
  int32_t _M0L12digit__startS328,
  int32_t _M0L10total__lenS330,
  int32_t _M0L5radixS321
) {
  uint32_t _M0L4baseS320;
  int32_t _M0L6_2atmpS1256;
  int32_t _M0L6_2atmpS1255;
  #line 57 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS320 = *(uint32_t*)&_M0L5radixS321;
  _M0L6_2atmpS1256 = _M0L5radixS321 - 1;
  _M0L6_2atmpS1255 = _M0L5radixS321 & _M0L6_2atmpS1256;
  if (_M0L6_2atmpS1255 == 0) {
    int32_t _M0L5shiftS322;
    uint32_t _M0L4maskS323;
    int32_t _M0L6_2atmpS1263;
    int32_t _M0L6offsetS324;
    uint32_t _M0L1nS325;
    #line 68 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS322 = moonbit_ctz32(_M0L5radixS321);
    _M0L4maskS323 = _M0L4baseS320 - 1u;
    _M0L6_2atmpS1263 = _M0L10total__lenS330 - _M0L12digit__startS328;
    _M0L6offsetS324 = _M0L6_2atmpS1263;
    _M0L1nS325 = _M0L3numS331;
    while (1) {
      if (_M0L1nS325 > 0u) {
        uint32_t _M0L6_2atmpS1262 = _M0L1nS325 & _M0L4maskS323;
        int32_t _M0L5digitS326 = *(int32_t*)&_M0L6_2atmpS1262;
        int32_t _M0L6_2atmpS1259 = _M0L12digit__startS328 + _M0L6offsetS324;
        int32_t _M0L6_2atmpS1257 = _M0L6_2atmpS1259 - 1;
        int32_t _M0L6_2atmpS1258 =
          ((moonbit_string_t)moonbit_string_literal_39.data)[_M0L5digitS326];
        int32_t _M0L6_2atmpS1260;
        uint32_t _M0L6_2atmpS1261;
        _M0L6bufferS327[_M0L6_2atmpS1257] = _M0L6_2atmpS1258;
        _M0L6_2atmpS1260 = _M0L6offsetS324 - 1;
        _M0L6_2atmpS1261 = _M0L1nS325 >> (_M0L5shiftS322 & 31);
        _M0L6offsetS324 = _M0L6_2atmpS1260;
        _M0L1nS325 = _M0L6_2atmpS1261;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS327);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1270 = _M0L10total__lenS330 - _M0L12digit__startS328;
    int32_t _M0L6offsetS332 = _M0L6_2atmpS1270;
    uint32_t _M0L1nS333 = _M0L3numS331;
    while (1) {
      if (_M0L1nS333 > 0u) {
        uint32_t _M0L1qS334 = _M0L1nS333 / _M0L4baseS320;
        uint32_t _M0L6_2atmpS1269 = _M0L1qS334 * _M0L4baseS320;
        uint32_t _M0L6_2atmpS1268 = _M0L1nS333 - _M0L6_2atmpS1269;
        int32_t _M0L5digitS335 = *(int32_t*)&_M0L6_2atmpS1268;
        int32_t _M0L6_2atmpS1266 = _M0L12digit__startS328 + _M0L6offsetS332;
        int32_t _M0L6_2atmpS1264 = _M0L6_2atmpS1266 - 1;
        int32_t _M0L6_2atmpS1265 =
          ((moonbit_string_t)moonbit_string_literal_39.data)[_M0L5digitS335];
        int32_t _M0L6_2atmpS1267;
        _M0L6bufferS327[_M0L6_2atmpS1264] = _M0L6_2atmpS1265;
        _M0L6_2atmpS1267 = _M0L6offsetS332 - 1;
        _M0L6offsetS332 = _M0L6_2atmpS1267;
        _M0L1nS333 = _M0L1qS334;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS327);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS314,
  uint32_t _M0L3numS319,
  int32_t _M0L12digit__startS315,
  int32_t _M0L10total__lenS318
) {
  int32_t _M0L6_2atmpS1254;
  int32_t _M0L6offsetS309;
  uint32_t _M0L1nS310;
  #line 29 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1254 = _M0L10total__lenS318 - _M0L12digit__startS315;
  _M0L6offsetS309 = _M0L6_2atmpS1254;
  _M0L1nS310 = _M0L3numS319;
  while (1) {
    if (_M0L6offsetS309 >= 2) {
      uint32_t _M0L6_2atmpS1251 = _M0L1nS310 & 255u;
      int32_t _M0L9byte__valS311 = *(int32_t*)&_M0L6_2atmpS1251;
      int32_t _M0L2hiS312 = _M0L9byte__valS311 / 16;
      int32_t _M0L2loS313 = _M0L9byte__valS311 % 16;
      int32_t _M0L6_2atmpS1245 = _M0L12digit__startS315 + _M0L6offsetS309;
      int32_t _M0L6_2atmpS1243 = _M0L6_2atmpS1245 - 2;
      int32_t _M0L6_2atmpS1244 =
        ((moonbit_string_t)moonbit_string_literal_39.data)[_M0L2hiS312];
      int32_t _M0L6_2atmpS1248;
      int32_t _M0L6_2atmpS1246;
      int32_t _M0L6_2atmpS1247;
      int32_t _M0L6_2atmpS1249;
      uint32_t _M0L6_2atmpS1250;
      _M0L6bufferS314[_M0L6_2atmpS1243] = _M0L6_2atmpS1244;
      _M0L6_2atmpS1248 = _M0L12digit__startS315 + _M0L6offsetS309;
      _M0L6_2atmpS1246 = _M0L6_2atmpS1248 - 1;
      _M0L6_2atmpS1247
      = ((moonbit_string_t)moonbit_string_literal_39.data)[
        _M0L2loS313
      ];
      _M0L6bufferS314[_M0L6_2atmpS1246] = _M0L6_2atmpS1247;
      _M0L6_2atmpS1249 = _M0L6offsetS309 - 2;
      _M0L6_2atmpS1250 = _M0L1nS310 >> 8;
      _M0L6offsetS309 = _M0L6_2atmpS1249;
      _M0L1nS310 = _M0L6_2atmpS1250;
      continue;
    } else if (_M0L6offsetS309 == 1) {
      uint32_t _M0L6_2atmpS1253 = _M0L1nS310 & 15u;
      int32_t _M0L6nibbleS317 = *(int32_t*)&_M0L6_2atmpS1253;
      int32_t _M0L6_2atmpS1252 =
        ((moonbit_string_t)moonbit_string_literal_39.data)[_M0L6nibbleS317];
      _M0L6bufferS314[_M0L12digit__startS315] = _M0L6_2atmpS1252;
      moonbit_decref(_M0L6bufferS314);
    } else {
      moonbit_decref(_M0L6bufferS314);
    }
    break;
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger _M0L4selfS292,
  struct _M0TWEOs* _M0L4iterS296,
  moonbit_string_t _M0L6prefixS293,
  moonbit_string_t _M0L6suffixS308,
  moonbit_string_t _M0L3sepS299,
  int32_t _M0L8trailingS294
) {
  #line 161 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  if (_M0L4selfS292.$1) {
    moonbit_incref(_M0L4selfS292.$1);
  }
  #line 169 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L6prefixS293);
  if (_M0L8trailingS294) {
    while (1) {
      moonbit_string_t _M0L7_2abindS295;
      moonbit_incref(_M0L4iterS296);
      #line 171 "/Users/user/.moon/lib/core/builtin/traits.mbt"
      _M0L7_2abindS295 = _M0MPB4Iter4nextGsE(_M0L4iterS296);
      if (_M0L7_2abindS295 == 0) {
        moonbit_decref(_M0L3sepS299);
        moonbit_decref(_M0L4iterS296);
        if (_M0L7_2abindS295) {
          moonbit_decref(_M0L7_2abindS295);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS297 = _M0L7_2abindS295;
        moonbit_string_t _M0L4_2axS298 = _M0L7_2aSomeS297;
        if (_M0L4selfS292.$1) {
          moonbit_incref(_M0L4selfS292.$1);
        }
        #line 172 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0MPB6Logger13write__objectGsE(_M0L4selfS292, _M0L4_2axS298);
        moonbit_incref(_M0L3sepS299);
        if (_M0L4selfS292.$1) {
          moonbit_incref(_M0L4selfS292.$1);
        }
        #line 173 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L3sepS299);
        continue;
      }
      break;
    }
  } else {
    moonbit_string_t _M0L7_2abindS301;
    moonbit_incref(_M0L4iterS296);
    #line 175 "/Users/user/.moon/lib/core/builtin/traits.mbt"
    _M0L7_2abindS301 = _M0MPB4Iter4nextGsE(_M0L4iterS296);
    if (_M0L7_2abindS301 == 0) {
      if (_M0L7_2abindS301) {
        moonbit_decref(_M0L7_2abindS301);
      }
      moonbit_decref(_M0L3sepS299);
      moonbit_decref(_M0L4iterS296);
    } else {
      moonbit_string_t _M0L7_2aSomeS302 = _M0L7_2abindS301;
      moonbit_string_t _M0L4_2axS303 = _M0L7_2aSomeS302;
      if (_M0L4selfS292.$1) {
        moonbit_incref(_M0L4selfS292.$1);
      }
      #line 176 "/Users/user/.moon/lib/core/builtin/traits.mbt"
      _M0MPB6Logger13write__objectGsE(_M0L4selfS292, _M0L4_2axS303);
      while (1) {
        moonbit_string_t _M0L7_2abindS304;
        moonbit_incref(_M0L4iterS296);
        #line 177 "/Users/user/.moon/lib/core/builtin/traits.mbt"
        _M0L7_2abindS304 = _M0MPB4Iter4nextGsE(_M0L4iterS296);
        if (_M0L7_2abindS304 == 0) {
          if (_M0L7_2abindS304) {
            moonbit_decref(_M0L7_2abindS304);
          }
          moonbit_decref(_M0L3sepS299);
          moonbit_decref(_M0L4iterS296);
        } else {
          moonbit_string_t _M0L7_2aSomeS305 = _M0L7_2abindS304;
          moonbit_string_t _M0L4_2axS306 = _M0L7_2aSomeS305;
          moonbit_incref(_M0L3sepS299);
          if (_M0L4selfS292.$1) {
            moonbit_incref(_M0L4selfS292.$1);
          }
          #line 178 "/Users/user/.moon/lib/core/builtin/traits.mbt"
          _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L3sepS299);
          if (_M0L4selfS292.$1) {
            moonbit_incref(_M0L4selfS292.$1);
          }
          #line 179 "/Users/user/.moon/lib/core/builtin/traits.mbt"
          _M0MPB6Logger13write__objectGsE(_M0L4selfS292, _M0L4_2axS306);
          continue;
        }
        break;
      }
    }
  }
  #line 182 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L6suffixS308);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS291) {
  struct _M0TWEOs* _M0L7_2afuncS290;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS290 = _M0L4selfS291;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS290->code(_M0L7_2afuncS290);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE* _M0L4selfS287
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS286;
  struct _M0TPB6Logger _M0L6_2atmpS1241;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS286 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS286);
  _M0L6_2atmpS1241
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS286
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L4selfS287, _M0L6_2atmpS1241);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS286);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS289
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS288;
  struct _M0TPB6Logger _M0L6_2atmpS1242;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS288 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS288);
  _M0L6_2atmpS1242
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS288
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS289, _M0L6_2atmpS1242);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS288);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS285
) {
  int32_t _result_2066;
  #line 98 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _result_2066 = _M0L4selfS285.$1;
  moonbit_decref(_M0L4selfS285.$0);
  return _result_2066;
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
  int32_t _M0L6_2atmpS1240;
  int64_t _M0L6_2atmpS1239;
  struct _M0TPC16string10StringView _M0L6_2atmpS1238;
  #line 102 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1240 = _M0L5startS282 + _M0L3lenS283;
  _M0L6_2atmpS1239 = (int64_t)_M0L6_2atmpS1240;
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1238
  = _M0MPC16string6String11sub_2einner(_M0L5valueS281, _M0L5startS282, _M0L6_2atmpS1239);
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS280, _M0L6_2atmpS1238);
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
  int32_t _if__result_2067;
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
      _if__result_2067 = _M0L3endS274 <= _M0L3lenS272;
    } else {
      _if__result_2067 = 0;
    }
  } else {
    _if__result_2067 = 0;
  }
  if (_if__result_2067) {
    if (_M0L5startS278 < _M0L3lenS272) {
      int32_t _M0L6_2atmpS1235 = _M0L4selfS273[_M0L5startS278];
      int32_t _M0L6_2atmpS1234;
      #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1234
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1235);
      if (!_M0L6_2atmpS1234) {
        
      } else {
        #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS274 < _M0L3lenS272) {
      int32_t _M0L6_2atmpS1237 = _M0L4selfS273[_M0L3endS274];
      int32_t _M0L6_2atmpS1236;
      #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1236
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1237);
      if (!_M0L6_2atmpS1236) {
        
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
  uint32_t _M0L6_2atmpS1233;
  uint32_t _M0L6_2atmpS1232;
  struct _M0TPB6Hasher* _block_2068;
  #line 75 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1233 = *(uint32_t*)&_M0L4seedS264;
  _M0L6_2atmpS1232 = _M0L6_2atmpS1233 + 374761393u;
  _block_2068
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2068)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2068->$0 = _M0L6_2atmpS1232;
  return _block_2068;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS263) {
  uint32_t _M0L6_2atmpS1231;
  #line 435 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1231 = _M0MPB6Hasher9avalanche(_M0L4selfS263);
  return *(int32_t*)&_M0L6_2atmpS1231;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS262) {
  uint32_t _M0Lm3accS261;
  uint32_t _M0L6_2atmpS1220;
  uint32_t _M0L6_2atmpS1222;
  uint32_t _M0L6_2atmpS1221;
  uint32_t _M0L6_2atmpS1223;
  uint32_t _M0L6_2atmpS1224;
  uint32_t _M0L6_2atmpS1226;
  uint32_t _M0L6_2atmpS1225;
  uint32_t _M0L6_2atmpS1227;
  uint32_t _M0L6_2atmpS1228;
  uint32_t _M0L6_2atmpS1230;
  uint32_t _M0L6_2atmpS1229;
  #line 440 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS261 = _M0L4selfS262->$0;
  moonbit_decref(_M0L4selfS262);
  _M0L6_2atmpS1220 = _M0Lm3accS261;
  _M0L6_2atmpS1222 = _M0Lm3accS261;
  _M0L6_2atmpS1221 = _M0L6_2atmpS1222 >> 15;
  _M0Lm3accS261 = _M0L6_2atmpS1220 ^ _M0L6_2atmpS1221;
  _M0L6_2atmpS1223 = _M0Lm3accS261;
  _M0Lm3accS261 = _M0L6_2atmpS1223 * 2246822519u;
  _M0L6_2atmpS1224 = _M0Lm3accS261;
  _M0L6_2atmpS1226 = _M0Lm3accS261;
  _M0L6_2atmpS1225 = _M0L6_2atmpS1226 >> 13;
  _M0Lm3accS261 = _M0L6_2atmpS1224 ^ _M0L6_2atmpS1225;
  _M0L6_2atmpS1227 = _M0Lm3accS261;
  _M0Lm3accS261 = _M0L6_2atmpS1227 * 3266489917u;
  _M0L6_2atmpS1228 = _M0Lm3accS261;
  _M0L6_2atmpS1230 = _M0Lm3accS261;
  _M0L6_2atmpS1229 = _M0L6_2atmpS1230 >> 16;
  _M0Lm3accS261 = _M0L6_2atmpS1228 ^ _M0L6_2atmpS1229;
  return _M0Lm3accS261;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS259,
  moonbit_string_t _M0L1yS260
) {
  int32_t _M0L6_2atmpS1219;
  #line 23 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 24 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1219 = moonbit_val_array_equal(_M0L1xS259, _M0L1yS260);
  moonbit_decref(_M0L1yS260);
  moonbit_decref(_M0L1xS259);
  return !_M0L6_2atmpS1219;
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
  uint32_t _M0L6_2atmpS1218;
  #line 187 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1218 = *(uint32_t*)&_M0L5valueS254;
  #line 188 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS253, _M0L6_2atmpS1218);
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
    moonbit_string_t _M0L6_2atmpS1216;
    moonbit_string_t _M0L6_2atmpS1215;
    moonbit_string_t _M0L6_2atmpS1214;
    moonbit_string_t _M0L14expect__base64S251;
    moonbit_string_t _M0L6_2atmpS1213;
    moonbit_string_t _M0L6_2atmpS1212;
    moonbit_string_t _M0L6_2atmpS1211;
    moonbit_string_t _M0L14actual__base64S252;
    moonbit_string_t _M0L6_2atmpS1210;
    moonbit_string_t _M0L6_2atmpS1209;
    moonbit_string_t _M0L6_2atmpS1207;
    moonbit_string_t _M0L6_2atmpS1208;
    moonbit_string_t _M0L6_2atmpS1206;
    moonbit_string_t _M0L6_2atmpS1204;
    moonbit_string_t _M0L6_2atmpS1205;
    moonbit_string_t _M0L6_2atmpS1203;
    moonbit_string_t _M0L6_2atmpS1201;
    moonbit_string_t _M0L6_2atmpS1202;
    moonbit_string_t _M0L6_2atmpS1200;
    moonbit_string_t _M0L6_2atmpS1198;
    moonbit_string_t _M0L6_2atmpS1199;
    moonbit_string_t _M0L6_2atmpS1197;
    moonbit_string_t _M0L6_2atmpS1195;
    moonbit_string_t _M0L6_2atmpS1196;
    moonbit_string_t _M0L6_2atmpS1194;
    moonbit_string_t _M0L6_2atmpS1193;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1192;
    struct moonbit_result_0 _result_2069;
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
    _M0L6_2atmpS1216
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS244);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1215
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1216);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1214
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS1215);
    moonbit_decref(_M0L6_2atmpS1215);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14expect__base64S251
    = moonbit_add_string(_M0L6_2atmpS1214, (moonbit_string_t)moonbit_string_literal_40.data);
    moonbit_decref(_M0L6_2atmpS1214);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1213
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS242);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1212
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1213);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1211
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS1212);
    moonbit_decref(_M0L6_2atmpS1212);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14actual__base64S252
    = moonbit_add_string(_M0L6_2atmpS1211, (moonbit_string_t)moonbit_string_literal_40.data);
    moonbit_decref(_M0L6_2atmpS1211);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1210 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS245);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1209
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS1210);
    moonbit_decref(_M0L6_2atmpS1210);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1207
    = moonbit_add_string(_M0L6_2atmpS1209, (moonbit_string_t)moonbit_string_literal_42.data);
    moonbit_decref(_M0L6_2atmpS1209);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1208
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS247);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1206 = moonbit_add_string(_M0L6_2atmpS1207, _M0L6_2atmpS1208);
    moonbit_decref(_M0L6_2atmpS1208);
    moonbit_decref(_M0L6_2atmpS1207);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1204
    = moonbit_add_string(_M0L6_2atmpS1206, (moonbit_string_t)moonbit_string_literal_43.data);
    moonbit_decref(_M0L6_2atmpS1206);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1205
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS249);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1203 = moonbit_add_string(_M0L6_2atmpS1204, _M0L6_2atmpS1205);
    moonbit_decref(_M0L6_2atmpS1205);
    moonbit_decref(_M0L6_2atmpS1204);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1201
    = moonbit_add_string(_M0L6_2atmpS1203, (moonbit_string_t)moonbit_string_literal_44.data);
    moonbit_decref(_M0L6_2atmpS1203);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1202
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS250);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1200 = moonbit_add_string(_M0L6_2atmpS1201, _M0L6_2atmpS1202);
    moonbit_decref(_M0L6_2atmpS1202);
    moonbit_decref(_M0L6_2atmpS1201);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1198
    = moonbit_add_string(_M0L6_2atmpS1200, (moonbit_string_t)moonbit_string_literal_45.data);
    moonbit_decref(_M0L6_2atmpS1200);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1199
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S251);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1197 = moonbit_add_string(_M0L6_2atmpS1198, _M0L6_2atmpS1199);
    moonbit_decref(_M0L6_2atmpS1199);
    moonbit_decref(_M0L6_2atmpS1198);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1195
    = moonbit_add_string(_M0L6_2atmpS1197, (moonbit_string_t)moonbit_string_literal_46.data);
    moonbit_decref(_M0L6_2atmpS1197);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1196
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S252);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1194 = moonbit_add_string(_M0L6_2atmpS1195, _M0L6_2atmpS1196);
    moonbit_decref(_M0L6_2atmpS1196);
    moonbit_decref(_M0L6_2atmpS1195);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1193
    = moonbit_add_string(_M0L6_2atmpS1194, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1194);
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1192
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1192)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1192)->$0
    = _M0L6_2atmpS1193;
    _result_2069.tag = 0;
    _result_2069.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1192;
    return _result_2069;
  } else {
    int32_t _M0L6_2atmpS1217;
    struct moonbit_result_0 _result_2070;
    moonbit_decref(_M0L9args__locS248);
    moonbit_decref(_M0L3locS246);
    moonbit_decref(_M0L7contentS244);
    moonbit_decref(_M0L6actualS242);
    _M0L6_2atmpS1217 = 0;
    _result_2070.tag = 1;
    _result_2070.data.ok = _M0L6_2atmpS1217;
    return _result_2070;
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
      moonbit_string_t* _M0L3bufS1191 = _M0L7_2aselfS234->$0;
      moonbit_string_t _M0L4itemS238 =
        (moonbit_string_t)_M0L3bufS1191[_M0L1iS237];
      int32_t _M0L6_2atmpS1190;
      if (_M0L1iS237 != 0) {
        if (_M0L4itemS238) {
          moonbit_incref(_M0L4itemS238);
        }
        moonbit_incref(_M0L3bufS233);
        #line 130 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, (moonbit_string_t)moonbit_string_literal_34.data);
      } else if (_M0L4itemS238) {
        moonbit_incref(_M0L4itemS238);
      }
      if (_M0L4itemS238 == 0) {
        if (_M0L4itemS238) {
          moonbit_decref(_M0L4itemS238);
        }
        moonbit_incref(_M0L3bufS233);
        #line 133 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, (moonbit_string_t)moonbit_string_literal_47.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS239 = _M0L4itemS238;
        moonbit_string_t _M0L6_2alocS240 = _M0L7_2aSomeS239;
        moonbit_string_t _M0L6_2atmpS1189;
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1189
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS240);
        moonbit_incref(_M0L3bufS233);
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS233, _M0L6_2atmpS1189);
      }
      _M0L6_2atmpS1190 = _M0L1iS237 + 1;
      _M0L1iS237 = _M0L6_2atmpS1190;
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
  moonbit_string_t _M0L6_2atmpS1188;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1187;
  #line 95 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1188 = _M0L4selfS232;
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1187 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1188);
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1187);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS231
) {
  struct _M0TPB13StringBuilder* _M0L2sbS230;
  struct _M0TPC16string10StringView _M0L8filenameS1173;
  struct _M0TPC16string10StringView _M0L11start__lineS1176;
  moonbit_string_t _M0L6_2atmpS1175;
  moonbit_string_t _M0L6_2atmpS1174;
  struct _M0TPC16string10StringView _M0L13start__columnS1179;
  moonbit_string_t _M0L6_2atmpS1178;
  moonbit_string_t _M0L6_2atmpS1177;
  struct _M0TPC16string10StringView _M0L9end__lineS1182;
  moonbit_string_t _M0L6_2atmpS1181;
  moonbit_string_t _M0L6_2atmpS1180;
  struct _M0TPC16string10StringView _M0L8_2afieldS1880;
  int32_t _M0L6_2acntS1965;
  struct _M0TPC16string10StringView _M0L11end__columnS1186;
  moonbit_string_t _M0L6_2atmpS1185;
  moonbit_string_t _M0L6_2atmpS1184;
  moonbit_string_t _M0L6_2atmpS1183;
  #line 82 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 83 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS230 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L2sbS230);
  #line 84 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, (moonbit_string_t)moonbit_string_literal_48.data);
  _M0L8filenameS1173
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$0_1, _M0L4selfS231->$0_2, _M0L4selfS231->$0_0
  };
  moonbit_incref(_M0L8filenameS1173.$0);
  moonbit_incref(_M0L2sbS230);
  #line 85 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS230, _M0L8filenameS1173);
  _M0L11start__lineS1176
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$1_1, _M0L4selfS231->$1_2, _M0L4selfS231->$1_0
  };
  moonbit_incref(_M0L11start__lineS1176.$0);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1175
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1176);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1174
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_49.data, _M0L6_2atmpS1175);
  moonbit_decref(_M0L6_2atmpS1175);
  moonbit_incref(_M0L2sbS230);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1174);
  _M0L13start__columnS1179
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$2_1, _M0L4selfS231->$2_2, _M0L4selfS231->$2_0
  };
  moonbit_incref(_M0L13start__columnS1179.$0);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1178
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1179);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1177
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS1178);
  moonbit_decref(_M0L6_2atmpS1178);
  moonbit_incref(_M0L2sbS230);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1177);
  _M0L9end__lineS1182
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$3_1, _M0L4selfS231->$3_2, _M0L4selfS231->$3_0
  };
  moonbit_incref(_M0L9end__lineS1182.$0);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1181
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1182);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1180
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_51.data, _M0L6_2atmpS1181);
  moonbit_decref(_M0L6_2atmpS1181);
  moonbit_incref(_M0L2sbS230);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1180);
  _M0L8_2afieldS1880
  = (struct _M0TPC16string10StringView){
    _M0L4selfS231->$4_1, _M0L4selfS231->$4_2, _M0L4selfS231->$4_0
  };
  _M0L6_2acntS1965 = Moonbit_object_header(_M0L4selfS231)->rc;
  if (_M0L6_2acntS1965 > 1) {
    int32_t _M0L11_2anew__cntS1970 = _M0L6_2acntS1965 - 1;
    Moonbit_object_header(_M0L4selfS231)->rc = _M0L11_2anew__cntS1970;
    moonbit_incref(_M0L8_2afieldS1880.$0);
  } else if (_M0L6_2acntS1965 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS1969 =
      (struct _M0TPC16string10StringView){_M0L4selfS231->$3_1,
                                            _M0L4selfS231->$3_2,
                                            _M0L4selfS231->$3_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS1968;
    struct _M0TPC16string10StringView _M0L8_2afieldS1967;
    struct _M0TPC16string10StringView _M0L8_2afieldS1966;
    moonbit_decref(_M0L8_2afieldS1969.$0);
    _M0L8_2afieldS1968
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$2_1, _M0L4selfS231->$2_2, _M0L4selfS231->$2_0
    };
    moonbit_decref(_M0L8_2afieldS1968.$0);
    _M0L8_2afieldS1967
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$1_1, _M0L4selfS231->$1_2, _M0L4selfS231->$1_0
    };
    moonbit_decref(_M0L8_2afieldS1967.$0);
    _M0L8_2afieldS1966
    = (struct _M0TPC16string10StringView){
      _M0L4selfS231->$0_1, _M0L4selfS231->$0_2, _M0L4selfS231->$0_0
    };
    moonbit_decref(_M0L8_2afieldS1966.$0);
    #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS231);
  }
  _M0L11end__columnS1186 = _M0L8_2afieldS1880;
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1185
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1186);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1184
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_52.data, _M0L6_2atmpS1185);
  moonbit_decref(_M0L6_2atmpS1185);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1183
  = moonbit_add_string(_M0L6_2atmpS1184, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1184);
  moonbit_incref(_M0L2sbS230);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS230, _M0L6_2atmpS1183);
  #line 90 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS230);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS229,
  moonbit_string_t _M0L3strS228
) {
  int32_t _M0L8str__lenS227;
  int32_t _M0L3lenS1168;
  int32_t _M0L6_2atmpS1167;
  uint16_t* _M0L4dataS1169;
  int32_t _M0L3lenS1170;
  int32_t _M0L3lenS1172;
  int32_t _M0L6_2atmpS1171;
  #line 81 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS227 = Moonbit_array_length(_M0L3strS228);
  _M0L3lenS1168 = _M0L4selfS229->$1;
  _M0L6_2atmpS1167 = _M0L3lenS1168 + _M0L8str__lenS227;
  moonbit_incref(_M0L4selfS229);
  #line 83 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS229, _M0L6_2atmpS1167);
  _M0L4dataS1169 = _M0L4selfS229->$0;
  _M0L3lenS1170 = _M0L4selfS229->$1;
  moonbit_incref(_M0L4dataS1169);
  #line 84 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1169, _M0L3lenS1170, _M0L3strS228, 0, _M0L8str__lenS227);
  _M0L3lenS1172 = _M0L4selfS229->$1;
  _M0L6_2atmpS1171 = _M0L3lenS1172 + _M0L8str__lenS227;
  _M0L4selfS229->$1 = _M0L6_2atmpS1171;
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
      int32_t _M0L6_2atmpS1164 = _M0L3strS224[_M0L1iS221];
      int32_t _M0L6_2atmpS1165;
      int32_t _M0L6_2atmpS1166;
      if (
        _M0L1jS222 < 0 || _M0L1jS222 >= Moonbit_array_length(_M0L4selfS223)
      ) {
        #line 75 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS223[_M0L1jS222] = _M0L6_2atmpS1164;
      _M0L6_2atmpS1165 = _M0L1iS221 + 1;
      _M0L6_2atmpS1166 = _M0L1jS222 + 1;
      _M0L1iS221 = _M0L6_2atmpS1165;
      _M0L1jS222 = _M0L6_2atmpS1166;
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
  struct _M0TPB6Logger _M0L6_2atmpS1163;
  #line 17 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1163
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS217
  };
  #line 21 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS216, _M0L6_2atmpS1163);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS161
) {
  int32_t _M0L6_2atmpS1162;
  struct _M0TPC16string10StringView _M0L7_2abindS160;
  moonbit_string_t _M0L7_2adataS162;
  int32_t _M0L8_2astartS163;
  int32_t _M0L6_2atmpS1161;
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
  int32_t _M0L6_2atmpS1120;
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1162 = Moonbit_array_length(_M0L4reprS161);
  _M0L7_2abindS160
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1162, _M0L4reprS161
  };
  moonbit_incref(_M0L7_2abindS160.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS162 = _M0MPC16string10StringView4data(_M0L7_2abindS160);
  moonbit_incref(_M0L7_2abindS160.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS163
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS160);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1161 = _M0MPC16string10StringView6length(_M0L7_2abindS160);
  _M0L6_2aendS164 = _M0L8_2astartS163 + _M0L6_2atmpS1161;
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
  _M0L6_2atmpS1120 = _M0Lm9_2acursorS165;
  if (_M0L6_2atmpS1120 < _M0L6_2aendS164) {
    int32_t _M0L6_2atmpS1121 = _M0Lm9_2acursorS165;
    int32_t _M0L12dispatch__15S188;
    _M0Lm9_2acursorS165 = _M0L6_2atmpS1121 + 1;
    _M0L12dispatch__15S188 = 0;
    loop__label__15_191:;
    while (1) {
      int32_t _M0L6_2atmpS1125;
      int32_t _M0L6_2atmpS1122;
      switch (_M0L12dispatch__15S188) {
        case 6: {
          int32_t _M0L6_2atmpS1128;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1128 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1128 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1130 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS196;
            int32_t _M0L6_2atmpS1129;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS196
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1130);
            _M0L6_2atmpS1129 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1129 + 1;
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
          int32_t _M0L6_2atmpS1131;
          _M0Lm9tag__0__2S175 = _M0Lm9tag__0__1S174;
          _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1131 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1131 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1136 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS198;
            int32_t _M0L6_2atmpS1132;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS198
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1136);
            _M0L6_2atmpS1132 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1132 + 1;
            if (_M0L10next__charS198 < 58) {
              if (_M0L10next__charS198 < 48) {
                goto join_197;
              } else {
                int32_t _M0L6_2atmpS1133;
                _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
                _M0Lm9tag__1__1S178 = _M0Lm6tag__1S177;
                _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
                _M0Lm6tag__2S176 = _M0Lm9_2acursorS165;
                _M0L6_2atmpS1133 = _M0Lm9_2acursorS165;
                if (_M0L6_2atmpS1133 < _M0L6_2aendS164) {
                  int32_t _M0L6_2atmpS1135 = _M0Lm9_2acursorS165;
                  int32_t _M0L10next__charS200;
                  int32_t _M0L6_2atmpS1134;
                  moonbit_incref(_M0L7_2adataS162);
                  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS200
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1135);
                  _M0L6_2atmpS1134 = _M0Lm9_2acursorS165;
                  _M0Lm9_2acursorS165 = _M0L6_2atmpS1134 + 1;
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
          int32_t _M0L6_2atmpS1137;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0Lm6tag__2S176 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1137 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1137 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1139 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS202;
            int32_t _M0L6_2atmpS1138;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS202
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1139);
            _M0L6_2atmpS1138 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1138 + 1;
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
          int32_t _M0L6_2atmpS1140;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0Lm6tag__4S179 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1140 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1140 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1142 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS204;
            int32_t _M0L6_2atmpS1141;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS204
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1142);
            _M0L6_2atmpS1141 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1141 + 1;
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
          int32_t _M0L6_2atmpS1143;
          _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1143 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1143 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1145 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS206;
            int32_t _M0L6_2atmpS1144;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS206
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1145);
            _M0L6_2atmpS1144 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1144 + 1;
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
          int32_t _M0L6_2atmpS1146;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__3S180 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1146 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1146 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1154 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS208;
            int32_t _M0L6_2atmpS1147;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS208
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1154);
            _M0L6_2atmpS1147 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1147 + 1;
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
              int32_t _M0L6_2atmpS1148;
              _M0Lm9tag__0__2S175 = _M0Lm9tag__0__1S174;
              _M0Lm9tag__0__1S174 = _M0Lm6tag__0S173;
              _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
              _M0L6_2atmpS1148 = _M0Lm9_2acursorS165;
              if (_M0L6_2atmpS1148 < _M0L6_2aendS164) {
                int32_t _M0L6_2atmpS1153 = _M0Lm9_2acursorS165;
                int32_t _M0L10next__charS210;
                int32_t _M0L6_2atmpS1149;
                moonbit_incref(_M0L7_2adataS162);
                #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                _M0L10next__charS210
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1153);
                _M0L6_2atmpS1149 = _M0Lm9_2acursorS165;
                _M0Lm9_2acursorS165 = _M0L6_2atmpS1149 + 1;
                if (_M0L10next__charS210 < 58) {
                  if (_M0L10next__charS210 < 48) {
                    goto join_209;
                  } else {
                    int32_t _M0L6_2atmpS1150;
                    _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
                    _M0Lm9tag__1__1S178 = _M0Lm6tag__1S177;
                    _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
                    _M0Lm6tag__4S179 = _M0Lm9_2acursorS165;
                    _M0L6_2atmpS1150 = _M0Lm9_2acursorS165;
                    if (_M0L6_2atmpS1150 < _M0L6_2aendS164) {
                      int32_t _M0L6_2atmpS1152 = _M0Lm9_2acursorS165;
                      int32_t _M0L10next__charS212;
                      int32_t _M0L6_2atmpS1151;
                      moonbit_incref(_M0L7_2adataS162);
                      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS212
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1152);
                      _M0L6_2atmpS1151 = _M0Lm9_2acursorS165;
                      _M0Lm9_2acursorS165 = _M0L6_2atmpS1151 + 1;
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
          int32_t _M0L6_2atmpS1155;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0Lm6tag__1S177 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1155 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1155 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1157 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS214;
            int32_t _M0L6_2atmpS1156;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS214
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1157);
            _M0L6_2atmpS1156 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1156 + 1;
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
          int32_t _M0L6_2atmpS1158;
          _M0Lm6tag__0S173 = _M0Lm9_2acursorS165;
          _M0L6_2atmpS1158 = _M0Lm9_2acursorS165;
          if (_M0L6_2atmpS1158 < _M0L6_2aendS164) {
            int32_t _M0L6_2atmpS1160 = _M0Lm9_2acursorS165;
            int32_t _M0L10next__charS215;
            int32_t _M0L6_2atmpS1159;
            moonbit_incref(_M0L7_2adataS162);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS215
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1160);
            _M0L6_2atmpS1159 = _M0Lm9_2acursorS165;
            _M0Lm9_2acursorS165 = _M0L6_2atmpS1159 + 1;
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
      _M0L6_2atmpS1125 = _M0Lm9_2acursorS165;
      if (_M0L6_2atmpS1125 < _M0L6_2aendS164) {
        int32_t _M0L6_2atmpS1127 = _M0Lm9_2acursorS165;
        int32_t _M0L10next__charS195;
        int32_t _M0L6_2atmpS1126;
        moonbit_incref(_M0L7_2adataS162);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS195
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1127);
        _M0L6_2atmpS1126 = _M0Lm9_2acursorS165;
        _M0Lm9_2acursorS165 = _M0L6_2atmpS1126 + 1;
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
      _M0L6_2atmpS1122 = _M0Lm9_2acursorS165;
      if (_M0L6_2atmpS1122 < _M0L6_2aendS164) {
        int32_t _M0L6_2atmpS1124 = _M0Lm9_2acursorS165;
        int32_t _M0L10next__charS192;
        int32_t _M0L6_2atmpS1123;
        moonbit_incref(_M0L7_2adataS162);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS192
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS162, _M0L6_2atmpS1124);
        _M0L6_2atmpS1123 = _M0Lm9_2acursorS165;
        _M0Lm9_2acursorS165 = _M0L6_2atmpS1123 + 1;
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
      int32_t _M0L6_2atmpS1119 = _M0Lm20match__tag__saver__0S168;
      int32_t _M0L6_2atmpS1118 = _M0L6_2atmpS1119 + 1;
      int64_t _M0L6_2atmpS1115 = (int64_t)_M0L6_2atmpS1118;
      int32_t _M0L6_2atmpS1117 = _M0Lm20match__tag__saver__1S169;
      int64_t _M0L6_2atmpS1116 = (int64_t)_M0L6_2atmpS1117;
      struct _M0TPC16string10StringView _M0L11start__lineS182;
      int32_t _M0L6_2atmpS1114;
      int32_t _M0L6_2atmpS1113;
      int64_t _M0L6_2atmpS1110;
      int32_t _M0L6_2atmpS1112;
      int64_t _M0L6_2atmpS1111;
      struct _M0TPC16string10StringView _M0L13start__columnS183;
      int64_t _M0L6_2atmpS1107;
      int32_t _M0L6_2atmpS1109;
      int64_t _M0L6_2atmpS1108;
      struct _M0TPC16string10StringView _M0L8filenameS184;
      int32_t _M0L6_2atmpS1106;
      int32_t _M0L6_2atmpS1105;
      int64_t _M0L6_2atmpS1102;
      int32_t _M0L6_2atmpS1104;
      int64_t _M0L6_2atmpS1103;
      struct _M0TPC16string10StringView _M0L9end__lineS185;
      int32_t _M0L6_2atmpS1101;
      int32_t _M0L6_2atmpS1100;
      int64_t _M0L6_2atmpS1097;
      int32_t _M0L6_2atmpS1099;
      int64_t _M0L6_2atmpS1098;
      struct _M0TPC16string10StringView _M0L11end__columnS186;
      int32_t _M0L6_2atmpS1096;
      int32_t _M0L6_2atmpS1095;
      int64_t _M0L6_2atmpS1092;
      int32_t _M0L6_2atmpS1094;
      int64_t _M0L6_2atmpS1093;
      struct _M0TPC16string10StringView _M0L6_2atmpS1886;
      struct _M0TPB13SourceLocRepr* _block_2088;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS182
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1115, _M0L6_2atmpS1116);
      _M0L6_2atmpS1114 = _M0Lm20match__tag__saver__1S169;
      _M0L6_2atmpS1113 = _M0L6_2atmpS1114 + 1;
      _M0L6_2atmpS1110 = (int64_t)_M0L6_2atmpS1113;
      _M0L6_2atmpS1112 = _M0Lm20match__tag__saver__2S170;
      _M0L6_2atmpS1111 = (int64_t)_M0L6_2atmpS1112;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS183
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1110, _M0L6_2atmpS1111);
      _M0L6_2atmpS1107 = (int64_t)_M0L8_2astartS163;
      _M0L6_2atmpS1109 = _M0Lm20match__tag__saver__0S168;
      _M0L6_2atmpS1108 = (int64_t)_M0L6_2atmpS1109;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS184
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1107, _M0L6_2atmpS1108);
      _M0L6_2atmpS1106 = _M0Lm20match__tag__saver__2S170;
      _M0L6_2atmpS1105 = _M0L6_2atmpS1106 + 1;
      _M0L6_2atmpS1102 = (int64_t)_M0L6_2atmpS1105;
      _M0L6_2atmpS1104 = _M0Lm20match__tag__saver__3S171;
      _M0L6_2atmpS1103 = (int64_t)_M0L6_2atmpS1104;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS185
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1102, _M0L6_2atmpS1103);
      _M0L6_2atmpS1101 = _M0Lm20match__tag__saver__3S171;
      _M0L6_2atmpS1100 = _M0L6_2atmpS1101 + 1;
      _M0L6_2atmpS1097 = (int64_t)_M0L6_2atmpS1100;
      _M0L6_2atmpS1099 = _M0Lm20match__tag__saver__4S172;
      _M0L6_2atmpS1098 = (int64_t)_M0L6_2atmpS1099;
      moonbit_incref(_M0L7_2adataS162);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS186
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1097, _M0L6_2atmpS1098);
      _M0L6_2atmpS1096 = _M0Lm20match__tag__saver__4S172;
      _M0L6_2atmpS1095 = _M0L6_2atmpS1096 + 1;
      _M0L6_2atmpS1092 = (int64_t)_M0L6_2atmpS1095;
      _M0L6_2atmpS1094 = _M0Lm10match__endS167;
      _M0L6_2atmpS1093 = (int64_t)_M0L6_2atmpS1094;
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6_2atmpS1886
      = _M0MPC16string6String4view(_M0L7_2adataS162, _M0L6_2atmpS1092, _M0L6_2atmpS1093);
      moonbit_decref(_M0L6_2atmpS1886.$0);
      _block_2088
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2088)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 5, 0);
      _block_2088->$0_0 = _M0L8filenameS184.$0;
      _block_2088->$0_1 = _M0L8filenameS184.$1;
      _block_2088->$0_2 = _M0L8filenameS184.$2;
      _block_2088->$1_0 = _M0L11start__lineS182.$0;
      _block_2088->$1_1 = _M0L11start__lineS182.$1;
      _block_2088->$1_2 = _M0L11start__lineS182.$2;
      _block_2088->$2_0 = _M0L13start__columnS183.$0;
      _block_2088->$2_1 = _M0L13start__columnS183.$1;
      _block_2088->$2_2 = _M0L13start__columnS183.$2;
      _block_2088->$3_0 = _M0L9end__lineS185.$0;
      _block_2088->$3_1 = _M0L9end__lineS185.$1;
      _block_2088->$3_2 = _M0L9end__lineS185.$2;
      _block_2088->$4_0 = _M0L11end__columnS186.$0;
      _block_2088->$4_1 = _M0L11end__columnS186.$1;
      _block_2088->$4_2 = _M0L11end__columnS186.$2;
      return _block_2088;
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
  int32_t _M0L6_2atmpS1091;
  struct _M0TPC16string10StringView _M0L6_2atmpS1089;
  struct _M0TPB6Logger _M0L6_2atmpS1090;
  #line 145 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 146 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS157 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS1091 = Moonbit_array_length(_M0L4selfS158);
  _M0L6_2atmpS1089
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1091, _M0L4selfS158
  };
  moonbit_incref(_M0L3bufS157);
  _M0L6_2atmpS1090
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS157
  };
  #line 147 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1089, _M0L6_2atmpS1090, _M0L5quoteS159);
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
    int32_t _M0L6_2atmpS1073;
    int32_t _M0L6_2atmpS1074;
    int32_t _M0L6_2atmpS1075;
    int32_t _tmp_2092;
    int32_t _tmp_2093;
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
        int32_t _M0L6_2atmpS1076;
        int32_t _M0L6_2atmpS1077;
        moonbit_incref(_M0L6_2aenvS150);
        #line 207 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 208 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_53.data);
        _M0L6_2atmpS1076 = _M0L1iS151 + 1;
        _M0L6_2atmpS1077 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1076;
        _M0L3segS152 = _M0L6_2atmpS1077;
        goto _2afor_153;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1078;
        int32_t _M0L6_2atmpS1079;
        moonbit_incref(_M0L6_2aenvS150);
        #line 212 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 213 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_54.data);
        _M0L6_2atmpS1078 = _M0L1iS151 + 1;
        _M0L6_2atmpS1079 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1078;
        _M0L3segS152 = _M0L6_2atmpS1079;
        goto _2afor_153;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1080;
        int32_t _M0L6_2atmpS1081;
        moonbit_incref(_M0L6_2aenvS150);
        #line 217 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 218 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_55.data);
        _M0L6_2atmpS1080 = _M0L1iS151 + 1;
        _M0L6_2atmpS1081 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1080;
        _M0L3segS152 = _M0L6_2atmpS1081;
        goto _2afor_153;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1082;
        int32_t _M0L6_2atmpS1083;
        moonbit_incref(_M0L6_2aenvS150);
        #line 222 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
        if (_M0L6loggerS147.$1) {
          moonbit_incref(_M0L6loggerS147.$1);
        }
        #line 223 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_56.data);
        _M0L6_2atmpS1082 = _M0L1iS151 + 1;
        _M0L6_2atmpS1083 = _M0L1iS151 + 1;
        _M0L1iS151 = _M0L6_2atmpS1082;
        _M0L3segS152 = _M0L6_2atmpS1083;
        goto _2afor_153;
        break;
      }
      default: {
        if (_M0L4codeS154 < 32) {
          int32_t _M0L6_2atmpS1085;
          moonbit_string_t _M0L6_2atmpS1084;
          int32_t _M0L6_2atmpS1086;
          int32_t _M0L6_2atmpS1087;
          moonbit_incref(_M0L6_2aenvS150);
          #line 228 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS150, _M0L3segS152, _M0L1iS151);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 229 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, (moonbit_string_t)moonbit_string_literal_57.data);
          _M0L6_2atmpS1085 = _M0L4codeS154 & 0xff;
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1084 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1085);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_0(_M0L6loggerS147.$1, _M0L6_2atmpS1084);
          if (_M0L6loggerS147.$1) {
            moonbit_incref(_M0L6loggerS147.$1);
          }
          #line 231 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, 125);
          _M0L6_2atmpS1086 = _M0L1iS151 + 1;
          _M0L6_2atmpS1087 = _M0L1iS151 + 1;
          _M0L1iS151 = _M0L6_2atmpS1086;
          _M0L3segS152 = _M0L6_2atmpS1087;
          goto _2afor_153;
        } else {
          int32_t _M0L6_2atmpS1088 = _M0L1iS151 + 1;
          int32_t _tmp_2091 = _M0L3segS152;
          _M0L1iS151 = _M0L6_2atmpS1088;
          _M0L3segS152 = _tmp_2091;
          goto _2afor_153;
        }
        break;
      }
    }
    goto joinlet_2090;
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
    _M0L6_2atmpS1073 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS156);
    if (_M0L6loggerS147.$1) {
      moonbit_incref(_M0L6loggerS147.$1);
    }
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS147.$0->$method_3(_M0L6loggerS147.$1, _M0L6_2atmpS1073);
    _M0L6_2atmpS1074 = _M0L1iS151 + 1;
    _M0L6_2atmpS1075 = _M0L1iS151 + 1;
    _M0L1iS151 = _M0L6_2atmpS1074;
    _M0L3segS152 = _M0L6_2atmpS1075;
    continue;
    joinlet_2090:;
    _tmp_2092 = _M0L1iS151;
    _tmp_2093 = _M0L3segS152;
    _M0L1iS151 = _tmp_2092;
    _M0L3segS152 = _tmp_2093;
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
  struct _M0TPB6Logger _M0L8_2afieldS1887;
  int32_t _M0L6_2acntS1971;
  struct _M0TPB6Logger _M0L6loggerS143;
  #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L4selfS141
  = (struct _M0TPC16string10StringView){
    _M0L6_2aenvS142->$1_1, _M0L6_2aenvS142->$1_2, _M0L6_2aenvS142->$1_0
  };
  _M0L8_2afieldS1887
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS142->$0_0, _M0L6_2aenvS142->$0_1
  };
  _M0L6_2acntS1971 = Moonbit_object_header(_M0L6_2aenvS142)->rc;
  if (_M0L6_2acntS1971 > 1) {
    int32_t _M0L11_2anew__cntS1972 = _M0L6_2acntS1971 - 1;
    Moonbit_object_header(_M0L6_2aenvS142)->rc = _M0L11_2anew__cntS1972;
    moonbit_incref(_M0L4selfS141.$0);
    if (_M0L8_2afieldS1887.$1) {
      moonbit_incref(_M0L8_2afieldS1887.$1);
    }
  } else if (_M0L6_2acntS1971 == 1) {
    #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS142);
  }
  _M0L6loggerS143 = _M0L8_2afieldS1887;
  if (_M0L1iS144 > _M0L3segS145) {
    int64_t _M0L6_2atmpS1072 = (int64_t)_M0L1iS144;
    struct _M0TPC16string10StringView _M0L6_2atmpS1071;
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1071
    = _M0MPC16string10StringView11sub_2einner(_M0L4selfS141, _M0L3segS145, _M0L6_2atmpS1072);
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS143.$0->$method_2(_M0L6loggerS143.$1, _M0L6_2atmpS1071);
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
  moonbit_string_t _M0L3strS1068;
  int32_t _M0L5startS1070;
  int32_t _M0L6_2atmpS1069;
  int32_t _result_2094;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1068 = _M0L4selfS139.$0;
  _M0L5startS1070 = _M0L4selfS139.$1;
  _M0L6_2atmpS1069 = _M0L5startS1070 + _M0L5indexS140;
  _result_2094 = _M0L3strS1068[_M0L6_2atmpS1069];
  moonbit_decref(_M0L3strS1068);
  return _result_2094;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView _M0L4selfS132,
  int32_t _M0L5startS138,
  int64_t _M0L3endS134
) {
  moonbit_string_t _M0L3strS1067;
  int32_t _M0L8str__lenS131;
  int32_t _M0L8abs__endS133;
  int32_t _M0L10abs__startS137;
  int32_t _M0L5startS1055;
  int32_t _if__result_2095;
  #line 712 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1067 = _M0L4selfS132.$0;
  _M0L8str__lenS131 = Moonbit_array_length(_M0L3strS1067);
  if (_M0L3endS134 == 4294967296ll) {
    _M0L8abs__endS133 = _M0L4selfS132.$2;
  } else {
    int64_t _M0L7_2aSomeS135 = _M0L3endS134;
    int32_t _M0L6_2aendS136 = (int32_t)_M0L7_2aSomeS135;
    if (_M0L6_2aendS136 < 0) {
      int32_t _M0L3endS1065 = _M0L4selfS132.$2;
      _M0L8abs__endS133 = _M0L3endS1065 + _M0L6_2aendS136;
    } else {
      int32_t _M0L5startS1066 = _M0L4selfS132.$1;
      _M0L8abs__endS133 = _M0L5startS1066 + _M0L6_2aendS136;
    }
  }
  if (_M0L5startS138 < 0) {
    int32_t _M0L3endS1063 = _M0L4selfS132.$2;
    _M0L10abs__startS137 = _M0L3endS1063 + _M0L5startS138;
  } else {
    int32_t _M0L5startS1064 = _M0L4selfS132.$1;
    _M0L10abs__startS137 = _M0L5startS1064 + _M0L5startS138;
  }
  _M0L5startS1055 = _M0L4selfS132.$1;
  if (_M0L10abs__startS137 >= _M0L5startS1055) {
    if (_M0L10abs__startS137 <= _M0L8abs__endS133) {
      int32_t _M0L3endS1054 = _M0L4selfS132.$2;
      _if__result_2095 = _M0L8abs__endS133 <= _M0L3endS1054;
    } else {
      _if__result_2095 = 0;
    }
  } else {
    _if__result_2095 = 0;
  }
  if (_if__result_2095) {
    moonbit_string_t _M0L3strS1062;
    if (_M0L10abs__startS137 < _M0L8str__lenS131) {
      moonbit_string_t _M0L3strS1058 = _M0L4selfS132.$0;
      int32_t _M0L6_2atmpS1057 = _M0L3strS1058[_M0L10abs__startS137];
      int32_t _M0L6_2atmpS1056;
      #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1056
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1057);
      if (!_M0L6_2atmpS1056) {
        
      } else {
        #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L8abs__endS133 < _M0L8str__lenS131) {
      moonbit_string_t _M0L3strS1061 = _M0L4selfS132.$0;
      int32_t _M0L6_2atmpS1060 = _M0L3strS1061[_M0L8abs__endS133];
      int32_t _M0L6_2atmpS1059;
      #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1059
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1060);
      if (!_M0L6_2atmpS1059) {
        
      } else {
        #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    _M0L3strS1062 = _M0L4selfS132.$0;
    return (struct _M0TPC16string10StringView){_M0L10abs__startS137,
                                                 _M0L8abs__endS133,
                                                 _M0L3strS1062};
  } else {
    moonbit_decref(_M0L4selfS132.$0);
    #line 732 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS130
) {
  int32_t _M0L3endS1052;
  int32_t _M0L5startS1053;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1052 = _M0L4selfS130.$2;
  _M0L5startS1053 = _M0L4selfS130.$1;
  moonbit_decref(_M0L4selfS130.$0);
  return _M0L3endS1052 - _M0L5startS1053;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS129) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS128;
  int32_t _M0L6_2atmpS1049;
  int32_t _M0L6_2atmpS1048;
  int32_t _M0L6_2atmpS1051;
  int32_t _M0L6_2atmpS1050;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1047;
  #line 109 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS128 = _M0MPB13StringBuilder11new_2einner(0);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1049 = _M0IPC14byte4BytePB3Div3div(_M0L1bS129, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1048
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1049);
  moonbit_incref(_M0L7_2aselfS128);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS128, _M0L6_2atmpS1048);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1051 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS129, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1050
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1051);
  moonbit_incref(_M0L7_2aselfS128);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS128, _M0L6_2atmpS1050);
  _M0L6_2atmpS1047 = _M0L7_2aselfS128;
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1047);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t _M0L1iS127) {
  #line 110 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS127 < 10) {
    int32_t _M0L6_2atmpS1044;
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1044 = _M0IPC14byte4BytePB3Add3add(_M0L1iS127, 48);
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1044);
  } else {
    int32_t _M0L6_2atmpS1046;
    int32_t _M0L6_2atmpS1045;
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1046 = _M0IPC14byte4BytePB3Add3add(_M0L1iS127, 97);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1045 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1046, 10);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1045);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS125,
  int32_t _M0L4thatS126
) {
  int32_t _M0L6_2atmpS1042;
  int32_t _M0L6_2atmpS1043;
  int32_t _M0L6_2atmpS1041;
  #line 120 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1042 = (int32_t)_M0L4selfS125;
  _M0L6_2atmpS1043 = (int32_t)_M0L4thatS126;
  _M0L6_2atmpS1041 = _M0L6_2atmpS1042 - _M0L6_2atmpS1043;
  return _M0L6_2atmpS1041 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS123,
  int32_t _M0L4thatS124
) {
  int32_t _M0L6_2atmpS1039;
  int32_t _M0L6_2atmpS1040;
  int32_t _M0L6_2atmpS1038;
  #line 67 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1039 = (int32_t)_M0L4selfS123;
  _M0L6_2atmpS1040 = (int32_t)_M0L4thatS124;
  _M0L6_2atmpS1038 = _M0L6_2atmpS1039 % _M0L6_2atmpS1040;
  return _M0L6_2atmpS1038 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS121,
  int32_t _M0L4thatS122
) {
  int32_t _M0L6_2atmpS1036;
  int32_t _M0L6_2atmpS1037;
  int32_t _M0L6_2atmpS1035;
  #line 62 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1036 = (int32_t)_M0L4selfS121;
  _M0L6_2atmpS1037 = (int32_t)_M0L4thatS122;
  _M0L6_2atmpS1035 = _M0L6_2atmpS1036 / _M0L6_2atmpS1037;
  return _M0L6_2atmpS1035 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS119,
  int32_t _M0L4thatS120
) {
  int32_t _M0L6_2atmpS1033;
  int32_t _M0L6_2atmpS1034;
  int32_t _M0L6_2atmpS1032;
  #line 106 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1033 = (int32_t)_M0L4selfS119;
  _M0L6_2atmpS1034 = (int32_t)_M0L4thatS120;
  _M0L6_2atmpS1032 = _M0L6_2atmpS1033 + _M0L6_2atmpS1034;
  return _M0L6_2atmpS1032 & 0xff;
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS113
) {
  int32_t _M0L17codepoint__lengthS112;
  int32_t _M0L6_2atmpS1031;
  moonbit_bytes_t _M0L4dataS114;
  int32_t _M0L1iS115;
  int32_t _M0L12utf16__indexS116;
  #line 102 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS113);
  #line 104 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS112
  = _M0MPC16string6String20char__length_2einner(_M0L1sS113, 0, 4294967296ll);
  _M0L6_2atmpS1031 = _M0L17codepoint__lengthS112 * 4;
  _M0L4dataS114 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1031, 0);
  _M0L1iS115 = 0;
  _M0L12utf16__indexS116 = 0;
  while (1) {
    if (_M0L1iS115 < _M0L17codepoint__lengthS112) {
      int32_t _M0L6_2atmpS1028;
      int32_t _M0L1cS117;
      int32_t _M0L6_2atmpS1029;
      int32_t _M0L6_2atmpS1030;
      moonbit_incref(_M0L1sS113);
      #line 109 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1028
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS113, _M0L12utf16__indexS116);
      _M0L1cS117 = _M0L6_2atmpS1028;
      if (_M0L1cS117 > 65535) {
        int32_t _M0L6_2atmpS996 = _M0L1iS115 * 4;
        int32_t _M0L6_2atmpS998 = _M0L1cS117 & 255;
        int32_t _M0L6_2atmpS997 = _M0L6_2atmpS998 & 0xff;
        int32_t _M0L6_2atmpS1003;
        int32_t _M0L6_2atmpS999;
        int32_t _M0L6_2atmpS1002;
        int32_t _M0L6_2atmpS1001;
        int32_t _M0L6_2atmpS1000;
        int32_t _M0L6_2atmpS1008;
        int32_t _M0L6_2atmpS1004;
        int32_t _M0L6_2atmpS1007;
        int32_t _M0L6_2atmpS1006;
        int32_t _M0L6_2atmpS1005;
        int32_t _M0L6_2atmpS1013;
        int32_t _M0L6_2atmpS1009;
        int32_t _M0L6_2atmpS1012;
        int32_t _M0L6_2atmpS1011;
        int32_t _M0L6_2atmpS1010;
        int32_t _M0L6_2atmpS1014;
        int32_t _M0L6_2atmpS1015;
        if (
          _M0L6_2atmpS996 < 0
          || _M0L6_2atmpS996 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 111 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS996] = _M0L6_2atmpS997;
        _M0L6_2atmpS1003 = _M0L1iS115 * 4;
        _M0L6_2atmpS999 = _M0L6_2atmpS1003 + 1;
        _M0L6_2atmpS1002 = _M0L1cS117 >> 8;
        _M0L6_2atmpS1001 = _M0L6_2atmpS1002 & 255;
        _M0L6_2atmpS1000 = _M0L6_2atmpS1001 & 0xff;
        if (
          _M0L6_2atmpS999 < 0
          || _M0L6_2atmpS999 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 112 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS999] = _M0L6_2atmpS1000;
        _M0L6_2atmpS1008 = _M0L1iS115 * 4;
        _M0L6_2atmpS1004 = _M0L6_2atmpS1008 + 2;
        _M0L6_2atmpS1007 = _M0L1cS117 >> 16;
        _M0L6_2atmpS1006 = _M0L6_2atmpS1007 & 255;
        _M0L6_2atmpS1005 = _M0L6_2atmpS1006 & 0xff;
        if (
          _M0L6_2atmpS1004 < 0
          || _M0L6_2atmpS1004 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 113 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1004] = _M0L6_2atmpS1005;
        _M0L6_2atmpS1013 = _M0L1iS115 * 4;
        _M0L6_2atmpS1009 = _M0L6_2atmpS1013 + 3;
        _M0L6_2atmpS1012 = _M0L1cS117 >> 24;
        _M0L6_2atmpS1011 = _M0L6_2atmpS1012 & 255;
        _M0L6_2atmpS1010 = _M0L6_2atmpS1011 & 0xff;
        if (
          _M0L6_2atmpS1009 < 0
          || _M0L6_2atmpS1009 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 114 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1009] = _M0L6_2atmpS1010;
        _M0L6_2atmpS1014 = _M0L1iS115 + 1;
        _M0L6_2atmpS1015 = _M0L12utf16__indexS116 + 2;
        _M0L1iS115 = _M0L6_2atmpS1014;
        _M0L12utf16__indexS116 = _M0L6_2atmpS1015;
        continue;
      } else {
        int32_t _M0L6_2atmpS1016 = _M0L1iS115 * 4;
        int32_t _M0L6_2atmpS1018 = _M0L1cS117 & 255;
        int32_t _M0L6_2atmpS1017 = _M0L6_2atmpS1018 & 0xff;
        int32_t _M0L6_2atmpS1023;
        int32_t _M0L6_2atmpS1019;
        int32_t _M0L6_2atmpS1022;
        int32_t _M0L6_2atmpS1021;
        int32_t _M0L6_2atmpS1020;
        int32_t _M0L6_2atmpS1025;
        int32_t _M0L6_2atmpS1024;
        int32_t _M0L6_2atmpS1027;
        int32_t _M0L6_2atmpS1026;
        if (
          _M0L6_2atmpS1016 < 0
          || _M0L6_2atmpS1016 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 117 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1016] = _M0L6_2atmpS1017;
        _M0L6_2atmpS1023 = _M0L1iS115 * 4;
        _M0L6_2atmpS1019 = _M0L6_2atmpS1023 + 1;
        _M0L6_2atmpS1022 = _M0L1cS117 >> 8;
        _M0L6_2atmpS1021 = _M0L6_2atmpS1022 & 255;
        _M0L6_2atmpS1020 = _M0L6_2atmpS1021 & 0xff;
        if (
          _M0L6_2atmpS1019 < 0
          || _M0L6_2atmpS1019 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 118 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1019] = _M0L6_2atmpS1020;
        _M0L6_2atmpS1025 = _M0L1iS115 * 4;
        _M0L6_2atmpS1024 = _M0L6_2atmpS1025 + 2;
        if (
          _M0L6_2atmpS1024 < 0
          || _M0L6_2atmpS1024 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 119 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1024] = 0;
        _M0L6_2atmpS1027 = _M0L1iS115 * 4;
        _M0L6_2atmpS1026 = _M0L6_2atmpS1027 + 3;
        if (
          _M0L6_2atmpS1026 < 0
          || _M0L6_2atmpS1026 >= Moonbit_array_length(_M0L4dataS114)
        ) {
          #line 120 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS114[_M0L6_2atmpS1026] = 0;
      }
      _M0L6_2atmpS1029 = _M0L1iS115 + 1;
      _M0L6_2atmpS1030 = _M0L12utf16__indexS116 + 1;
      _M0L1iS115 = _M0L6_2atmpS1029;
      _M0L12utf16__indexS116 = _M0L6_2atmpS1030;
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
    int32_t _M0L6_2atmpS995 = _M0L5indexS110 + 1;
    int32_t _M0L2c2S111 = _M0L4selfS109[_M0L6_2atmpS995];
    int32_t _M0L6_2atmpS993;
    int32_t _M0L6_2atmpS994;
    moonbit_decref(_M0L4selfS109);
    _M0L6_2atmpS993 = (int32_t)_M0L2c1S108;
    _M0L6_2atmpS994 = (int32_t)_M0L2c2S111;
    #line 96 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS993, _M0L6_2atmpS994);
  } else {
    moonbit_decref(_M0L4selfS109);
    #line 98 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S108);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS107) {
  int32_t _M0L6_2atmpS992;
  #line 68 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS992 = (int32_t)_M0L4selfS107;
  return _M0L6_2atmpS992;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS105,
  int32_t _M0L8trailingS106
) {
  int32_t _M0L6_2atmpS991;
  int32_t _M0L6_2atmpS990;
  int32_t _M0L6_2atmpS989;
  int32_t _M0L6_2atmpS988;
  int32_t _M0L6_2atmpS987;
  #line 40 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS991 = _M0L7leadingS105 - 55296;
  _M0L6_2atmpS990 = _M0L6_2atmpS991 * 1024;
  _M0L6_2atmpS989 = _M0L6_2atmpS990 + _M0L8trailingS106;
  _M0L6_2atmpS988 = _M0L6_2atmpS989 - 56320;
  _M0L6_2atmpS987 = _M0L6_2atmpS988 + 65536;
  return _M0L6_2atmpS987;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS98,
  int32_t _M0L13start__offsetS99,
  int64_t _M0L11end__offsetS96
) {
  int32_t _M0L11end__offsetS95;
  int32_t _if__result_2097;
  #line 60 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS96 == 4294967296ll) {
    _M0L11end__offsetS95 = Moonbit_array_length(_M0L4selfS98);
  } else {
    int64_t _M0L7_2aSomeS97 = _M0L11end__offsetS96;
    _M0L11end__offsetS95 = (int32_t)_M0L7_2aSomeS97;
  }
  if (_M0L13start__offsetS99 >= 0) {
    if (_M0L13start__offsetS99 <= _M0L11end__offsetS95) {
      int32_t _M0L6_2atmpS980 = Moonbit_array_length(_M0L4selfS98);
      _if__result_2097 = _M0L11end__offsetS95 <= _M0L6_2atmpS980;
    } else {
      _if__result_2097 = 0;
    }
  } else {
    _if__result_2097 = 0;
  }
  if (_if__result_2097) {
    int32_t _M0L12utf16__indexS100 = _M0L13start__offsetS99;
    int32_t _M0L11char__countS101 = 0;
    while (1) {
      if (_M0L12utf16__indexS100 < _M0L11end__offsetS95) {
        int32_t _M0L2c1S102 = _M0L4selfS98[_M0L12utf16__indexS100];
        int32_t _if__result_2099;
        int32_t _M0L6_2atmpS985;
        int32_t _M0L6_2atmpS986;
        #line 76 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S102)) {
          int32_t _M0L6_2atmpS981 = _M0L12utf16__indexS100 + 1;
          _if__result_2099 = _M0L6_2atmpS981 < _M0L11end__offsetS95;
        } else {
          _if__result_2099 = 0;
        }
        if (_if__result_2099) {
          int32_t _M0L6_2atmpS984 = _M0L12utf16__indexS100 + 1;
          int32_t _M0L2c2S103 = _M0L4selfS98[_M0L6_2atmpS984];
          #line 78 "/Users/user/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S103)) {
            int32_t _M0L6_2atmpS982 = _M0L12utf16__indexS100 + 2;
            int32_t _M0L6_2atmpS983 = _M0L11char__countS101 + 1;
            _M0L12utf16__indexS100 = _M0L6_2atmpS982;
            _M0L11char__countS101 = _M0L6_2atmpS983;
            continue;
          } else {
            #line 81 "/Users/user/.moon/lib/core/builtin/string.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_35.data);
          }
        }
        _M0L6_2atmpS985 = _M0L12utf16__indexS100 + 1;
        _M0L6_2atmpS986 = _M0L11char__countS101 + 1;
        _M0L12utf16__indexS100 = _M0L6_2atmpS985;
        _M0L11char__countS101 = _M0L6_2atmpS986;
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
    return _M0FPC15abort5abortGiE((moonbit_string_t)moonbit_string_literal_58.data);
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
    int32_t _M0L6_2atmpS932 = _M0L3lenS73 - _M0L3remS75;
    if (_M0L1iS76 < _M0L6_2atmpS932) {
      int32_t _M0L6_2atmpS954;
      int32_t _M0L2b0S77;
      int32_t _M0L6_2atmpS953;
      int32_t _M0L6_2atmpS952;
      int32_t _M0L2b1S78;
      int32_t _M0L6_2atmpS951;
      int32_t _M0L6_2atmpS950;
      int32_t _M0L2b2S79;
      int32_t _M0L6_2atmpS949;
      int32_t _M0L6_2atmpS948;
      int32_t _M0L2x0S80;
      int32_t _M0L6_2atmpS947;
      int32_t _M0L6_2atmpS944;
      int32_t _M0L6_2atmpS946;
      int32_t _M0L6_2atmpS945;
      int32_t _M0L6_2atmpS943;
      int32_t _M0L2x1S81;
      int32_t _M0L6_2atmpS942;
      int32_t _M0L6_2atmpS939;
      int32_t _M0L6_2atmpS941;
      int32_t _M0L6_2atmpS940;
      int32_t _M0L6_2atmpS938;
      int32_t _M0L2x2S82;
      int32_t _M0L6_2atmpS937;
      int32_t _M0L2x3S83;
      int32_t _M0L6_2atmpS933;
      int32_t _M0L6_2atmpS934;
      int32_t _M0L6_2atmpS935;
      int32_t _M0L6_2atmpS936;
      int32_t _M0L6_2atmpS955;
      if (_M0L1iS76 < 0 || _M0L1iS76 >= Moonbit_array_length(_M0L4dataS74)) {
        #line 67 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS954 = (int32_t)_M0L4dataS74[_M0L1iS76];
      _M0L2b0S77 = (int32_t)_M0L6_2atmpS954;
      _M0L6_2atmpS953 = _M0L1iS76 + 1;
      if (
        _M0L6_2atmpS953 < 0
        || _M0L6_2atmpS953 >= Moonbit_array_length(_M0L4dataS74)
      ) {
        #line 68 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS952 = (int32_t)_M0L4dataS74[_M0L6_2atmpS953];
      _M0L2b1S78 = (int32_t)_M0L6_2atmpS952;
      _M0L6_2atmpS951 = _M0L1iS76 + 2;
      if (
        _M0L6_2atmpS951 < 0
        || _M0L6_2atmpS951 >= Moonbit_array_length(_M0L4dataS74)
      ) {
        #line 69 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS950 = (int32_t)_M0L4dataS74[_M0L6_2atmpS951];
      _M0L2b2S79 = (int32_t)_M0L6_2atmpS950;
      _M0L6_2atmpS949 = _M0L2b0S77 & 252;
      _M0L6_2atmpS948 = _M0L6_2atmpS949 >> 2;
      if (
        _M0L6_2atmpS948 < 0
        || _M0L6_2atmpS948
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 70 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S80 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS948];
      _M0L6_2atmpS947 = _M0L2b0S77 & 3;
      _M0L6_2atmpS944 = _M0L6_2atmpS947 << 4;
      _M0L6_2atmpS946 = _M0L2b1S78 & 240;
      _M0L6_2atmpS945 = _M0L6_2atmpS946 >> 4;
      _M0L6_2atmpS943 = _M0L6_2atmpS944 | _M0L6_2atmpS945;
      if (
        _M0L6_2atmpS943 < 0
        || _M0L6_2atmpS943
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 71 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S81 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS943];
      _M0L6_2atmpS942 = _M0L2b1S78 & 15;
      _M0L6_2atmpS939 = _M0L6_2atmpS942 << 2;
      _M0L6_2atmpS941 = _M0L2b2S79 & 192;
      _M0L6_2atmpS940 = _M0L6_2atmpS941 >> 6;
      _M0L6_2atmpS938 = _M0L6_2atmpS939 | _M0L6_2atmpS940;
      if (
        _M0L6_2atmpS938 < 0
        || _M0L6_2atmpS938
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 72 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S82 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS938];
      _M0L6_2atmpS937 = _M0L2b2S79 & 63;
      if (
        _M0L6_2atmpS937 < 0
        || _M0L6_2atmpS937
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 73 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S83 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS937];
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS933 = _M0MPC14byte4Byte8to__char(_M0L2x0S80);
      moonbit_incref(_M0L3bufS72);
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS933);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS934 = _M0MPC14byte4Byte8to__char(_M0L2x1S81);
      moonbit_incref(_M0L3bufS72);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS934);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS935 = _M0MPC14byte4Byte8to__char(_M0L2x2S82);
      moonbit_incref(_M0L3bufS72);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS935);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS936 = _M0MPC14byte4Byte8to__char(_M0L2x3S83);
      moonbit_incref(_M0L3bufS72);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS936);
      _M0L6_2atmpS955 = _M0L1iS76 + 3;
      _M0L1iS76 = _M0L6_2atmpS955;
      continue;
    }
    break;
  }
  if (_M0L3remS75 == 1) {
    int32_t _M0L6_2atmpS963 = _M0L3lenS73 - 1;
    int32_t _M0L6_2atmpS962;
    int32_t _M0L2b0S85;
    int32_t _M0L6_2atmpS961;
    int32_t _M0L6_2atmpS960;
    int32_t _M0L2x0S86;
    int32_t _M0L6_2atmpS959;
    int32_t _M0L6_2atmpS958;
    int32_t _M0L2x1S87;
    int32_t _M0L6_2atmpS956;
    int32_t _M0L6_2atmpS957;
    if (
      _M0L6_2atmpS963 < 0
      || _M0L6_2atmpS963 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 80 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS962 = (int32_t)_M0L4dataS74[_M0L6_2atmpS963];
    moonbit_decref(_M0L4dataS74);
    _M0L2b0S85 = (int32_t)_M0L6_2atmpS962;
    _M0L6_2atmpS961 = _M0L2b0S85 & 252;
    _M0L6_2atmpS960 = _M0L6_2atmpS961 >> 2;
    if (
      _M0L6_2atmpS960 < 0
      || _M0L6_2atmpS960
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 81 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S86 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS960];
    _M0L6_2atmpS959 = _M0L2b0S85 & 3;
    _M0L6_2atmpS958 = _M0L6_2atmpS959 << 4;
    if (
      _M0L6_2atmpS958 < 0
      || _M0L6_2atmpS958
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 82 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S87 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS958];
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS956 = _M0MPC14byte4Byte8to__char(_M0L2x0S86);
    moonbit_incref(_M0L3bufS72);
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS956);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS957 = _M0MPC14byte4Byte8to__char(_M0L2x1S87);
    moonbit_incref(_M0L3bufS72);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS957);
    moonbit_incref(_M0L3bufS72);
    #line 85 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
    moonbit_incref(_M0L3bufS72);
    #line 86 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, 61);
  } else if (_M0L3remS75 == 2) {
    int32_t _M0L6_2atmpS979 = _M0L3lenS73 - 2;
    int32_t _M0L6_2atmpS978;
    int32_t _M0L2b0S88;
    int32_t _M0L6_2atmpS977;
    int32_t _M0L6_2atmpS976;
    int32_t _M0L2b1S89;
    int32_t _M0L6_2atmpS975;
    int32_t _M0L6_2atmpS974;
    int32_t _M0L2x0S90;
    int32_t _M0L6_2atmpS973;
    int32_t _M0L6_2atmpS970;
    int32_t _M0L6_2atmpS972;
    int32_t _M0L6_2atmpS971;
    int32_t _M0L6_2atmpS969;
    int32_t _M0L2x1S91;
    int32_t _M0L6_2atmpS968;
    int32_t _M0L6_2atmpS967;
    int32_t _M0L2x2S92;
    int32_t _M0L6_2atmpS964;
    int32_t _M0L6_2atmpS965;
    int32_t _M0L6_2atmpS966;
    if (
      _M0L6_2atmpS979 < 0
      || _M0L6_2atmpS979 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 88 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS978 = (int32_t)_M0L4dataS74[_M0L6_2atmpS979];
    _M0L2b0S88 = (int32_t)_M0L6_2atmpS978;
    _M0L6_2atmpS977 = _M0L3lenS73 - 1;
    if (
      _M0L6_2atmpS977 < 0
      || _M0L6_2atmpS977 >= Moonbit_array_length(_M0L4dataS74)
    ) {
      #line 89 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS976 = (int32_t)_M0L4dataS74[_M0L6_2atmpS977];
    moonbit_decref(_M0L4dataS74);
    _M0L2b1S89 = (int32_t)_M0L6_2atmpS976;
    _M0L6_2atmpS975 = _M0L2b0S88 & 252;
    _M0L6_2atmpS974 = _M0L6_2atmpS975 >> 2;
    if (
      _M0L6_2atmpS974 < 0
      || _M0L6_2atmpS974
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 90 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S90 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS974];
    _M0L6_2atmpS973 = _M0L2b0S88 & 3;
    _M0L6_2atmpS970 = _M0L6_2atmpS973 << 4;
    _M0L6_2atmpS972 = _M0L2b1S89 & 240;
    _M0L6_2atmpS971 = _M0L6_2atmpS972 >> 4;
    _M0L6_2atmpS969 = _M0L6_2atmpS970 | _M0L6_2atmpS971;
    if (
      _M0L6_2atmpS969 < 0
      || _M0L6_2atmpS969
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 91 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S91 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS969];
    _M0L6_2atmpS968 = _M0L2b1S89 & 15;
    _M0L6_2atmpS967 = _M0L6_2atmpS968 << 2;
    if (
      _M0L6_2atmpS967 < 0
      || _M0L6_2atmpS967
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 92 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S92 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS967];
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS964 = _M0MPC14byte4Byte8to__char(_M0L2x0S90);
    moonbit_incref(_M0L3bufS72);
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS964);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS965 = _M0MPC14byte4Byte8to__char(_M0L2x1S91);
    moonbit_incref(_M0L3bufS72);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS965);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS966 = _M0MPC14byte4Byte8to__char(_M0L2x2S92);
    moonbit_incref(_M0L3bufS72);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS72, _M0L6_2atmpS966);
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
    int32_t _M0L3lenS911 = _M0L4selfS70->$1;
    int32_t _M0L6_2atmpS910 = _M0L3lenS911 + 1;
    uint16_t* _M0L4dataS912;
    int32_t _M0L3lenS913;
    int32_t _M0L6_2atmpS914;
    int32_t _M0L3lenS916;
    int32_t _M0L6_2atmpS915;
    moonbit_incref(_M0L4selfS70);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS70, _M0L6_2atmpS910);
    _M0L4dataS912 = _M0L4selfS70->$0;
    _M0L3lenS913 = _M0L4selfS70->$1;
    moonbit_incref(_M0L4dataS912);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS914 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS68);
    if (
      _M0L3lenS913 < 0 || _M0L3lenS913 >= Moonbit_array_length(_M0L4dataS912)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS912[_M0L3lenS913] = _M0L6_2atmpS914;
    moonbit_decref(_M0L4dataS912);
    _M0L3lenS916 = _M0L4selfS70->$1;
    _M0L6_2atmpS915 = _M0L3lenS916 + 1;
    _M0L4selfS70->$1 = _M0L6_2atmpS915;
    moonbit_decref(_M0L4selfS70);
  } else if (_M0L4codeS68 <= 1114111u) {
    int32_t _M0L3lenS918 = _M0L4selfS70->$1;
    int32_t _M0L6_2atmpS917 = _M0L3lenS918 + 2;
    uint32_t _M0L4codeS71;
    uint16_t* _M0L4dataS919;
    int32_t _M0L3lenS920;
    uint32_t _M0L6_2atmpS923;
    uint32_t _M0L6_2atmpS922;
    int32_t _M0L6_2atmpS921;
    uint16_t* _M0L4dataS924;
    int32_t _M0L3lenS929;
    int32_t _M0L6_2atmpS925;
    uint32_t _M0L6_2atmpS928;
    uint32_t _M0L6_2atmpS927;
    int32_t _M0L6_2atmpS926;
    int32_t _M0L3lenS931;
    int32_t _M0L6_2atmpS930;
    moonbit_incref(_M0L4selfS70);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS70, _M0L6_2atmpS917);
    _M0L4codeS71 = _M0L4codeS68 - 65536u;
    _M0L4dataS919 = _M0L4selfS70->$0;
    _M0L3lenS920 = _M0L4selfS70->$1;
    _M0L6_2atmpS923 = _M0L4codeS71 >> 10;
    _M0L6_2atmpS922 = 55296u + _M0L6_2atmpS923;
    moonbit_incref(_M0L4dataS919);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS921 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS922);
    if (
      _M0L3lenS920 < 0 || _M0L3lenS920 >= Moonbit_array_length(_M0L4dataS919)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS919[_M0L3lenS920] = _M0L6_2atmpS921;
    moonbit_decref(_M0L4dataS919);
    _M0L4dataS924 = _M0L4selfS70->$0;
    _M0L3lenS929 = _M0L4selfS70->$1;
    _M0L6_2atmpS925 = _M0L3lenS929 + 1;
    _M0L6_2atmpS928 = _M0L4codeS71 & 1023u;
    _M0L6_2atmpS927 = 56320u + _M0L6_2atmpS928;
    moonbit_incref(_M0L4dataS924);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS926 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS927);
    if (
      _M0L6_2atmpS925 < 0
      || _M0L6_2atmpS925 >= Moonbit_array_length(_M0L4dataS924)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS924[_M0L6_2atmpS925] = _M0L6_2atmpS926;
    moonbit_decref(_M0L4dataS924);
    _M0L3lenS931 = _M0L4selfS70->$1;
    _M0L6_2atmpS930 = _M0L3lenS931 + 2;
    _M0L4selfS70->$1 = _M0L6_2atmpS930;
    moonbit_decref(_M0L4selfS70);
  } else {
    moonbit_decref(_M0L4selfS70);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_59.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS62,
  int32_t _M0L8requiredS63
) {
  uint16_t* _M0L4dataS909;
  int32_t _M0L12current__lenS61;
  int32_t _M0L13enough__spaceS64;
  int32_t _M0L13enough__spaceS65;
  int32_t _M0L6_2atmpS907;
  uint16_t* _M0L9new__dataS67;
  uint16_t* _M0L4dataS905;
  int32_t _M0L3lenS906;
  uint16_t* _M0L6_2aoldS1897;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS909 = _M0L4selfS62->$0;
  _M0L12current__lenS61 = Moonbit_array_length(_M0L4dataS909);
  if (_M0L8requiredS63 <= _M0L12current__lenS61) {
    moonbit_decref(_M0L4selfS62);
    return 0;
  }
  _M0L13enough__spaceS65 = _M0L12current__lenS61;
  while (1) {
    if (_M0L13enough__spaceS65 < _M0L8requiredS63) {
      int32_t _M0L6_2atmpS908 = _M0L13enough__spaceS65 * 2;
      _M0L13enough__spaceS65 = _M0L6_2atmpS908;
      continue;
    } else {
      _M0L13enough__spaceS64 = _M0L13enough__spaceS65;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS907 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS67
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS64, _M0L6_2atmpS907);
  _M0L4dataS905 = _M0L4selfS62->$0;
  _M0L3lenS906 = _M0L4selfS62->$1;
  moonbit_incref(_M0L4dataS905);
  moonbit_incref(_M0L9new__dataS67);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS67, 0, _M0L4dataS905, 0, _M0L3lenS906);
  _M0L6_2aoldS1897 = _M0L4selfS62->$0;
  moonbit_decref(_M0L6_2aoldS1897);
  _M0L4selfS62->$0 = _M0L9new__dataS67;
  moonbit_decref(_M0L4selfS62);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS60) {
  int32_t _M0L6_2atmpS904;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS904 = *(int32_t*)&_M0L4selfS60;
  return (uint16_t)_M0L6_2atmpS904;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS903;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS903 = _M0L4selfS59;
  return *(uint32_t*)&_M0L6_2atmpS903;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS57
) {
  int32_t _M0L3lenS895;
  uint16_t* _M0L4dataS897;
  int32_t _M0L6_2atmpS896;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS895 = _M0L4selfS57->$1;
  _M0L4dataS897 = _M0L4selfS57->$0;
  _M0L6_2atmpS896 = Moonbit_array_length(_M0L4dataS897);
  if (_M0L3lenS895 == _M0L6_2atmpS896) {
    uint16_t* _M0L8_2afieldS1900 = _M0L4selfS57->$0;
    int32_t _M0L6_2acntS1973 = Moonbit_object_header(_M0L4selfS57)->rc;
    uint16_t* _M0L4dataS898;
    if (_M0L6_2acntS1973 > 1) {
      int32_t _M0L11_2anew__cntS1974 = _M0L6_2acntS1973 - 1;
      Moonbit_object_header(_M0L4selfS57)->rc = _M0L11_2anew__cntS1974;
      moonbit_incref(_M0L8_2afieldS1900);
    } else if (_M0L6_2acntS1973 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS57);
    }
    _M0L4dataS898 = _M0L8_2afieldS1900;
    return _M0L4dataS898;
  } else {
    int32_t _M0L3lenS901 = _M0L4selfS57->$1;
    int32_t _M0L6_2atmpS902;
    uint16_t* _M0L4dataS58;
    uint16_t* _M0L4dataS899;
    int32_t _M0L3lenS900;
    int32_t _M0L6_2acntS1975;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS902 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS58
    = (uint16_t*)moonbit_make_string(_M0L3lenS901, _M0L6_2atmpS902);
    _M0L4dataS899 = _M0L4selfS57->$0;
    _M0L3lenS900 = _M0L4selfS57->$1;
    _M0L6_2acntS1975 = Moonbit_object_header(_M0L4selfS57)->rc;
    if (_M0L6_2acntS1975 > 1) {
      int32_t _M0L11_2anew__cntS1976 = _M0L6_2acntS1975 - 1;
      Moonbit_object_header(_M0L4selfS57)->rc = _M0L11_2anew__cntS1976;
      moonbit_incref(_M0L4dataS899);
    } else if (_M0L6_2acntS1975 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS57);
    }
    moonbit_incref(_M0L4dataS58);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS58, 0, _M0L4dataS899, 0, _M0L3lenS900);
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
  struct _M0TPB13StringBuilder* _block_2102;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS55 < 1) {
    _M0L7initialS54 = 1;
  } else {
    int32_t _M0L6_2atmpS894 = _M0L10size__hintS55 + 1;
    _M0L7initialS54 = _M0L6_2atmpS894 / 2;
  }
  _M0L4dataS56 = (uint16_t*)moonbit_make_string(_M0L7initialS54, 0);
  _block_2102
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2102)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2102->$0 = _M0L4dataS56;
  _block_2102->$1 = 0;
  return _block_2102;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS53) {
  int32_t _M0L6_2atmpS893;
  #line 1867 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS893 = (int32_t)_M0L4selfS53;
  return _M0L6_2atmpS893;
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
  int32_t _if__result_2103;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS16 == _M0L3srcS17) {
    _if__result_2103 = _M0L11dst__offsetS18 < _M0L11src__offsetS19;
  } else {
    _if__result_2103 = 0;
  }
  if (_if__result_2103) {
    int32_t _M0L1iS20 = 0;
    while (1) {
      if (_M0L1iS20 < _M0L3lenS21) {
        int32_t _M0L6_2atmpS866 = _M0L11dst__offsetS18 + _M0L1iS20;
        int32_t _M0L6_2atmpS868 = _M0L11src__offsetS19 + _M0L1iS20;
        int32_t _M0L6_2atmpS867;
        int32_t _M0L6_2atmpS869;
        if (
          _M0L6_2atmpS868 < 0
          || _M0L6_2atmpS868 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS867 = (int32_t)_M0L3srcS17[_M0L6_2atmpS868];
        if (
          _M0L6_2atmpS866 < 0
          || _M0L6_2atmpS866 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS866] = _M0L6_2atmpS867;
        _M0L6_2atmpS869 = _M0L1iS20 + 1;
        _M0L1iS20 = _M0L6_2atmpS869;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS874 = _M0L3lenS21 - 1;
    int32_t _M0L1iS23 = _M0L6_2atmpS874;
    while (1) {
      if (_M0L1iS23 >= 0) {
        int32_t _M0L6_2atmpS870 = _M0L11dst__offsetS18 + _M0L1iS23;
        int32_t _M0L6_2atmpS872 = _M0L11src__offsetS19 + _M0L1iS23;
        int32_t _M0L6_2atmpS871;
        int32_t _M0L6_2atmpS873;
        if (
          _M0L6_2atmpS872 < 0
          || _M0L6_2atmpS872 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS871 = (int32_t)_M0L3srcS17[_M0L6_2atmpS872];
        if (
          _M0L6_2atmpS870 < 0
          || _M0L6_2atmpS870 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS870] = _M0L6_2atmpS871;
        _M0L6_2atmpS873 = _M0L1iS23 - 1;
        _M0L1iS23 = _M0L6_2atmpS873;
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
  int32_t _if__result_2106;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS25 == _M0L3srcS26) {
    _if__result_2106 = _M0L11dst__offsetS27 < _M0L11src__offsetS28;
  } else {
    _if__result_2106 = 0;
  }
  if (_if__result_2106) {
    int32_t _M0L1iS29 = 0;
    while (1) {
      if (_M0L1iS29 < _M0L3lenS30) {
        int32_t _M0L6_2atmpS875 = _M0L11dst__offsetS27 + _M0L1iS29;
        int32_t _M0L6_2atmpS877 = _M0L11src__offsetS28 + _M0L1iS29;
        moonbit_string_t _M0L6_2atmpS876;
        moonbit_string_t _M0L6_2aoldS1903;
        int32_t _M0L6_2atmpS878;
        if (
          _M0L6_2atmpS877 < 0
          || _M0L6_2atmpS877 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS876 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS877];
        if (
          _M0L6_2atmpS875 < 0
          || _M0L6_2atmpS875 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1903 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS875];
        moonbit_incref(_M0L6_2atmpS876);
        moonbit_decref(_M0L6_2aoldS1903);
        _M0L3dstS25[_M0L6_2atmpS875] = _M0L6_2atmpS876;
        _M0L6_2atmpS878 = _M0L1iS29 + 1;
        _M0L1iS29 = _M0L6_2atmpS878;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS883 = _M0L3lenS30 - 1;
    int32_t _M0L1iS32 = _M0L6_2atmpS883;
    while (1) {
      if (_M0L1iS32 >= 0) {
        int32_t _M0L6_2atmpS879 = _M0L11dst__offsetS27 + _M0L1iS32;
        int32_t _M0L6_2atmpS881 = _M0L11src__offsetS28 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS880;
        moonbit_string_t _M0L6_2aoldS1905;
        int32_t _M0L6_2atmpS882;
        if (
          _M0L6_2atmpS881 < 0
          || _M0L6_2atmpS881 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS880 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS881];
        if (
          _M0L6_2atmpS879 < 0
          || _M0L6_2atmpS879 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1905 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS879];
        moonbit_incref(_M0L6_2atmpS880);
        moonbit_decref(_M0L6_2aoldS1905);
        _M0L3dstS25[_M0L6_2atmpS879] = _M0L6_2atmpS880;
        _M0L6_2atmpS882 = _M0L1iS32 - 1;
        _M0L1iS32 = _M0L6_2atmpS882;
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
  int32_t _if__result_2109;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS34 == _M0L3srcS35) {
    _if__result_2109 = _M0L11dst__offsetS36 < _M0L11src__offsetS37;
  } else {
    _if__result_2109 = 0;
  }
  if (_if__result_2109) {
    int32_t _M0L1iS38 = 0;
    while (1) {
      if (_M0L1iS38 < _M0L3lenS39) {
        int32_t _M0L6_2atmpS884 = _M0L11dst__offsetS36 + _M0L1iS38;
        int32_t _M0L6_2atmpS886 = _M0L11src__offsetS37 + _M0L1iS38;
        struct _M0TUsiE* _M0L6_2atmpS885;
        struct _M0TUsiE* _M0L6_2aoldS1907;
        int32_t _M0L6_2atmpS887;
        if (
          _M0L6_2atmpS886 < 0
          || _M0L6_2atmpS886 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS885 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS886];
        if (
          _M0L6_2atmpS884 < 0
          || _M0L6_2atmpS884 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1907 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS884];
        if (_M0L6_2atmpS885) {
          moonbit_incref(_M0L6_2atmpS885);
        }
        if (_M0L6_2aoldS1907) {
          moonbit_decref(_M0L6_2aoldS1907);
        }
        _M0L3dstS34[_M0L6_2atmpS884] = _M0L6_2atmpS885;
        _M0L6_2atmpS887 = _M0L1iS38 + 1;
        _M0L1iS38 = _M0L6_2atmpS887;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS892 = _M0L3lenS39 - 1;
    int32_t _M0L1iS41 = _M0L6_2atmpS892;
    while (1) {
      if (_M0L1iS41 >= 0) {
        int32_t _M0L6_2atmpS888 = _M0L11dst__offsetS36 + _M0L1iS41;
        int32_t _M0L6_2atmpS890 = _M0L11src__offsetS37 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS889;
        struct _M0TUsiE* _M0L6_2aoldS1909;
        int32_t _M0L6_2atmpS891;
        if (
          _M0L6_2atmpS890 < 0
          || _M0L6_2atmpS890 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS889 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS890];
        if (
          _M0L6_2atmpS888 < 0
          || _M0L6_2atmpS888 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1909 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS888];
        if (_M0L6_2atmpS889) {
          moonbit_incref(_M0L6_2atmpS889);
        }
        if (_M0L6_2aoldS1909) {
          moonbit_decref(_M0L6_2aoldS1909);
        }
        _M0L3dstS34[_M0L6_2atmpS888] = _M0L6_2atmpS889;
        _M0L6_2atmpS891 = _M0L1iS41 - 1;
        _M0L1iS41 = _M0L6_2atmpS891;
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
  uint32_t _M0L3accS865;
  uint32_t _M0L6_2atmpS864;
  #line 236 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS865 = _M0L4selfS14->$0;
  _M0L6_2atmpS864 = _M0L3accS865 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS864;
  #line 238 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS862;
  uint32_t _M0L6_2atmpS863;
  uint32_t _M0L6_2atmpS861;
  uint32_t _M0L6_2atmpS860;
  uint32_t _M0L6_2atmpS859;
  #line 451 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS862 = _M0L4selfS12->$0;
  _M0L6_2atmpS863 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS861 = _M0L3accS862 + _M0L6_2atmpS863;
  #line 452 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS860 = _M0FPB4rotl(_M0L6_2atmpS861, 17);
  _M0L6_2atmpS859 = _M0L6_2atmpS860 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS859;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS856;
  int32_t _M0L6_2atmpS858;
  uint32_t _M0L6_2atmpS857;
  #line 461 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS856 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS858 = 32 - _M0L1rS11;
  _M0L6_2atmpS857 = _M0L1xS10 >> (_M0L6_2atmpS858 & 31);
  return _M0L6_2atmpS856 | _M0L6_2atmpS857;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__5208S6,
  struct _M0TPB6Logger _M0L10_2ax__5209S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS1911;
  int32_t _M0L6_2acntS1977;
  moonbit_string_t _M0L15_2a_2aarg__5210S8;
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__5208S6;
  _M0L8_2afieldS1911 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS1977 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS1977 > 1) {
    int32_t _M0L11_2anew__cntS1978 = _M0L6_2acntS1977 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS1978;
    moonbit_incref(_M0L8_2afieldS1911);
  } else if (_M0L6_2acntS1977 == 1) {
    #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__5210S8 = _M0L8_2afieldS1911;
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_60.data);
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__5209S9, _M0L15_2a_2aarg__5210S8);
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_61.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS807) {
  switch (Moonbit_object_tag(_M0L4_2aeS807)) {
    case 1: {
      moonbit_decref(_M0L4_2aeS807);
      return (moonbit_string_t)moonbit_string_literal_62.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS807);
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS807);
      return (moonbit_string_t)moonbit_string_literal_63.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS807);
      return (moonbit_string_t)moonbit_string_literal_64.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS807);
      return (moonbit_string_t)moonbit_string_literal_65.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS834
) {
  moonbit_string_t _M0L7_2aselfS833 = (moonbit_string_t)_M0L11_2aobj__ptrS834;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS833);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS832,
  struct _M0TPB6Logger _M0L8_2aparamS831
) {
  moonbit_string_t _M0L7_2aselfS830 = (moonbit_string_t)_M0L11_2aobj__ptrS832;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS830, _M0L8_2aparamS831);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS829,
  int32_t _M0L8_2aparamS828
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS827 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS829;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS827, _M0L8_2aparamS828);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS826,
  struct _M0TPC16string10StringView _M0L8_2aparamS825
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS824 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS826;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS824, _M0L8_2aparamS825);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS823,
  moonbit_string_t _M0L8_2aparamS820,
  int32_t _M0L8_2aparamS821,
  int32_t _M0L8_2aparamS822
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS819 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS823;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS819, _M0L8_2aparamS820, _M0L8_2aparamS821, _M0L8_2aparamS822);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS818,
  moonbit_string_t _M0L8_2aparamS817
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS816 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS818;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS816, _M0L8_2aparamS817);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGRPB5ArrayGsEE(
  void* _M0L11_2aobj__ptrS815
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS814 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS815;
  return _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(_M0L7_2aselfS814);
}

int32_t _M0IPC15array5ArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGsE(
  void* _M0L11_2aobj__ptrS813,
  struct _M0TPB6Logger _M0L8_2aparamS812
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS811 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS813;
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L7_2aselfS811, _M0L8_2aparamS812);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS855 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS854;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS850;
  moonbit_string_t* _M0L6_2atmpS853;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS852;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS851;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS733;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS849;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS848;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS847;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS842;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS734;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS846;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS845;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS844;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS843;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS732;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS841;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS840;
  _M0L6_2atmpS855[0] = (moonbit_string_t)moonbit_string_literal_66.data;
  moonbit_incref(_M0FP36mulpjs4mulp32plugin__examples__blackbox__test65____test__6275696c645f77726170706572735f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS854
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS854)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS854->$0
  = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test65____test__6275696c645f77726170706572735f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS854->$1 = _M0L6_2atmpS855;
  _M0L8_2atupleS850
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS850)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS850->$0 = 0;
  _M0L8_2atupleS850->$1 = _M0L8_2atupleS854;
  _M0L6_2atmpS853 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS853[0] = (moonbit_string_t)moonbit_string_literal_67.data;
  moonbit_incref(_M0FP36mulpjs4mulp32plugin__examples__blackbox__test65____test__6275696c645f77726170706572735f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS852
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS852)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS852->$0
  = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test65____test__6275696c645f77726170706572735f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS852->$1 = _M0L6_2atmpS853;
  _M0L8_2atupleS851
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS851)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS851->$0 = 1;
  _M0L8_2atupleS851->$1 = _M0L8_2atupleS852;
  _M0L7_2abindS733
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS733[0] = _M0L8_2atupleS850;
  _M0L7_2abindS733[1] = _M0L8_2atupleS851;
  _M0L6_2atmpS849 = _M0L7_2abindS733;
  _M0L6_2atmpS848
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 2, _M0L6_2atmpS849
  };
  #line 398 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS847
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS848);
  _M0L8_2atupleS842
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS842)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS842->$0 = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L8_2atupleS842->$1 = _M0L6_2atmpS847;
  _M0L7_2abindS734
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS846 = _M0L7_2abindS734;
  _M0L6_2atmpS845
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS846
  };
  #line 402 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS844
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS845);
  _M0L8_2atupleS843
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS843)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS843->$0 = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L8_2atupleS843->$1 = _M0L6_2atmpS844;
  _M0L7_2abindS732
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS732[0] = _M0L8_2atupleS842;
  _M0L7_2abindS732[1] = _M0L8_2atupleS843;
  _M0L6_2atmpS841 = _M0L7_2abindS732;
  _M0L6_2atmpS840
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS841
  };
  #line 397 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp32plugin__examples__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS840);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS839;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS801;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS802;
  int32_t _M0L7_2abindS803;
  int32_t _M0L2__S804;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS839
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS801
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS801)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS801->$0 = _M0L6_2atmpS839;
  _M0L12async__testsS801->$1 = 0;
  #line 441 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS802
  = _M0FP36mulpjs4mulp32plugin__examples__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS803 = _M0L7_2abindS802->$1;
  _M0L2__S804 = 0;
  while (1) {
    if (_M0L2__S804 < _M0L7_2abindS803) {
      struct _M0TUsiE** _M0L3bufS838 = _M0L7_2abindS802->$0;
      struct _M0TUsiE* _M0L3argS805 =
        (struct _M0TUsiE*)_M0L3bufS838[_M0L2__S804];
      moonbit_string_t _M0L6_2atmpS835 = _M0L3argS805->$0;
      int32_t _M0L6_2atmpS836 = _M0L3argS805->$1;
      int32_t _M0L6_2atmpS837;
      moonbit_incref(_M0L6_2atmpS835);
      moonbit_incref(_M0L12async__testsS801);
      #line 442 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
      _M0FP36mulpjs4mulp32plugin__examples__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS801, _M0L6_2atmpS835, _M0L6_2atmpS836);
      _M0L6_2atmpS837 = _M0L2__S804 + 1;
      _M0L2__S804 = _M0L6_2atmpS837;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS802);
    }
    break;
  }
  #line 444 "/Users/user/workspace/github/gulp/mulp/plugin_examples/__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP36mulpjs4mulp32plugin__examples__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp32plugin__examples__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS801);
  return 0;
}