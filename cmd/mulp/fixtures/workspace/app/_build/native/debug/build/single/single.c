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
struct _M0TPB5ArrayGsE;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  uint16_t* $0;
  
};

moonbit_string_t _M0FP34moon4test6single14selected__task(
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGsE* _M0FPC13env4args();

struct _M0TPB5ArrayGsE* _M0FPC13env24get__cli__args__internal();

moonbit_string_t _M0FPC13env28utf8__bytes__to__mbt__string(moonbit_bytes_t);

#define _M0FPC13env19get__cli__args__ffi moonbit_get_cli_args

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

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

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
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

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    114, 111, 111, 116, 10, 32, 32, 97, 112, 112, 10, 32, 32, 98, 117, 
    105, 108, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_2 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    45, 45, 119, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 112, 112, 10, 98, 117, 105, 108, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    45, 45, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    109, 117, 108, 112, 32, 102, 105, 120, 116, 117, 114, 101, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    45, 45, 116, 114, 101, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    97, 112, 112, 0
  };

moonbit_string_t _M0FP34moon4test6single14selected__task(
  struct _M0TPB5ArrayGsE* _M0L4argsS74
) {
  int32_t _M0L6_2atmpS226;
  int32_t _if__result_254;
  #line 7 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
  moonbit_incref(_M0L4argsS74);
  #line 8 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
  _M0L6_2atmpS226 = _M0MPC15array5Array6lengthGsE(_M0L4argsS74);
  if (_M0L6_2atmpS226 >= 3) {
    moonbit_string_t _M0L6_2atmpS225;
    moonbit_incref(_M0L4argsS74);
    #line 8 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _M0L6_2atmpS225 = _M0MPC15array5Array2atGsE(_M0L4argsS74, 1);
    #line 8 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _if__result_254
    = moonbit_val_array_equal(_M0L6_2atmpS225, (moonbit_string_t)moonbit_string_literal_0.data);
    moonbit_decref(_M0L6_2atmpS225);
  } else {
    _if__result_254 = 0;
  }
  if (_if__result_254) {
    #line 9 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    return _M0MPC15array5Array2atGsE(_M0L4argsS74, 2);
  } else {
    int32_t _M0L6_2atmpS227;
    moonbit_incref(_M0L4argsS74);
    #line 10 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _M0L6_2atmpS227 = _M0MPC15array5Array6lengthGsE(_M0L4argsS74);
    if (_M0L6_2atmpS227 >= 2) {
      #line 11 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      return _M0MPC15array5Array2atGsE(_M0L4argsS74, 1);
    } else {
      moonbit_decref(_M0L4argsS74);
      return (moonbit_string_t)moonbit_string_literal_1.data;
    }
  }
}

struct _M0TPB5ArrayGsE* _M0FPC13env4args() {
  #line 17 "/Users/user/.moon/lib/core/env/env.mbt"
  #line 18 "/Users/user/.moon/lib/core/env/env.mbt"
  return _M0FPC13env24get__cli__args__internal();
}

struct _M0TPB5ArrayGsE* _M0FPC13env24get__cli__args__internal() {
  moonbit_string_t* _M0L6_2atmpS224;
  struct _M0TPB5ArrayGsE* _M0L6_2aaccS68;
  moonbit_bytes_t* _M0L7_2abindS69;
  int32_t _M0L7_2abindS70;
  int32_t _M0L2__S71;
  #line 19 "/Users/user/.moon/lib/core/env/env_native.mbt"
  _M0L6_2atmpS224 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2aaccS68
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2aaccS68)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2aaccS68->$0 = _M0L6_2atmpS224;
  _M0L6_2aaccS68->$1 = 0;
  #line 21 "/Users/user/.moon/lib/core/env/env_native.mbt"
  _M0L7_2abindS69 = _M0FPC13env19get__cli__args__ffi();
  _M0L7_2abindS70 = Moonbit_array_length(_M0L7_2abindS69);
  _M0L2__S71 = 0;
  while (1) {
    if (_M0L2__S71 < _M0L7_2abindS70) {
      moonbit_bytes_t _M0L1tS72 =
        (moonbit_bytes_t)_M0L7_2abindS69[_M0L2__S71];
      moonbit_string_t _M0L6_2atmpS222;
      int32_t _M0L6_2atmpS223;
      moonbit_incref(_M0L1tS72);
      #line 21 "/Users/user/.moon/lib/core/env/env_native.mbt"
      _M0L6_2atmpS222 = _M0FPC13env28utf8__bytes__to__mbt__string(_M0L1tS72);
      moonbit_incref(_M0L6_2aaccS68);
      #line 20 "/Users/user/.moon/lib/core/env/env_native.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6_2aaccS68, _M0L6_2atmpS222);
      _M0L6_2atmpS223 = _M0L2__S71 + 1;
      _M0L2__S71 = _M0L6_2atmpS223;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS69);
      return _M0L6_2aaccS68;
    }
    break;
  }
}

