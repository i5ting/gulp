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
struct _M0TPC16string10StringView;

struct _M0TPB5ArrayGsE;

struct _M0TPB13StringBuilder;

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

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

int32_t _M0FP34moon4test6single13exit__process(int32_t);

#define _M0FP34moon4test6single9write__fd write

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView,
  int32_t
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

int32_t _M0MPC16uint166UInt1613is__surrogate(int32_t);

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView,
  int32_t
);

int32_t _M0MPC16string10StringView6length(struct _M0TPC16string10StringView);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t);

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

int32_t write(int32_t, moonbit_bytes_t, int32_t);

void exit(int32_t);

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_3 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    105, 110, 118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 
    111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    99, 108, 101, 97, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    45, 45, 119, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    98, 117, 105, 108, 100, 10, 99, 108, 101, 97, 110, 10, 119, 97, 116, 
    99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    45, 45, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    109, 117, 108, 112, 32, 109, 98, 116, 120, 32, 102, 105, 120, 116, 
    117, 114, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    114, 111, 111, 116, 10, 32, 32, 98, 117, 105, 108, 100, 10, 32, 32, 
    99, 108, 101, 97, 110, 10, 32, 32, 119, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_13 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    98, 117, 105, 108, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    119, 97, 116, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    85, 110, 107, 110, 111, 119, 110, 32, 116, 97, 115, 107, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    45, 45, 116, 114, 101, 101, 0
  };

moonbit_string_t _M0FP34moon4test6single14selected__task(
  struct _M0TPB5ArrayGsE* _M0L4argsS96
) {
  int32_t _M0L6_2atmpS328;
  int32_t _if__result_357;
  #line 15 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
  moonbit_incref(_M0L4argsS96);
  #line 16 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
  _M0L6_2atmpS328 = _M0MPC15array5Array6lengthGsE(_M0L4argsS96);
  if (_M0L6_2atmpS328 >= 3) {
    moonbit_string_t _M0L6_2atmpS327;
    moonbit_incref(_M0L4argsS96);
    #line 16 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _M0L6_2atmpS327 = _M0MPC15array5Array2atGsE(_M0L4argsS96, 1);
    #line 16 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _if__result_357
    = moonbit_val_array_equal(_M0L6_2atmpS327, (moonbit_string_t)moonbit_string_literal_0.data);
    moonbit_decref(_M0L6_2atmpS327);
  } else {
    _if__result_357 = 0;
  }
  if (_if__result_357) {
    #line 17 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    return _M0MPC15array5Array2atGsE(_M0L4argsS96, 2);
  } else {
    int32_t _M0L6_2atmpS329;
    moonbit_incref(_M0L4argsS96);
    #line 18 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _M0L6_2atmpS329 = _M0MPC15array5Array6lengthGsE(_M0L4argsS96);
    if (_M0L6_2atmpS329 >= 2) {
      #line 19 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
      return _M0MPC15array5Array2atGsE(_M0L4argsS96, 1);
    } else {
      moonbit_decref(_M0L4argsS96);
      return (moonbit_string_t)moonbit_string_literal_1.data;
    }
  }
}

