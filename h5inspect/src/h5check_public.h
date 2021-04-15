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

typedef unsigned long long ck_addr_t;
typedef unsigned int ck_bool_t;
typedef int ck_err_t;

#define CK_ADDR_UNDEF           	((ck_addr_t)(-1))
#define	NSLOTS 	32

typedef struct errmsg_t {
    const char *desc;
    ck_addr_t addr;
}errmsg_t;

typedef struct ck_errmsg_t {
    int	nused;
    errmsg_t slot[NSLOTS];
} ck_errmsg_t;

ck_err_t h5checker_obj(char *, ck_addr_t, int, ck_errmsg_t *);