moonbit_string_t _M0FPC13env28utf8__bytes__to__mbt__string(
  moonbit_bytes_t _M0L5bytesS64
) {
  struct _M0TPB13StringBuilder* _M0L3resS62;
  int32_t _M0L3lenS63;
  int32_t _M0Lm1iS65;
  #line 26 "/Users/user/.moon/lib/core/env/env_native.mbt"
  #line 27 "/Users/user/.moon/lib/core/env/env_native.mbt"
  _M0L3resS62 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS63 = Moonbit_array_length(_M0L5bytesS64);
  _M0Lm1iS65 = 0;
  while (1) {
    int32_t _M0L6_2atmpS145 = _M0Lm1iS65;
    if (_M0L6_2atmpS145 < _M0L3lenS63) {
      int32_t _M0L6_2atmpS221 = _M0Lm1iS65;
      int32_t _M0L6_2atmpS220;
      int32_t _M0Lm1cS66;
      int32_t _M0L6_2atmpS146;
      if (
        _M0L6_2atmpS221 < 0
        || _M0L6_2atmpS221 >= Moonbit_array_length(_M0L5bytesS64)
      ) {
        #line 31 "/Users/user/.moon/lib/core/env/env_native.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS220 = _M0L5bytesS64[_M0L6_2atmpS221];
      _M0Lm1cS66 = (int32_t)_M0L6_2atmpS220;
      _M0L6_2atmpS146 = _M0Lm1cS66;
      if (_M0L6_2atmpS146 == 0) {
        moonbit_decref(_M0L5bytesS64);
        break;
      } else {
        int32_t _M0L6_2atmpS147 = _M0Lm1cS66;
        if (_M0L6_2atmpS147 < 128) {
          int32_t _M0L6_2atmpS149 = _M0Lm1cS66;
          int32_t _M0L6_2atmpS148 = _M0L6_2atmpS149;
          int32_t _M0L6_2atmpS150;
          moonbit_incref(_M0L3resS62);
          #line 36 "/Users/user/.moon/lib/core/env/env_native.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS62, _M0L6_2atmpS148);
          _M0L6_2atmpS150 = _M0Lm1iS65;
          _M0Lm1iS65 = _M0L6_2atmpS150 + 1;
        } else {
          int32_t _M0L6_2atmpS151 = _M0Lm1cS66;
          if (_M0L6_2atmpS151 < 224) {
            int32_t _M0L6_2atmpS153 = _M0Lm1iS65;
            int32_t _M0L6_2atmpS152 = _M0L6_2atmpS153 + 1;
            int32_t _M0L6_2atmpS161;
            int32_t _M0L6_2atmpS160;
            int32_t _M0L6_2atmpS154;
            int32_t _M0L6_2atmpS159;
            int32_t _M0L6_2atmpS158;
            int32_t _M0L6_2atmpS157;
            int32_t _M0L6_2atmpS156;
            int32_t _M0L6_2atmpS155;
            int32_t _M0L6_2atmpS163;
            int32_t _M0L6_2atmpS162;
            int32_t _M0L6_2atmpS164;
            if (_M0L6_2atmpS152 >= _M0L3lenS63) {
              moonbit_decref(_M0L5bytesS64);
              break;
            }
            _M0L6_2atmpS161 = _M0Lm1cS66;
            _M0L6_2atmpS160 = _M0L6_2atmpS161 & 31;
            _M0L6_2atmpS154 = _M0L6_2atmpS160 << 6;
            _M0L6_2atmpS159 = _M0Lm1iS65;
            _M0L6_2atmpS158 = _M0L6_2atmpS159 + 1;
            if (
              _M0L6_2atmpS158 < 0
              || _M0L6_2atmpS158 >= Moonbit_array_length(_M0L5bytesS64)
            ) {
              #line 42 "/Users/user/.moon/lib/core/env/env_native.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS157 = _M0L5bytesS64[_M0L6_2atmpS158];
            _M0L6_2atmpS156 = (int32_t)_M0L6_2atmpS157;
            _M0L6_2atmpS155 = _M0L6_2atmpS156 & 63;
            _M0Lm1cS66 = _M0L6_2atmpS154 | _M0L6_2atmpS155;
            _M0L6_2atmpS163 = _M0Lm1cS66;
            _M0L6_2atmpS162 = _M0L6_2atmpS163;
            moonbit_incref(_M0L3resS62);
            #line 43 "/Users/user/.moon/lib/core/env/env_native.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS62, _M0L6_2atmpS162);
            _M0L6_2atmpS164 = _M0Lm1iS65;
            _M0Lm1iS65 = _M0L6_2atmpS164 + 2;
          } else {
            int32_t _M0L6_2atmpS165 = _M0Lm1cS66;
            if (_M0L6_2atmpS165 < 240) {
              int32_t _M0L6_2atmpS167 = _M0Lm1iS65;
              int32_t _M0L6_2atmpS166 = _M0L6_2atmpS167 + 2;
              int32_t _M0L6_2atmpS182;
              int32_t _M0L6_2atmpS181;
              int32_t _M0L6_2atmpS174;
              int32_t _M0L6_2atmpS180;
              int32_t _M0L6_2atmpS179;
              int32_t _M0L6_2atmpS178;
              int32_t _M0L6_2atmpS177;
              int32_t _M0L6_2atmpS176;
              int32_t _M0L6_2atmpS175;
              int32_t _M0L6_2atmpS168;
              int32_t _M0L6_2atmpS173;
              int32_t _M0L6_2atmpS172;
              int32_t _M0L6_2atmpS171;
              int32_t _M0L6_2atmpS170;
              int32_t _M0L6_2atmpS169;
              int32_t _M0L6_2atmpS184;
              int32_t _M0L6_2atmpS183;
              int32_t _M0L6_2atmpS185;
              if (_M0L6_2atmpS166 >= _M0L3lenS63) {
                moonbit_decref(_M0L5bytesS64);
                break;
              }
              _M0L6_2atmpS182 = _M0Lm1cS66;
              _M0L6_2atmpS181 = _M0L6_2atmpS182 & 15;
              _M0L6_2atmpS174 = _M0L6_2atmpS181 << 12;
              _M0L6_2atmpS180 = _M0Lm1iS65;
              _M0L6_2atmpS179 = _M0L6_2atmpS180 + 1;
              if (
                _M0L6_2atmpS179 < 0
                || _M0L6_2atmpS179 >= Moonbit_array_length(_M0L5bytesS64)
              ) {
                #line 50 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS178 = _M0L5bytesS64[_M0L6_2atmpS179];
              _M0L6_2atmpS177 = (int32_t)_M0L6_2atmpS178;
              _M0L6_2atmpS176 = _M0L6_2atmpS177 & 63;
              _M0L6_2atmpS175 = _M0L6_2atmpS176 << 6;
              _M0L6_2atmpS168 = _M0L6_2atmpS174 | _M0L6_2atmpS175;
              _M0L6_2atmpS173 = _M0Lm1iS65;
              _M0L6_2atmpS172 = _M0L6_2atmpS173 + 2;
              if (
                _M0L6_2atmpS172 < 0
                || _M0L6_2atmpS172 >= Moonbit_array_length(_M0L5bytesS64)
              ) {
                #line 51 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS171 = _M0L5bytesS64[_M0L6_2atmpS172];
              _M0L6_2atmpS170 = (int32_t)_M0L6_2atmpS171;
              _M0L6_2atmpS169 = _M0L6_2atmpS170 & 63;
              _M0Lm1cS66 = _M0L6_2atmpS168 | _M0L6_2atmpS169;
              _M0L6_2atmpS184 = _M0Lm1cS66;
              _M0L6_2atmpS183 = _M0L6_2atmpS184;
              moonbit_incref(_M0L3resS62);
              #line 52 "/Users/user/.moon/lib/core/env/env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS62, _M0L6_2atmpS183);
              _M0L6_2atmpS185 = _M0Lm1iS65;
              _M0Lm1iS65 = _M0L6_2atmpS185 + 3;
            } else {
              int32_t _M0L6_2atmpS187 = _M0Lm1iS65;
              int32_t _M0L6_2atmpS186 = _M0L6_2atmpS187 + 3;
              int32_t _M0L6_2atmpS209;
              int32_t _M0L6_2atmpS208;
              int32_t _M0L6_2atmpS201;
              int32_t _M0L6_2atmpS207;
              int32_t _M0L6_2atmpS206;
              int32_t _M0L6_2atmpS205;
              int32_t _M0L6_2atmpS204;
              int32_t _M0L6_2atmpS203;
              int32_t _M0L6_2atmpS202;
              int32_t _M0L6_2atmpS194;
              int32_t _M0L6_2atmpS200;
              int32_t _M0L6_2atmpS199;
              int32_t _M0L6_2atmpS198;
              int32_t _M0L6_2atmpS197;
              int32_t _M0L6_2atmpS196;
              int32_t _M0L6_2atmpS195;
              int32_t _M0L6_2atmpS188;
              int32_t _M0L6_2atmpS193;
              int32_t _M0L6_2atmpS192;
              int32_t _M0L6_2atmpS191;
              int32_t _M0L6_2atmpS190;
              int32_t _M0L6_2atmpS189;
              int32_t _M0L6_2atmpS210;
              int32_t _M0L6_2atmpS214;
              int32_t _M0L6_2atmpS213;
              int32_t _M0L6_2atmpS212;
              int32_t _M0L6_2atmpS211;
              int32_t _M0L6_2atmpS218;
              int32_t _M0L6_2atmpS217;
              int32_t _M0L6_2atmpS216;
              int32_t _M0L6_2atmpS215;
              int32_t _M0L6_2atmpS219;
              if (_M0L6_2atmpS186 >= _M0L3lenS63) {
                moonbit_decref(_M0L5bytesS64);
                break;
              }
              _M0L6_2atmpS209 = _M0Lm1cS66;
              _M0L6_2atmpS208 = _M0L6_2atmpS209 & 7;
              _M0L6_2atmpS201 = _M0L6_2atmpS208 << 18;
              _M0L6_2atmpS207 = _M0Lm1iS65;
              _M0L6_2atmpS206 = _M0L6_2atmpS207 + 1;
              if (
                _M0L6_2atmpS206 < 0
                || _M0L6_2atmpS206 >= Moonbit_array_length(_M0L5bytesS64)
              ) {
                #line 59 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS205 = _M0L5bytesS64[_M0L6_2atmpS206];
              _M0L6_2atmpS204 = (int32_t)_M0L6_2atmpS205;
              _M0L6_2atmpS203 = _M0L6_2atmpS204 & 63;
              _M0L6_2atmpS202 = _M0L6_2atmpS203 << 12;
              _M0L6_2atmpS194 = _M0L6_2atmpS201 | _M0L6_2atmpS202;
              _M0L6_2atmpS200 = _M0Lm1iS65;
              _M0L6_2atmpS199 = _M0L6_2atmpS200 + 2;
              if (
                _M0L6_2atmpS199 < 0
                || _M0L6_2atmpS199 >= Moonbit_array_length(_M0L5bytesS64)
              ) {
                #line 60 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS198 = _M0L5bytesS64[_M0L6_2atmpS199];
              _M0L6_2atmpS197 = (int32_t)_M0L6_2atmpS198;
              _M0L6_2atmpS196 = _M0L6_2atmpS197 & 63;
              _M0L6_2atmpS195 = _M0L6_2atmpS196 << 6;
              _M0L6_2atmpS188 = _M0L6_2atmpS194 | _M0L6_2atmpS195;
              _M0L6_2atmpS193 = _M0Lm1iS65;
              _M0L6_2atmpS192 = _M0L6_2atmpS193 + 3;
              if (
                _M0L6_2atmpS192 < 0
                || _M0L6_2atmpS192 >= Moonbit_array_length(_M0L5bytesS64)
              ) {
                #line 61 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS191 = _M0L5bytesS64[_M0L6_2atmpS192];
              _M0L6_2atmpS190 = (int32_t)_M0L6_2atmpS191;
              _M0L6_2atmpS189 = _M0L6_2atmpS190 & 63;
              _M0Lm1cS66 = _M0L6_2atmpS188 | _M0L6_2atmpS189;
              _M0L6_2atmpS210 = _M0Lm1cS66;
              _M0Lm1cS66 = _M0L6_2atmpS210 - 65536;
              _M0L6_2atmpS214 = _M0Lm1cS66;
              _M0L6_2atmpS213 = _M0L6_2atmpS214 >> 10;
              _M0L6_2atmpS212 = _M0L6_2atmpS213 + 55296;
              _M0L6_2atmpS211 = _M0L6_2atmpS212;
              moonbit_incref(_M0L3resS62);
              #line 63 "/Users/user/.moon/lib/core/env/env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS62, _M0L6_2atmpS211);
              _M0L6_2atmpS218 = _M0Lm1cS66;
              _M0L6_2atmpS217 = _M0L6_2atmpS218 & 1023;
              _M0L6_2atmpS216 = _M0L6_2atmpS217 + 56320;
              _M0L6_2atmpS215 = _M0L6_2atmpS216;
              moonbit_incref(_M0L3resS62);
              #line 64 "/Users/user/.moon/lib/core/env/env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS62, _M0L6_2atmpS215);
              _M0L6_2atmpS219 = _M0Lm1iS65;
              _M0Lm1iS65 = _M0L6_2atmpS219 + 4;
            }
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L5bytesS64);
    }
    break;
  }
  #line 68 "/Users/user/.moon/lib/core/env/env_native.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS62);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS60,
  int32_t _M0L5indexS61
) {
  int32_t _M0L3lenS59;
  int32_t _if__result_257;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS59 = _M0L4selfS60->$1;
  if (_M0L5indexS61 >= 0) {
    _if__result_257 = _M0L5indexS61 < _M0L3lenS59;
  } else {
    _if__result_257 = 0;
  }
  if (_if__result_257) {
    moonbit_string_t* _M0L6_2atmpS144;
    moonbit_string_t _M0L6_2atmpS229;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS144 = _M0MPC15array5Array6bufferGsE(_M0L4selfS60);
    if (
      _M0L5indexS61 < 0
      || _M0L5indexS61 >= Moonbit_array_length(_M0L6_2atmpS144)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS229 = (moonbit_string_t)_M0L6_2atmpS144[_M0L5indexS61];
    moonbit_incref(_M0L6_2atmpS229);
    moonbit_decref(_M0L6_2atmpS144);
    return _M0L6_2atmpS229;
  } else {
    moonbit_decref(_M0L4selfS60);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS58) {
  moonbit_string_t _M0L6_2atmpS143;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS143 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS58);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS143);
  moonbit_decref(_M0L6_2atmpS143);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS55,
  moonbit_string_t _M0L5valueS57
) {
  int32_t _M0L3lenS138;
  moonbit_string_t* _M0L6_2atmpS140;
  int32_t _M0L6_2atmpS139;
  int32_t _M0L6lengthS56;
  moonbit_string_t* _M0L3bufS141;
  moonbit_string_t _M0L6_2aoldS230;
  int32_t _M0L6_2atmpS142;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS138 = _M0L4selfS55->$1;
  moonbit_incref(_M0L4selfS55);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS140 = _M0MPC15array5Array6bufferGsE(_M0L4selfS55);
  _M0L6_2atmpS139 = Moonbit_array_length(_M0L6_2atmpS140);
  moonbit_decref(_M0L6_2atmpS140);
  if (_M0L3lenS138 == _M0L6_2atmpS139) {
    moonbit_incref(_M0L4selfS55);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS55);
  }
  _M0L6lengthS56 = _M0L4selfS55->$1;
  _M0L3bufS141 = _M0L4selfS55->$0;
  _M0L6_2aoldS230 = (moonbit_string_t)_M0L3bufS141[_M0L6lengthS56];
  moonbit_decref(_M0L6_2aoldS230);
  _M0L3bufS141[_M0L6lengthS56] = _M0L5valueS57;
  _M0L6_2atmpS142 = _M0L6lengthS56 + 1;
  _M0L4selfS55->$1 = _M0L6_2atmpS142;
  moonbit_decref(_M0L4selfS55);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS53) {
  int32_t _M0L8old__capS52;
  int32_t _M0L8new__capS54;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS52 = _M0L4selfS53->$1;
  if (_M0L8old__capS52 == 0) {
    _M0L8new__capS54 = 8;
  } else {
    _M0L8new__capS54 = _M0L8old__capS52 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS53, _M0L8new__capS54);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS49,
  int32_t _M0L13new__capacityS47
) {
  moonbit_string_t* _M0L8new__bufS46;
  moonbit_string_t* _M0L8old__bufS48;
  int32_t _M0L8old__capS50;
  int32_t _M0L9copy__lenS51;
  moonbit_string_t* _M0L6_2aoldS232;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS46
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS47, (moonbit_string_t)moonbit_string_literal_2.data);
  _M0L8old__bufS48 = _M0L4selfS49->$0;
  _M0L8old__capS50 = Moonbit_array_length(_M0L8old__bufS48);
  if (_M0L8old__capS50 < _M0L13new__capacityS47) {
    _M0L9copy__lenS51 = _M0L8old__capS50;
  } else {
    _M0L9copy__lenS51 = _M0L13new__capacityS47;
  }
  moonbit_incref(_M0L8old__bufS48);
  moonbit_incref(_M0L8new__bufS46);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS46, 0, _M0L8old__bufS48, 0, _M0L9copy__lenS51);
  _M0L6_2aoldS232 = _M0L4selfS49->$0;
  moonbit_decref(_M0L6_2aoldS232);
  _M0L4selfS49->$0 = _M0L8new__bufS46;
  moonbit_decref(_M0L4selfS49);
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS45) {
  int32_t _result_258;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_258 = _M0L4selfS45->$1;
  moonbit_decref(_M0L4selfS45);
  return _result_258;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS44
) {
  moonbit_string_t* _M0L8_2afieldS234;
  int32_t _M0L6_2acntS248;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS234 = _M0L4selfS44->$0;
  _M0L6_2acntS248 = Moonbit_object_header(_M0L4selfS44)->rc;
  if (_M0L6_2acntS248 > 1) {
    int32_t _M0L11_2anew__cntS249 = _M0L6_2acntS248 - 1;
    Moonbit_object_header(_M0L4selfS44)->rc = _M0L11_2anew__cntS249;
    moonbit_incref(_M0L8_2afieldS234);
  } else if (_M0L6_2acntS248 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS44);
  }
  return _M0L8_2afieldS234;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS43
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS43;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS41,
  int32_t _M0L2chS40
) {
  uint32_t _M0L4codeS39;
  #line 90 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 91 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS39 = _M0MPC14char4Char8to__uint(_M0L2chS40);
  if (_M0L4codeS39 <= 65535u) {
    int32_t _M0L3lenS117 = _M0L4selfS41->$1;
    int32_t _M0L6_2atmpS116 = _M0L3lenS117 + 1;
    uint16_t* _M0L4dataS118;
    int32_t _M0L3lenS119;
    int32_t _M0L6_2atmpS120;
    int32_t _M0L3lenS122;
    int32_t _M0L6_2atmpS121;
    moonbit_incref(_M0L4selfS41);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS41, _M0L6_2atmpS116);
    _M0L4dataS118 = _M0L4selfS41->$0;
    _M0L3lenS119 = _M0L4selfS41->$1;
    moonbit_incref(_M0L4dataS118);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS120 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS39);
    if (
      _M0L3lenS119 < 0 || _M0L3lenS119 >= Moonbit_array_length(_M0L4dataS118)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS118[_M0L3lenS119] = _M0L6_2atmpS120;
    moonbit_decref(_M0L4dataS118);
    _M0L3lenS122 = _M0L4selfS41->$1;
    _M0L6_2atmpS121 = _M0L3lenS122 + 1;
    _M0L4selfS41->$1 = _M0L6_2atmpS121;
    moonbit_decref(_M0L4selfS41);
  } else if (_M0L4codeS39 <= 1114111u) {
    int32_t _M0L3lenS124 = _M0L4selfS41->$1;
    int32_t _M0L6_2atmpS123 = _M0L3lenS124 + 2;
    uint32_t _M0L4codeS42;
    uint16_t* _M0L4dataS125;
    int32_t _M0L3lenS126;
    uint32_t _M0L6_2atmpS129;
    uint32_t _M0L6_2atmpS128;
    int32_t _M0L6_2atmpS127;
    uint16_t* _M0L4dataS130;
    int32_t _M0L3lenS135;
    int32_t _M0L6_2atmpS131;
    uint32_t _M0L6_2atmpS134;
    uint32_t _M0L6_2atmpS133;
    int32_t _M0L6_2atmpS132;
    int32_t _M0L3lenS137;
    int32_t _M0L6_2atmpS136;
    moonbit_incref(_M0L4selfS41);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS41, _M0L6_2atmpS123);
    _M0L4codeS42 = _M0L4codeS39 - 65536u;
    _M0L4dataS125 = _M0L4selfS41->$0;
    _M0L3lenS126 = _M0L4selfS41->$1;
    _M0L6_2atmpS129 = _M0L4codeS42 >> 10;
    _M0L6_2atmpS128 = 55296u + _M0L6_2atmpS129;
    moonbit_incref(_M0L4dataS125);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS127 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS128);
    if (
      _M0L3lenS126 < 0 || _M0L3lenS126 >= Moonbit_array_length(_M0L4dataS125)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS125[_M0L3lenS126] = _M0L6_2atmpS127;
    moonbit_decref(_M0L4dataS125);
    _M0L4dataS130 = _M0L4selfS41->$0;
    _M0L3lenS135 = _M0L4selfS41->$1;
    _M0L6_2atmpS131 = _M0L3lenS135 + 1;
    _M0L6_2atmpS134 = _M0L4codeS42 & 1023u;
    _M0L6_2atmpS133 = 56320u + _M0L6_2atmpS134;
    moonbit_incref(_M0L4dataS130);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS132 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS133);
    if (
      _M0L6_2atmpS131 < 0
      || _M0L6_2atmpS131 >= Moonbit_array_length(_M0L4dataS130)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS130[_M0L6_2atmpS131] = _M0L6_2atmpS132;
    moonbit_decref(_M0L4dataS130);
    _M0L3lenS137 = _M0L4selfS41->$1;
    _M0L6_2atmpS136 = _M0L3lenS137 + 2;
    _M0L4selfS41->$1 = _M0L6_2atmpS136;
    moonbit_decref(_M0L4selfS41);
  } else {
    moonbit_decref(_M0L4selfS41);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_3.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS33,
  int32_t _M0L8requiredS34
) {
  uint16_t* _M0L4dataS115;
  int32_t _M0L12current__lenS32;
  int32_t _M0L13enough__spaceS35;
  int32_t _M0L13enough__spaceS36;
  int32_t _M0L6_2atmpS113;
  uint16_t* _M0L9new__dataS38;
  uint16_t* _M0L4dataS111;
  int32_t _M0L3lenS112;
  uint16_t* _M0L6_2aoldS238;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS115 = _M0L4selfS33->$0;
  _M0L12current__lenS32 = Moonbit_array_length(_M0L4dataS115);
  if (_M0L8requiredS34 <= _M0L12current__lenS32) {
    moonbit_decref(_M0L4selfS33);
    return 0;
  }
  _M0L13enough__spaceS36 = _M0L12current__lenS32;
  while (1) {
    if (_M0L13enough__spaceS36 < _M0L8requiredS34) {
      int32_t _M0L6_2atmpS114 = _M0L13enough__spaceS36 * 2;
      _M0L13enough__spaceS36 = _M0L6_2atmpS114;
      continue;
    } else {
      _M0L13enough__spaceS35 = _M0L13enough__spaceS36;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS113 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS38
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS35, _M0L6_2atmpS113);
  _M0L4dataS111 = _M0L4selfS33->$0;
  _M0L3lenS112 = _M0L4selfS33->$1;
  moonbit_incref(_M0L4dataS111);
  moonbit_incref(_M0L9new__dataS38);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS38, 0, _M0L4dataS111, 0, _M0L3lenS112);
  _M0L6_2aoldS238 = _M0L4selfS33->$0;
  moonbit_decref(_M0L6_2aoldS238);
  _M0L4selfS33->$0 = _M0L9new__dataS38;
  moonbit_decref(_M0L4selfS33);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS31) {
  int32_t _M0L6_2atmpS110;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS110 = *(int32_t*)&_M0L4selfS31;
  return (uint16_t)_M0L6_2atmpS110;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS30) {
  int32_t _M0L6_2atmpS109;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS109 = _M0L4selfS30;
  return *(uint32_t*)&_M0L6_2atmpS109;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS28
) {
  int32_t _M0L3lenS101;
  uint16_t* _M0L4dataS103;
  int32_t _M0L6_2atmpS102;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS101 = _M0L4selfS28->$1;
  _M0L4dataS103 = _M0L4selfS28->$0;
  _M0L6_2atmpS102 = Moonbit_array_length(_M0L4dataS103);
  if (_M0L3lenS101 == _M0L6_2atmpS102) {
    uint16_t* _M0L8_2afieldS241 = _M0L4selfS28->$0;
    int32_t _M0L6_2acntS250 = Moonbit_object_header(_M0L4selfS28)->rc;
    uint16_t* _M0L4dataS104;
    if (_M0L6_2acntS250 > 1) {
      int32_t _M0L11_2anew__cntS251 = _M0L6_2acntS250 - 1;
      Moonbit_object_header(_M0L4selfS28)->rc = _M0L11_2anew__cntS251;
      moonbit_incref(_M0L8_2afieldS241);
    } else if (_M0L6_2acntS250 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS28);
    }
    _M0L4dataS104 = _M0L8_2afieldS241;
    return _M0L4dataS104;
  } else {
    int32_t _M0L3lenS107 = _M0L4selfS28->$1;
    int32_t _M0L6_2atmpS108;
    uint16_t* _M0L4dataS29;
    uint16_t* _M0L4dataS105;
    int32_t _M0L3lenS106;
    int32_t _M0L6_2acntS252;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS108 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS29
    = (uint16_t*)moonbit_make_string(_M0L3lenS107, _M0L6_2atmpS108);
    _M0L4dataS105 = _M0L4selfS28->$0;
    _M0L3lenS106 = _M0L4selfS28->$1;
    _M0L6_2acntS252 = Moonbit_object_header(_M0L4selfS28)->rc;
    if (_M0L6_2acntS252 > 1) {
      int32_t _M0L11_2anew__cntS253 = _M0L6_2acntS252 - 1;
      Moonbit_object_header(_M0L4selfS28)->rc = _M0L11_2anew__cntS253;
      moonbit_incref(_M0L4dataS105);
    } else if (_M0L6_2acntS252 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS28);
    }
    moonbit_incref(_M0L4dataS29);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS29, 0, _M0L4dataS105, 0, _M0L3lenS106);
    return _M0L4dataS29;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 153 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS26
) {
  int32_t _M0L7initialS25;
  uint16_t* _M0L4dataS27;
  struct _M0TPB13StringBuilder* _block_260;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS26 < 1) {
    _M0L7initialS25 = 1;
  } else {
    int32_t _M0L6_2atmpS100 = _M0L10size__hintS26 + 1;
    _M0L7initialS25 = _M0L6_2atmpS100 / 2;
  }
  _M0L4dataS27 = (uint16_t*)moonbit_make_string(_M0L7initialS25, 0);
  _block_260
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_260)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_260->$0 = _M0L4dataS27;
  _block_260->$1 = 0;
  return _block_260;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS20,
  int32_t _M0L11dst__offsetS21,
  moonbit_string_t* _M0L3srcS22,
  int32_t _M0L11src__offsetS23,
  int32_t _M0L3lenS24
) {
  #line 104 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/Users/user/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS20, _M0L11dst__offsetS21, _M0L3srcS22, _M0L11src__offsetS23, _M0L3lenS24);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGkE(
  uint16_t* _M0L3dstS2,
  int32_t _M0L11dst__offsetS4,
  uint16_t* _M0L3srcS3,
  int32_t _M0L11src__offsetS5,
  int32_t _M0L3lenS7
) {
  int32_t _if__result_261;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS2 == _M0L3srcS3) {
    _if__result_261 = _M0L11dst__offsetS4 < _M0L11src__offsetS5;
  } else {
    _if__result_261 = 0;
  }
  if (_if__result_261) {
    int32_t _M0L1iS6 = 0;
    while (1) {
      if (_M0L1iS6 < _M0L3lenS7) {
        int32_t _M0L6_2atmpS82 = _M0L11dst__offsetS4 + _M0L1iS6;
        int32_t _M0L6_2atmpS84 = _M0L11src__offsetS5 + _M0L1iS6;
        int32_t _M0L6_2atmpS83;
        int32_t _M0L6_2atmpS85;
        if (
          _M0L6_2atmpS84 < 0
          || _M0L6_2atmpS84 >= Moonbit_array_length(_M0L3srcS3)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS83 = (int32_t)_M0L3srcS3[_M0L6_2atmpS84];
        if (
          _M0L6_2atmpS82 < 0
          || _M0L6_2atmpS82 >= Moonbit_array_length(_M0L3dstS2)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS2[_M0L6_2atmpS82] = _M0L6_2atmpS83;
        _M0L6_2atmpS85 = _M0L1iS6 + 1;
        _M0L1iS6 = _M0L6_2atmpS85;
        continue;
      } else {
        moonbit_decref(_M0L3srcS3);
        moonbit_decref(_M0L3dstS2);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS90 = _M0L3lenS7 - 1;
    int32_t _M0L1iS9 = _M0L6_2atmpS90;
    while (1) {
      if (_M0L1iS9 >= 0) {
        int32_t _M0L6_2atmpS86 = _M0L11dst__offsetS4 + _M0L1iS9;
        int32_t _M0L6_2atmpS88 = _M0L11src__offsetS5 + _M0L1iS9;
        int32_t _M0L6_2atmpS87;
        int32_t _M0L6_2atmpS89;
        if (
          _M0L6_2atmpS88 < 0
          || _M0L6_2atmpS88 >= Moonbit_array_length(_M0L3srcS3)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS87 = (int32_t)_M0L3srcS3[_M0L6_2atmpS88];
        if (
          _M0L6_2atmpS86 < 0
          || _M0L6_2atmpS86 >= Moonbit_array_length(_M0L3dstS2)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS2[_M0L6_2atmpS86] = _M0L6_2atmpS87;
        _M0L6_2atmpS89 = _M0L1iS9 - 1;
        _M0L1iS9 = _M0L6_2atmpS89;
        continue;
      } else {
        moonbit_decref(_M0L3srcS3);
        moonbit_decref(_M0L3dstS2);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS11,
  int32_t _M0L11dst__offsetS13,
  moonbit_string_t* _M0L3srcS12,
  int32_t _M0L11src__offsetS14,
  int32_t _M0L3lenS16
) {
  int32_t _if__result_264;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS11 == _M0L3srcS12) {
    _if__result_264 = _M0L11dst__offsetS13 < _M0L11src__offsetS14;
  } else {
    _if__result_264 = 0;
  }
  if (_if__result_264) {
    int32_t _M0L1iS15 = 0;
    while (1) {
      if (_M0L1iS15 < _M0L3lenS16) {
        int32_t _M0L6_2atmpS91 = _M0L11dst__offsetS13 + _M0L1iS15;
        int32_t _M0L6_2atmpS93 = _M0L11src__offsetS14 + _M0L1iS15;
        moonbit_string_t _M0L6_2atmpS92;
        moonbit_string_t _M0L6_2aoldS244;
        int32_t _M0L6_2atmpS94;
        if (
          _M0L6_2atmpS93 < 0
          || _M0L6_2atmpS93 >= Moonbit_array_length(_M0L3srcS12)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS92 = (moonbit_string_t)_M0L3srcS12[_M0L6_2atmpS93];
        if (
          _M0L6_2atmpS91 < 0
          || _M0L6_2atmpS91 >= Moonbit_array_length(_M0L3dstS11)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS244 = (moonbit_string_t)_M0L3dstS11[_M0L6_2atmpS91];
        moonbit_incref(_M0L6_2atmpS92);
        moonbit_decref(_M0L6_2aoldS244);
        _M0L3dstS11[_M0L6_2atmpS91] = _M0L6_2atmpS92;
        _M0L6_2atmpS94 = _M0L1iS15 + 1;
        _M0L1iS15 = _M0L6_2atmpS94;
        continue;
      } else {
        moonbit_decref(_M0L3srcS12);
        moonbit_decref(_M0L3dstS11);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS99 = _M0L3lenS16 - 1;
    int32_t _M0L1iS18 = _M0L6_2atmpS99;
    while (1) {
      if (_M0L1iS18 >= 0) {
        int32_t _M0L6_2atmpS95 = _M0L11dst__offsetS13 + _M0L1iS18;
        int32_t _M0L6_2atmpS97 = _M0L11src__offsetS14 + _M0L1iS18;
        moonbit_string_t _M0L6_2atmpS96;
        moonbit_string_t _M0L6_2aoldS246;
        int32_t _M0L6_2atmpS98;
        if (
          _M0L6_2atmpS97 < 0
          || _M0L6_2atmpS97 >= Moonbit_array_length(_M0L3srcS12)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS96 = (moonbit_string_t)_M0L3srcS12[_M0L6_2atmpS97];
        if (
          _M0L6_2atmpS95 < 0
          || _M0L6_2atmpS95 >= Moonbit_array_length(_M0L3dstS11)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS246 = (moonbit_string_t)_M0L3dstS11[_M0L6_2atmpS95];
        moonbit_incref(_M0L6_2atmpS96);
        moonbit_decref(_M0L6_2aoldS246);
        _M0L3dstS11[_M0L6_2atmpS95] = _M0L6_2atmpS96;
        _M0L6_2atmpS98 = _M0L1iS18 - 1;
        _M0L1iS18 = _M0L6_2atmpS98;
        continue;
      } else {
        moonbit_decref(_M0L3srcS12);
        moonbit_decref(_M0L3dstS11);
      }
      break;
    }
  }
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

void moonbit_init() {
  
}

int main(int argc, char** argv) {
  struct _M0TPB5ArrayGsE* _M0L4argsS75;
  int32_t _M0L6_2atmpS77;
  int32_t _if__result_267;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 19 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
  _M0L4argsS75 = _M0FPC13env4args();
  moonbit_incref(_M0L4argsS75);
  #line 20 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
  _M0L6_2atmpS77 = _M0MPC15array5Array6lengthGsE(_M0L4argsS75);
  if (_M0L6_2atmpS77 >= 2) {
    moonbit_string_t _M0L6_2atmpS76;
    moonbit_incref(_M0L4argsS75);
    #line 20 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _M0L6_2atmpS76 = _M0MPC15array5Array2atGsE(_M0L4argsS75, 1);
    #line 20 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _if__result_267
    = moonbit_val_array_equal(_M0L6_2atmpS76, (moonbit_string_t)moonbit_string_literal_4.data);
    moonbit_decref(_M0L6_2atmpS76);
  } else {
    _if__result_267 = 0;
  }
  if (_if__result_267) {
    moonbit_decref(_M0L4argsS75);
    #line 21 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_5.data);
  } else {
    int32_t _M0L6_2atmpS79;
    int32_t _if__result_268;
    moonbit_incref(_M0L4argsS75);
    #line 22 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
    _M0L6_2atmpS79 = _M0MPC15array5Array6lengthGsE(_M0L4argsS75);
    if (_M0L6_2atmpS79 >= 2) {
      moonbit_string_t _M0L6_2atmpS78;
      moonbit_incref(_M0L4argsS75);
      #line 22 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      _M0L6_2atmpS78 = _M0MPC15array5Array2atGsE(_M0L4argsS75, 1);
      #line 22 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      _if__result_268
      = moonbit_val_array_equal(_M0L6_2atmpS78, (moonbit_string_t)moonbit_string_literal_6.data);
      moonbit_decref(_M0L6_2atmpS78);
    } else {
      _if__result_268 = 0;
    }
    if (_if__result_268) {
      moonbit_decref(_M0L4argsS75);
      #line 23 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_7.data);
    } else {
      moonbit_string_t _M0L6_2atmpS81;
      moonbit_string_t _M0L6_2atmpS80;
      #line 25 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      _M0L6_2atmpS81 = _M0FP34moon4test6single14selected__task(_M0L4argsS75);
      #line 25 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      _M0L6_2atmpS80
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_8.data, _M0L6_2atmpS81);
      moonbit_decref(_M0L6_2atmpS81);
      #line 25 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/workspace/app/_build/mulp.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS80);
    }
  }
  return 0;
}