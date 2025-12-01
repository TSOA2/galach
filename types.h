#ifndef _GALACH_TYPES_H
#define _GALACH_TYPES_H

#include <stdint.h>

typedef uint8_t   u8;
typedef int8_t    i8;
typedef uint16_t  u16;
typedef int16_t   i16;
typedef uint32_t  u32;
typedef int32_t   i32;
typedef uint64_t  u64;
typedef int64_t   i64;
typedef float     f32;
typedef double    f64;

#if defined(__has_builtin)
#	if __has_builtin(__builtin_expect)
#		define LIKELY(x)   __builtin_expect(!!(x), 1)
#		define UNLIKELY(x) __builtin_expect(!!(x), 0)
#	else
#		define LIKELY(x) (!!(x))
#		define UNLIKELY(x) (!!(x))
#	endif
#else
#	define LIKELY(x) (!!(x))
#	define UNLIKELY(x) (!!(x))
#endif

#define fallthrough() __attribute__((fallthrough))

#ifdef DEBUG
#	include <signal.h>
#	define BREAKPOINT() raise(SIGTRAP)
#endif // DEBUG

#endif // _GALACH_TYPES_H
