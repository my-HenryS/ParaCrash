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

typedef enum primary_err_t {
	ERR_NONE_PRIM	= 0,
        ERR_LEV_0,
        ERR_LEV_1,
        ERR_LEV_2,
        ERR_FILE,
	ERR_INTERNAL
} primary_err_t;


typedef enum secondary_err_t {
	ERR_NONE_SEC 	= 0,
        ERR_LEV_0A,
        ERR_LEV_0B,
        ERR_LEV_0C,
        ERR_LEV_1A1,
        ERR_LEV_1A2,
        ERR_LEV_1B,
        ERR_LEV_1C,
        ERR_LEV_1D,
        ERR_LEV_1E,
        ERR_LEV_1F,
        ERR_LEV_1G,
        ERR_LEV_1H,
        ERR_LEV_2A,
        ERR_LEV_2A1a,
        ERR_LEV_2A1b,
        ERR_LEV_2A2,	/* Shared Message */
        ERR_LEV_2A2a,	/* NIL */
        ERR_LEV_2A2b,	/* Dataspace */
        ERR_LEV_2A2c,	/* Link Info */
        ERR_LEV_2A2d,	/* Datatype */
        ERR_LEV_2A2e,	/* Data Storage-Fill Value(Old) */
        ERR_LEV_2A2f,	/* Data Storage-FIll Value */
        ERR_LEV_2A2g,	/* Link Message */
        ERR_LEV_2A2h,	/* Data Storage-External Data Files */
        ERR_LEV_2A2i,	/* Data Storage-Layout */
        ERR_LEV_2A2j,	/* Reserved-not assigned yet */
        ERR_LEV_2A2k,	/* Group Info */
        ERR_LEV_2A2l,	/* Data Storage-Filter Pipeline */
        ERR_LEV_2A2m,	/* Attribute */
        ERR_LEV_2A2n,	/* Object COmment */
        ERR_LEV_2A2o,	/* Object Modification Time (Old) */
        ERR_LEV_2A2p,	/* Shared Message Table */
        ERR_LEV_2A2q,	/* Object Header Continuation */
        ERR_LEV_2A2r,	/* Symbol Table */
        ERR_LEV_2A2s,	/* Object Modification Time */
        ERR_LEV_2A2t,	/* B-tree 'K' Values */
        ERR_LEV_2A2u,	/* Driver Info */
        ERR_LEV_2A2v,	/* Attibute Info" */
	ERR_LEV_2A2w,	/* Reference Count */
        ERR_LEV_2B	/* Data Object Data Storage */
} secondary_err_t;	

typedef struct   primary_err_mesg_t {
        primary_err_t   err_code;
        const   char    *str;
} primary_err_mesg_t;

typedef struct   secondary_err_mesg_t {
        secondary_err_t err_code;
        const   char    *str;
} secondary_err_mesg_t;

/* mainly used to report the version number decoded */
typedef struct	err_rep_t {
	int	reported;	/* whether to report the "bad_info" */
	int	badinfo;
} err_rep_t;

/* Information about an error */
typedef struct  error_t {
        primary_err_t   prim_err;	/* Primary Format Level where error is found */
        secondary_err_t sec_err;	/* Secondary Format Level where error is found */
        const char      *desc;		/* Detail description of error */
    	ck_addr_t	logical_addr;  	/* logical address where error occurs */
	const char	*fname;		/* file name where errors occur */
	err_rep_t	err_info;	/* for reporting wrong/correct version info */
} error_t;

#define	REPORTED	1
#define	NOT_REP		0
#define H5E_NSLOTS      32      /* number of slots in an error stack */

/* An error stack */
typedef struct ERR_t {
    int nused;                  	/* num slots currently used in stack  */
    error_t slot[H5E_NSLOTS];       /* array of error records */
} ERR_t;

/* the current error stack */
ERR_t	ERR_stack_g[1];

#define	ERR_get_my_stack()	(ERR_stack_g+0)

/* Function prototypes */
void	error_push(primary_err_t, secondary_err_t, const char *, ck_addr_t,
	    int *);
ck_err_t error_clear(void);
void 	error_print(FILE *, driver_t *);
int 	found_error(void);
void	process_errors(ck_errmsg_t *errbuf);