int32_t _M0FP34moon4test6single13exit__process(int32_t _M0L8_2aparamS104) {
  exit(_M0L8_2aparamS104);
  return 0;
}

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView _M0L3strS81,
  int32_t _M0L3bomS87
) {
  int32_t _M0L6lengthS80;
  int32_t _M0L12utf8__lengthS82;
  int32_t _M0L1iS83;
  int32_t _M0L12utf8__lengthS84;
  moonbit_bytes_t _M0L3arrS88;
  int32_t _M0L6_2atmpS315;
  int32_t _M0L1iS89;
  int32_t _M0L6offsetS90;
  #line 28 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
  moonbit_incref(_M0L3strS81.$0);
  #line 29 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
  _M0L6lengthS80 = _M0MPC16string10StringView6length(_M0L3strS81);
  _M0L1iS83 = 0;
  _M0L12utf8__lengthS84 = 0;
  while (1) {
    if (_M0L1iS83 < _M0L6lengthS80) {
      int32_t _M0L4codeS85;
      int32_t _tmp_360;
      int32_t _tmp_361;
      moonbit_incref(_M0L3strS81.$0);
      #line 31 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      _M0L4codeS85
      = _M0MPC16string10StringView11unsafe__get(_M0L3strS81, _M0L1iS83);
      if (_M0L4codeS85 < 128) {
        int32_t _M0L6_2atmpS316 = _M0L1iS83 + 1;
        int32_t _M0L6_2atmpS317 = _M0L12utf8__lengthS84 + 1;
        _M0L1iS83 = _M0L6_2atmpS316;
        _M0L12utf8__lengthS84 = _M0L6_2atmpS317;
        continue;
      } else if (_M0L4codeS85 < 2048) {
        int32_t _M0L6_2atmpS318 = _M0L1iS83 + 1;
        int32_t _M0L6_2atmpS319 = _M0L12utf8__lengthS84 + 2;
        _M0L1iS83 = _M0L6_2atmpS318;
        _M0L12utf8__lengthS84 = _M0L6_2atmpS319;
        continue;
      } else {
        int32_t _if__result_359;
        #line 36 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L4codeS85)) {
          int32_t _M0L6_2atmpS322 = _M0L1iS83 + 1;
          if (_M0L6_2atmpS322 < _M0L6lengthS80) {
            int32_t _M0L6_2atmpS321 = _M0L1iS83 + 1;
            int32_t _M0L6_2atmpS320;
            moonbit_incref(_M0L3strS81.$0);
            #line 38 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0L6_2atmpS320
            = _M0MPC16string10StringView11unsafe__get(_M0L3strS81, _M0L6_2atmpS321);
            #line 38 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _if__result_359
            = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS320);
          } else {
            _if__result_359 = 0;
          }
        } else {
          _if__result_359 = 0;
        }
        if (_if__result_359) {
          int32_t _M0L6_2atmpS323 = _M0L1iS83 + 2;
          int32_t _M0L6_2atmpS324 = _M0L12utf8__lengthS84 + 4;
          _M0L1iS83 = _M0L6_2atmpS323;
          _M0L12utf8__lengthS84 = _M0L6_2atmpS324;
          continue;
        } else {
          #line 40 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          if (_M0MPC16uint166UInt1613is__surrogate(_M0L4codeS85)) {
            #line 41 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_2.data);
          } else {
            int32_t _M0L6_2atmpS325 = _M0L1iS83 + 1;
            int32_t _M0L6_2atmpS326 = _M0L12utf8__lengthS84 + 3;
            _M0L1iS83 = _M0L6_2atmpS325;
            _M0L12utf8__lengthS84 = _M0L6_2atmpS326;
            continue;
          }
        }
      }
      _tmp_360 = _M0L1iS83;
      _tmp_361 = _M0L12utf8__lengthS84;
      _M0L1iS83 = _tmp_360;
      _M0L12utf8__lengthS84 = _tmp_361;
      continue;
    } else if (_M0L3bomS87) {
      _M0L12utf8__lengthS82 = 3 + _M0L12utf8__lengthS84;
    } else {
      _M0L12utf8__lengthS82 = _M0L12utf8__lengthS84;
    }
    break;
  }
  _M0L3arrS88 = (moonbit_bytes_t)moonbit_make_bytes(_M0L12utf8__lengthS82, 0);
  if (_M0L3bomS87) {
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L3arrS88)) {
      #line 55 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      moonbit_panic();
    }
    _M0L3arrS88[0] = 239;
    if (1 < 0 || 1 >= Moonbit_array_length(_M0L3arrS88)) {
      #line 56 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      moonbit_panic();
    }
    _M0L3arrS88[1] = 187;
    if (2 < 0 || 2 >= Moonbit_array_length(_M0L3arrS88)) {
      #line 57 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      moonbit_panic();
    }
    _M0L3arrS88[2] = 191;
    _M0L6_2atmpS315 = 3;
  } else {
    _M0L6_2atmpS315 = 0;
  }
  _M0L1iS89 = 0;
  _M0L6offsetS90 = _M0L6_2atmpS315;
  while (1) {
    if (_M0L1iS89 < _M0L6lengthS80) {
      int32_t _M0L10code__unitS91;
      int32_t _M0L4codeS92;
      int32_t _tmp_364;
      int32_t _tmp_365;
      moonbit_incref(_M0L3strS81.$0);
      #line 62 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
      _M0L10code__unitS91
      = _M0MPC16string10StringView11unsafe__get(_M0L3strS81, _M0L1iS89);
      _M0L4codeS92 = (int32_t)_M0L10code__unitS91;
      if (_M0L4codeS92 < 128) {
        int32_t _M0L6_2atmpS263 = _M0L4codeS92 & 0xff;
        int32_t _M0L6_2atmpS264;
        int32_t _M0L6_2atmpS265;
        if (
          _M0L6offsetS90 < 0
          || _M0L6offsetS90 >= Moonbit_array_length(_M0L3arrS88)
        ) {
          #line 65 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          moonbit_panic();
        }
        _M0L3arrS88[_M0L6offsetS90] = _M0L6_2atmpS263;
        _M0L6_2atmpS264 = _M0L1iS89 + 1;
        _M0L6_2atmpS265 = _M0L6offsetS90 + 1;
        _M0L1iS89 = _M0L6_2atmpS264;
        _M0L6offsetS90 = _M0L6_2atmpS265;
        continue;
      } else if (_M0L4codeS92 < 2048) {
        int32_t _M0L6_2atmpS268 = _M0L4codeS92 >> 6;
        int32_t _M0L6_2atmpS267 = 192 + _M0L6_2atmpS268;
        int32_t _M0L6_2atmpS266 = _M0L6_2atmpS267 & 0xff;
        int32_t _M0L6_2atmpS269;
        int32_t _M0L6_2atmpS272;
        int32_t _M0L6_2atmpS271;
        int32_t _M0L6_2atmpS270;
        int32_t _M0L6_2atmpS273;
        int32_t _M0L6_2atmpS274;
        if (
          _M0L6offsetS90 < 0
          || _M0L6offsetS90 >= Moonbit_array_length(_M0L3arrS88)
        ) {
          #line 68 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          moonbit_panic();
        }
        _M0L3arrS88[_M0L6offsetS90] = _M0L6_2atmpS266;
        _M0L6_2atmpS269 = _M0L6offsetS90 + 1;
        _M0L6_2atmpS272 = _M0L4codeS92 & 63;
        _M0L6_2atmpS271 = 128 + _M0L6_2atmpS272;
        _M0L6_2atmpS270 = _M0L6_2atmpS271 & 0xff;
        if (
          _M0L6_2atmpS269 < 0
          || _M0L6_2atmpS269 >= Moonbit_array_length(_M0L3arrS88)
        ) {
          #line 69 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          moonbit_panic();
        }
        _M0L3arrS88[_M0L6_2atmpS269] = _M0L6_2atmpS270;
        _M0L6_2atmpS273 = _M0L1iS89 + 1;
        _M0L6_2atmpS274 = _M0L6offsetS90 + 2;
        _M0L1iS89 = _M0L6_2atmpS273;
        _M0L6offsetS90 = _M0L6_2atmpS274;
        continue;
      } else {
        int32_t _if__result_363;
        #line 71 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
        if (
          _M0MPC16uint166UInt1622is__leading__surrogate(_M0L10code__unitS91)
        ) {
          int32_t _M0L6_2atmpS275 = _M0L1iS89 + 1;
          _if__result_363 = _M0L6_2atmpS275 < _M0L6lengthS80;
        } else {
          _if__result_363 = 0;
        }
        if (_if__result_363) {
          int32_t _M0L6_2atmpS300 = _M0L1iS89 + 1;
          int32_t _M0L8trailingS94;
          moonbit_incref(_M0L3strS81.$0);
          #line 72 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          _M0L8trailingS94
          = _M0MPC16string10StringView11unsafe__get(_M0L3strS81, _M0L6_2atmpS300);
          #line 73 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          if (
            _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L8trailingS94)
          ) {
            int32_t _M0L6_2atmpS299 = _M0L4codeS92 - 55296;
            int32_t _M0L6_2atmpS296 = _M0L6_2atmpS299 << 10;
            int32_t _M0L6_2atmpS298 = (int32_t)_M0L8trailingS94;
            int32_t _M0L6_2atmpS297 = _M0L6_2atmpS298 - 56320;
            int32_t _M0L6_2atmpS295 = _M0L6_2atmpS296 + _M0L6_2atmpS297;
            int32_t _M0L4codeS95 = _M0L6_2atmpS295 + 65536;
            int32_t _M0L6_2atmpS278 = _M0L4codeS95 >> 18;
            int32_t _M0L6_2atmpS277 = 240 + _M0L6_2atmpS278;
            int32_t _M0L6_2atmpS276 = _M0L6_2atmpS277 & 0xff;
            int32_t _M0L6_2atmpS279;
            int32_t _M0L6_2atmpS283;
            int32_t _M0L6_2atmpS282;
            int32_t _M0L6_2atmpS281;
            int32_t _M0L6_2atmpS280;
            int32_t _M0L6_2atmpS284;
            int32_t _M0L6_2atmpS288;
            int32_t _M0L6_2atmpS287;
            int32_t _M0L6_2atmpS286;
            int32_t _M0L6_2atmpS285;
            int32_t _M0L6_2atmpS289;
            int32_t _M0L6_2atmpS292;
            int32_t _M0L6_2atmpS291;
            int32_t _M0L6_2atmpS290;
            int32_t _M0L6_2atmpS293;
            int32_t _M0L6_2atmpS294;
            if (
              _M0L6offsetS90 < 0
              || _M0L6offsetS90 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 77 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6offsetS90] = _M0L6_2atmpS276;
            _M0L6_2atmpS279 = _M0L6offsetS90 + 1;
            _M0L6_2atmpS283 = _M0L4codeS95 >> 12;
            _M0L6_2atmpS282 = _M0L6_2atmpS283 & 63;
            _M0L6_2atmpS281 = 128 + _M0L6_2atmpS282;
            _M0L6_2atmpS280 = _M0L6_2atmpS281 & 0xff;
            if (
              _M0L6_2atmpS279 < 0
              || _M0L6_2atmpS279 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 78 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6_2atmpS279] = _M0L6_2atmpS280;
            _M0L6_2atmpS284 = _M0L6offsetS90 + 2;
            _M0L6_2atmpS288 = _M0L4codeS95 >> 6;
            _M0L6_2atmpS287 = _M0L6_2atmpS288 & 63;
            _M0L6_2atmpS286 = 128 + _M0L6_2atmpS287;
            _M0L6_2atmpS285 = _M0L6_2atmpS286 & 0xff;
            if (
              _M0L6_2atmpS284 < 0
              || _M0L6_2atmpS284 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 79 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6_2atmpS284] = _M0L6_2atmpS285;
            _M0L6_2atmpS289 = _M0L6offsetS90 + 3;
            _M0L6_2atmpS292 = _M0L4codeS95 & 63;
            _M0L6_2atmpS291 = 128 + _M0L6_2atmpS292;
            _M0L6_2atmpS290 = _M0L6_2atmpS291 & 0xff;
            if (
              _M0L6_2atmpS289 < 0
              || _M0L6_2atmpS289 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 80 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6_2atmpS289] = _M0L6_2atmpS290;
            _M0L6_2atmpS293 = _M0L1iS89 + 2;
            _M0L6_2atmpS294 = _M0L6offsetS90 + 4;
            _M0L1iS89 = _M0L6_2atmpS293;
            _M0L6offsetS90 = _M0L6_2atmpS294;
            continue;
          } else {
            #line 83 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_2.data);
          }
        } else {
          #line 85 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
          if (_M0MPC16uint166UInt1613is__surrogate(_M0L10code__unitS91)) {
            #line 86 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
            _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_2.data);
          } else {
            int32_t _M0L6_2atmpS303 = _M0L4codeS92 >> 12;
            int32_t _M0L6_2atmpS302 = 224 + _M0L6_2atmpS303;
            int32_t _M0L6_2atmpS301 = _M0L6_2atmpS302 & 0xff;
            int32_t _M0L6_2atmpS304;
            int32_t _M0L6_2atmpS308;
            int32_t _M0L6_2atmpS307;
            int32_t _M0L6_2atmpS306;
            int32_t _M0L6_2atmpS305;
            int32_t _M0L6_2atmpS309;
            int32_t _M0L6_2atmpS312;
            int32_t _M0L6_2atmpS311;
            int32_t _M0L6_2atmpS310;
            int32_t _M0L6_2atmpS313;
            int32_t _M0L6_2atmpS314;
            if (
              _M0L6offsetS90 < 0
              || _M0L6offsetS90 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 88 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6offsetS90] = _M0L6_2atmpS301;
            _M0L6_2atmpS304 = _M0L6offsetS90 + 1;
            _M0L6_2atmpS308 = _M0L4codeS92 >> 6;
            _M0L6_2atmpS307 = _M0L6_2atmpS308 & 63;
            _M0L6_2atmpS306 = 128 + _M0L6_2atmpS307;
            _M0L6_2atmpS305 = _M0L6_2atmpS306 & 0xff;
            if (
              _M0L6_2atmpS304 < 0
              || _M0L6_2atmpS304 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 89 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6_2atmpS304] = _M0L6_2atmpS305;
            _M0L6_2atmpS309 = _M0L6offsetS90 + 2;
            _M0L6_2atmpS312 = _M0L4codeS92 & 63;
            _M0L6_2atmpS311 = 128 + _M0L6_2atmpS312;
            _M0L6_2atmpS310 = _M0L6_2atmpS311 & 0xff;
            if (
              _M0L6_2atmpS309 < 0
              || _M0L6_2atmpS309 >= Moonbit_array_length(_M0L3arrS88)
            ) {
              #line 90 "/Users/user/.moon/lib/core/encoding/utf8/encode.mbt"
              moonbit_panic();
            }
            _M0L3arrS88[_M0L6_2atmpS309] = _M0L6_2atmpS310;
            _M0L6_2atmpS313 = _M0L1iS89 + 1;
            _M0L6_2atmpS314 = _M0L6offsetS90 + 3;
            _M0L1iS89 = _M0L6_2atmpS313;
            _M0L6offsetS90 = _M0L6_2atmpS314;
            continue;
          }
        }
      }
      _tmp_364 = _M0L1iS89;
      _tmp_365 = _M0L6offsetS90;
      _M0L1iS89 = _tmp_364;
      _M0L6offsetS90 = _tmp_365;
      continue;
    } else {
      moonbit_decref(_M0L3strS81.$0);
    }
    break;
  }
  return _M0L3arrS88;
}

