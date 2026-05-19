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

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__;

struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942;

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

struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__;

struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp27log__events__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC15error5Error113mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB8MutLocalGiE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder;

struct _M0TPB13SourceLocRepr;

struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError;

struct _M0KTPB4ShowS3Int;

struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart;

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

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp27log__events__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0Y3Int {
  int32_t $0;
  
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

struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
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

struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPB8MutLocalGiE* $1;
  
};

struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop {
  int64_t $1;
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp27log__events__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC15error5Error113mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
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

struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder {
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* $0;
  
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

struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0KTPB4ShowS3Int {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart {
  moonbit_string_t $0;
  
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

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE {
  int32_t $1;
  void** $0;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGbRP36mulpjs4mulp27log__events__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
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

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS951(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS942(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testC2135l430(
  struct _M0TWEu*
);

int32_t _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testC2131l431(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS875(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS870(
  int32_t
);

moonbit_string_t _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S857(
  int32_t,
  moonbit_string_t
);

#define _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp27log__events__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__0(
  
);

moonbit_string_t _M0MP36mulpjs4mulp11log__events16LogEventRecorder6render(
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder*
);

struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0MP36mulpjs4mulp11log__events16LogEventRecorder6events(
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder*
);

int32_t _M0MP36mulpjs4mulp11log__events16LogEventRecorder4emit(
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder*,
  void*
);

struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _M0FP36mulpjs4mulp11log__events25new__log__event__recorder(
  
);

moonbit_string_t _M0FP36mulpjs4mulp11log__events19render__log__events(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*
);

moonbit_string_t _M0MP36mulpjs4mulp11log__events8LogEvent18render__with__time(
  void*,
  moonbit_string_t
);

moonbit_string_t _M0MP36mulpjs4mulp11log__events8LogEvent6render(void*);

void* _M0FP36mulpjs4mulp11log__events11task__error(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP36mulpjs4mulp11log__events10task__stop(moonbit_string_t, int64_t);

void* _M0FP36mulpjs4mulp11log__events11task__start(moonbit_string_t);

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

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1656l680(struct _M0TWEOs*);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

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

int32_t _M0MPC15array5Array4pushGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*,
  void*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*
);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*
);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

void** _M0MPC15array5Array6bufferGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*
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

moonbit_string_t _M0MPC15int645Int6418to__string_2einner(int64_t, int32_t);

int32_t _M0FPB22int64__to__string__dec(uint16_t*, uint64_t, int32_t, int32_t);

int32_t _M0FPB26int64__to__string__generic(
  uint16_t*,
  uint64_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB22int64__to__string__hex(uint16_t*, uint64_t, int32_t, int32_t);

int32_t _M0FPB14radix__count64(uint64_t, int32_t);

int32_t _M0FPB12hex__count64(uint64_t);

int32_t _M0FPB12dec__count64(uint64_t);

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

uint64_t _M0MPC13int3Int10to__uint64(int32_t);

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

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp11log__events8LogEventE(
  void**,
  int32_t,
  void**,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP36mulpjs4mulp11log__events8LogEventEE(
  void**,
  int32_t,
  void**,
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

moonbit_string_t _M0IPC13int3IntPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    83, 116, 97, 114, 116, 105, 110, 103, 32, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    58, 51, 45, 52, 58, 49, 48, 51, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
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
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    98, 111, 111, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    101, 118, 101, 110, 116, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    91, 50, 48, 58, 53, 56, 58, 53, 53, 93, 32, 83, 116, 97, 114, 116, 
    105, 110, 103, 32, 39, 98, 117, 105, 108, 100, 39, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 
    58, 49, 49, 45, 51, 58, 51, 57, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    57, 58, 51, 45, 51, 50, 58, 52, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 41), 
    108, 111, 103, 32, 101, 118, 101, 110, 116, 115, 32, 114, 101, 110, 
    100, 101, 114, 32, 116, 97, 115, 107, 32, 108, 105, 102, 101, 99, 
    121, 99, 108, 101, 32, 109, 101, 115, 115, 97, 103, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 
    58, 51, 45, 51, 58, 55, 56, 64, 109, 117, 108, 112, 106, 115, 47, 
    109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    49, 58, 51, 45, 52, 52, 58, 52, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 
    58, 52, 57, 45, 51, 58, 55, 55, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    48, 58, 51, 45, 52, 48, 58, 53, 49, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_12 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    108, 111, 103, 32, 101, 118, 101, 110, 116, 32, 114, 101, 99, 111, 
    114, 100, 101, 114, 32, 112, 114, 101, 115, 101, 114, 118, 101, 115, 
    32, 115, 112, 97, 114, 107, 108, 101, 115, 32, 115, 116, 121, 108, 
    101, 32, 101, 118, 101, 110, 116, 32, 111, 114, 100, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 
    58, 53, 55, 45, 53, 58, 56, 55, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_66 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    101, 118, 101, 110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    91, 109, 117, 108, 112, 93, 32, 70, 105, 110, 105, 115, 104, 101, 
    100, 32, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    123, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    91, 109, 117, 108, 112, 93, 32, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    39, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    58, 49, 49, 45, 52, 58, 53, 53, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    54, 58, 53, 45, 50, 54, 58, 54, 57, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_76 =
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
} const moonbit_string_literal_90 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_51 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    51, 58, 49, 51, 45, 50, 51, 58, 52, 53, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[36]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 35), 
    91, 109, 117, 108, 112, 93, 32, 70, 105, 110, 105, 115, 104, 101, 
    100, 32, 39, 98, 117, 105, 108, 100, 39, 32, 97, 102, 116, 101, 114, 
    32, 49, 50, 32, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_50 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    91, 109, 117, 108, 112, 93, 32, 83, 116, 97, 114, 116, 105, 110, 
    103, 32, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    91, 50, 48, 58, 53, 56, 58, 53, 55, 93, 32, 39, 98, 117, 105, 108, 
    100, 39, 32, 101, 114, 114, 111, 114, 101, 100, 58, 32, 98, 111, 
    111, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_18 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    50, 58, 53, 45, 50, 50, 58, 53, 51, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    51, 58, 49, 51, 45, 52, 51, 58, 55, 55, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    108, 111, 103, 32, 101, 118, 101, 110, 116, 115, 32, 114, 101, 110, 
    100, 101, 114, 32, 103, 117, 108, 112, 32, 99, 108, 105, 32, 116, 
    105, 109, 101, 115, 116, 97, 109, 112, 32, 115, 116, 121, 108, 101, 
    32, 109, 101, 115, 115, 97, 103, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_79 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    48, 58, 49, 49, 45, 52, 48, 58, 51, 55, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
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
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 
    58, 49, 49, 45, 53, 58, 52, 55, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
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
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    39, 32, 97, 102, 116, 101, 114, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    91, 109, 117, 108, 112, 93, 32, 39, 98, 117, 105, 108, 100, 39, 32, 
    101, 114, 114, 111, 114, 101, 100, 58, 32, 98, 111, 111, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    50, 48, 58, 53, 56, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    53, 58, 51, 45, 50, 56, 58, 52, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    108, 111, 103, 32, 101, 118, 101, 110, 116, 115, 32, 114, 101, 110, 
    100, 101, 114, 32, 111, 114, 100, 101, 114, 101, 100, 32, 101, 118, 
    101, 110, 116, 32, 115, 116, 114, 101, 97, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    99, 108, 101, 97, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 
    49, 58, 53, 45, 49, 52, 58, 55, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 
    48, 58, 51, 45, 49, 54, 58, 52, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[105]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 104), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 108, 111, 
    103, 95, 101, 118, 101, 110, 116, 115, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 
    105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 109, 117, 
    108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 108, 111, 103, 95, 
    101, 118, 101, 110, 116, 115, 34, 44, 32, 34, 102, 105, 108, 101, 
    110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    48, 58, 52, 55, 45, 52, 48, 58, 53, 48, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    58, 54, 53, 45, 52, 58, 49, 48, 50, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 
    53, 58, 49, 51, 45, 49, 53, 58, 55, 55, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    50, 48, 58, 53, 56, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    39, 32, 101, 114, 114, 111, 114, 101, 100, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 
    58, 51, 45, 53, 58, 56, 56, 64, 109, 117, 108, 112, 106, 115, 47, 
    109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    55, 58, 49, 51, 45, 50, 55, 58, 53, 52, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 
    49, 58, 51, 45, 50, 52, 58, 52, 64, 109, 117, 108, 112, 106, 115, 
    47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 
    49, 58, 49, 51, 45, 51, 49, 58, 52, 55, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    50, 48, 58, 53, 56, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_68 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    70, 105, 110, 105, 115, 104, 101, 100, 32, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    91, 50, 48, 58, 53, 56, 58, 53, 54, 93, 32, 70, 105, 110, 105, 115, 
    104, 101, 100, 32, 39, 98, 117, 105, 108, 100, 39, 32, 97, 102, 116, 
    101, 114, 32, 49, 50, 32, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    91, 109, 117, 108, 112, 93, 32, 83, 116, 97, 114, 116, 105, 110, 
    103, 32, 39, 99, 108, 101, 97, 110, 39, 46, 46, 46, 10, 91, 109, 
    117, 108, 112, 93, 32, 70, 105, 110, 105, 115, 104, 101, 100, 32, 
    39, 99, 108, 101, 97, 110, 39, 32, 97, 102, 116, 101, 114, 32, 49, 
    32, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    93, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[103]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 102), 
    109, 117, 108, 112, 106, 115, 47, 109, 117, 108, 112, 47, 108, 111, 
    103, 95, 101, 118, 101, 110, 116, 115, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 
    114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 
    48, 58, 53, 45, 51, 48, 58, 54, 49, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    32, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_58 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 39, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    108, 111, 103, 95, 101, 118, 101, 110, 116, 115, 47, 101, 118, 101, 
    110, 116, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 
    50, 58, 53, 45, 52, 50, 58, 50, 50, 64, 109, 117, 108, 112, 106, 
    115, 47, 109, 117, 108, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 26), 
    91, 109, 117, 108, 112, 93, 32, 83, 116, 97, 114, 116, 105, 110, 
    103, 32, 39, 98, 117, 105, 108, 100, 39, 46, 46, 46, 0
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

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS951$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS951
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__3_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__1_2edyncall$closure.data;

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

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP36mulpjs4mulp27log__events__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2169
) {
  return _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2168
) {
  return _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2167
) {
  return _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test53____test__6576656e74735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2166
) {
  return _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__0();
}

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS972,
  moonbit_string_t _M0L8filenameS947,
  int32_t _M0L5indexS950
) {
  struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942* _closure_2406;
  struct _M0TWssbEu* _M0L14handle__resultS942;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS951;
  void* _M0L11_2atry__errS966;
  struct moonbit_result_0 _tmp_2408;
  int32_t _handle__error__result_2409;
  int32_t _M0L6_2atmpS2154;
  void* _M0L3errS967;
  moonbit_string_t _M0L4nameS969;
  struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS970;
  moonbit_string_t _M0L8_2afieldS2170;
  int32_t _M0L6_2acntS2328;
  moonbit_string_t _M0L7_2anameS971;
  #line 529 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS947);
  _closure_2406
  = (struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942*)moonbit_malloc(sizeof(struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942));
  Moonbit_object_header(_closure_2406)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942, $1) >> 2, 1, 0);
  _closure_2406->code
  = &_M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS942;
  _closure_2406->$0 = _M0L5indexS950;
  _closure_2406->$1 = _M0L8filenameS947;
  _M0L14handle__resultS942 = (struct _M0TWssbEu*)_closure_2406;
  _M0L17error__to__stringS951
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS951$closure.data;
  moonbit_incref(_M0L12async__testsS972);
  moonbit_incref(_M0L17error__to__stringS951);
  moonbit_incref(_M0L8filenameS947);
  moonbit_incref(_M0L14handle__resultS942);
  #line 563 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _tmp_2408
  = _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS972, _M0L8filenameS947, _M0L5indexS950, _M0L14handle__resultS942, _M0L17error__to__stringS951);
  if (_tmp_2408.tag) {
    int32_t const _M0L5_2aokS2163 = _tmp_2408.data.ok;
    _handle__error__result_2409 = _M0L5_2aokS2163;
  } else {
    void* const _M0L6_2aerrS2164 = _tmp_2408.data.err;
    moonbit_decref(_M0L12async__testsS972);
    moonbit_decref(_M0L17error__to__stringS951);
    moonbit_decref(_M0L8filenameS947);
    _M0L11_2atry__errS966 = _M0L6_2aerrS2164;
    goto join_965;
  }
  if (_handle__error__result_2409) {
    moonbit_decref(_M0L12async__testsS972);
    moonbit_decref(_M0L17error__to__stringS951);
    moonbit_decref(_M0L8filenameS947);
    _M0L6_2atmpS2154 = 1;
  } else {
    struct moonbit_result_0 _tmp_2410;
    int32_t _handle__error__result_2411;
    moonbit_incref(_M0L12async__testsS972);
    moonbit_incref(_M0L17error__to__stringS951);
    moonbit_incref(_M0L8filenameS947);
    moonbit_incref(_M0L14handle__resultS942);
    #line 566 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    _tmp_2410
    = _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS972, _M0L8filenameS947, _M0L5indexS950, _M0L14handle__resultS942, _M0L17error__to__stringS951);
    if (_tmp_2410.tag) {
      int32_t const _M0L5_2aokS2161 = _tmp_2410.data.ok;
      _handle__error__result_2411 = _M0L5_2aokS2161;
    } else {
      void* const _M0L6_2aerrS2162 = _tmp_2410.data.err;
      moonbit_decref(_M0L12async__testsS972);
      moonbit_decref(_M0L17error__to__stringS951);
      moonbit_decref(_M0L8filenameS947);
      _M0L11_2atry__errS966 = _M0L6_2aerrS2162;
      goto join_965;
    }
    if (_handle__error__result_2411) {
      moonbit_decref(_M0L12async__testsS972);
      moonbit_decref(_M0L17error__to__stringS951);
      moonbit_decref(_M0L8filenameS947);
      _M0L6_2atmpS2154 = 1;
    } else {
      struct moonbit_result_0 _tmp_2412;
      int32_t _handle__error__result_2413;
      moonbit_incref(_M0L12async__testsS972);
      moonbit_incref(_M0L17error__to__stringS951);
      moonbit_incref(_M0L8filenameS947);
      moonbit_incref(_M0L14handle__resultS942);
      #line 569 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _tmp_2412
      = _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS972, _M0L8filenameS947, _M0L5indexS950, _M0L14handle__resultS942, _M0L17error__to__stringS951);
      if (_tmp_2412.tag) {
        int32_t const _M0L5_2aokS2159 = _tmp_2412.data.ok;
        _handle__error__result_2413 = _M0L5_2aokS2159;
      } else {
        void* const _M0L6_2aerrS2160 = _tmp_2412.data.err;
        moonbit_decref(_M0L12async__testsS972);
        moonbit_decref(_M0L17error__to__stringS951);
        moonbit_decref(_M0L8filenameS947);
        _M0L11_2atry__errS966 = _M0L6_2aerrS2160;
        goto join_965;
      }
      if (_handle__error__result_2413) {
        moonbit_decref(_M0L12async__testsS972);
        moonbit_decref(_M0L17error__to__stringS951);
        moonbit_decref(_M0L8filenameS947);
        _M0L6_2atmpS2154 = 1;
      } else {
        struct moonbit_result_0 _tmp_2414;
        int32_t _handle__error__result_2415;
        moonbit_incref(_M0L12async__testsS972);
        moonbit_incref(_M0L17error__to__stringS951);
        moonbit_incref(_M0L8filenameS947);
        moonbit_incref(_M0L14handle__resultS942);
        #line 572 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        _tmp_2414
        = _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS972, _M0L8filenameS947, _M0L5indexS950, _M0L14handle__resultS942, _M0L17error__to__stringS951);
        if (_tmp_2414.tag) {
          int32_t const _M0L5_2aokS2157 = _tmp_2414.data.ok;
          _handle__error__result_2415 = _M0L5_2aokS2157;
        } else {
          void* const _M0L6_2aerrS2158 = _tmp_2414.data.err;
          moonbit_decref(_M0L12async__testsS972);
          moonbit_decref(_M0L17error__to__stringS951);
          moonbit_decref(_M0L8filenameS947);
          _M0L11_2atry__errS966 = _M0L6_2aerrS2158;
          goto join_965;
        }
        if (_handle__error__result_2415) {
          moonbit_decref(_M0L12async__testsS972);
          moonbit_decref(_M0L17error__to__stringS951);
          moonbit_decref(_M0L8filenameS947);
          _M0L6_2atmpS2154 = 1;
        } else {
          struct moonbit_result_0 _tmp_2416;
          moonbit_incref(_M0L14handle__resultS942);
          #line 575 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
          _tmp_2416
          = _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS972, _M0L8filenameS947, _M0L5indexS950, _M0L14handle__resultS942, _M0L17error__to__stringS951);
          if (_tmp_2416.tag) {
            int32_t const _M0L5_2aokS2155 = _tmp_2416.data.ok;
            _M0L6_2atmpS2154 = _M0L5_2aokS2155;
          } else {
            void* const _M0L6_2aerrS2156 = _tmp_2416.data.err;
            _M0L11_2atry__errS966 = _M0L6_2aerrS2156;
            goto join_965;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2154) {
    void* _M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2165 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2165)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2165)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS966
    = _M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2165;
    goto join_965;
  } else {
    moonbit_decref(_M0L14handle__resultS942);
  }
  goto joinlet_2407;
  join_965:;
  _M0L3errS967 = _M0L11_2atry__errS966;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS970
  = (struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS967;
  _M0L8_2afieldS2170 = _M0L36_2aMoonBitTestDriverInternalSkipTestS970->$0;
  _M0L6_2acntS2328
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS970)->rc;
  if (_M0L6_2acntS2328 > 1) {
    int32_t _M0L11_2anew__cntS2329 = _M0L6_2acntS2328 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS970)->rc
    = _M0L11_2anew__cntS2329;
    moonbit_incref(_M0L8_2afieldS2170);
  } else if (_M0L6_2acntS2328 == 1) {
    #line 582 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS970);
  }
  _M0L7_2anameS971 = _M0L8_2afieldS2170;
  _M0L4nameS969 = _M0L7_2anameS971;
  goto join_968;
  goto joinlet_2417;
  join_968:;
  #line 583 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS942(_M0L14handle__resultS942, _M0L4nameS969, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2417:;
  joinlet_2407:;
  return 0;
}

moonbit_string_t _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS951(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2153,
  void* _M0L3errS952
) {
  void* _M0L1eS954;
  moonbit_string_t _M0L1eS956;
  #line 552 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2153);
  switch (Moonbit_object_tag(_M0L3errS952)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS957 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS952;
      moonbit_string_t _M0L8_2afieldS2171 = _M0L10_2aFailureS957->$0;
      int32_t _M0L6_2acntS2330 =
        Moonbit_object_header(_M0L10_2aFailureS957)->rc;
      moonbit_string_t _M0L4_2aeS958;
      if (_M0L6_2acntS2330 > 1) {
        int32_t _M0L11_2anew__cntS2331 = _M0L6_2acntS2330 - 1;
        Moonbit_object_header(_M0L10_2aFailureS957)->rc
        = _M0L11_2anew__cntS2331;
        moonbit_incref(_M0L8_2afieldS2171);
      } else if (_M0L6_2acntS2330 == 1) {
        #line 553 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS957);
      }
      _M0L4_2aeS958 = _M0L8_2afieldS2171;
      _M0L1eS956 = _M0L4_2aeS958;
      goto join_955;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS959 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS952;
      moonbit_string_t _M0L8_2afieldS2172 = _M0L15_2aInspectErrorS959->$0;
      int32_t _M0L6_2acntS2332 =
        Moonbit_object_header(_M0L15_2aInspectErrorS959)->rc;
      moonbit_string_t _M0L4_2aeS960;
      if (_M0L6_2acntS2332 > 1) {
        int32_t _M0L11_2anew__cntS2333 = _M0L6_2acntS2332 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS959)->rc
        = _M0L11_2anew__cntS2333;
        moonbit_incref(_M0L8_2afieldS2172);
      } else if (_M0L6_2acntS2332 == 1) {
        #line 553 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS959);
      }
      _M0L4_2aeS960 = _M0L8_2afieldS2172;
      _M0L1eS956 = _M0L4_2aeS960;
      goto join_955;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS961 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS952;
      moonbit_string_t _M0L8_2afieldS2173 = _M0L16_2aSnapshotErrorS961->$0;
      int32_t _M0L6_2acntS2334 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS961)->rc;
      moonbit_string_t _M0L4_2aeS962;
      if (_M0L6_2acntS2334 > 1) {
        int32_t _M0L11_2anew__cntS2335 = _M0L6_2acntS2334 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS961)->rc
        = _M0L11_2anew__cntS2335;
        moonbit_incref(_M0L8_2afieldS2173);
      } else if (_M0L6_2acntS2334 == 1) {
        #line 553 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS961);
      }
      _M0L4_2aeS962 = _M0L8_2afieldS2173;
      _M0L1eS956 = _M0L4_2aeS962;
      goto join_955;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error113mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS963 =
        (struct _M0DTPC15error5Error113mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS952;
      moonbit_string_t _M0L8_2afieldS2174 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS963->$0;
      int32_t _M0L6_2acntS2336 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS963)->rc;
      moonbit_string_t _M0L4_2aeS964;
      if (_M0L6_2acntS2336 > 1) {
        int32_t _M0L11_2anew__cntS2337 = _M0L6_2acntS2336 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS963)->rc
        = _M0L11_2anew__cntS2337;
        moonbit_incref(_M0L8_2afieldS2174);
      } else if (_M0L6_2acntS2336 == 1) {
        #line 553 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS963);
      }
      _M0L4_2aeS964 = _M0L8_2afieldS2174;
      _M0L1eS956 = _M0L4_2aeS964;
      goto join_955;
      break;
    }
    default: {
      _M0L1eS954 = _M0L3errS952;
      goto join_953;
      break;
    }
  }
  join_955:;
  return _M0L1eS956;
  join_953:;
  #line 558 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS954);
}

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS942(
  struct _M0TWssbEu* _M0L6_2aenvS2139,
  moonbit_string_t _M0L8testnameS943,
  moonbit_string_t _M0L7messageS944,
  int32_t _M0L7skippedS945
) {
  struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942* _M0L14_2acasted__envS2140;
  moonbit_string_t _M0L8filenameS947;
  int32_t _M0L5indexS950;
  int32_t _M0L6_2acntS2338;
  int32_t _if__result_2420;
  moonbit_string_t _M0L10file__nameS946;
  moonbit_string_t _M0L10test__nameS948;
  moonbit_string_t _M0L7messageS949;
  moonbit_string_t _M0L6_2atmpS2152;
  moonbit_string_t _M0L6_2atmpS2151;
  moonbit_string_t _M0L6_2atmpS2149;
  moonbit_string_t _M0L6_2atmpS2150;
  moonbit_string_t _M0L6_2atmpS2148;
  moonbit_string_t _M0L6_2atmpS2146;
  moonbit_string_t _M0L6_2atmpS2147;
  moonbit_string_t _M0L6_2atmpS2145;
  moonbit_string_t _M0L6_2atmpS2143;
  moonbit_string_t _M0L6_2atmpS2144;
  moonbit_string_t _M0L6_2atmpS2142;
  moonbit_string_t _M0L6_2atmpS2141;
  #line 536 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2140
  = (struct _M0R116_24mulpjs_2fmulp_2flog__events__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c942*)_M0L6_2aenvS2139;
  _M0L8filenameS947 = _M0L14_2acasted__envS2140->$1;
  _M0L5indexS950 = _M0L14_2acasted__envS2140->$0;
  _M0L6_2acntS2338 = Moonbit_object_header(_M0L14_2acasted__envS2140)->rc;
  if (_M0L6_2acntS2338 > 1) {
    int32_t _M0L11_2anew__cntS2339 = _M0L6_2acntS2338 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2140)->rc
    = _M0L11_2anew__cntS2339;
    moonbit_incref(_M0L8filenameS947);
  } else if (_M0L6_2acntS2338 == 1) {
    #line 536 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2140);
  }
  if (!_M0L7skippedS945) {
    _if__result_2420 = 1;
  } else {
    _if__result_2420 = 0;
  }
  if (_if__result_2420) {
    
  }
  #line 542 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS946
  = _M0MPC16string6String14escape_2einner(_M0L8filenameS947, 1);
  #line 543 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS948
  = _M0MPC16string6String14escape_2einner(_M0L8testnameS943, 1);
  #line 544 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS949
  = _M0MPC16string6String14escape_2einner(_M0L7messageS944, 1);
  #line 545 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 547 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2152
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS946);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2151
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2152);
  moonbit_decref(_M0L6_2atmpS2152);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2149
  = moonbit_add_string(_M0L6_2atmpS2151, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2151);
  #line 547 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2150 = _M0IPC13int3IntPB4Show10to__string(_M0L5indexS950);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2148 = moonbit_add_string(_M0L6_2atmpS2149, _M0L6_2atmpS2150);
  moonbit_decref(_M0L6_2atmpS2150);
  moonbit_decref(_M0L6_2atmpS2149);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2146
  = moonbit_add_string(_M0L6_2atmpS2148, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2148);
  #line 547 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2147
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS948);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2145 = moonbit_add_string(_M0L6_2atmpS2146, _M0L6_2atmpS2147);
  moonbit_decref(_M0L6_2atmpS2147);
  moonbit_decref(_M0L6_2atmpS2146);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2143
  = moonbit_add_string(_M0L6_2atmpS2145, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2145);
  #line 547 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2144
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS949);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2142 = moonbit_add_string(_M0L6_2atmpS2143, _M0L6_2atmpS2144);
  moonbit_decref(_M0L6_2atmpS2144);
  moonbit_decref(_M0L6_2atmpS2143);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2141
  = moonbit_add_string(_M0L6_2atmpS2142, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2142);
  #line 546 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2141);
  #line 549 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S941,
  moonbit_string_t _M0L8filenameS938,
  int32_t _M0L5indexS932,
  struct _M0TWssbEu* _M0L14handle__resultS928,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS930
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS908;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS937;
  struct _M0TWEuQRPC15error5Error* _M0L1fS910;
  moonbit_string_t* _M0L5attrsS911;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS931;
  moonbit_string_t _M0L4nameS914;
  moonbit_string_t _M0L4nameS912;
  int32_t _M0L6_2atmpS2138;
  struct _M0TWEOs* _M0L5_2aitS916;
  struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__* _closure_2429;
  struct _M0TWEu* _M0L6_2atmpS2129;
  struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__* _closure_2430;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2130;
  struct moonbit_result_0 _result_2431;
  #line 410 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S941);
  moonbit_incref(_M0FP36mulpjs4mulp27log__events__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 417 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS937
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP36mulpjs4mulp27log__events__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS938);
  if (_M0L7_2abindS937 == 0) {
    struct moonbit_result_0 _result_2422;
    if (_M0L7_2abindS937) {
      moonbit_decref(_M0L7_2abindS937);
    }
    moonbit_decref(_M0L17error__to__stringS930);
    moonbit_decref(_M0L14handle__resultS928);
    _result_2422.tag = 1;
    _result_2422.data.ok = 0;
    return _result_2422;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS939 =
      _M0L7_2abindS937;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS940 =
      _M0L7_2aSomeS939;
    _M0L10index__mapS908 = _M0L13_2aindex__mapS940;
    goto join_907;
  }
  join_907:;
  #line 419 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS931
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS908, _M0L5indexS932);
  if (_M0L7_2abindS931 == 0) {
    struct moonbit_result_0 _result_2424;
    if (_M0L7_2abindS931) {
      moonbit_decref(_M0L7_2abindS931);
    }
    moonbit_decref(_M0L17error__to__stringS930);
    moonbit_decref(_M0L14handle__resultS928);
    _result_2424.tag = 1;
    _result_2424.data.ok = 0;
    return _result_2424;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS933 = _M0L7_2abindS931;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS934 = _M0L7_2aSomeS933;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS935 = _M0L4_2axS934->$0;
    moonbit_string_t* _M0L8_2afieldS2177 = _M0L4_2axS934->$1;
    int32_t _M0L6_2acntS2340 = Moonbit_object_header(_M0L4_2axS934)->rc;
    moonbit_string_t* _M0L8_2aattrsS936;
    if (_M0L6_2acntS2340 > 1) {
      int32_t _M0L11_2anew__cntS2341 = _M0L6_2acntS2340 - 1;
      Moonbit_object_header(_M0L4_2axS934)->rc = _M0L11_2anew__cntS2341;
      moonbit_incref(_M0L8_2afieldS2177);
      moonbit_incref(_M0L4_2afS935);
    } else if (_M0L6_2acntS2340 == 1) {
      #line 417 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS934);
    }
    _M0L8_2aattrsS936 = _M0L8_2afieldS2177;
    _M0L1fS910 = _M0L4_2afS935;
    _M0L5attrsS911 = _M0L8_2aattrsS936;
    goto join_909;
  }
  join_909:;
  _M0L6_2atmpS2138 = Moonbit_array_length(_M0L5attrsS911);
  if (_M0L6_2atmpS2138 >= 1) {
    moonbit_string_t _M0L7_2anameS915 = (moonbit_string_t)_M0L5attrsS911[0];
    moonbit_incref(_M0L7_2anameS915);
    _M0L4nameS914 = _M0L7_2anameS915;
    goto join_913;
  } else {
    _M0L4nameS912 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_2425;
  join_913:;
  _M0L4nameS912 = _M0L4nameS914;
  joinlet_2425:;
  #line 420 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS916 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS911);
  while (1) {
    moonbit_string_t _M0L4attrS918;
    moonbit_string_t _M0L7_2abindS925;
    int32_t _M0L6_2atmpS2122;
    int64_t _M0L6_2atmpS2121;
    moonbit_incref(_M0L5_2aitS916);
    #line 422 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS925 = _M0MPB4Iter4nextGsE(_M0L5_2aitS916);
    if (_M0L7_2abindS925 == 0) {
      if (_M0L7_2abindS925) {
        moonbit_decref(_M0L7_2abindS925);
      }
      moonbit_decref(_M0L5_2aitS916);
    } else {
      moonbit_string_t _M0L7_2aSomeS926 = _M0L7_2abindS925;
      moonbit_string_t _M0L7_2aattrS927 = _M0L7_2aSomeS926;
      _M0L4attrS918 = _M0L7_2aattrS927;
      goto join_917;
    }
    goto joinlet_2427;
    join_917:;
    _M0L6_2atmpS2122 = Moonbit_array_length(_M0L4attrS918);
    _M0L6_2atmpS2121 = (int64_t)_M0L6_2atmpS2122;
    moonbit_incref(_M0L4attrS918);
    #line 423 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS918, 5, 0, _M0L6_2atmpS2121)
    ) {
      int32_t _M0L6_2atmpS2128 = _M0L4attrS918[0];
      int32_t _M0L4_2axS919 = _M0L6_2atmpS2128;
      if (_M0L4_2axS919 == 112) {
        int32_t _M0L6_2atmpS2127 = _M0L4attrS918[1];
        int32_t _M0L4_2axS920 = _M0L6_2atmpS2127;
        if (_M0L4_2axS920 == 97) {
          int32_t _M0L6_2atmpS2126 = _M0L4attrS918[2];
          int32_t _M0L4_2axS921 = _M0L6_2atmpS2126;
          if (_M0L4_2axS921 == 110) {
            int32_t _M0L6_2atmpS2125 = _M0L4attrS918[3];
            int32_t _M0L4_2axS922 = _M0L6_2atmpS2125;
            if (_M0L4_2axS922 == 105) {
              int32_t _M0L6_2atmpS2124 = _M0L4attrS918[4];
              int32_t _M0L4_2axS923;
              moonbit_decref(_M0L4attrS918);
              _M0L4_2axS923 = _M0L6_2atmpS2124;
              if (_M0L4_2axS923 == 99) {
                void* _M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2123;
                struct moonbit_result_0 _result_2428;
                moonbit_decref(_M0L17error__to__stringS930);
                moonbit_decref(_M0L14handle__resultS928);
                moonbit_decref(_M0L5_2aitS916);
                moonbit_decref(_M0L1fS910);
                _M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2123
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2123)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2123)->$0
                = _M0L4nameS912;
                _result_2428.tag = 0;
                _result_2428.data.err
                = _M0L115mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2123;
                return _result_2428;
              }
            } else {
              moonbit_decref(_M0L4attrS918);
            }
          } else {
            moonbit_decref(_M0L4attrS918);
          }
        } else {
          moonbit_decref(_M0L4attrS918);
        }
      } else {
        moonbit_decref(_M0L4attrS918);
      }
    } else {
      moonbit_decref(_M0L4attrS918);
    }
    continue;
    joinlet_2427:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS928);
  moonbit_incref(_M0L4nameS912);
  _closure_2429
  = (struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__*)moonbit_malloc(sizeof(struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__));
  Moonbit_object_header(_closure_2429)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__, $0) >> 2, 2, 0);
  _closure_2429->code
  = &_M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testC2135l430;
  _closure_2429->$0 = _M0L14handle__resultS928;
  _closure_2429->$1 = _M0L4nameS912;
  _M0L6_2atmpS2129 = (struct _M0TWEu*)_closure_2429;
  _closure_2430
  = (struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__*)moonbit_malloc(sizeof(struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__));
  Moonbit_object_header(_closure_2430)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__, $0) >> 2, 3, 0);
  _closure_2430->code
  = &_M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testC2131l431;
  _closure_2430->$0 = _M0L17error__to__stringS930;
  _closure_2430->$1 = _M0L14handle__resultS928;
  _closure_2430->$2 = _M0L4nameS912;
  _M0L6_2atmpS2130 = (struct _M0TWRPC15error5ErrorEu*)_closure_2430;
  #line 428 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp27log__events__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS910, _M0L6_2atmpS2129, _M0L6_2atmpS2130);
  _result_2431.tag = 1;
  _result_2431.data.ok = 1;
  return _result_2431;
}

int32_t _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testC2135l430(
  struct _M0TWEu* _M0L6_2aenvS2136
) {
  struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__* _M0L14_2acasted__envS2137;
  moonbit_string_t _M0L4nameS912;
  struct _M0TWssbEu* _M0L8_2afieldS2179;
  int32_t _M0L6_2acntS2342;
  struct _M0TWssbEu* _M0L14handle__resultS928;
  #line 430 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2137
  = (struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2135__l430__*)_M0L6_2aenvS2136;
  _M0L4nameS912 = _M0L14_2acasted__envS2137->$1;
  _M0L8_2afieldS2179 = _M0L14_2acasted__envS2137->$0;
  _M0L6_2acntS2342 = Moonbit_object_header(_M0L14_2acasted__envS2137)->rc;
  if (_M0L6_2acntS2342 > 1) {
    int32_t _M0L11_2anew__cntS2343 = _M0L6_2acntS2342 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2137)->rc
    = _M0L11_2anew__cntS2343;
    moonbit_incref(_M0L4nameS912);
    moonbit_incref(_M0L8_2afieldS2179);
  } else if (_M0L6_2acntS2342 == 1) {
    #line 430 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2137);
  }
  _M0L14handle__resultS928 = _M0L8_2afieldS2179;
  #line 430 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS928->code(_M0L14handle__resultS928, _M0L4nameS912, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP36mulpjs4mulp27log__events__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testC2131l431(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2132,
  void* _M0L3errS929
) {
  struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__* _M0L14_2acasted__envS2133;
  moonbit_string_t _M0L4nameS912;
  struct _M0TWssbEu* _M0L14handle__resultS928;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2181;
  int32_t _M0L6_2acntS2344;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS930;
  moonbit_string_t _M0L6_2atmpS2134;
  #line 431 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2133
  = (struct _M0R203_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40mulpjs_2fmulp_2flog__events__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2131__l431__*)_M0L6_2aenvS2132;
  _M0L4nameS912 = _M0L14_2acasted__envS2133->$2;
  _M0L14handle__resultS928 = _M0L14_2acasted__envS2133->$1;
  _M0L8_2afieldS2181 = _M0L14_2acasted__envS2133->$0;
  _M0L6_2acntS2344 = Moonbit_object_header(_M0L14_2acasted__envS2133)->rc;
  if (_M0L6_2acntS2344 > 1) {
    int32_t _M0L11_2anew__cntS2345 = _M0L6_2acntS2344 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2133)->rc
    = _M0L11_2anew__cntS2345;
    moonbit_incref(_M0L4nameS912);
    moonbit_incref(_M0L14handle__resultS928);
    moonbit_incref(_M0L8_2afieldS2181);
  } else if (_M0L6_2acntS2344 == 1) {
    #line 431 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2133);
  }
  _M0L17error__to__stringS930 = _M0L8_2afieldS2181;
  #line 431 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2134
  = _M0L17error__to__stringS930->code(_M0L17error__to__stringS930, _M0L3errS929);
  #line 431 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS928->code(_M0L14handle__resultS928, _M0L4nameS912, _M0L6_2atmpS2134, 0);
  return 0;
}

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS902,
  struct _M0TWEu* _M0L6on__okS903,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS900
) {
  void* _M0L11_2atry__errS898;
  struct moonbit_result_0 _tmp_2433;
  void* _M0L3errS899;
  #line 375 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  #line 382 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _tmp_2433 = _M0L1fS902->code(_M0L1fS902);
  if (_tmp_2433.tag) {
    int32_t const _M0L5_2aokS2119 = _tmp_2433.data.ok;
    moonbit_decref(_M0L7on__errS900);
  } else {
    void* const _M0L6_2aerrS2120 = _tmp_2433.data.err;
    moonbit_decref(_M0L6on__okS903);
    _M0L11_2atry__errS898 = _M0L6_2aerrS2120;
    goto join_897;
  }
  #line 382 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS903->code(_M0L6on__okS903);
  goto joinlet_2432;
  join_897:;
  _M0L3errS899 = _M0L11_2atry__errS898;
  #line 383 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS900->code(_M0L7on__errS900, _M0L3errS899);
  joinlet_2432:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S857;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS870;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS875;
  struct _M0TUsiE** _M0L6_2atmpS2118;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS882;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS883;
  moonbit_string_t _M0L6_2atmpS2117;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS884;
  int32_t _M0L7_2abindS885;
  int32_t _M0L2__S886;
  #line 193 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S857 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS870
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS875 = 0;
  _M0L6_2atmpS2118 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS882
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS882)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS882->$0 = _M0L6_2atmpS2118;
  _M0L16file__and__indexS882->$1 = 0;
  #line 282 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS883
  = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS870(_M0L57moonbit__test__driver__internal__get__cli__args__internalS870);
  #line 284 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2117 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS883, 1);
  #line 283 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS884
  = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS875(_M0L51moonbit__test__driver__internal__split__mbt__stringS875, _M0L6_2atmpS2117, 47);
  _M0L7_2abindS885 = _M0L10test__argsS884->$1;
  _M0L2__S886 = 0;
  while (1) {
    if (_M0L2__S886 < _M0L7_2abindS885) {
      moonbit_string_t* _M0L3bufS2116 = _M0L10test__argsS884->$0;
      moonbit_string_t _M0L3argS887 =
        (moonbit_string_t)_M0L3bufS2116[_M0L2__S886];
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS888;
      moonbit_string_t _M0L4fileS889;
      moonbit_string_t _M0L5rangeS890;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS891;
      moonbit_string_t _M0L6_2atmpS2114;
      int32_t _M0L5startS892;
      moonbit_string_t _M0L6_2atmpS2113;
      int32_t _M0L3endS893;
      int32_t _M0L1iS894;
      int32_t _M0L6_2atmpS2115;
      moonbit_incref(_M0L3argS887);
      #line 288 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS888
      = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS875(_M0L51moonbit__test__driver__internal__split__mbt__stringS875, _M0L3argS887, 58);
      moonbit_incref(_M0L16file__and__rangeS888);
      #line 289 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS889
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS888, 0);
      #line 290 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS890
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS888, 1);
      #line 291 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS891
      = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS875(_M0L51moonbit__test__driver__internal__split__mbt__stringS875, _M0L5rangeS890, 45);
      moonbit_incref(_M0L15start__and__endS891);
      #line 294 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2114
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS891, 0);
      #line 294 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L5startS892
      = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S857(_M0L45moonbit__test__driver__internal__parse__int__S857, _M0L6_2atmpS2114);
      #line 295 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2113
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS891, 1);
      #line 295 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L3endS893
      = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S857(_M0L45moonbit__test__driver__internal__parse__int__S857, _M0L6_2atmpS2113);
      _M0L1iS894 = _M0L5startS892;
      while (1) {
        if (_M0L1iS894 < _M0L3endS893) {
          struct _M0TUsiE* _M0L8_2atupleS2111;
          int32_t _M0L6_2atmpS2112;
          moonbit_incref(_M0L4fileS889);
          _M0L8_2atupleS2111
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2111)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2111->$0 = _M0L4fileS889;
          _M0L8_2atupleS2111->$1 = _M0L1iS894;
          moonbit_incref(_M0L16file__and__indexS882);
          #line 297 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS882, _M0L8_2atupleS2111);
          _M0L6_2atmpS2112 = _M0L1iS894 + 1;
          _M0L1iS894 = _M0L6_2atmpS2112;
          continue;
        } else {
          moonbit_decref(_M0L4fileS889);
        }
        break;
      }
      _M0L6_2atmpS2115 = _M0L2__S886 + 1;
      _M0L2__S886 = _M0L6_2atmpS2115;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS884);
    }
    break;
  }
  return _M0L16file__and__indexS882;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS875(
  int32_t _M0L6_2aenvS2092,
  moonbit_string_t _M0L1sS876,
  int32_t _M0L3sepS877
) {
  moonbit_string_t* _M0L6_2atmpS2110;
  struct _M0TPB5ArrayGsE* _M0L3resS878;
  struct _M0TPB8MutLocalGiE* _M0L1iS879;
  struct _M0TPB8MutLocalGiE* _M0L5startS880;
  int32_t _M0L3valS2105;
  int32_t _M0L6_2atmpS2106;
  #line 261 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2110 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS878
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS878)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS878->$0 = _M0L6_2atmpS2110;
  _M0L3resS878->$1 = 0;
  _M0L1iS879
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS879)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS879->$0 = 0;
  _M0L5startS880
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS880)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L5startS880->$0 = 0;
  while (1) {
    int32_t _M0L3valS2093 = _M0L1iS879->$0;
    int32_t _M0L6_2atmpS2094 = Moonbit_array_length(_M0L1sS876);
    if (_M0L3valS2093 < _M0L6_2atmpS2094) {
      int32_t _M0L3valS2097 = _M0L1iS879->$0;
      int32_t _M0L6_2atmpS2096;
      int32_t _M0L6_2atmpS2095;
      int32_t _M0L3valS2104;
      int32_t _M0L6_2atmpS2103;
      if (
        _M0L3valS2097 < 0
        || _M0L3valS2097 >= Moonbit_array_length(_M0L1sS876)
      ) {
        #line 269 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2096 = _M0L1sS876[_M0L3valS2097];
      _M0L6_2atmpS2095 = _M0L6_2atmpS2096;
      if (_M0L6_2atmpS2095 == _M0L3sepS877) {
        int32_t _M0L3valS2099 = _M0L5startS880->$0;
        int32_t _M0L3valS2100 = _M0L1iS879->$0;
        moonbit_string_t _M0L6_2atmpS2098;
        int32_t _M0L3valS2102;
        int32_t _M0L6_2atmpS2101;
        moonbit_incref(_M0L1sS876);
        #line 270 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS2098
        = _M0MPC16string6String17unsafe__substring(_M0L1sS876, _M0L3valS2099, _M0L3valS2100);
        moonbit_incref(_M0L3resS878);
        #line 270 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS878, _M0L6_2atmpS2098);
        _M0L3valS2102 = _M0L1iS879->$0;
        _M0L6_2atmpS2101 = _M0L3valS2102 + 1;
        _M0L5startS880->$0 = _M0L6_2atmpS2101;
      }
      _M0L3valS2104 = _M0L1iS879->$0;
      _M0L6_2atmpS2103 = _M0L3valS2104 + 1;
      _M0L1iS879->$0 = _M0L6_2atmpS2103;
      continue;
    } else {
      moonbit_decref(_M0L1iS879);
    }
    break;
  }
  _M0L3valS2105 = _M0L5startS880->$0;
  _M0L6_2atmpS2106 = Moonbit_array_length(_M0L1sS876);
  if (_M0L3valS2105 < _M0L6_2atmpS2106) {
    int32_t _M0L3valS2108 = _M0L5startS880->$0;
    int32_t _M0L6_2atmpS2109;
    moonbit_string_t _M0L6_2atmpS2107;
    moonbit_decref(_M0L5startS880);
    _M0L6_2atmpS2109 = Moonbit_array_length(_M0L1sS876);
    #line 276 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS2107
    = _M0MPC16string6String17unsafe__substring(_M0L1sS876, _M0L3valS2108, _M0L6_2atmpS2109);
    moonbit_incref(_M0L3resS878);
    #line 276 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS878, _M0L6_2atmpS2107);
  } else {
    moonbit_decref(_M0L5startS880);
    moonbit_decref(_M0L1sS876);
  }
  return _M0L3resS878;
}

struct _M0TPB5ArrayGsE* _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS870(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863
) {
  moonbit_bytes_t* _M0L3tmpS871;
  int32_t _M0L6_2atmpS2091;
  struct _M0TPB5ArrayGsE* _M0L3resS872;
  int32_t _M0L1iS873;
  #line 250 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  #line 253 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS871
  = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2091 = Moonbit_array_length(_M0L3tmpS871);
  #line 254 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS872 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2091);
  _M0L1iS873 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2087 = Moonbit_array_length(_M0L3tmpS871);
    if (_M0L1iS873 < _M0L6_2atmpS2087) {
      moonbit_bytes_t _M0L6_2atmpS2089;
      moonbit_string_t _M0L6_2atmpS2088;
      int32_t _M0L6_2atmpS2090;
      if (_M0L1iS873 < 0 || _M0L1iS873 >= Moonbit_array_length(_M0L3tmpS871)) {
        #line 256 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2089 = (moonbit_bytes_t)_M0L3tmpS871[_M0L1iS873];
      moonbit_incref(_M0L6_2atmpS2089);
      #line 256 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2088
      = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863, _M0L6_2atmpS2089);
      moonbit_incref(_M0L3resS872);
      #line 256 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS872, _M0L6_2atmpS2088);
      _M0L6_2atmpS2090 = _M0L1iS873 + 1;
      _M0L1iS873 = _M0L6_2atmpS2090;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS871);
    }
    break;
  }
  return _M0L3resS872;
}

moonbit_string_t _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS863(
  int32_t _M0L6_2aenvS2001,
  moonbit_bytes_t _M0L5bytesS864
) {
  struct _M0TPB13StringBuilder* _M0L3resS865;
  int32_t _M0L3lenS866;
  struct _M0TPB8MutLocalGiE* _M0L1iS867;
  #line 206 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  #line 209 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS865 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS866 = Moonbit_array_length(_M0L5bytesS864);
  _M0L1iS867
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS867)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS867->$0 = 0;
  while (1) {
    int32_t _M0L3valS2002 = _M0L1iS867->$0;
    if (_M0L3valS2002 < _M0L3lenS866) {
      int32_t _M0L3valS2086 = _M0L1iS867->$0;
      int32_t _M0L6_2atmpS2085;
      int32_t _M0L6_2atmpS2084;
      struct _M0TPB8MutLocalGiE* _M0L1cS868;
      int32_t _M0L3valS2003;
      if (
        _M0L3valS2086 < 0
        || _M0L3valS2086 >= Moonbit_array_length(_M0L5bytesS864)
      ) {
        #line 213 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2085 = _M0L5bytesS864[_M0L3valS2086];
      _M0L6_2atmpS2084 = (int32_t)_M0L6_2atmpS2085;
      _M0L1cS868
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L1cS868)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
      _M0L1cS868->$0 = _M0L6_2atmpS2084;
      _M0L3valS2003 = _M0L1cS868->$0;
      if (_M0L3valS2003 < 128) {
        int32_t _M0L3valS2005 = _M0L1cS868->$0;
        int32_t _M0L6_2atmpS2004;
        int32_t _M0L3valS2007;
        int32_t _M0L6_2atmpS2006;
        moonbit_decref(_M0L1cS868);
        _M0L6_2atmpS2004 = _M0L3valS2005;
        moonbit_incref(_M0L3resS865);
        #line 215 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS865, _M0L6_2atmpS2004);
        _M0L3valS2007 = _M0L1iS867->$0;
        _M0L6_2atmpS2006 = _M0L3valS2007 + 1;
        _M0L1iS867->$0 = _M0L6_2atmpS2006;
      } else {
        int32_t _M0L3valS2008 = _M0L1cS868->$0;
        if (_M0L3valS2008 < 224) {
          int32_t _M0L3valS2010 = _M0L1iS867->$0;
          int32_t _M0L6_2atmpS2009 = _M0L3valS2010 + 1;
          int32_t _M0L3valS2019;
          int32_t _M0L6_2atmpS2018;
          int32_t _M0L6_2atmpS2012;
          int32_t _M0L3valS2017;
          int32_t _M0L6_2atmpS2016;
          int32_t _M0L6_2atmpS2015;
          int32_t _M0L6_2atmpS2014;
          int32_t _M0L6_2atmpS2013;
          int32_t _M0L6_2atmpS2011;
          int32_t _M0L3valS2021;
          int32_t _M0L6_2atmpS2020;
          int32_t _M0L3valS2023;
          int32_t _M0L6_2atmpS2022;
          if (_M0L6_2atmpS2009 >= _M0L3lenS866) {
            moonbit_decref(_M0L1cS868);
            moonbit_decref(_M0L1iS867);
            moonbit_decref(_M0L5bytesS864);
            break;
          }
          _M0L3valS2019 = _M0L1cS868->$0;
          _M0L6_2atmpS2018 = _M0L3valS2019 & 31;
          _M0L6_2atmpS2012 = _M0L6_2atmpS2018 << 6;
          _M0L3valS2017 = _M0L1iS867->$0;
          _M0L6_2atmpS2016 = _M0L3valS2017 + 1;
          if (
            _M0L6_2atmpS2016 < 0
            || _M0L6_2atmpS2016 >= Moonbit_array_length(_M0L5bytesS864)
          ) {
            #line 221 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2015 = _M0L5bytesS864[_M0L6_2atmpS2016];
          _M0L6_2atmpS2014 = (int32_t)_M0L6_2atmpS2015;
          _M0L6_2atmpS2013 = _M0L6_2atmpS2014 & 63;
          _M0L6_2atmpS2011 = _M0L6_2atmpS2012 | _M0L6_2atmpS2013;
          _M0L1cS868->$0 = _M0L6_2atmpS2011;
          _M0L3valS2021 = _M0L1cS868->$0;
          moonbit_decref(_M0L1cS868);
          _M0L6_2atmpS2020 = _M0L3valS2021;
          moonbit_incref(_M0L3resS865);
          #line 222 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS865, _M0L6_2atmpS2020);
          _M0L3valS2023 = _M0L1iS867->$0;
          _M0L6_2atmpS2022 = _M0L3valS2023 + 2;
          _M0L1iS867->$0 = _M0L6_2atmpS2022;
        } else {
          int32_t _M0L3valS2024 = _M0L1cS868->$0;
          if (_M0L3valS2024 < 240) {
            int32_t _M0L3valS2026 = _M0L1iS867->$0;
            int32_t _M0L6_2atmpS2025 = _M0L3valS2026 + 2;
            int32_t _M0L3valS2042;
            int32_t _M0L6_2atmpS2041;
            int32_t _M0L6_2atmpS2034;
            int32_t _M0L3valS2040;
            int32_t _M0L6_2atmpS2039;
            int32_t _M0L6_2atmpS2038;
            int32_t _M0L6_2atmpS2037;
            int32_t _M0L6_2atmpS2036;
            int32_t _M0L6_2atmpS2035;
            int32_t _M0L6_2atmpS2028;
            int32_t _M0L3valS2033;
            int32_t _M0L6_2atmpS2032;
            int32_t _M0L6_2atmpS2031;
            int32_t _M0L6_2atmpS2030;
            int32_t _M0L6_2atmpS2029;
            int32_t _M0L6_2atmpS2027;
            int32_t _M0L3valS2044;
            int32_t _M0L6_2atmpS2043;
            int32_t _M0L3valS2046;
            int32_t _M0L6_2atmpS2045;
            if (_M0L6_2atmpS2025 >= _M0L3lenS866) {
              moonbit_decref(_M0L1cS868);
              moonbit_decref(_M0L1iS867);
              moonbit_decref(_M0L5bytesS864);
              break;
            }
            _M0L3valS2042 = _M0L1cS868->$0;
            _M0L6_2atmpS2041 = _M0L3valS2042 & 15;
            _M0L6_2atmpS2034 = _M0L6_2atmpS2041 << 12;
            _M0L3valS2040 = _M0L1iS867->$0;
            _M0L6_2atmpS2039 = _M0L3valS2040 + 1;
            if (
              _M0L6_2atmpS2039 < 0
              || _M0L6_2atmpS2039 >= Moonbit_array_length(_M0L5bytesS864)
            ) {
              #line 229 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2038 = _M0L5bytesS864[_M0L6_2atmpS2039];
            _M0L6_2atmpS2037 = (int32_t)_M0L6_2atmpS2038;
            _M0L6_2atmpS2036 = _M0L6_2atmpS2037 & 63;
            _M0L6_2atmpS2035 = _M0L6_2atmpS2036 << 6;
            _M0L6_2atmpS2028 = _M0L6_2atmpS2034 | _M0L6_2atmpS2035;
            _M0L3valS2033 = _M0L1iS867->$0;
            _M0L6_2atmpS2032 = _M0L3valS2033 + 2;
            if (
              _M0L6_2atmpS2032 < 0
              || _M0L6_2atmpS2032 >= Moonbit_array_length(_M0L5bytesS864)
            ) {
              #line 230 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2031 = _M0L5bytesS864[_M0L6_2atmpS2032];
            _M0L6_2atmpS2030 = (int32_t)_M0L6_2atmpS2031;
            _M0L6_2atmpS2029 = _M0L6_2atmpS2030 & 63;
            _M0L6_2atmpS2027 = _M0L6_2atmpS2028 | _M0L6_2atmpS2029;
            _M0L1cS868->$0 = _M0L6_2atmpS2027;
            _M0L3valS2044 = _M0L1cS868->$0;
            moonbit_decref(_M0L1cS868);
            _M0L6_2atmpS2043 = _M0L3valS2044;
            moonbit_incref(_M0L3resS865);
            #line 231 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS865, _M0L6_2atmpS2043);
            _M0L3valS2046 = _M0L1iS867->$0;
            _M0L6_2atmpS2045 = _M0L3valS2046 + 3;
            _M0L1iS867->$0 = _M0L6_2atmpS2045;
          } else {
            int32_t _M0L3valS2048 = _M0L1iS867->$0;
            int32_t _M0L6_2atmpS2047 = _M0L3valS2048 + 3;
            int32_t _M0L3valS2071;
            int32_t _M0L6_2atmpS2070;
            int32_t _M0L6_2atmpS2063;
            int32_t _M0L3valS2069;
            int32_t _M0L6_2atmpS2068;
            int32_t _M0L6_2atmpS2067;
            int32_t _M0L6_2atmpS2066;
            int32_t _M0L6_2atmpS2065;
            int32_t _M0L6_2atmpS2064;
            int32_t _M0L6_2atmpS2056;
            int32_t _M0L3valS2062;
            int32_t _M0L6_2atmpS2061;
            int32_t _M0L6_2atmpS2060;
            int32_t _M0L6_2atmpS2059;
            int32_t _M0L6_2atmpS2058;
            int32_t _M0L6_2atmpS2057;
            int32_t _M0L6_2atmpS2050;
            int32_t _M0L3valS2055;
            int32_t _M0L6_2atmpS2054;
            int32_t _M0L6_2atmpS2053;
            int32_t _M0L6_2atmpS2052;
            int32_t _M0L6_2atmpS2051;
            int32_t _M0L6_2atmpS2049;
            int32_t _M0L3valS2073;
            int32_t _M0L6_2atmpS2072;
            int32_t _M0L3valS2077;
            int32_t _M0L6_2atmpS2076;
            int32_t _M0L6_2atmpS2075;
            int32_t _M0L6_2atmpS2074;
            int32_t _M0L3valS2081;
            int32_t _M0L6_2atmpS2080;
            int32_t _M0L6_2atmpS2079;
            int32_t _M0L6_2atmpS2078;
            int32_t _M0L3valS2083;
            int32_t _M0L6_2atmpS2082;
            if (_M0L6_2atmpS2047 >= _M0L3lenS866) {
              moonbit_decref(_M0L1cS868);
              moonbit_decref(_M0L1iS867);
              moonbit_decref(_M0L5bytesS864);
              break;
            }
            _M0L3valS2071 = _M0L1cS868->$0;
            _M0L6_2atmpS2070 = _M0L3valS2071 & 7;
            _M0L6_2atmpS2063 = _M0L6_2atmpS2070 << 18;
            _M0L3valS2069 = _M0L1iS867->$0;
            _M0L6_2atmpS2068 = _M0L3valS2069 + 1;
            if (
              _M0L6_2atmpS2068 < 0
              || _M0L6_2atmpS2068 >= Moonbit_array_length(_M0L5bytesS864)
            ) {
              #line 238 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2067 = _M0L5bytesS864[_M0L6_2atmpS2068];
            _M0L6_2atmpS2066 = (int32_t)_M0L6_2atmpS2067;
            _M0L6_2atmpS2065 = _M0L6_2atmpS2066 & 63;
            _M0L6_2atmpS2064 = _M0L6_2atmpS2065 << 12;
            _M0L6_2atmpS2056 = _M0L6_2atmpS2063 | _M0L6_2atmpS2064;
            _M0L3valS2062 = _M0L1iS867->$0;
            _M0L6_2atmpS2061 = _M0L3valS2062 + 2;
            if (
              _M0L6_2atmpS2061 < 0
              || _M0L6_2atmpS2061 >= Moonbit_array_length(_M0L5bytesS864)
            ) {
              #line 239 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2060 = _M0L5bytesS864[_M0L6_2atmpS2061];
            _M0L6_2atmpS2059 = (int32_t)_M0L6_2atmpS2060;
            _M0L6_2atmpS2058 = _M0L6_2atmpS2059 & 63;
            _M0L6_2atmpS2057 = _M0L6_2atmpS2058 << 6;
            _M0L6_2atmpS2050 = _M0L6_2atmpS2056 | _M0L6_2atmpS2057;
            _M0L3valS2055 = _M0L1iS867->$0;
            _M0L6_2atmpS2054 = _M0L3valS2055 + 3;
            if (
              _M0L6_2atmpS2054 < 0
              || _M0L6_2atmpS2054 >= Moonbit_array_length(_M0L5bytesS864)
            ) {
              #line 240 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2053 = _M0L5bytesS864[_M0L6_2atmpS2054];
            _M0L6_2atmpS2052 = (int32_t)_M0L6_2atmpS2053;
            _M0L6_2atmpS2051 = _M0L6_2atmpS2052 & 63;
            _M0L6_2atmpS2049 = _M0L6_2atmpS2050 | _M0L6_2atmpS2051;
            _M0L1cS868->$0 = _M0L6_2atmpS2049;
            _M0L3valS2073 = _M0L1cS868->$0;
            _M0L6_2atmpS2072 = _M0L3valS2073 - 65536;
            _M0L1cS868->$0 = _M0L6_2atmpS2072;
            _M0L3valS2077 = _M0L1cS868->$0;
            _M0L6_2atmpS2076 = _M0L3valS2077 >> 10;
            _M0L6_2atmpS2075 = _M0L6_2atmpS2076 + 55296;
            _M0L6_2atmpS2074 = _M0L6_2atmpS2075;
            moonbit_incref(_M0L3resS865);
            #line 242 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS865, _M0L6_2atmpS2074);
            _M0L3valS2081 = _M0L1cS868->$0;
            moonbit_decref(_M0L1cS868);
            _M0L6_2atmpS2080 = _M0L3valS2081 & 1023;
            _M0L6_2atmpS2079 = _M0L6_2atmpS2080 + 56320;
            _M0L6_2atmpS2078 = _M0L6_2atmpS2079;
            moonbit_incref(_M0L3resS865);
            #line 243 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS865, _M0L6_2atmpS2078);
            _M0L3valS2083 = _M0L1iS867->$0;
            _M0L6_2atmpS2082 = _M0L3valS2083 + 4;
            _M0L1iS867->$0 = _M0L6_2atmpS2082;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS867);
      moonbit_decref(_M0L5bytesS864);
    }
    break;
  }
  #line 247 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS865);
}

int32_t _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S857(
  int32_t _M0L6_2aenvS1994,
  moonbit_string_t _M0L1sS858
) {
  struct _M0TPB8MutLocalGiE* _M0L3resS859;
  int32_t _M0L3lenS860;
  int32_t _M0L1iS861;
  int32_t _result_2440;
  #line 197 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS859
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L3resS859)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L3resS859->$0 = 0;
  _M0L3lenS860 = Moonbit_array_length(_M0L1sS858);
  _M0L1iS861 = 0;
  while (1) {
    if (_M0L1iS861 < _M0L3lenS860) {
      int32_t _M0L3valS1999 = _M0L3resS859->$0;
      int32_t _M0L6_2atmpS1996 = _M0L3valS1999 * 10;
      int32_t _M0L6_2atmpS1998;
      int32_t _M0L6_2atmpS1997;
      int32_t _M0L6_2atmpS1995;
      int32_t _M0L6_2atmpS2000;
      if (_M0L1iS861 < 0 || _M0L1iS861 >= Moonbit_array_length(_M0L1sS858)) {
        #line 201 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1998 = _M0L1sS858[_M0L1iS861];
      _M0L6_2atmpS1997 = _M0L6_2atmpS1998 - 48;
      _M0L6_2atmpS1995 = _M0L6_2atmpS1996 + _M0L6_2atmpS1997;
      _M0L3resS859->$0 = _M0L6_2atmpS1995;
      _M0L6_2atmpS2000 = _M0L1iS861 + 1;
      _M0L1iS861 = _M0L6_2atmpS2000;
      continue;
    } else {
      moonbit_decref(_M0L1sS858);
    }
    break;
  }
  _result_2440 = _M0L3resS859->$0;
  moonbit_decref(_M0L3resS859);
  return _result_2440;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S837,
  moonbit_string_t _M0L12_2adiscard__S838,
  int32_t _M0L12_2adiscard__S839,
  struct _M0TWssbEu* _M0L12_2adiscard__S840,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S841
) {
  struct moonbit_result_0 _result_2441;
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S841);
  moonbit_decref(_M0L12_2adiscard__S840);
  moonbit_decref(_M0L12_2adiscard__S838);
  moonbit_decref(_M0L12_2adiscard__S837);
  _result_2441.tag = 1;
  _result_2441.data.ok = 0;
  return _result_2441;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S842,
  moonbit_string_t _M0L12_2adiscard__S843,
  int32_t _M0L12_2adiscard__S844,
  struct _M0TWssbEu* _M0L12_2adiscard__S845,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S846
) {
  struct moonbit_result_0 _result_2442;
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S846);
  moonbit_decref(_M0L12_2adiscard__S845);
  moonbit_decref(_M0L12_2adiscard__S843);
  moonbit_decref(_M0L12_2adiscard__S842);
  _result_2442.tag = 1;
  _result_2442.data.ok = 0;
  return _result_2442;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S847,
  moonbit_string_t _M0L12_2adiscard__S848,
  int32_t _M0L12_2adiscard__S849,
  struct _M0TWssbEu* _M0L12_2adiscard__S850,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S851
) {
  struct moonbit_result_0 _result_2443;
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S851);
  moonbit_decref(_M0L12_2adiscard__S850);
  moonbit_decref(_M0L12_2adiscard__S848);
  moonbit_decref(_M0L12_2adiscard__S847);
  _result_2443.tag = 1;
  _result_2443.data.ok = 0;
  return _result_2443;
}

struct moonbit_result_0 _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test21MoonBit__Test__Driver9run__testGRP36mulpjs4mulp27log__events__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S852,
  moonbit_string_t _M0L12_2adiscard__S853,
  int32_t _M0L12_2adiscard__S854,
  struct _M0TWssbEu* _M0L12_2adiscard__S855,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S856
) {
  struct moonbit_result_0 _result_2444;
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S856);
  moonbit_decref(_M0L12_2adiscard__S855);
  moonbit_decref(_M0L12_2adiscard__S853);
  moonbit_decref(_M0L12_2adiscard__S852);
  _result_2444.tag = 1;
  _result_2444.data.ok = 0;
  return _result_2444;
}

int32_t _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp27log__events__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S836
) {
  #line 12 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S836);
  return 0;
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__3(
  
) {
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _M0L8recorderS835;
  void* _M0L6_2atmpS1972;
  void* _M0L6_2atmpS1973;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6_2atmpS1983;
  int32_t _M0L6_2atmpS1981;
  struct _M0Y3Int* _M0L14_2aboxed__selfS1982;
  struct _M0TPB4Show _M0L6_2atmpS1974;
  moonbit_string_t _M0L6_2atmpS1977;
  moonbit_string_t _M0L6_2atmpS1978;
  moonbit_string_t _M0L6_2atmpS1979;
  moonbit_string_t _M0L6_2atmpS1980;
  moonbit_string_t* _M0L6_2atmpS1976;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1975;
  struct moonbit_result_0 _tmp_2445;
  moonbit_string_t _M0L6_2atmpS1993;
  struct _M0TPB4Show _M0L6_2atmpS1986;
  moonbit_string_t _M0L6_2atmpS1989;
  moonbit_string_t _M0L6_2atmpS1990;
  moonbit_string_t _M0L6_2atmpS1991;
  moonbit_string_t _M0L6_2atmpS1992;
  moonbit_string_t* _M0L6_2atmpS1988;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1987;
  #line 36 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  #line 37 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L8recorderS835
  = _M0FP36mulpjs4mulp11log__events25new__log__event__recorder();
  #line 38 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1972
  = _M0FP36mulpjs4mulp11log__events11task__start((moonbit_string_t)moonbit_string_literal_9.data);
  moonbit_incref(_M0L8recorderS835);
  #line 38 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0MP36mulpjs4mulp11log__events16LogEventRecorder4emit(_M0L8recorderS835, _M0L6_2atmpS1972);
  #line 39 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1973
  = _M0FP36mulpjs4mulp11log__events10task__stop((moonbit_string_t)moonbit_string_literal_9.data, 1ll);
  moonbit_incref(_M0L8recorderS835);
  #line 39 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0MP36mulpjs4mulp11log__events16LogEventRecorder4emit(_M0L8recorderS835, _M0L6_2atmpS1973);
  moonbit_incref(_M0L8recorderS835);
  #line 40 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1983
  = _M0MP36mulpjs4mulp11log__events16LogEventRecorder6events(_M0L8recorderS835);
  #line 40 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1981
  = _M0MPC15array5Array6lengthGRP36mulpjs4mulp11log__events8LogEventE(_M0L6_2atmpS1983);
  _M0L14_2aboxed__selfS1982
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS1982)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS1982->$0 = _M0L6_2atmpS1981;
  _M0L6_2atmpS1974
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS1982
  };
  _M0L6_2atmpS1977 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1978 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS1979 = 0;
  _M0L6_2atmpS1980 = 0;
  _M0L6_2atmpS1976 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1976[0] = _M0L6_2atmpS1977;
  _M0L6_2atmpS1976[1] = _M0L6_2atmpS1978;
  _M0L6_2atmpS1976[2] = _M0L6_2atmpS1979;
  _M0L6_2atmpS1976[3] = _M0L6_2atmpS1980;
  _M0L6_2atmpS1975
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1975)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1975->$0 = _M0L6_2atmpS1976;
  _M0L6_2atmpS1975->$1 = 4;
  #line 40 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _tmp_2445
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1974, (moonbit_string_t)moonbit_string_literal_12.data, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS1975);
  if (_tmp_2445.tag) {
    int32_t const _M0L5_2aokS1984 = _tmp_2445.data.ok;
  } else {
    void* const _M0L6_2aerrS1985 = _tmp_2445.data.err;
    struct moonbit_result_0 _result_2446;
    moonbit_decref(_M0L8recorderS835);
    _result_2446.tag = 0;
    _result_2446.data.err = _M0L6_2aerrS1985;
    return _result_2446;
  }
  #line 42 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1993
  = _M0MP36mulpjs4mulp11log__events16LogEventRecorder6render(_M0L8recorderS835);
  _M0L6_2atmpS1986
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1993
  };
  _M0L6_2atmpS1989 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS1990 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS1991 = 0;
  _M0L6_2atmpS1992 = 0;
  _M0L6_2atmpS1988 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1988[0] = _M0L6_2atmpS1989;
  _M0L6_2atmpS1988[1] = _M0L6_2atmpS1990;
  _M0L6_2atmpS1988[2] = _M0L6_2atmpS1991;
  _M0L6_2atmpS1988[3] = _M0L6_2atmpS1992;
  _M0L6_2atmpS1987
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1987)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1987->$0 = _M0L6_2atmpS1988;
  _M0L6_2atmpS1987->$1 = 4;
  #line 41 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1986, (moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS1987);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__2(
  
) {
  void* _M0L6_2atmpS1949;
  moonbit_string_t _M0L6_2atmpS1948;
  struct _M0TPB4Show _M0L6_2atmpS1941;
  moonbit_string_t _M0L6_2atmpS1944;
  moonbit_string_t _M0L6_2atmpS1945;
  moonbit_string_t _M0L6_2atmpS1946;
  moonbit_string_t _M0L6_2atmpS1947;
  moonbit_string_t* _M0L6_2atmpS1943;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1942;
  struct moonbit_result_0 _tmp_2447;
  void* _M0L6_2atmpS1960;
  moonbit_string_t _M0L6_2atmpS1959;
  struct _M0TPB4Show _M0L6_2atmpS1952;
  moonbit_string_t _M0L6_2atmpS1955;
  moonbit_string_t _M0L6_2atmpS1956;
  moonbit_string_t _M0L6_2atmpS1957;
  moonbit_string_t _M0L6_2atmpS1958;
  moonbit_string_t* _M0L6_2atmpS1954;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1953;
  struct moonbit_result_0 _tmp_2449;
  void* _M0L6_2atmpS1971;
  moonbit_string_t _M0L6_2atmpS1970;
  struct _M0TPB4Show _M0L6_2atmpS1963;
  moonbit_string_t _M0L6_2atmpS1966;
  moonbit_string_t _M0L6_2atmpS1967;
  moonbit_string_t _M0L6_2atmpS1968;
  moonbit_string_t _M0L6_2atmpS1969;
  moonbit_string_t* _M0L6_2atmpS1965;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1964;
  #line 20 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  #line 22 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1949
  = _M0FP36mulpjs4mulp11log__events11task__start((moonbit_string_t)moonbit_string_literal_18.data);
  #line 22 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1948
  = _M0MP36mulpjs4mulp11log__events8LogEvent18render__with__time(_M0L6_2atmpS1949, (moonbit_string_t)moonbit_string_literal_19.data);
  _M0L6_2atmpS1941
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1948
  };
  _M0L6_2atmpS1944 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS1945 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS1946 = 0;
  _M0L6_2atmpS1947 = 0;
  _M0L6_2atmpS1943 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1943[0] = _M0L6_2atmpS1944;
  _M0L6_2atmpS1943[1] = _M0L6_2atmpS1945;
  _M0L6_2atmpS1943[2] = _M0L6_2atmpS1946;
  _M0L6_2atmpS1943[3] = _M0L6_2atmpS1947;
  _M0L6_2atmpS1942
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1942)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1942->$0 = _M0L6_2atmpS1943;
  _M0L6_2atmpS1942->$1 = 4;
  #line 21 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _tmp_2447
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1941, (moonbit_string_t)moonbit_string_literal_22.data, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS1942);
  if (_tmp_2447.tag) {
    int32_t const _M0L5_2aokS1950 = _tmp_2447.data.ok;
  } else {
    void* const _M0L6_2aerrS1951 = _tmp_2447.data.err;
    struct moonbit_result_0 _result_2448;
    _result_2448.tag = 0;
    _result_2448.data.err = _M0L6_2aerrS1951;
    return _result_2448;
  }
  #line 26 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1960
  = _M0FP36mulpjs4mulp11log__events10task__stop((moonbit_string_t)moonbit_string_literal_18.data, 12ll);
  #line 26 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1959
  = _M0MP36mulpjs4mulp11log__events8LogEvent18render__with__time(_M0L6_2atmpS1960, (moonbit_string_t)moonbit_string_literal_24.data);
  _M0L6_2atmpS1952
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1959
  };
  _M0L6_2atmpS1955 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS1956 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS1957 = 0;
  _M0L6_2atmpS1958 = 0;
  _M0L6_2atmpS1954 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1954[0] = _M0L6_2atmpS1955;
  _M0L6_2atmpS1954[1] = _M0L6_2atmpS1956;
  _M0L6_2atmpS1954[2] = _M0L6_2atmpS1957;
  _M0L6_2atmpS1954[3] = _M0L6_2atmpS1958;
  _M0L6_2atmpS1953
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1953)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1953->$0 = _M0L6_2atmpS1954;
  _M0L6_2atmpS1953->$1 = 4;
  #line 25 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _tmp_2449
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1952, (moonbit_string_t)moonbit_string_literal_27.data, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS1953);
  if (_tmp_2449.tag) {
    int32_t const _M0L5_2aokS1961 = _tmp_2449.data.ok;
  } else {
    void* const _M0L6_2aerrS1962 = _tmp_2449.data.err;
    struct moonbit_result_0 _result_2450;
    _result_2450.tag = 0;
    _result_2450.data.err = _M0L6_2aerrS1962;
    return _result_2450;
  }
  #line 30 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1971
  = _M0FP36mulpjs4mulp11log__events11task__error((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_29.data);
  #line 30 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1970
  = _M0MP36mulpjs4mulp11log__events8LogEvent18render__with__time(_M0L6_2atmpS1971, (moonbit_string_t)moonbit_string_literal_30.data);
  _M0L6_2atmpS1963
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1970
  };
  _M0L6_2atmpS1966 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS1967 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS1968 = 0;
  _M0L6_2atmpS1969 = 0;
  _M0L6_2atmpS1965 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1965[0] = _M0L6_2atmpS1966;
  _M0L6_2atmpS1965[1] = _M0L6_2atmpS1967;
  _M0L6_2atmpS1965[2] = _M0L6_2atmpS1968;
  _M0L6_2atmpS1965[3] = _M0L6_2atmpS1969;
  _M0L6_2atmpS1964
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1964)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1964->$0 = _M0L6_2atmpS1965;
  _M0L6_2atmpS1964->$1 = 4;
  #line 29 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1963, (moonbit_string_t)moonbit_string_literal_33.data, (moonbit_string_t)moonbit_string_literal_34.data, _M0L6_2atmpS1964);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__1(
  
) {
  void* _M0L6_2atmpS1939;
  void* _M0L6_2atmpS1940;
  void** _M0L6_2atmpS1938;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6_2atmpS1937;
  moonbit_string_t _M0L6_2atmpS1936;
  struct _M0TPB4Show _M0L6_2atmpS1929;
  moonbit_string_t _M0L6_2atmpS1932;
  moonbit_string_t _M0L6_2atmpS1933;
  moonbit_string_t _M0L6_2atmpS1934;
  moonbit_string_t _M0L6_2atmpS1935;
  moonbit_string_t* _M0L6_2atmpS1931;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1930;
  #line 9 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  #line 12 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1939
  = _M0FP36mulpjs4mulp11log__events11task__start((moonbit_string_t)moonbit_string_literal_9.data);
  #line 13 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1940
  = _M0FP36mulpjs4mulp11log__events10task__stop((moonbit_string_t)moonbit_string_literal_9.data, 1ll);
  _M0L6_2atmpS1938 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS1938[0] = _M0L6_2atmpS1939;
  _M0L6_2atmpS1938[1] = _M0L6_2atmpS1940;
  _M0L6_2atmpS1937
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE));
  Moonbit_object_header(_M0L6_2atmpS1937)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1937->$0 = _M0L6_2atmpS1938;
  _M0L6_2atmpS1937->$1 = 2;
  #line 11 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1936
  = _M0FP36mulpjs4mulp11log__events19render__log__events(_M0L6_2atmpS1937);
  _M0L6_2atmpS1929
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1936
  };
  _M0L6_2atmpS1932 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS1933 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS1934 = 0;
  _M0L6_2atmpS1935 = 0;
  _M0L6_2atmpS1931 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1931[0] = _M0L6_2atmpS1932;
  _M0L6_2atmpS1931[1] = _M0L6_2atmpS1933;
  _M0L6_2atmpS1931[2] = _M0L6_2atmpS1934;
  _M0L6_2atmpS1931[3] = _M0L6_2atmpS1935;
  _M0L6_2atmpS1930
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1930)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1930->$0 = _M0L6_2atmpS1931;
  _M0L6_2atmpS1930->$1 = 4;
  #line 10 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1929, (moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS1930);
}

struct moonbit_result_0 _M0FP36mulpjs4mulp27log__events__blackbox__test43____test__6576656e74735f746573742e6d6274__0(
  
) {
  void* _M0L6_2atmpS1906;
  moonbit_string_t _M0L6_2atmpS1905;
  struct _M0TPB4Show _M0L6_2atmpS1898;
  moonbit_string_t _M0L6_2atmpS1901;
  moonbit_string_t _M0L6_2atmpS1902;
  moonbit_string_t _M0L6_2atmpS1903;
  moonbit_string_t _M0L6_2atmpS1904;
  moonbit_string_t* _M0L6_2atmpS1900;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1899;
  struct moonbit_result_0 _tmp_2451;
  void* _M0L6_2atmpS1917;
  moonbit_string_t _M0L6_2atmpS1916;
  struct _M0TPB4Show _M0L6_2atmpS1909;
  moonbit_string_t _M0L6_2atmpS1912;
  moonbit_string_t _M0L6_2atmpS1913;
  moonbit_string_t _M0L6_2atmpS1914;
  moonbit_string_t _M0L6_2atmpS1915;
  moonbit_string_t* _M0L6_2atmpS1911;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1910;
  struct moonbit_result_0 _tmp_2453;
  void* _M0L6_2atmpS1928;
  moonbit_string_t _M0L6_2atmpS1927;
  struct _M0TPB4Show _M0L6_2atmpS1920;
  moonbit_string_t _M0L6_2atmpS1923;
  moonbit_string_t _M0L6_2atmpS1924;
  moonbit_string_t _M0L6_2atmpS1925;
  moonbit_string_t _M0L6_2atmpS1926;
  moonbit_string_t* _M0L6_2atmpS1922;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1921;
  #line 2 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  #line 3 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1906
  = _M0FP36mulpjs4mulp11log__events11task__start((moonbit_string_t)moonbit_string_literal_18.data);
  #line 3 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1905
  = _M0MP36mulpjs4mulp11log__events8LogEvent6render(_M0L6_2atmpS1906);
  _M0L6_2atmpS1898
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1905
  };
  _M0L6_2atmpS1901 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS1902 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS1903 = 0;
  _M0L6_2atmpS1904 = 0;
  _M0L6_2atmpS1900 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1900[0] = _M0L6_2atmpS1901;
  _M0L6_2atmpS1900[1] = _M0L6_2atmpS1902;
  _M0L6_2atmpS1900[2] = _M0L6_2atmpS1903;
  _M0L6_2atmpS1900[3] = _M0L6_2atmpS1904;
  _M0L6_2atmpS1899
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1899)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1899->$0 = _M0L6_2atmpS1900;
  _M0L6_2atmpS1899->$1 = 4;
  #line 3 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _tmp_2451
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1898, (moonbit_string_t)moonbit_string_literal_40.data, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS1899);
  if (_tmp_2451.tag) {
    int32_t const _M0L5_2aokS1907 = _tmp_2451.data.ok;
  } else {
    void* const _M0L6_2aerrS1908 = _tmp_2451.data.err;
    struct moonbit_result_0 _result_2452;
    _result_2452.tag = 0;
    _result_2452.data.err = _M0L6_2aerrS1908;
    return _result_2452;
  }
  #line 4 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1917
  = _M0FP36mulpjs4mulp11log__events10task__stop((moonbit_string_t)moonbit_string_literal_18.data, 12ll);
  #line 4 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1916
  = _M0MP36mulpjs4mulp11log__events8LogEvent6render(_M0L6_2atmpS1917);
  _M0L6_2atmpS1909
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1916
  };
  _M0L6_2atmpS1912 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS1913 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS1914 = 0;
  _M0L6_2atmpS1915 = 0;
  _M0L6_2atmpS1911 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1911[0] = _M0L6_2atmpS1912;
  _M0L6_2atmpS1911[1] = _M0L6_2atmpS1913;
  _M0L6_2atmpS1911[2] = _M0L6_2atmpS1914;
  _M0L6_2atmpS1911[3] = _M0L6_2atmpS1915;
  _M0L6_2atmpS1910
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1910)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1910->$0 = _M0L6_2atmpS1911;
  _M0L6_2atmpS1910->$1 = 4;
  #line 4 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _tmp_2453
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1909, (moonbit_string_t)moonbit_string_literal_44.data, (moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS1910);
  if (_tmp_2453.tag) {
    int32_t const _M0L5_2aokS1918 = _tmp_2453.data.ok;
  } else {
    void* const _M0L6_2aerrS1919 = _tmp_2453.data.err;
    struct moonbit_result_0 _result_2454;
    _result_2454.tag = 0;
    _result_2454.data.err = _M0L6_2aerrS1919;
    return _result_2454;
  }
  #line 5 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1928
  = _M0FP36mulpjs4mulp11log__events11task__error((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_29.data);
  #line 5 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  _M0L6_2atmpS1927
  = _M0MP36mulpjs4mulp11log__events8LogEvent6render(_M0L6_2atmpS1928);
  _M0L6_2atmpS1920
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS1927
  };
  _M0L6_2atmpS1923 = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS1924 = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS1925 = 0;
  _M0L6_2atmpS1926 = 0;
  _M0L6_2atmpS1922 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1922[0] = _M0L6_2atmpS1923;
  _M0L6_2atmpS1922[1] = _M0L6_2atmpS1924;
  _M0L6_2atmpS1922[2] = _M0L6_2atmpS1925;
  _M0L6_2atmpS1922[3] = _M0L6_2atmpS1926;
  _M0L6_2atmpS1921
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1921)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1921->$0 = _M0L6_2atmpS1922;
  _M0L6_2atmpS1921->$1 = 4;
  #line 5 "/Users/user/workspace/github/gulp/mulp/log_events/events_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1920, (moonbit_string_t)moonbit_string_literal_48.data, (moonbit_string_t)moonbit_string_literal_49.data, _M0L6_2atmpS1921);
}

moonbit_string_t _M0MP36mulpjs4mulp11log__events16LogEventRecorder6render(
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _M0L4selfS834
) {
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L8_2afieldS2187;
  int32_t _M0L6_2acntS2346;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6eventsS1897;
  #line 78 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L8_2afieldS2187 = _M0L4selfS834->$0;
  _M0L6_2acntS2346 = Moonbit_object_header(_M0L4selfS834)->rc;
  if (_M0L6_2acntS2346 > 1) {
    int32_t _M0L11_2anew__cntS2347 = _M0L6_2acntS2346 - 1;
    Moonbit_object_header(_M0L4selfS834)->rc = _M0L11_2anew__cntS2347;
    moonbit_incref(_M0L8_2afieldS2187);
  } else if (_M0L6_2acntS2346 == 1) {
    #line 79 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
    moonbit_free(_M0L4selfS834);
  }
  _M0L6eventsS1897 = _M0L8_2afieldS2187;
  #line 79 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  return _M0FP36mulpjs4mulp11log__events19render__log__events(_M0L6eventsS1897);
}

struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0MP36mulpjs4mulp11log__events16LogEventRecorder6events(
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _M0L4selfS829
) {
  void** _M0L6_2atmpS1896;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6copiedS827;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L8_2afieldS2190;
  int32_t _M0L6_2acntS2348;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L7_2abindS828;
  int32_t _M0L7_2abindS830;
  int32_t _M0L2__S831;
  #line 69 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1896 = (void**)moonbit_empty_ref_array;
  _M0L6copiedS827
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE));
  Moonbit_object_header(_M0L6copiedS827)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE, $0) >> 2, 1, 0);
  _M0L6copiedS827->$0 = _M0L6_2atmpS1896;
  _M0L6copiedS827->$1 = 0;
  _M0L8_2afieldS2190 = _M0L4selfS829->$0;
  _M0L6_2acntS2348 = Moonbit_object_header(_M0L4selfS829)->rc;
  if (_M0L6_2acntS2348 > 1) {
    int32_t _M0L11_2anew__cntS2349 = _M0L6_2acntS2348 - 1;
    Moonbit_object_header(_M0L4selfS829)->rc = _M0L11_2anew__cntS2349;
    moonbit_incref(_M0L8_2afieldS2190);
  } else if (_M0L6_2acntS2348 == 1) {
    #line 71 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
    moonbit_free(_M0L4selfS829);
  }
  _M0L7_2abindS828 = _M0L8_2afieldS2190;
  _M0L7_2abindS830 = _M0L7_2abindS828->$1;
  _M0L2__S831 = 0;
  while (1) {
    if (_M0L2__S831 < _M0L7_2abindS830) {
      void** _M0L3bufS1895 = _M0L7_2abindS828->$0;
      void* _M0L5eventS832 = (void*)_M0L3bufS1895[_M0L2__S831];
      int32_t _M0L6_2atmpS1894;
      moonbit_incref(_M0L5eventS832);
      moonbit_incref(_M0L6copiedS827);
      #line 72 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
      _M0MPC15array5Array4pushGRP36mulpjs4mulp11log__events8LogEventE(_M0L6copiedS827, _M0L5eventS832);
      _M0L6_2atmpS1894 = _M0L2__S831 + 1;
      _M0L2__S831 = _M0L6_2atmpS1894;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS828);
    }
    break;
  }
  return _M0L6copiedS827;
}

int32_t _M0MP36mulpjs4mulp11log__events16LogEventRecorder4emit(
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _M0L4selfS825,
  void* _M0L5eventS826
) {
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L8_2afieldS2191;
  int32_t _M0L6_2acntS2350;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6eventsS1893;
  #line 64 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L8_2afieldS2191 = _M0L4selfS825->$0;
  _M0L6_2acntS2350 = Moonbit_object_header(_M0L4selfS825)->rc;
  if (_M0L6_2acntS2350 > 1) {
    int32_t _M0L11_2anew__cntS2351 = _M0L6_2acntS2350 - 1;
    Moonbit_object_header(_M0L4selfS825)->rc = _M0L11_2anew__cntS2351;
    moonbit_incref(_M0L8_2afieldS2191);
  } else if (_M0L6_2acntS2350 == 1) {
    #line 65 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
    moonbit_free(_M0L4selfS825);
  }
  _M0L6eventsS1893 = _M0L8_2afieldS2191;
  #line 65 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0MPC15array5Array4pushGRP36mulpjs4mulp11log__events8LogEventE(_M0L6eventsS1893, _M0L5eventS826);
  return 0;
}

struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _M0FP36mulpjs4mulp11log__events25new__log__event__recorder(
  
) {
  void** _M0L6_2atmpS1892;
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6_2atmpS1891;
  struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder* _block_2456;
  #line 59 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1892 = (void**)moonbit_empty_ref_array;
  _M0L6_2atmpS1891
  = (struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE));
  Moonbit_object_header(_M0L6_2atmpS1891)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1891->$0 = _M0L6_2atmpS1892;
  _M0L6_2atmpS1891->$1 = 0;
  _block_2456
  = (struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder*)moonbit_malloc(sizeof(struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder));
  Moonbit_object_header(_block_2456)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP36mulpjs4mulp11log__events16LogEventRecorder, $0) >> 2, 1, 0);
  _block_2456->$0 = _M0L6_2atmpS1891;
  return _block_2456;
}

moonbit_string_t _M0FP36mulpjs4mulp11log__events19render__log__events(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L6eventsS820
) {
  moonbit_string_t* _M0L6_2atmpS1890;
  struct _M0TPB5ArrayGsE* _M0L8renderedS818;
  int32_t _M0L7_2abindS819;
  int32_t _M0L2__S821;
  moonbit_string_t _M0L7_2abindS824;
  int32_t _M0L6_2atmpS1889;
  struct _M0TPC16string10StringView _M0L6_2atmpS1888;
  #line 50 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1890 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L8renderedS818
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L8renderedS818)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L8renderedS818->$0 = _M0L6_2atmpS1890;
  _M0L8renderedS818->$1 = 0;
  _M0L7_2abindS819 = _M0L6eventsS820->$1;
  _M0L2__S821 = 0;
  while (1) {
    if (_M0L2__S821 < _M0L7_2abindS819) {
      void** _M0L3bufS1887 = _M0L6eventsS820->$0;
      void* _M0L5eventS822 = (void*)_M0L3bufS1887[_M0L2__S821];
      moonbit_string_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1886;
      moonbit_incref(_M0L5eventS822);
      #line 53 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
      _M0L6_2atmpS1885
      = _M0MP36mulpjs4mulp11log__events8LogEvent6render(_M0L5eventS822);
      moonbit_incref(_M0L8renderedS818);
      #line 53 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
      _M0MPC15array5Array4pushGsE(_M0L8renderedS818, _M0L6_2atmpS1885);
      _M0L6_2atmpS1886 = _M0L2__S821 + 1;
      _M0L2__S821 = _M0L6_2atmpS1886;
      continue;
    } else {
      moonbit_decref(_M0L6eventsS820);
    }
    break;
  }
  _M0L7_2abindS824 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS1889 = Moonbit_array_length(_M0L7_2abindS824);
  _M0L6_2atmpS1888
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1889, _M0L7_2abindS824
  };
  #line 55 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  return _M0MPC15array5Array4joinGsE(_M0L8renderedS818, _M0L6_2atmpS1888);
}

moonbit_string_t _M0MP36mulpjs4mulp11log__events8LogEvent18render__with__time(
  void* _M0L4selfS809,
  moonbit_string_t _M0L4timeS800
) {
  moonbit_string_t _M0L6_2atmpS1884;
  moonbit_string_t _M0L6prefixS799;
  moonbit_string_t _M0L4nameS802;
  moonbit_string_t _M0L6detailS803;
  moonbit_string_t _M0L4nameS805;
  int64_t _M0L12duration__msS806;
  moonbit_string_t _M0L4nameS808;
  moonbit_string_t _M0L6_2atmpS1883;
  moonbit_string_t _M0L6_2atmpS1882;
  moonbit_string_t _result_2461;
  moonbit_string_t _M0L6_2atmpS1881;
  moonbit_string_t _M0L6_2atmpS1880;
  moonbit_string_t _M0L6_2atmpS1878;
  moonbit_string_t _M0L6_2atmpS1879;
  moonbit_string_t _M0L6_2atmpS1877;
  moonbit_string_t _result_2462;
  moonbit_string_t _M0L6_2atmpS1876;
  moonbit_string_t _M0L6_2atmpS1875;
  moonbit_string_t _M0L6_2atmpS1874;
  moonbit_string_t _result_2463;
  #line 39 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  #line 40 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1884
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_51.data, _M0L4timeS800);
  moonbit_decref(_M0L4timeS800);
  #line 40 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6prefixS799
  = moonbit_add_string(_M0L6_2atmpS1884, (moonbit_string_t)moonbit_string_literal_52.data);
  moonbit_decref(_M0L6_2atmpS1884);
  switch (Moonbit_object_tag(_M0L4selfS809)) {
    case 0: {
      struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart* _M0L12_2aTaskStartS810 =
        (struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart*)_M0L4selfS809;
      moonbit_string_t _M0L8_2afieldS2194 = _M0L12_2aTaskStartS810->$0;
      int32_t _M0L6_2acntS2352 =
        Moonbit_object_header(_M0L12_2aTaskStartS810)->rc;
      moonbit_string_t _M0L7_2anameS811;
      if (_M0L6_2acntS2352 > 1) {
        int32_t _M0L11_2anew__cntS2353 = _M0L6_2acntS2352 - 1;
        Moonbit_object_header(_M0L12_2aTaskStartS810)->rc
        = _M0L11_2anew__cntS2353;
        moonbit_incref(_M0L8_2afieldS2194);
      } else if (_M0L6_2acntS2352 == 1) {
        #line 41 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
        moonbit_free(_M0L12_2aTaskStartS810);
      }
      _M0L7_2anameS811 = _M0L8_2afieldS2194;
      _M0L4nameS808 = _M0L7_2anameS811;
      goto join_807;
      break;
    }
    
    case 1: {
      struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop* _M0L11_2aTaskStopS812 =
        (struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop*)_M0L4selfS809;
      moonbit_string_t _M0L7_2anameS813 = _M0L11_2aTaskStopS812->$0;
      int64_t _M0L15_2aduration__msS814 = _M0L11_2aTaskStopS812->$1;
      int32_t _M0L6_2acntS2354 =
        Moonbit_object_header(_M0L11_2aTaskStopS812)->rc;
      if (_M0L6_2acntS2354 > 1) {
        int32_t _M0L11_2anew__cntS2355 = _M0L6_2acntS2354 - 1;
        Moonbit_object_header(_M0L11_2aTaskStopS812)->rc
        = _M0L11_2anew__cntS2355;
        moonbit_incref(_M0L7_2anameS813);
      } else if (_M0L6_2acntS2354 == 1) {
        #line 41 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
        moonbit_free(_M0L11_2aTaskStopS812);
      }
      _M0L4nameS805 = _M0L7_2anameS813;
      _M0L12duration__msS806 = _M0L15_2aduration__msS814;
      goto join_804;
      break;
    }
    default: {
      struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError* _M0L12_2aTaskErrorS815 =
        (struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError*)_M0L4selfS809;
      moonbit_string_t _M0L7_2anameS816 = _M0L12_2aTaskErrorS815->$0;
      moonbit_string_t _M0L8_2afieldS2196 = _M0L12_2aTaskErrorS815->$1;
      int32_t _M0L6_2acntS2356 =
        Moonbit_object_header(_M0L12_2aTaskErrorS815)->rc;
      moonbit_string_t _M0L9_2adetailS817;
      if (_M0L6_2acntS2356 > 1) {
        int32_t _M0L11_2anew__cntS2357 = _M0L6_2acntS2356 - 1;
        Moonbit_object_header(_M0L12_2aTaskErrorS815)->rc
        = _M0L11_2anew__cntS2357;
        moonbit_incref(_M0L8_2afieldS2196);
        moonbit_incref(_M0L7_2anameS816);
      } else if (_M0L6_2acntS2356 == 1) {
        #line 41 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
        moonbit_free(_M0L12_2aTaskErrorS815);
      }
      _M0L9_2adetailS817 = _M0L8_2afieldS2196;
      _M0L4nameS802 = _M0L7_2anameS816;
      _M0L6detailS803 = _M0L9_2adetailS817;
      goto join_801;
      break;
    }
  }
  join_807:;
  #line 42 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1883
  = moonbit_add_string(_M0L6prefixS799, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6prefixS799);
  #line 42 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1882 = moonbit_add_string(_M0L6_2atmpS1883, _M0L4nameS808);
  moonbit_decref(_M0L4nameS808);
  moonbit_decref(_M0L6_2atmpS1883);
  #line 42 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _result_2461
  = moonbit_add_string(_M0L6_2atmpS1882, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1882);
  return _result_2461;
  join_804:;
  #line 44 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1881
  = moonbit_add_string(_M0L6prefixS799, (moonbit_string_t)moonbit_string_literal_55.data);
  moonbit_decref(_M0L6prefixS799);
  #line 44 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1880 = moonbit_add_string(_M0L6_2atmpS1881, _M0L4nameS805);
  moonbit_decref(_M0L4nameS805);
  moonbit_decref(_M0L6_2atmpS1881);
  #line 44 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1878
  = moonbit_add_string(_M0L6_2atmpS1880, (moonbit_string_t)moonbit_string_literal_56.data);
  moonbit_decref(_M0L6_2atmpS1880);
  #line 44 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1879
  = _M0MPC15int645Int6418to__string_2einner(_M0L12duration__msS806, 10);
  #line 44 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1877 = moonbit_add_string(_M0L6_2atmpS1878, _M0L6_2atmpS1879);
  moonbit_decref(_M0L6_2atmpS1879);
  moonbit_decref(_M0L6_2atmpS1878);
  #line 44 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _result_2462
  = moonbit_add_string(_M0L6_2atmpS1877, (moonbit_string_t)moonbit_string_literal_57.data);
  moonbit_decref(_M0L6_2atmpS1877);
  return _result_2462;
  join_801:;
  #line 45 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1876
  = moonbit_add_string(_M0L6prefixS799, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6prefixS799);
  #line 45 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1875 = moonbit_add_string(_M0L6_2atmpS1876, _M0L4nameS802);
  moonbit_decref(_M0L4nameS802);
  moonbit_decref(_M0L6_2atmpS1876);
  #line 45 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1874
  = moonbit_add_string(_M0L6_2atmpS1875, (moonbit_string_t)moonbit_string_literal_59.data);
  moonbit_decref(_M0L6_2atmpS1875);
  #line 45 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _result_2463 = moonbit_add_string(_M0L6_2atmpS1874, _M0L6detailS803);
  moonbit_decref(_M0L6detailS803);
  moonbit_decref(_M0L6_2atmpS1874);
  return _result_2463;
}

moonbit_string_t _M0MP36mulpjs4mulp11log__events8LogEvent6render(
  void* _M0L4selfS790
) {
  moonbit_string_t _M0L4nameS783;
  moonbit_string_t _M0L6detailS784;
  moonbit_string_t _M0L4nameS786;
  int64_t _M0L12duration__msS787;
  moonbit_string_t _M0L4nameS789;
  moonbit_string_t _M0L6_2atmpS1873;
  moonbit_string_t _result_2467;
  moonbit_string_t _M0L6_2atmpS1872;
  moonbit_string_t _M0L6_2atmpS1870;
  moonbit_string_t _M0L6_2atmpS1871;
  moonbit_string_t _M0L6_2atmpS1869;
  moonbit_string_t _result_2468;
  moonbit_string_t _M0L6_2atmpS1868;
  moonbit_string_t _M0L6_2atmpS1867;
  moonbit_string_t _result_2469;
  #line 29 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  switch (Moonbit_object_tag(_M0L4selfS790)) {
    case 0: {
      struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart* _M0L12_2aTaskStartS791 =
        (struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart*)_M0L4selfS790;
      moonbit_string_t _M0L8_2afieldS2198 = _M0L12_2aTaskStartS791->$0;
      int32_t _M0L6_2acntS2358 =
        Moonbit_object_header(_M0L12_2aTaskStartS791)->rc;
      moonbit_string_t _M0L7_2anameS792;
      if (_M0L6_2acntS2358 > 1) {
        int32_t _M0L11_2anew__cntS2359 = _M0L6_2acntS2358 - 1;
        Moonbit_object_header(_M0L12_2aTaskStartS791)->rc
        = _M0L11_2anew__cntS2359;
        moonbit_incref(_M0L8_2afieldS2198);
      } else if (_M0L6_2acntS2358 == 1) {
        #line 30 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
        moonbit_free(_M0L12_2aTaskStartS791);
      }
      _M0L7_2anameS792 = _M0L8_2afieldS2198;
      _M0L4nameS789 = _M0L7_2anameS792;
      goto join_788;
      break;
    }
    
    case 1: {
      struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop* _M0L11_2aTaskStopS793 =
        (struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop*)_M0L4selfS790;
      moonbit_string_t _M0L7_2anameS794 = _M0L11_2aTaskStopS793->$0;
      int64_t _M0L15_2aduration__msS795 = _M0L11_2aTaskStopS793->$1;
      int32_t _M0L6_2acntS2360 =
        Moonbit_object_header(_M0L11_2aTaskStopS793)->rc;
      if (_M0L6_2acntS2360 > 1) {
        int32_t _M0L11_2anew__cntS2361 = _M0L6_2acntS2360 - 1;
        Moonbit_object_header(_M0L11_2aTaskStopS793)->rc
        = _M0L11_2anew__cntS2361;
        moonbit_incref(_M0L7_2anameS794);
      } else if (_M0L6_2acntS2360 == 1) {
        #line 30 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
        moonbit_free(_M0L11_2aTaskStopS793);
      }
      _M0L4nameS786 = _M0L7_2anameS794;
      _M0L12duration__msS787 = _M0L15_2aduration__msS795;
      goto join_785;
      break;
    }
    default: {
      struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError* _M0L12_2aTaskErrorS796 =
        (struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError*)_M0L4selfS790;
      moonbit_string_t _M0L7_2anameS797 = _M0L12_2aTaskErrorS796->$0;
      moonbit_string_t _M0L8_2afieldS2200 = _M0L12_2aTaskErrorS796->$1;
      int32_t _M0L6_2acntS2362 =
        Moonbit_object_header(_M0L12_2aTaskErrorS796)->rc;
      moonbit_string_t _M0L9_2adetailS798;
      if (_M0L6_2acntS2362 > 1) {
        int32_t _M0L11_2anew__cntS2363 = _M0L6_2acntS2362 - 1;
        Moonbit_object_header(_M0L12_2aTaskErrorS796)->rc
        = _M0L11_2anew__cntS2363;
        moonbit_incref(_M0L8_2afieldS2200);
        moonbit_incref(_M0L7_2anameS797);
      } else if (_M0L6_2acntS2362 == 1) {
        #line 30 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
        moonbit_free(_M0L12_2aTaskErrorS796);
      }
      _M0L9_2adetailS798 = _M0L8_2afieldS2200;
      _M0L4nameS783 = _M0L7_2anameS797;
      _M0L6detailS784 = _M0L9_2adetailS798;
      goto join_782;
      break;
    }
  }
  join_788:;
  #line 31 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1873
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_60.data, _M0L4nameS789);
  moonbit_decref(_M0L4nameS789);
  #line 31 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _result_2467
  = moonbit_add_string(_M0L6_2atmpS1873, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1873);
  return _result_2467;
  join_785:;
  #line 33 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1872
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_61.data, _M0L4nameS786);
  moonbit_decref(_M0L4nameS786);
  #line 33 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1870
  = moonbit_add_string(_M0L6_2atmpS1872, (moonbit_string_t)moonbit_string_literal_56.data);
  moonbit_decref(_M0L6_2atmpS1872);
  #line 33 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1871
  = _M0MPC15int645Int6418to__string_2einner(_M0L12duration__msS787, 10);
  #line 33 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1869 = moonbit_add_string(_M0L6_2atmpS1870, _M0L6_2atmpS1871);
  moonbit_decref(_M0L6_2atmpS1871);
  moonbit_decref(_M0L6_2atmpS1870);
  #line 33 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _result_2468
  = moonbit_add_string(_M0L6_2atmpS1869, (moonbit_string_t)moonbit_string_literal_57.data);
  moonbit_decref(_M0L6_2atmpS1869);
  return _result_2468;
  join_782:;
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1868
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_62.data, _M0L4nameS783);
  moonbit_decref(_M0L4nameS783);
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _M0L6_2atmpS1867
  = moonbit_add_string(_M0L6_2atmpS1868, (moonbit_string_t)moonbit_string_literal_59.data);
  moonbit_decref(_M0L6_2atmpS1868);
  #line 34 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _result_2469 = moonbit_add_string(_M0L6_2atmpS1867, _M0L6detailS784);
  moonbit_decref(_M0L6detailS784);
  moonbit_decref(_M0L6_2atmpS1867);
  return _result_2469;
}

void* _M0FP36mulpjs4mulp11log__events11task__error(
  moonbit_string_t _M0L4nameS780,
  moonbit_string_t _M0L6detailS781
) {
  void* _block_2470;
  #line 24 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _block_2470
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError));
  Moonbit_object_header(_block_2470)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError, $0) >> 2, 2, 2);
  ((struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError*)_block_2470)->$0
  = _M0L4nameS780;
  ((struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskError*)_block_2470)->$1
  = _M0L6detailS781;
  return _block_2470;
}

void* _M0FP36mulpjs4mulp11log__events10task__stop(
  moonbit_string_t _M0L4nameS778,
  int64_t _M0L12duration__msS779
) {
  void* _block_2471;
  #line 19 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _block_2471
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop));
  Moonbit_object_header(_block_2471)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop, $0) >> 2, 1, 1);
  ((struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop*)_block_2471)->$0
  = _M0L4nameS778;
  ((struct _M0DTP36mulpjs4mulp11log__events8LogEvent8TaskStop*)_block_2471)->$1
  = _M0L12duration__msS779;
  return _block_2471;
}

void* _M0FP36mulpjs4mulp11log__events11task__start(
  moonbit_string_t _M0L4nameS777
) {
  void* _block_2472;
  #line 14 "/Users/user/workspace/github/gulp/mulp/log_events/events.mbt"
  _block_2472
  = (void*)moonbit_malloc(sizeof(struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart));
  Moonbit_object_header(_block_2472)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart, $0) >> 2, 1, 0);
  ((struct _M0DTP36mulpjs4mulp11log__events8LogEvent9TaskStart*)_block_2472)->$0
  = _M0L4nameS777;
  return _block_2472;
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS775,
  struct _M0TPC16string10StringView _M0L9separatorS776
) {
  moonbit_string_t* _M0L3bufS1865;
  int32_t _M0L3lenS1866;
  int32_t _M0L6_2acntS2364;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1864;
  #line 2070 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS1865 = _M0L4selfS775->$0;
  _M0L3lenS1866 = _M0L4selfS775->$1;
  _M0L6_2acntS2364 = Moonbit_object_header(_M0L4selfS775)->rc;
  if (_M0L6_2acntS2364 > 1) {
    int32_t _M0L11_2anew__cntS2365 = _M0L6_2acntS2364 - 1;
    Moonbit_object_header(_M0L4selfS775)->rc = _M0L11_2anew__cntS2365;
    moonbit_incref(_M0L3bufS1865);
  } else if (_M0L6_2acntS2364 == 1) {
    #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_free(_M0L4selfS775);
  }
  _M0L6_2atmpS1864
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1866, _M0L3bufS1865
  };
  #line 2074 "/Users/user/.moon/lib/core/builtin/array.mbt"
  return _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS1864, _M0L9separatorS776);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS773,
  int32_t _M0L5indexS774
) {
  int32_t _M0L3lenS772;
  int32_t _if__result_2473;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS772 = _M0L4selfS773->$1;
  if (_M0L5indexS774 >= 0) {
    _if__result_2473 = _M0L5indexS774 < _M0L3lenS772;
  } else {
    _if__result_2473 = 0;
  }
  if (_if__result_2473) {
    moonbit_string_t* _M0L6_2atmpS1863;
    moonbit_string_t _M0L6_2atmpS2203;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS1863 = _M0MPC15array5Array6bufferGsE(_M0L4selfS773);
    if (
      _M0L5indexS774 < 0
      || _M0L5indexS774 >= Moonbit_array_length(_M0L6_2atmpS1863)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2203 = (moonbit_string_t)_M0L6_2atmpS1863[_M0L5indexS774];
    moonbit_incref(_M0L6_2atmpS2203);
    moonbit_decref(_M0L6_2atmpS1863);
    return _M0L6_2atmpS2203;
  } else {
    moonbit_decref(_M0L4selfS773);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS747,
  struct _M0TPC16string10StringView _M0L9separatorS759
) {
  int32_t _M0L3endS1842;
  int32_t _M0L5startS1843;
  int32_t _M0L6_2atmpS1841;
  #line 1369 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1842 = _M0L4selfS747.$2;
  _M0L5startS1843 = _M0L4selfS747.$1;
  _M0L6_2atmpS1841 = _M0L3endS1842 - _M0L5startS1843;
  if (_M0L6_2atmpS1841 == 0) {
    moonbit_decref(_M0L9separatorS759.$0);
    moonbit_decref(_M0L4selfS747.$0);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t* _M0L3bufS1861 = _M0L4selfS747.$0;
    int32_t _M0L5startS1862 = _M0L4selfS747.$1;
    moonbit_string_t _M0L5_2ahdS748 =
      (moonbit_string_t)_M0L3bufS1861[_M0L5startS1862];
    moonbit_string_t* _M0L9_2ax__bufS749 = _M0L4selfS747.$0;
    int32_t _M0L5startS1860 = _M0L4selfS747.$1;
    int32_t _M0L11_2ax__startS750 = 1 + _M0L5startS1860;
    int32_t _M0L9_2ax__endS751 = _M0L4selfS747.$2;
    struct _M0TPC16string10StringView _M0L2hdS752;
    int32_t _M0L7_2abindS753;
    int32_t _M0L6_2atmpS1859;
    int32_t _M0L10size__hintS754;
    int32_t _M0L2__S755;
    int32_t _M0L10size__hintS756;
    int32_t _M0L10size__hintS760;
    struct _M0TPB13StringBuilder* _M0L3bufS761;
    moonbit_string_t _M0L3strS1844;
    int32_t _M0L5startS1845;
    int32_t _M0L3endS1847;
    int64_t _M0L6_2atmpS1846;
    moonbit_incref(_M0L5_2ahdS748);
    #line 1376 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS752
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS748);
    _M0L7_2abindS753 = _M0L9_2ax__endS751 - _M0L11_2ax__startS750;
    moonbit_incref(_M0L2hdS752.$0);
    #line 1377 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L6_2atmpS1859 = _M0MPC16string10StringView6length(_M0L2hdS752);
    _M0L2__S755 = 0;
    _M0L10size__hintS756 = _M0L6_2atmpS1859;
    while (1) {
      if (_M0L2__S755 < _M0L7_2abindS753) {
        int32_t _M0L6_2atmpS1858 = _M0L11_2ax__startS750 + _M0L2__S755;
        moonbit_string_t _M0L1sS757 =
          (moonbit_string_t)_M0L9_2ax__bufS749[_M0L6_2atmpS1858];
        int32_t _M0L6_2atmpS1852 = _M0L2__S755 + 1;
        struct _M0TPC16string10StringView _M0L6_2atmpS1857;
        int32_t _M0L6_2atmpS1856;
        int32_t _M0L6_2atmpS1854;
        int32_t _M0L6_2atmpS1855;
        int32_t _M0L6_2atmpS1853;
        moonbit_incref(_M0L1sS757);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS1857
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS757);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS1856
        = _M0MPC16string10StringView6length(_M0L6_2atmpS1857);
        _M0L6_2atmpS1854 = _M0L10size__hintS756 + _M0L6_2atmpS1856;
        moonbit_incref(_M0L9separatorS759.$0);
        #line 1378 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
        _M0L6_2atmpS1855
        = _M0MPC16string10StringView6length(_M0L9separatorS759);
        _M0L6_2atmpS1853 = _M0L6_2atmpS1854 + _M0L6_2atmpS1855;
        _M0L2__S755 = _M0L6_2atmpS1852;
        _M0L10size__hintS756 = _M0L6_2atmpS1853;
        continue;
      } else {
        _M0L10size__hintS754 = _M0L10size__hintS756;
      }
      break;
    }
    _M0L10size__hintS760 = _M0L10size__hintS754 << 1;
    #line 1383 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS761 = _M0MPB13StringBuilder11new_2einner(_M0L10size__hintS760);
    moonbit_incref(_M0L3bufS761);
    #line 1385 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS761, _M0L2hdS752);
    _M0L3strS1844 = _M0L9separatorS759.$0;
    _M0L5startS1845 = _M0L9separatorS759.$1;
    _M0L3endS1847 = _M0L9separatorS759.$2;
    _M0L6_2atmpS1846 = (int64_t)_M0L3endS1847;
    moonbit_incref(_M0L3strS1844);
    #line 1386 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS1844, 0, _M0L5startS1845, _M0L6_2atmpS1846)
    ) {
      int32_t _M0L7_2abindS762;
      int32_t _M0L2__S763;
      moonbit_decref(_M0L9separatorS759.$0);
      _M0L7_2abindS762 = _M0L9_2ax__endS751 - _M0L11_2ax__startS750;
      _M0L2__S763 = 0;
      while (1) {
        if (_M0L2__S763 < _M0L7_2abindS762) {
          int32_t _M0L6_2atmpS1849 = _M0L11_2ax__startS750 + _M0L2__S763;
          moonbit_string_t _M0L1sS764 =
            (moonbit_string_t)_M0L9_2ax__bufS749[_M0L6_2atmpS1849];
          struct _M0TPC16string10StringView _M0L1sS765;
          int32_t _M0L6_2atmpS1848;
          moonbit_incref(_M0L1sS764);
          #line 1389 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS765
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS764);
          moonbit_incref(_M0L3bufS761);
          #line 1390 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS761, _M0L1sS765);
          _M0L6_2atmpS1848 = _M0L2__S763 + 1;
          _M0L2__S763 = _M0L6_2atmpS1848;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS749);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS767 = _M0L9_2ax__endS751 - _M0L11_2ax__startS750;
      int32_t _M0L2__S768 = 0;
      while (1) {
        if (_M0L2__S768 < _M0L7_2abindS767) {
          int32_t _M0L6_2atmpS1851 = _M0L11_2ax__startS750 + _M0L2__S768;
          moonbit_string_t _M0L1sS769 =
            (moonbit_string_t)_M0L9_2ax__bufS749[_M0L6_2atmpS1851];
          struct _M0TPC16string10StringView _M0L1sS770;
          int32_t _M0L6_2atmpS1850;
          moonbit_incref(_M0L1sS769);
          #line 1394 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS770
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS769);
          moonbit_incref(_M0L3bufS761);
          moonbit_incref(_M0L9separatorS759.$0);
          #line 1395 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS761, _M0L9separatorS759);
          moonbit_incref(_M0L3bufS761);
          #line 1397 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS761, _M0L1sS770);
          _M0L6_2atmpS1850 = _M0L2__S768 + 1;
          _M0L2__S768 = _M0L6_2atmpS1850;
          continue;
        } else {
          moonbit_decref(_M0L9separatorS759.$0);
          moonbit_decref(_M0L9_2ax__bufS749);
        }
        break;
      }
    }
    #line 1400 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS761);
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS746) {
  moonbit_string_t _M0L6_2atmpS1840;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS1840 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS746);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS1840);
  moonbit_decref(_M0L6_2atmpS1840);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS745,
  struct _M0TPB6Hasher* _M0L6hasherS744
) {
  #line 530 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 531 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS744, _M0L4selfS745);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS743,
  struct _M0TPB6Hasher* _M0L6hasherS742
) {
  #line 496 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 497 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS742, _M0L4selfS743);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS740,
  moonbit_string_t _M0L5valueS738
) {
  int32_t _M0L7_2abindS737;
  int32_t _M0L1iS739;
  #line 387 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS737 = Moonbit_array_length(_M0L5valueS738);
  _M0L1iS739 = 0;
  while (1) {
    if (_M0L1iS739 < _M0L7_2abindS737) {
      int32_t _M0L6_2atmpS1838 = _M0L5valueS738[_M0L1iS739];
      int32_t _M0L6_2atmpS1837 = (int32_t)_M0L6_2atmpS1838;
      uint32_t _M0L6_2atmpS1836 = *(uint32_t*)&_M0L6_2atmpS1837;
      int32_t _M0L6_2atmpS1839;
      moonbit_incref(_M0L4selfS740);
      #line 389 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS740, _M0L6_2atmpS1836);
      _M0L6_2atmpS1839 = _M0L1iS739 + 1;
      _M0L1iS739 = _M0L6_2atmpS1839;
      continue;
    } else {
      moonbit_decref(_M0L4selfS740);
      moonbit_decref(_M0L5valueS738);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS735,
  int32_t _M0L3idxS736
) {
  int32_t _result_2478;
  #line 1778 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _result_2478 = _M0L4selfS735[_M0L3idxS736];
  moonbit_decref(_M0L4selfS735);
  return _result_2478;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS722,
  int32_t _M0L3keyS718
) {
  int32_t _M0L4hashS717;
  int32_t _M0L14capacity__maskS1821;
  int32_t _M0L6_2atmpS1820;
  int32_t _M0L1iS719;
  int32_t _M0L3idxS720;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS717 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS718);
  _M0L14capacity__maskS1821 = _M0L4selfS722->$3;
  _M0L6_2atmpS1820 = _M0L4hashS717 & _M0L14capacity__maskS1821;
  _M0L1iS719 = 0;
  _M0L3idxS720 = _M0L6_2atmpS1820;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1819 =
      _M0L4selfS722->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS721;
    if (
      _M0L3idxS720 < 0
      || _M0L3idxS720 >= Moonbit_array_length(_M0L7entriesS1819)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS721
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1819[
        _M0L3idxS720
      ];
    if (_M0L7_2abindS721 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1808;
      if (_M0L7_2abindS721) {
        moonbit_incref(_M0L7_2abindS721);
      }
      moonbit_decref(_M0L4selfS722);
      if (_M0L7_2abindS721) {
        moonbit_decref(_M0L7_2abindS721);
      }
      _M0L6_2atmpS1808 = 0;
      return _M0L6_2atmpS1808;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS723 =
        _M0L7_2abindS721;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS724 =
        _M0L7_2aSomeS723;
      int32_t _M0L4hashS1810 = _M0L8_2aentryS724->$3;
      int32_t _if__result_2480;
      int32_t _M0L3pslS1813;
      int32_t _M0L6_2atmpS1815;
      int32_t _M0L6_2atmpS1817;
      int32_t _M0L14capacity__maskS1818;
      int32_t _M0L6_2atmpS1816;
      if (_M0L4hashS1810 == _M0L4hashS717) {
        int32_t _M0L3keyS1809 = _M0L8_2aentryS724->$4;
        _if__result_2480 = _M0L3keyS1809 == _M0L3keyS718;
      } else {
        _if__result_2480 = 0;
      }
      if (_if__result_2480) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2211;
        int32_t _M0L6_2acntS2366;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1812;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1811;
        moonbit_incref(_M0L8_2aentryS724);
        moonbit_decref(_M0L4selfS722);
        _M0L8_2afieldS2211 = _M0L8_2aentryS724->$5;
        _M0L6_2acntS2366 = Moonbit_object_header(_M0L8_2aentryS724)->rc;
        if (_M0L6_2acntS2366 > 1) {
          int32_t _M0L11_2anew__cntS2368 = _M0L6_2acntS2366 - 1;
          Moonbit_object_header(_M0L8_2aentryS724)->rc
          = _M0L11_2anew__cntS2368;
          moonbit_incref(_M0L8_2afieldS2211);
        } else if (_M0L6_2acntS2366 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2367 =
            _M0L8_2aentryS724->$1;
          if (_M0L8_2afieldS2367) {
            moonbit_decref(_M0L8_2afieldS2367);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS724);
        }
        _M0L5valueS1812 = _M0L8_2afieldS2211;
        _M0L6_2atmpS1811 = _M0L5valueS1812;
        return _M0L6_2atmpS1811;
      } else {
        moonbit_incref(_M0L8_2aentryS724);
      }
      _M0L3pslS1813 = _M0L8_2aentryS724->$2;
      moonbit_decref(_M0L8_2aentryS724);
      if (_M0L1iS719 > _M0L3pslS1813) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1814;
        moonbit_decref(_M0L4selfS722);
        _M0L6_2atmpS1814 = 0;
        return _M0L6_2atmpS1814;
      }
      _M0L6_2atmpS1815 = _M0L1iS719 + 1;
      _M0L6_2atmpS1817 = _M0L3idxS720 + 1;
      _M0L14capacity__maskS1818 = _M0L4selfS722->$3;
      _M0L6_2atmpS1816 = _M0L6_2atmpS1817 & _M0L14capacity__maskS1818;
      _M0L1iS719 = _M0L6_2atmpS1815;
      _M0L3idxS720 = _M0L6_2atmpS1816;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS731,
  moonbit_string_t _M0L3keyS727
) {
  int32_t _M0L4hashS726;
  int32_t _M0L14capacity__maskS1835;
  int32_t _M0L6_2atmpS1834;
  int32_t _M0L1iS728;
  int32_t _M0L3idxS729;
  #line 216 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS727);
  #line 217 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS726 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS727);
  _M0L14capacity__maskS1835 = _M0L4selfS731->$3;
  _M0L6_2atmpS1834 = _M0L4hashS726 & _M0L14capacity__maskS1835;
  _M0L1iS728 = 0;
  _M0L3idxS729 = _M0L6_2atmpS1834;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1833 =
      _M0L4selfS731->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS730;
    if (
      _M0L3idxS729 < 0
      || _M0L3idxS729 >= Moonbit_array_length(_M0L7entriesS1833)
    ) {
      #line 219 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS730
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1833[
        _M0L3idxS729
      ];
    if (_M0L7_2abindS730 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1822;
      if (_M0L7_2abindS730) {
        moonbit_incref(_M0L7_2abindS730);
      }
      moonbit_decref(_M0L4selfS731);
      if (_M0L7_2abindS730) {
        moonbit_decref(_M0L7_2abindS730);
      }
      moonbit_decref(_M0L3keyS727);
      _M0L6_2atmpS1822 = 0;
      return _M0L6_2atmpS1822;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS732 =
        _M0L7_2abindS730;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS733 =
        _M0L7_2aSomeS732;
      int32_t _M0L4hashS1824 = _M0L8_2aentryS733->$3;
      int32_t _if__result_2482;
      int32_t _M0L3pslS1827;
      int32_t _M0L6_2atmpS1829;
      int32_t _M0L6_2atmpS1831;
      int32_t _M0L14capacity__maskS1832;
      int32_t _M0L6_2atmpS1830;
      if (_M0L4hashS1824 == _M0L4hashS726) {
        moonbit_string_t _M0L3keyS1823 = _M0L8_2aentryS733->$4;
        #line 220 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_2482
        = moonbit_val_array_equal(_M0L3keyS1823, _M0L3keyS727);
      } else {
        _if__result_2482 = 0;
      }
      if (_if__result_2482) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2214;
        int32_t _M0L6_2acntS2369;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1826;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1825;
        moonbit_incref(_M0L8_2aentryS733);
        moonbit_decref(_M0L4selfS731);
        moonbit_decref(_M0L3keyS727);
        _M0L8_2afieldS2214 = _M0L8_2aentryS733->$5;
        _M0L6_2acntS2369 = Moonbit_object_header(_M0L8_2aentryS733)->rc;
        if (_M0L6_2acntS2369 > 1) {
          int32_t _M0L11_2anew__cntS2372 = _M0L6_2acntS2369 - 1;
          Moonbit_object_header(_M0L8_2aentryS733)->rc
          = _M0L11_2anew__cntS2372;
          moonbit_incref(_M0L8_2afieldS2214);
        } else if (_M0L6_2acntS2369 == 1) {
          moonbit_string_t _M0L8_2afieldS2371 = _M0L8_2aentryS733->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2370;
          moonbit_decref(_M0L8_2afieldS2371);
          _M0L8_2afieldS2370 = _M0L8_2aentryS733->$1;
          if (_M0L8_2afieldS2370) {
            moonbit_decref(_M0L8_2afieldS2370);
          }
          #line 221 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS733);
        }
        _M0L5valueS1826 = _M0L8_2afieldS2214;
        _M0L6_2atmpS1825 = _M0L5valueS1826;
        return _M0L6_2atmpS1825;
      } else {
        moonbit_incref(_M0L8_2aentryS733);
      }
      _M0L3pslS1827 = _M0L8_2aentryS733->$2;
      moonbit_decref(_M0L8_2aentryS733);
      if (_M0L1iS728 > _M0L3pslS1827) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1828;
        moonbit_decref(_M0L4selfS731);
        moonbit_decref(_M0L3keyS727);
        _M0L6_2atmpS1828 = 0;
        return _M0L6_2atmpS1828;
      }
      _M0L6_2atmpS1829 = _M0L1iS728 + 1;
      _M0L6_2atmpS1831 = _M0L3idxS729 + 1;
      _M0L14capacity__maskS1832 = _M0L4selfS731->$3;
      _M0L6_2atmpS1830 = _M0L6_2atmpS1831 & _M0L14capacity__maskS1832;
      _M0L1iS728 = _M0L6_2atmpS1829;
      _M0L3idxS729 = _M0L6_2atmpS1830;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS702
) {
  int32_t _M0L6lengthS701;
  int32_t _M0Lm8capacityS703;
  int32_t _M0L6_2atmpS1785;
  int32_t _M0L6_2atmpS1784;
  int32_t _M0L6_2atmpS1795;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS704;
  int32_t _M0L3endS1793;
  int32_t _M0L5startS1794;
  int32_t _M0L7_2abindS705;
  int32_t _M0L2__S706;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS702.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS701
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS702);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS703 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS701);
  _M0L6_2atmpS1785 = _M0Lm8capacityS703;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1784 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1785);
  if (_M0L6lengthS701 > _M0L6_2atmpS1784) {
    int32_t _M0L6_2atmpS1786 = _M0Lm8capacityS703;
    _M0Lm8capacityS703 = _M0L6_2atmpS1786 * 2;
  }
  _M0L6_2atmpS1795 = _M0Lm8capacityS703;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS704
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1795);
  _M0L3endS1793 = _M0L3arrS702.$2;
  _M0L5startS1794 = _M0L3arrS702.$1;
  _M0L7_2abindS705 = _M0L3endS1793 - _M0L5startS1794;
  _M0L2__S706 = 0;
  while (1) {
    if (_M0L2__S706 < _M0L7_2abindS705) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1790 =
        _M0L3arrS702.$0;
      int32_t _M0L5startS1792 = _M0L3arrS702.$1;
      int32_t _M0L6_2atmpS1791 = _M0L5startS1792 + _M0L2__S706;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS707 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1790[
          _M0L6_2atmpS1791
        ];
      moonbit_string_t _M0L6_2atmpS1787 = _M0L1eS707->$0;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1788 =
        _M0L1eS707->$1;
      int32_t _M0L6_2atmpS1789;
      moonbit_incref(_M0L6_2atmpS1788);
      moonbit_incref(_M0L6_2atmpS1787);
      moonbit_incref(_M0L1mS704);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS704, _M0L6_2atmpS1787, _M0L6_2atmpS1788);
      _M0L6_2atmpS1789 = _M0L2__S706 + 1;
      _M0L2__S706 = _M0L6_2atmpS1789;
      continue;
    } else {
      moonbit_decref(_M0L3arrS702.$0);
    }
    break;
  }
  return _M0L1mS704;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS710
) {
  int32_t _M0L6lengthS709;
  int32_t _M0Lm8capacityS711;
  int32_t _M0L6_2atmpS1797;
  int32_t _M0L6_2atmpS1796;
  int32_t _M0L6_2atmpS1807;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS712;
  int32_t _M0L3endS1805;
  int32_t _M0L5startS1806;
  int32_t _M0L7_2abindS713;
  int32_t _M0L2__S714;
  #line 72 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS710.$0);
  #line 73 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS709
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS710);
  #line 74 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS711 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS709);
  _M0L6_2atmpS1797 = _M0Lm8capacityS711;
  #line 75 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1796 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1797);
  if (_M0L6lengthS709 > _M0L6_2atmpS1796) {
    int32_t _M0L6_2atmpS1798 = _M0Lm8capacityS711;
    _M0Lm8capacityS711 = _M0L6_2atmpS1798 * 2;
  }
  _M0L6_2atmpS1807 = _M0Lm8capacityS711;
  #line 78 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS712
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1807);
  _M0L3endS1805 = _M0L3arrS710.$2;
  _M0L5startS1806 = _M0L3arrS710.$1;
  _M0L7_2abindS713 = _M0L3endS1805 - _M0L5startS1806;
  _M0L2__S714 = 0;
  while (1) {
    if (_M0L2__S714 < _M0L7_2abindS713) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1802 =
        _M0L3arrS710.$0;
      int32_t _M0L5startS1804 = _M0L3arrS710.$1;
      int32_t _M0L6_2atmpS1803 = _M0L5startS1804 + _M0L2__S714;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS715 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1802[
          _M0L6_2atmpS1803
        ];
      int32_t _M0L6_2atmpS1799 = _M0L1eS715->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1800 = _M0L1eS715->$1;
      int32_t _M0L6_2atmpS1801;
      moonbit_incref(_M0L6_2atmpS1800);
      moonbit_incref(_M0L1mS712);
      #line 80 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS712, _M0L6_2atmpS1799, _M0L6_2atmpS1800);
      _M0L6_2atmpS1801 = _M0L2__S714 + 1;
      _M0L2__S714 = _M0L6_2atmpS1801;
      continue;
    } else {
      moonbit_decref(_M0L3arrS710.$0);
    }
    break;
  }
  return _M0L1mS712;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS695,
  moonbit_string_t _M0L3keyS696,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS697
) {
  int32_t _M0L6_2atmpS1782;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS696);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1782 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS696);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS695, _M0L3keyS696, _M0L5valueS697, _M0L6_2atmpS1782);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS698,
  int32_t _M0L3keyS699,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS700
) {
  int32_t _M0L6_2atmpS1783;
  #line 107 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1783 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS699);
  #line 109 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS698, _M0L3keyS699, _M0L5valueS700, _M0L6_2atmpS1783);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS674
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS673;
  int32_t _M0L8capacityS1774;
  int32_t _M0L13new__capacityS675;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1769;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1768;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2229;
  int32_t _M0L6_2atmpS1770;
  int32_t _M0L8capacityS1772;
  int32_t _M0L6_2atmpS1771;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1773;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2228;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1xS676;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS673 = _M0L4selfS674->$5;
  _M0L8capacityS1774 = _M0L4selfS674->$2;
  _M0L13new__capacityS675 = _M0L8capacityS1774 << 1;
  _M0L6_2atmpS1769 = 0;
  _M0L6_2atmpS1768
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS675, _M0L6_2atmpS1769);
  _M0L6_2aoldS2229 = _M0L4selfS674->$0;
  if (_M0L9old__headS673) {
    moonbit_incref(_M0L9old__headS673);
  }
  moonbit_decref(_M0L6_2aoldS2229);
  _M0L4selfS674->$0 = _M0L6_2atmpS1768;
  _M0L4selfS674->$2 = _M0L13new__capacityS675;
  _M0L6_2atmpS1770 = _M0L13new__capacityS675 - 1;
  _M0L4selfS674->$3 = _M0L6_2atmpS1770;
  _M0L8capacityS1772 = _M0L4selfS674->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1771 = _M0FPB21calc__grow__threshold(_M0L8capacityS1772);
  _M0L4selfS674->$4 = _M0L6_2atmpS1771;
  _M0L4selfS674->$1 = 0;
  _M0L6_2atmpS1773 = 0;
  _M0L6_2aoldS2228 = _M0L4selfS674->$5;
  if (_M0L6_2aoldS2228) {
    moonbit_decref(_M0L6_2aoldS2228);
  }
  _M0L4selfS674->$5 = _M0L6_2atmpS1773;
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
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS677 =
        _M0L1xS676;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS678 =
        _M0L7_2aSomeS677;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS679 =
        _M0L4_2axS678->$1;
      moonbit_string_t _M0L6_2akeyS680 = _M0L4_2axS678->$4;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS681 =
        _M0L4_2axS678->$5;
      int32_t _M0L7_2ahashS682 = _M0L4_2axS678->$3;
      int32_t _M0L6_2acntS2373 = Moonbit_object_header(_M0L4_2axS678)->rc;
      if (_M0L6_2acntS2373 > 1) {
        int32_t _M0L11_2anew__cntS2374 = _M0L6_2acntS2373 - 1;
        Moonbit_object_header(_M0L4_2axS678)->rc = _M0L11_2anew__cntS2374;
        moonbit_incref(_M0L8_2avalueS681);
        moonbit_incref(_M0L6_2akeyS680);
        if (_M0L7_2anextS679) {
          moonbit_incref(_M0L7_2anextS679);
        }
      } else if (_M0L6_2acntS2373 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS678);
      }
      moonbit_incref(_M0L4selfS674);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS674, _M0L6_2akeyS680, _M0L8_2avalueS681, _M0L7_2ahashS682);
      _M0L1xS676 = _M0L7_2anextS679;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS685
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS684;
  int32_t _M0L8capacityS1781;
  int32_t _M0L13new__capacityS686;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1776;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1775;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2234;
  int32_t _M0L6_2atmpS1777;
  int32_t _M0L8capacityS1779;
  int32_t _M0L6_2atmpS1778;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1780;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2233;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L1xS687;
  #line 488 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS684 = _M0L4selfS685->$5;
  _M0L8capacityS1781 = _M0L4selfS685->$2;
  _M0L13new__capacityS686 = _M0L8capacityS1781 << 1;
  _M0L6_2atmpS1776 = 0;
  _M0L6_2atmpS1775
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS686, _M0L6_2atmpS1776);
  _M0L6_2aoldS2234 = _M0L4selfS685->$0;
  if (_M0L9old__headS684) {
    moonbit_incref(_M0L9old__headS684);
  }
  moonbit_decref(_M0L6_2aoldS2234);
  _M0L4selfS685->$0 = _M0L6_2atmpS1775;
  _M0L4selfS685->$2 = _M0L13new__capacityS686;
  _M0L6_2atmpS1777 = _M0L13new__capacityS686 - 1;
  _M0L4selfS685->$3 = _M0L6_2atmpS1777;
  _M0L8capacityS1779 = _M0L4selfS685->$2;
  #line 494 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1778 = _M0FPB21calc__grow__threshold(_M0L8capacityS1779);
  _M0L4selfS685->$4 = _M0L6_2atmpS1778;
  _M0L4selfS685->$1 = 0;
  _M0L6_2atmpS1780 = 0;
  _M0L6_2aoldS2233 = _M0L4selfS685->$5;
  if (_M0L6_2aoldS2233) {
    moonbit_decref(_M0L6_2aoldS2233);
  }
  _M0L4selfS685->$5 = _M0L6_2atmpS1780;
  _M0L4selfS685->$6 = -1;
  _M0L1xS687 = _M0L9old__headS684;
  while (1) {
    if (_M0L1xS687 == 0) {
      if (_M0L1xS687) {
        moonbit_decref(_M0L1xS687);
      }
      moonbit_decref(_M0L4selfS685);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS688 =
        _M0L1xS687;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS689 =
        _M0L7_2aSomeS688;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS690 =
        _M0L4_2axS689->$1;
      int32_t _M0L6_2akeyS691 = _M0L4_2axS689->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS692 =
        _M0L4_2axS689->$5;
      int32_t _M0L7_2ahashS693 = _M0L4_2axS689->$3;
      int32_t _M0L6_2acntS2375 = Moonbit_object_header(_M0L4_2axS689)->rc;
      if (_M0L6_2acntS2375 > 1) {
        int32_t _M0L11_2anew__cntS2376 = _M0L6_2acntS2375 - 1;
        Moonbit_object_header(_M0L4_2axS689)->rc = _M0L11_2anew__cntS2376;
        moonbit_incref(_M0L8_2avalueS692);
        if (_M0L7_2anextS690) {
          moonbit_incref(_M0L7_2anextS690);
        }
      } else if (_M0L6_2acntS2375 == 1) {
        #line 498 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS689);
      }
      moonbit_incref(_M0L4selfS685);
      #line 501 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS685, _M0L6_2akeyS691, _M0L8_2avalueS692, _M0L7_2ahashS693);
      _M0L1xS687 = _M0L7_2anextS690;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS644,
  moonbit_string_t _M0L3keyS650,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS651,
  int32_t _M0L4hashS646
) {
  int32_t _M0L14capacity__maskS1749;
  int32_t _M0L6_2atmpS1748;
  int32_t _M0L3pslS641;
  int32_t _M0L3idxS642;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1749 = _M0L4selfS644->$3;
  _M0L6_2atmpS1748 = _M0L4hashS646 & _M0L14capacity__maskS1749;
  _M0L3pslS641 = 0;
  _M0L3idxS642 = _M0L6_2atmpS1748;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1747 =
      _M0L4selfS644->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS643;
    if (
      _M0L3idxS642 < 0
      || _M0L3idxS642 >= Moonbit_array_length(_M0L7entriesS1747)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS643
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1747[
        _M0L3idxS642
      ];
    if (_M0L7_2abindS643 == 0) {
      int32_t _M0L4sizeS1732 = _M0L4selfS644->$1;
      int32_t _M0L8grow__atS1733 = _M0L4selfS644->$4;
      int32_t _M0L7_2abindS647;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS648;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS649;
      if (_M0L4sizeS1732 >= _M0L8grow__atS1733) {
        int32_t _M0L14capacity__maskS1735;
        int32_t _M0L6_2atmpS1734;
        moonbit_incref(_M0L4selfS644);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS644);
        _M0L14capacity__maskS1735 = _M0L4selfS644->$3;
        _M0L6_2atmpS1734 = _M0L4hashS646 & _M0L14capacity__maskS1735;
        _M0L3pslS641 = 0;
        _M0L3idxS642 = _M0L6_2atmpS1734;
        continue;
      }
      _M0L7_2abindS647 = _M0L4selfS644->$6;
      _M0L7_2abindS648 = 0;
      _M0L5entryS649
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS649)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS649->$0 = _M0L7_2abindS647;
      _M0L5entryS649->$1 = _M0L7_2abindS648;
      _M0L5entryS649->$2 = _M0L3pslS641;
      _M0L5entryS649->$3 = _M0L4hashS646;
      _M0L5entryS649->$4 = _M0L3keyS650;
      _M0L5entryS649->$5 = _M0L5valueS651;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS644, _M0L3idxS642, _M0L5entryS649);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS652 =
        _M0L7_2abindS643;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS653 =
        _M0L7_2aSomeS652;
      int32_t _M0L4hashS1737 = _M0L14_2acurr__entryS653->$3;
      int32_t _if__result_2488;
      int32_t _M0L3pslS1738;
      int32_t _M0L6_2atmpS1743;
      int32_t _M0L6_2atmpS1745;
      int32_t _M0L14capacity__maskS1746;
      int32_t _M0L6_2atmpS1744;
      if (_M0L4hashS1737 == _M0L4hashS646) {
        moonbit_string_t _M0L3keyS1736 = _M0L14_2acurr__entryS653->$4;
        #line 134 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_2488
        = moonbit_val_array_equal(_M0L3keyS1736, _M0L3keyS650);
      } else {
        _if__result_2488 = 0;
      }
      if (_if__result_2488) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2236;
        moonbit_incref(_M0L14_2acurr__entryS653);
        moonbit_decref(_M0L3keyS650);
        moonbit_decref(_M0L4selfS644);
        _M0L6_2aoldS2236 = _M0L14_2acurr__entryS653->$5;
        moonbit_decref(_M0L6_2aoldS2236);
        _M0L14_2acurr__entryS653->$5 = _M0L5valueS651;
        moonbit_decref(_M0L14_2acurr__entryS653);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS653);
      }
      _M0L3pslS1738 = _M0L14_2acurr__entryS653->$2;
      if (_M0L3pslS641 > _M0L3pslS1738) {
        int32_t _M0L4sizeS1739 = _M0L4selfS644->$1;
        int32_t _M0L8grow__atS1740 = _M0L4selfS644->$4;
        int32_t _M0L7_2abindS654;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS655;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS656;
        if (_M0L4sizeS1739 >= _M0L8grow__atS1740) {
          int32_t _M0L14capacity__maskS1742;
          int32_t _M0L6_2atmpS1741;
          moonbit_decref(_M0L14_2acurr__entryS653);
          moonbit_incref(_M0L4selfS644);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS644);
          _M0L14capacity__maskS1742 = _M0L4selfS644->$3;
          _M0L6_2atmpS1741 = _M0L4hashS646 & _M0L14capacity__maskS1742;
          _M0L3pslS641 = 0;
          _M0L3idxS642 = _M0L6_2atmpS1741;
          continue;
        }
        moonbit_incref(_M0L4selfS644);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS644, _M0L3idxS642, _M0L14_2acurr__entryS653);
        _M0L7_2abindS654 = _M0L4selfS644->$6;
        _M0L7_2abindS655 = 0;
        _M0L5entryS656
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS656)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS656->$0 = _M0L7_2abindS654;
        _M0L5entryS656->$1 = _M0L7_2abindS655;
        _M0L5entryS656->$2 = _M0L3pslS641;
        _M0L5entryS656->$3 = _M0L4hashS646;
        _M0L5entryS656->$4 = _M0L3keyS650;
        _M0L5entryS656->$5 = _M0L5valueS651;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS644, _M0L3idxS642, _M0L5entryS656);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS653);
      }
      _M0L6_2atmpS1743 = _M0L3pslS641 + 1;
      _M0L6_2atmpS1745 = _M0L3idxS642 + 1;
      _M0L14capacity__maskS1746 = _M0L4selfS644->$3;
      _M0L6_2atmpS1744 = _M0L6_2atmpS1745 & _M0L14capacity__maskS1746;
      _M0L3pslS641 = _M0L6_2atmpS1743;
      _M0L3idxS642 = _M0L6_2atmpS1744;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS660,
  int32_t _M0L3keyS666,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS667,
  int32_t _M0L4hashS662
) {
  int32_t _M0L14capacity__maskS1767;
  int32_t _M0L6_2atmpS1766;
  int32_t _M0L3pslS657;
  int32_t _M0L3idxS658;
  #line 113 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1767 = _M0L4selfS660->$3;
  _M0L6_2atmpS1766 = _M0L4hashS662 & _M0L14capacity__maskS1767;
  _M0L3pslS657 = 0;
  _M0L3idxS658 = _M0L6_2atmpS1766;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1765 =
      _M0L4selfS660->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS659;
    if (
      _M0L3idxS658 < 0
      || _M0L3idxS658 >= Moonbit_array_length(_M0L7entriesS1765)
    ) {
      #line 121 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS659
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1765[
        _M0L3idxS658
      ];
    if (_M0L7_2abindS659 == 0) {
      int32_t _M0L4sizeS1750 = _M0L4selfS660->$1;
      int32_t _M0L8grow__atS1751 = _M0L4selfS660->$4;
      int32_t _M0L7_2abindS663;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS664;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS665;
      if (_M0L4sizeS1750 >= _M0L8grow__atS1751) {
        int32_t _M0L14capacity__maskS1753;
        int32_t _M0L6_2atmpS1752;
        moonbit_incref(_M0L4selfS660);
        #line 125 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS660);
        _M0L14capacity__maskS1753 = _M0L4selfS660->$3;
        _M0L6_2atmpS1752 = _M0L4hashS662 & _M0L14capacity__maskS1753;
        _M0L3pslS657 = 0;
        _M0L3idxS658 = _M0L6_2atmpS1752;
        continue;
      }
      _M0L7_2abindS663 = _M0L4selfS660->$6;
      _M0L7_2abindS664 = 0;
      _M0L5entryS665
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS665)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS665->$0 = _M0L7_2abindS663;
      _M0L5entryS665->$1 = _M0L7_2abindS664;
      _M0L5entryS665->$2 = _M0L3pslS657;
      _M0L5entryS665->$3 = _M0L4hashS662;
      _M0L5entryS665->$4 = _M0L3keyS666;
      _M0L5entryS665->$5 = _M0L5valueS667;
      #line 130 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS660, _M0L3idxS658, _M0L5entryS665);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS668 =
        _M0L7_2abindS659;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS669 =
        _M0L7_2aSomeS668;
      int32_t _M0L4hashS1755 = _M0L14_2acurr__entryS669->$3;
      int32_t _if__result_2490;
      int32_t _M0L3pslS1756;
      int32_t _M0L6_2atmpS1761;
      int32_t _M0L6_2atmpS1763;
      int32_t _M0L14capacity__maskS1764;
      int32_t _M0L6_2atmpS1762;
      if (_M0L4hashS1755 == _M0L4hashS662) {
        int32_t _M0L3keyS1754 = _M0L14_2acurr__entryS669->$4;
        _if__result_2490 = _M0L3keyS1754 == _M0L3keyS666;
      } else {
        _if__result_2490 = 0;
      }
      if (_if__result_2490) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2240;
        moonbit_incref(_M0L14_2acurr__entryS669);
        moonbit_decref(_M0L4selfS660);
        _M0L6_2aoldS2240 = _M0L14_2acurr__entryS669->$5;
        moonbit_decref(_M0L6_2aoldS2240);
        _M0L14_2acurr__entryS669->$5 = _M0L5valueS667;
        moonbit_decref(_M0L14_2acurr__entryS669);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS669);
      }
      _M0L3pslS1756 = _M0L14_2acurr__entryS669->$2;
      if (_M0L3pslS657 > _M0L3pslS1756) {
        int32_t _M0L4sizeS1757 = _M0L4selfS660->$1;
        int32_t _M0L8grow__atS1758 = _M0L4selfS660->$4;
        int32_t _M0L7_2abindS670;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS671;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS672;
        if (_M0L4sizeS1757 >= _M0L8grow__atS1758) {
          int32_t _M0L14capacity__maskS1760;
          int32_t _M0L6_2atmpS1759;
          moonbit_decref(_M0L14_2acurr__entryS669);
          moonbit_incref(_M0L4selfS660);
          #line 142 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS660);
          _M0L14capacity__maskS1760 = _M0L4selfS660->$3;
          _M0L6_2atmpS1759 = _M0L4hashS662 & _M0L14capacity__maskS1760;
          _M0L3pslS657 = 0;
          _M0L3idxS658 = _M0L6_2atmpS1759;
          continue;
        }
        moonbit_incref(_M0L4selfS660);
        #line 146 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS660, _M0L3idxS658, _M0L14_2acurr__entryS669);
        _M0L7_2abindS670 = _M0L4selfS660->$6;
        _M0L7_2abindS671 = 0;
        _M0L5entryS672
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS672)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS672->$0 = _M0L7_2abindS670;
        _M0L5entryS672->$1 = _M0L7_2abindS671;
        _M0L5entryS672->$2 = _M0L3pslS657;
        _M0L5entryS672->$3 = _M0L4hashS662;
        _M0L5entryS672->$4 = _M0L3keyS666;
        _M0L5entryS672->$5 = _M0L5valueS667;
        #line 148 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS660, _M0L3idxS658, _M0L5entryS672);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS669);
      }
      _M0L6_2atmpS1761 = _M0L3pslS657 + 1;
      _M0L6_2atmpS1763 = _M0L3idxS658 + 1;
      _M0L14capacity__maskS1764 = _M0L4selfS660->$3;
      _M0L6_2atmpS1762 = _M0L6_2atmpS1763 & _M0L14capacity__maskS1764;
      _M0L3pslS657 = _M0L6_2atmpS1761;
      _M0L3idxS658 = _M0L6_2atmpS1762;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS625,
  int32_t _M0L3idxS630,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS629
) {
  int32_t _M0L3pslS1715;
  int32_t _M0L6_2atmpS1711;
  int32_t _M0L6_2atmpS1713;
  int32_t _M0L14capacity__maskS1714;
  int32_t _M0L6_2atmpS1712;
  int32_t _M0L3pslS621;
  int32_t _M0L3idxS622;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS623;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1715 = _M0L5entryS629->$2;
  _M0L6_2atmpS1711 = _M0L3pslS1715 + 1;
  _M0L6_2atmpS1713 = _M0L3idxS630 + 1;
  _M0L14capacity__maskS1714 = _M0L4selfS625->$3;
  _M0L6_2atmpS1712 = _M0L6_2atmpS1713 & _M0L14capacity__maskS1714;
  _M0L3pslS621 = _M0L6_2atmpS1711;
  _M0L3idxS622 = _M0L6_2atmpS1712;
  _M0L5entryS623 = _M0L5entryS629;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1710 =
      _M0L4selfS625->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS624;
    if (
      _M0L3idxS622 < 0
      || _M0L3idxS622 >= Moonbit_array_length(_M0L7entriesS1710)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS624
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1710[
        _M0L3idxS622
      ];
    if (_M0L7_2abindS624 == 0) {
      _M0L5entryS623->$2 = _M0L3pslS621;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS625, _M0L5entryS623, _M0L3idxS622);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS627 =
        _M0L7_2abindS624;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS628 =
        _M0L7_2aSomeS627;
      int32_t _M0L3pslS1700 = _M0L14_2acurr__entryS628->$2;
      if (_M0L3pslS621 > _M0L3pslS1700) {
        int32_t _M0L3pslS1705;
        int32_t _M0L6_2atmpS1701;
        int32_t _M0L6_2atmpS1703;
        int32_t _M0L14capacity__maskS1704;
        int32_t _M0L6_2atmpS1702;
        _M0L5entryS623->$2 = _M0L3pslS621;
        moonbit_incref(_M0L14_2acurr__entryS628);
        moonbit_incref(_M0L4selfS625);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS625, _M0L5entryS623, _M0L3idxS622);
        _M0L3pslS1705 = _M0L14_2acurr__entryS628->$2;
        _M0L6_2atmpS1701 = _M0L3pslS1705 + 1;
        _M0L6_2atmpS1703 = _M0L3idxS622 + 1;
        _M0L14capacity__maskS1704 = _M0L4selfS625->$3;
        _M0L6_2atmpS1702 = _M0L6_2atmpS1703 & _M0L14capacity__maskS1704;
        _M0L3pslS621 = _M0L6_2atmpS1701;
        _M0L3idxS622 = _M0L6_2atmpS1702;
        _M0L5entryS623 = _M0L14_2acurr__entryS628;
        continue;
      } else {
        int32_t _M0L6_2atmpS1706 = _M0L3pslS621 + 1;
        int32_t _M0L6_2atmpS1708 = _M0L3idxS622 + 1;
        int32_t _M0L14capacity__maskS1709 = _M0L4selfS625->$3;
        int32_t _M0L6_2atmpS1707 =
          _M0L6_2atmpS1708 & _M0L14capacity__maskS1709;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_2492 =
          _M0L5entryS623;
        _M0L3pslS621 = _M0L6_2atmpS1706;
        _M0L3idxS622 = _M0L6_2atmpS1707;
        _M0L5entryS623 = _tmp_2492;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS635,
  int32_t _M0L3idxS640,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS639
) {
  int32_t _M0L3pslS1731;
  int32_t _M0L6_2atmpS1727;
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L14capacity__maskS1730;
  int32_t _M0L6_2atmpS1728;
  int32_t _M0L3pslS631;
  int32_t _M0L3idxS632;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS633;
  #line 158 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1731 = _M0L5entryS639->$2;
  _M0L6_2atmpS1727 = _M0L3pslS1731 + 1;
  _M0L6_2atmpS1729 = _M0L3idxS640 + 1;
  _M0L14capacity__maskS1730 = _M0L4selfS635->$3;
  _M0L6_2atmpS1728 = _M0L6_2atmpS1729 & _M0L14capacity__maskS1730;
  _M0L3pslS631 = _M0L6_2atmpS1727;
  _M0L3idxS632 = _M0L6_2atmpS1728;
  _M0L5entryS633 = _M0L5entryS639;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1726 =
      _M0L4selfS635->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS634;
    if (
      _M0L3idxS632 < 0
      || _M0L3idxS632 >= Moonbit_array_length(_M0L7entriesS1726)
    ) {
      #line 164 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS634
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1726[
        _M0L3idxS632
      ];
    if (_M0L7_2abindS634 == 0) {
      _M0L5entryS633->$2 = _M0L3pslS631;
      #line 167 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS635, _M0L5entryS633, _M0L3idxS632);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS637 =
        _M0L7_2abindS634;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS638 =
        _M0L7_2aSomeS637;
      int32_t _M0L3pslS1716 = _M0L14_2acurr__entryS638->$2;
      if (_M0L3pslS631 > _M0L3pslS1716) {
        int32_t _M0L3pslS1721;
        int32_t _M0L6_2atmpS1717;
        int32_t _M0L6_2atmpS1719;
        int32_t _M0L14capacity__maskS1720;
        int32_t _M0L6_2atmpS1718;
        _M0L5entryS633->$2 = _M0L3pslS631;
        moonbit_incref(_M0L14_2acurr__entryS638);
        moonbit_incref(_M0L4selfS635);
        #line 173 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS635, _M0L5entryS633, _M0L3idxS632);
        _M0L3pslS1721 = _M0L14_2acurr__entryS638->$2;
        _M0L6_2atmpS1717 = _M0L3pslS1721 + 1;
        _M0L6_2atmpS1719 = _M0L3idxS632 + 1;
        _M0L14capacity__maskS1720 = _M0L4selfS635->$3;
        _M0L6_2atmpS1718 = _M0L6_2atmpS1719 & _M0L14capacity__maskS1720;
        _M0L3pslS631 = _M0L6_2atmpS1717;
        _M0L3idxS632 = _M0L6_2atmpS1718;
        _M0L5entryS633 = _M0L14_2acurr__entryS638;
        continue;
      } else {
        int32_t _M0L6_2atmpS1722 = _M0L3pslS631 + 1;
        int32_t _M0L6_2atmpS1724 = _M0L3idxS632 + 1;
        int32_t _M0L14capacity__maskS1725 = _M0L4selfS635->$3;
        int32_t _M0L6_2atmpS1723 =
          _M0L6_2atmpS1724 & _M0L14capacity__maskS1725;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_2494 =
          _M0L5entryS633;
        _M0L3pslS631 = _M0L6_2atmpS1722;
        _M0L3idxS632 = _M0L6_2atmpS1723;
        _M0L5entryS633 = _tmp_2494;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS609,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS611,
  int32_t _M0L8new__idxS610
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1696;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1697;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2248;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2247;
  int32_t _M0L6_2acntS2377;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS612;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1696 = _M0L4selfS609->$0;
  moonbit_incref(_M0L5entryS611);
  _M0L6_2atmpS1697 = _M0L5entryS611;
  if (
    _M0L8new__idxS610 < 0
    || _M0L8new__idxS610 >= Moonbit_array_length(_M0L7entriesS1696)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2248
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1696[
      _M0L8new__idxS610
    ];
  if (_M0L6_2aoldS2248) {
    moonbit_decref(_M0L6_2aoldS2248);
  }
  _M0L7entriesS1696[_M0L8new__idxS610] = _M0L6_2atmpS1697;
  _M0L8_2afieldS2247 = _M0L5entryS611->$1;
  _M0L6_2acntS2377 = Moonbit_object_header(_M0L5entryS611)->rc;
  if (_M0L6_2acntS2377 > 1) {
    int32_t _M0L11_2anew__cntS2380 = _M0L6_2acntS2377 - 1;
    Moonbit_object_header(_M0L5entryS611)->rc = _M0L11_2anew__cntS2380;
    if (_M0L8_2afieldS2247) {
      moonbit_incref(_M0L8_2afieldS2247);
    }
  } else if (_M0L6_2acntS2377 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2379 =
      _M0L5entryS611->$5;
    moonbit_string_t _M0L8_2afieldS2378;
    moonbit_decref(_M0L8_2afieldS2379);
    _M0L8_2afieldS2378 = _M0L5entryS611->$4;
    moonbit_decref(_M0L8_2afieldS2378);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS611);
  }
  _M0L7_2abindS612 = _M0L8_2afieldS2247;
  if (_M0L7_2abindS612 == 0) {
    if (_M0L7_2abindS612) {
      moonbit_decref(_M0L7_2abindS612);
    }
    _M0L4selfS609->$6 = _M0L8new__idxS610;
    moonbit_decref(_M0L4selfS609);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS613;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS614;
    moonbit_decref(_M0L4selfS609);
    _M0L7_2aSomeS613 = _M0L7_2abindS612;
    _M0L7_2anextS614 = _M0L7_2aSomeS613;
    _M0L7_2anextS614->$0 = _M0L8new__idxS610;
    moonbit_decref(_M0L7_2anextS614);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS615,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS617,
  int32_t _M0L8new__idxS616
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1698;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1699;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2251;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2250;
  int32_t _M0L6_2acntS2381;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS618;
  #line 185 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS1698 = _M0L4selfS615->$0;
  moonbit_incref(_M0L5entryS617);
  _M0L6_2atmpS1699 = _M0L5entryS617;
  if (
    _M0L8new__idxS616 < 0
    || _M0L8new__idxS616 >= Moonbit_array_length(_M0L7entriesS1698)
  ) {
    #line 190 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2251
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1698[
      _M0L8new__idxS616
    ];
  if (_M0L6_2aoldS2251) {
    moonbit_decref(_M0L6_2aoldS2251);
  }
  _M0L7entriesS1698[_M0L8new__idxS616] = _M0L6_2atmpS1699;
  _M0L8_2afieldS2250 = _M0L5entryS617->$1;
  _M0L6_2acntS2381 = Moonbit_object_header(_M0L5entryS617)->rc;
  if (_M0L6_2acntS2381 > 1) {
    int32_t _M0L11_2anew__cntS2383 = _M0L6_2acntS2381 - 1;
    Moonbit_object_header(_M0L5entryS617)->rc = _M0L11_2anew__cntS2383;
    if (_M0L8_2afieldS2250) {
      moonbit_incref(_M0L8_2afieldS2250);
    }
  } else if (_M0L6_2acntS2381 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2382 =
      _M0L5entryS617->$5;
    moonbit_decref(_M0L8_2afieldS2382);
    #line 191 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS617);
  }
  _M0L7_2abindS618 = _M0L8_2afieldS2250;
  if (_M0L7_2abindS618 == 0) {
    if (_M0L7_2abindS618) {
      moonbit_decref(_M0L7_2abindS618);
    }
    _M0L4selfS615->$6 = _M0L8new__idxS616;
    moonbit_decref(_M0L4selfS615);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS619;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS620;
    moonbit_decref(_M0L4selfS615);
    _M0L7_2aSomeS619 = _M0L7_2abindS618;
    _M0L7_2anextS620 = _M0L7_2aSomeS619;
    _M0L7_2anextS620->$0 = _M0L8new__idxS616;
    moonbit_decref(_M0L7_2anextS620);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS602,
  int32_t _M0L3idxS604,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS603
) {
  int32_t _M0L7_2abindS601;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1683;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1684;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2253;
  int32_t _M0L4sizeS1686;
  int32_t _M0L6_2atmpS1685;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS601 = _M0L4selfS602->$6;
  switch (_M0L7_2abindS601) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1678;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2255;
      moonbit_incref(_M0L5entryS603);
      _M0L6_2atmpS1678 = _M0L5entryS603;
      _M0L6_2aoldS2255 = _M0L4selfS602->$5;
      if (_M0L6_2aoldS2255) {
        moonbit_decref(_M0L6_2aoldS2255);
      }
      _M0L4selfS602->$5 = _M0L6_2atmpS1678;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1682 =
        _M0L4selfS602->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1681;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1679;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1680;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2256;
      if (
        _M0L7_2abindS601 < 0
        || _M0L7_2abindS601 >= Moonbit_array_length(_M0L7entriesS1682)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1681
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1682[
          _M0L7_2abindS601
        ];
      if (_M0L6_2atmpS1681) {
        moonbit_incref(_M0L6_2atmpS1681);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1679
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1681);
      moonbit_incref(_M0L5entryS603);
      _M0L6_2atmpS1680 = _M0L5entryS603;
      _M0L6_2aoldS2256 = _M0L6_2atmpS1679->$1;
      if (_M0L6_2aoldS2256) {
        moonbit_decref(_M0L6_2aoldS2256);
      }
      _M0L6_2atmpS1679->$1 = _M0L6_2atmpS1680;
      moonbit_decref(_M0L6_2atmpS1679);
      break;
    }
  }
  _M0L4selfS602->$6 = _M0L3idxS604;
  _M0L7entriesS1683 = _M0L4selfS602->$0;
  _M0L6_2atmpS1684 = _M0L5entryS603;
  if (
    _M0L3idxS604 < 0
    || _M0L3idxS604 >= Moonbit_array_length(_M0L7entriesS1683)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2253
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1683[
      _M0L3idxS604
    ];
  if (_M0L6_2aoldS2253) {
    moonbit_decref(_M0L6_2aoldS2253);
  }
  _M0L7entriesS1683[_M0L3idxS604] = _M0L6_2atmpS1684;
  _M0L4sizeS1686 = _M0L4selfS602->$1;
  _M0L6_2atmpS1685 = _M0L4sizeS1686 + 1;
  _M0L4selfS602->$1 = _M0L6_2atmpS1685;
  moonbit_decref(_M0L4selfS602);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS606,
  int32_t _M0L3idxS608,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS607
) {
  int32_t _M0L7_2abindS605;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1692;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1693;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2259;
  int32_t _M0L4sizeS1695;
  int32_t _M0L6_2atmpS1694;
  #line 443 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS605 = _M0L4selfS606->$6;
  switch (_M0L7_2abindS605) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1687;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2261;
      moonbit_incref(_M0L5entryS607);
      _M0L6_2atmpS1687 = _M0L5entryS607;
      _M0L6_2aoldS2261 = _M0L4selfS606->$5;
      if (_M0L6_2aoldS2261) {
        moonbit_decref(_M0L6_2aoldS2261);
      }
      _M0L4selfS606->$5 = _M0L6_2atmpS1687;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1691 =
        _M0L4selfS606->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1690;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1688;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1689;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2262;
      if (
        _M0L7_2abindS605 < 0
        || _M0L7_2abindS605 >= Moonbit_array_length(_M0L7entriesS1691)
      ) {
        #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1690
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1691[
          _M0L7_2abindS605
        ];
      if (_M0L6_2atmpS1690) {
        moonbit_incref(_M0L6_2atmpS1690);
      }
      #line 450 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1688
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1690);
      moonbit_incref(_M0L5entryS607);
      _M0L6_2atmpS1689 = _M0L5entryS607;
      _M0L6_2aoldS2262 = _M0L6_2atmpS1688->$1;
      if (_M0L6_2aoldS2262) {
        moonbit_decref(_M0L6_2aoldS2262);
      }
      _M0L6_2atmpS1688->$1 = _M0L6_2atmpS1689;
      moonbit_decref(_M0L6_2atmpS1688);
      break;
    }
  }
  _M0L4selfS606->$6 = _M0L3idxS608;
  _M0L7entriesS1692 = _M0L4selfS606->$0;
  _M0L6_2atmpS1693 = _M0L5entryS607;
  if (
    _M0L3idxS608 < 0
    || _M0L3idxS608 >= Moonbit_array_length(_M0L7entriesS1692)
  ) {
    #line 453 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2259
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1692[
      _M0L3idxS608
    ];
  if (_M0L6_2aoldS2259) {
    moonbit_decref(_M0L6_2aoldS2259);
  }
  _M0L7entriesS1692[_M0L3idxS608] = _M0L6_2atmpS1693;
  _M0L4sizeS1695 = _M0L4selfS606->$1;
  _M0L6_2atmpS1694 = _M0L4sizeS1695 + 1;
  _M0L4selfS606->$1 = _M0L6_2atmpS1694;
  moonbit_decref(_M0L4selfS606);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS590
) {
  int32_t _M0L8capacityS589;
  int32_t _M0L7_2abindS591;
  int32_t _M0L7_2abindS592;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1676;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS593;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS594;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_2495;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS589
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS590);
  _M0L7_2abindS591 = _M0L8capacityS589 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS592 = _M0FPB21calc__grow__threshold(_M0L8capacityS589);
  _M0L6_2atmpS1676 = 0;
  _M0L7_2abindS593
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS589, _M0L6_2atmpS1676);
  _M0L7_2abindS594 = 0;
  _block_2495
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_2495)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_2495->$0 = _M0L7_2abindS593;
  _block_2495->$1 = 0;
  _block_2495->$2 = _M0L8capacityS589;
  _block_2495->$3 = _M0L7_2abindS591;
  _block_2495->$4 = _M0L7_2abindS592;
  _block_2495->$5 = _M0L7_2abindS594;
  _block_2495->$6 = -1;
  return _block_2495;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS596
) {
  int32_t _M0L8capacityS595;
  int32_t _M0L7_2abindS597;
  int32_t _M0L7_2abindS598;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1677;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS599;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS600;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_2496;
  #line 57 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS595
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS596);
  _M0L7_2abindS597 = _M0L8capacityS595 - 1;
  #line 63 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS598 = _M0FPB21calc__grow__threshold(_M0L8capacityS595);
  _M0L6_2atmpS1677 = 0;
  _M0L7_2abindS599
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS595, _M0L6_2atmpS1677);
  _M0L7_2abindS600 = 0;
  _block_2496
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_2496)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_2496->$0 = _M0L7_2abindS599;
  _block_2496->$1 = 0;
  _block_2496->$2 = _M0L8capacityS595;
  _block_2496->$3 = _M0L7_2abindS597;
  _block_2496->$4 = _M0L7_2abindS598;
  _block_2496->$5 = _M0L7_2abindS600;
  _block_2496->$6 = -1;
  return _block_2496;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS588) {
  #line 33 "/Users/user/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS588 >= 0) {
    int32_t _M0L6_2atmpS1675;
    int32_t _M0L6_2atmpS1674;
    int32_t _M0L6_2atmpS1673;
    int32_t _M0L6_2atmpS1672;
    if (_M0L4selfS588 <= 1) {
      return 1;
    }
    if (_M0L4selfS588 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1675 = _M0L4selfS588 - 1;
    #line 44 "/Users/user/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS1674 = moonbit_clz32(_M0L6_2atmpS1675);
    _M0L6_2atmpS1673 = _M0L6_2atmpS1674 - 1;
    _M0L6_2atmpS1672 = 2147483647 >> (_M0L6_2atmpS1673 & 31);
    return _M0L6_2atmpS1672 + 1;
  } else {
    #line 34 "/Users/user/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS587) {
  int32_t _M0L6_2atmpS1671;
  #line 510 "/Users/user/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1671 = _M0L8capacityS587 * 13;
  return _M0L6_2atmpS1671 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS583
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS583 == 0) {
    if (_M0L4selfS583) {
      moonbit_decref(_M0L4selfS583);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS584 =
      _M0L4selfS583;
    return _M0L7_2aSomeS584;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS585
) {
  #line 38 "/Users/user/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS585 == 0) {
    if (_M0L4selfS585) {
      moonbit_decref(_M0L4selfS585);
    }
    #line 40 "/Users/user/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS586 =
      _M0L4selfS585;
    return _M0L7_2aSomeS586;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS582
) {
  moonbit_string_t* _M0L6_2atmpS1670;
  #line 165 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS1670 = _M0L4selfS582;
  #line 167 "/Users/user/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1670);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS581
) {
  moonbit_string_t* _M0L6_2atmpS1668;
  int32_t _M0L6_2atmpS1669;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1667;
  #line 1509 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS581);
  _M0L6_2atmpS1668 = _M0L4selfS581;
  _M0L6_2atmpS1669 = Moonbit_array_length(_M0L4selfS581);
  moonbit_decref(_M0L4selfS581);
  _M0L6_2atmpS1667
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1669, _M0L6_2atmpS1668
  };
  #line 1511 "/Users/user/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1667);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS579
) {
  struct _M0TPB8MutLocalGiE* _M0L1iS578;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__* _closure_2497;
  struct _M0TWEOs* _M0L6_2atmpS1655;
  #line 677 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS578
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS578)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB8MutLocalGiE) >> 2, 0, 0);
  _M0L1iS578->$0 = 0;
  _closure_2497
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__));
  Moonbit_object_header(_closure_2497)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__, $0_0) >> 2, 2, 0);
  _closure_2497->code = &_M0MPC15array9ArrayView4iterGsEC1656l680;
  _closure_2497->$0_0 = _M0L4selfS579.$0;
  _closure_2497->$0_1 = _M0L4selfS579.$1;
  _closure_2497->$0_2 = _M0L4selfS579.$2;
  _closure_2497->$1 = _M0L1iS578;
  _M0L6_2atmpS1655 = (struct _M0TWEOs*)_closure_2497;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1655);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1656l680(
  struct _M0TWEOs* _M0L6_2aenvS1657
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__* _M0L14_2acasted__envS1658;
  struct _M0TPB8MutLocalGiE* _M0L1iS578;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2267;
  int32_t _M0L6_2acntS2384;
  struct _M0TPB9ArrayViewGsE _M0L4selfS579;
  int32_t _M0L3valS1659;
  int32_t _M0L6_2atmpS1660;
  #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1658
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1656__l680__*)_M0L6_2aenvS1657;
  _M0L1iS578 = _M0L14_2acasted__envS1658->$1;
  _M0L8_2afieldS2267
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1658->$0_1,
      _M0L14_2acasted__envS1658->$0_2,
      _M0L14_2acasted__envS1658->$0_0
  };
  _M0L6_2acntS2384 = Moonbit_object_header(_M0L14_2acasted__envS1658)->rc;
  if (_M0L6_2acntS2384 > 1) {
    int32_t _M0L11_2anew__cntS2385 = _M0L6_2acntS2384 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1658)->rc
    = _M0L11_2anew__cntS2385;
    moonbit_incref(_M0L1iS578);
    moonbit_incref(_M0L8_2afieldS2267.$0);
  } else if (_M0L6_2acntS2384 == 1) {
    #line 680 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1658);
  }
  _M0L4selfS579 = _M0L8_2afieldS2267;
  _M0L3valS1659 = _M0L1iS578->$0;
  moonbit_incref(_M0L4selfS579.$0);
  #line 681 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1660 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS579);
  if (_M0L3valS1659 < _M0L6_2atmpS1660) {
    moonbit_string_t* _M0L3bufS1663 = _M0L4selfS579.$0;
    int32_t _M0L5startS1665 = _M0L4selfS579.$1;
    int32_t _M0L3valS1666 = _M0L1iS578->$0;
    int32_t _M0L6_2atmpS1664 = _M0L5startS1665 + _M0L3valS1666;
    moonbit_string_t _M0L6_2atmpS2265 =
      (moonbit_string_t)_M0L3bufS1663[_M0L6_2atmpS1664];
    moonbit_string_t _M0L4elemS580;
    int32_t _M0L3valS1662;
    int32_t _M0L6_2atmpS1661;
    moonbit_incref(_M0L6_2atmpS2265);
    moonbit_decref(_M0L3bufS1663);
    _M0L4elemS580 = _M0L6_2atmpS2265;
    _M0L3valS1662 = _M0L1iS578->$0;
    _M0L6_2atmpS1661 = _M0L3valS1662 + 1;
    _M0L1iS578->$0 = _M0L6_2atmpS1661;
    moonbit_decref(_M0L1iS578);
    return _M0L4elemS580;
  } else {
    moonbit_decref(_M0L4selfS579.$0);
    moonbit_decref(_M0L1iS578);
    return 0;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS576,
  struct _M0TPB6Logger _M0L6loggerS577
) {
  int32_t _M0L6_2atmpS1654;
  struct _M0TPC16string10StringView _M0L6_2atmpS1653;
  #line 244 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1654 = Moonbit_array_length(_M0L4selfS576);
  _M0L6_2atmpS1653
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1654, _M0L4selfS576
  };
  #line 245 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1653, _M0L6loggerS577, 1);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS575) {
  #line 45 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 46 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS575, 10);
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS574,
  struct _M0TPB6Logger _M0L6loggerS573
) {
  moonbit_string_t _M0L6_2atmpS1652;
  #line 40 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 41 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1652 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS574, 10);
  #line 41 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS573.$0->$method_0(_M0L6loggerS573.$1, _M0L6_2atmpS1652);
  return 0;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS572
) {
  int32_t _M0L6_2atmpS1651;
  #line 24 "/Users/user/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS1651 = Moonbit_array_length(_M0L4selfS572);
  return (struct _M0TPC16string10StringView){0,
                                               _M0L6_2atmpS1651,
                                               _M0L4selfS572};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS563,
  moonbit_string_t _M0L5valueS565
) {
  int32_t _M0L3lenS1636;
  moonbit_string_t* _M0L6_2atmpS1638;
  int32_t _M0L6_2atmpS1637;
  int32_t _M0L6lengthS564;
  moonbit_string_t* _M0L3bufS1639;
  moonbit_string_t _M0L6_2aoldS2269;
  int32_t _M0L6_2atmpS1640;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1636 = _M0L4selfS563->$1;
  moonbit_incref(_M0L4selfS563);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1638 = _M0MPC15array5Array6bufferGsE(_M0L4selfS563);
  _M0L6_2atmpS1637 = Moonbit_array_length(_M0L6_2atmpS1638);
  moonbit_decref(_M0L6_2atmpS1638);
  if (_M0L3lenS1636 == _M0L6_2atmpS1637) {
    moonbit_incref(_M0L4selfS563);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS563);
  }
  _M0L6lengthS564 = _M0L4selfS563->$1;
  _M0L3bufS1639 = _M0L4selfS563->$0;
  _M0L6_2aoldS2269 = (moonbit_string_t)_M0L3bufS1639[_M0L6lengthS564];
  moonbit_decref(_M0L6_2aoldS2269);
  _M0L3bufS1639[_M0L6lengthS564] = _M0L5valueS565;
  _M0L6_2atmpS1640 = _M0L6lengthS564 + 1;
  _M0L4selfS563->$1 = _M0L6_2atmpS1640;
  moonbit_decref(_M0L4selfS563);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS566,
  struct _M0TUsiE* _M0L5valueS568
) {
  int32_t _M0L3lenS1641;
  struct _M0TUsiE** _M0L6_2atmpS1643;
  int32_t _M0L6_2atmpS1642;
  int32_t _M0L6lengthS567;
  struct _M0TUsiE** _M0L3bufS1644;
  struct _M0TUsiE* _M0L6_2aoldS2271;
  int32_t _M0L6_2atmpS1645;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1641 = _M0L4selfS566->$1;
  moonbit_incref(_M0L4selfS566);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1643 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS566);
  _M0L6_2atmpS1642 = Moonbit_array_length(_M0L6_2atmpS1643);
  moonbit_decref(_M0L6_2atmpS1643);
  if (_M0L3lenS1641 == _M0L6_2atmpS1642) {
    moonbit_incref(_M0L4selfS566);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS566);
  }
  _M0L6lengthS567 = _M0L4selfS566->$1;
  _M0L3bufS1644 = _M0L4selfS566->$0;
  _M0L6_2aoldS2271 = (struct _M0TUsiE*)_M0L3bufS1644[_M0L6lengthS567];
  if (_M0L6_2aoldS2271) {
    moonbit_decref(_M0L6_2aoldS2271);
  }
  _M0L3bufS1644[_M0L6lengthS567] = _M0L5valueS568;
  _M0L6_2atmpS1645 = _M0L6lengthS567 + 1;
  _M0L4selfS566->$1 = _M0L6_2atmpS1645;
  moonbit_decref(_M0L4selfS566);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L4selfS569,
  void* _M0L5valueS571
) {
  int32_t _M0L3lenS1646;
  void** _M0L6_2atmpS1648;
  int32_t _M0L6_2atmpS1647;
  int32_t _M0L6lengthS570;
  void** _M0L3bufS1649;
  void* _M0L6_2aoldS2273;
  int32_t _M0L6_2atmpS1650;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1646 = _M0L4selfS569->$1;
  moonbit_incref(_M0L4selfS569);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1648
  = _M0MPC15array5Array6bufferGRP36mulpjs4mulp11log__events8LogEventE(_M0L4selfS569);
  _M0L6_2atmpS1647 = Moonbit_array_length(_M0L6_2atmpS1648);
  moonbit_decref(_M0L6_2atmpS1648);
  if (_M0L3lenS1646 == _M0L6_2atmpS1647) {
    moonbit_incref(_M0L4selfS569);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP36mulpjs4mulp11log__events8LogEventE(_M0L4selfS569);
  }
  _M0L6lengthS570 = _M0L4selfS569->$1;
  _M0L3bufS1649 = _M0L4selfS569->$0;
  _M0L6_2aoldS2273 = (void*)_M0L3bufS1649[_M0L6lengthS570];
  moonbit_decref(_M0L6_2aoldS2273);
  _M0L3bufS1649[_M0L6lengthS570] = _M0L5valueS571;
  _M0L6_2atmpS1650 = _M0L6lengthS570 + 1;
  _M0L4selfS569->$1 = _M0L6_2atmpS1650;
  moonbit_decref(_M0L4selfS569);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS555) {
  int32_t _M0L8old__capS554;
  int32_t _M0L8new__capS556;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS554 = _M0L4selfS555->$1;
  if (_M0L8old__capS554 == 0) {
    _M0L8new__capS556 = 8;
  } else {
    _M0L8new__capS556 = _M0L8old__capS554 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS555, _M0L8new__capS556);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS558
) {
  int32_t _M0L8old__capS557;
  int32_t _M0L8new__capS559;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS557 = _M0L4selfS558->$1;
  if (_M0L8old__capS557 == 0) {
    _M0L8new__capS559 = 8;
  } else {
    _M0L8new__capS559 = _M0L8old__capS557 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS558, _M0L8new__capS559);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L4selfS561
) {
  int32_t _M0L8old__capS560;
  int32_t _M0L8new__capS562;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS560 = _M0L4selfS561->$1;
  if (_M0L8old__capS560 == 0) {
    _M0L8new__capS562 = 8;
  } else {
    _M0L8new__capS562 = _M0L8old__capS560 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp11log__events8LogEventE(_M0L4selfS561, _M0L8new__capS562);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS539,
  int32_t _M0L13new__capacityS537
) {
  moonbit_string_t* _M0L8new__bufS536;
  moonbit_string_t* _M0L8old__bufS538;
  int32_t _M0L8old__capS540;
  int32_t _M0L9copy__lenS541;
  moonbit_string_t* _M0L6_2aoldS2275;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS536
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS537, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8old__bufS538 = _M0L4selfS539->$0;
  _M0L8old__capS540 = Moonbit_array_length(_M0L8old__bufS538);
  if (_M0L8old__capS540 < _M0L13new__capacityS537) {
    _M0L9copy__lenS541 = _M0L8old__capS540;
  } else {
    _M0L9copy__lenS541 = _M0L13new__capacityS537;
  }
  moonbit_incref(_M0L8old__bufS538);
  moonbit_incref(_M0L8new__bufS536);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS536, 0, _M0L8old__bufS538, 0, _M0L9copy__lenS541);
  _M0L6_2aoldS2275 = _M0L4selfS539->$0;
  moonbit_decref(_M0L6_2aoldS2275);
  _M0L4selfS539->$0 = _M0L8new__bufS536;
  moonbit_decref(_M0L4selfS539);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS545,
  int32_t _M0L13new__capacityS543
) {
  struct _M0TUsiE** _M0L8new__bufS542;
  struct _M0TUsiE** _M0L8old__bufS544;
  int32_t _M0L8old__capS546;
  int32_t _M0L9copy__lenS547;
  struct _M0TUsiE** _M0L6_2aoldS2277;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS542
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS543, 0);
  _M0L8old__bufS544 = _M0L4selfS545->$0;
  _M0L8old__capS546 = Moonbit_array_length(_M0L8old__bufS544);
  if (_M0L8old__capS546 < _M0L13new__capacityS543) {
    _M0L9copy__lenS547 = _M0L8old__capS546;
  } else {
    _M0L9copy__lenS547 = _M0L13new__capacityS543;
  }
  moonbit_incref(_M0L8old__bufS544);
  moonbit_incref(_M0L8new__bufS542);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS542, 0, _M0L8old__bufS544, 0, _M0L9copy__lenS547);
  _M0L6_2aoldS2277 = _M0L4selfS545->$0;
  moonbit_decref(_M0L6_2aoldS2277);
  _M0L4selfS545->$0 = _M0L8new__bufS542;
  moonbit_decref(_M0L4selfS545);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L4selfS551,
  int32_t _M0L13new__capacityS549
) {
  void** _M0L8new__bufS548;
  void** _M0L8old__bufS550;
  int32_t _M0L8old__capS552;
  int32_t _M0L9copy__lenS553;
  void** _M0L6_2aoldS2279;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS548
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS549, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8old__bufS550 = _M0L4selfS551->$0;
  _M0L8old__capS552 = Moonbit_array_length(_M0L8old__bufS550);
  if (_M0L8old__capS552 < _M0L13new__capacityS549) {
    _M0L9copy__lenS553 = _M0L8old__capS552;
  } else {
    _M0L9copy__lenS553 = _M0L13new__capacityS549;
  }
  moonbit_incref(_M0L8old__bufS550);
  moonbit_incref(_M0L8new__bufS548);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp11log__events8LogEventE(_M0L8new__bufS548, 0, _M0L8old__bufS550, 0, _M0L9copy__lenS553);
  _M0L6_2aoldS2279 = _M0L4selfS551->$0;
  moonbit_decref(_M0L6_2aoldS2279);
  _M0L4selfS551->$0 = _M0L8new__bufS548;
  moonbit_decref(_M0L4selfS551);
  return 0;
}

int32_t _M0MPC15array5Array6lengthGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L4selfS535
) {
  int32_t _result_2498;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_2498 = _M0L4selfS535->$1;
  moonbit_decref(_M0L4selfS535);
  return _result_2498;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS532
) {
  moonbit_string_t* _M0L8_2afieldS2281;
  int32_t _M0L6_2acntS2386;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2281 = _M0L4selfS532->$0;
  _M0L6_2acntS2386 = Moonbit_object_header(_M0L4selfS532)->rc;
  if (_M0L6_2acntS2386 > 1) {
    int32_t _M0L11_2anew__cntS2387 = _M0L6_2acntS2386 - 1;
    Moonbit_object_header(_M0L4selfS532)->rc = _M0L11_2anew__cntS2387;
    moonbit_incref(_M0L8_2afieldS2281);
  } else if (_M0L6_2acntS2386 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS532);
  }
  return _M0L8_2afieldS2281;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS533
) {
  struct _M0TUsiE** _M0L8_2afieldS2282;
  int32_t _M0L6_2acntS2388;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2282 = _M0L4selfS533->$0;
  _M0L6_2acntS2388 = Moonbit_object_header(_M0L4selfS533)->rc;
  if (_M0L6_2acntS2388 > 1) {
    int32_t _M0L11_2anew__cntS2389 = _M0L6_2acntS2388 - 1;
    Moonbit_object_header(_M0L4selfS533)->rc = _M0L11_2anew__cntS2389;
    moonbit_incref(_M0L8_2afieldS2282);
  } else if (_M0L6_2acntS2388 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS533);
  }
  return _M0L8_2afieldS2282;
}

void** _M0MPC15array5Array6bufferGRP36mulpjs4mulp11log__events8LogEventE(
  struct _M0TPB5ArrayGRP36mulpjs4mulp11log__events8LogEventE* _M0L4selfS534
) {
  void** _M0L8_2afieldS2283;
  int32_t _M0L6_2acntS2390;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2283 = _M0L4selfS534->$0;
  _M0L6_2acntS2390 = Moonbit_object_header(_M0L4selfS534)->rc;
  if (_M0L6_2acntS2390 > 1) {
    int32_t _M0L11_2anew__cntS2391 = _M0L6_2acntS2390 - 1;
    Moonbit_object_header(_M0L4selfS534)->rc = _M0L11_2anew__cntS2391;
    moonbit_incref(_M0L8_2afieldS2283);
  } else if (_M0L6_2acntS2390 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS534);
  }
  return _M0L8_2afieldS2283;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS531
) {
  #line 53 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS531 == 0) {
    moonbit_string_t* _M0L6_2atmpS1634 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2499 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2499)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2499->$0 = _M0L6_2atmpS1634;
    _block_2499->$1 = 0;
    return _block_2499;
  } else {
    moonbit_string_t* _M0L6_2atmpS1635 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS531, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2500 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2500)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2500->$0 = _M0L6_2atmpS1635;
    _block_2500->$1 = 0;
    return _block_2500;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS530
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS530;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS529,
  struct _M0TPC16string10StringView _M0L3strS528
) {
  int32_t _M0L8str__lenS527;
  int32_t _M0L3lenS1627;
  int32_t _M0L6_2atmpS1626;
  uint16_t* _M0L4dataS1628;
  int32_t _M0L3lenS1629;
  moonbit_string_t _M0L6_2atmpS1630;
  int32_t _M0L6_2atmpS1631;
  int32_t _M0L3lenS1633;
  int32_t _M0L6_2atmpS1632;
  #line 126 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  moonbit_incref(_M0L3strS528.$0);
  #line 130 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS527 = _M0MPC16string10StringView6length(_M0L3strS528);
  _M0L3lenS1627 = _M0L4selfS529->$1;
  _M0L6_2atmpS1626 = _M0L3lenS1627 + _M0L8str__lenS527;
  moonbit_incref(_M0L4selfS529);
  #line 131 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS529, _M0L6_2atmpS1626);
  _M0L4dataS1628 = _M0L4selfS529->$0;
  _M0L3lenS1629 = _M0L4selfS529->$1;
  moonbit_incref(_M0L4dataS1628);
  moonbit_incref(_M0L3strS528.$0);
  #line 134 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1630 = _M0MPC16string10StringView4data(_M0L3strS528);
  #line 135 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1631 = _M0MPC16string10StringView13start__offset(_M0L3strS528);
  #line 132 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1628, _M0L3lenS1629, _M0L6_2atmpS1630, _M0L6_2atmpS1631, _M0L8str__lenS527);
  _M0L3lenS1633 = _M0L4selfS529->$1;
  _M0L6_2atmpS1632 = _M0L3lenS1633 + _M0L8str__lenS527;
  _M0L4selfS529->$1 = _M0L6_2atmpS1632;
  moonbit_decref(_M0L4selfS529);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS519,
  int32_t _M0L3lenS522,
  int32_t _M0L13start__offsetS526,
  int64_t _M0L11end__offsetS517
) {
  int32_t _M0L11end__offsetS516;
  int32_t _M0L5indexS520;
  int32_t _M0L5countS521;
  #line 441 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS517 == 4294967296ll) {
    _M0L11end__offsetS516 = Moonbit_array_length(_M0L4selfS519);
  } else {
    int64_t _M0L7_2aSomeS518 = _M0L11end__offsetS517;
    _M0L11end__offsetS516 = (int32_t)_M0L7_2aSomeS518;
  }
  _M0L5indexS520 = _M0L13start__offsetS526;
  _M0L5countS521 = 0;
  while (1) {
    int32_t _if__result_2502;
    if (_M0L5indexS520 < _M0L11end__offsetS516) {
      _if__result_2502 = _M0L5countS521 < _M0L3lenS522;
    } else {
      _if__result_2502 = 0;
    }
    if (_if__result_2502) {
      int32_t _M0L2c1S523 = _M0L4selfS519[_M0L5indexS520];
      int32_t _if__result_2503;
      int32_t _M0L6_2atmpS1624;
      int32_t _M0L6_2atmpS1625;
      #line 452 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S523)) {
        int32_t _M0L6_2atmpS1620 = _M0L5indexS520 + 1;
        _if__result_2503 = _M0L6_2atmpS1620 < _M0L11end__offsetS516;
      } else {
        _if__result_2503 = 0;
      }
      if (_if__result_2503) {
        int32_t _M0L6_2atmpS1623 = _M0L5indexS520 + 1;
        int32_t _M0L2c2S524 = _M0L4selfS519[_M0L6_2atmpS1623];
        #line 454 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S524)) {
          int32_t _M0L6_2atmpS1621 = _M0L5indexS520 + 2;
          int32_t _M0L6_2atmpS1622 = _M0L5countS521 + 1;
          _M0L5indexS520 = _M0L6_2atmpS1621;
          _M0L5countS521 = _M0L6_2atmpS1622;
          continue;
        } else {
          #line 457 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_63.data);
        }
      }
      _M0L6_2atmpS1624 = _M0L5indexS520 + 1;
      _M0L6_2atmpS1625 = _M0L5countS521 + 1;
      _M0L5indexS520 = _M0L6_2atmpS1624;
      _M0L5countS521 = _M0L6_2atmpS1625;
      continue;
    } else {
      moonbit_decref(_M0L4selfS519);
      return _M0L5countS521 >= _M0L3lenS522;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS508,
  int32_t _M0L3lenS511,
  int32_t _M0L13start__offsetS515,
  int64_t _M0L11end__offsetS506
) {
  int32_t _M0L11end__offsetS505;
  int32_t _M0L5indexS509;
  int32_t _M0L5countS510;
  #line 413 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS506 == 4294967296ll) {
    _M0L11end__offsetS505 = Moonbit_array_length(_M0L4selfS508);
  } else {
    int64_t _M0L7_2aSomeS507 = _M0L11end__offsetS506;
    _M0L11end__offsetS505 = (int32_t)_M0L7_2aSomeS507;
  }
  _M0L5indexS509 = _M0L13start__offsetS515;
  _M0L5countS510 = 0;
  while (1) {
    int32_t _if__result_2505;
    if (_M0L5indexS509 < _M0L11end__offsetS505) {
      _if__result_2505 = _M0L5countS510 < _M0L3lenS511;
    } else {
      _if__result_2505 = 0;
    }
    if (_if__result_2505) {
      int32_t _M0L2c1S512 = _M0L4selfS508[_M0L5indexS509];
      int32_t _if__result_2506;
      int32_t _M0L6_2atmpS1618;
      int32_t _M0L6_2atmpS1619;
      #line 424 "/Users/user/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S512)) {
        int32_t _M0L6_2atmpS1614 = _M0L5indexS509 + 1;
        _if__result_2506 = _M0L6_2atmpS1614 < _M0L11end__offsetS505;
      } else {
        _if__result_2506 = 0;
      }
      if (_if__result_2506) {
        int32_t _M0L6_2atmpS1617 = _M0L5indexS509 + 1;
        int32_t _M0L2c2S513 = _M0L4selfS508[_M0L6_2atmpS1617];
        #line 426 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S513)) {
          int32_t _M0L6_2atmpS1615 = _M0L5indexS509 + 2;
          int32_t _M0L6_2atmpS1616 = _M0L5countS510 + 1;
          _M0L5indexS509 = _M0L6_2atmpS1615;
          _M0L5countS510 = _M0L6_2atmpS1616;
          continue;
        } else {
          #line 429 "/Users/user/.moon/lib/core/builtin/string.mbt"
          _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_63.data);
        }
      }
      _M0L6_2atmpS1618 = _M0L5indexS509 + 1;
      _M0L6_2atmpS1619 = _M0L5countS510 + 1;
      _M0L5indexS509 = _M0L6_2atmpS1618;
      _M0L5countS510 = _M0L6_2atmpS1619;
      continue;
    } else {
      moonbit_decref(_M0L4selfS508);
      if (_M0L5countS510 == _M0L3lenS511) {
        return _M0L5indexS509 == _M0L11end__offsetS505;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS502
) {
  int32_t _M0L3endS1608;
  int32_t _M0L5startS1609;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1608 = _M0L4selfS502.$2;
  _M0L5startS1609 = _M0L4selfS502.$1;
  moonbit_decref(_M0L4selfS502.$0);
  return _M0L3endS1608 - _M0L5startS1609;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS503
) {
  int32_t _M0L3endS1610;
  int32_t _M0L5startS1611;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1610 = _M0L4selfS503.$2;
  _M0L5startS1611 = _M0L4selfS503.$1;
  moonbit_decref(_M0L4selfS503.$0);
  return _M0L3endS1610 - _M0L5startS1611;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS504
) {
  int32_t _M0L3endS1612;
  int32_t _M0L5startS1613;
  #line 74 "/Users/user/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1612 = _M0L4selfS504.$2;
  _M0L5startS1613 = _M0L4selfS504.$1;
  moonbit_decref(_M0L4selfS504.$0);
  return _M0L3endS1612 - _M0L5startS1613;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS500,
  int64_t _M0L19start__offset_2eoptS498,
  int64_t _M0L11end__offsetS501
) {
  int32_t _M0L13start__offsetS497;
  if (_M0L19start__offset_2eoptS498 == 4294967296ll) {
    _M0L13start__offsetS497 = 0;
  } else {
    int64_t _M0L7_2aSomeS499 = _M0L19start__offset_2eoptS498;
    _M0L13start__offsetS497 = (int32_t)_M0L7_2aSomeS499;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS500, _M0L13start__offsetS497, _M0L11end__offsetS501);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS495,
  int32_t _M0L13start__offsetS496,
  int64_t _M0L11end__offsetS493
) {
  int32_t _M0L11end__offsetS492;
  int32_t _if__result_2507;
  #line 512 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS493 == 4294967296ll) {
    _M0L11end__offsetS492 = Moonbit_array_length(_M0L4selfS495);
  } else {
    int64_t _M0L7_2aSomeS494 = _M0L11end__offsetS493;
    _M0L11end__offsetS492 = (int32_t)_M0L7_2aSomeS494;
  }
  if (_M0L13start__offsetS496 >= 0) {
    if (_M0L13start__offsetS496 <= _M0L11end__offsetS492) {
      int32_t _M0L6_2atmpS1607 = Moonbit_array_length(_M0L4selfS495);
      _if__result_2507 = _M0L11end__offsetS492 <= _M0L6_2atmpS1607;
    } else {
      _if__result_2507 = 0;
    }
  } else {
    _if__result_2507 = 0;
  }
  if (_if__result_2507) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS496,
                                                 _M0L11end__offsetS492,
                                                 _M0L4selfS495};
  } else {
    moonbit_decref(_M0L4selfS495);
    #line 521 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_64.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS491
) {
  #line 197 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 198 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string10StringView9to__owned(_M0L4selfS491);
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS490
) {
  moonbit_string_t _M0L3strS1604;
  int32_t _M0L5startS1605;
  int32_t _M0L3endS1606;
  #line 190 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1604 = _M0L4selfS490.$0;
  _M0L5startS1605 = _M0L4selfS490.$1;
  _M0L3endS1606 = _M0L4selfS490.$2;
  #line 193 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1604, _M0L5startS1605, _M0L3endS1606);
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS487,
  int32_t _M0L5startS485,
  int32_t _M0L3endS486
) {
  int32_t _if__result_2508;
  int32_t _M0L3lenS488;
  int32_t _M0L6_2atmpS1602;
  int32_t _M0L6_2atmpS1603;
  moonbit_bytes_t _M0L5bytesS489;
  moonbit_bytes_t _M0L6_2atmpS1601;
  #line 91 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS485 == 0) {
    int32_t _M0L6_2atmpS1600 = Moonbit_array_length(_M0L3strS487);
    _if__result_2508 = _M0L3endS486 == _M0L6_2atmpS1600;
  } else {
    _if__result_2508 = 0;
  }
  if (_if__result_2508) {
    return _M0L3strS487;
  }
  _M0L3lenS488 = _M0L3endS486 - _M0L5startS485;
  _M0L6_2atmpS1602 = _M0L3lenS488 * 2;
  #line 101 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1603 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS489
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1602, _M0L6_2atmpS1603);
  moonbit_incref(_M0L5bytesS489);
  #line 102 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS489, 0, _M0L3strS487, _M0L5startS485, _M0L3lenS488);
  _M0L6_2atmpS1601 = _M0L5bytesS489;
  #line 103 "/Users/user/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1601, 0, 4294967296ll);
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS480,
  int32_t _M0L6offsetS484,
  int64_t _M0L6lengthS482
) {
  int32_t _M0L3lenS479;
  int32_t _M0L6lengthS481;
  int32_t _if__result_2509;
  #line 76 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS479 = Moonbit_array_length(_M0L4selfS480);
  if (_M0L6lengthS482 == 4294967296ll) {
    _M0L6lengthS481 = _M0L3lenS479 - _M0L6offsetS484;
  } else {
    int64_t _M0L7_2aSomeS483 = _M0L6lengthS482;
    _M0L6lengthS481 = (int32_t)_M0L7_2aSomeS483;
  }
  if (_M0L6offsetS484 >= 0) {
    if (_M0L6lengthS481 >= 0) {
      int32_t _M0L6_2atmpS1599 = _M0L6offsetS484 + _M0L6lengthS481;
      _if__result_2509 = _M0L6_2atmpS1599 <= _M0L3lenS479;
    } else {
      _if__result_2509 = 0;
    }
  } else {
    _if__result_2509 = 0;
  }
  if (_if__result_2509) {
    #line 84 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS480, _M0L6offsetS484, _M0L6lengthS481);
  } else {
    moonbit_decref(_M0L4selfS480);
    #line 83 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS471,
  int32_t _M0L13bytes__offsetS466,
  moonbit_string_t _M0L3strS473,
  int32_t _M0L11str__offsetS469,
  int32_t _M0L6lengthS467
) {
  int32_t _M0L6_2atmpS1598;
  int32_t _M0L6_2atmpS1597;
  int32_t _M0L2e1S465;
  int32_t _M0L6_2atmpS1596;
  int32_t _M0L2e2S468;
  int32_t _M0L4len1S470;
  int32_t _M0L4len2S472;
  int32_t _if__result_2510;
  #line 124 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1598 = _M0L6lengthS467 * 2;
  _M0L6_2atmpS1597 = _M0L13bytes__offsetS466 + _M0L6_2atmpS1598;
  _M0L2e1S465 = _M0L6_2atmpS1597 - 1;
  _M0L6_2atmpS1596 = _M0L11str__offsetS469 + _M0L6lengthS467;
  _M0L2e2S468 = _M0L6_2atmpS1596 - 1;
  _M0L4len1S470 = Moonbit_array_length(_M0L4selfS471);
  _M0L4len2S472 = Moonbit_array_length(_M0L3strS473);
  if (_M0L6lengthS467 >= 0) {
    if (_M0L13bytes__offsetS466 >= 0) {
      if (_M0L2e1S465 < _M0L4len1S470) {
        if (_M0L11str__offsetS469 >= 0) {
          _if__result_2510 = _M0L2e2S468 < _M0L4len2S472;
        } else {
          _if__result_2510 = 0;
        }
      } else {
        _if__result_2510 = 0;
      }
    } else {
      _if__result_2510 = 0;
    }
  } else {
    _if__result_2510 = 0;
  }
  if (_if__result_2510) {
    int32_t _M0L16end__str__offsetS474 =
      _M0L11str__offsetS469 + _M0L6lengthS467;
    int32_t _M0L1iS475 = _M0L11str__offsetS469;
    int32_t _M0L1jS476 = _M0L13bytes__offsetS466;
    while (1) {
      if (_M0L1iS475 < _M0L16end__str__offsetS474) {
        int32_t _M0L6_2atmpS1593 = _M0L3strS473[_M0L1iS475];
        int32_t _M0L6_2atmpS1592 = (int32_t)_M0L6_2atmpS1593;
        uint32_t _M0L1cS477 = *(uint32_t*)&_M0L6_2atmpS1592;
        uint32_t _M0L6_2atmpS1588 = _M0L1cS477 & 255u;
        int32_t _M0L6_2atmpS1587;
        int32_t _M0L6_2atmpS1589;
        uint32_t _M0L6_2atmpS1591;
        int32_t _M0L6_2atmpS1590;
        int32_t _M0L6_2atmpS1594;
        int32_t _M0L6_2atmpS1595;
        #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1587 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1588);
        if (
          _M0L1jS476 < 0 || _M0L1jS476 >= Moonbit_array_length(_M0L4selfS471)
        ) {
          #line 141 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS471[_M0L1jS476] = _M0L6_2atmpS1587;
        _M0L6_2atmpS1589 = _M0L1jS476 + 1;
        _M0L6_2atmpS1591 = _M0L1cS477 >> 8;
        #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1590 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1591);
        if (
          _M0L6_2atmpS1589 < 0
          || _M0L6_2atmpS1589 >= Moonbit_array_length(_M0L4selfS471)
        ) {
          #line 142 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS471[_M0L6_2atmpS1589] = _M0L6_2atmpS1590;
        _M0L6_2atmpS1594 = _M0L1iS475 + 1;
        _M0L6_2atmpS1595 = _M0L1jS476 + 2;
        _M0L1iS475 = _M0L6_2atmpS1594;
        _M0L1jS476 = _M0L6_2atmpS1595;
        continue;
      } else {
        moonbit_decref(_M0L3strS473);
        moonbit_decref(_M0L4selfS471);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS473);
    moonbit_decref(_M0L4selfS471);
    #line 137 "/Users/user/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS464) {
  int32_t _M0L6_2atmpS1586;
  #line 2518 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1586 = *(int32_t*)&_M0L4selfS464;
  return _M0L6_2atmpS1586 & 0xff;
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS462,
  struct _M0TPB6Logger _M0L6loggerS463
) {
  #line 166 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  #line 167 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L4selfS462, _M0L6loggerS463, 1);
  return 0;
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS461) {
  #line 207 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS461;
}

moonbit_string_t _M0MPC15int645Int6418to__string_2einner(
  int64_t _M0L4selfS445,
  int32_t _M0L5radixS444
) {
  int32_t _if__result_2512;
  int32_t _M0L12is__negativeS446;
  uint64_t _M0L3numS447;
  uint16_t* _M0L6bufferS448;
  #line 548 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS444 < 2) {
    _if__result_2512 = 1;
  } else {
    _if__result_2512 = _M0L5radixS444 > 36;
  }
  if (_if__result_2512) {
    #line 552 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_65.data);
  }
  if (_M0L4selfS445 == 0ll) {
    return (moonbit_string_t)moonbit_string_literal_66.data;
  }
  _M0L12is__negativeS446 = _M0L4selfS445 < 0ll;
  if (_M0L12is__negativeS446) {
    int64_t _M0L6_2atmpS1585 = -_M0L4selfS445;
    _M0L3numS447 = *(uint64_t*)&_M0L6_2atmpS1585;
  } else {
    _M0L3numS447 = *(uint64_t*)&_M0L4selfS445;
  }
  switch (_M0L5radixS444) {
    case 10: {
      int32_t _M0L10digit__lenS449;
      int32_t _M0L6_2atmpS1582;
      int32_t _M0L10total__lenS450;
      uint16_t* _M0L6bufferS451;
      int32_t _M0L12digit__startS452;
      #line 573 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS449 = _M0FPB12dec__count64(_M0L3numS447);
      if (_M0L12is__negativeS446) {
        _M0L6_2atmpS1582 = 1;
      } else {
        _M0L6_2atmpS1582 = 0;
      }
      _M0L10total__lenS450 = _M0L10digit__lenS449 + _M0L6_2atmpS1582;
      _M0L6bufferS451
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS450, 0);
      if (_M0L12is__negativeS446) {
        _M0L12digit__startS452 = 1;
      } else {
        _M0L12digit__startS452 = 0;
      }
      moonbit_incref(_M0L6bufferS451);
      #line 577 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS451, _M0L3numS447, _M0L12digit__startS452, _M0L10total__lenS450);
      _M0L6bufferS448 = _M0L6bufferS451;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS453;
      int32_t _M0L6_2atmpS1583;
      int32_t _M0L10total__lenS454;
      uint16_t* _M0L6bufferS455;
      int32_t _M0L12digit__startS456;
      #line 581 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS453 = _M0FPB12hex__count64(_M0L3numS447);
      if (_M0L12is__negativeS446) {
        _M0L6_2atmpS1583 = 1;
      } else {
        _M0L6_2atmpS1583 = 0;
      }
      _M0L10total__lenS454 = _M0L10digit__lenS453 + _M0L6_2atmpS1583;
      _M0L6bufferS455
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS454, 0);
      if (_M0L12is__negativeS446) {
        _M0L12digit__startS456 = 1;
      } else {
        _M0L12digit__startS456 = 0;
      }
      moonbit_incref(_M0L6bufferS455);
      #line 585 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS455, _M0L3numS447, _M0L12digit__startS456, _M0L10total__lenS454);
      _M0L6bufferS448 = _M0L6bufferS455;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS457;
      int32_t _M0L6_2atmpS1584;
      int32_t _M0L10total__lenS458;
      uint16_t* _M0L6bufferS459;
      int32_t _M0L12digit__startS460;
      #line 589 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS457
      = _M0FPB14radix__count64(_M0L3numS447, _M0L5radixS444);
      if (_M0L12is__negativeS446) {
        _M0L6_2atmpS1584 = 1;
      } else {
        _M0L6_2atmpS1584 = 0;
      }
      _M0L10total__lenS458 = _M0L10digit__lenS457 + _M0L6_2atmpS1584;
      _M0L6bufferS459
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS458, 0);
      if (_M0L12is__negativeS446) {
        _M0L12digit__startS460 = 1;
      } else {
        _M0L12digit__startS460 = 0;
      }
      moonbit_incref(_M0L6bufferS459);
      #line 593 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS459, _M0L3numS447, _M0L12digit__startS460, _M0L10total__lenS458, _M0L5radixS444);
      _M0L6bufferS448 = _M0L6bufferS459;
      break;
    }
  }
  if (_M0L12is__negativeS446) {
    _M0L6bufferS448[0] = 45;
  }
  return _M0L6bufferS448;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS430,
  uint64_t _M0L3numS442,
  int32_t _M0L12digit__startS431,
  int32_t _M0L10total__lenS443
) {
  int32_t _M0L6_2atmpS1581;
  uint64_t _M0L3numS420;
  int32_t _M0L6offsetS421;
  #line 493 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1581 = _M0L10total__lenS443 - _M0L12digit__startS431;
  _M0L3numS420 = _M0L3numS442;
  _M0L6offsetS421 = _M0L6_2atmpS1581;
  while (1) {
    if (_M0L3numS420 >= 10000ull) {
      uint64_t _M0L1tS422 = _M0L3numS420 / 10000ull;
      uint64_t _M0L6_2atmpS1558 = _M0L3numS420 % 10000ull;
      int32_t _M0L1rS423 = (int32_t)_M0L6_2atmpS1558;
      int32_t _M0L2d1S424 = _M0L1rS423 / 100;
      int32_t _M0L2d2S425 = _M0L1rS423 % 100;
      int32_t _M0L6_2atmpS1557 = _M0L2d1S424 / 10;
      int32_t _M0L6_2atmpS1556 = 48 + _M0L6_2atmpS1557;
      int32_t _M0L6d1__hiS426 = (uint16_t)_M0L6_2atmpS1556;
      int32_t _M0L6_2atmpS1555 = _M0L2d1S424 % 10;
      int32_t _M0L6_2atmpS1554 = 48 + _M0L6_2atmpS1555;
      int32_t _M0L6d1__loS427 = (uint16_t)_M0L6_2atmpS1554;
      int32_t _M0L6_2atmpS1553 = _M0L2d2S425 / 10;
      int32_t _M0L6_2atmpS1552 = 48 + _M0L6_2atmpS1553;
      int32_t _M0L6d2__hiS428 = (uint16_t)_M0L6_2atmpS1552;
      int32_t _M0L6_2atmpS1551 = _M0L2d2S425 % 10;
      int32_t _M0L6_2atmpS1550 = 48 + _M0L6_2atmpS1551;
      int32_t _M0L6d2__loS429 = (uint16_t)_M0L6_2atmpS1550;
      int32_t _M0L6_2atmpS1542 = _M0L12digit__startS431 + _M0L6offsetS421;
      int32_t _M0L6_2atmpS1541 = _M0L6_2atmpS1542 - 4;
      int32_t _M0L6_2atmpS1544;
      int32_t _M0L6_2atmpS1543;
      int32_t _M0L6_2atmpS1546;
      int32_t _M0L6_2atmpS1545;
      int32_t _M0L6_2atmpS1548;
      int32_t _M0L6_2atmpS1547;
      int32_t _M0L6_2atmpS1549;
      _M0L6bufferS430[_M0L6_2atmpS1541] = _M0L6d1__hiS426;
      _M0L6_2atmpS1544 = _M0L12digit__startS431 + _M0L6offsetS421;
      _M0L6_2atmpS1543 = _M0L6_2atmpS1544 - 3;
      _M0L6bufferS430[_M0L6_2atmpS1543] = _M0L6d1__loS427;
      _M0L6_2atmpS1546 = _M0L12digit__startS431 + _M0L6offsetS421;
      _M0L6_2atmpS1545 = _M0L6_2atmpS1546 - 2;
      _M0L6bufferS430[_M0L6_2atmpS1545] = _M0L6d2__hiS428;
      _M0L6_2atmpS1548 = _M0L12digit__startS431 + _M0L6offsetS421;
      _M0L6_2atmpS1547 = _M0L6_2atmpS1548 - 1;
      _M0L6bufferS430[_M0L6_2atmpS1547] = _M0L6d2__loS429;
      _M0L6_2atmpS1549 = _M0L6offsetS421 - 4;
      _M0L3numS420 = _M0L1tS422;
      _M0L6offsetS421 = _M0L6_2atmpS1549;
      continue;
    } else {
      int32_t _M0L6_2atmpS1580 = (int32_t)_M0L3numS420;
      int32_t _M0L9remainingS433 = _M0L6_2atmpS1580;
      int32_t _M0L6offsetS434 = _M0L6offsetS421;
      while (1) {
        if (_M0L9remainingS433 >= 100) {
          int32_t _M0L1tS435 = _M0L9remainingS433 / 100;
          int32_t _M0L1dS436 = _M0L9remainingS433 % 100;
          int32_t _M0L6_2atmpS1567 = _M0L1dS436 / 10;
          int32_t _M0L6_2atmpS1566 = 48 + _M0L6_2atmpS1567;
          int32_t _M0L5d__hiS437 = (uint16_t)_M0L6_2atmpS1566;
          int32_t _M0L6_2atmpS1565 = _M0L1dS436 % 10;
          int32_t _M0L6_2atmpS1564 = 48 + _M0L6_2atmpS1565;
          int32_t _M0L5d__loS438 = (uint16_t)_M0L6_2atmpS1564;
          int32_t _M0L6_2atmpS1560 = _M0L12digit__startS431 + _M0L6offsetS434;
          int32_t _M0L6_2atmpS1559 = _M0L6_2atmpS1560 - 2;
          int32_t _M0L6_2atmpS1562;
          int32_t _M0L6_2atmpS1561;
          int32_t _M0L6_2atmpS1563;
          _M0L6bufferS430[_M0L6_2atmpS1559] = _M0L5d__hiS437;
          _M0L6_2atmpS1562 = _M0L12digit__startS431 + _M0L6offsetS434;
          _M0L6_2atmpS1561 = _M0L6_2atmpS1562 - 1;
          _M0L6bufferS430[_M0L6_2atmpS1561] = _M0L5d__loS438;
          _M0L6_2atmpS1563 = _M0L6offsetS434 - 2;
          _M0L9remainingS433 = _M0L1tS435;
          _M0L6offsetS434 = _M0L6_2atmpS1563;
          continue;
        } else if (_M0L9remainingS433 >= 10) {
          int32_t _M0L6_2atmpS1575 = _M0L9remainingS433 / 10;
          int32_t _M0L6_2atmpS1574 = 48 + _M0L6_2atmpS1575;
          int32_t _M0L5d__hiS440 = (uint16_t)_M0L6_2atmpS1574;
          int32_t _M0L6_2atmpS1573 = _M0L9remainingS433 % 10;
          int32_t _M0L6_2atmpS1572 = 48 + _M0L6_2atmpS1573;
          int32_t _M0L5d__loS441 = (uint16_t)_M0L6_2atmpS1572;
          int32_t _M0L6_2atmpS1569 = _M0L12digit__startS431 + _M0L6offsetS434;
          int32_t _M0L6_2atmpS1568 = _M0L6_2atmpS1569 - 2;
          int32_t _M0L6_2atmpS1571;
          int32_t _M0L6_2atmpS1570;
          _M0L6bufferS430[_M0L6_2atmpS1568] = _M0L5d__hiS440;
          _M0L6_2atmpS1571 = _M0L12digit__startS431 + _M0L6offsetS434;
          _M0L6_2atmpS1570 = _M0L6_2atmpS1571 - 1;
          _M0L6bufferS430[_M0L6_2atmpS1570] = _M0L5d__loS441;
          moonbit_decref(_M0L6bufferS430);
        } else {
          int32_t _M0L6_2atmpS1579 = _M0L12digit__startS431 + _M0L6offsetS434;
          int32_t _M0L6_2atmpS1576 = _M0L6_2atmpS1579 - 1;
          int32_t _M0L6_2atmpS1578 = 48 + _M0L9remainingS433;
          int32_t _M0L6_2atmpS1577 = (uint16_t)_M0L6_2atmpS1578;
          _M0L6bufferS430[_M0L6_2atmpS1576] = _M0L6_2atmpS1577;
          moonbit_decref(_M0L6bufferS430);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS410,
  uint64_t _M0L3numS414,
  int32_t _M0L12digit__startS411,
  int32_t _M0L10total__lenS413,
  int32_t _M0L5radixS404
) {
  uint64_t _M0L4baseS403;
  int32_t _M0L6_2atmpS1526;
  int32_t _M0L6_2atmpS1525;
  #line 462 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS403 = _M0MPC13int3Int10to__uint64(_M0L5radixS404);
  _M0L6_2atmpS1526 = _M0L5radixS404 - 1;
  _M0L6_2atmpS1525 = _M0L5radixS404 & _M0L6_2atmpS1526;
  if (_M0L6_2atmpS1525 == 0) {
    int32_t _M0L5shiftS405;
    uint64_t _M0L4maskS406;
    int32_t _M0L6_2atmpS1533;
    int32_t _M0L6offsetS407;
    uint64_t _M0L1nS408;
    #line 473 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS405 = moonbit_ctz32(_M0L5radixS404);
    _M0L4maskS406 = _M0L4baseS403 - 1ull;
    _M0L6_2atmpS1533 = _M0L10total__lenS413 - _M0L12digit__startS411;
    _M0L6offsetS407 = _M0L6_2atmpS1533;
    _M0L1nS408 = _M0L3numS414;
    while (1) {
      if (_M0L1nS408 > 0ull) {
        uint64_t _M0L6_2atmpS1532 = _M0L1nS408 & _M0L4maskS406;
        int32_t _M0L5digitS409 = (int32_t)_M0L6_2atmpS1532;
        int32_t _M0L6_2atmpS1529 = _M0L12digit__startS411 + _M0L6offsetS407;
        int32_t _M0L6_2atmpS1527 = _M0L6_2atmpS1529 - 1;
        int32_t _M0L6_2atmpS1528 =
          ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L5digitS409];
        int32_t _M0L6_2atmpS1530;
        uint64_t _M0L6_2atmpS1531;
        _M0L6bufferS410[_M0L6_2atmpS1527] = _M0L6_2atmpS1528;
        _M0L6_2atmpS1530 = _M0L6offsetS407 - 1;
        _M0L6_2atmpS1531 = _M0L1nS408 >> (_M0L5shiftS405 & 63);
        _M0L6offsetS407 = _M0L6_2atmpS1530;
        _M0L1nS408 = _M0L6_2atmpS1531;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS410);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1540 = _M0L10total__lenS413 - _M0L12digit__startS411;
    int32_t _M0L6offsetS415 = _M0L6_2atmpS1540;
    uint64_t _M0L1nS416 = _M0L3numS414;
    while (1) {
      if (_M0L1nS416 > 0ull) {
        uint64_t _M0L1qS417 = _M0L1nS416 / _M0L4baseS403;
        uint64_t _M0L6_2atmpS1539 = _M0L1qS417 * _M0L4baseS403;
        uint64_t _M0L6_2atmpS1538 = _M0L1nS416 - _M0L6_2atmpS1539;
        int32_t _M0L5digitS418 = (int32_t)_M0L6_2atmpS1538;
        int32_t _M0L6_2atmpS1536 = _M0L12digit__startS411 + _M0L6offsetS415;
        int32_t _M0L6_2atmpS1534 = _M0L6_2atmpS1536 - 1;
        int32_t _M0L6_2atmpS1535 =
          ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L5digitS418];
        int32_t _M0L6_2atmpS1537;
        _M0L6bufferS410[_M0L6_2atmpS1534] = _M0L6_2atmpS1535;
        _M0L6_2atmpS1537 = _M0L6offsetS415 - 1;
        _M0L6offsetS415 = _M0L6_2atmpS1537;
        _M0L1nS416 = _M0L1qS417;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS410);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS397,
  uint64_t _M0L3numS402,
  int32_t _M0L12digit__startS398,
  int32_t _M0L10total__lenS401
) {
  int32_t _M0L6_2atmpS1524;
  int32_t _M0L6offsetS392;
  uint64_t _M0L1nS393;
  #line 434 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1524 = _M0L10total__lenS401 - _M0L12digit__startS398;
  _M0L6offsetS392 = _M0L6_2atmpS1524;
  _M0L1nS393 = _M0L3numS402;
  while (1) {
    if (_M0L6offsetS392 >= 2) {
      uint64_t _M0L6_2atmpS1521 = _M0L1nS393 & 255ull;
      int32_t _M0L9byte__valS394 = (int32_t)_M0L6_2atmpS1521;
      int32_t _M0L2hiS395 = _M0L9byte__valS394 / 16;
      int32_t _M0L2loS396 = _M0L9byte__valS394 % 16;
      int32_t _M0L6_2atmpS1515 = _M0L12digit__startS398 + _M0L6offsetS392;
      int32_t _M0L6_2atmpS1513 = _M0L6_2atmpS1515 - 2;
      int32_t _M0L6_2atmpS1514 =
        ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L2hiS395];
      int32_t _M0L6_2atmpS1518;
      int32_t _M0L6_2atmpS1516;
      int32_t _M0L6_2atmpS1517;
      int32_t _M0L6_2atmpS1519;
      uint64_t _M0L6_2atmpS1520;
      _M0L6bufferS397[_M0L6_2atmpS1513] = _M0L6_2atmpS1514;
      _M0L6_2atmpS1518 = _M0L12digit__startS398 + _M0L6offsetS392;
      _M0L6_2atmpS1516 = _M0L6_2atmpS1518 - 1;
      _M0L6_2atmpS1517
      = ((moonbit_string_t)moonbit_string_literal_67.data)[
        _M0L2loS396
      ];
      _M0L6bufferS397[_M0L6_2atmpS1516] = _M0L6_2atmpS1517;
      _M0L6_2atmpS1519 = _M0L6offsetS392 - 2;
      _M0L6_2atmpS1520 = _M0L1nS393 >> 8;
      _M0L6offsetS392 = _M0L6_2atmpS1519;
      _M0L1nS393 = _M0L6_2atmpS1520;
      continue;
    } else if (_M0L6offsetS392 == 1) {
      uint64_t _M0L6_2atmpS1523 = _M0L1nS393 & 15ull;
      int32_t _M0L6nibbleS400 = (int32_t)_M0L6_2atmpS1523;
      int32_t _M0L6_2atmpS1522 =
        ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L6nibbleS400];
      _M0L6bufferS397[_M0L12digit__startS398] = _M0L6_2atmpS1522;
      moonbit_decref(_M0L6bufferS397);
    } else {
      moonbit_decref(_M0L6bufferS397);
    }
    break;
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS386,
  int32_t _M0L5radixS388
) {
  uint64_t _M0L4baseS387;
  uint64_t _M0L3numS389;
  int32_t _M0L5countS390;
  #line 419 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS386 == 0ull) {
    return 1;
  }
  #line 424 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS387 = _M0MPC13int3Int10to__uint64(_M0L5radixS388);
  _M0L3numS389 = _M0L5valueS386;
  _M0L5countS390 = 0;
  while (1) {
    if (_M0L3numS389 > 0ull) {
      uint64_t _M0L6_2atmpS1511 = _M0L3numS389 / _M0L4baseS387;
      int32_t _M0L6_2atmpS1512 = _M0L5countS390 + 1;
      _M0L3numS389 = _M0L6_2atmpS1511;
      _M0L5countS390 = _M0L6_2atmpS1512;
      continue;
    } else {
      return _M0L5countS390;
    }
    break;
  }
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS384) {
  #line 407 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS384 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS385;
    int32_t _M0L6_2atmpS1510;
    int32_t _M0L6_2atmpS1509;
    #line 412 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS385 = moonbit_clz64(_M0L5valueS384);
    _M0L6_2atmpS1510 = 63 - _M0L14leading__zerosS385;
    _M0L6_2atmpS1509 = _M0L6_2atmpS1510 / 4;
    return _M0L6_2atmpS1509 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS383) {
  #line 343 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS383 >= 10000000000ull) {
    if (_M0L5valueS383 >= 100000000000000ull) {
      if (_M0L5valueS383 >= 10000000000000000ull) {
        if (_M0L5valueS383 >= 1000000000000000000ull) {
          if (_M0L5valueS383 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS383 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS383 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS383 >= 1000000000000ull) {
      if (_M0L5valueS383 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS383 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS383 >= 100000ull) {
    if (_M0L5valueS383 >= 10000000ull) {
      if (_M0L5valueS383 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS383 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS383 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS383 >= 1000ull) {
    if (_M0L5valueS383 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS383 >= 100ull) {
    return 3;
  } else if (_M0L5valueS383 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS367,
  int32_t _M0L5radixS366
) {
  int32_t _if__result_2519;
  int32_t _M0L12is__negativeS368;
  uint32_t _M0L3numS369;
  uint16_t* _M0L6bufferS370;
  #line 209 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS366 < 2) {
    _if__result_2519 = 1;
  } else {
    _if__result_2519 = _M0L5radixS366 > 36;
  }
  if (_if__result_2519) {
    #line 213 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_65.data);
  }
  if (_M0L4selfS367 == 0) {
    return (moonbit_string_t)moonbit_string_literal_66.data;
  }
  _M0L12is__negativeS368 = _M0L4selfS367 < 0;
  if (_M0L12is__negativeS368) {
    int32_t _M0L6_2atmpS1508 = -_M0L4selfS367;
    _M0L3numS369 = *(uint32_t*)&_M0L6_2atmpS1508;
  } else {
    _M0L3numS369 = *(uint32_t*)&_M0L4selfS367;
  }
  switch (_M0L5radixS366) {
    case 10: {
      int32_t _M0L10digit__lenS371;
      int32_t _M0L6_2atmpS1505;
      int32_t _M0L10total__lenS372;
      uint16_t* _M0L6bufferS373;
      int32_t _M0L12digit__startS374;
      #line 235 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS371 = _M0FPB12dec__count32(_M0L3numS369);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS1505 = 1;
      } else {
        _M0L6_2atmpS1505 = 0;
      }
      _M0L10total__lenS372 = _M0L10digit__lenS371 + _M0L6_2atmpS1505;
      _M0L6bufferS373
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS372, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS374 = 1;
      } else {
        _M0L12digit__startS374 = 0;
      }
      moonbit_incref(_M0L6bufferS373);
      #line 239 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS373, _M0L3numS369, _M0L12digit__startS374, _M0L10total__lenS372);
      _M0L6bufferS370 = _M0L6bufferS373;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS375;
      int32_t _M0L6_2atmpS1506;
      int32_t _M0L10total__lenS376;
      uint16_t* _M0L6bufferS377;
      int32_t _M0L12digit__startS378;
      #line 243 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS375 = _M0FPB12hex__count32(_M0L3numS369);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS1506 = 1;
      } else {
        _M0L6_2atmpS1506 = 0;
      }
      _M0L10total__lenS376 = _M0L10digit__lenS375 + _M0L6_2atmpS1506;
      _M0L6bufferS377
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS376, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS378 = 1;
      } else {
        _M0L12digit__startS378 = 0;
      }
      moonbit_incref(_M0L6bufferS377);
      #line 247 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS377, _M0L3numS369, _M0L12digit__startS378, _M0L10total__lenS376);
      _M0L6bufferS370 = _M0L6bufferS377;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS379;
      int32_t _M0L6_2atmpS1507;
      int32_t _M0L10total__lenS380;
      uint16_t* _M0L6bufferS381;
      int32_t _M0L12digit__startS382;
      #line 251 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS379
      = _M0FPB14radix__count32(_M0L3numS369, _M0L5radixS366);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS1507 = 1;
      } else {
        _M0L6_2atmpS1507 = 0;
      }
      _M0L10total__lenS380 = _M0L10digit__lenS379 + _M0L6_2atmpS1507;
      _M0L6bufferS381
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS380, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS382 = 1;
      } else {
        _M0L12digit__startS382 = 0;
      }
      moonbit_incref(_M0L6bufferS381);
      #line 255 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS381, _M0L3numS369, _M0L12digit__startS382, _M0L10total__lenS380, _M0L5radixS366);
      _M0L6bufferS370 = _M0L6bufferS381;
      break;
    }
  }
  if (_M0L12is__negativeS368) {
    _M0L6bufferS370[0] = 45;
  }
  return _M0L6bufferS370;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS360,
  int32_t _M0L5radixS362
) {
  uint32_t _M0L4baseS361;
  uint32_t _M0L3numS363;
  int32_t _M0L5countS364;
  #line 189 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS360 == 0u) {
    return 1;
  }
  _M0L4baseS361 = *(uint32_t*)&_M0L5radixS362;
  _M0L3numS363 = _M0L5valueS360;
  _M0L5countS364 = 0;
  while (1) {
    if (_M0L3numS363 > 0u) {
      uint32_t _M0L6_2atmpS1503 = _M0L3numS363 / _M0L4baseS361;
      int32_t _M0L6_2atmpS1504 = _M0L5countS364 + 1;
      _M0L3numS363 = _M0L6_2atmpS1503;
      _M0L5countS364 = _M0L6_2atmpS1504;
      continue;
    } else {
      return _M0L5countS364;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS358) {
  #line 177 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS358 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS359;
    int32_t _M0L6_2atmpS1502;
    int32_t _M0L6_2atmpS1501;
    #line 182 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS359 = moonbit_clz32(_M0L5valueS358);
    _M0L6_2atmpS1502 = 31 - _M0L14leading__zerosS359;
    _M0L6_2atmpS1501 = _M0L6_2atmpS1502 / 4;
    return _M0L6_2atmpS1501 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS357) {
  #line 143 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS357 >= 100000u) {
    if (_M0L5valueS357 >= 10000000u) {
      if (_M0L5valueS357 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS357 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS357 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS357 >= 1000u) {
    if (_M0L5valueS357 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS357 >= 100u) {
    return 3;
  } else if (_M0L5valueS357 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS343,
  uint32_t _M0L3numS355,
  int32_t _M0L12digit__startS344,
  int32_t _M0L10total__lenS356
) {
  int32_t _M0L6_2atmpS1500;
  uint32_t _M0L3numS333;
  int32_t _M0L6offsetS334;
  #line 88 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1500 = _M0L10total__lenS356 - _M0L12digit__startS344;
  _M0L3numS333 = _M0L3numS355;
  _M0L6offsetS334 = _M0L6_2atmpS1500;
  while (1) {
    if (_M0L3numS333 >= 10000u) {
      uint32_t _M0L1tS335 = _M0L3numS333 / 10000u;
      uint32_t _M0L6_2atmpS1477 = _M0L3numS333 % 10000u;
      int32_t _M0L1rS336 = *(int32_t*)&_M0L6_2atmpS1477;
      int32_t _M0L2d1S337 = _M0L1rS336 / 100;
      int32_t _M0L2d2S338 = _M0L1rS336 % 100;
      int32_t _M0L6_2atmpS1476 = _M0L2d1S337 / 10;
      int32_t _M0L6_2atmpS1475 = 48 + _M0L6_2atmpS1476;
      int32_t _M0L6d1__hiS339 = (uint16_t)_M0L6_2atmpS1475;
      int32_t _M0L6_2atmpS1474 = _M0L2d1S337 % 10;
      int32_t _M0L6_2atmpS1473 = 48 + _M0L6_2atmpS1474;
      int32_t _M0L6d1__loS340 = (uint16_t)_M0L6_2atmpS1473;
      int32_t _M0L6_2atmpS1472 = _M0L2d2S338 / 10;
      int32_t _M0L6_2atmpS1471 = 48 + _M0L6_2atmpS1472;
      int32_t _M0L6d2__hiS341 = (uint16_t)_M0L6_2atmpS1471;
      int32_t _M0L6_2atmpS1470 = _M0L2d2S338 % 10;
      int32_t _M0L6_2atmpS1469 = 48 + _M0L6_2atmpS1470;
      int32_t _M0L6d2__loS342 = (uint16_t)_M0L6_2atmpS1469;
      int32_t _M0L6_2atmpS1461 = _M0L12digit__startS344 + _M0L6offsetS334;
      int32_t _M0L6_2atmpS1460 = _M0L6_2atmpS1461 - 4;
      int32_t _M0L6_2atmpS1463;
      int32_t _M0L6_2atmpS1462;
      int32_t _M0L6_2atmpS1465;
      int32_t _M0L6_2atmpS1464;
      int32_t _M0L6_2atmpS1467;
      int32_t _M0L6_2atmpS1466;
      int32_t _M0L6_2atmpS1468;
      _M0L6bufferS343[_M0L6_2atmpS1460] = _M0L6d1__hiS339;
      _M0L6_2atmpS1463 = _M0L12digit__startS344 + _M0L6offsetS334;
      _M0L6_2atmpS1462 = _M0L6_2atmpS1463 - 3;
      _M0L6bufferS343[_M0L6_2atmpS1462] = _M0L6d1__loS340;
      _M0L6_2atmpS1465 = _M0L12digit__startS344 + _M0L6offsetS334;
      _M0L6_2atmpS1464 = _M0L6_2atmpS1465 - 2;
      _M0L6bufferS343[_M0L6_2atmpS1464] = _M0L6d2__hiS341;
      _M0L6_2atmpS1467 = _M0L12digit__startS344 + _M0L6offsetS334;
      _M0L6_2atmpS1466 = _M0L6_2atmpS1467 - 1;
      _M0L6bufferS343[_M0L6_2atmpS1466] = _M0L6d2__loS342;
      _M0L6_2atmpS1468 = _M0L6offsetS334 - 4;
      _M0L3numS333 = _M0L1tS335;
      _M0L6offsetS334 = _M0L6_2atmpS1468;
      continue;
    } else {
      int32_t _M0L6_2atmpS1499 = *(int32_t*)&_M0L3numS333;
      int32_t _M0L9remainingS346 = _M0L6_2atmpS1499;
      int32_t _M0L6offsetS347 = _M0L6offsetS334;
      while (1) {
        if (_M0L9remainingS346 >= 100) {
          int32_t _M0L1tS348 = _M0L9remainingS346 / 100;
          int32_t _M0L1dS349 = _M0L9remainingS346 % 100;
          int32_t _M0L6_2atmpS1486 = _M0L1dS349 / 10;
          int32_t _M0L6_2atmpS1485 = 48 + _M0L6_2atmpS1486;
          int32_t _M0L5d__hiS350 = (uint16_t)_M0L6_2atmpS1485;
          int32_t _M0L6_2atmpS1484 = _M0L1dS349 % 10;
          int32_t _M0L6_2atmpS1483 = 48 + _M0L6_2atmpS1484;
          int32_t _M0L5d__loS351 = (uint16_t)_M0L6_2atmpS1483;
          int32_t _M0L6_2atmpS1479 = _M0L12digit__startS344 + _M0L6offsetS347;
          int32_t _M0L6_2atmpS1478 = _M0L6_2atmpS1479 - 2;
          int32_t _M0L6_2atmpS1481;
          int32_t _M0L6_2atmpS1480;
          int32_t _M0L6_2atmpS1482;
          _M0L6bufferS343[_M0L6_2atmpS1478] = _M0L5d__hiS350;
          _M0L6_2atmpS1481 = _M0L12digit__startS344 + _M0L6offsetS347;
          _M0L6_2atmpS1480 = _M0L6_2atmpS1481 - 1;
          _M0L6bufferS343[_M0L6_2atmpS1480] = _M0L5d__loS351;
          _M0L6_2atmpS1482 = _M0L6offsetS347 - 2;
          _M0L9remainingS346 = _M0L1tS348;
          _M0L6offsetS347 = _M0L6_2atmpS1482;
          continue;
        } else if (_M0L9remainingS346 >= 10) {
          int32_t _M0L6_2atmpS1494 = _M0L9remainingS346 / 10;
          int32_t _M0L6_2atmpS1493 = 48 + _M0L6_2atmpS1494;
          int32_t _M0L5d__hiS353 = (uint16_t)_M0L6_2atmpS1493;
          int32_t _M0L6_2atmpS1492 = _M0L9remainingS346 % 10;
          int32_t _M0L6_2atmpS1491 = 48 + _M0L6_2atmpS1492;
          int32_t _M0L5d__loS354 = (uint16_t)_M0L6_2atmpS1491;
          int32_t _M0L6_2atmpS1488 = _M0L12digit__startS344 + _M0L6offsetS347;
          int32_t _M0L6_2atmpS1487 = _M0L6_2atmpS1488 - 2;
          int32_t _M0L6_2atmpS1490;
          int32_t _M0L6_2atmpS1489;
          _M0L6bufferS343[_M0L6_2atmpS1487] = _M0L5d__hiS353;
          _M0L6_2atmpS1490 = _M0L12digit__startS344 + _M0L6offsetS347;
          _M0L6_2atmpS1489 = _M0L6_2atmpS1490 - 1;
          _M0L6bufferS343[_M0L6_2atmpS1489] = _M0L5d__loS354;
          moonbit_decref(_M0L6bufferS343);
        } else {
          int32_t _M0L6_2atmpS1498 = _M0L12digit__startS344 + _M0L6offsetS347;
          int32_t _M0L6_2atmpS1495 = _M0L6_2atmpS1498 - 1;
          int32_t _M0L6_2atmpS1497 = 48 + _M0L9remainingS346;
          int32_t _M0L6_2atmpS1496 = (uint16_t)_M0L6_2atmpS1497;
          _M0L6bufferS343[_M0L6_2atmpS1495] = _M0L6_2atmpS1496;
          moonbit_decref(_M0L6bufferS343);
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS323,
  uint32_t _M0L3numS327,
  int32_t _M0L12digit__startS324,
  int32_t _M0L10total__lenS326,
  int32_t _M0L5radixS317
) {
  uint32_t _M0L4baseS316;
  int32_t _M0L6_2atmpS1445;
  int32_t _M0L6_2atmpS1444;
  #line 57 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS316 = *(uint32_t*)&_M0L5radixS317;
  _M0L6_2atmpS1445 = _M0L5radixS317 - 1;
  _M0L6_2atmpS1444 = _M0L5radixS317 & _M0L6_2atmpS1445;
  if (_M0L6_2atmpS1444 == 0) {
    int32_t _M0L5shiftS318;
    uint32_t _M0L4maskS319;
    int32_t _M0L6_2atmpS1452;
    int32_t _M0L6offsetS320;
    uint32_t _M0L1nS321;
    #line 68 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS318 = moonbit_ctz32(_M0L5radixS317);
    _M0L4maskS319 = _M0L4baseS316 - 1u;
    _M0L6_2atmpS1452 = _M0L10total__lenS326 - _M0L12digit__startS324;
    _M0L6offsetS320 = _M0L6_2atmpS1452;
    _M0L1nS321 = _M0L3numS327;
    while (1) {
      if (_M0L1nS321 > 0u) {
        uint32_t _M0L6_2atmpS1451 = _M0L1nS321 & _M0L4maskS319;
        int32_t _M0L5digitS322 = *(int32_t*)&_M0L6_2atmpS1451;
        int32_t _M0L6_2atmpS1448 = _M0L12digit__startS324 + _M0L6offsetS320;
        int32_t _M0L6_2atmpS1446 = _M0L6_2atmpS1448 - 1;
        int32_t _M0L6_2atmpS1447 =
          ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L5digitS322];
        int32_t _M0L6_2atmpS1449;
        uint32_t _M0L6_2atmpS1450;
        _M0L6bufferS323[_M0L6_2atmpS1446] = _M0L6_2atmpS1447;
        _M0L6_2atmpS1449 = _M0L6offsetS320 - 1;
        _M0L6_2atmpS1450 = _M0L1nS321 >> (_M0L5shiftS318 & 31);
        _M0L6offsetS320 = _M0L6_2atmpS1449;
        _M0L1nS321 = _M0L6_2atmpS1450;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS323);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1459 = _M0L10total__lenS326 - _M0L12digit__startS324;
    int32_t _M0L6offsetS328 = _M0L6_2atmpS1459;
    uint32_t _M0L1nS329 = _M0L3numS327;
    while (1) {
      if (_M0L1nS329 > 0u) {
        uint32_t _M0L1qS330 = _M0L1nS329 / _M0L4baseS316;
        uint32_t _M0L6_2atmpS1458 = _M0L1qS330 * _M0L4baseS316;
        uint32_t _M0L6_2atmpS1457 = _M0L1nS329 - _M0L6_2atmpS1458;
        int32_t _M0L5digitS331 = *(int32_t*)&_M0L6_2atmpS1457;
        int32_t _M0L6_2atmpS1455 = _M0L12digit__startS324 + _M0L6offsetS328;
        int32_t _M0L6_2atmpS1453 = _M0L6_2atmpS1455 - 1;
        int32_t _M0L6_2atmpS1454 =
          ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L5digitS331];
        int32_t _M0L6_2atmpS1456;
        _M0L6bufferS323[_M0L6_2atmpS1453] = _M0L6_2atmpS1454;
        _M0L6_2atmpS1456 = _M0L6offsetS328 - 1;
        _M0L6offsetS328 = _M0L6_2atmpS1456;
        _M0L1nS329 = _M0L1qS330;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS323);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS310,
  uint32_t _M0L3numS315,
  int32_t _M0L12digit__startS311,
  int32_t _M0L10total__lenS314
) {
  int32_t _M0L6_2atmpS1443;
  int32_t _M0L6offsetS305;
  uint32_t _M0L1nS306;
  #line 29 "/Users/user/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS1443 = _M0L10total__lenS314 - _M0L12digit__startS311;
  _M0L6offsetS305 = _M0L6_2atmpS1443;
  _M0L1nS306 = _M0L3numS315;
  while (1) {
    if (_M0L6offsetS305 >= 2) {
      uint32_t _M0L6_2atmpS1440 = _M0L1nS306 & 255u;
      int32_t _M0L9byte__valS307 = *(int32_t*)&_M0L6_2atmpS1440;
      int32_t _M0L2hiS308 = _M0L9byte__valS307 / 16;
      int32_t _M0L2loS309 = _M0L9byte__valS307 % 16;
      int32_t _M0L6_2atmpS1434 = _M0L12digit__startS311 + _M0L6offsetS305;
      int32_t _M0L6_2atmpS1432 = _M0L6_2atmpS1434 - 2;
      int32_t _M0L6_2atmpS1433 =
        ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L2hiS308];
      int32_t _M0L6_2atmpS1437;
      int32_t _M0L6_2atmpS1435;
      int32_t _M0L6_2atmpS1436;
      int32_t _M0L6_2atmpS1438;
      uint32_t _M0L6_2atmpS1439;
      _M0L6bufferS310[_M0L6_2atmpS1432] = _M0L6_2atmpS1433;
      _M0L6_2atmpS1437 = _M0L12digit__startS311 + _M0L6offsetS305;
      _M0L6_2atmpS1435 = _M0L6_2atmpS1437 - 1;
      _M0L6_2atmpS1436
      = ((moonbit_string_t)moonbit_string_literal_67.data)[
        _M0L2loS309
      ];
      _M0L6bufferS310[_M0L6_2atmpS1435] = _M0L6_2atmpS1436;
      _M0L6_2atmpS1438 = _M0L6offsetS305 - 2;
      _M0L6_2atmpS1439 = _M0L1nS306 >> 8;
      _M0L6offsetS305 = _M0L6_2atmpS1438;
      _M0L1nS306 = _M0L6_2atmpS1439;
      continue;
    } else if (_M0L6offsetS305 == 1) {
      uint32_t _M0L6_2atmpS1442 = _M0L1nS306 & 15u;
      int32_t _M0L6nibbleS313 = *(int32_t*)&_M0L6_2atmpS1442;
      int32_t _M0L6_2atmpS1441 =
        ((moonbit_string_t)moonbit_string_literal_67.data)[_M0L6nibbleS313];
      _M0L6bufferS310[_M0L12digit__startS311] = _M0L6_2atmpS1441;
      moonbit_decref(_M0L6bufferS310);
    } else {
      moonbit_decref(_M0L6bufferS310);
    }
    break;
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS304) {
  struct _M0TWEOs* _M0L7_2afuncS303;
  #line 28 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS303 = _M0L4selfS304;
  #line 31 "/Users/user/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS303->code(_M0L7_2afuncS303);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS302
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS301;
  struct _M0TPB6Logger _M0L6_2atmpS1431;
  #line 147 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 148 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS301 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS301);
  _M0L6_2atmpS1431
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS301
  };
  #line 149 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS302, _M0L6_2atmpS1431);
  #line 150 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS301);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS300
) {
  int32_t _result_2526;
  #line 98 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _result_2526 = _M0L4selfS300.$1;
  moonbit_decref(_M0L4selfS300.$0);
  return _result_2526;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS299
) {
  #line 91 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS299.$0;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS295,
  moonbit_string_t _M0L5valueS296,
  int32_t _M0L5startS297,
  int32_t _M0L3lenS298
) {
  int32_t _M0L6_2atmpS1430;
  int64_t _M0L6_2atmpS1429;
  struct _M0TPC16string10StringView _M0L6_2atmpS1428;
  #line 102 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1430 = _M0L5startS297 + _M0L3lenS298;
  _M0L6_2atmpS1429 = (int64_t)_M0L6_2atmpS1430;
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1428
  = _M0MPC16string6String11sub_2einner(_M0L5valueS296, _M0L5startS297, _M0L6_2atmpS1429);
  #line 103 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS295, _M0L6_2atmpS1428);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS288,
  int32_t _M0L5startS294,
  int64_t _M0L3endS290
) {
  int32_t _M0L3lenS287;
  int32_t _M0L3endS289;
  int32_t _M0L5startS293;
  int32_t _if__result_2527;
  #line 653 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS287 = Moonbit_array_length(_M0L4selfS288);
  if (_M0L3endS290 == 4294967296ll) {
    _M0L3endS289 = _M0L3lenS287;
  } else {
    int64_t _M0L7_2aSomeS291 = _M0L3endS290;
    int32_t _M0L6_2aendS292 = (int32_t)_M0L7_2aSomeS291;
    if (_M0L6_2aendS292 < 0) {
      _M0L3endS289 = _M0L3lenS287 + _M0L6_2aendS292;
    } else {
      _M0L3endS289 = _M0L6_2aendS292;
    }
  }
  if (_M0L5startS294 < 0) {
    _M0L5startS293 = _M0L3lenS287 + _M0L5startS294;
  } else {
    _M0L5startS293 = _M0L5startS294;
  }
  if (_M0L5startS293 >= 0) {
    if (_M0L5startS293 <= _M0L3endS289) {
      _if__result_2527 = _M0L3endS289 <= _M0L3lenS287;
    } else {
      _if__result_2527 = 0;
    }
  } else {
    _if__result_2527 = 0;
  }
  if (_if__result_2527) {
    if (_M0L5startS293 < _M0L3lenS287) {
      int32_t _M0L6_2atmpS1425 = _M0L4selfS288[_M0L5startS293];
      int32_t _M0L6_2atmpS1424;
      #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1424
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1425);
      if (!_M0L6_2atmpS1424) {
        
      } else {
        #line 663 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS289 < _M0L3lenS287) {
      int32_t _M0L6_2atmpS1427 = _M0L4selfS288[_M0L3endS289];
      int32_t _M0L6_2atmpS1426;
      #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1426
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1427);
      if (!_M0L6_2atmpS1426) {
        
      } else {
        #line 666 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS293,
                                                 _M0L3endS289,
                                                 _M0L4selfS288};
  } else {
    moonbit_decref(_M0L4selfS288);
    #line 661 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS284) {
  struct _M0TPB6Hasher* _M0L1hS283;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS283 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS283);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS283, _M0L4selfS284);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS283);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS286
) {
  struct _M0TPB6Hasher* _M0L1hS285;
  #line 79 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 80 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS285 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS285);
  #line 81 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS285, _M0L4selfS286);
  #line 82 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS285);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS281) {
  int32_t _M0L4seedS280;
  if (_M0L10seed_2eoptS281 == 4294967296ll) {
    _M0L4seedS280 = 0;
  } else {
    int64_t _M0L7_2aSomeS282 = _M0L10seed_2eoptS281;
    _M0L4seedS280 = (int32_t)_M0L7_2aSomeS282;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS280);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS279) {
  uint32_t _M0L6_2atmpS1423;
  uint32_t _M0L6_2atmpS1422;
  struct _M0TPB6Hasher* _block_2528;
  #line 75 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1423 = *(uint32_t*)&_M0L4seedS279;
  _M0L6_2atmpS1422 = _M0L6_2atmpS1423 + 374761393u;
  _block_2528
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2528)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2528->$0 = _M0L6_2atmpS1422;
  return _block_2528;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS278) {
  uint32_t _M0L6_2atmpS1421;
  #line 435 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 436 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1421 = _M0MPB6Hasher9avalanche(_M0L4selfS278);
  return *(int32_t*)&_M0L6_2atmpS1421;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS277) {
  uint32_t _M0Lm3accS276;
  uint32_t _M0L6_2atmpS1410;
  uint32_t _M0L6_2atmpS1412;
  uint32_t _M0L6_2atmpS1411;
  uint32_t _M0L6_2atmpS1413;
  uint32_t _M0L6_2atmpS1414;
  uint32_t _M0L6_2atmpS1416;
  uint32_t _M0L6_2atmpS1415;
  uint32_t _M0L6_2atmpS1417;
  uint32_t _M0L6_2atmpS1418;
  uint32_t _M0L6_2atmpS1420;
  uint32_t _M0L6_2atmpS1419;
  #line 440 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS276 = _M0L4selfS277->$0;
  moonbit_decref(_M0L4selfS277);
  _M0L6_2atmpS1410 = _M0Lm3accS276;
  _M0L6_2atmpS1412 = _M0Lm3accS276;
  _M0L6_2atmpS1411 = _M0L6_2atmpS1412 >> 15;
  _M0Lm3accS276 = _M0L6_2atmpS1410 ^ _M0L6_2atmpS1411;
  _M0L6_2atmpS1413 = _M0Lm3accS276;
  _M0Lm3accS276 = _M0L6_2atmpS1413 * 2246822519u;
  _M0L6_2atmpS1414 = _M0Lm3accS276;
  _M0L6_2atmpS1416 = _M0Lm3accS276;
  _M0L6_2atmpS1415 = _M0L6_2atmpS1416 >> 13;
  _M0Lm3accS276 = _M0L6_2atmpS1414 ^ _M0L6_2atmpS1415;
  _M0L6_2atmpS1417 = _M0Lm3accS276;
  _M0Lm3accS276 = _M0L6_2atmpS1417 * 3266489917u;
  _M0L6_2atmpS1418 = _M0Lm3accS276;
  _M0L6_2atmpS1420 = _M0Lm3accS276;
  _M0L6_2atmpS1419 = _M0L6_2atmpS1420 >> 16;
  _M0Lm3accS276 = _M0L6_2atmpS1418 ^ _M0L6_2atmpS1419;
  return _M0Lm3accS276;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS274,
  moonbit_string_t _M0L1yS275
) {
  int32_t _M0L6_2atmpS1409;
  #line 23 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  #line 24 "/Users/user/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1409 = moonbit_val_array_equal(_M0L1xS274, _M0L1yS275);
  moonbit_decref(_M0L1yS275);
  moonbit_decref(_M0L1xS274);
  return !_M0L6_2atmpS1409;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS271,
  int32_t _M0L5valueS270
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS270, _M0L4selfS271);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS273,
  moonbit_string_t _M0L5valueS272
) {
  #line 120 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS272, _M0L4selfS273);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS269) {
  int64_t _M0L6_2atmpS1408;
  #line 907 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1408 = (int64_t)_M0L4selfS269;
  return *(uint64_t*)&_M0L6_2atmpS1408;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS267,
  int32_t _M0L5valueS268
) {
  uint32_t _M0L6_2atmpS1407;
  #line 187 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1407 = *(uint32_t*)&_M0L5valueS268;
  #line 188 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS267, _M0L6_2atmpS1407);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS257,
  moonbit_string_t _M0L7contentS258,
  moonbit_string_t _M0L3locS260,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS262
) {
  moonbit_string_t _M0L6actualS256;
  #line 184 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 191 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6actualS256 = _M0L3objS257.$0->$method_1(_M0L3objS257.$1);
  moonbit_incref(_M0L7contentS258);
  moonbit_incref(_M0L6actualS256);
  #line 192 "/Users/user/.moon/lib/core/builtin/console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS256, _M0L7contentS258)
  ) {
    moonbit_string_t _M0L3locS259;
    moonbit_string_t _M0L9args__locS261;
    moonbit_string_t _M0L15expect__escapedS263;
    moonbit_string_t _M0L15actual__escapedS264;
    moonbit_string_t _M0L6_2atmpS1405;
    moonbit_string_t _M0L6_2atmpS1404;
    moonbit_string_t _M0L6_2atmpS1403;
    moonbit_string_t _M0L14expect__base64S265;
    moonbit_string_t _M0L6_2atmpS1402;
    moonbit_string_t _M0L6_2atmpS1401;
    moonbit_string_t _M0L6_2atmpS1400;
    moonbit_string_t _M0L14actual__base64S266;
    moonbit_string_t _M0L6_2atmpS1399;
    moonbit_string_t _M0L6_2atmpS1398;
    moonbit_string_t _M0L6_2atmpS1396;
    moonbit_string_t _M0L6_2atmpS1397;
    moonbit_string_t _M0L6_2atmpS1395;
    moonbit_string_t _M0L6_2atmpS1393;
    moonbit_string_t _M0L6_2atmpS1394;
    moonbit_string_t _M0L6_2atmpS1392;
    moonbit_string_t _M0L6_2atmpS1390;
    moonbit_string_t _M0L6_2atmpS1391;
    moonbit_string_t _M0L6_2atmpS1389;
    moonbit_string_t _M0L6_2atmpS1387;
    moonbit_string_t _M0L6_2atmpS1388;
    moonbit_string_t _M0L6_2atmpS1386;
    moonbit_string_t _M0L6_2atmpS1384;
    moonbit_string_t _M0L6_2atmpS1385;
    moonbit_string_t _M0L6_2atmpS1383;
    moonbit_string_t _M0L6_2atmpS1382;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1381;
    struct moonbit_result_0 _result_2529;
    #line 193 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L3locS259 = _M0MPB9SourceLoc16to__json__string(_M0L3locS260);
    #line 194 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L9args__locS261 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS262);
    moonbit_incref(_M0L7contentS258);
    #line 195 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L15expect__escapedS263
    = _M0MPC16string6String14escape_2einner(_M0L7contentS258, 1);
    moonbit_incref(_M0L6actualS256);
    #line 196 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L15actual__escapedS264
    = _M0MPC16string6String14escape_2einner(_M0L6actualS256, 1);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1405
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS258);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1404
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1405);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1403
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS1404);
    moonbit_decref(_M0L6_2atmpS1404);
    #line 197 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14expect__base64S265
    = moonbit_add_string(_M0L6_2atmpS1403, (moonbit_string_t)moonbit_string_literal_68.data);
    moonbit_decref(_M0L6_2atmpS1403);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1402
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS256);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1401
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1402);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1400
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS1401);
    moonbit_decref(_M0L6_2atmpS1401);
    #line 198 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L14actual__base64S266
    = moonbit_add_string(_M0L6_2atmpS1400, (moonbit_string_t)moonbit_string_literal_68.data);
    moonbit_decref(_M0L6_2atmpS1400);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1399 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS259);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1398
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_69.data, _M0L6_2atmpS1399);
    moonbit_decref(_M0L6_2atmpS1399);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1396
    = moonbit_add_string(_M0L6_2atmpS1398, (moonbit_string_t)moonbit_string_literal_70.data);
    moonbit_decref(_M0L6_2atmpS1398);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1397
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS261);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1395 = moonbit_add_string(_M0L6_2atmpS1396, _M0L6_2atmpS1397);
    moonbit_decref(_M0L6_2atmpS1397);
    moonbit_decref(_M0L6_2atmpS1396);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1393
    = moonbit_add_string(_M0L6_2atmpS1395, (moonbit_string_t)moonbit_string_literal_71.data);
    moonbit_decref(_M0L6_2atmpS1395);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1394
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS263);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1392 = moonbit_add_string(_M0L6_2atmpS1393, _M0L6_2atmpS1394);
    moonbit_decref(_M0L6_2atmpS1394);
    moonbit_decref(_M0L6_2atmpS1393);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1390
    = moonbit_add_string(_M0L6_2atmpS1392, (moonbit_string_t)moonbit_string_literal_72.data);
    moonbit_decref(_M0L6_2atmpS1392);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1391
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS264);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1389 = moonbit_add_string(_M0L6_2atmpS1390, _M0L6_2atmpS1391);
    moonbit_decref(_M0L6_2atmpS1391);
    moonbit_decref(_M0L6_2atmpS1390);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1387
    = moonbit_add_string(_M0L6_2atmpS1389, (moonbit_string_t)moonbit_string_literal_73.data);
    moonbit_decref(_M0L6_2atmpS1389);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1388
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S265);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1386 = moonbit_add_string(_M0L6_2atmpS1387, _M0L6_2atmpS1388);
    moonbit_decref(_M0L6_2atmpS1388);
    moonbit_decref(_M0L6_2atmpS1387);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1384
    = moonbit_add_string(_M0L6_2atmpS1386, (moonbit_string_t)moonbit_string_literal_74.data);
    moonbit_decref(_M0L6_2atmpS1386);
    #line 200 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1385
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S266);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1383 = moonbit_add_string(_M0L6_2atmpS1384, _M0L6_2atmpS1385);
    moonbit_decref(_M0L6_2atmpS1385);
    moonbit_decref(_M0L6_2atmpS1384);
    #line 199 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1382
    = moonbit_add_string(_M0L6_2atmpS1383, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1383);
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1381
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1381)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1381)->$0
    = _M0L6_2atmpS1382;
    _result_2529.tag = 0;
    _result_2529.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1381;
    return _result_2529;
  } else {
    int32_t _M0L6_2atmpS1406;
    struct moonbit_result_0 _result_2530;
    moonbit_decref(_M0L9args__locS262);
    moonbit_decref(_M0L3locS260);
    moonbit_decref(_M0L7contentS258);
    moonbit_decref(_M0L6actualS256);
    _M0L6_2atmpS1406 = 0;
    _result_2530.tag = 1;
    _result_2530.data.ok = _M0L6_2atmpS1406;
    return _result_2530;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS249
) {
  struct _M0TPB13StringBuilder* _M0L3bufS247;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS248;
  int32_t _M0L7_2abindS250;
  int32_t _M0L1iS251;
  #line 124 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 125 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L3bufS247 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS248 = _M0L4selfS249;
  moonbit_incref(_M0L3bufS247);
  #line 127 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS247, 91);
  _M0L7_2abindS250 = _M0L7_2aselfS248->$1;
  _M0L1iS251 = 0;
  while (1) {
    if (_M0L1iS251 < _M0L7_2abindS250) {
      moonbit_string_t* _M0L3bufS1380 = _M0L7_2aselfS248->$0;
      moonbit_string_t _M0L4itemS252 =
        (moonbit_string_t)_M0L3bufS1380[_M0L1iS251];
      int32_t _M0L6_2atmpS1379;
      if (_M0L1iS251 != 0) {
        if (_M0L4itemS252) {
          moonbit_incref(_M0L4itemS252);
        }
        moonbit_incref(_M0L3bufS247);
        #line 130 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS247, (moonbit_string_t)moonbit_string_literal_75.data);
      } else if (_M0L4itemS252) {
        moonbit_incref(_M0L4itemS252);
      }
      if (_M0L4itemS252 == 0) {
        if (_M0L4itemS252) {
          moonbit_decref(_M0L4itemS252);
        }
        moonbit_incref(_M0L3bufS247);
        #line 133 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS247, (moonbit_string_t)moonbit_string_literal_76.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS253 = _M0L4itemS252;
        moonbit_string_t _M0L6_2alocS254 = _M0L7_2aSomeS253;
        moonbit_string_t _M0L6_2atmpS1378;
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1378
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS254);
        moonbit_incref(_M0L3bufS247);
        #line 134 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS247, _M0L6_2atmpS1378);
      }
      _M0L6_2atmpS1379 = _M0L1iS251 + 1;
      _M0L1iS251 = _M0L6_2atmpS1379;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS248);
    }
    break;
  }
  moonbit_incref(_M0L3bufS247);
  #line 137 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS247, 93);
  #line 138 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS247);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS246
) {
  moonbit_string_t _M0L6_2atmpS1377;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1376;
  #line 95 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1377 = _M0L4selfS246;
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1376 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1377);
  #line 96 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1376);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS245
) {
  struct _M0TPB13StringBuilder* _M0L2sbS244;
  struct _M0TPC16string10StringView _M0L8filenameS1362;
  struct _M0TPC16string10StringView _M0L11start__lineS1365;
  moonbit_string_t _M0L6_2atmpS1364;
  moonbit_string_t _M0L6_2atmpS1363;
  struct _M0TPC16string10StringView _M0L13start__columnS1368;
  moonbit_string_t _M0L6_2atmpS1367;
  moonbit_string_t _M0L6_2atmpS1366;
  struct _M0TPC16string10StringView _M0L9end__lineS1371;
  moonbit_string_t _M0L6_2atmpS1370;
  moonbit_string_t _M0L6_2atmpS1369;
  struct _M0TPC16string10StringView _M0L8_2afieldS2289;
  int32_t _M0L6_2acntS2392;
  struct _M0TPC16string10StringView _M0L11end__columnS1375;
  moonbit_string_t _M0L6_2atmpS1374;
  moonbit_string_t _M0L6_2atmpS1373;
  moonbit_string_t _M0L6_2atmpS1372;
  #line 82 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  #line 83 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS244 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L2sbS244);
  #line 84 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS244, (moonbit_string_t)moonbit_string_literal_77.data);
  _M0L8filenameS1362
  = (struct _M0TPC16string10StringView){
    _M0L4selfS245->$0_1, _M0L4selfS245->$0_2, _M0L4selfS245->$0_0
  };
  moonbit_incref(_M0L8filenameS1362.$0);
  moonbit_incref(_M0L2sbS244);
  #line 85 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS244, _M0L8filenameS1362);
  _M0L11start__lineS1365
  = (struct _M0TPC16string10StringView){
    _M0L4selfS245->$1_1, _M0L4selfS245->$1_2, _M0L4selfS245->$1_0
  };
  moonbit_incref(_M0L11start__lineS1365.$0);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1364
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1365);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1363
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_78.data, _M0L6_2atmpS1364);
  moonbit_decref(_M0L6_2atmpS1364);
  moonbit_incref(_M0L2sbS244);
  #line 86 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS244, _M0L6_2atmpS1363);
  _M0L13start__columnS1368
  = (struct _M0TPC16string10StringView){
    _M0L4selfS245->$2_1, _M0L4selfS245->$2_2, _M0L4selfS245->$2_0
  };
  moonbit_incref(_M0L13start__columnS1368.$0);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1367
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1368);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1366
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_79.data, _M0L6_2atmpS1367);
  moonbit_decref(_M0L6_2atmpS1367);
  moonbit_incref(_M0L2sbS244);
  #line 87 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS244, _M0L6_2atmpS1366);
  _M0L9end__lineS1371
  = (struct _M0TPC16string10StringView){
    _M0L4selfS245->$3_1, _M0L4selfS245->$3_2, _M0L4selfS245->$3_0
  };
  moonbit_incref(_M0L9end__lineS1371.$0);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1370
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1371);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1369
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_80.data, _M0L6_2atmpS1370);
  moonbit_decref(_M0L6_2atmpS1370);
  moonbit_incref(_M0L2sbS244);
  #line 88 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS244, _M0L6_2atmpS1369);
  _M0L8_2afieldS2289
  = (struct _M0TPC16string10StringView){
    _M0L4selfS245->$4_1, _M0L4selfS245->$4_2, _M0L4selfS245->$4_0
  };
  _M0L6_2acntS2392 = Moonbit_object_header(_M0L4selfS245)->rc;
  if (_M0L6_2acntS2392 > 1) {
    int32_t _M0L11_2anew__cntS2397 = _M0L6_2acntS2392 - 1;
    Moonbit_object_header(_M0L4selfS245)->rc = _M0L11_2anew__cntS2397;
    moonbit_incref(_M0L8_2afieldS2289.$0);
  } else if (_M0L6_2acntS2392 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2396 =
      (struct _M0TPC16string10StringView){_M0L4selfS245->$3_1,
                                            _M0L4selfS245->$3_2,
                                            _M0L4selfS245->$3_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2395;
    struct _M0TPC16string10StringView _M0L8_2afieldS2394;
    struct _M0TPC16string10StringView _M0L8_2afieldS2393;
    moonbit_decref(_M0L8_2afieldS2396.$0);
    _M0L8_2afieldS2395
    = (struct _M0TPC16string10StringView){
      _M0L4selfS245->$2_1, _M0L4selfS245->$2_2, _M0L4selfS245->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2395.$0);
    _M0L8_2afieldS2394
    = (struct _M0TPC16string10StringView){
      _M0L4selfS245->$1_1, _M0L4selfS245->$1_2, _M0L4selfS245->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2394.$0);
    _M0L8_2afieldS2393
    = (struct _M0TPC16string10StringView){
      _M0L4selfS245->$0_1, _M0L4selfS245->$0_2, _M0L4selfS245->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2393.$0);
    #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS245);
  }
  _M0L11end__columnS1375 = _M0L8_2afieldS2289;
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1374
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1375);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1373
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_81.data, _M0L6_2atmpS1374);
  moonbit_decref(_M0L6_2atmpS1374);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1372
  = moonbit_add_string(_M0L6_2atmpS1373, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1373);
  moonbit_incref(_M0L2sbS244);
  #line 89 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS244, _M0L6_2atmpS1372);
  #line 90 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS244);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS243,
  moonbit_string_t _M0L3strS242
) {
  int32_t _M0L8str__lenS241;
  int32_t _M0L3lenS1357;
  int32_t _M0L6_2atmpS1356;
  uint16_t* _M0L4dataS1358;
  int32_t _M0L3lenS1359;
  int32_t _M0L3lenS1361;
  int32_t _M0L6_2atmpS1360;
  #line 81 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS241 = Moonbit_array_length(_M0L3strS242);
  _M0L3lenS1357 = _M0L4selfS243->$1;
  _M0L6_2atmpS1356 = _M0L3lenS1357 + _M0L8str__lenS241;
  moonbit_incref(_M0L4selfS243);
  #line 83 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS243, _M0L6_2atmpS1356);
  _M0L4dataS1358 = _M0L4selfS243->$0;
  _M0L3lenS1359 = _M0L4selfS243->$1;
  moonbit_incref(_M0L4dataS1358);
  #line 84 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS1358, _M0L3lenS1359, _M0L3strS242, 0, _M0L8str__lenS241);
  _M0L3lenS1361 = _M0L4selfS243->$1;
  _M0L6_2atmpS1360 = _M0L3lenS1361 + _M0L8str__lenS241;
  _M0L4selfS243->$1 = _M0L6_2atmpS1360;
  moonbit_decref(_M0L4selfS243);
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS237,
  int32_t _M0L11dst__offsetS240,
  moonbit_string_t _M0L3strS238,
  int32_t _M0L11str__offsetS233,
  int32_t _M0L3lenS234
) {
  int32_t _M0L16end__str__offsetS232;
  int32_t _M0L1iS235;
  int32_t _M0L1jS236;
  #line 66 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS232 = _M0L11str__offsetS233 + _M0L3lenS234;
  _M0L1iS235 = _M0L11str__offsetS233;
  _M0L1jS236 = _M0L11dst__offsetS240;
  while (1) {
    if (_M0L1iS235 < _M0L16end__str__offsetS232) {
      int32_t _M0L6_2atmpS1353 = _M0L3strS238[_M0L1iS235];
      int32_t _M0L6_2atmpS1354;
      int32_t _M0L6_2atmpS1355;
      if (
        _M0L1jS236 < 0 || _M0L1jS236 >= Moonbit_array_length(_M0L4selfS237)
      ) {
        #line 75 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS237[_M0L1jS236] = _M0L6_2atmpS1353;
      _M0L6_2atmpS1354 = _M0L1iS235 + 1;
      _M0L6_2atmpS1355 = _M0L1jS236 + 1;
      _M0L1iS235 = _M0L6_2atmpS1354;
      _M0L1jS236 = _M0L6_2atmpS1355;
      continue;
    } else {
      moonbit_decref(_M0L3strS238);
      moonbit_decref(_M0L4selfS237);
    }
    break;
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS231,
  struct _M0TPC16string10StringView _M0L3objS230
) {
  struct _M0TPB6Logger _M0L6_2atmpS1352;
  #line 17 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1352
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS231
  };
  #line 21 "/Users/user/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS230, _M0L6_2atmpS1352);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS175
) {
  int32_t _M0L6_2atmpS1351;
  struct _M0TPC16string10StringView _M0L7_2abindS174;
  moonbit_string_t _M0L7_2adataS176;
  int32_t _M0L8_2astartS177;
  int32_t _M0L6_2atmpS1350;
  int32_t _M0L6_2aendS178;
  int32_t _M0Lm9_2acursorS179;
  int32_t _M0Lm13accept__stateS180;
  int32_t _M0Lm10match__endS181;
  int32_t _M0Lm20match__tag__saver__0S182;
  int32_t _M0Lm20match__tag__saver__1S183;
  int32_t _M0Lm20match__tag__saver__2S184;
  int32_t _M0Lm20match__tag__saver__3S185;
  int32_t _M0Lm20match__tag__saver__4S186;
  int32_t _M0Lm6tag__0S187;
  int32_t _M0Lm9tag__0__1S188;
  int32_t _M0Lm9tag__0__2S189;
  int32_t _M0Lm6tag__2S190;
  int32_t _M0Lm6tag__1S191;
  int32_t _M0Lm9tag__1__1S192;
  int32_t _M0Lm6tag__4S193;
  int32_t _M0Lm6tag__3S194;
  int32_t _M0L6_2atmpS1309;
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1351 = Moonbit_array_length(_M0L4reprS175);
  _M0L7_2abindS174
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1351, _M0L4reprS175
  };
  moonbit_incref(_M0L7_2abindS174.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS176 = _M0MPC16string10StringView4data(_M0L7_2abindS174);
  moonbit_incref(_M0L7_2abindS174.$0);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS177
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS174);
  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1350 = _M0MPC16string10StringView6length(_M0L7_2abindS174);
  _M0L6_2aendS178 = _M0L8_2astartS177 + _M0L6_2atmpS1350;
  _M0Lm9_2acursorS179 = _M0L8_2astartS177;
  _M0Lm13accept__stateS180 = -1;
  _M0Lm10match__endS181 = -1;
  _M0Lm20match__tag__saver__0S182 = -1;
  _M0Lm20match__tag__saver__1S183 = -1;
  _M0Lm20match__tag__saver__2S184 = -1;
  _M0Lm20match__tag__saver__3S185 = -1;
  _M0Lm20match__tag__saver__4S186 = -1;
  _M0Lm6tag__0S187 = -1;
  _M0Lm9tag__0__1S188 = -1;
  _M0Lm9tag__0__2S189 = -1;
  _M0Lm6tag__2S190 = -1;
  _M0Lm6tag__1S191 = -1;
  _M0Lm9tag__1__1S192 = -1;
  _M0Lm6tag__4S193 = -1;
  _M0Lm6tag__3S194 = -1;
  _M0L6_2atmpS1309 = _M0Lm9_2acursorS179;
  if (_M0L6_2atmpS1309 < _M0L6_2aendS178) {
    int32_t _M0L6_2atmpS1310 = _M0Lm9_2acursorS179;
    int32_t _M0L12dispatch__15S202;
    _M0Lm9_2acursorS179 = _M0L6_2atmpS1310 + 1;
    _M0L12dispatch__15S202 = 0;
    loop__label__15_205:;
    while (1) {
      int32_t _M0L6_2atmpS1314;
      int32_t _M0L6_2atmpS1311;
      switch (_M0L12dispatch__15S202) {
        case 6: {
          int32_t _M0L6_2atmpS1317;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1317 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1317 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1319 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS210;
            int32_t _M0L6_2atmpS1318;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS210
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1319);
            _M0L6_2atmpS1318 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1318 + 1;
            if (_M0L10next__charS210 == 58) {
              _M0L12dispatch__15S202 = 1;
              goto loop__label__15_205;
            } else {
              _M0L12dispatch__15S202 = 6;
              goto loop__label__15_205;
            }
          } else {
            goto join_207;
          }
          break;
        }
        
        case 3: {
          int32_t _M0L6_2atmpS1320;
          _M0Lm9tag__0__2S189 = _M0Lm9tag__0__1S188;
          _M0Lm9tag__0__1S188 = _M0Lm6tag__0S187;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1320 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1320 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1325 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS212;
            int32_t _M0L6_2atmpS1321;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS212
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1325);
            _M0L6_2atmpS1321 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1321 + 1;
            if (_M0L10next__charS212 < 58) {
              if (_M0L10next__charS212 < 48) {
                goto join_211;
              } else {
                int32_t _M0L6_2atmpS1322;
                _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
                _M0Lm9tag__1__1S192 = _M0Lm6tag__1S191;
                _M0Lm6tag__1S191 = _M0Lm9_2acursorS179;
                _M0Lm6tag__2S190 = _M0Lm9_2acursorS179;
                _M0L6_2atmpS1322 = _M0Lm9_2acursorS179;
                if (_M0L6_2atmpS1322 < _M0L6_2aendS178) {
                  int32_t _M0L6_2atmpS1324 = _M0Lm9_2acursorS179;
                  int32_t _M0L10next__charS214;
                  int32_t _M0L6_2atmpS1323;
                  moonbit_incref(_M0L7_2adataS176);
                  #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS214
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1324);
                  _M0L6_2atmpS1323 = _M0Lm9_2acursorS179;
                  _M0Lm9_2acursorS179 = _M0L6_2atmpS1323 + 1;
                  if (_M0L10next__charS214 < 48) {
                    if (_M0L10next__charS214 == 45) {
                      goto join_203;
                    } else {
                      goto join_213;
                    }
                  } else if (_M0L10next__charS214 > 57) {
                    if (_M0L10next__charS214 < 59) {
                      _M0L12dispatch__15S202 = 3;
                      goto loop__label__15_205;
                    } else {
                      goto join_213;
                    }
                  } else {
                    _M0L12dispatch__15S202 = 7;
                    goto loop__label__15_205;
                  }
                  join_213:;
                  _M0L12dispatch__15S202 = 0;
                  goto loop__label__15_205;
                } else {
                  goto join_195;
                }
              }
            } else if (_M0L10next__charS212 > 58) {
              goto join_211;
            } else {
              _M0L12dispatch__15S202 = 1;
              goto loop__label__15_205;
            }
            join_211:;
            _M0L12dispatch__15S202 = 0;
            goto loop__label__15_205;
          } else {
            goto join_195;
          }
          break;
        }
        
        case 7: {
          int32_t _M0L6_2atmpS1326;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0Lm6tag__1S191 = _M0Lm9_2acursorS179;
          _M0Lm6tag__2S190 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1326 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1326 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1328 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS216;
            int32_t _M0L6_2atmpS1327;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS216
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1328);
            _M0L6_2atmpS1327 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1327 + 1;
            if (_M0L10next__charS216 < 48) {
              if (_M0L10next__charS216 == 45) {
                goto join_203;
              } else {
                goto join_215;
              }
            } else if (_M0L10next__charS216 > 57) {
              if (_M0L10next__charS216 < 59) {
                _M0L12dispatch__15S202 = 3;
                goto loop__label__15_205;
              } else {
                goto join_215;
              }
            } else {
              _M0L12dispatch__15S202 = 7;
              goto loop__label__15_205;
            }
            join_215:;
            _M0L12dispatch__15S202 = 0;
            goto loop__label__15_205;
          } else {
            goto join_195;
          }
          break;
        }
        
        case 5: {
          int32_t _M0L6_2atmpS1329;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0Lm6tag__1S191 = _M0Lm9_2acursorS179;
          _M0Lm6tag__4S193 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1329 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1329 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1331 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS218;
            int32_t _M0L6_2atmpS1330;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS218
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1331);
            _M0L6_2atmpS1330 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1330 + 1;
            if (_M0L10next__charS218 < 59) {
              if (_M0L10next__charS218 < 48) {
                goto join_217;
              } else if (_M0L10next__charS218 > 57) {
                _M0L12dispatch__15S202 = 3;
                goto loop__label__15_205;
              } else {
                _M0L12dispatch__15S202 = 5;
                goto loop__label__15_205;
              }
            } else if (_M0L10next__charS218 > 63) {
              if (_M0L10next__charS218 < 65) {
                goto join_208;
              } else {
                goto join_217;
              }
            } else {
              goto join_217;
            }
            join_217:;
            _M0L12dispatch__15S202 = 0;
            goto loop__label__15_205;
          } else {
            goto join_195;
          }
          break;
        }
        
        case 1: {
          int32_t _M0L6_2atmpS1332;
          _M0Lm9tag__0__1S188 = _M0Lm6tag__0S187;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1332 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1332 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1334 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS220;
            int32_t _M0L6_2atmpS1333;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS220
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1334);
            _M0L6_2atmpS1333 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1333 + 1;
            if (_M0L10next__charS220 < 58) {
              if (_M0L10next__charS220 < 48) {
                goto join_219;
              } else {
                _M0L12dispatch__15S202 = 2;
                goto loop__label__15_205;
              }
            } else if (_M0L10next__charS220 > 58) {
              goto join_219;
            } else {
              _M0L12dispatch__15S202 = 1;
              goto loop__label__15_205;
            }
            join_219:;
            _M0L12dispatch__15S202 = 0;
            goto loop__label__15_205;
          } else {
            goto join_195;
          }
          break;
        }
        
        case 4: {
          int32_t _M0L6_2atmpS1335;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0Lm6tag__3S194 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1335 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1335 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1343 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS222;
            int32_t _M0L6_2atmpS1336;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS222
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1343);
            _M0L6_2atmpS1336 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1336 + 1;
            if (_M0L10next__charS222 < 58) {
              if (_M0L10next__charS222 < 48) {
                goto join_221;
              } else {
                _M0L12dispatch__15S202 = 4;
                goto loop__label__15_205;
              }
            } else if (_M0L10next__charS222 > 58) {
              goto join_221;
            } else {
              int32_t _M0L6_2atmpS1337;
              _M0Lm9tag__0__2S189 = _M0Lm9tag__0__1S188;
              _M0Lm9tag__0__1S188 = _M0Lm6tag__0S187;
              _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
              _M0L6_2atmpS1337 = _M0Lm9_2acursorS179;
              if (_M0L6_2atmpS1337 < _M0L6_2aendS178) {
                int32_t _M0L6_2atmpS1342 = _M0Lm9_2acursorS179;
                int32_t _M0L10next__charS224;
                int32_t _M0L6_2atmpS1338;
                moonbit_incref(_M0L7_2adataS176);
                #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                _M0L10next__charS224
                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1342);
                _M0L6_2atmpS1338 = _M0Lm9_2acursorS179;
                _M0Lm9_2acursorS179 = _M0L6_2atmpS1338 + 1;
                if (_M0L10next__charS224 < 58) {
                  if (_M0L10next__charS224 < 48) {
                    goto join_223;
                  } else {
                    int32_t _M0L6_2atmpS1339;
                    _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
                    _M0Lm9tag__1__1S192 = _M0Lm6tag__1S191;
                    _M0Lm6tag__1S191 = _M0Lm9_2acursorS179;
                    _M0Lm6tag__4S193 = _M0Lm9_2acursorS179;
                    _M0L6_2atmpS1339 = _M0Lm9_2acursorS179;
                    if (_M0L6_2atmpS1339 < _M0L6_2aendS178) {
                      int32_t _M0L6_2atmpS1341 = _M0Lm9_2acursorS179;
                      int32_t _M0L10next__charS226;
                      int32_t _M0L6_2atmpS1340;
                      moonbit_incref(_M0L7_2adataS176);
                      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS226
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1341);
                      _M0L6_2atmpS1340 = _M0Lm9_2acursorS179;
                      _M0Lm9_2acursorS179 = _M0L6_2atmpS1340 + 1;
                      if (_M0L10next__charS226 < 59) {
                        if (_M0L10next__charS226 < 48) {
                          goto join_225;
                        } else if (_M0L10next__charS226 > 57) {
                          _M0L12dispatch__15S202 = 3;
                          goto loop__label__15_205;
                        } else {
                          _M0L12dispatch__15S202 = 5;
                          goto loop__label__15_205;
                        }
                      } else if (_M0L10next__charS226 > 63) {
                        if (_M0L10next__charS226 < 65) {
                          goto join_208;
                        } else {
                          goto join_225;
                        }
                      } else {
                        goto join_225;
                      }
                      join_225:;
                      _M0L12dispatch__15S202 = 0;
                      goto loop__label__15_205;
                    } else {
                      goto join_195;
                    }
                  }
                } else if (_M0L10next__charS224 > 58) {
                  goto join_223;
                } else {
                  _M0L12dispatch__15S202 = 1;
                  goto loop__label__15_205;
                }
                join_223:;
                _M0L12dispatch__15S202 = 0;
                goto loop__label__15_205;
              } else {
                goto join_195;
              }
            }
            join_221:;
            _M0L12dispatch__15S202 = 0;
            goto loop__label__15_205;
          } else {
            goto join_195;
          }
          break;
        }
        
        case 2: {
          int32_t _M0L6_2atmpS1344;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0Lm6tag__1S191 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1344 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1344 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1346 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS228;
            int32_t _M0L6_2atmpS1345;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS228
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1346);
            _M0L6_2atmpS1345 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1345 + 1;
            if (_M0L10next__charS228 < 58) {
              if (_M0L10next__charS228 < 48) {
                goto join_227;
              } else {
                _M0L12dispatch__15S202 = 2;
                goto loop__label__15_205;
              }
            } else if (_M0L10next__charS228 > 58) {
              goto join_227;
            } else {
              _M0L12dispatch__15S202 = 3;
              goto loop__label__15_205;
            }
            join_227:;
            _M0L12dispatch__15S202 = 0;
            goto loop__label__15_205;
          } else {
            goto join_195;
          }
          break;
        }
        
        case 0: {
          int32_t _M0L6_2atmpS1347;
          _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
          _M0L6_2atmpS1347 = _M0Lm9_2acursorS179;
          if (_M0L6_2atmpS1347 < _M0L6_2aendS178) {
            int32_t _M0L6_2atmpS1349 = _M0Lm9_2acursorS179;
            int32_t _M0L10next__charS229;
            int32_t _M0L6_2atmpS1348;
            moonbit_incref(_M0L7_2adataS176);
            #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS229
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1349);
            _M0L6_2atmpS1348 = _M0Lm9_2acursorS179;
            _M0Lm9_2acursorS179 = _M0L6_2atmpS1348 + 1;
            if (_M0L10next__charS229 == 58) {
              _M0L12dispatch__15S202 = 1;
              goto loop__label__15_205;
            } else {
              _M0L12dispatch__15S202 = 0;
              goto loop__label__15_205;
            }
          } else {
            goto join_195;
          }
          break;
        }
        default: {
          goto join_195;
          break;
        }
      }
      join_208:;
      _M0Lm9tag__0__1S188 = _M0Lm9tag__0__2S189;
      _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
      _M0Lm6tag__1S191 = _M0Lm9tag__1__1S192;
      _M0L6_2atmpS1314 = _M0Lm9_2acursorS179;
      if (_M0L6_2atmpS1314 < _M0L6_2aendS178) {
        int32_t _M0L6_2atmpS1316 = _M0Lm9_2acursorS179;
        int32_t _M0L10next__charS209;
        int32_t _M0L6_2atmpS1315;
        moonbit_incref(_M0L7_2adataS176);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS209
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1316);
        _M0L6_2atmpS1315 = _M0Lm9_2acursorS179;
        _M0Lm9_2acursorS179 = _M0L6_2atmpS1315 + 1;
        if (_M0L10next__charS209 == 58) {
          _M0L12dispatch__15S202 = 1;
          continue;
        } else {
          _M0L12dispatch__15S202 = 6;
          continue;
        }
      } else {
        goto join_207;
      }
      join_207:;
      _M0Lm6tag__0S187 = _M0Lm9tag__0__1S188;
      _M0Lm20match__tag__saver__0S182 = _M0Lm6tag__0S187;
      _M0Lm20match__tag__saver__1S183 = _M0Lm6tag__1S191;
      _M0Lm20match__tag__saver__2S184 = _M0Lm6tag__2S190;
      _M0Lm20match__tag__saver__3S185 = _M0Lm6tag__3S194;
      _M0Lm20match__tag__saver__4S186 = _M0Lm6tag__4S193;
      _M0Lm13accept__stateS180 = 0;
      _M0Lm10match__endS181 = _M0Lm9_2acursorS179;
      goto join_195;
      join_203:;
      _M0Lm9tag__0__1S188 = _M0Lm9tag__0__2S189;
      _M0Lm6tag__0S187 = _M0Lm9_2acursorS179;
      _M0Lm6tag__1S191 = _M0Lm9tag__1__1S192;
      _M0L6_2atmpS1311 = _M0Lm9_2acursorS179;
      if (_M0L6_2atmpS1311 < _M0L6_2aendS178) {
        int32_t _M0L6_2atmpS1313 = _M0Lm9_2acursorS179;
        int32_t _M0L10next__charS206;
        int32_t _M0L6_2atmpS1312;
        moonbit_incref(_M0L7_2adataS176);
        #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
        _M0L10next__charS206
        = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS176, _M0L6_2atmpS1313);
        _M0L6_2atmpS1312 = _M0Lm9_2acursorS179;
        _M0Lm9_2acursorS179 = _M0L6_2atmpS1312 + 1;
        if (_M0L10next__charS206 < 58) {
          if (_M0L10next__charS206 < 48) {
            goto join_204;
          } else {
            _M0L12dispatch__15S202 = 4;
            continue;
          }
        } else if (_M0L10next__charS206 > 58) {
          goto join_204;
        } else {
          _M0L12dispatch__15S202 = 1;
          continue;
        }
        join_204:;
        _M0L12dispatch__15S202 = 0;
        continue;
      } else {
        goto join_195;
      }
      break;
    }
  } else {
    goto join_195;
  }
  join_195:;
  switch (_M0Lm13accept__stateS180) {
    case 0: {
      int32_t _M0L6_2atmpS1308 = _M0Lm20match__tag__saver__0S182;
      int32_t _M0L6_2atmpS1307 = _M0L6_2atmpS1308 + 1;
      int64_t _M0L6_2atmpS1304 = (int64_t)_M0L6_2atmpS1307;
      int32_t _M0L6_2atmpS1306 = _M0Lm20match__tag__saver__1S183;
      int64_t _M0L6_2atmpS1305 = (int64_t)_M0L6_2atmpS1306;
      struct _M0TPC16string10StringView _M0L11start__lineS196;
      int32_t _M0L6_2atmpS1303;
      int32_t _M0L6_2atmpS1302;
      int64_t _M0L6_2atmpS1299;
      int32_t _M0L6_2atmpS1301;
      int64_t _M0L6_2atmpS1300;
      struct _M0TPC16string10StringView _M0L13start__columnS197;
      int64_t _M0L6_2atmpS1296;
      int32_t _M0L6_2atmpS1298;
      int64_t _M0L6_2atmpS1297;
      struct _M0TPC16string10StringView _M0L8filenameS198;
      int32_t _M0L6_2atmpS1295;
      int32_t _M0L6_2atmpS1294;
      int64_t _M0L6_2atmpS1291;
      int32_t _M0L6_2atmpS1293;
      int64_t _M0L6_2atmpS1292;
      struct _M0TPC16string10StringView _M0L9end__lineS199;
      int32_t _M0L6_2atmpS1290;
      int32_t _M0L6_2atmpS1289;
      int64_t _M0L6_2atmpS1286;
      int32_t _M0L6_2atmpS1288;
      int64_t _M0L6_2atmpS1287;
      struct _M0TPC16string10StringView _M0L11end__columnS200;
      int32_t _M0L6_2atmpS1285;
      int32_t _M0L6_2atmpS1284;
      int64_t _M0L6_2atmpS1281;
      int32_t _M0L6_2atmpS1283;
      int64_t _M0L6_2atmpS1282;
      struct _M0TPC16string10StringView _M0L6_2atmpS2295;
      struct _M0TPB13SourceLocRepr* _block_2548;
      moonbit_incref(_M0L7_2adataS176);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS196
      = _M0MPC16string6String4view(_M0L7_2adataS176, _M0L6_2atmpS1304, _M0L6_2atmpS1305);
      _M0L6_2atmpS1303 = _M0Lm20match__tag__saver__1S183;
      _M0L6_2atmpS1302 = _M0L6_2atmpS1303 + 1;
      _M0L6_2atmpS1299 = (int64_t)_M0L6_2atmpS1302;
      _M0L6_2atmpS1301 = _M0Lm20match__tag__saver__2S184;
      _M0L6_2atmpS1300 = (int64_t)_M0L6_2atmpS1301;
      moonbit_incref(_M0L7_2adataS176);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS197
      = _M0MPC16string6String4view(_M0L7_2adataS176, _M0L6_2atmpS1299, _M0L6_2atmpS1300);
      _M0L6_2atmpS1296 = (int64_t)_M0L8_2astartS177;
      _M0L6_2atmpS1298 = _M0Lm20match__tag__saver__0S182;
      _M0L6_2atmpS1297 = (int64_t)_M0L6_2atmpS1298;
      moonbit_incref(_M0L7_2adataS176);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS198
      = _M0MPC16string6String4view(_M0L7_2adataS176, _M0L6_2atmpS1296, _M0L6_2atmpS1297);
      _M0L6_2atmpS1295 = _M0Lm20match__tag__saver__2S184;
      _M0L6_2atmpS1294 = _M0L6_2atmpS1295 + 1;
      _M0L6_2atmpS1291 = (int64_t)_M0L6_2atmpS1294;
      _M0L6_2atmpS1293 = _M0Lm20match__tag__saver__3S185;
      _M0L6_2atmpS1292 = (int64_t)_M0L6_2atmpS1293;
      moonbit_incref(_M0L7_2adataS176);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS199
      = _M0MPC16string6String4view(_M0L7_2adataS176, _M0L6_2atmpS1291, _M0L6_2atmpS1292);
      _M0L6_2atmpS1290 = _M0Lm20match__tag__saver__3S185;
      _M0L6_2atmpS1289 = _M0L6_2atmpS1290 + 1;
      _M0L6_2atmpS1286 = (int64_t)_M0L6_2atmpS1289;
      _M0L6_2atmpS1288 = _M0Lm20match__tag__saver__4S186;
      _M0L6_2atmpS1287 = (int64_t)_M0L6_2atmpS1288;
      moonbit_incref(_M0L7_2adataS176);
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS200
      = _M0MPC16string6String4view(_M0L7_2adataS176, _M0L6_2atmpS1286, _M0L6_2atmpS1287);
      _M0L6_2atmpS1285 = _M0Lm20match__tag__saver__4S186;
      _M0L6_2atmpS1284 = _M0L6_2atmpS1285 + 1;
      _M0L6_2atmpS1281 = (int64_t)_M0L6_2atmpS1284;
      _M0L6_2atmpS1283 = _M0Lm10match__endS181;
      _M0L6_2atmpS1282 = (int64_t)_M0L6_2atmpS1283;
      #line 62 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6_2atmpS2295
      = _M0MPC16string6String4view(_M0L7_2adataS176, _M0L6_2atmpS1281, _M0L6_2atmpS1282);
      moonbit_decref(_M0L6_2atmpS2295.$0);
      _block_2548
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2548)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 5, 0);
      _block_2548->$0_0 = _M0L8filenameS198.$0;
      _block_2548->$0_1 = _M0L8filenameS198.$1;
      _block_2548->$0_2 = _M0L8filenameS198.$2;
      _block_2548->$1_0 = _M0L11start__lineS196.$0;
      _block_2548->$1_1 = _M0L11start__lineS196.$1;
      _block_2548->$1_2 = _M0L11start__lineS196.$2;
      _block_2548->$2_0 = _M0L13start__columnS197.$0;
      _block_2548->$2_1 = _M0L13start__columnS197.$1;
      _block_2548->$2_2 = _M0L13start__columnS197.$2;
      _block_2548->$3_0 = _M0L9end__lineS199.$0;
      _block_2548->$3_1 = _M0L9end__lineS199.$1;
      _block_2548->$3_2 = _M0L9end__lineS199.$2;
      _block_2548->$4_0 = _M0L11end__columnS200.$0;
      _block_2548->$4_1 = _M0L11end__columnS200.$1;
      _block_2548->$4_2 = _M0L11end__columnS200.$2;
      return _block_2548;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS176);
      #line 77 "/Users/user/.moon/lib/core/builtin/autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC16string6String14escape_2einner(
  moonbit_string_t _M0L4selfS172,
  int32_t _M0L5quoteS173
) {
  struct _M0TPB13StringBuilder* _M0L3bufS171;
  int32_t _M0L6_2atmpS1280;
  struct _M0TPC16string10StringView _M0L6_2atmpS1278;
  struct _M0TPB6Logger _M0L6_2atmpS1279;
  #line 145 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 146 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS171 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS1280 = Moonbit_array_length(_M0L4selfS172);
  _M0L6_2atmpS1278
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1280, _M0L4selfS172
  };
  moonbit_incref(_M0L3bufS171);
  _M0L6_2atmpS1279
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS171
  };
  #line 147 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0MPC16string10StringView18escape__to_2einner(_M0L6_2atmpS1278, _M0L6_2atmpS1279, _M0L5quoteS173);
  #line 148 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS171);
}

int32_t _M0MPC16string10StringView18escape__to_2einner(
  struct _M0TPC16string10StringView _M0L4selfS163,
  struct _M0TPB6Logger _M0L6loggerS161,
  int32_t _M0L5quoteS160
) {
  int32_t _M0L3lenS162;
  struct _M0TURPB6LoggerRPC16string10StringViewE* _M0L6_2aenvS164;
  int32_t _M0L1iS165;
  int32_t _M0L3segS166;
  #line 179 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L5quoteS160) {
    if (_M0L6loggerS161.$1) {
      moonbit_incref(_M0L6loggerS161.$1);
    }
    #line 185 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS161.$0->$method_3(_M0L6loggerS161.$1, 34);
  }
  moonbit_incref(_M0L4selfS163.$0);
  #line 187 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L3lenS162 = _M0MPC16string10StringView6length(_M0L4selfS163);
  if (_M0L6loggerS161.$1) {
    moonbit_incref(_M0L6loggerS161.$1);
  }
  moonbit_incref(_M0L4selfS163.$0);
  _M0L6_2aenvS164
  = (struct _M0TURPB6LoggerRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TURPB6LoggerRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2aenvS164)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPB6LoggerRPC16string10StringViewE, $0_0) >> 2, 3, 0);
  _M0L6_2aenvS164->$0_0 = _M0L6loggerS161.$0;
  _M0L6_2aenvS164->$0_1 = _M0L6loggerS161.$1;
  _M0L6_2aenvS164->$1_0 = _M0L4selfS163.$0;
  _M0L6_2aenvS164->$1_1 = _M0L4selfS163.$1;
  _M0L6_2aenvS164->$1_2 = _M0L4selfS163.$2;
  _M0L1iS165 = 0;
  _M0L3segS166 = 0;
  _2afor_167:;
  while (1) {
    int32_t _M0L4codeS168;
    int32_t _M0L1cS170;
    int32_t _M0L6_2atmpS1262;
    int32_t _M0L6_2atmpS1263;
    int32_t _M0L6_2atmpS1264;
    int32_t _tmp_2552;
    int32_t _tmp_2553;
    if (_M0L1iS165 >= _M0L3lenS162) {
      moonbit_decref(_M0L4selfS163.$0);
      #line 195 "/Users/user/.moon/lib/core/builtin/show.mbt"
      _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
      break;
    }
    moonbit_incref(_M0L4selfS163.$0);
    #line 198 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L4codeS168
    = _M0MPC16string10StringView11unsafe__get(_M0L4selfS163, _M0L1iS165);
    switch (_M0L4codeS168) {
      case 34: {
        _M0L1cS170 = _M0L4codeS168;
        goto join_169;
        break;
      }
      
      case 92: {
        _M0L1cS170 = _M0L4codeS168;
        goto join_169;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1265;
        int32_t _M0L6_2atmpS1266;
        moonbit_incref(_M0L6_2aenvS164);
        #line 207 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
        if (_M0L6loggerS161.$1) {
          moonbit_incref(_M0L6loggerS161.$1);
        }
        #line 208 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS161.$0->$method_0(_M0L6loggerS161.$1, (moonbit_string_t)moonbit_string_literal_82.data);
        _M0L6_2atmpS1265 = _M0L1iS165 + 1;
        _M0L6_2atmpS1266 = _M0L1iS165 + 1;
        _M0L1iS165 = _M0L6_2atmpS1265;
        _M0L3segS166 = _M0L6_2atmpS1266;
        goto _2afor_167;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1267;
        int32_t _M0L6_2atmpS1268;
        moonbit_incref(_M0L6_2aenvS164);
        #line 212 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
        if (_M0L6loggerS161.$1) {
          moonbit_incref(_M0L6loggerS161.$1);
        }
        #line 213 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS161.$0->$method_0(_M0L6loggerS161.$1, (moonbit_string_t)moonbit_string_literal_83.data);
        _M0L6_2atmpS1267 = _M0L1iS165 + 1;
        _M0L6_2atmpS1268 = _M0L1iS165 + 1;
        _M0L1iS165 = _M0L6_2atmpS1267;
        _M0L3segS166 = _M0L6_2atmpS1268;
        goto _2afor_167;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1269;
        int32_t _M0L6_2atmpS1270;
        moonbit_incref(_M0L6_2aenvS164);
        #line 217 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
        if (_M0L6loggerS161.$1) {
          moonbit_incref(_M0L6loggerS161.$1);
        }
        #line 218 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS161.$0->$method_0(_M0L6loggerS161.$1, (moonbit_string_t)moonbit_string_literal_84.data);
        _M0L6_2atmpS1269 = _M0L1iS165 + 1;
        _M0L6_2atmpS1270 = _M0L1iS165 + 1;
        _M0L1iS165 = _M0L6_2atmpS1269;
        _M0L3segS166 = _M0L6_2atmpS1270;
        goto _2afor_167;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1271;
        int32_t _M0L6_2atmpS1272;
        moonbit_incref(_M0L6_2aenvS164);
        #line 222 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
        if (_M0L6loggerS161.$1) {
          moonbit_incref(_M0L6loggerS161.$1);
        }
        #line 223 "/Users/user/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS161.$0->$method_0(_M0L6loggerS161.$1, (moonbit_string_t)moonbit_string_literal_85.data);
        _M0L6_2atmpS1271 = _M0L1iS165 + 1;
        _M0L6_2atmpS1272 = _M0L1iS165 + 1;
        _M0L1iS165 = _M0L6_2atmpS1271;
        _M0L3segS166 = _M0L6_2atmpS1272;
        goto _2afor_167;
        break;
      }
      default: {
        if (_M0L4codeS168 < 32) {
          int32_t _M0L6_2atmpS1274;
          moonbit_string_t _M0L6_2atmpS1273;
          int32_t _M0L6_2atmpS1275;
          int32_t _M0L6_2atmpS1276;
          moonbit_incref(_M0L6_2aenvS164);
          #line 228 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
          if (_M0L6loggerS161.$1) {
            moonbit_incref(_M0L6loggerS161.$1);
          }
          #line 229 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS161.$0->$method_0(_M0L6loggerS161.$1, (moonbit_string_t)moonbit_string_literal_86.data);
          _M0L6_2atmpS1274 = _M0L4codeS168 & 0xff;
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1273 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1274);
          if (_M0L6loggerS161.$1) {
            moonbit_incref(_M0L6loggerS161.$1);
          }
          #line 230 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS161.$0->$method_0(_M0L6loggerS161.$1, _M0L6_2atmpS1273);
          if (_M0L6loggerS161.$1) {
            moonbit_incref(_M0L6loggerS161.$1);
          }
          #line 231 "/Users/user/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS161.$0->$method_3(_M0L6loggerS161.$1, 125);
          _M0L6_2atmpS1275 = _M0L1iS165 + 1;
          _M0L6_2atmpS1276 = _M0L1iS165 + 1;
          _M0L1iS165 = _M0L6_2atmpS1275;
          _M0L3segS166 = _M0L6_2atmpS1276;
          goto _2afor_167;
        } else {
          int32_t _M0L6_2atmpS1277 = _M0L1iS165 + 1;
          int32_t _tmp_2551 = _M0L3segS166;
          _M0L1iS165 = _M0L6_2atmpS1277;
          _M0L3segS166 = _tmp_2551;
          goto _2afor_167;
        }
        break;
      }
    }
    goto joinlet_2550;
    join_169:;
    moonbit_incref(_M0L6_2aenvS164);
    #line 201 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(_M0L6_2aenvS164, _M0L3segS166, _M0L1iS165);
    if (_M0L6loggerS161.$1) {
      moonbit_incref(_M0L6loggerS161.$1);
    }
    #line 202 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS161.$0->$method_3(_M0L6loggerS161.$1, 92);
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1262 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS170);
    if (_M0L6loggerS161.$1) {
      moonbit_incref(_M0L6loggerS161.$1);
    }
    #line 203 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS161.$0->$method_3(_M0L6loggerS161.$1, _M0L6_2atmpS1262);
    _M0L6_2atmpS1263 = _M0L1iS165 + 1;
    _M0L6_2atmpS1264 = _M0L1iS165 + 1;
    _M0L1iS165 = _M0L6_2atmpS1263;
    _M0L3segS166 = _M0L6_2atmpS1264;
    continue;
    joinlet_2550:;
    _tmp_2552 = _M0L1iS165;
    _tmp_2553 = _M0L3segS166;
    _M0L1iS165 = _tmp_2552;
    _M0L3segS166 = _tmp_2553;
    continue;
    break;
  }
  if (_M0L5quoteS160) {
    #line 239 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS161.$0->$method_3(_M0L6loggerS161.$1, 34);
  } else if (_M0L6loggerS161.$1) {
    moonbit_decref(_M0L6loggerS161.$1);
  }
  return 0;
}

int32_t _M0MPC16string10StringView18escape__to_2einnerN14flush__segmentS3700(
  struct _M0TURPB6LoggerRPC16string10StringViewE* _M0L6_2aenvS156,
  int32_t _M0L3segS159,
  int32_t _M0L1iS158
) {
  struct _M0TPC16string10StringView _M0L4selfS155;
  struct _M0TPB6Logger _M0L8_2afieldS2296;
  int32_t _M0L6_2acntS2398;
  struct _M0TPB6Logger _M0L6loggerS157;
  #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L4selfS155
  = (struct _M0TPC16string10StringView){
    _M0L6_2aenvS156->$1_1, _M0L6_2aenvS156->$1_2, _M0L6_2aenvS156->$1_0
  };
  _M0L8_2afieldS2296
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS156->$0_0, _M0L6_2aenvS156->$0_1
  };
  _M0L6_2acntS2398 = Moonbit_object_header(_M0L6_2aenvS156)->rc;
  if (_M0L6_2acntS2398 > 1) {
    int32_t _M0L11_2anew__cntS2399 = _M0L6_2acntS2398 - 1;
    Moonbit_object_header(_M0L6_2aenvS156)->rc = _M0L11_2anew__cntS2399;
    moonbit_incref(_M0L4selfS155.$0);
    if (_M0L8_2afieldS2296.$1) {
      moonbit_incref(_M0L8_2afieldS2296.$1);
    }
  } else if (_M0L6_2acntS2398 == 1) {
    #line 188 "/Users/user/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS156);
  }
  _M0L6loggerS157 = _M0L8_2afieldS2296;
  if (_M0L1iS158 > _M0L3segS159) {
    int64_t _M0L6_2atmpS1261 = (int64_t)_M0L1iS158;
    struct _M0TPC16string10StringView _M0L6_2atmpS1260;
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1260
    = _M0MPC16string10StringView11sub_2einner(_M0L4selfS155, _M0L3segS159, _M0L6_2atmpS1261);
    #line 190 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS157.$0->$method_2(_M0L6loggerS157.$1, _M0L6_2atmpS1260);
  } else {
    if (_M0L6loggerS157.$1) {
      moonbit_decref(_M0L6loggerS157.$1);
    }
    moonbit_decref(_M0L4selfS155.$0);
  }
  return 0;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS153,
  int32_t _M0L5indexS154
) {
  moonbit_string_t _M0L3strS1257;
  int32_t _M0L5startS1259;
  int32_t _M0L6_2atmpS1258;
  int32_t _result_2554;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1257 = _M0L4selfS153.$0;
  _M0L5startS1259 = _M0L4selfS153.$1;
  _M0L6_2atmpS1258 = _M0L5startS1259 + _M0L5indexS154;
  _result_2554 = _M0L3strS1257[_M0L6_2atmpS1258];
  moonbit_decref(_M0L3strS1257);
  return _result_2554;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView11sub_2einner(
  struct _M0TPC16string10StringView _M0L4selfS146,
  int32_t _M0L5startS152,
  int64_t _M0L3endS148
) {
  moonbit_string_t _M0L3strS1256;
  int32_t _M0L8str__lenS145;
  int32_t _M0L8abs__endS147;
  int32_t _M0L10abs__startS151;
  int32_t _M0L5startS1244;
  int32_t _if__result_2555;
  #line 712 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS1256 = _M0L4selfS146.$0;
  _M0L8str__lenS145 = Moonbit_array_length(_M0L3strS1256);
  if (_M0L3endS148 == 4294967296ll) {
    _M0L8abs__endS147 = _M0L4selfS146.$2;
  } else {
    int64_t _M0L7_2aSomeS149 = _M0L3endS148;
    int32_t _M0L6_2aendS150 = (int32_t)_M0L7_2aSomeS149;
    if (_M0L6_2aendS150 < 0) {
      int32_t _M0L3endS1254 = _M0L4selfS146.$2;
      _M0L8abs__endS147 = _M0L3endS1254 + _M0L6_2aendS150;
    } else {
      int32_t _M0L5startS1255 = _M0L4selfS146.$1;
      _M0L8abs__endS147 = _M0L5startS1255 + _M0L6_2aendS150;
    }
  }
  if (_M0L5startS152 < 0) {
    int32_t _M0L3endS1252 = _M0L4selfS146.$2;
    _M0L10abs__startS151 = _M0L3endS1252 + _M0L5startS152;
  } else {
    int32_t _M0L5startS1253 = _M0L4selfS146.$1;
    _M0L10abs__startS151 = _M0L5startS1253 + _M0L5startS152;
  }
  _M0L5startS1244 = _M0L4selfS146.$1;
  if (_M0L10abs__startS151 >= _M0L5startS1244) {
    if (_M0L10abs__startS151 <= _M0L8abs__endS147) {
      int32_t _M0L3endS1243 = _M0L4selfS146.$2;
      _if__result_2555 = _M0L8abs__endS147 <= _M0L3endS1243;
    } else {
      _if__result_2555 = 0;
    }
  } else {
    _if__result_2555 = 0;
  }
  if (_if__result_2555) {
    moonbit_string_t _M0L3strS1251;
    if (_M0L10abs__startS151 < _M0L8str__lenS145) {
      moonbit_string_t _M0L3strS1247 = _M0L4selfS146.$0;
      int32_t _M0L6_2atmpS1246 = _M0L3strS1247[_M0L10abs__startS151];
      int32_t _M0L6_2atmpS1245;
      #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1245
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1246);
      if (!_M0L6_2atmpS1245) {
        
      } else {
        #line 738 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L8abs__endS147 < _M0L8str__lenS145) {
      moonbit_string_t _M0L3strS1250 = _M0L4selfS146.$0;
      int32_t _M0L6_2atmpS1249 = _M0L3strS1250[_M0L8abs__endS147];
      int32_t _M0L6_2atmpS1248;
      #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1248
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1249);
      if (!_M0L6_2atmpS1248) {
        
      } else {
        #line 741 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    _M0L3strS1251 = _M0L4selfS146.$0;
    return (struct _M0TPC16string10StringView){_M0L10abs__startS151,
                                                 _M0L8abs__endS147,
                                                 _M0L3strS1251};
  } else {
    moonbit_decref(_M0L4selfS146.$0);
    #line 732 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS144
) {
  int32_t _M0L3endS1241;
  int32_t _M0L5startS1242;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1241 = _M0L4selfS144.$2;
  _M0L5startS1242 = _M0L4selfS144.$1;
  moonbit_decref(_M0L4selfS144.$0);
  return _M0L3endS1241 - _M0L5startS1242;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS143) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS142;
  int32_t _M0L6_2atmpS1238;
  int32_t _M0L6_2atmpS1237;
  int32_t _M0L6_2atmpS1240;
  int32_t _M0L6_2atmpS1239;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1236;
  #line 109 "/Users/user/.moon/lib/core/builtin/show.mbt"
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS142 = _M0MPB13StringBuilder11new_2einner(0);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1238 = _M0IPC14byte4BytePB3Div3div(_M0L1bS143, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1237
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1238);
  moonbit_incref(_M0L7_2aselfS142);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS142, _M0L6_2atmpS1237);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1240 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS143, 16);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1239
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(_M0L6_2atmpS1240);
  moonbit_incref(_M0L7_2aselfS142);
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS142, _M0L6_2atmpS1239);
  _M0L6_2atmpS1236 = _M0L7_2aselfS142;
  #line 118 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1236);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3715(int32_t _M0L1iS141) {
  #line 110 "/Users/user/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS141 < 10) {
    int32_t _M0L6_2atmpS1233;
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1233 = _M0IPC14byte4BytePB3Add3add(_M0L1iS141, 48);
    #line 112 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1233);
  } else {
    int32_t _M0L6_2atmpS1235;
    int32_t _M0L6_2atmpS1234;
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1235 = _M0IPC14byte4BytePB3Add3add(_M0L1iS141, 97);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1234 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1235, 10);
    #line 114 "/Users/user/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1234);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS139,
  int32_t _M0L4thatS140
) {
  int32_t _M0L6_2atmpS1231;
  int32_t _M0L6_2atmpS1232;
  int32_t _M0L6_2atmpS1230;
  #line 120 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1231 = (int32_t)_M0L4selfS139;
  _M0L6_2atmpS1232 = (int32_t)_M0L4thatS140;
  _M0L6_2atmpS1230 = _M0L6_2atmpS1231 - _M0L6_2atmpS1232;
  return _M0L6_2atmpS1230 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS137,
  int32_t _M0L4thatS138
) {
  int32_t _M0L6_2atmpS1228;
  int32_t _M0L6_2atmpS1229;
  int32_t _M0L6_2atmpS1227;
  #line 67 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1228 = (int32_t)_M0L4selfS137;
  _M0L6_2atmpS1229 = (int32_t)_M0L4thatS138;
  _M0L6_2atmpS1227 = _M0L6_2atmpS1228 % _M0L6_2atmpS1229;
  return _M0L6_2atmpS1227 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS135,
  int32_t _M0L4thatS136
) {
  int32_t _M0L6_2atmpS1225;
  int32_t _M0L6_2atmpS1226;
  int32_t _M0L6_2atmpS1224;
  #line 62 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1225 = (int32_t)_M0L4selfS135;
  _M0L6_2atmpS1226 = (int32_t)_M0L4thatS136;
  _M0L6_2atmpS1224 = _M0L6_2atmpS1225 / _M0L6_2atmpS1226;
  return _M0L6_2atmpS1224 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS133,
  int32_t _M0L4thatS134
) {
  int32_t _M0L6_2atmpS1222;
  int32_t _M0L6_2atmpS1223;
  int32_t _M0L6_2atmpS1221;
  #line 106 "/Users/user/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1222 = (int32_t)_M0L4selfS133;
  _M0L6_2atmpS1223 = (int32_t)_M0L4thatS134;
  _M0L6_2atmpS1221 = _M0L6_2atmpS1222 + _M0L6_2atmpS1223;
  return _M0L6_2atmpS1221 & 0xff;
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS127
) {
  int32_t _M0L17codepoint__lengthS126;
  int32_t _M0L6_2atmpS1220;
  moonbit_bytes_t _M0L4dataS128;
  int32_t _M0L1iS129;
  int32_t _M0L12utf16__indexS130;
  #line 102 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS127);
  #line 104 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS126
  = _M0MPC16string6String20char__length_2einner(_M0L1sS127, 0, 4294967296ll);
  _M0L6_2atmpS1220 = _M0L17codepoint__lengthS126 * 4;
  _M0L4dataS128 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1220, 0);
  _M0L1iS129 = 0;
  _M0L12utf16__indexS130 = 0;
  while (1) {
    if (_M0L1iS129 < _M0L17codepoint__lengthS126) {
      int32_t _M0L6_2atmpS1217;
      int32_t _M0L1cS131;
      int32_t _M0L6_2atmpS1218;
      int32_t _M0L6_2atmpS1219;
      moonbit_incref(_M0L1sS127);
      #line 109 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1217
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS127, _M0L12utf16__indexS130);
      _M0L1cS131 = _M0L6_2atmpS1217;
      if (_M0L1cS131 > 65535) {
        int32_t _M0L6_2atmpS1185 = _M0L1iS129 * 4;
        int32_t _M0L6_2atmpS1187 = _M0L1cS131 & 255;
        int32_t _M0L6_2atmpS1186 = _M0L6_2atmpS1187 & 0xff;
        int32_t _M0L6_2atmpS1192;
        int32_t _M0L6_2atmpS1188;
        int32_t _M0L6_2atmpS1191;
        int32_t _M0L6_2atmpS1190;
        int32_t _M0L6_2atmpS1189;
        int32_t _M0L6_2atmpS1197;
        int32_t _M0L6_2atmpS1193;
        int32_t _M0L6_2atmpS1196;
        int32_t _M0L6_2atmpS1195;
        int32_t _M0L6_2atmpS1194;
        int32_t _M0L6_2atmpS1202;
        int32_t _M0L6_2atmpS1198;
        int32_t _M0L6_2atmpS1201;
        int32_t _M0L6_2atmpS1200;
        int32_t _M0L6_2atmpS1199;
        int32_t _M0L6_2atmpS1203;
        int32_t _M0L6_2atmpS1204;
        if (
          _M0L6_2atmpS1185 < 0
          || _M0L6_2atmpS1185 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 111 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1185] = _M0L6_2atmpS1186;
        _M0L6_2atmpS1192 = _M0L1iS129 * 4;
        _M0L6_2atmpS1188 = _M0L6_2atmpS1192 + 1;
        _M0L6_2atmpS1191 = _M0L1cS131 >> 8;
        _M0L6_2atmpS1190 = _M0L6_2atmpS1191 & 255;
        _M0L6_2atmpS1189 = _M0L6_2atmpS1190 & 0xff;
        if (
          _M0L6_2atmpS1188 < 0
          || _M0L6_2atmpS1188 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 112 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1188] = _M0L6_2atmpS1189;
        _M0L6_2atmpS1197 = _M0L1iS129 * 4;
        _M0L6_2atmpS1193 = _M0L6_2atmpS1197 + 2;
        _M0L6_2atmpS1196 = _M0L1cS131 >> 16;
        _M0L6_2atmpS1195 = _M0L6_2atmpS1196 & 255;
        _M0L6_2atmpS1194 = _M0L6_2atmpS1195 & 0xff;
        if (
          _M0L6_2atmpS1193 < 0
          || _M0L6_2atmpS1193 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 113 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1193] = _M0L6_2atmpS1194;
        _M0L6_2atmpS1202 = _M0L1iS129 * 4;
        _M0L6_2atmpS1198 = _M0L6_2atmpS1202 + 3;
        _M0L6_2atmpS1201 = _M0L1cS131 >> 24;
        _M0L6_2atmpS1200 = _M0L6_2atmpS1201 & 255;
        _M0L6_2atmpS1199 = _M0L6_2atmpS1200 & 0xff;
        if (
          _M0L6_2atmpS1198 < 0
          || _M0L6_2atmpS1198 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 114 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1198] = _M0L6_2atmpS1199;
        _M0L6_2atmpS1203 = _M0L1iS129 + 1;
        _M0L6_2atmpS1204 = _M0L12utf16__indexS130 + 2;
        _M0L1iS129 = _M0L6_2atmpS1203;
        _M0L12utf16__indexS130 = _M0L6_2atmpS1204;
        continue;
      } else {
        int32_t _M0L6_2atmpS1205 = _M0L1iS129 * 4;
        int32_t _M0L6_2atmpS1207 = _M0L1cS131 & 255;
        int32_t _M0L6_2atmpS1206 = _M0L6_2atmpS1207 & 0xff;
        int32_t _M0L6_2atmpS1212;
        int32_t _M0L6_2atmpS1208;
        int32_t _M0L6_2atmpS1211;
        int32_t _M0L6_2atmpS1210;
        int32_t _M0L6_2atmpS1209;
        int32_t _M0L6_2atmpS1214;
        int32_t _M0L6_2atmpS1213;
        int32_t _M0L6_2atmpS1216;
        int32_t _M0L6_2atmpS1215;
        if (
          _M0L6_2atmpS1205 < 0
          || _M0L6_2atmpS1205 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 117 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1205] = _M0L6_2atmpS1206;
        _M0L6_2atmpS1212 = _M0L1iS129 * 4;
        _M0L6_2atmpS1208 = _M0L6_2atmpS1212 + 1;
        _M0L6_2atmpS1211 = _M0L1cS131 >> 8;
        _M0L6_2atmpS1210 = _M0L6_2atmpS1211 & 255;
        _M0L6_2atmpS1209 = _M0L6_2atmpS1210 & 0xff;
        if (
          _M0L6_2atmpS1208 < 0
          || _M0L6_2atmpS1208 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 118 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1208] = _M0L6_2atmpS1209;
        _M0L6_2atmpS1214 = _M0L1iS129 * 4;
        _M0L6_2atmpS1213 = _M0L6_2atmpS1214 + 2;
        if (
          _M0L6_2atmpS1213 < 0
          || _M0L6_2atmpS1213 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 119 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1213] = 0;
        _M0L6_2atmpS1216 = _M0L1iS129 * 4;
        _M0L6_2atmpS1215 = _M0L6_2atmpS1216 + 3;
        if (
          _M0L6_2atmpS1215 < 0
          || _M0L6_2atmpS1215 >= Moonbit_array_length(_M0L4dataS128)
        ) {
          #line 120 "/Users/user/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS128[_M0L6_2atmpS1215] = 0;
      }
      _M0L6_2atmpS1218 = _M0L1iS129 + 1;
      _M0L6_2atmpS1219 = _M0L12utf16__indexS130 + 1;
      _M0L1iS129 = _M0L6_2atmpS1218;
      _M0L12utf16__indexS130 = _M0L6_2atmpS1219;
      continue;
    } else {
      moonbit_decref(_M0L1sS127);
    }
    break;
  }
  #line 123 "/Users/user/.moon/lib/core/builtin/console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS128);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS123,
  int32_t _M0L5indexS124
) {
  int32_t _M0L2c1S122;
  #line 91 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
  _M0L2c1S122 = _M0L4selfS123[_M0L5indexS124];
  #line 94 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S122)) {
    int32_t _M0L6_2atmpS1184 = _M0L5indexS124 + 1;
    int32_t _M0L2c2S125 = _M0L4selfS123[_M0L6_2atmpS1184];
    int32_t _M0L6_2atmpS1182;
    int32_t _M0L6_2atmpS1183;
    moonbit_decref(_M0L4selfS123);
    _M0L6_2atmpS1182 = (int32_t)_M0L2c1S122;
    _M0L6_2atmpS1183 = (int32_t)_M0L2c2S125;
    #line 96 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1182, _M0L6_2atmpS1183);
  } else {
    moonbit_decref(_M0L4selfS123);
    #line 98 "/Users/user/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S122);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS121) {
  int32_t _M0L6_2atmpS1181;
  #line 68 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS1181 = (int32_t)_M0L4selfS121;
  return _M0L6_2atmpS1181;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS119,
  int32_t _M0L8trailingS120
) {
  int32_t _M0L6_2atmpS1180;
  int32_t _M0L6_2atmpS1179;
  int32_t _M0L6_2atmpS1178;
  int32_t _M0L6_2atmpS1177;
  int32_t _M0L6_2atmpS1176;
  #line 40 "/Users/user/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1180 = _M0L7leadingS119 - 55296;
  _M0L6_2atmpS1179 = _M0L6_2atmpS1180 * 1024;
  _M0L6_2atmpS1178 = _M0L6_2atmpS1179 + _M0L8trailingS120;
  _M0L6_2atmpS1177 = _M0L6_2atmpS1178 - 56320;
  _M0L6_2atmpS1176 = _M0L6_2atmpS1177 + 65536;
  return _M0L6_2atmpS1176;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS112,
  int32_t _M0L13start__offsetS113,
  int64_t _M0L11end__offsetS110
) {
  int32_t _M0L11end__offsetS109;
  int32_t _if__result_2557;
  #line 60 "/Users/user/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS110 == 4294967296ll) {
    _M0L11end__offsetS109 = Moonbit_array_length(_M0L4selfS112);
  } else {
    int64_t _M0L7_2aSomeS111 = _M0L11end__offsetS110;
    _M0L11end__offsetS109 = (int32_t)_M0L7_2aSomeS111;
  }
  if (_M0L13start__offsetS113 >= 0) {
    if (_M0L13start__offsetS113 <= _M0L11end__offsetS109) {
      int32_t _M0L6_2atmpS1169 = Moonbit_array_length(_M0L4selfS112);
      _if__result_2557 = _M0L11end__offsetS109 <= _M0L6_2atmpS1169;
    } else {
      _if__result_2557 = 0;
    }
  } else {
    _if__result_2557 = 0;
  }
  if (_if__result_2557) {
    int32_t _M0L12utf16__indexS114 = _M0L13start__offsetS113;
    int32_t _M0L11char__countS115 = 0;
    while (1) {
      if (_M0L12utf16__indexS114 < _M0L11end__offsetS109) {
        int32_t _M0L2c1S116 = _M0L4selfS112[_M0L12utf16__indexS114];
        int32_t _if__result_2559;
        int32_t _M0L6_2atmpS1174;
        int32_t _M0L6_2atmpS1175;
        #line 76 "/Users/user/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S116)) {
          int32_t _M0L6_2atmpS1170 = _M0L12utf16__indexS114 + 1;
          _if__result_2559 = _M0L6_2atmpS1170 < _M0L11end__offsetS109;
        } else {
          _if__result_2559 = 0;
        }
        if (_if__result_2559) {
          int32_t _M0L6_2atmpS1173 = _M0L12utf16__indexS114 + 1;
          int32_t _M0L2c2S117 = _M0L4selfS112[_M0L6_2atmpS1173];
          #line 78 "/Users/user/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S117)) {
            int32_t _M0L6_2atmpS1171 = _M0L12utf16__indexS114 + 2;
            int32_t _M0L6_2atmpS1172 = _M0L11char__countS115 + 1;
            _M0L12utf16__indexS114 = _M0L6_2atmpS1171;
            _M0L11char__countS115 = _M0L6_2atmpS1172;
            continue;
          } else {
            #line 81 "/Users/user/.moon/lib/core/builtin/string.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_63.data);
          }
        }
        _M0L6_2atmpS1174 = _M0L12utf16__indexS114 + 1;
        _M0L6_2atmpS1175 = _M0L11char__countS115 + 1;
        _M0L12utf16__indexS114 = _M0L6_2atmpS1174;
        _M0L11char__countS115 = _M0L6_2atmpS1175;
        continue;
      } else {
        moonbit_decref(_M0L4selfS112);
        return _M0L11char__countS115;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS112);
    #line 70 "/Users/user/.moon/lib/core/builtin/string.mbt"
    return _M0FPC15abort5abortGiE((moonbit_string_t)moonbit_string_literal_87.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS108) {
  #line 45 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS108 >= 56320) {
    return _M0L4selfS108 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS107) {
  #line 28 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS107 >= 55296) {
    return _M0L4selfS107 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS88) {
  struct _M0TPB13StringBuilder* _M0L3bufS86;
  int32_t _M0L3lenS87;
  int32_t _M0L3remS89;
  int32_t _M0L1iS90;
  #line 61 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 63 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L3bufS86 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS87 = Moonbit_array_length(_M0L4dataS88);
  _M0L3remS89 = _M0L3lenS87 % 3;
  _M0L1iS90 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1121 = _M0L3lenS87 - _M0L3remS89;
    if (_M0L1iS90 < _M0L6_2atmpS1121) {
      int32_t _M0L6_2atmpS1143;
      int32_t _M0L2b0S91;
      int32_t _M0L6_2atmpS1142;
      int32_t _M0L6_2atmpS1141;
      int32_t _M0L2b1S92;
      int32_t _M0L6_2atmpS1140;
      int32_t _M0L6_2atmpS1139;
      int32_t _M0L2b2S93;
      int32_t _M0L6_2atmpS1138;
      int32_t _M0L6_2atmpS1137;
      int32_t _M0L2x0S94;
      int32_t _M0L6_2atmpS1136;
      int32_t _M0L6_2atmpS1133;
      int32_t _M0L6_2atmpS1135;
      int32_t _M0L6_2atmpS1134;
      int32_t _M0L6_2atmpS1132;
      int32_t _M0L2x1S95;
      int32_t _M0L6_2atmpS1131;
      int32_t _M0L6_2atmpS1128;
      int32_t _M0L6_2atmpS1130;
      int32_t _M0L6_2atmpS1129;
      int32_t _M0L6_2atmpS1127;
      int32_t _M0L2x2S96;
      int32_t _M0L6_2atmpS1126;
      int32_t _M0L2x3S97;
      int32_t _M0L6_2atmpS1122;
      int32_t _M0L6_2atmpS1123;
      int32_t _M0L6_2atmpS1124;
      int32_t _M0L6_2atmpS1125;
      int32_t _M0L6_2atmpS1144;
      if (_M0L1iS90 < 0 || _M0L1iS90 >= Moonbit_array_length(_M0L4dataS88)) {
        #line 67 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1143 = (int32_t)_M0L4dataS88[_M0L1iS90];
      _M0L2b0S91 = (int32_t)_M0L6_2atmpS1143;
      _M0L6_2atmpS1142 = _M0L1iS90 + 1;
      if (
        _M0L6_2atmpS1142 < 0
        || _M0L6_2atmpS1142 >= Moonbit_array_length(_M0L4dataS88)
      ) {
        #line 68 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1141 = (int32_t)_M0L4dataS88[_M0L6_2atmpS1142];
      _M0L2b1S92 = (int32_t)_M0L6_2atmpS1141;
      _M0L6_2atmpS1140 = _M0L1iS90 + 2;
      if (
        _M0L6_2atmpS1140 < 0
        || _M0L6_2atmpS1140 >= Moonbit_array_length(_M0L4dataS88)
      ) {
        #line 69 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1139 = (int32_t)_M0L4dataS88[_M0L6_2atmpS1140];
      _M0L2b2S93 = (int32_t)_M0L6_2atmpS1139;
      _M0L6_2atmpS1138 = _M0L2b0S91 & 252;
      _M0L6_2atmpS1137 = _M0L6_2atmpS1138 >> 2;
      if (
        _M0L6_2atmpS1137 < 0
        || _M0L6_2atmpS1137
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 70 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S94 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1137];
      _M0L6_2atmpS1136 = _M0L2b0S91 & 3;
      _M0L6_2atmpS1133 = _M0L6_2atmpS1136 << 4;
      _M0L6_2atmpS1135 = _M0L2b1S92 & 240;
      _M0L6_2atmpS1134 = _M0L6_2atmpS1135 >> 4;
      _M0L6_2atmpS1132 = _M0L6_2atmpS1133 | _M0L6_2atmpS1134;
      if (
        _M0L6_2atmpS1132 < 0
        || _M0L6_2atmpS1132
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 71 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S95 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1132];
      _M0L6_2atmpS1131 = _M0L2b1S92 & 15;
      _M0L6_2atmpS1128 = _M0L6_2atmpS1131 << 2;
      _M0L6_2atmpS1130 = _M0L2b2S93 & 192;
      _M0L6_2atmpS1129 = _M0L6_2atmpS1130 >> 6;
      _M0L6_2atmpS1127 = _M0L6_2atmpS1128 | _M0L6_2atmpS1129;
      if (
        _M0L6_2atmpS1127 < 0
        || _M0L6_2atmpS1127
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 72 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S96 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1127];
      _M0L6_2atmpS1126 = _M0L2b2S93 & 63;
      if (
        _M0L6_2atmpS1126 < 0
        || _M0L6_2atmpS1126
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
      ) {
        #line 73 "/Users/user/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S97 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1126];
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1122 = _M0MPC14byte4Byte8to__char(_M0L2x0S94);
      moonbit_incref(_M0L3bufS86);
      #line 74 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1122);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1123 = _M0MPC14byte4Byte8to__char(_M0L2x1S95);
      moonbit_incref(_M0L3bufS86);
      #line 75 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1123);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1124 = _M0MPC14byte4Byte8to__char(_M0L2x2S96);
      moonbit_incref(_M0L3bufS86);
      #line 76 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1124);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1125 = _M0MPC14byte4Byte8to__char(_M0L2x3S97);
      moonbit_incref(_M0L3bufS86);
      #line 77 "/Users/user/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1125);
      _M0L6_2atmpS1144 = _M0L1iS90 + 3;
      _M0L1iS90 = _M0L6_2atmpS1144;
      continue;
    }
    break;
  }
  if (_M0L3remS89 == 1) {
    int32_t _M0L6_2atmpS1152 = _M0L3lenS87 - 1;
    int32_t _M0L6_2atmpS1151;
    int32_t _M0L2b0S99;
    int32_t _M0L6_2atmpS1150;
    int32_t _M0L6_2atmpS1149;
    int32_t _M0L2x0S100;
    int32_t _M0L6_2atmpS1148;
    int32_t _M0L6_2atmpS1147;
    int32_t _M0L2x1S101;
    int32_t _M0L6_2atmpS1145;
    int32_t _M0L6_2atmpS1146;
    if (
      _M0L6_2atmpS1152 < 0
      || _M0L6_2atmpS1152 >= Moonbit_array_length(_M0L4dataS88)
    ) {
      #line 80 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1151 = (int32_t)_M0L4dataS88[_M0L6_2atmpS1152];
    moonbit_decref(_M0L4dataS88);
    _M0L2b0S99 = (int32_t)_M0L6_2atmpS1151;
    _M0L6_2atmpS1150 = _M0L2b0S99 & 252;
    _M0L6_2atmpS1149 = _M0L6_2atmpS1150 >> 2;
    if (
      _M0L6_2atmpS1149 < 0
      || _M0L6_2atmpS1149
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 81 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S100 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1149];
    _M0L6_2atmpS1148 = _M0L2b0S99 & 3;
    _M0L6_2atmpS1147 = _M0L6_2atmpS1148 << 4;
    if (
      _M0L6_2atmpS1147 < 0
      || _M0L6_2atmpS1147
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 82 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S101 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1147];
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1145 = _M0MPC14byte4Byte8to__char(_M0L2x0S100);
    moonbit_incref(_M0L3bufS86);
    #line 83 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1145);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1146 = _M0MPC14byte4Byte8to__char(_M0L2x1S101);
    moonbit_incref(_M0L3bufS86);
    #line 84 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1146);
    moonbit_incref(_M0L3bufS86);
    #line 85 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, 61);
    moonbit_incref(_M0L3bufS86);
    #line 86 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, 61);
  } else if (_M0L3remS89 == 2) {
    int32_t _M0L6_2atmpS1168 = _M0L3lenS87 - 2;
    int32_t _M0L6_2atmpS1167;
    int32_t _M0L2b0S102;
    int32_t _M0L6_2atmpS1166;
    int32_t _M0L6_2atmpS1165;
    int32_t _M0L2b1S103;
    int32_t _M0L6_2atmpS1164;
    int32_t _M0L6_2atmpS1163;
    int32_t _M0L2x0S104;
    int32_t _M0L6_2atmpS1162;
    int32_t _M0L6_2atmpS1159;
    int32_t _M0L6_2atmpS1161;
    int32_t _M0L6_2atmpS1160;
    int32_t _M0L6_2atmpS1158;
    int32_t _M0L2x1S105;
    int32_t _M0L6_2atmpS1157;
    int32_t _M0L6_2atmpS1156;
    int32_t _M0L2x2S106;
    int32_t _M0L6_2atmpS1153;
    int32_t _M0L6_2atmpS1154;
    int32_t _M0L6_2atmpS1155;
    if (
      _M0L6_2atmpS1168 < 0
      || _M0L6_2atmpS1168 >= Moonbit_array_length(_M0L4dataS88)
    ) {
      #line 88 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1167 = (int32_t)_M0L4dataS88[_M0L6_2atmpS1168];
    _M0L2b0S102 = (int32_t)_M0L6_2atmpS1167;
    _M0L6_2atmpS1166 = _M0L3lenS87 - 1;
    if (
      _M0L6_2atmpS1166 < 0
      || _M0L6_2atmpS1166 >= Moonbit_array_length(_M0L4dataS88)
    ) {
      #line 89 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1165 = (int32_t)_M0L4dataS88[_M0L6_2atmpS1166];
    moonbit_decref(_M0L4dataS88);
    _M0L2b1S103 = (int32_t)_M0L6_2atmpS1165;
    _M0L6_2atmpS1164 = _M0L2b0S102 & 252;
    _M0L6_2atmpS1163 = _M0L6_2atmpS1164 >> 2;
    if (
      _M0L6_2atmpS1163 < 0
      || _M0L6_2atmpS1163
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 90 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S104 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1163];
    _M0L6_2atmpS1162 = _M0L2b0S102 & 3;
    _M0L6_2atmpS1159 = _M0L6_2atmpS1162 << 4;
    _M0L6_2atmpS1161 = _M0L2b1S103 & 240;
    _M0L6_2atmpS1160 = _M0L6_2atmpS1161 >> 4;
    _M0L6_2atmpS1158 = _M0L6_2atmpS1159 | _M0L6_2atmpS1160;
    if (
      _M0L6_2atmpS1158 < 0
      || _M0L6_2atmpS1158
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 91 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S105 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1158];
    _M0L6_2atmpS1157 = _M0L2b1S103 & 15;
    _M0L6_2atmpS1156 = _M0L6_2atmpS1157 << 2;
    if (
      _M0L6_2atmpS1156 < 0
      || _M0L6_2atmpS1156
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1826)
    ) {
      #line 92 "/Users/user/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S106 = _M0FPB14base64__encodeN6base64S1826[_M0L6_2atmpS1156];
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1153 = _M0MPC14byte4Byte8to__char(_M0L2x0S104);
    moonbit_incref(_M0L3bufS86);
    #line 93 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1153);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1154 = _M0MPC14byte4Byte8to__char(_M0L2x1S105);
    moonbit_incref(_M0L3bufS86);
    #line 94 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1154);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1155 = _M0MPC14byte4Byte8to__char(_M0L2x2S106);
    moonbit_incref(_M0L3bufS86);
    #line 95 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, _M0L6_2atmpS1155);
    moonbit_incref(_M0L3bufS86);
    #line 96 "/Users/user/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS86, 61);
  } else {
    moonbit_decref(_M0L4dataS88);
  }
  #line 98 "/Users/user/.moon/lib/core/builtin/console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS86);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  int32_t _M0L2chS83
) {
  uint32_t _M0L4codeS82;
  #line 90 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 91 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS82 = _M0MPC14char4Char8to__uint(_M0L2chS83);
  if (_M0L4codeS82 <= 65535u) {
    int32_t _M0L3lenS1100 = _M0L4selfS84->$1;
    int32_t _M0L6_2atmpS1099 = _M0L3lenS1100 + 1;
    uint16_t* _M0L4dataS1101;
    int32_t _M0L3lenS1102;
    int32_t _M0L6_2atmpS1103;
    int32_t _M0L3lenS1105;
    int32_t _M0L6_2atmpS1104;
    moonbit_incref(_M0L4selfS84);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS84, _M0L6_2atmpS1099);
    _M0L4dataS1101 = _M0L4selfS84->$0;
    _M0L3lenS1102 = _M0L4selfS84->$1;
    moonbit_incref(_M0L4dataS1101);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1103 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS82);
    if (
      _M0L3lenS1102 < 0
      || _M0L3lenS1102 >= Moonbit_array_length(_M0L4dataS1101)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1101[_M0L3lenS1102] = _M0L6_2atmpS1103;
    moonbit_decref(_M0L4dataS1101);
    _M0L3lenS1105 = _M0L4selfS84->$1;
    _M0L6_2atmpS1104 = _M0L3lenS1105 + 1;
    _M0L4selfS84->$1 = _M0L6_2atmpS1104;
    moonbit_decref(_M0L4selfS84);
  } else if (_M0L4codeS82 <= 1114111u) {
    int32_t _M0L3lenS1107 = _M0L4selfS84->$1;
    int32_t _M0L6_2atmpS1106 = _M0L3lenS1107 + 2;
    uint32_t _M0L4codeS85;
    uint16_t* _M0L4dataS1108;
    int32_t _M0L3lenS1109;
    uint32_t _M0L6_2atmpS1112;
    uint32_t _M0L6_2atmpS1111;
    int32_t _M0L6_2atmpS1110;
    uint16_t* _M0L4dataS1113;
    int32_t _M0L3lenS1118;
    int32_t _M0L6_2atmpS1114;
    uint32_t _M0L6_2atmpS1117;
    uint32_t _M0L6_2atmpS1116;
    int32_t _M0L6_2atmpS1115;
    int32_t _M0L3lenS1120;
    int32_t _M0L6_2atmpS1119;
    moonbit_incref(_M0L4selfS84);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS84, _M0L6_2atmpS1106);
    _M0L4codeS85 = _M0L4codeS82 - 65536u;
    _M0L4dataS1108 = _M0L4selfS84->$0;
    _M0L3lenS1109 = _M0L4selfS84->$1;
    _M0L6_2atmpS1112 = _M0L4codeS85 >> 10;
    _M0L6_2atmpS1111 = 55296u + _M0L6_2atmpS1112;
    moonbit_incref(_M0L4dataS1108);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1110 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1111);
    if (
      _M0L3lenS1109 < 0
      || _M0L3lenS1109 >= Moonbit_array_length(_M0L4dataS1108)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1108[_M0L3lenS1109] = _M0L6_2atmpS1110;
    moonbit_decref(_M0L4dataS1108);
    _M0L4dataS1113 = _M0L4selfS84->$0;
    _M0L3lenS1118 = _M0L4selfS84->$1;
    _M0L6_2atmpS1114 = _M0L3lenS1118 + 1;
    _M0L6_2atmpS1117 = _M0L4codeS85 & 1023u;
    _M0L6_2atmpS1116 = 56320u + _M0L6_2atmpS1117;
    moonbit_incref(_M0L4dataS1113);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1115 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS1116);
    if (
      _M0L6_2atmpS1114 < 0
      || _M0L6_2atmpS1114 >= Moonbit_array_length(_M0L4dataS1113)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1113[_M0L6_2atmpS1114] = _M0L6_2atmpS1115;
    moonbit_decref(_M0L4dataS1113);
    _M0L3lenS1120 = _M0L4selfS84->$1;
    _M0L6_2atmpS1119 = _M0L3lenS1120 + 2;
    _M0L4selfS84->$1 = _M0L6_2atmpS1119;
    moonbit_decref(_M0L4selfS84);
  } else {
    moonbit_decref(_M0L4selfS84);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_88.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS76,
  int32_t _M0L8requiredS77
) {
  uint16_t* _M0L4dataS1098;
  int32_t _M0L12current__lenS75;
  int32_t _M0L13enough__spaceS78;
  int32_t _M0L13enough__spaceS79;
  int32_t _M0L6_2atmpS1096;
  uint16_t* _M0L9new__dataS81;
  uint16_t* _M0L4dataS1094;
  int32_t _M0L3lenS1095;
  uint16_t* _M0L6_2aoldS2306;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS1098 = _M0L4selfS76->$0;
  _M0L12current__lenS75 = Moonbit_array_length(_M0L4dataS1098);
  if (_M0L8requiredS77 <= _M0L12current__lenS75) {
    moonbit_decref(_M0L4selfS76);
    return 0;
  }
  _M0L13enough__spaceS79 = _M0L12current__lenS75;
  while (1) {
    if (_M0L13enough__spaceS79 < _M0L8requiredS77) {
      int32_t _M0L6_2atmpS1097 = _M0L13enough__spaceS79 * 2;
      _M0L13enough__spaceS79 = _M0L6_2atmpS1097;
      continue;
    } else {
      _M0L13enough__spaceS78 = _M0L13enough__spaceS79;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1096 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS81
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS78, _M0L6_2atmpS1096);
  _M0L4dataS1094 = _M0L4selfS76->$0;
  _M0L3lenS1095 = _M0L4selfS76->$1;
  moonbit_incref(_M0L4dataS1094);
  moonbit_incref(_M0L9new__dataS81);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS81, 0, _M0L4dataS1094, 0, _M0L3lenS1095);
  _M0L6_2aoldS2306 = _M0L4selfS76->$0;
  moonbit_decref(_M0L6_2aoldS2306);
  _M0L4selfS76->$0 = _M0L9new__dataS81;
  moonbit_decref(_M0L4selfS76);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS74) {
  int32_t _M0L6_2atmpS1093;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1093 = *(int32_t*)&_M0L4selfS74;
  return (uint16_t)_M0L6_2atmpS1093;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS73) {
  int32_t _M0L6_2atmpS1092;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1092 = _M0L4selfS73;
  return *(uint32_t*)&_M0L6_2atmpS1092;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS71
) {
  int32_t _M0L3lenS1084;
  uint16_t* _M0L4dataS1086;
  int32_t _M0L6_2atmpS1085;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1084 = _M0L4selfS71->$1;
  _M0L4dataS1086 = _M0L4selfS71->$0;
  _M0L6_2atmpS1085 = Moonbit_array_length(_M0L4dataS1086);
  if (_M0L3lenS1084 == _M0L6_2atmpS1085) {
    uint16_t* _M0L8_2afieldS2309 = _M0L4selfS71->$0;
    int32_t _M0L6_2acntS2400 = Moonbit_object_header(_M0L4selfS71)->rc;
    uint16_t* _M0L4dataS1087;
    if (_M0L6_2acntS2400 > 1) {
      int32_t _M0L11_2anew__cntS2401 = _M0L6_2acntS2400 - 1;
      Moonbit_object_header(_M0L4selfS71)->rc = _M0L11_2anew__cntS2401;
      moonbit_incref(_M0L8_2afieldS2309);
    } else if (_M0L6_2acntS2400 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS71);
    }
    _M0L4dataS1087 = _M0L8_2afieldS2309;
    return _M0L4dataS1087;
  } else {
    int32_t _M0L3lenS1090 = _M0L4selfS71->$1;
    int32_t _M0L6_2atmpS1091;
    uint16_t* _M0L4dataS72;
    uint16_t* _M0L4dataS1088;
    int32_t _M0L3lenS1089;
    int32_t _M0L6_2acntS2402;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS1091 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS72
    = (uint16_t*)moonbit_make_string(_M0L3lenS1090, _M0L6_2atmpS1091);
    _M0L4dataS1088 = _M0L4selfS71->$0;
    _M0L3lenS1089 = _M0L4selfS71->$1;
    _M0L6_2acntS2402 = Moonbit_object_header(_M0L4selfS71)->rc;
    if (_M0L6_2acntS2402 > 1) {
      int32_t _M0L11_2anew__cntS2403 = _M0L6_2acntS2402 - 1;
      Moonbit_object_header(_M0L4selfS71)->rc = _M0L11_2anew__cntS2403;
      moonbit_incref(_M0L4dataS1088);
    } else if (_M0L6_2acntS2402 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS71);
    }
    moonbit_incref(_M0L4dataS72);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS72, 0, _M0L4dataS1088, 0, _M0L3lenS1089);
    return _M0L4dataS72;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 153 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS69
) {
  int32_t _M0L7initialS68;
  uint16_t* _M0L4dataS70;
  struct _M0TPB13StringBuilder* _block_2562;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS69 < 1) {
    _M0L7initialS68 = 1;
  } else {
    int32_t _M0L6_2atmpS1083 = _M0L10size__hintS69 + 1;
    _M0L7initialS68 = _M0L6_2atmpS1083 / 2;
  }
  _M0L4dataS70 = (uint16_t*)moonbit_make_string(_M0L7initialS68, 0);
  _block_2562
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2562)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2562->$0 = _M0L4dataS70;
  _block_2562->$1 = 0;
  return _block_2562;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS67) {
  int32_t _M0L6_2atmpS1082;
  #line 1867 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1082 = (int32_t)_M0L4selfS67;
  return _M0L6_2atmpS1082;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS52,
  int32_t _M0L11dst__offsetS53,
  moonbit_string_t* _M0L3srcS54,
  int32_t _M0L11src__offsetS55,
  int32_t _M0L3lenS56
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS52, _M0L11dst__offsetS53, _M0L3srcS54, _M0L11src__offsetS55, _M0L3lenS56);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS57,
  int32_t _M0L11dst__offsetS58,
  struct _M0TUsiE** _M0L3srcS59,
  int32_t _M0L11src__offsetS60,
  int32_t _M0L3lenS61
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS57, _M0L11dst__offsetS58, _M0L3srcS59, _M0L11src__offsetS60, _M0L3lenS61);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP36mulpjs4mulp11log__events8LogEventE(
  void** _M0L3dstS62,
  int32_t _M0L11dst__offsetS63,
  void** _M0L3srcS64,
  int32_t _M0L11src__offsetS65,
  int32_t _M0L3lenS66
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP36mulpjs4mulp11log__events8LogEventEE(_M0L3dstS62, _M0L11dst__offsetS63, _M0L3srcS64, _M0L11src__offsetS65, _M0L3lenS66);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS16,
  int32_t _M0L11dst__offsetS18,
  uint16_t* _M0L3srcS17,
  int32_t _M0L11src__offsetS19,
  int32_t _M0L3lenS21
) {
  int32_t _if__result_2563;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS16 == _M0L3srcS17) {
    _if__result_2563 = _M0L11dst__offsetS18 < _M0L11src__offsetS19;
  } else {
    _if__result_2563 = 0;
  }
  if (_if__result_2563) {
    int32_t _M0L1iS20 = 0;
    while (1) {
      if (_M0L1iS20 < _M0L3lenS21) {
        int32_t _M0L6_2atmpS1046 = _M0L11dst__offsetS18 + _M0L1iS20;
        int32_t _M0L6_2atmpS1048 = _M0L11src__offsetS19 + _M0L1iS20;
        int32_t _M0L6_2atmpS1047;
        int32_t _M0L6_2atmpS1049;
        if (
          _M0L6_2atmpS1048 < 0
          || _M0L6_2atmpS1048 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1047 = (int32_t)_M0L3srcS17[_M0L6_2atmpS1048];
        if (
          _M0L6_2atmpS1046 < 0
          || _M0L6_2atmpS1046 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS1046] = _M0L6_2atmpS1047;
        _M0L6_2atmpS1049 = _M0L1iS20 + 1;
        _M0L1iS20 = _M0L6_2atmpS1049;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1054 = _M0L3lenS21 - 1;
    int32_t _M0L1iS23 = _M0L6_2atmpS1054;
    while (1) {
      if (_M0L1iS23 >= 0) {
        int32_t _M0L6_2atmpS1050 = _M0L11dst__offsetS18 + _M0L1iS23;
        int32_t _M0L6_2atmpS1052 = _M0L11src__offsetS19 + _M0L1iS23;
        int32_t _M0L6_2atmpS1051;
        int32_t _M0L6_2atmpS1053;
        if (
          _M0L6_2atmpS1052 < 0
          || _M0L6_2atmpS1052 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1051 = (int32_t)_M0L3srcS17[_M0L6_2atmpS1052];
        if (
          _M0L6_2atmpS1050 < 0
          || _M0L6_2atmpS1050 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS1050] = _M0L6_2atmpS1051;
        _M0L6_2atmpS1053 = _M0L1iS23 - 1;
        _M0L1iS23 = _M0L6_2atmpS1053;
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
  int32_t _if__result_2566;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS25 == _M0L3srcS26) {
    _if__result_2566 = _M0L11dst__offsetS27 < _M0L11src__offsetS28;
  } else {
    _if__result_2566 = 0;
  }
  if (_if__result_2566) {
    int32_t _M0L1iS29 = 0;
    while (1) {
      if (_M0L1iS29 < _M0L3lenS30) {
        int32_t _M0L6_2atmpS1055 = _M0L11dst__offsetS27 + _M0L1iS29;
        int32_t _M0L6_2atmpS1057 = _M0L11src__offsetS28 + _M0L1iS29;
        moonbit_string_t _M0L6_2atmpS1056;
        moonbit_string_t _M0L6_2aoldS2312;
        int32_t _M0L6_2atmpS1058;
        if (
          _M0L6_2atmpS1057 < 0
          || _M0L6_2atmpS1057 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1056 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS1057];
        if (
          _M0L6_2atmpS1055 < 0
          || _M0L6_2atmpS1055 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2312 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS1055];
        moonbit_incref(_M0L6_2atmpS1056);
        moonbit_decref(_M0L6_2aoldS2312);
        _M0L3dstS25[_M0L6_2atmpS1055] = _M0L6_2atmpS1056;
        _M0L6_2atmpS1058 = _M0L1iS29 + 1;
        _M0L1iS29 = _M0L6_2atmpS1058;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1063 = _M0L3lenS30 - 1;
    int32_t _M0L1iS32 = _M0L6_2atmpS1063;
    while (1) {
      if (_M0L1iS32 >= 0) {
        int32_t _M0L6_2atmpS1059 = _M0L11dst__offsetS27 + _M0L1iS32;
        int32_t _M0L6_2atmpS1061 = _M0L11src__offsetS28 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS1060;
        moonbit_string_t _M0L6_2aoldS2314;
        int32_t _M0L6_2atmpS1062;
        if (
          _M0L6_2atmpS1061 < 0
          || _M0L6_2atmpS1061 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1060 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS1061];
        if (
          _M0L6_2atmpS1059 < 0
          || _M0L6_2atmpS1059 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2314 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS1059];
        moonbit_incref(_M0L6_2atmpS1060);
        moonbit_decref(_M0L6_2aoldS2314);
        _M0L3dstS25[_M0L6_2atmpS1059] = _M0L6_2atmpS1060;
        _M0L6_2atmpS1062 = _M0L1iS32 - 1;
        _M0L1iS32 = _M0L6_2atmpS1062;
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
  int32_t _if__result_2569;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS34 == _M0L3srcS35) {
    _if__result_2569 = _M0L11dst__offsetS36 < _M0L11src__offsetS37;
  } else {
    _if__result_2569 = 0;
  }
  if (_if__result_2569) {
    int32_t _M0L1iS38 = 0;
    while (1) {
      if (_M0L1iS38 < _M0L3lenS39) {
        int32_t _M0L6_2atmpS1064 = _M0L11dst__offsetS36 + _M0L1iS38;
        int32_t _M0L6_2atmpS1066 = _M0L11src__offsetS37 + _M0L1iS38;
        struct _M0TUsiE* _M0L6_2atmpS1065;
        struct _M0TUsiE* _M0L6_2aoldS2316;
        int32_t _M0L6_2atmpS1067;
        if (
          _M0L6_2atmpS1066 < 0
          || _M0L6_2atmpS1066 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1065 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS1066];
        if (
          _M0L6_2atmpS1064 < 0
          || _M0L6_2atmpS1064 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2316 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS1064];
        if (_M0L6_2atmpS1065) {
          moonbit_incref(_M0L6_2atmpS1065);
        }
        if (_M0L6_2aoldS2316) {
          moonbit_decref(_M0L6_2aoldS2316);
        }
        _M0L3dstS34[_M0L6_2atmpS1064] = _M0L6_2atmpS1065;
        _M0L6_2atmpS1067 = _M0L1iS38 + 1;
        _M0L1iS38 = _M0L6_2atmpS1067;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1072 = _M0L3lenS39 - 1;
    int32_t _M0L1iS41 = _M0L6_2atmpS1072;
    while (1) {
      if (_M0L1iS41 >= 0) {
        int32_t _M0L6_2atmpS1068 = _M0L11dst__offsetS36 + _M0L1iS41;
        int32_t _M0L6_2atmpS1070 = _M0L11src__offsetS37 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS1069;
        struct _M0TUsiE* _M0L6_2aoldS2318;
        int32_t _M0L6_2atmpS1071;
        if (
          _M0L6_2atmpS1070 < 0
          || _M0L6_2atmpS1070 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1069 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS1070];
        if (
          _M0L6_2atmpS1068 < 0
          || _M0L6_2atmpS1068 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2318 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS1068];
        if (_M0L6_2atmpS1069) {
          moonbit_incref(_M0L6_2atmpS1069);
        }
        if (_M0L6_2aoldS2318) {
          moonbit_decref(_M0L6_2aoldS2318);
        }
        _M0L3dstS34[_M0L6_2atmpS1068] = _M0L6_2atmpS1069;
        _M0L6_2atmpS1071 = _M0L1iS41 - 1;
        _M0L1iS41 = _M0L6_2atmpS1071;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP36mulpjs4mulp11log__events8LogEventEE(
  void** _M0L3dstS43,
  int32_t _M0L11dst__offsetS45,
  void** _M0L3srcS44,
  int32_t _M0L11src__offsetS46,
  int32_t _M0L3lenS48
) {
  int32_t _if__result_2572;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS43 == _M0L3srcS44) {
    _if__result_2572 = _M0L11dst__offsetS45 < _M0L11src__offsetS46;
  } else {
    _if__result_2572 = 0;
  }
  if (_if__result_2572) {
    int32_t _M0L1iS47 = 0;
    while (1) {
      if (_M0L1iS47 < _M0L3lenS48) {
        int32_t _M0L6_2atmpS1073 = _M0L11dst__offsetS45 + _M0L1iS47;
        int32_t _M0L6_2atmpS1075 = _M0L11src__offsetS46 + _M0L1iS47;
        void* _M0L6_2atmpS1074;
        void* _M0L6_2aoldS2320;
        int32_t _M0L6_2atmpS1076;
        if (
          _M0L6_2atmpS1075 < 0
          || _M0L6_2atmpS1075 >= Moonbit_array_length(_M0L3srcS44)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1074 = (void*)_M0L3srcS44[_M0L6_2atmpS1075];
        if (
          _M0L6_2atmpS1073 < 0
          || _M0L6_2atmpS1073 >= Moonbit_array_length(_M0L3dstS43)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2320 = (void*)_M0L3dstS43[_M0L6_2atmpS1073];
        moonbit_incref(_M0L6_2atmpS1074);
        moonbit_decref(_M0L6_2aoldS2320);
        _M0L3dstS43[_M0L6_2atmpS1073] = _M0L6_2atmpS1074;
        _M0L6_2atmpS1076 = _M0L1iS47 + 1;
        _M0L1iS47 = _M0L6_2atmpS1076;
        continue;
      } else {
        moonbit_decref(_M0L3srcS44);
        moonbit_decref(_M0L3dstS43);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1081 = _M0L3lenS48 - 1;
    int32_t _M0L1iS50 = _M0L6_2atmpS1081;
    while (1) {
      if (_M0L1iS50 >= 0) {
        int32_t _M0L6_2atmpS1077 = _M0L11dst__offsetS45 + _M0L1iS50;
        int32_t _M0L6_2atmpS1079 = _M0L11src__offsetS46 + _M0L1iS50;
        void* _M0L6_2atmpS1078;
        void* _M0L6_2aoldS2322;
        int32_t _M0L6_2atmpS1080;
        if (
          _M0L6_2atmpS1079 < 0
          || _M0L6_2atmpS1079 >= Moonbit_array_length(_M0L3srcS44)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1078 = (void*)_M0L3srcS44[_M0L6_2atmpS1079];
        if (
          _M0L6_2atmpS1077 < 0
          || _M0L6_2atmpS1077 >= Moonbit_array_length(_M0L3dstS43)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2322 = (void*)_M0L3dstS43[_M0L6_2atmpS1077];
        moonbit_incref(_M0L6_2atmpS1078);
        moonbit_decref(_M0L6_2aoldS2322);
        _M0L3dstS43[_M0L6_2atmpS1077] = _M0L6_2atmpS1078;
        _M0L6_2atmpS1080 = _M0L1iS50 - 1;
        _M0L1iS50 = _M0L6_2atmpS1080;
        continue;
      } else {
        moonbit_decref(_M0L3srcS44);
        moonbit_decref(_M0L3dstS43);
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
  uint32_t _M0L3accS1045;
  uint32_t _M0L6_2atmpS1044;
  #line 236 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1045 = _M0L4selfS14->$0;
  _M0L6_2atmpS1044 = _M0L3accS1045 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1044;
  #line 238 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS1042;
  uint32_t _M0L6_2atmpS1043;
  uint32_t _M0L6_2atmpS1041;
  uint32_t _M0L6_2atmpS1040;
  uint32_t _M0L6_2atmpS1039;
  #line 451 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS1042 = _M0L4selfS12->$0;
  _M0L6_2atmpS1043 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS1041 = _M0L3accS1042 + _M0L6_2atmpS1043;
  #line 452 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1040 = _M0FPB4rotl(_M0L6_2atmpS1041, 17);
  _M0L6_2atmpS1039 = _M0L6_2atmpS1040 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS1039;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS1036;
  int32_t _M0L6_2atmpS1038;
  uint32_t _M0L6_2atmpS1037;
  #line 461 "/Users/user/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1036 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS1038 = 32 - _M0L1rS11;
  _M0L6_2atmpS1037 = _M0L1xS10 >> (_M0L6_2atmpS1038 & 31);
  return _M0L6_2atmpS1036 | _M0L6_2atmpS1037;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__5208S6,
  struct _M0TPB6Logger _M0L10_2ax__5209S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS2324;
  int32_t _M0L6_2acntS2404;
  moonbit_string_t _M0L15_2a_2aarg__5210S8;
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__5208S6;
  _M0L8_2afieldS2324 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS2404 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS2404 > 1) {
    int32_t _M0L11_2anew__cntS2405 = _M0L6_2acntS2404 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS2405;
    moonbit_incref(_M0L8_2afieldS2324);
  } else if (_M0L6_2acntS2404 == 1) {
    #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__5210S8 = _M0L8_2afieldS2324;
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_89.data);
  if (_M0L10_2ax__5209S9.$1) {
    moonbit_incref(_M0L10_2ax__5209S9.$1);
  }
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__5209S9, _M0L15_2a_2aarg__5210S8);
  #line 37 "/Users/user/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__5209S9.$0->$method_0(_M0L10_2ax__5209S9.$1, (moonbit_string_t)moonbit_string_literal_90.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS979) {
  switch (Moonbit_object_tag(_M0L4_2aeS979)) {
    case 1: {
      moonbit_decref(_M0L4_2aeS979);
      return (moonbit_string_t)moonbit_string_literal_91.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS979);
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS979);
      return (moonbit_string_t)moonbit_string_literal_92.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS979);
      return (moonbit_string_t)moonbit_string_literal_93.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS979);
      return (moonbit_string_t)moonbit_string_literal_94.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1008
) {
  moonbit_string_t _M0L7_2aselfS1007 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1008;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS1007);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1006,
  struct _M0TPB6Logger _M0L8_2aparamS1005
) {
  moonbit_string_t _M0L7_2aselfS1004 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1006;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS1004, _M0L8_2aparamS1005);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1003,
  int32_t _M0L8_2aparamS1002
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1001 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1003;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1001, _M0L8_2aparamS1002);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1000,
  struct _M0TPC16string10StringView _M0L8_2aparamS999
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS998 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1000;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS998, _M0L8_2aparamS999);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS997,
  moonbit_string_t _M0L8_2aparamS994,
  int32_t _M0L8_2aparamS995,
  int32_t _M0L8_2aparamS996
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS993 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS997;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS993, _M0L8_2aparamS994, _M0L8_2aparamS995, _M0L8_2aparamS996);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS992,
  moonbit_string_t _M0L8_2aparamS991
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS990 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS992;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS990, _M0L8_2aparamS991);
  return 0;
}

moonbit_string_t _M0IPC13int3IntPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS988
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS989 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS988;
  int32_t _M0L7_2aselfS987 = _M0L14_2aboxed__selfS989->$0;
  moonbit_decref(_M0L14_2aboxed__selfS989);
  return _M0IPC13int3IntPB4Show10to__string(_M0L7_2aselfS987);
}

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS985,
  struct _M0TPB6Logger _M0L8_2aparamS984
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS986 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS985;
  int32_t _M0L7_2aselfS983 = _M0L14_2aboxed__selfS986->$0;
  moonbit_decref(_M0L14_2aboxed__selfS986);
  _M0IPC13int3IntPB4Show6output(_M0L7_2aselfS983, _M0L8_2aparamS984);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1035 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1034;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1024;
  moonbit_string_t* _M0L6_2atmpS1033;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1032;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1025;
  moonbit_string_t* _M0L6_2atmpS1031;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1030;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1026;
  moonbit_string_t* _M0L6_2atmpS1029;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1028;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1027;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS905;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1023;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1022;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1021;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1016;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS906;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1020;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1019;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1018;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1017;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS904;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1015;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1014;
  _M0L6_2atmpS1035[0] = (moonbit_string_t)moonbit_string_literal_95.data;
  moonbit_incref(_M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1034
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1034)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1034->$0
  = _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1034->$1 = _M0L6_2atmpS1035;
  _M0L8_2atupleS1024
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1024)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1024->$0 = 0;
  _M0L8_2atupleS1024->$1 = _M0L8_2atupleS1034;
  _M0L6_2atmpS1033 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1033[0] = (moonbit_string_t)moonbit_string_literal_96.data;
  moonbit_incref(_M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1032
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1032)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1032->$0
  = _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1032->$1 = _M0L6_2atmpS1033;
  _M0L8_2atupleS1025
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1025)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1025->$0 = 1;
  _M0L8_2atupleS1025->$1 = _M0L8_2atupleS1032;
  _M0L6_2atmpS1031 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1031[0] = (moonbit_string_t)moonbit_string_literal_97.data;
  moonbit_incref(_M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1030
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1030)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1030->$0
  = _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1030->$1 = _M0L6_2atmpS1031;
  _M0L8_2atupleS1026
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1026)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1026->$0 = 2;
  _M0L8_2atupleS1026->$1 = _M0L8_2atupleS1030;
  _M0L6_2atmpS1029 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1029[0] = (moonbit_string_t)moonbit_string_literal_98.data;
  moonbit_incref(_M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1028
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1028)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1028->$0
  = _M0FP36mulpjs4mulp27log__events__blackbox__test49____test__6576656e74735f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1028->$1 = _M0L6_2atmpS1029;
  _M0L8_2atupleS1027
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1027)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1027->$0 = 3;
  _M0L8_2atupleS1027->$1 = _M0L8_2atupleS1028;
  _M0L7_2abindS905
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS905[0] = _M0L8_2atupleS1024;
  _M0L7_2abindS905[1] = _M0L8_2atupleS1025;
  _M0L7_2abindS905[2] = _M0L8_2atupleS1026;
  _M0L7_2abindS905[3] = _M0L8_2atupleS1027;
  _M0L6_2atmpS1023 = _M0L7_2abindS905;
  _M0L6_2atmpS1022
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 4, _M0L6_2atmpS1023
  };
  #line 398 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1021
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1022);
  _M0L8_2atupleS1016
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1016)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1016->$0 = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L8_2atupleS1016->$1 = _M0L6_2atmpS1021;
  _M0L7_2abindS906
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1020 = _M0L7_2abindS906;
  _M0L6_2atmpS1019
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1020
  };
  #line 404 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1018
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1019);
  _M0L8_2atupleS1017
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1017)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1017->$0 = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L8_2atupleS1017->$1 = _M0L6_2atmpS1018;
  _M0L7_2abindS904
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS904[0] = _M0L8_2atupleS1016;
  _M0L7_2abindS904[1] = _M0L8_2atupleS1017;
  _M0L6_2atmpS1015 = _M0L7_2abindS904;
  _M0L6_2atmpS1014
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1015
  };
  #line 397 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0FP36mulpjs4mulp27log__events__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1014);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1013;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS973;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS974;
  int32_t _M0L7_2abindS975;
  int32_t _M0L2__S976;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1013
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS973
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS973)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS973->$0 = _M0L6_2atmpS1013;
  _M0L12async__testsS973->$1 = 0;
  #line 443 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS974
  = _M0FP36mulpjs4mulp27log__events__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS975 = _M0L7_2abindS974->$1;
  _M0L2__S976 = 0;
  while (1) {
    if (_M0L2__S976 < _M0L7_2abindS975) {
      struct _M0TUsiE** _M0L3bufS1012 = _M0L7_2abindS974->$0;
      struct _M0TUsiE* _M0L3argS977 =
        (struct _M0TUsiE*)_M0L3bufS1012[_M0L2__S976];
      moonbit_string_t _M0L6_2atmpS1009 = _M0L3argS977->$0;
      int32_t _M0L6_2atmpS1010 = _M0L3argS977->$1;
      int32_t _M0L6_2atmpS1011;
      moonbit_incref(_M0L6_2atmpS1009);
      moonbit_incref(_M0L12async__testsS973);
      #line 444 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
      _M0FP36mulpjs4mulp27log__events__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS973, _M0L6_2atmpS1009, _M0L6_2atmpS1010);
      _M0L6_2atmpS1011 = _M0L2__S976 + 1;
      _M0L2__S976 = _M0L6_2atmpS1011;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS974);
    }
    break;
  }
  #line 446 "/Users/user/workspace/github/gulp/mulp/log_events/__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP36mulpjs4mulp27log__events__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP36mulpjs4mulp27log__events__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS973);
  return 0;
}