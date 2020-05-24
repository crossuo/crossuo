#ifndef MINICONV_CONFIG_H
#define MINICONV_CONFIG_H

#include <stddef.h>

/* 	You must define __BIG_ENDIAN__ if compiling on a big endian platform. */
/*#define __BIG_ENDIAN__*/

#ifndef __BIG_ENDIAN__
	#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		#define __BIG_ENDIAN__
	#endif
#endif

#ifdef __cplusplus
	#define MINICONV_INLINE static inline
#elif defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
	#define MINICONV_INLINE static __inline
#else
	#define MINICONV_INLINE static
#endif

#ifndef MINICONV_PUBLIC
	#define MINICONV_PUBLIC
#endif

/* If your platform has it, include <stdint.h> here */
/* #include <stdint.h> */

#if defined(UINT16_MAX) && defined(UINT32_MAX)

	typedef uint16_t miniconv_uint16_t;
	typedef uint32_t miniconv_uint32_t;

#else

	typedef unsigned short miniconv_uint16_t;
	typedef char miniconv_verify_uint16_t_size[sizeof(miniconv_uint16_t) == 2 ? 1 : -1];

	typedef unsigned int miniconv_uint32_t;
	typedef char miniconv_verify_uint32_t_size[sizeof(miniconv_uint32_t) == 4 ? 1 : -1];

#endif

#endif