struct _M0TPB5ArrayGsE* _M0FPC13env4args() {
  #line 17 "/Users/user/.moon/lib/core/env/env.mbt"
  #line 18 "/Users/user/.moon/lib/core/env/env.mbt"
  return _M0FPC13env24get__cli__args__internal();
}

struct _M0TPB5ArrayGsE* _M0FPC13env24get__cli__args__internal() {
  moonbit_string_t* _M0L6_2atmpS262;
  struct _M0TPB5ArrayGsE* _M0L6_2aaccS74;
  moonbit_bytes_t* _M0L7_2abindS75;
  int32_t _M0L7_2abindS76;
  int32_t _M0L2__S77;
  #line 19 "/Users/user/.moon/lib/core/env/env_native.mbt"
  _M0L6_2atmpS262 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2aaccS74
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2aaccS74)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2aaccS74->$0 = _M0L6_2atmpS262;
  _M0L6_2aaccS74->$1 = 0;
  #line 21 "/Users/user/.moon/lib/core/env/env_native.mbt"
  _M0L7_2abindS75 = _M0FPC13env19get__cli__args__ffi();
  _M0L7_2abindS76 = Moonbit_array_length(_M0L7_2abindS75);
  _M0L2__S77 = 0;
  while (1) {
    if (_M0L2__S77 < _M0L7_2abindS76) {
      moonbit_bytes_t _M0L1tS78 =
        (moonbit_bytes_t)_M0L7_2abindS75[_M0L2__S77];
      moonbit_string_t _M0L6_2atmpS260;
      int32_t _M0L6_2atmpS261;
      moonbit_incref(_M0L1tS78);
      #line 21 "/Users/user/.moon/lib/core/env/env_native.mbt"
      _M0L6_2atmpS260 = _M0FPC13env28utf8__bytes__to__mbt__string(_M0L1tS78);
      moonbit_incref(_M0L6_2aaccS74);
      #line 20 "/Users/user/.moon/lib/core/env/env_native.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6_2aaccS74, _M0L6_2atmpS260);
      _M0L6_2atmpS261 = _M0L2__S77 + 1;
      _M0L2__S77 = _M0L6_2atmpS261;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS75);
      return _M0L6_2aaccS74;
    }
    break;
  }
}

