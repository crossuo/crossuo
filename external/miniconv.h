/*
	Minimal iconv
	Copyright (c) 2005-2012 Mark H. P. Lord. All rights reserved.

	See README for a list of supported encodings.
*/

#ifndef MINICONV_MINICONV_H
#define MINICONV_MINICONV_H

#include "miniconv_config.h"

#define MINICONV_VERSION 0x103

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque type used to represent a miniconv context. */
typedef size_t miniconv_t;

/** Create a miniconv context to encode from the specified encoding name in
	to the specified encoding name.
	@return Returns (miniconv_t) -1 on failure. */
MINICONV_PUBLIC
miniconv_t miniconv_open(const char *toenc, const char *fromenc);

/** Create a miniconv context to encode from the specified encoding name in
	to the specified encoding name. Differs from miniconv_open() in that
	instead of using errno, any error code is written to *error.
	@return Returns (miniconv_t) -1 on failure. */
MINICONV_PUBLIC
miniconv_t miniconv_open2(const char *toenc, const char *fromenc, int *error);

/** Converts as many characters as possible from inbuf and writes the
	converted characters in to outbuf, updating inbytesleft and outbytesleft
	as conversion proceeds.
	@return Returns the number of non-reversible transformations that
	occurred, or (size_t) -1 on failure. */
MINICONV_PUBLIC
size_t miniconv(miniconv_t cd, const char **inbuf, size_t *inbytesleft,
	char **outbuf, size_t *outbytesleft);
	
/** Converts as many characters as possible from inbuf and writes the
	converted characters in to outbuf, updating inbytesleft and outbytesleft
	as conversion proceeds. Differs from miniconv() in that instead of using
	errno, any error code is written to *error.
	@return Returns the number of non-reversible transformations that
	occurred, or (size_t) -1 on failure. */
MINICONV_PUBLIC
size_t miniconv2(miniconv_t cd, const char **inbuf, size_t *inbytesleft,
	char **outbuf, size_t *outbytesleft, int *error);

/** Free all memory associated with a miniconv context. */
MINICONV_PUBLIC
int miniconv_close(miniconv_t cd);

/* Define MINICONV_BE_ICONV to allow miniconv to be used as a substitute for
   iconv. */
#ifdef MINICONV_BE_ICONV
	#define iconv_open miniconv_open
	#define iconv_close miniconv_close
	#define iconv miniconv

	typedef miniconv_t iconv_t;
#endif

#ifdef __cplusplus
}
#endif

#endif
