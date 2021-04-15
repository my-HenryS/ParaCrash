/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of h5check. The full h5check copyright notice,          *
 * including terms governing use, modification, and redistribution, is       *
 * contained in the file COPYING, which can be found at the root of the      *
 * source code distribution tree.  If you do not have access to this file,   *
 * you may request a copy from help@hdfgroup.org.                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * This header file contains information required for testing.
 */

#ifndef TESTGEN_H
#define TESTGEN_H

/*
 * Include required headers.
 */
#define H5_USE_16_API		/* Use v16 API even if an v18 library is used. */

#include "hdf5.h"
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

/* Determine the version of the HDF5 library I am using. */
#if H5_VERS_MAJOR <= 1 && H5_VERS_MINOR < 8
#define H5_LIBVERSION 16
#else
#define H5_LIBVERSION 18
#endif

#ifndef TRUE
#define TRUE    1
#endif  /* !TRUE */

#ifndef FALSE
#define FALSE   (!TRUE)
#endif  /* !FALSE */

#define SUCCEED         0
#define FAIL            (-1)
#define UFAIL           (unsigned)(-1)

#define VERBOSE 0

#define MAX(a, b)  ((a)>(b))?a:b
#define MIN(a, b)  ((a)<(b))?a:b



/*
 * A macro to portably increment enumerated types.
 */
#ifndef H5_INC_ENUM
#  define H5_INC_ENUM(TYPE,VAR) (VAR)=((TYPE)((VAR)+1))
#endif




/* Define some handy debugging shorthands, routines, ... */
/* debugging tools */

#define MESG(x)                                                         \
	if (VERBOSE) printf("%s\n", x);                                 \

#define VRFY(val, mesg) do {                                            \
    if (val) {                                                          \
        if (*mesg != '\0') {                                            \
            MESG(mesg);                                                 \
        }                                                               \
    } else {                                                            \
        printf("*** HDF5 ERROR ***\n");                                \
        printf("        Assertion (%s) failed at line %4d in %s\n",     \
               mesg, (int)__LINE__, __FILE__);                          \
        ++nerrors;                                                      \
        fflush(stdout);                                                 \
        if (!VERBOSE) {                                                 \
            printf("aborting process\n");                           \
            exit(nerrors);                                              \
        }                                                               \
    }                                                                   \
} while(0)

/*
 * Checking for information purpose.
 * If val is false, print mesg; else nothing.
 * Either case, no error setting.
 */
#define INFO(val, mesg) do {                                            \
    if (val) {                                                          \
        if (*mesg != '\0') {                                            \
            MESG(mesg);                                                 \
        }                                                               \
    } else {                                                            \
        printf("*** HDF5 REMARK (not an error) ***\n");                \
        printf("        Condition (%s) failed at line %4d in %s\n",     \
               mesg, (int)__LINE__, __FILE__);                          \
        fflush(stdout);                                                 \
    }                                                                   \
} while(0)


/* End of Define some handy debugging shorthands, routines, ... */

/*
 * Does the compiler support the __attribute__(()) syntax?  This is how gcc
 * suppresses warnings about unused function arguments.  It's no big deal if
 * we don't.
 */
#ifdef __cplusplus
#   define __attribute__(X)     /*void*/
#   define UNUSED               /*void*/
#else /* __cplusplus */
#ifdef H5_HAVE_ATTRIBUTE
#   define UNUSED               __attribute__((unused))
#else
#   define __attribute__(X)     /*void*/
#   define UNUSED               /*void*/
#endif
#endif /* __cplusplus */



#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes for the test routines */

#ifdef __cplusplus
}
#endif

extern int nerrors;

#endif /* TESTGEN_H */
