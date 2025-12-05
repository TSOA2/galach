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


void *gh_malloc(u64 s);
void *gh_realloc(void *old, u64 s);
void gh_free(void *p);

#define vector_block_size (64)
#define VEC(type) struct __vec_ ## type
#define DEFINE_VEC(type) VEC(type) { \
	type *data; \
	u64 used; \
	u64 size; \
}

#define INIT_VEC(type) (VEC(type)) { \
	.data = gh_malloc(sizeof(type) * vector_block_size), \
	.used = 0, \
	.size = vector_block_size, \
}

#define MAKE_VEC(type, name) VEC(type) name = INIT_VEC(type)

#define GROW_VEC(vec, size_left) do { \
	typeof(vec) *_vec = &(vec); \
	u64 _size_left = (size_left); \
	if ((_vec->size - _vec->used) < _size_left) { \
		_vec->size += vector_block_size > _size_left ? \
			vector_block_size : _size_left; \
		_vec->data = gh_realloc(_vec->data, _vec->size * sizeof(typeof(*_vec->data))); \
	} \
} while (0)

#define APPEND_VEC_RAW(vec, new) do { \
	typeof(vec) *_vec = &(vec); \
	_vec->data[_vec->used++] = (new); \
} while (0)

#define APPEND_VEC(vec, new) do { \
	GROW_VEC((vec), 1); \
	APPEND_VEC_RAW((vec), (new)); \
} while (0)

#define LAST_VEC(vec) (&(vec).data[(vec).used-1])

#define LOOP_VEC(vec, ptr_name, body) \
for (u64 __vec_counter__ = 0; __vec_counter__ < (vec).used; __vec_counter__++) { \
	typeof(vec.data) ptr_name = &(vec).data[__vec_counter__]; \
	body \
}

#define FREE_VEC(vec) do { \
	typeof(vec) *_vec = &(vec); \
	if (_vec->data) gh_free(_vec->data); \
	else gh_log(GH_LOG_WARN, "attempted to free null vector: line %d", __LINE__); \
	_vec->data = NULL; \
	_vec->used = 0; \
	_vec->size = 0; \
} while (0)

#define NULL_VEC(type) ((VEC(type)){})
#define VEC_IS_NULL(vec) (!(vec).data)

#endif // _GALACH_TYPES_H