moonbit_string_t _M0FPC13env28utf8__bytes__to__mbt__string(
  moonbit_bytes_t _M0L5bytesS70
) {
  struct _M0TPB13StringBuilder* _M0L3resS68;
  int32_t _M0L3lenS69;
  int32_t _M0Lm1iS71;
  #line 26 "/Users/user/.moon/lib/core/env/env_native.mbt"
  #line 27 "/Users/user/.moon/lib/core/env/env_native.mbt"
  _M0L3resS68 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS69 = Moonbit_array_length(_M0L5bytesS70);
  _M0Lm1iS71 = 0;
  while (1) {
    int32_t _M0L6_2atmpS183 = _M0Lm1iS71;
    if (_M0L6_2atmpS183 < _M0L3lenS69) {
      int32_t _M0L6_2atmpS259 = _M0Lm1iS71;
      int32_t _M0L6_2atmpS258;
      int32_t _M0Lm1cS72;
      int32_t _M0L6_2atmpS184;
      if (
        _M0L6_2atmpS259 < 0
        || _M0L6_2atmpS259 >= Moonbit_array_length(_M0L5bytesS70)
      ) {
        #line 31 "/Users/user/.moon/lib/core/env/env_native.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS258 = _M0L5bytesS70[_M0L6_2atmpS259];
      _M0Lm1cS72 = (int32_t)_M0L6_2atmpS258;
      _M0L6_2atmpS184 = _M0Lm1cS72;
      if (_M0L6_2atmpS184 == 0) {
        moonbit_decref(_M0L5bytesS70);
        break;
      } else {
        int32_t _M0L6_2atmpS185 = _M0Lm1cS72;
        if (_M0L6_2atmpS185 < 128) {
          int32_t _M0L6_2atmpS187 = _M0Lm1cS72;
          int32_t _M0L6_2atmpS186 = _M0L6_2atmpS187;
          int32_t _M0L6_2atmpS188;
          moonbit_incref(_M0L3resS68);
          #line 36 "/Users/user/.moon/lib/core/env/env_native.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS68, _M0L6_2atmpS186);
          _M0L6_2atmpS188 = _M0Lm1iS71;
          _M0Lm1iS71 = _M0L6_2atmpS188 + 1;
        } else {
          int32_t _M0L6_2atmpS189 = _M0Lm1cS72;
          if (_M0L6_2atmpS189 < 224) {
            int32_t _M0L6_2atmpS191 = _M0Lm1iS71;
            int32_t _M0L6_2atmpS190 = _M0L6_2atmpS191 + 1;
            int32_t _M0L6_2atmpS199;
            int32_t _M0L6_2atmpS198;
            int32_t _M0L6_2atmpS192;
            int32_t _M0L6_2atmpS197;
            int32_t _M0L6_2atmpS196;
            int32_t _M0L6_2atmpS195;
            int32_t _M0L6_2atmpS194;
            int32_t _M0L6_2atmpS193;
            int32_t _M0L6_2atmpS201;
            int32_t _M0L6_2atmpS200;
            int32_t _M0L6_2atmpS202;
            if (_M0L6_2atmpS190 >= _M0L3lenS69) {
              moonbit_decref(_M0L5bytesS70);
              break;
            }
            _M0L6_2atmpS199 = _M0Lm1cS72;
            _M0L6_2atmpS198 = _M0L6_2atmpS199 & 31;
            _M0L6_2atmpS192 = _M0L6_2atmpS198 << 6;
            _M0L6_2atmpS197 = _M0Lm1iS71;
            _M0L6_2atmpS196 = _M0L6_2atmpS197 + 1;
            if (
              _M0L6_2atmpS196 < 0
              || _M0L6_2atmpS196 >= Moonbit_array_length(_M0L5bytesS70)
            ) {
              #line 42 "/Users/user/.moon/lib/core/env/env_native.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS195 = _M0L5bytesS70[_M0L6_2atmpS196];
            _M0L6_2atmpS194 = (int32_t)_M0L6_2atmpS195;
            _M0L6_2atmpS193 = _M0L6_2atmpS194 & 63;
            _M0Lm1cS72 = _M0L6_2atmpS192 | _M0L6_2atmpS193;
            _M0L6_2atmpS201 = _M0Lm1cS72;
            _M0L6_2atmpS200 = _M0L6_2atmpS201;
            moonbit_incref(_M0L3resS68);
            #line 43 "/Users/user/.moon/lib/core/env/env_native.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS68, _M0L6_2atmpS200);
            _M0L6_2atmpS202 = _M0Lm1iS71;
            _M0Lm1iS71 = _M0L6_2atmpS202 + 2;
          } else {
            int32_t _M0L6_2atmpS203 = _M0Lm1cS72;
            if (_M0L6_2atmpS203 < 240) {
              int32_t _M0L6_2atmpS205 = _M0Lm1iS71;
              int32_t _M0L6_2atmpS204 = _M0L6_2atmpS205 + 2;
              int32_t _M0L6_2atmpS220;
              int32_t _M0L6_2atmpS219;
              int32_t _M0L6_2atmpS212;
              int32_t _M0L6_2atmpS218;
              int32_t _M0L6_2atmpS217;
              int32_t _M0L6_2atmpS216;
              int32_t _M0L6_2atmpS215;
              int32_t _M0L6_2atmpS214;
              int32_t _M0L6_2atmpS213;
              int32_t _M0L6_2atmpS206;
              int32_t _M0L6_2atmpS211;
              int32_t _M0L6_2atmpS210;
              int32_t _M0L6_2atmpS209;
              int32_t _M0L6_2atmpS208;
              int32_t _M0L6_2atmpS207;
              int32_t _M0L6_2atmpS222;
              int32_t _M0L6_2atmpS221;
              int32_t _M0L6_2atmpS223;
              if (_M0L6_2atmpS204 >= _M0L3lenS69) {
                moonbit_decref(_M0L5bytesS70);
                break;
              }
              _M0L6_2atmpS220 = _M0Lm1cS72;
              _M0L6_2atmpS219 = _M0L6_2atmpS220 & 15;
              _M0L6_2atmpS212 = _M0L6_2atmpS219 << 12;
              _M0L6_2atmpS218 = _M0Lm1iS71;
              _M0L6_2atmpS217 = _M0L6_2atmpS218 + 1;
              if (
                _M0L6_2atmpS217 < 0
                || _M0L6_2atmpS217 >= Moonbit_array_length(_M0L5bytesS70)
              ) {
                #line 50 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS216 = _M0L5bytesS70[_M0L6_2atmpS217];
              _M0L6_2atmpS215 = (int32_t)_M0L6_2atmpS216;
              _M0L6_2atmpS214 = _M0L6_2atmpS215 & 63;
              _M0L6_2atmpS213 = _M0L6_2atmpS214 << 6;
              _M0L6_2atmpS206 = _M0L6_2atmpS212 | _M0L6_2atmpS213;
              _M0L6_2atmpS211 = _M0Lm1iS71;
              _M0L6_2atmpS210 = _M0L6_2atmpS211 + 2;
              if (
                _M0L6_2atmpS210 < 0
                || _M0L6_2atmpS210 >= Moonbit_array_length(_M0L5bytesS70)
              ) {
                #line 51 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS209 = _M0L5bytesS70[_M0L6_2atmpS210];
              _M0L6_2atmpS208 = (int32_t)_M0L6_2atmpS209;
              _M0L6_2atmpS207 = _M0L6_2atmpS208 & 63;
              _M0Lm1cS72 = _M0L6_2atmpS206 | _M0L6_2atmpS207;
              _M0L6_2atmpS222 = _M0Lm1cS72;
              _M0L6_2atmpS221 = _M0L6_2atmpS222;
              moonbit_incref(_M0L3resS68);
              #line 52 "/Users/user/.moon/lib/core/env/env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS68, _M0L6_2atmpS221);
              _M0L6_2atmpS223 = _M0Lm1iS71;
              _M0Lm1iS71 = _M0L6_2atmpS223 + 3;
            } else {
              int32_t _M0L6_2atmpS225 = _M0Lm1iS71;
              int32_t _M0L6_2atmpS224 = _M0L6_2atmpS225 + 3;
              int32_t _M0L6_2atmpS247;
              int32_t _M0L6_2atmpS246;
              int32_t _M0L6_2atmpS239;
              int32_t _M0L6_2atmpS245;
              int32_t _M0L6_2atmpS244;
              int32_t _M0L6_2atmpS243;
              int32_t _M0L6_2atmpS242;
              int32_t _M0L6_2atmpS241;
              int32_t _M0L6_2atmpS240;
              int32_t _M0L6_2atmpS232;
              int32_t _M0L6_2atmpS238;
              int32_t _M0L6_2atmpS237;
              int32_t _M0L6_2atmpS236;
              int32_t _M0L6_2atmpS235;
              int32_t _M0L6_2atmpS234;
              int32_t _M0L6_2atmpS233;
              int32_t _M0L6_2atmpS226;
              int32_t _M0L6_2atmpS231;
              int32_t _M0L6_2atmpS230;
              int32_t _M0L6_2atmpS229;
              int32_t _M0L6_2atmpS228;
              int32_t _M0L6_2atmpS227;
              int32_t _M0L6_2atmpS248;
              int32_t _M0L6_2atmpS252;
              int32_t _M0L6_2atmpS251;
              int32_t _M0L6_2atmpS250;
              int32_t _M0L6_2atmpS249;
              int32_t _M0L6_2atmpS256;
              int32_t _M0L6_2atmpS255;
              int32_t _M0L6_2atmpS254;
              int32_t _M0L6_2atmpS253;
              int32_t _M0L6_2atmpS257;
              if (_M0L6_2atmpS224 >= _M0L3lenS69) {
                moonbit_decref(_M0L5bytesS70);
                break;
              }
              _M0L6_2atmpS247 = _M0Lm1cS72;
              _M0L6_2atmpS246 = _M0L6_2atmpS247 & 7;
              _M0L6_2atmpS239 = _M0L6_2atmpS246 << 18;
              _M0L6_2atmpS245 = _M0Lm1iS71;
              _M0L6_2atmpS244 = _M0L6_2atmpS245 + 1;
              if (
                _M0L6_2atmpS244 < 0
                || _M0L6_2atmpS244 >= Moonbit_array_length(_M0L5bytesS70)
              ) {
                #line 59 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS243 = _M0L5bytesS70[_M0L6_2atmpS244];
              _M0L6_2atmpS242 = (int32_t)_M0L6_2atmpS243;
              _M0L6_2atmpS241 = _M0L6_2atmpS242 & 63;
              _M0L6_2atmpS240 = _M0L6_2atmpS241 << 12;
              _M0L6_2atmpS232 = _M0L6_2atmpS239 | _M0L6_2atmpS240;
              _M0L6_2atmpS238 = _M0Lm1iS71;
              _M0L6_2atmpS237 = _M0L6_2atmpS238 + 2;
              if (
                _M0L6_2atmpS237 < 0
                || _M0L6_2atmpS237 >= Moonbit_array_length(_M0L5bytesS70)
              ) {
                #line 60 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS236 = _M0L5bytesS70[_M0L6_2atmpS237];
              _M0L6_2atmpS235 = (int32_t)_M0L6_2atmpS236;
              _M0L6_2atmpS234 = _M0L6_2atmpS235 & 63;
              _M0L6_2atmpS233 = _M0L6_2atmpS234 << 6;
              _M0L6_2atmpS226 = _M0L6_2atmpS232 | _M0L6_2atmpS233;
              _M0L6_2atmpS231 = _M0Lm1iS71;
              _M0L6_2atmpS230 = _M0L6_2atmpS231 + 3;
              if (
                _M0L6_2atmpS230 < 0
                || _M0L6_2atmpS230 >= Moonbit_array_length(_M0L5bytesS70)
              ) {
                #line 61 "/Users/user/.moon/lib/core/env/env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS229 = _M0L5bytesS70[_M0L6_2atmpS230];
              _M0L6_2atmpS228 = (int32_t)_M0L6_2atmpS229;
              _M0L6_2atmpS227 = _M0L6_2atmpS228 & 63;
              _M0Lm1cS72 = _M0L6_2atmpS226 | _M0L6_2atmpS227;
              _M0L6_2atmpS248 = _M0Lm1cS72;
              _M0Lm1cS72 = _M0L6_2atmpS248 - 65536;
              _M0L6_2atmpS252 = _M0Lm1cS72;
              _M0L6_2atmpS251 = _M0L6_2atmpS252 >> 10;
              _M0L6_2atmpS250 = _M0L6_2atmpS251 + 55296;
              _M0L6_2atmpS249 = _M0L6_2atmpS250;
              moonbit_incref(_M0L3resS68);
              #line 63 "/Users/user/.moon/lib/core/env/env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS68, _M0L6_2atmpS249);
              _M0L6_2atmpS256 = _M0Lm1cS72;
              _M0L6_2atmpS255 = _M0L6_2atmpS256 & 1023;
              _M0L6_2atmpS254 = _M0L6_2atmpS255 + 56320;
              _M0L6_2atmpS253 = _M0L6_2atmpS254;
              moonbit_incref(_M0L3resS68);
              #line 64 "/Users/user/.moon/lib/core/env/env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS68, _M0L6_2atmpS253);
              _M0L6_2atmpS257 = _M0Lm1iS71;
              _M0Lm1iS71 = _M0L6_2atmpS257 + 4;
            }
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L5bytesS70);
    }
    break;
  }
  #line 68 "/Users/user/.moon/lib/core/env/env_native.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS68);
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS66,
  int32_t _M0L5indexS67
) {
  int32_t _M0L3lenS65;
  int32_t _if__result_368;
  #line 183 "/Users/user/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS65 = _M0L4selfS66->$1;
  if (_M0L5indexS67 >= 0) {
    _if__result_368 = _M0L5indexS67 < _M0L3lenS65;
  } else {
    _if__result_368 = 0;
  }
  if (_if__result_368) {
    moonbit_string_t* _M0L6_2atmpS182;
    moonbit_string_t _M0L6_2atmpS331;
    #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS182 = _M0MPC15array5Array6bufferGsE(_M0L4selfS66);
    if (
      _M0L5indexS67 < 0
      || _M0L5indexS67 >= Moonbit_array_length(_M0L6_2atmpS182)
    ) {
      #line 188 "/Users/user/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS331 = (moonbit_string_t)_M0L6_2atmpS182[_M0L5indexS67];
    moonbit_incref(_M0L6_2atmpS331);
    moonbit_decref(_M0L6_2atmpS182);
    return _M0L6_2atmpS331;
  } else {
    moonbit_decref(_M0L4selfS66);
    #line 187 "/Users/user/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS64) {
  moonbit_string_t _M0L6_2atmpS181;
  #line 37 "/Users/user/.moon/lib/core/builtin/console.mbt"
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS181 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS64);
  #line 38 "/Users/user/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS181);
  moonbit_decref(_M0L6_2atmpS181);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS61,
  moonbit_string_t _M0L5valueS63
) {
  int32_t _M0L3lenS176;
  moonbit_string_t* _M0L6_2atmpS178;
  int32_t _M0L6_2atmpS177;
  int32_t _M0L6lengthS62;
  moonbit_string_t* _M0L3bufS179;
  moonbit_string_t _M0L6_2aoldS332;
  int32_t _M0L6_2atmpS180;
  #line 242 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS176 = _M0L4selfS61->$1;
  moonbit_incref(_M0L4selfS61);
  #line 243 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS178 = _M0MPC15array5Array6bufferGsE(_M0L4selfS61);
  _M0L6_2atmpS177 = Moonbit_array_length(_M0L6_2atmpS178);
  moonbit_decref(_M0L6_2atmpS178);
  if (_M0L3lenS176 == _M0L6_2atmpS177) {
    moonbit_incref(_M0L4selfS61);
    #line 244 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS61);
  }
  _M0L6lengthS62 = _M0L4selfS61->$1;
  _M0L3bufS179 = _M0L4selfS61->$0;
  _M0L6_2aoldS332 = (moonbit_string_t)_M0L3bufS179[_M0L6lengthS62];
  moonbit_decref(_M0L6_2aoldS332);
  _M0L3bufS179[_M0L6lengthS62] = _M0L5valueS63;
  _M0L6_2atmpS180 = _M0L6lengthS62 + 1;
  _M0L4selfS61->$1 = _M0L6_2atmpS180;
  moonbit_decref(_M0L4selfS61);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS59) {
  int32_t _M0L8old__capS58;
  int32_t _M0L8new__capS60;
  #line 182 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS58 = _M0L4selfS59->$1;
  if (_M0L8old__capS58 == 0) {
    _M0L8new__capS60 = 8;
  } else {
    _M0L8new__capS60 = _M0L8old__capS58 * 2;
  }
  #line 185 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS59, _M0L8new__capS60);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS55,
  int32_t _M0L13new__capacityS53
) {
  moonbit_string_t* _M0L8new__bufS52;
  moonbit_string_t* _M0L8old__bufS54;
  int32_t _M0L8old__capS56;
  int32_t _M0L9copy__lenS57;
  moonbit_string_t* _M0L6_2aoldS334;
  #line 129 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS52
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS53, (moonbit_string_t)moonbit_string_literal_3.data);
  _M0L8old__bufS54 = _M0L4selfS55->$0;
  _M0L8old__capS56 = Moonbit_array_length(_M0L8old__bufS54);
  if (_M0L8old__capS56 < _M0L13new__capacityS53) {
    _M0L9copy__lenS57 = _M0L8old__capS56;
  } else {
    _M0L9copy__lenS57 = _M0L13new__capacityS53;
  }
  moonbit_incref(_M0L8old__bufS54);
  moonbit_incref(_M0L8new__bufS52);
  #line 134 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS52, 0, _M0L8old__bufS54, 0, _M0L9copy__lenS57);
  _M0L6_2aoldS334 = _M0L4selfS55->$0;
  moonbit_decref(_M0L6_2aoldS334);
  _M0L4selfS55->$0 = _M0L8new__bufS52;
  moonbit_decref(_M0L4selfS55);
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS51) {
  int32_t _result_369;
  #line 80 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _result_369 = _M0L4selfS51->$1;
  moonbit_decref(_M0L4selfS51);
  return _result_369;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS50
) {
  moonbit_string_t* _M0L8_2afieldS336;
  int32_t _M0L6_2acntS351;
  #line 124 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS336 = _M0L4selfS50->$0;
  _M0L6_2acntS351 = Moonbit_object_header(_M0L4selfS50)->rc;
  if (_M0L6_2acntS351 > 1) {
    int32_t _M0L11_2anew__cntS352 = _M0L6_2acntS351 - 1;
    Moonbit_object_header(_M0L4selfS50)->rc = _M0L11_2anew__cntS352;
    moonbit_incref(_M0L8_2afieldS336);
  } else if (_M0L6_2acntS351 == 1) {
    #line 125 "/Users/user/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS50);
  }
  return _M0L8_2afieldS336;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS49
) {
  #line 262 "/Users/user/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS49;
}

int32_t _M0MPC16uint166UInt1613is__surrogate(int32_t _M0L4selfS48) {
  #line 62 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS48 >= 55296) {
    return _M0L4selfS48 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS46,
  int32_t _M0L5indexS47
) {
  moonbit_string_t _M0L3strS173;
  int32_t _M0L5startS175;
  int32_t _M0L6_2atmpS174;
  int32_t _result_370;
  #line 127 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS173 = _M0L4selfS46.$0;
  _M0L5startS175 = _M0L4selfS46.$1;
  _M0L6_2atmpS174 = _M0L5startS175 + _M0L5indexS47;
  _result_370 = _M0L3strS173[_M0L6_2atmpS174];
  moonbit_decref(_M0L3strS173);
  return _result_370;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS45
) {
  int32_t _M0L3endS171;
  int32_t _M0L5startS172;
  #line 49 "/Users/user/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS171 = _M0L4selfS45.$2;
  _M0L5startS172 = _M0L4selfS45.$1;
  moonbit_decref(_M0L4selfS45.$0);
  return _M0L3endS171 - _M0L5startS172;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS44) {
  #line 45 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS44 >= 56320) {
    return _M0L4selfS44 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS43) {
  #line 28 "/Users/user/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS43 >= 55296) {
    return _M0L4selfS43 <= 56319;
  } else {
    return 0;
  }
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
    int32_t _M0L3lenS150 = _M0L4selfS41->$1;
    int32_t _M0L6_2atmpS149 = _M0L3lenS150 + 1;
    uint16_t* _M0L4dataS151;
    int32_t _M0L3lenS152;
    int32_t _M0L6_2atmpS153;
    int32_t _M0L3lenS155;
    int32_t _M0L6_2atmpS154;
    moonbit_incref(_M0L4selfS41);
    #line 93 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS41, _M0L6_2atmpS149);
    _M0L4dataS151 = _M0L4selfS41->$0;
    _M0L3lenS152 = _M0L4selfS41->$1;
    moonbit_incref(_M0L4dataS151);
    #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS153 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS39);
    if (
      _M0L3lenS152 < 0 || _M0L3lenS152 >= Moonbit_array_length(_M0L4dataS151)
    ) {
      #line 94 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS151[_M0L3lenS152] = _M0L6_2atmpS153;
    moonbit_decref(_M0L4dataS151);
    _M0L3lenS155 = _M0L4selfS41->$1;
    _M0L6_2atmpS154 = _M0L3lenS155 + 1;
    _M0L4selfS41->$1 = _M0L6_2atmpS154;
    moonbit_decref(_M0L4selfS41);
  } else if (_M0L4codeS39 <= 1114111u) {
    int32_t _M0L3lenS157 = _M0L4selfS41->$1;
    int32_t _M0L6_2atmpS156 = _M0L3lenS157 + 2;
    uint32_t _M0L4codeS42;
    uint16_t* _M0L4dataS158;
    int32_t _M0L3lenS159;
    uint32_t _M0L6_2atmpS162;
    uint32_t _M0L6_2atmpS161;
    int32_t _M0L6_2atmpS160;
    uint16_t* _M0L4dataS163;
    int32_t _M0L3lenS168;
    int32_t _M0L6_2atmpS164;
    uint32_t _M0L6_2atmpS167;
    uint32_t _M0L6_2atmpS166;
    int32_t _M0L6_2atmpS165;
    int32_t _M0L3lenS170;
    int32_t _M0L6_2atmpS169;
    moonbit_incref(_M0L4selfS41);
    #line 97 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS41, _M0L6_2atmpS156);
    _M0L4codeS42 = _M0L4codeS39 - 65536u;
    _M0L4dataS158 = _M0L4selfS41->$0;
    _M0L3lenS159 = _M0L4selfS41->$1;
    _M0L6_2atmpS162 = _M0L4codeS42 >> 10;
    _M0L6_2atmpS161 = 55296u + _M0L6_2atmpS162;
    moonbit_incref(_M0L4dataS158);
    #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS160 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS161);
    if (
      _M0L3lenS159 < 0 || _M0L3lenS159 >= Moonbit_array_length(_M0L4dataS158)
    ) {
      #line 99 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS158[_M0L3lenS159] = _M0L6_2atmpS160;
    moonbit_decref(_M0L4dataS158);
    _M0L4dataS163 = _M0L4selfS41->$0;
    _M0L3lenS168 = _M0L4selfS41->$1;
    _M0L6_2atmpS164 = _M0L3lenS168 + 1;
    _M0L6_2atmpS167 = _M0L4codeS42 & 1023u;
    _M0L6_2atmpS166 = 56320u + _M0L6_2atmpS167;
    moonbit_incref(_M0L4dataS163);
    #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS165 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS166);
    if (
      _M0L6_2atmpS164 < 0
      || _M0L6_2atmpS164 >= Moonbit_array_length(_M0L4dataS163)
    ) {
      #line 100 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS163[_M0L6_2atmpS164] = _M0L6_2atmpS165;
    moonbit_decref(_M0L4dataS163);
    _M0L3lenS170 = _M0L4selfS41->$1;
    _M0L6_2atmpS169 = _M0L3lenS170 + 2;
    _M0L4selfS41->$1 = _M0L6_2atmpS169;
    moonbit_decref(_M0L4selfS41);
  } else {
    moonbit_decref(_M0L4selfS41);
    #line 103 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_4.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS33,
  int32_t _M0L8requiredS34
) {
  uint16_t* _M0L4dataS148;
  int32_t _M0L12current__lenS32;
  int32_t _M0L13enough__spaceS35;
  int32_t _M0L13enough__spaceS36;
  int32_t _M0L6_2atmpS146;
  uint16_t* _M0L9new__dataS38;
  uint16_t* _M0L4dataS144;
  int32_t _M0L3lenS145;
  uint16_t* _M0L6_2aoldS341;
  #line 45 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS148 = _M0L4selfS33->$0;
  _M0L12current__lenS32 = Moonbit_array_length(_M0L4dataS148);
  if (_M0L8requiredS34 <= _M0L12current__lenS32) {
    moonbit_decref(_M0L4selfS33);
    return 0;
  }
  _M0L13enough__spaceS36 = _M0L12current__lenS32;
  while (1) {
    if (_M0L13enough__spaceS36 < _M0L8requiredS34) {
      int32_t _M0L6_2atmpS147 = _M0L13enough__spaceS36 * 2;
      _M0L13enough__spaceS36 = _M0L6_2atmpS147;
      continue;
    } else {
      _M0L13enough__spaceS35 = _M0L13enough__spaceS36;
    }
    break;
  }
  #line 60 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS146 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L9new__dataS38
  = (uint16_t*)moonbit_make_string(_M0L13enough__spaceS35, _M0L6_2atmpS146);
  _M0L4dataS144 = _M0L4selfS33->$0;
  _M0L3lenS145 = _M0L4selfS33->$1;
  moonbit_incref(_M0L4dataS144);
  moonbit_incref(_M0L9new__dataS38);
  #line 61 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L9new__dataS38, 0, _M0L4dataS144, 0, _M0L3lenS145);
  _M0L6_2aoldS341 = _M0L4selfS33->$0;
  moonbit_decref(_M0L6_2aoldS341);
  _M0L4selfS33->$0 = _M0L9new__dataS38;
  moonbit_decref(_M0L4selfS33);
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS31) {
  int32_t _M0L6_2atmpS143;
  #line 2675 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS143 = *(int32_t*)&_M0L4selfS31;
  return (uint16_t)_M0L6_2atmpS143;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS30) {
  int32_t _M0L6_2atmpS142;
  #line 1254 "/Users/user/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS142 = _M0L4selfS30;
  return *(uint32_t*)&_M0L6_2atmpS142;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS28
) {
  int32_t _M0L3lenS134;
  uint16_t* _M0L4dataS136;
  int32_t _M0L6_2atmpS135;
  #line 143 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS134 = _M0L4selfS28->$1;
  _M0L4dataS136 = _M0L4selfS28->$0;
  _M0L6_2atmpS135 = Moonbit_array_length(_M0L4dataS136);
  if (_M0L3lenS134 == _M0L6_2atmpS135) {
    uint16_t* _M0L8_2afieldS344 = _M0L4selfS28->$0;
    int32_t _M0L6_2acntS353 = Moonbit_object_header(_M0L4selfS28)->rc;
    uint16_t* _M0L4dataS137;
    if (_M0L6_2acntS353 > 1) {
      int32_t _M0L11_2anew__cntS354 = _M0L6_2acntS353 - 1;
      Moonbit_object_header(_M0L4selfS28)->rc = _M0L11_2anew__cntS354;
      moonbit_incref(_M0L8_2afieldS344);
    } else if (_M0L6_2acntS353 == 1) {
      #line 145 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS28);
    }
    _M0L4dataS137 = _M0L8_2afieldS344;
    return _M0L4dataS137;
  } else {
    int32_t _M0L3lenS140 = _M0L4selfS28->$1;
    int32_t _M0L6_2atmpS141;
    uint16_t* _M0L4dataS29;
    uint16_t* _M0L4dataS138;
    int32_t _M0L3lenS139;
    int32_t _M0L6_2acntS355;
    #line 147 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS141 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L4dataS29
    = (uint16_t*)moonbit_make_string(_M0L3lenS140, _M0L6_2atmpS141);
    _M0L4dataS138 = _M0L4selfS28->$0;
    _M0L3lenS139 = _M0L4selfS28->$1;
    _M0L6_2acntS355 = Moonbit_object_header(_M0L4selfS28)->rc;
    if (_M0L6_2acntS355 > 1) {
      int32_t _M0L11_2anew__cntS356 = _M0L6_2acntS355 - 1;
      Moonbit_object_header(_M0L4selfS28)->rc = _M0L11_2anew__cntS356;
      moonbit_incref(_M0L4dataS138);
    } else if (_M0L6_2acntS355 == 1) {
      #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_free(_M0L4selfS28);
    }
    moonbit_incref(_M0L4dataS29);
    #line 148 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGkE(_M0L4dataS29, 0, _M0L4dataS138, 0, _M0L3lenS139);
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
  struct _M0TPB13StringBuilder* _block_372;
  #line 32 "/Users/user/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS26 < 1) {
    _M0L7initialS25 = 1;
  } else {
    int32_t _M0L6_2atmpS133 = _M0L10size__hintS26 + 1;
    _M0L7initialS25 = _M0L6_2atmpS133 / 2;
  }
  _M0L4dataS27 = (uint16_t*)moonbit_make_string(_M0L7initialS25, 0);
  _block_372
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_372)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_372->$0 = _M0L4dataS27;
  _block_372->$1 = 0;
  return _block_372;
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
  int32_t _if__result_373;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS2 == _M0L3srcS3) {
    _if__result_373 = _M0L11dst__offsetS4 < _M0L11src__offsetS5;
  } else {
    _if__result_373 = 0;
  }
  if (_if__result_373) {
    int32_t _M0L1iS6 = 0;
    while (1) {
      if (_M0L1iS6 < _M0L3lenS7) {
        int32_t _M0L6_2atmpS115 = _M0L11dst__offsetS4 + _M0L1iS6;
        int32_t _M0L6_2atmpS117 = _M0L11src__offsetS5 + _M0L1iS6;
        int32_t _M0L6_2atmpS116;
        int32_t _M0L6_2atmpS118;
        if (
          _M0L6_2atmpS117 < 0
          || _M0L6_2atmpS117 >= Moonbit_array_length(_M0L3srcS3)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS116 = (int32_t)_M0L3srcS3[_M0L6_2atmpS117];
        if (
          _M0L6_2atmpS115 < 0
          || _M0L6_2atmpS115 >= Moonbit_array_length(_M0L3dstS2)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS2[_M0L6_2atmpS115] = _M0L6_2atmpS116;
        _M0L6_2atmpS118 = _M0L1iS6 + 1;
        _M0L1iS6 = _M0L6_2atmpS118;
        continue;
      } else {
        moonbit_decref(_M0L3srcS3);
        moonbit_decref(_M0L3dstS2);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS123 = _M0L3lenS7 - 1;
    int32_t _M0L1iS9 = _M0L6_2atmpS123;
    while (1) {
      if (_M0L1iS9 >= 0) {
        int32_t _M0L6_2atmpS119 = _M0L11dst__offsetS4 + _M0L1iS9;
        int32_t _M0L6_2atmpS121 = _M0L11src__offsetS5 + _M0L1iS9;
        int32_t _M0L6_2atmpS120;
        int32_t _M0L6_2atmpS122;
        if (
          _M0L6_2atmpS121 < 0
          || _M0L6_2atmpS121 >= Moonbit_array_length(_M0L3srcS3)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS120 = (int32_t)_M0L3srcS3[_M0L6_2atmpS121];
        if (
          _M0L6_2atmpS119 < 0
          || _M0L6_2atmpS119 >= Moonbit_array_length(_M0L3dstS2)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS2[_M0L6_2atmpS119] = _M0L6_2atmpS120;
        _M0L6_2atmpS122 = _M0L1iS9 - 1;
        _M0L1iS9 = _M0L6_2atmpS122;
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
  int32_t _if__result_376;
  #line 38 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS11 == _M0L3srcS12) {
    _if__result_376 = _M0L11dst__offsetS13 < _M0L11src__offsetS14;
  } else {
    _if__result_376 = 0;
  }
  if (_if__result_376) {
    int32_t _M0L1iS15 = 0;
    while (1) {
      if (_M0L1iS15 < _M0L3lenS16) {
        int32_t _M0L6_2atmpS124 = _M0L11dst__offsetS13 + _M0L1iS15;
        int32_t _M0L6_2atmpS126 = _M0L11src__offsetS14 + _M0L1iS15;
        moonbit_string_t _M0L6_2atmpS125;
        moonbit_string_t _M0L6_2aoldS347;
        int32_t _M0L6_2atmpS127;
        if (
          _M0L6_2atmpS126 < 0
          || _M0L6_2atmpS126 >= Moonbit_array_length(_M0L3srcS12)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS125 = (moonbit_string_t)_M0L3srcS12[_M0L6_2atmpS126];
        if (
          _M0L6_2atmpS124 < 0
          || _M0L6_2atmpS124 >= Moonbit_array_length(_M0L3dstS11)
        ) {
          #line 49 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS347 = (moonbit_string_t)_M0L3dstS11[_M0L6_2atmpS124];
        moonbit_incref(_M0L6_2atmpS125);
        moonbit_decref(_M0L6_2aoldS347);
        _M0L3dstS11[_M0L6_2atmpS124] = _M0L6_2atmpS125;
        _M0L6_2atmpS127 = _M0L1iS15 + 1;
        _M0L1iS15 = _M0L6_2atmpS127;
        continue;
      } else {
        moonbit_decref(_M0L3srcS12);
        moonbit_decref(_M0L3dstS11);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS132 = _M0L3lenS16 - 1;
    int32_t _M0L1iS18 = _M0L6_2atmpS132;
    while (1) {
      if (_M0L1iS18 >= 0) {
        int32_t _M0L6_2atmpS128 = _M0L11dst__offsetS13 + _M0L1iS18;
        int32_t _M0L6_2atmpS130 = _M0L11src__offsetS14 + _M0L1iS18;
        moonbit_string_t _M0L6_2atmpS129;
        moonbit_string_t _M0L6_2aoldS349;
        int32_t _M0L6_2atmpS131;
        if (
          _M0L6_2atmpS130 < 0
          || _M0L6_2atmpS130 >= Moonbit_array_length(_M0L3srcS12)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS129 = (moonbit_string_t)_M0L3srcS12[_M0L6_2atmpS130];
        if (
          _M0L6_2atmpS128 < 0
          || _M0L6_2atmpS128 >= Moonbit_array_length(_M0L3dstS11)
        ) {
          #line 53 "/Users/user/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS349 = (moonbit_string_t)_M0L3dstS11[_M0L6_2atmpS128];
        moonbit_incref(_M0L6_2atmpS129);
        moonbit_decref(_M0L6_2aoldS349);
        _M0L3dstS11[_M0L6_2atmpS128] = _M0L6_2atmpS129;
        _M0L6_2atmpS131 = _M0L1iS18 - 1;
        _M0L1iS18 = _M0L6_2atmpS131;
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
  struct _M0TPB5ArrayGsE* _M0L4argsS97;
  int32_t _M0L6_2atmpS106;
  int32_t _if__result_379;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 27 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
  _M0L4argsS97 = _M0FPC13env4args();
  moonbit_incref(_M0L4argsS97);
  #line 28 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
  _M0L6_2atmpS106 = _M0MPC15array5Array6lengthGsE(_M0L4argsS97);
  if (_M0L6_2atmpS106 >= 2) {
    moonbit_string_t _M0L6_2atmpS105;
    moonbit_incref(_M0L4argsS97);
    #line 28 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _M0L6_2atmpS105 = _M0MPC15array5Array2atGsE(_M0L4argsS97, 1);
    #line 28 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _if__result_379
    = moonbit_val_array_equal(_M0L6_2atmpS105, (moonbit_string_t)moonbit_string_literal_5.data);
    moonbit_decref(_M0L6_2atmpS105);
  } else {
    _if__result_379 = 0;
  }
  if (_if__result_379) {
    moonbit_decref(_M0L4argsS97);
    #line 29 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_6.data);
  } else {
    int32_t _M0L6_2atmpS108;
    int32_t _if__result_380;
    moonbit_incref(_M0L4argsS97);
    #line 30 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
    _M0L6_2atmpS108 = _M0MPC15array5Array6lengthGsE(_M0L4argsS97);
    if (_M0L6_2atmpS108 >= 2) {
      moonbit_string_t _M0L6_2atmpS107;
      moonbit_incref(_M0L4argsS97);
      #line 30 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
      _M0L6_2atmpS107 = _M0MPC15array5Array2atGsE(_M0L4argsS97, 1);
      #line 30 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
      _if__result_380
      = moonbit_val_array_equal(_M0L6_2atmpS107, (moonbit_string_t)moonbit_string_literal_7.data);
      moonbit_decref(_M0L6_2atmpS107);
    } else {
      _if__result_380 = 0;
    }
    if (_if__result_380) {
      moonbit_decref(_M0L4argsS97);
      #line 31 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
      _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
    } else {
      moonbit_string_t _M0L4taskS98;
      int32_t _if__result_381;
      #line 33 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
      _M0L4taskS98 = _M0FP34moon4test6single14selected__task(_M0L4argsS97);
      #line 34 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
      if (
        moonbit_val_array_equal(_M0L4taskS98, (moonbit_string_t)moonbit_string_literal_1.data)
      ) {
        _if__result_381 = 1;
      } else {
        #line 34 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        if (
          moonbit_val_array_equal(_M0L4taskS98, (moonbit_string_t)moonbit_string_literal_9.data)
        ) {
          _if__result_381 = 1;
        } else {
          #line 34 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
          _if__result_381
          = moonbit_val_array_equal(_M0L4taskS98, (moonbit_string_t)moonbit_string_literal_10.data);
        }
      }
      if (_if__result_381) {
        moonbit_string_t _M0L6_2atmpS109;
        #line 35 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0L6_2atmpS109
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_11.data, _M0L4taskS98);
        moonbit_decref(_M0L4taskS98);
        #line 35 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0FPB7printlnGsE(_M0L6_2atmpS109);
      } else {
        moonbit_string_t _M0L6_2atmpS114;
        moonbit_string_t _M0L7_2abindS100;
        int32_t _M0L6_2atmpS113;
        struct _M0TPC16string10StringView _M0L6_2atmpS112;
        moonbit_bytes_t _M0L7messageS99;
        int32_t _M0L6_2atmpS111;
        int32_t _M0L6_2atmpS110;
        #line 37 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0L6_2atmpS114
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_12.data, _M0L4taskS98);
        moonbit_decref(_M0L4taskS98);
        #line 37 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0L7_2abindS100
        = moonbit_add_string(_M0L6_2atmpS114, (moonbit_string_t)moonbit_string_literal_13.data);
        moonbit_decref(_M0L6_2atmpS114);
        _M0L6_2atmpS113 = Moonbit_array_length(_M0L7_2abindS100);
        _M0L6_2atmpS112
        = (struct _M0TPC16string10StringView){
          0, _M0L6_2atmpS113, _M0L7_2abindS100
        };
        #line 37 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0L7messageS99
        = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS112, 0);
        _M0L6_2atmpS111 = Moonbit_array_length(_M0L7messageS99);
        #line 38 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0L6_2atmpS110
        = _M0FP34moon4test6single9write__fd(2, _M0L7messageS99, _M0L6_2atmpS111);
        moonbit_decref(_M0L7messageS99);
        #line 39 "/Users/user/workspace/github/gulp/mulp/cmd/mulp/fixtures/mbtx-workspace/_build/mulp.mbt"
        _M0FP34moon4test6single13exit__process(1);
      }
    }
  }
  return 0;
}