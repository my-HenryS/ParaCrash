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

#include "h5chk_config.h"
#include "h5check_public.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define FORMAT_ONE_SIX 16   /* library release version 1.6.6 */
#define FORMAT_ONE_EIGHT 18 /* library release version 1.8 */
#define DEFAULT_FORMAT FORMAT_ONE_EIGHT

/* exit status */
#define EXIT_COMMAND_SUCCESS 0
#define EXIT_COMMAND_FAILURE 1
#define EXIT_FORMAT_FAILURE 2

/* command line options */
#define DEFAULT_VERBOSE 1
#define TERSE_VERBOSE 0
#define DEBUG_VERBOSE 2

/* release version of h5checker: Major and Minor version numbers are
 * based on the Format Specification version */
#define H5Check_MAJOR 2
#define H5Check_MINOR 0
#define H5Check_RELEASE 1
#define H5Check_VERSION "H5Check Version 2.0 Release 1, August, 2011"

#define CK_ADDR_MAX (CK_ADDR_UNDEF - 1)
#define addr_defined(X) (X != CK_ADDR_UNDEF)
#define addr_eq(X, Y) ((X) != CK_ADDR_UNDEF && (X) == (Y))

/* see H5public.h for definition of ck_size_t, H5pubconf.h */
typedef size_t ck_size_t;
typedef unsigned long long ck_hsize_t;

#define long_long long long

#define SUCCEED 0
#define FAIL (-1)
#define UFAIL (unsigned)(-1)
#define UNUSED /*void*/
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* For handling objects */
typedef struct stat_info_t
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
} stat_info_t;

typedef struct obj_t
{
    union {
        ck_addr_t addr;
        stat_info_t stat;
    } u;
} obj_t;

typedef struct table_t
{
    size_t size;
    size_t nobjs;
    obj_t *objs;
} table_t;

/* for handling names */
typedef struct name_t
{
    char *name;
    struct name_t *next;
} name_t;

typedef struct name_list_t
{
    name_t *head;
    name_t *tail;
} name_list_t;

#define CK_SET_RET_CONTINUE(ret_val) \
    {                                \
        ret_value = ret_val;         \
        continue;                    \
    }
#define CK_SET_RET(ret_val)  \
    {                        \
        ret_value = ret_val; \
    }
#define CK_SET_RET_DONE(ret_val) \
    {                            \
        ret_value = ret_val;     \
        goto done;               \
    }

#define CK_INC_ERR_CONTINUE \
    {                       \
        ++ret_err;          \
        continue;           \
    }
#define CK_INC_ERR \
    {              \
        ++ret_err; \
    }
#define CK_INC_ERR_DONE \
    {                   \
        ++ret_err;      \
        goto done;      \
    }

#define NELMTS(X) (sizeof(X) / sizeof(X[0]))
#define CK_ALIGN(X) (8 * (((X) + 8 - 1) / 8))

/*
 * Super block
 */
#define SUPERBLOCK_VERSION_0 0
#define SUPERBLOCK_VERSION_1 1                         /* Version with non-default B-tree 'K' value */
#define SUPERBLOCK_VERSION_2 2                         /* Revised version with superblock extension and checksum */
#define SUPERBLOCK_VERSION_3 3                         /* The field “File Consistency Flags” is used for file locking */
#define SUPERBLOCK_VERSION_LATEST SUPERBLOCK_VERSION_3 /* The maximum super block format */

#define LOGI_SUPER_BASE 0 /* logical base address of super block */

#define HDF_SIGNATURE "\211HDF\r\n\032\n"
#define HDF_SIGNATURE_LEN 8
#define MAX_SUPERBLOCK_SIZE 134
#define SUPERBLOCK_FIXED_SIZE (HDF_SIGNATURE_LEN + 1)

#define SIZEOF_ADDR(FS) ((FS)->size_offsets)
#define SIZEOF_SIZE(FS) ((FS)->size_lengths)
#define SYM_LEAF_K(FS) ((FS)->gr_leaf_node_k)
#define CRT_SYM_LEAF_DEF 4

#define FREESPACE_VERSION 0    /* of the Free-Space Info         */
#define OBJECTDIR_VERSION 0    /* of the Object Directory format */
#define SHAREDHEADER_VERSION 0 /* of the Shared-Header Info      */
#define DRIVERINFO_VERSION 0   /* of the Driver Information Block*/

#define SIZEOF_CHKSUM 4 /* Checksum size in the file */

#define GP_SIZEOF_SCRATCH 16
#define GP_SIZEOF_ENTRY(F)                          \
    (SIZEOF_SIZE(F) +   /* name offset */           \
     SIZEOF_ADDR(F) +   /* object header address */ \
     4 +                /* cache type */            \
     4 +                /* reserved   */            \
     GP_SIZEOF_SCRATCH) /* scratch pad space */

#define SUPERBLOCK_VARLEN_SIZE_COMMON                                \
    (2    /* freespace, and root group versions */                   \
     + 1  /* reserved */                                             \
     + 3  /* shared header vers, size of address, size of lengths */ \
     + 1  /* reserved */                                             \
     + 4  /* group leaf k, group internal k */                       \
     + 4) /* consistency flags */

#define SUPERBLOCK_VARLEN_SIZE_V0 SUPERBLOCK_VARLEN_SIZE_COMMON

#define SUPERBLOCK_VARLEN_SIZE_V1                                      \
    (SUPERBLOCK_VARLEN_SIZE_COMMON + 2 /* indexed B-tree internal k */ \
     + 2 /* reserved */)

#define SUPERBLOCK_VARLEN_SIZE_V2             \
    (2 /* size of address, size of lengths */ \
     + 1 /* consistency flags */)

#define SUPERBLOCK_VARLEN_SIZE_V3 SUPERBLOCK_VARLEN_SIZE_V2

#define SUPERBLOCK_VARLEN_SIZE(v) \
    ((v == 0 ? SUPERBLOCK_VARLEN_SIZE_V0 : 0) + (v == 1 ? SUPERBLOCK_VARLEN_SIZE_V1 : 0) + (v == 2 ? SUPERBLOCK_VARLEN_SIZE_V2 : 0) + (v == 3 ? SUPERBLOCK_VARLEN_SIZE_V3 : 0))

#define SUPERBLOCK_REMAIN_SIZE_COMMON(fs)              \
    (SIZEOF_ADDR(fs)        /* base address */         \
     + SIZEOF_ADDR(fs)      /* <unused> */             \
     + SIZEOF_ADDR(fs)      /* EOF address */          \
     + SIZEOF_ADDR(fs)      /* driver block address */ \
     + GP_SIZEOF_ENTRY(fs)) /* root group ptr */

#define SUPERBLOCK_REMAIN_SIZE_V0(fs) SUPERBLOCK_REMAIN_SIZE_COMMON(fs)
#define SUPERBLOCK_REMAIN_SIZE_V1(fs) SUPERBLOCK_REMAIN_SIZE_COMMON(fs)
#define SUPERBLOCK_REMAIN_SIZE_V2(fs)                         \
    (SIZEOF_ADDR(fs)   /* base address */                     \
     + SIZEOF_ADDR(fs) /* superblock extension address */     \
     + SIZEOF_ADDR(fs) /* EOF address */                      \
     + SIZEOF_ADDR(fs) /* root group object header address */ \
     + SIZEOF_CHKSUM)  /* superblock checksum  */
#define SUPERBLOCK_REMAIN_SIZE_V3(fs) SUPERBLOCK_REMAIN_SIZE_V2(fs)

#define SUPERBLOCK_REMAIN_SIZE(v, fs)                                                              \
    ((v == 0 ? SUPERBLOCK_REMAIN_SIZE_V0(fs) : 0) + (v == 1 ? SUPERBLOCK_REMAIN_SIZE_V1(fs) : 0) + \
     (v == 2 ? SUPERBLOCK_REMAIN_SIZE_V2(fs) : 0) + (v == 3 ? SUPERBLOCK_REMAIN_SIZE_V3(fs) : 0))

#define DRVINFOBLOCK_SIZE 1024
#define DRVINFOBLOCK_HDR_SIZE 16

#define SUPER_WRITE_ACCESS 0x01
#define SUPER_FILE_OK 0x02
#define SUPER_SWMR_ACCESS 0x04
#define SUPER_ALL_FLAGS (SUPER_WRITE_ACCESS | SUPER_FILE_OK | SUPER_SWMR_ACCESS)

#define UINT16DECODE(p, i)                   \
    {                                        \
        (i) = (uint16_t)(*(p)&0xff);         \
        (p)++;                               \
        (i) |= (uint16_t)((*(p)&0xff) << 8); \
        (p)++;                               \
    }

#define UINT32DECODE(p, i)                    \
    {                                         \
        (i) = (uint32_t)(*(p)&0xff);          \
        (p)++;                                \
        (i) |= ((uint32_t)(*(p)&0xff) << 8);  \
        (p)++;                                \
        (i) |= ((uint32_t)(*(p)&0xff) << 16); \
        (p)++;                                \
        (i) |= ((uint32_t)(*(p)&0xff) << 24); \
        (p)++;                                \
    }

#define UINT32DECODE_VAR(p, n, l)  \
    {                              \
        size_t _i;                 \
                                   \
        n = 0;                     \
        (p) += l;                  \
        for (_i = 0; _i < l; _i++) \
            n = (n << 8) | *(--p); \
        (p) += l;                  \
    }

#define INT64DECODE(p, n)                        \
    {                                            \
        /* WE DON'T CHECK FOR OVERFLOW! */       \
        size_t _i;                               \
                                                 \
        n = 0;                                   \
        (p) += 8;                                \
        for (_i = 0; _i < sizeof(int64_t); _i++) \
            n = (n << 8) | *(--p);               \
        (p) += 8;                                \
    }

#define UINT64DECODE(p, n)                        \
    {                                             \
        /* WE DON'T CHECK FOR OVERFLOW! */        \
        size_t _i;                                \
        n = 0;                                    \
        (p) += 8;                                 \
        for (_i = 0; _i < sizeof(uint64_t); _i++) \
        {                                         \
            n = (n << 8) | *(--p);                \
        }                                         \
        (p) += 8;                                 \
    }

#define UINT64DECODE_VAR(p, n, l)  \
    {                              \
        size_t _i;                 \
                                   \
        n = 0;                     \
        (p) += l;                  \
        for (_i = 0; _i < l; _i++) \
            n = (n << 8) | *(--p); \
        (p) += l;                  \
    }

#define DECODE_LENGTH(F, p, l) \
    switch (SIZEOF_SIZE(F))    \
    {                          \
    case 4:                    \
        UINT32DECODE(p, l);    \
        break;                 \
    case 8:                    \
        UINT64DECODE(p, l);    \
        break;                 \
    case 2:                    \
        UINT16DECODE(p, l);    \
        break;                 \
    }

#define INT32DECODE(p, i)                                      \
    {                                                          \
        (i) = (*(p)&0xff);                                     \
        (p)++;                                                 \
        (i) |= ((int32_t)(*(p)&0xff) << 8);                    \
        (p)++;                                                 \
        (i) |= ((int32_t)(*(p)&0xff) << 16);                   \
        (p)++;                                                 \
        (i) |= ((int32_t)(((*(p)&0xff) << 24) |                \
                          ((*(p)&0x80) ? ~0xffffffff : 0x0))); \
        (p)++;                                                 \
    }

#define CHECK_OVERFLOW(var, vartype, casttype)    \
    {                                             \
        casttype _tmp_overflow = (casttype)(var); \
        assert((var) == (vartype)_tmp_overflow);  \
    }

#define BT_SNODE_K 16
#define BT_ISTORE_K 32 /* for older version of superblock < 1 */

/*
 * symbol table
 */
#define SNODE_MAGIC "SNOD"   /*symbol table node magic number     */
#define SNODE_SIZEOF_MAGIC 4 /*sizeof symbol node magic number    */
#define SNODE_VERS 1         /*symbol table node version number   */
#define SNODE_SIZEOF_HDR(F) (SNODE_SIZEOF_MAGIC + 4)

typedef enum GP_type_t
{
    GP_CACHED_ERROR = -1,  /*force enum to be signed       */
    GP_NOTHING_CACHED = 0, /*nothing is cached, must be 0  */
    GP_CACHED_STAB = 1,    /*symbol table, `stab'          */
    GP_CACHED_SLINK = 2,   /*symbolic link                 */
    GP_NCACHED = 3         /*THIS MUST BE LAST             */
} GP_type_t;

typedef union GP_cache_t {
    struct
    {
        ck_addr_t btree_addr; /* file address of symbol table B-tree */
        ck_addr_t heap_addr;  /* file address of stab name heap      */
    } stab;

    struct
    {
        size_t lval_offset; /* link value offset              */
    } slink;
} GP_cache_t;

typedef struct GP_entry_t
{
    GP_type_t type;     /* type of information cached         */
    GP_cache_t cache;   /* cached data from object header     */
    ck_size_t name_off; /* offset of name within name heap    */
    ck_addr_t header;   /* file address of object header      */
} GP_entry_t;

typedef struct GP_node_t
{
    unsigned nsyms;    /*number of symbols                  */
    GP_entry_t *entry; /*array of symbol table entries      */
} GP_node_t;

/* forward references */
typedef struct driver_t driver_t;
typedef struct driver_class_t driver_class_t;

/* 1.6 B-tree IDs */
typedef enum BT_subid_t
{
    BT_SNODE_ID = 0,  /* B-tree is for symbol table nodes        	*/
    BT_ISTORE_ID = 1, /* B-tree is for indexed object storage       	*/
    BT_NUM_BTREE_ID   /* Number of B-tree key IDs (must be last)	*/
} BT_subid_t;

/*
 *  A global structure for storing the information obtained
 *  from the superblock to be shared by all routines.
 */
typedef struct global_shared_t
{
    ck_addr_t super_addr;              /* absolute address of the super block */
    size_t size_offsets;               /* size of offsets: sizeof_addr */
    size_t size_lengths;               /* size of lengths: sizeof_size */
    unsigned gr_leaf_node_k;           /* group leaf node k */
    uint32_t file_consist_flg;         /* file consistency flags */
    unsigned btree_k[BT_NUM_BTREE_ID]; /* internal node k for SNODE & ISTORE */
    ck_addr_t base_addr;               /* absolute base address for rel.addrs. */
    ck_addr_t freespace_addr;          /* relative address of free-space info  */
    ck_addr_t stored_eoa;              /* end of file address */
    ck_addr_t driver_addr;             /* file driver information block address*/
    GP_entry_t *root_grp;              /* pointer to root group symbol table entry */
    ck_addr_t extension_addr;          /* 1.8: extension address */
    int driverid;                      /* the driver id to be used */
    void *sohm_tbl;                    /* address of the master table of shared messages */
    void *fa;                          /* driver specific info */
    table_t *obj_table;                /* Table for handling hard links */
    char *extpath;                     /* Path for searching target external linked file */
} global_shared_t;

/*
 * Object Headers
 */

/* Object Header Message IDs */
#define OBJ_NIL_ID 0x0000      /* NIL 				  */
#define OBJ_SDS_ID 0x0001      /* Simple Dataspace  			  */
#define OBJ_LINFO_ID 0x0002    /* Link info Message. 			  */
#define OBJ_DT_ID 0x0003       /* Datatype   				  */
#define OBJ_FILL_OLD_ID 0x0004 /* Data Storage - Fill Value (Old)  	  */
#define OBJ_FILL_ID 0x0005     /* Data Storage - Fill Value 		  */
#define OBJ_LINK_ID 0x0006     /* Link Message. */
#define OBJ_EDF_ID 0x0007      /* Data Storage - External Data Files 	  */
#define OBJ_LAYOUT_ID 0x0008   /* Data Storage - Layout 		  */
#define OBJ_BOGUS_ID 0x0009    /* Bogus Message.  			  */
#define OBJ_GINFO_ID 0x000a    /* Group info Message.  		  */
#define OBJ_FILTER_ID 0x000b   /* Data Storage - Filter pipeline  	  */
#define OBJ_ATTR_ID 0x000c     /* Attribute 				  */
#define OBJ_COMM_ID 0x000d     /* Object Comment 			  */
#define OBJ_MDT_OLD_ID 0x000e  /* Object Modification Date & time (Old) */
#define OBJ_SHMESG_ID 0x000f   /* Shared message "SOHM" table. */
#define OBJ_CONT_ID 0x0010     /* Object Header Continuation 		  */
#define OBJ_GROUP_ID 0x0011    /* Symbol Table Message		  */
#define OBJ_MDT_ID 0x0012      /* Object Modification Date & Time 	  */
#define OBJ_BTREEK_ID 0x0013   /* v1 B-tree 'K' values message.         */
#define OBJ_DRVINFO_ID 0x0014  /* Driver info message.                  */
#define OBJ_AINFO_ID 0x0015    /* Attribute info message.  		  */
#define OBJ_REFCOUNT_ID 0x0016 /* Reference count message.  		  */
#define OBJ_UNKNOWN_ID 0x0017  /* Placeholder message ID for unknown message.  */

/* 
 * Simple Dataspace 
 */
#define OBJ_SDS_MAX_RANK 32

typedef enum OBJ_sds_class_t
{
    OBJ_SDS_NO_CLASS = -1, /*error                                      */
    OBJ_SDS_SCALAR = 0,    /*scalar variable                            */
    OBJ_SDS_SIMPLE = 1,    /*simple data space                          */
    OBJ_SDS_NULL = 2       /* 1.8 null data space 			 */
                           /* 1.6 OBJ_SDS_COMPLEX: not supported yet 	 */
} OBJ_sds_class_t;

#define OBJ_SDS_VERSION_1 1
#define OBJ_SDS_VERSION_2 2
#define OBJ_SDS_VERSION_LATEST OBJ_SDS_VERSION_2
#define OBJ_SDS_VALID_MAX 0x01

typedef struct OBJ_sds_extent_t
{
    OBJ_sds_class_t type; /* Type of extent */
    ck_hsize_t nelem;     /* Number of elements in extent */
    unsigned rank;        /* Number of dimensions */
    ck_hsize_t *size;     /* Current size of the dimensions */
    ck_hsize_t *max;      /* Maximum size of the dimensions */
} OBJ_sds_extent_t;

/*
 * Link Information Message 
 */
#define OBJ_LINFO_VERSION 0
#define OBJ_LINFO_TRACK_CORDER 0x01
#define OBJ_LINFO_INDEX_CORDER 0x02
#define OBJ_LINFO_ALL_FLAGS (OBJ_LINFO_TRACK_CORDER | OBJ_LINFO_INDEX_CORDER)

typedef struct OBJ_linfo_t
{
    ck_bool_t track_corder;    /* Are creation order values tracked on links? */
    ck_bool_t index_corder;    /* Are creation order values indexed on links? */
    int64_t max_corder;        /* Current max. creation order value for group */
    ck_addr_t corder_bt2_addr; /* Address of v2 B-tree for indexing creation order values of links */
                               /* ??? DO I NEED THIS FIELD */
    ck_size_t nlinks;          /* Number of links in the group      */
    ck_addr_t fheap_addr;      /* Address of fractal heap for storing "dense" links */
    ck_addr_t name_bt2_addr;   /* Address of v2 B-tree for indexing names of links */
} OBJ_linfo_t;

/* Datatype Message */

#define DT_VERSION_1 1
#define DT_VERSION_2 2
#define DT_VERSION_3 3
#define DT_VERSION_LATEST DT_VERSION_3

#define DT_OPAQUE_TAG_MAX 256

typedef enum DT_order_t
{
    DT_ORDER_ERROR = -1, /*error                                      */
    DT_ORDER_LE = 0,     /*little endian                              */
    DT_ORDER_BE = 1,     /*bit endian                                 */
    DT_ORDER_VAX = 2,    /*VAX mixed endian                           */
    DT_ORDER_NONE = 3    /*no particular order (strings, bits,..)     */
    /*DT_ORDER_NONE must be last */
} DT_order_t;

typedef enum DT_sign_t
{
    DT_SGN_ERROR = -1, /*error                                      */
    DT_SGN_NONE = 0,   /*this is an unsigned type                   */
    DT_SGN_2 = 1,      /*two's complement                           */
    DT_NSGN = 2        /*this must be last!                         */
} DT_sign_t;

typedef enum DT_norm_t
{
    DT_NORM_ERROR = -1,  /*error                                      */
    DT_NORM_IMPLIED = 0, /*msb of mantissa isn't stored, always 1     */
    DT_NORM_MSBSET = 1,  /*msb of mantissa is always 1                */
    DT_NORM_NONE = 2     /*not normalized                             */
                         /*DT_NORM_NONE must be last */
} DT_norm_t;

typedef enum DT_pad_t
{
    DT_PAD_ERROR = -1,     /*error                                      */
    DT_PAD_ZERO = 0,       /*always set to zero                         */
    DT_PAD_ONE = 1,        /*always set to one                          */
    DT_PAD_BACKGROUND = 2, /*set to background value                    */
    DT_NPAD = 3            /*THIS MUST BE LAST                          */
} DT_pad_t;

typedef enum DT_cset_t
{
    DT_CSET_ERROR = -1,       /*error                                      */
    DT_CSET_ASCII = 0,        /*US ASCII                                   */
    DT_CSET_UTF8 = 1,         /*UTF-8 Unicode encoding                     */
    DT_CSET_RESERVED_2 = 2,   /*reserved for later use                     */
    DT_CSET_RESERVED_3 = 3,   /*reserved for later use                     */
    DT_CSET_RESERVED_4 = 4,   /*reserved for later use                     */
    DT_CSET_RESERVED_5 = 5,   /*reserved for later use                     */
    DT_CSET_RESERVED_6 = 6,   /*reserved for later use                     */
    DT_CSET_RESERVED_7 = 7,   /*reserved for later use                     */
    DT_CSET_RESERVED_8 = 8,   /*reserved for later use                     */
    DT_CSET_RESERVED_9 = 9,   /*reserved for later use                     */
    DT_CSET_RESERVED_10 = 10, /*reserved for later use                     */
    DT_CSET_RESERVED_11 = 11, /*reserved for later use                     */
    DT_CSET_RESERVED_12 = 12, /*reserved for later use                     */
    DT_CSET_RESERVED_13 = 13, /*reserved for later use                     */
    DT_CSET_RESERVED_14 = 14, /*reserved for later use                     */
    DT_CSET_RESERVED_15 = 15  /*reserved for later use                     */
} DT_cset_t;

typedef enum DT_str_t
{
    DT_STR_ERROR = -1,       /*error                                      */
    DT_STR_NULLTERM = 0,     /*null terminate like in C                   */
    DT_STR_NULLPAD = 1,      /*pad with nulls                             */
    DT_STR_SPACEPAD = 2,     /*pad with spaces like in Fortran            */
    DT_STR_RESERVED_3 = 3,   /*reserved for later use                     */
    DT_STR_RESERVED_4 = 4,   /*reserved for later use                     */
    DT_STR_RESERVED_5 = 5,   /*reserved for later use                     */
    DT_STR_RESERVED_6 = 6,   /*reserved for later use                     */
    DT_STR_RESERVED_7 = 7,   /*reserved for later use                     */
    DT_STR_RESERVED_8 = 8,   /*reserved for later use                     */
    DT_STR_RESERVED_9 = 9,   /*reserved for later use                     */
    DT_STR_RESERVED_10 = 10, /*reserved for later use                     */
    DT_STR_RESERVED_11 = 11, /*reserved for later use                     */
    DT_STR_RESERVED_12 = 12, /*reserved for later use                     */
    DT_STR_RESERVED_13 = 13, /*reserved for later use                     */
    DT_STR_RESERVED_14 = 14, /*reserved for later use                     */
    DT_STR_RESERVED_15 = 15  /*reserved for later use                     */
} DT_str_t;

typedef enum
{
    DTR_BADTYPE = (-1), /*invalid Reference Type                     */
    DTR_OBJECT,         /*Object reference                           */
    DTR_DATASET_REGION, /*Dataset Region Reference                   */
    DTR_INTERNAL,       /*Internal Reference                         */
    DTR_MAXTYPE         /*highest type (Invalid as true type)        */
} DTR_type_t;

typedef enum DT_class_t
{
    DT_NO_CLASS = -1, /* error                                      */
    DT_INTEGER = 0,   /* integer types                              */
    DT_FLOAT = 1,     /* floating-point types                       */
    DT_TIME = 2,      /* date and time types                        */
    DT_STRING = 3,    /* character string types                     */
    DT_BITFIELD = 4,  /* bit field types                            */
    DT_OPAQUE = 5,    /* opaque types                               */
    DT_COMPOUND = 6,  /* compound types                             */
    DT_REFERENCE = 7, /* reference types                            */
    DT_ENUM = 8,      /* enumeration types                          */
    DT_VLEN = 9,      /* variable-Length types                      */
    DT_ARRAY = 10,    /* array types                                */
    DT_NCLASSES       /* this must be last                          */
} DT_class_t;

/* Forward declaration */
typedef struct OBJ_type_t OBJ_type_t;

typedef struct DT_atomic_t
{
    DT_order_t order; /*byte order                                 */
    size_t prec;      /*precision in bits                          */
    size_t offset;    /*bit position of lsb of value               */
    DT_pad_t lsb_pad; /*type of lsb padding                       */
    DT_pad_t msb_pad; /*type of msb padding                       */
    union {
        struct
        {
            DT_sign_t sign; /*type of integer sign                      */
        } i;                /*integer; integer types                    */

        struct
        {
            size_t sign;    /*bit position of sign bit                   */
            size_t epos;    /*position of lsb of exponent                */
            size_t esize;   /*size of exponent in bits                   */
            uint64_t ebias; /*exponent bias                              */
            size_t mpos;    /*position of lsb of mantissa                */
            size_t msize;   /*size of mantissa                           */
            DT_norm_t norm; /*normalization                              */
            DT_pad_t pad;   /*type of padding for internal bits          */
        } f;                /*floating-point types                       */

        struct
        {
            DT_cset_t cset; /*character set                              */
            DT_str_t pad;   /*space or null padding of extra bytes       */
        } s;                /*string types                               */

        struct
        {
            DTR_type_t rtype; /*type of reference stored                   */
        } r;                  /*reference types                            */
    } u;
} DT_atomic_t;

typedef enum DT_sort_t
{
    DT_SORT_NONE = 0, /*not sorted                         */
    DT_SORT_NAME = 1, /*sorted by member name              */
    DT_SORT_VALUE = 2 /*sorted by memb offset or enum value*/
} DT_sort_t;

typedef struct DT_enum_t
{
    unsigned nalloc;  /*num entries allocated              */
    unsigned nmembs;  /*number of members defined in enum  */
    DT_sort_t sorted; /*how are members sorted?            */
    uint8_t *value;   /*array of values                    */
    char **name;      /*array of symbol names              */
} DT_enum_t;

typedef struct DT_cmemb_t
{
    char *name;              /*name of this member                	*/
    size_t offset;           /*offset from beginning of struct    	*/
    size_t size;             /*total size: dims * type_size       	*/
    struct OBJ_type_t *type; /*type of this member        		*/
} DT_cmemb_t;

typedef struct DT_compnd_t
{
    unsigned nalloc;         /*num entries allocated in MEMB array*/
    unsigned nmembs;         /*number of members defined in struct*/
    DT_sort_t sorted;        /*how are members sorted?            */
    ck_bool_t packed;        /*are members packed together?       */
    struct DT_cmemb_t *memb; /*array of struct members            */
} DT_compnd_t;

typedef enum
{
    DT_VLEN_BADTYPE = -1, /* invalid VL Type */
    DT_VLEN_SEQUENCE = 0, /* VL sequence */
    DT_VLEN_STRING,       /* VL string */
    DT_VLEN_MAXTYPE       /* highest type (Invalid as true type) */
} DT_vlen_type_t;

typedef struct DT_vlen_t
{
    DT_vlen_type_t type; /* Type of VL data in buffer */
    DT_cset_t cset;      /* For VL string. character set */
    DT_str_t pad;        /* For VL string.  space or null padding of extra bytes */
} DT_vlen_t;

typedef struct DT_opaque_t
{
    char *tag; /*short type description string      */
} DT_opaque_t;

typedef struct DT_array_t
{
    size_t nelem;                 /* total number of elements in array */
    int ndims;                    /* member dimensionality        */
    size_t dim[OBJ_SDS_MAX_RANK]; /* size in each dimension       */
    int perm[OBJ_SDS_MAX_RANK];   /* index permutation            */
} DT_array_t;

typedef struct DT_shared_t
{
    ck_size_t fo_count;        /* number of references to this file object */
    DT_class_t type;           /*which class of type is this?               */
    size_t size;               /*total size of an instance of this type     */
    struct OBJ_type_t *parent; /*parent type for derived datatypes  */
    union {
        DT_atomic_t atomic; /* an atomic datatype	*/
        DT_compnd_t compnd; /* compound datatype */
        DT_enum_t enumer;   /* enumerated datatype */
        DT_vlen_t vlen;     /* variable length datatype */
        DT_array_t array;   /* an array datatype 	*/
        DT_opaque_t opaque;
    } u;
} DT_shared_t;

struct OBJ_type_t
{
    GP_entry_t ent;      /* entry information if the type is a named type */
    DT_shared_t *shared; /* all other information */
};                       /* OBJ_type_t */

#ifdef TEMP

typedef struct type_t type_t;

struct type_t
{
    GP_entry_t ent;      /* entry information if the type is a named type */
    DT_shared_t *shared; /* all other information */
};

typedef struct DT_cmemb_t
{
    char *name;          /*name of this member                */
    size_t offset;       /*offset from beginning of struct    */
    size_t size;         /*total size: dims * type_size       */
    struct type_t *type; /*type of this member                */
} DT_cmemb_t;

#endif

/* 
 * Data Storage -  Fill Value 
 */
#define OBJ_FILL_VERSION 1
#define OBJ_FILL_VERSION_2 2
#define OBJ_FILL_VERSION_3 3
#define OBJ_FILL_VERSION_LATEST OBJ_FILL_VERSION_3

#define OBJ_FILL_MASK_ALLOC_TIME 0x03
#define OBJ_FILL_SHIFT_ALLOC_TIME 0
#define OBJ_FILL_MASK_FILL_TIME 0x03
#define OBJ_FILL_SHIFT_FILL_TIME 2
#define OBJ_FILL_FLAG_UNDEFINED_VALUE 0x10
#define OBJ_FILL_FLAG_HAVE_VALUE 0x20
#define OBJ_FILL_FLAGS_ALL (OBJ_FILL_MASK_ALLOC_TIME | (OBJ_FILL_MASK_FILL_TIME << OBJ_FILL_SHIFT_FILL_TIME) | OBJ_FILL_FLAG_UNDEFINED_VALUE | OBJ_FILL_FLAG_HAVE_VALUE)

typedef enum fill_alloc_time_t
{
    FILL_ALLOC_TIME_ERROR = -1,
    FILL_ALLOC_TIME_DEFAULT = 0,
    FILL_ALLOC_TIME_EARLY = 1,
    FILL_ALLOC_TIME_LATE = 2,
    FILL_ALLOC_TIME_INCR = 3
} fill_alloc_time_t;

typedef enum fill_time_t
{
    FILL_TIME_ERROR = -1,
    FILL_TIME_ALLOC = 0,
    FILL_TIME_NEVER = 1,
    FILL_TIME_IFSET = 2
} fill_time_t;

typedef struct OBJ_fill_t
{
    unsigned version;
    ssize_t size;                 /*number of bytes in the fill value  */
    void *buf;                    /*the fill value                     */
    fill_alloc_time_t alloc_time; /* time to allocate space            */
    fill_time_t fill_time;        /* time to write fill value          */
    ck_bool_t fill_defined;       /* whether fill value is defined     */
} OBJ_fill_t;

/* 
 * Link Message 
 */

typedef enum
{
    L_TYPE_ERROR = (-1),  /* Invalid link type id         */
    L_TYPE_HARD = 0,      /* Hard link id                 */
    L_TYPE_SOFT = 1,      /* Soft link id                 */
    L_TYPE_EXTERNAL = 64, /* External link id             */
    L_TYPE_MAX = 255      /* Maximum link type id         */
} L_type_t;

#define L_TYPE_BUILTIN_MAX L_TYPE_SOFT /* Maximum value link value for "built-in" link types */
#define L_TYPE_UD_MIN L_TYPE_EXTERNAL  /* Link ids at or above this value are "user-defined" link types. */

/* Version and flags of external link format */
#define L_EXT_VERSION 0
#define L_EXT_FLAGS_ALL 0

typedef struct L_link_hard_t
{
    ck_addr_t addr; /* Object header address */
} L_link_hard_t;

typedef struct L_link_soft_t
{
    char *name; /* Destination name */
} L_link_soft_t;

typedef struct L_link_ud_t
{
    void *udata;    /* Opaque data supplied by the user */
    ck_size_t size; /* Size of udata */
} L_link_ud_t;

#define OBJ_LINK_VERSION 1

/* Flags for link flag encoding */
#define OBJ_LINK_NAME_SIZE 0x03       /* 2-bit field for size of name length */
#define OBJ_LINK_STORE_CORDER 0x04    /* Whether to store creation index */
#define OBJ_LINK_STORE_LINK_TYPE 0x08 /* Whether to store non-default link type */
#define OBJ_LINK_STORE_NAME_CSET 0x10 /* Whether to store non-default name character set */
#define OBJ_LINK_ALL_FLAGS (OBJ_LINK_NAME_SIZE | OBJ_LINK_STORE_CORDER | OBJ_LINK_STORE_LINK_TYPE | OBJ_LINK_STORE_NAME_CSET)

/* Individual definitions of name size values */
#define OBJ_LINK_NAME_1 0x00 /* Use 1-byte value for name length */
#define OBJ_LINK_NAME_2 0x01 /* Use 2-byte value for name length */
#define OBJ_LINK_NAME_4 0x02 /* Use 4-byte value for name length */

typedef struct OBJ_link_t
{
    L_type_t type;          /* Type of link */
    ck_bool_t corder_valid; /* Creation order for link is valid (not stored) */
    int64_t corder;         /* Creation order for link (stored if it's valid) */
    DT_cset_t cset;         /* Character set of link name   */
    char *name;             /* Link name */
    union {
        L_link_hard_t hard; /* Information for hard links */
        L_link_soft_t soft; /* Information for soft links */
        L_link_ud_t ud;     /* Information for user-defined links */
    } u;
} OBJ_link_t;

/* 
 * Data Storage - External Data Files
 */
#define OBJ_EDF_VERSION 1
#define OBJ_EDF_ALLOC 16                /*number of slots to alloc at once   */
#define OBJ_EDF_UNLIMITED H5F_UNLIMITED /*max possible file size       */

typedef struct OBJ_edf_entry_t
{
    size_t name_offset; /*offset of name within heap         */
    char *name;         /*malloc'd name                      */
    off_t offset;       /*offset of data within file         */
    ck_size_t size;     /*size allocated within file         */
} OBJ_edf_entry_t;

typedef struct OBJ_edf_t
{
    ck_addr_t heap_addr;   /*address of name heap               */
    size_t nalloc;         /*number of slots allocated          */
    size_t nused;          /*number of slots used               */
    OBJ_edf_entry_t *slot; /*array of external file entries     */
} OBJ_edf_t;

/*
 * Group info message.
 */
#define OBJ_GINFO_VERSION 0
#define OBJ_GINFO_ALL_FLAGS (OBJ_GINFO_STORE_PHASE_CHANGE | OBJ_GINFO_STORE_EST_ENTRY_INFO)
#define OBJ_GINFO_STORE_PHASE_CHANGE 0x01
#define OBJ_GINFO_STORE_EST_ENTRY_INFO 0x02
#define OBJ_CRT_GINFO_MAX_COMPACT 8
#define OBJ_CRT_GINFO_MIN_DENSE 6
#define OBJ_CRT_GINFO_EST_NUM_ENTRIES 4
#define OBJ_CRT_GINFO_EST_NAME_LEN 8

typedef struct OBJ_ginfo_t
{
    /* "Old" format group info (not stored) */
    uint32_t lheap_size_hint; /* Local heap size hint              */

    /* "New" format group info */
    ck_bool_t store_link_phase_change; /* Whether to store the link phase change values */
    uint16_t max_compact;              /* Maximum # of compact links        */
    uint16_t min_dense;                /* Minimum # of "dense" links        */

    ck_bool_t store_est_entry_info; /* Whether to store the est. entry values */
    uint16_t est_num_entries;       /* Estimated # of entries in group   */
    uint16_t est_name_len;          /* Estimated length of entry name    */
} OBJ_ginfo_t;

/*
 * Data Storage: layout 
 */
#define OBJ_LAYOUT_VERSION_1 1
#define OBJ_LAYOUT_VERSION_2 2
#define OBJ_LAYOUT_VERSION_3 3
#define OBJ_LAYOUT_VERSION_4 4
#define OBJ_LAYOUT_VERSION_LATEST OBJ_LAYOUT_VERSION_4
#define OBJ_LAYOUT_NDIMS (OBJ_SDS_MAX_RANK + 1)
#define OBJ_FLAG_MASK 0x03

#define OBJ_LAYOUT_CHUNK_SINGLE 1
#define OBJ_LAYOUT_CHUNK_IMPLICIT 2
#define OBJ_LAYOUT_CHUNK_FIXED 3
#define OBJ_LAYOUT_CHUNK_EXTENSIBLE 4
#define OBJ_LAYOUT_CHUNK_v2_BTREE 5
#define OBJ_LAYOUT_CHUNK_v1_BTREE 6

typedef enum DATA_layout_t
{
    DATA_LAYOUT_ERROR = -1,
    DATA_COMPACT = 0,    /*raw data is very small                     */
    DATA_CONTIGUOUS = 1, /*the default                                */
    DATA_CHUNKED = 2,    /*slow and fancy                             */
    DATA_VIRTUAL = 3     /*virtual dataset (VDS) this one must be last! */
} DATA_layout_t;

typedef struct OBJ_layout_contig_t
{
    ck_addr_t addr; /* File address of data              */
    ck_size_t size; /* Size of data in bytes             */
} OBJ_layout_contig_t;

typedef struct OBJ_layout_chunk_t
{
    ck_addr_t addr;               /* File address of B-tree            */
    unsigned ndims;               /* Num dimensions in chunk           */
    size_t dim[OBJ_LAYOUT_NDIMS]; /* Size of chunk in elements         */
    size_t size;                  /* Size of chunk in bytes            */
    unsigned flags;               /* Chunked layout feature flag       */
    unsigned index;               /* Chunked Indexing Type             */
} OBJ_layout_chunk_t;

typedef struct OBJ_layout_compact_t
{
    ck_bool_t dirty; /* Dirty flag for compact dataset    */
    size_t size;     /* Size of buffer in bytes           */
    void *buf;       /* Buffer for compact dataset        */
} OBJ_layout_compact_t;

typedef struct OBJ_layout_t
{
    DATA_layout_t type; /* Type of layout                    */
    unsigned version;   /* Version of message                */
    /* Structure for "unused" dimension information */
    struct
    {
        unsigned ndims;                  /*num dimensions in stored data      */
        ck_size_t dim[OBJ_LAYOUT_NDIMS]; /*size of data or chunk in bytes     */
    } unused;
    union {
        OBJ_layout_contig_t contig;   /* Information for contiguous layout */
        OBJ_layout_chunk_t chunk;     /* Information for chunked layout    */
        OBJ_layout_compact_t compact; /* Information for compact layout    */
    } u;
} OBJ_layout_t;

/* 
 * Bogus Message
 */
#define OBJ_BOGUS_VALUE 0xdeadbeef

typedef struct OBj_bogus_t
{
    unsigned u; /* Hold the bogus info */
} OBJ_bogus_t;

/* 
 * Data Storage: Filter Pipeline
 */
#define OBJ_FILTER_VERSION_1 1
#define OBJ_FILTER_VERSION_2 2
#define OBJ_FILTER_VERSION_LATEST OBJ_FILTER_VERSION_2
#define OBJ_MAX_NFILTERS 32

#define OBJ_FILTER_RESERVED 256 /* filter ids below this value are reserved for library use */
#define Z_COMMON_NAME_LEN 12
#define Z_COMMON_CD_VALUES 4

typedef struct
{
    int id;                                  /* filter identification number       */
    unsigned flags;                          /* defn and invocation flags          */
    char _name[Z_COMMON_NAME_LEN];           /*internal filter name  */
    char *name;                              /* optional filter name               */
    size_t cd_nelmts;                        /* number of elements in cd_values[]  */
    unsigned _cd_values[Z_COMMON_CD_VALUES]; /*internal client data values */
    unsigned *cd_values;                     /* client data values                 */
} OBJ_filter_info_t;

typedef struct OBJ_filter_t
{
    size_t nalloc;             /*num elements in `filter' array     */
    size_t nused;              /*num filters defined                */
    OBJ_filter_info_t *filter; /*array of filters                   */
} OBJ_filter_t;

/* 
 * Attribute
 */
/* This is the initial version, which does not have support for shared datatypes */
#define OBJ_ATTR_VERSION_1 1
#define OBJ_ATTR_VERSION_2 2
#define OBJ_ATTR_VERSION_3 3
#define OBJ_ATTR_VERSION_LATEST OBJ_ATTR_VERSION_3

#define OBJ_ATTR_FLAG_TYPE_SHARED 0x01 /* data type for flag */
#define OBJ_ATTR_FLAG_SPACE_SHARED 0x02
#define OBJ_ATTR_FLAG_ALL 0x03

struct OBJ_space_t
{
    OBJ_sds_extent_t extent; /* Dataspace extent */
    void *select;            /* ??? DELETED for now: Dataspace selection */
};

typedef struct OBJ_space_t OBJ_space_t;

typedef struct OBJ_attr_t OBJ_attr_t;

struct OBJ_attr_t
{
    char *name;       /* Attribute's name */
    OBJ_type_t *dt;   /* Attribute's datatype */
    size_t dt_size;   /* Size of datatype on disk */
    OBJ_space_t *ds;  /* Attribute's dataspace */
    size_t ds_size;   /* Size of dataspace on disk */
    void *data;       /* Attribute data (on a temporary basis) */
    size_t data_size; /* Size of data on disk */
};                    /* OBJ_attr_t */

/* 
 * Object Comment
 */

typedef struct OBJ_comm_t
{
    char *s; /*ptr to malloc'd memory             */
} OBJ_comm_t;

#define OBJ_FLAG_SHARED 0x02u

/* Old version, with full symbol table entry as link for object header sharing */
#define OBJ_SHARED_VERSION_1 1

/* New version, with just address of object as link for object header sharing */
#define OBJ_SHARED_VERSION_2 2

/* Newest version, which recognizes messages that are stored in the heap */
#define OBJ_SHARED_VERSION_3 3
#define OBJ_SHARED_VERSION_LATEST OBJ_SHARED_VERSION_3

/*
 * How message is shared: OBJ_shared_t.type:
 * Shared objects can be committed, in which case the shared message contains
 * the location of the object header that holds the message, or shared in the
 * heap, in which case the shared message holds their heap ID.
 */
#define OBJ_SHARE_TYPE_UNSHARED 0  /* Message is not shared */
#define OBJ_SHARE_TYPE_SOHM 1      /* Message is stored in SOHM heap */
#define OBJ_SHARE_TYPE_COMMITTED 2 /* Message is stored in another object header */
#define OBJ_SHARE_TYPE_HERE 3      /* Message is stored in this object header, but is sharable */

typedef uint32_t OBJ_msg_crt_idx_t;
#define OBJ_FHEAP_ID_LEN 8
typedef union {
    uint8_t id[OBJ_FHEAP_ID_LEN]; /* Buffer to hold ID, for encoding/decoding */
    uint64_t val;                 /* Value, for quick comparisons */
} OBJ_fheap_id_t;

typedef struct OBJ_mesg_loc_t
{
    OBJ_msg_crt_idx_t index; /* index within object header   */
    ck_addr_t oh_addr;       /* address of object header    */
} OBJ_mesg_loc_t;

typedef struct OBJ_shared_t
{
    unsigned type;        /* how message is shared */
    unsigned msg_type_id; /* message's type ID */
    union {
        OBJ_mesg_loc_t loc;     /* object location info */
        OBJ_fheap_id_t heap_id; /* ID within the SOHM heap */
    } u;
} OBJ_shared_t;

/*
 * Shared message table Message
 */
#define OBJ_SHMESG_MAX_NINDEXES 8

typedef struct OBJ_shmesg_table_t
{
    ck_addr_t addr;    /* file address of SOHM table */
    unsigned version;  /* SOHM table version number */
    unsigned nindexes; /* number of indexes in the table */
} OBJ_shmesg_table_t;

/* Object Header Continuation Message */
typedef struct OBJ_cont_t
{
    ck_addr_t addr; /*address of continuation block      */
    size_t size;    /*size of continuation block         */

    /* the following field(s) do not appear on disk */
    unsigned chunkno; /*chunk this mesg refers to          */
} OBJ_cont_t;

/* Group message */

typedef struct OBJ_stab_t
{
    ck_addr_t btree_addr; /*address of B-tree                  */
    ck_addr_t heap_addr;  /*address of name heap               */
} OBJ_stab_t;

/* Object Modification Date & Time Message */
#define OBJ_MTIME_VERSION 1

/*
 * v1 B-tree 'K' value message
 */
#define OBJ_BTREEK_VERSION 0
typedef struct OBJ_btreek_t
{
    unsigned btree_k[BT_NUM_BTREE_ID]; /* B-tree internal node 'K' values */
    unsigned sym_leaf_k;               /* Symbol table leaf node's 'K' value */
} OBJ_btreek_t;

/*
 * Driver info message
 */
#define OBJ_DRVINFO_VERSION 0
typedef struct OBJ_drvinfo_t
{
    char name[9]; /* Driver name */
    size_t len;   /* Driver Information Size */
    uint8_t *buf; /* Buffer for Driver Information */
} OBJ_drvinfo_t;

/*
 * Attribute Info Message.
 */

#define OBJ_AINFO_VERSION 0
#define OBJ_AINFO_TRACK_CORDER 0x01
#define OBJ_AINFO_INDEX_CORDER 0x02
#define OBJ_AINFO_ALL_FLAGS (OBJ_AINFO_TRACK_CORDER | OBJ_AINFO_INDEX_CORDER)
#define OBJ_MAX_CRT_ORDER_IDX 65535 /* Max. creation order index value   */

typedef struct OBJ_ainfo_t
{
    /* Creation order info */
    ck_bool_t track_corder;        /* Are creation order values tracked on attributes? */
    ck_bool_t index_corder;        /* Are creation order values indexed on attributes? */
    OBJ_msg_crt_idx_t max_crt_idx; /* Maximum attribute creation index used */
    ck_addr_t corder_bt2_addr;     /* Address of v2 B-tree for indexing creation order values of "dense" attributes */

    /* Storage management info */
    ck_hsize_t nattrs;       /* Number of attributes on the object */
    ck_addr_t fheap_addr;    /* Address of fractal heap for storing "dense" attributes */
    ck_addr_t name_bt2_addr; /* Address of v2 B-tree for indexing names of "dense" attributes */
} OBJ_ainfo_t;

/* Object's Reference Count Message */
#define OBJ_REFCOUNT_VERSION 0

typedef uint32_t OBJ_refcount_t; /* Contains # of links to object, if >1 */

/*
 * Data object headers
 */
#define MSG_TYPES 24 /* # of types of messages            */

typedef struct obj_class_t
{
    int id;                                                                   /* header message ID */
    void *(*decode)(driver_t *, const uint8_t *, const uint8_t *, ck_addr_t); /* Decode method */
    void *(*copy)(void *, void *);                                            /* copy native value	  */
    ck_err_t (*free)(void *);                                                 /* free main data struct  */
} obj_class_t;

#define OBJ_MSG_FLAG_CONSTANT 0x01u
#define OBJ_MSG_FLAG_SHARED 0x02u
#define OBJ_MSG_FLAG_DONTSHARE 0x04u

#define OBJ_MSG_FLAG_FAIL_IF_UNKNOWN 0x08u
#define OBJ_MSG_FLAG_MARK_IF_UNKNOWN 0x10u

#define OBJ_MSG_FLAG_WAS_UNKNOWN 0x20u
#define OBJ_MSG_FLAG_SHAREABLE 0x40u

#define OBJ_MSG_FLAG_BITS (OBJ_MSG_FLAG_CONSTANT | OBJ_MSG_FLAG_SHARED | OBJ_MSG_FLAG_DONTSHARE | OBJ_MSG_FLAG_FAIL_IF_UNKNOWN | OBJ_MSG_FLAG_MARK_IF_UNKNOWN | OBJ_MSG_FLAG_WAS_UNKNOWN | OBJ_MSG_FLAG_SHAREABLE)

typedef struct OBJ_mesg_t
{
    const obj_class_t *type; /*type of message                    */
    ck_bool_t dirty;         /*raw out of date wrt native         */
    uint8_t flags;           /*message flags                      */
    unsigned chunkno;        /*chunk number for this mesg         */
    void *native;            /*native format message              */
    uint8_t *raw;            /*ptr to raw data                    */
    size_t raw_size;         /*size with alignment                */
} OBJ_mesg_t;

typedef struct OBJ_chunk_t
{
    ck_addr_t addr; /*chunk file address                 */
    size_t size;    /*chunk size                         */
    uint8_t *image; /*image of file                      */
} OBJ_chunk_t;

#define OBJ_VERSION_1 1
#define OBJ_VERSION_2 2
#define OBJ_SIZEOF_MAGIC 4
#define OBJ_SPEC_READ_SIZE 512

/* Object header signatures */
#define OBJ_HDR_MAGIC "OHDR" /* Header */
#define OBJ_CHK_MAGIC "OCHK" /* Continuation chunk */

#define OBJ_SIZEOF_CHKSUM 4

/* NEED TO BE CHECKED */
#define OBJ_NMESGS 32 /*initial number of messages         */
#define OBJ_NCHUNKS 2 /*initial number of chunks           */

/* Object header flag definitions */
#define OBJ_HDR_CHUNK0_SIZE 0x03             /* 2-bit field indicating # of bytes to store the size of chunk 0's data */
#define OBJ_HDR_ATTR_CRT_ORDER_TRACKED 0x04  /* Attribute creation order is tracked */
#define OBJ_HDR_ATTR_CRT_ORDER_INDEXED 0x08  /* Attribute creation order has index */
#define OBJ_HDR_ATTR_STORE_PHASE_CHANGE 0x10 /* Non-default attribute storage phase change values stored */
#define OBJ_HDR_STORE_TIMES 0x20             /* Store access, modification, change & birth times for object */
#define OBJ_HDR_ALL_FLAGS (OBJ_HDR_CHUNK0_SIZE | OBJ_HDR_ATTR_CRT_ORDER_TRACKED | OBJ_HDR_ATTR_CRT_ORDER_INDEXED | OBJ_HDR_ATTR_STORE_PHASE_CHANGE | OBJ_HDR_STORE_TIMES)

#define OBJ_CRT_ATTR_MAX_COMPACT_DEF 8
#define OBJ_CRT_ATTR_MIN_DENSE_DEF 6
#define OBJ_CRT_OHDR_FLAGS_DEF OBJ_HDR_STORE_TIMES

#define OBJ_ALIGN_OLD(X) (8 * (((X) + 7) / 8))
#define OBJ_ALIGN_VERS(V, X)                   \
    (((V) == OBJ_VERSION_1) ? OBJ_ALIGN_OLD(X) \
                            : (X))
#define OBJ_ALIGN_OH(O, X) \
    OBJ_ALIGN_VERS((O)->version, X)

/*
 * Size of object header message prefix
 */
#define OBJ_SIZEOF_MSGHDR_VERS(V, C)                    \
    (((V) == OBJ_VERSION_1)                             \
         ? OBJ_ALIGN_OLD(2 + /*message type          */ \
                         2 + /*sizeof message data   */ \
                         1 + /*flags                 */ \
                         3)  /*reserved              */ \
         : (1 +              /*message type          */ \
            2 +              /*sizeof message data   */ \
            1 +              /*flags                 */ \
            ((C) ? (                                    \
                       2 /*creation index        */     \
                       )                                \
                 : 0)))
#define OBJ_SIZEOF_MSGHDR_OH(O) \
    OBJ_SIZEOF_MSGHDR_VERS((O)->version, (O)->flags &OBJ_HDR_ATTR_CRT_ORDER_TRACKED)

/* size of the header for the data object headers */
#define OBJ_SIZEOF_HDR_VERS(V, O)                                                                 \
    (((V) == OBJ_VERSION_1)                                                                       \
         ? OBJ_ALIGN_OLD(1 +   /*version number        */                                         \
                         1 +   /*reserved              */                                         \
                         2 +   /*number of messages    */                                         \
                         4 +   /*reference count       */                                         \
                         4)    /*chunk data size       */                                         \
         : (OBJ_SIZEOF_MAGIC + /*magic number          */                                         \
            1 +                /*version number        */                                         \
            1 +                /*flags                 */                                         \
            (((O)->flags & OBJ_HDR_STORE_TIMES) ? (                                               \
                                                      4 + /*access time           */              \
                                                      4 + /*modification time     */              \
                                                      4 + /*change time           */              \
                                                      4   /*birth time            */              \
                                                      )                                           \
                                                : 0) +                                            \
            (((O)->flags & OBJ_HDR_ATTR_STORE_PHASE_CHANGE) ? (                                   \
                                                                  2 + /*max compact attributes */ \
                                                                  2   /*min dense attributes  */  \
                                                                  )                               \
                                                            : 0) +                                \
            (1 << ((O)->flags & OBJ_HDR_CHUNK0_SIZE)) + /*chunk 0 data size */                    \
            OBJ_SIZEOF_CHKSUM)                          /*checksum size      */                   \
    )

#define OBJ_SIZEOF_HDR(O) \
    OBJ_SIZEOF_HDR_VERS((O)->version, O)

/*
 * Size of checksum for each chunk
 */
#define OBJ_SIZEOF_CHKSUM_VERS(V)             \
    (((V) == OBJ_VERSION_1)                   \
         ? 0                 /*no checksum */ \
         : OBJ_SIZEOF_CHKSUM /*checksum */    \
    )
#define OBJ_SIZEOF_CHKSUM_OH(O) \
    OBJ_SIZEOF_CHKSUM_VERS((O)->version)

typedef struct OBJ_t
{
    int version;   /*version number                     */
    int nlink;     /*link count                         */
    uint8_t flags; /*flags                              */

    /* Time information (for versions > 1 & OBJ_HDR_STORE_TIMES flag set)    */
    time_t atime; /*access time                        */
    time_t mtime; /*modification time                  */
    time_t ctime; /*change time                        */
    time_t btime; /*birth time                         */

    /* Attribute information (for versions > 1) */
    unsigned max_compact; /* Maximum # of compact attributes   */
    unsigned min_dense;   /* Minimum # of "dense" attributes   */

    unsigned nmesgs;        /*number of messages                 */
    unsigned alloc_nmesgs;  /*number of message slots            */
    OBJ_mesg_t *mesg;       /*array of messages                  */
    unsigned nchunks;       /*number of chunks                   */
    unsigned alloc_nchunks; /*chunks allocated                   */
    OBJ_chunk_t *chunk;     /*array of chunks                    */
} OBJ_t;

/*
 * 1.6 B-tree 
 */
#define BT_SIZEOF_MAGIC 4 /* size of magic number */
#define BT_SIZEOF_HDR(F)                                           \
    (BT_SIZEOF_MAGIC +   /* magic number                        */ \
     4 +                 /* type, level, num entries            */ \
     2 * SIZEOF_ADDR(F)) /* left and right sibling addresses    */

#define BT_MAGIC "TREE" /* tree node magic number */

typedef struct key_info_t
{
    uint8_t *heap_chunk; /* heap data */
    size_t heap_size;    /* heap size */
    size_t ndims;        /* for chunk node */
} key_info_t;

typedef struct BT_class_t
{
    BT_subid_t id;                                                                                    /* id as found in file*/
    ck_size_t sizeof_nkey;                                                                            /* size of native (memory) key*/
    ck_size_t (*get_sizeof_rkey)(global_shared_t *shared, key_info_t *key_info);                      /*raw key size   */
    ck_err_t (*decode)(global_shared_t *shared, key_info_t *key_info, const uint8_t **p, void **key); /* decode key */
    int (*cmp)(global_shared_t *shared, key_info_t *key_info, void *lt_key, void *rt_key);            /* compare key */
} BT_class_t;

typedef struct GP_node_key_t
{
    ck_size_t offset; /*offset into heap for name          */
} GP_node_key_t;

typedef struct RAW_node_key_t
{
    ck_size_t nbytes;                    /*size of stored data   */
    ck_hsize_t offset[OBJ_LAYOUT_NDIMS]; /*logical offset to start*/
    unsigned filter_mask;                /*excluded filters      */
} RAW_node_key_t;

/*
 * 1.8 B-tree
 */
#define B2_SIZEOF_MAGIC 4
#define B2_HDR_MAGIC "BTHD"  /* Header */
#define B2_INT_MAGIC "BTIN"  /* Internal node */
#define B2_LEAF_MAGIC "BTLF" /* Leaf node */
#define B2_HDR_VERSION 0
#define B2_INT_VERSION 0
#define B2_LEAF_VERSION 0

/* Size of storage for number of records per node (on disk) */
#define B2_SIZEOF_RECORDS_PER_NODE 2

/* Size of a "tree pointer" */
/* (essentially, the largest internal pointer allowed) */
#define B2_TREE_POINTER_SIZE(f) (SIZEOF_ADDR(f) + B2_SIZEOF_RECORDS_PER_NODE + SIZEOF_SIZE(f))

/* Size of an internal node pointer */
#define B2_INT_POINTER_SIZE(f, s, d) (                                                  \
    SIZEOF_ADDR(f)                            /* Address of child node */               \
    + (s)->max_nrec_size                      /* # of records in child node */          \
    + (s)->node_info[(d)-1].cum_max_nrec_size /* Total # of records in child & below */ \
)

/* Size of checksum information (on disk) */
#define B2_SIZEOF_CHKSUM 4

/* Format overhead for all v2 B-tree metadata in the file */
#define B2_METADATA_PREFIX_SIZE (              \
    B2_SIZEOF_MAGIC    /* Signature */         \
    + 1                /* Version */           \
    + 1                /* Tree type */         \
    + B2_SIZEOF_CHKSUM /* Metadata checksum */ \
)

/* Size of the v2 B-tree header on disk */
#define B2_HEADER_SIZE(f) (/* General metadata fields */                                                    \
                           B2_METADATA_PREFIX_SIZE                                                          \
                                                                                                            \
                           /* Header specific fields */                                                     \
                           + 4                       /* Node size, in bytes */                              \
                           + 2                       /* Record size, in bytes */                            \
                           + 2                       /* Depth of tree */                                    \
                           + 1                       /* Split % of full (as integer, ie. "98" means 98%) */ \
                           + 1                       /* Merge % of full (as integer, ie. "98" means 98%) */ \
                           + B2_TREE_POINTER_SIZE(f) /* Node pointer to root node in tree */                \
)

/* Size of the v2 B-tree internal node prefix */
#define B2_INT_PREFIX_SIZE (/* General metadata fields */             \
                            B2_METADATA_PREFIX_SIZE                   \
                                                                      \
                            /* Header specific fields */ /* <none> */ \
)

/* Size of the v2 B-tree leaf node prefix */
#define B2_LEAF_PREFIX_SIZE (/* General metadata fields */             \
                             B2_METADATA_PREFIX_SIZE                   \
                                                                       \
                             /* Header specific fields */ /* <none> */ \
)

/* Number of records that fit into leaf node */
#define B2_NUM_LEAF_REC(n, r) \
    (((n)-B2_LEAF_PREFIX_SIZE) / (r))

#define B2_NUM_INT_REC(f, s, d) \
    (((s)->node_size - (B2_INT_PREFIX_SIZE + B2_INT_POINTER_SIZE(f, s, d))) / ((s)->rrec_size + B2_INT_POINTER_SIZE(f, s, d)))

/* Macro to retrieve pointer to i'th native record for native record buffer */
#define B2_NAT_NREC(b, shared, idx) ((b) + (shared)->nat_off[(idx)])

/* Macro to retrieve pointer to i'th native record for internal node */
#define B2_INT_NREC(i, shared, idx) B2_NAT_NREC((i)->int_native, (shared), (idx))

/* Macro to retrieve pointer to i'th native record for leaf node */
#define B2_LEAF_NREC(l, shared, idx) B2_NAT_NREC((l)->leaf_native, (shared), (idx))

/* 1.8 B-tree IDs  */
typedef enum B2_subid_t
{
    B2_TEST_ID = 0,              /* B-tree is for testing (do not use for actual data) */
    B2_FHEAP_HUGE_INDIR_ID,      /* B-tree is for fractal heap indirectly accessed, non-filtered 'huge' objects */
    B2_FHEAP_HUGE_FILT_INDIR_ID, /* B-tree is for fractal heap indirectly accessed, filtered 'huge' objects */
    B2_FHEAP_HUGE_DIR_ID,        /* B-tree is for fractal heap directly accessed, non-filtered 'huge' objects */
    B2_FHEAP_HUGE_FILT_DIR_ID,   /* B-tree is for fractal heap directly accessed, filtered 'huge' objects */
    B2_GRP_DENSE_NAME_ID,        /* B-tree is for indexing 'name' field for "dense" link storage in groups */
    B2_GRP_DENSE_CORDER_ID,      /* B-tree is for indexing 'creation order' field for "dense" link storage in groups */
    B2_SOHM_INDEX_ID,            /* B-tree is an index for shared object header messages */
    B2_ATTR_DENSE_NAME_ID,       /* B-tree is for indexing 'name' field for "dense" attribute storage on objects */
    B2_ATTR_DENSE_CORDER_ID,     /* B-tree is for indexing 'creation order' field for "dense" attribute storage on objects */
    B2_DATA_CHUNKS_ID,           /* B-tree is for indexing non-filtered dataset chunks */
    B2_DATA_FILT_CHUNKS_ID,      /* B-tree is for indexing filtered dataset chunks */
    B2_NUM_BTREE_ID              /* Number of B-tree IDs (must be last)  */
} B2_subid_t;

typedef struct B2_class_t B2_class_t;

struct B2_class_t
{
    B2_subid_t id;
    ck_size_t nrec_size; /* Size of native (memory) record */
    ck_err_t (*decode)(driver_t *f, const uint8_t *raw, void *record, void *_ck_udata);
    ck_err_t (*compare)(const void *rec1, const void *rec2);
};

typedef ck_err_t (*B2_found_t)(const void *record, void *op_data);

/* Typedef for indirectly accessed 'huge' object's records in the v2 B-tree */
typedef struct HF_huge_bt2_indir_rec_t
{
    ck_addr_t addr; /* Address of the object in the file */
    ck_hsize_t len; /* Length of the object in the file */
    ck_hsize_t id;  /* ID used for object (not used for 'huge' objects directly accessed) */
} HF_huge_bt2_indir_rec_t;

/* Typedef for indirectly accessed, filtered 'huge' object's records in the v2 B-tree */
typedef struct HF_huge_bt2_filt_indir_rec_t
{
    ck_addr_t addr;       /* Address of the filtered object in the file */
    ck_hsize_t len;       /* Length of the filtered object in the file */
    unsigned filter_mask; /* I/O pipeline filter mask for filtered object in the file */
    ck_hsize_t obj_size;  /* Size of the de-filtered object in memory */
    ck_hsize_t id;        /* ID used for object (not used for 'huge' objects directly accessed) */
} HF_huge_bt2_filt_indir_rec_t;

/* Typedef for directly accessed 'huge' object's records in the v2 B-tree */
typedef struct HF_huge_bt2_dir_rec_t
{
    ck_addr_t addr; /* Address of the object in the file */
    ck_hsize_t len; /* Length of the object in the file */
} HF_huge_bt2_dir_rec_t;

/* Typedef for directly accessed, filtered 'huge' object's records in the v2 B-tree */
typedef struct HF_huge_bt2_filt_dir_rec_t
{
    ck_addr_t addr;       /* Address of the filtered object in the file */
    ck_hsize_t len;       /* Length of the filtered object in the file */
    unsigned filter_mask; /* I/O pipeline filter mask for filtered object in the file */
    ck_hsize_t obj_size;  /* Size of the de-filtered object in memory */
} HF_huge_bt2_filt_dir_rec_t;

/* Standard length of fractal heap ID for link */
#define G_DENSE_FHEAP_ID_LEN 7

/* Typedef for native 'name' field index records in the v2 B-tree */
/* (Keep 'id' field first so generic record handling in callbacks works) */
typedef struct G_dense_bt2_name_rec_t
{
    uint8_t id[G_DENSE_FHEAP_ID_LEN]; /* Heap ID for link */
    uint32_t hash;                    /* Hash of 'name' field value */
} G_dense_bt2_name_rec_t;

/* Typedef for native 'creation order' field index records in the v2 B-tree */
/* (Keep 'id' field first so generic record handling in callbacks works) */
typedef struct G_dense_bt2_corder_rec_t
{
    uint8_t id[G_DENSE_FHEAP_ID_LEN]; /* Heap ID for link */
    int64_t corder;                   /* 'creation order' field value */
} G_dense_bt2_corder_rec_t;

/* 
 * Specify the object header address and index needed
 * to locate a message in another object header.
 */

/* Typedef for a record's location if it's stored in the heap */
typedef struct SM_heap_loc_t
{
    ck_hsize_t ref_count;    /* Number of times this message is used in the file */
    OBJ_fheap_id_t fheap_id; /* ID of the OHM in the fractal heap */
} SM_heap_loc_t;

/* Where a message is stored */
typedef enum
{
    SM_NO_LOC = -1,
    SM_IN_HEAP = 0, /* Message is stored in the heap */
    SM_IN_OH        /* Message is stored in an object header */
} SM_storage_loc_t;

/* Typedef for a SOHM index node */
typedef struct
{
    SM_storage_loc_t location; /* Type of message location */
    uint32_t hash;             /* Hash value for encoded OHM */
    unsigned msg_type_id;      /* Message's type ID */
    union {
        OBJ_mesg_loc_t mesg_loc; /* Location of message in object header */
        SM_heap_loc_t heap_loc;  /* Heap ID for message in SOHM heap */
    } u;
} SM_sohm_t;

/* Typedef for native 'name' field index records in the v2 B-tree */
/* (Keep 'id' field first so generic record handling in callbacks works) */
typedef struct A_dense_bt2_name_rec_t
{
    OBJ_fheap_id_t id;        /* Heap ID for attribute */
    uint8_t flags;            /* Object header message flags for attribute */
    OBJ_msg_crt_idx_t corder; /* 'creation order' field value */
    uint32_t hash;            /* Hash of 'name' field value */
} A_dense_bt2_name_rec_t;

/* Typedef for native 'creation order' field index records in the v2 B-tree */
/* (Keep 'id' field first so generic record handling in callbacks works) */
typedef struct H5A_dense_bt2_corder_rec_t
{
    OBJ_fheap_id_t id;        /* Heap ID for attribute */
    uint8_t flags;            /* Object header message flags for attribute */
    OBJ_msg_crt_idx_t corder; /* 'creation order' field value */
} A_dense_bt2_corder_rec_t;

/* Typedef for native data layout non-filtered chunk index records in the v2 B-tree */
/* (Keep 'id' field first so generic record handling in callbacks works) */
typedef struct D_bt2_rec_t
{
    ck_addr_t addr;                                     /* Address of the non-filtered data chunk in the file */
    unsigned long long scaled_offset[OBJ_LAYOUT_NDIMS]; /* Scaled offset at each dimension */
} D_bt2_rec_t;

/* Typedef for native data layout filtered chunk index records in the v2 B-tree */
/* (Keep 'id' field first so generic record handling in callbacks works) */
typedef struct D_bt2_filt_rec_t
{
    ck_addr_t addr;                                     /* Address of the filtered data chunk in the file */
    unsigned long long chunk_size;                      /* Chunk size */
    unsigned mask;                                      /* Filter mask */
    unsigned long long scaled_offset[OBJ_LAYOUT_NDIMS]; /* Scaled offset at each dimension */
} D_bt2_filt_rec_t;

/* Information about a node at a given depth */
typedef struct
{
    unsigned max_nrec;
    ck_hsize_t cum_max_nrec;
    unsigned char cum_max_nrec_size;
} B2_node_info_t;

/* for passing info to traverse the btree */
typedef struct bt2_shared_t
{ /* shared info */
    const B2_class_t *type;
    ck_size_t node_size;
    ck_size_t rrec_size;
    unsigned depth;
    unsigned char max_nrec_size;
    ck_size_t *nat_off;
    B2_node_info_t *node_info;
} B2_shared_t;

typedef struct B2_node_ptr_t
{ /* node specific info */
    ck_addr_t addr;
    unsigned node_nrec;
    ck_hsize_t all_nrec;
} B2_node_ptr_t;

/* The B-tree information */
typedef struct B2_t
{
    B2_node_ptr_t root;  /* Node pointer to root node in B-tree        */
    B2_shared_t *shared; /* shared info                    */
} B2_t;

/* B-tree leaf node information */
typedef struct B2_leaf_t
{
    B2_shared_t *shared;  /* shared info                    */
    uint8_t *leaf_native; /* Pointer to native records                  */
    unsigned nrec;        /* Number of records in node                  */
} B2_leaf_t;

/* B-tree internal node information */
typedef struct B2_internal_t
{
    B2_shared_t *shared;      /* shared info                    */
    uint8_t *int_native;      /* Pointer to native records                  */
    B2_node_ptr_t *node_ptrs; /* Pointer to node pointers                   */
    unsigned nrec;            /* Number of records in node                  */
    unsigned depth;           /* Depth of this node in the B-tree           */
} B2_internal_t;

typedef struct obj_info_t
{
    union {
        ck_addr_t addr; /* address of the huge object in the file */
        ck_hsize_t off; /* offset of the object (managed/tiny) in the heap */
    } u;
    ck_size_t size;      /* size of non-filtered object */
    unsigned mask;       /* filter mask for filtered object */
    ck_size_t filt_size; /* size of filtered object */
} obj_info_t;

/*
 * Local Heap
 */
#define HL_MAGIC "HEAP" /* heap magic number */
#define HL_SIZEOF_MAGIC 4
#define HL_ALIGN(X) (((X) + 7) & (unsigned)(~0x07)) /*align on 8-byte boundary  */

#define HL_VERSION 0
#define HL_FREE_NULL 1 /*end of free list on disk      */

#define HL_SIZEOF_HDR(F)                                          \
    HL_ALIGN(HL_SIZEOF_MAGIC + /*heap signature                */ \
             4 +               /*reserved                      */ \
             SIZEOF_SIZE(F) +  /*data size                     */ \
             SIZEOF_SIZE(F) +  /*free list head                */ \
             SIZEOF_ADDR(F))   /*data address                  */

/* 
 * Global Heap 
 */
#define H5HG_MINSIZE 4096
#define H5HG_VERSION 1

#define H5HG_MAGIC "GCOL"
#define H5HG_SIZEOF_MAGIC 4

#define H5HG_SIZEOF_HDR(F)                                \
    H5HG_ALIGN(4 +             /*magic number          */ \
               1 +             /*version number        */ \
               3 +             /*reserved              */ \
               SIZEOF_SIZE(F)) /*collection size       */

#define H5HG_ALIGNMENT 8
#define H5HG_ALIGN(X) (H5HG_ALIGNMENT * (((X) + H5HG_ALIGNMENT - 1) / \
                                         H5HG_ALIGNMENT))
#define H5HG_ISALIGNED(X) ((X) == H5HG_ALIGN(X))
#define H5HG_SIZEOF_OBJHDR(F)                             \
    H5HG_ALIGN(2 +             /*object id number      */ \
               2 +             /*reference count       */ \
               4 +             /*reserved              */ \
               SIZEOF_SIZE(F)) /*object data size      */

#define H5HG_NOBJS(F, z) (int)((((z)-H5HG_SIZEOF_HDR(F)) /  \
                                    H5HG_SIZEOF_OBJHDR(F) + \
                                2))

typedef struct H5HG_obj_t
{
    int nrefs;      /*reference count               */
    size_t size;    /*total size of object          */
    uint8_t *begin; /*ptr to object into heap->chunk*/
} H5HG_obj_t;

struct H5HG_heap_t
{
    ck_addr_t addr;  /*collection address            */
    size_t size;     /*total size of collection      */
    uint8_t *chunk;  /*the collection, incl. header  */
    size_t nalloc;   /*numb object slots allocated   */
    size_t nused;    /*number of slots used          */
                     /* If this value is >65535 then all indices */
                     /* have been used at some time and the */
                     /* correct new index should be searched for */
    H5HG_obj_t *obj; /*array of object descriptions  */
};

typedef struct H5HG_heap_t H5HG_heap_t;

/* 
 * 1.8 Fractal Heap 
 */
#define HF_HDR_BUF_SIZE 512
#define HF_IBLOCK_BUF_SIZE 4096
#define HF_HDR_VERSION 0    /* Header */
#define HF_DBLOCK_VERSION 0 /* Direct block */
#define HF_IBLOCK_VERSION 0 /* Indirect block */
#define HF_SIZEOF_MAGIC 4

/* Fractal heap signatures */
#define HF_HDR_MAGIC "FRHP"    /* Header */
#define HF_IBLOCK_MAGIC "FHIB" /* Indirect block */
#define HF_DBLOCK_MAGIC "FHDB" /* Direct block */

/* Flags */
#define HF_HDR_FLAGS_HUGE_ID_WRAPPED 0x01  /* "huge" object IDs have wrapped */
#define HF_HDR_FLAGS_CHECKSUM_DBLOCKS 0x02 /* checksum direct blocks */

/* Size of checksum information (on disk) */
#define HF_SIZEOF_CHKSUM 4

#define POWER_OF_TWO(n) (!(n & (n - 1)) && n)
#define HF_WIDTH_LIMIT (64 * 1024)
#define HF_MAX_DIRECT_SIZE_LIMIT ((ck_hsize_t)2 * 1024 * 1024 * 1024)
#define HF_MAX_ID_LEN (4096 + 1)

/* Heap ID bit flags */
/* Heap ID version (2 bits: 6-7) */
#define HF_ID_VERS_CURR 0x00 /* Current version of ID format */
#define HF_ID_VERS_MASK 0xC0 /* Mask for getting the ID version from flags */
/* Heap ID type (2 bits: 4-5) */
#define HF_ID_TYPE_MAN 0x00      /* "Managed" object - stored in fractal heap blocks */
#define HF_ID_TYPE_HUGE 0x10     /* "Huge" object - stored in file directly */
#define HF_ID_TYPE_TINY 0x20     /* "Tiny" object - stored in heap ID directly */
#define HF_ID_TYPE_RESERVED 0x30 /* "?" object - reserved for future use */

#define HF_ID_TYPE_MASK 0x30 /* Mask for getting the ID type from flags */

/* Tiny object length information */
#define HF_TINY_LEN_SHORT 16      /* Max. length able to be encoded in first heap ID byte */
#define HF_TINY_MASK_SHORT 0x0F   /* Mask for length in first heap ID byte */
#define HF_TINY_MASK_EXT 0x0FFF   /* Mask for length in two heap ID bytes */
#define HF_TINY_MASK_EXT_1 0x0F00 /* Mask for length in first byte of two heap ID bytes */
#define HF_TINY_MASK_EXT_2 0x00FF /* Mask for length in second byte of two heap ID bytes */

/* "Standard" size of prefix information for fractal heap metadata */
#define HF_METADATA_PREFIX_SIZE(c) (                       \
    HF_SIZEOF_MAGIC                /* Signature */         \
    + 1                            /* Version */           \
    + ((c) ? HF_SIZEOF_CHKSUM : 0) /* Metadata checksum */ \
)

/* Size of doubling-table information */
#define HF_DTABLE_INFO_SIZE(fs) (                                                            \
    2                    /* Width of table (i.e. # of columns) */                            \
    + (fs)->size_lengths /* Starting block size */                                           \
    + (fs)->size_lengths /* Maximum direct block size */                                     \
    + 2                  /* Max. size of heap (log2 of actual value - i.e. the # of bits) */ \
    + 2                  /* Starting # of rows in root indirect block */                     \
    + (fs)->size_offsets /* File address of table managed */                                 \
    + 2                  /* Current # of rows in root indirect block */                      \
)

/* Size of the fractal heap header on disk */
#define HF_HEADER_SIZE(fs) (/* General metadata fields */                                            \
                            HF_METADATA_PREFIX_SIZE(TRUE)                                            \
                                                                                                     \
                            /* Fractal Heap Header specific fields */                                \
                                                                                                     \
                            /* General heap information */                                           \
                            + 2 /* Heap ID len */                                                    \
                            + 2 /* I/O filters' encoded len */                                       \
                            + 1 /* Status flags */                                                   \
                                                                                                     \
                            /* "Huge" object fields */                                               \
                            + 4                  /* Max. size of "managed" object */                 \
                            + (fs)->size_lengths /* Next ID for "huge" object */                     \
                            + (fs)->size_offsets /* File address of "huge" object tracker B-tree  */ \
                                                                                                     \
                            /* "Managed" object free space fields */                                 \
                            + (fs)->size_lengths /* Total man. free space */                         \
                            + (fs)->size_offsets /* File address of free section header */           \
                                                                                                     \
                            /* Statistics fields */                                                  \
                            + (fs)->size_lengths /* Size of man. space in heap */                    \
                            + (fs)->size_lengths /* Size of man. space iterator offset in heap */    \
                            + (fs)->size_lengths /* Size of alloacted man. space in heap */          \
                            + (fs)->size_lengths /* Number of man. objects in heap */                \
                            + (fs)->size_lengths /* Size of huge space in heap */                    \
                            + (fs)->size_lengths /* Number of huge objects in heap */                \
                            + (fs)->size_lengths /* Size of tiny space in heap */                    \
                            + (fs)->size_lengths /* Number of tiny objects in heap */                \
                                                                                                     \
                            /* "Managed" object doubling table info */                               \
                            + HF_DTABLE_INFO_SIZE(fs) /* Size of managed obj. doubling-table info */ \
)

/* Creation parameters for doubling-tables */
typedef struct HF_dtable_cparam_t
{
    unsigned width;             /* Number of columns in the table (must be power of 2) */
    ck_size_t start_block_size; /* Starting block size for table (must be power of 2) */
    ck_size_t max_direct_size;  /* Maximum size of a direct block (must be power of 2) */
    unsigned max_index;         /* Maximum ID/offset for table (integer log2 of actual value, ie. the # of bits required) */
    unsigned start_root_rows;   /* Starting number of rows for root indirect block */
                                /* 0 indicates to create the full indirect block for the root,
                                   * right from the start.  Doesn't have to be power of 2
                                   */
} HF_dtable_cparam_t;

/* Doubling-table info */
typedef struct H5HF_dtable_t
{
    HF_dtable_cparam_t cparam; /* Creation parameters for table */

    /* Derived information (stored, varies during lifetime of table) */
    ck_addr_t table_addr;    /* Address of first block for table */
                             /* Undefined if no space allocated for table */
    unsigned curr_root_rows; /* Current number of rows in the root indirect block */
                             /* 0 indicates that the TABLE_ADDR field points
                                   * to direct block (of START_BLOCK_SIZE) instead
                                   * of indirect root block.
                                   */

    /* Computed information (not stored) */
    unsigned max_root_rows;          /* Maximum # of rows in root indirect block */
    unsigned max_direct_rows;        /* Maximum # of direct rows in any indirect block */
    unsigned start_bits;             /* # of bits for starting block size (i.e. log2(start_block_size)) */
    unsigned max_direct_bits;        /* # of bits for max. direct block size (i.e. log2(max_direct_size)) */
    unsigned max_dir_blk_off_size;   /* Max. size of offsets in direct blocks */
    unsigned first_row_bits;         /* # of bits in address of first row */
    ck_hsize_t num_id_first_row;     /* Number of IDs in first row of table */
    ck_hsize_t *row_block_size;      /* Block size per row of indirect block */
    ck_hsize_t *row_block_off;       /* Cumulative offset per row of indirect block */
    ck_hsize_t *row_tot_dblock_free; /* Total free space in dblocks for this row */
                                     /* (For indirect block rows, it's the total
                                     * free space in all direct blocks referenced
                                     * from the indirect block)
                                     */
    ck_size_t *row_max_dblock_free;  /* Max. free space in dblocks for this row */
                                     /* (For indirect block rows, it's the maximum
                                     * free space in a direct block referenced
                                     * from the indirect block)
                                     */
} HF_dtable_t;

/* The fractal heap header information */
typedef struct HF_hdr_t
{
    /* General header information (stored in header) */
    unsigned id_len;     /* Size of heap IDs (in bytes) */
    unsigned filter_len; /* Size of I/O filter information (in bytes) */

    /* Flags for heap settings (stored in status byte in header) */
    ck_bool_t debug_objs;       /* Is the heap storing objects in 'debug' format */
    ck_bool_t write_once;       /* Is heap being written in "write once" mode? */
    ck_bool_t huge_ids_wrapped; /* Have "huge" object IDs wrapped around? */
    ck_bool_t checksum_dblocks; /* Should the direct blocks in the heap be checksummed? */

    /* Doubling table information (partially stored in header) */
    /* (Partially set by user, partially derived/updated internally) */
    HF_dtable_t man_dtable; /* Doubling-table info for managed objects */

    /* Free space information for managed objects (stored in header) */
    ck_hsize_t total_man_free; /* Total amount of free space in managed blocks */
    ck_addr_t fs_addr;         /* Address of free space header on disk */

    /* "Huge" object support (stored in header) */
    uint32_t max_man_size;   /* Max. size of object to manage in doubling table */
    ck_hsize_t huge_next_id; /* Next ID to use for indirectly tracked 'huge' object */
    ck_addr_t huge_bt2_addr; /* Address of v2 B-tree for tracking "huge" object info */

    /* I/O filter support (stored in header, if any are used) */
    OBJ_filter_t *pline;                    /* I/O filter pipeline for heap objects */
    ck_size_t pline_root_direct_size;       /* Size of filtered root direct block */
    unsigned pline_root_direct_filter_mask; /* I/O filter mask for filtered root direct block */

    /* Statistics for heap (stored in header) */
    ck_hsize_t man_size;       /* Total amount of 'managed' space in heap */
    ck_hsize_t man_alloc_size; /* Total amount of allocated 'managed' space in heap */
    ck_hsize_t man_iter_off;   /* Offset of iterator in 'managed' heap space */
    ck_hsize_t man_nobjs;      /* Number of 'managed' objects in heap */
    ck_hsize_t huge_size;      /* Total size of 'huge' objects in heap */
    ck_hsize_t huge_nobjs;     /* Number of 'huge' objects in heap */
    ck_hsize_t tiny_size;      /* Total size of 'tiny' objects in heap */
    ck_hsize_t tiny_nobjs;     /* Number of 'tiny' objects in heap */

    /* Cached/computed values (not stored in header) */
    ck_size_t rc;             /* Reference count of heap's components using heap header */
    ck_bool_t dirty;          /* Shared info is modified */
    ck_addr_t heap_addr;      /* Address of heap header in the file */
    ck_size_t heap_size;      /* Size of heap header in the file */
    ck_size_t file_rc;        /* Reference count of files using heap header */
    ck_bool_t pending_delete; /* Heap is pending deletion */
/* NEED TO TAKE CARE OF THIS */
#if 0
    H5FS_t      *fspace;          /* Free space list for objects in heap */
#endif
    ck_hsize_t huge_max_id;      /* Max. 'huge' heap ID before rolling 'huge' heap IDs over */
    ck_bool_t huge_ids_direct;   /* Flag to indicate that 'huge' object's offset & length are stored directly in heap ID */
    ck_size_t tiny_max_len;      /* Max. size of tiny objects for this heap */
    ck_bool_t tiny_len_extended; /* Flag to indicate that 'tiny' object's length is stored in extended form (i.e. w/extra byte) */
    uint8_t huge_id_size;        /* Size of 'huge' heap IDs (in bytes) */
    uint8_t heap_off_size;       /* Size of heap offsets (in bytes) */
    uint8_t heap_len_size;       /* Size of heap ID lengths (in bytes) */
} HF_hdr_t;

/* Size of managed indirect block entry for a child direct block */
#define HF_MAN_INDIRECT_CHILD_DIR_ENTRY_SIZE(fs, h) (                                                                          \
    ((h)->filter_len > 0 ? ((fs)->size_offsets + (fs)->size_lengths + 4) : /* Size of entries for filtered direct blocks */    \
         (fs)->size_offsets)                                               /* Size of entries for un-filtered direct blocks */ \
)

/* Size of managed indirect block */
#define HF_MAN_INDIRECT_SIZE(fs, h, i) (/* General metadata fields */                                                                                                                                                                             \
                                        HF_METADATA_PREFIX_SIZE(TRUE)                                                                                                                                                                             \
                                                                                                                                                                                                                                                  \
                                        /* Fractal heap managed, absolutely mapped indirect block specific fields */                                                                                                                              \
                                        + (fs)->size_offsets                                                                                                                                          /* File address of heap owning the block */ \
                                        + (h)->heap_off_size                                                                                                                                          /* Offset of the block in the heap */       \
                                        + (MIN((i)->nrows, (h)->man_dtable.max_direct_rows) * (h)->man_dtable.cparam.width * HF_MAN_INDIRECT_CHILD_DIR_ENTRY_SIZE(fs, h))                             /* Size of entries for direct blocks */     \
                                        + ((((i)->nrows > (h)->man_dtable.max_direct_rows) ? ((i)->nrows - (h)->man_dtable.max_direct_rows) : 0) * (h)->man_dtable.cparam.width * (fs)->size_offsets) /* Size of entries for indirect blocks */   \
)

#define HF_IBLOCK_BUF_SIZE 4096

/* Common indirect block doubling table entry */
/* (common between entries pointing to direct & indirect child blocks) */
typedef struct HF_indirect_ent_t
{
    ck_addr_t addr; /* Direct block's address                     */
} HF_indirect_ent_t;

/* Extern indirect block doubling table entry for compressed direct blocks */
/* (only exists for indirect blocks in heaps that have I/O filters) */
typedef struct HF_indirect_filt_ent_t
{
    ck_size_t size;       /* Size of child direct block, after passing though I/O filters */
    unsigned filter_mask; /* Excluded filters for child direct block */
} HF_indirect_filt_ent_t;

/* Fractal heap indirect block */
typedef struct HF_indirect_t
{
    HF_hdr_t *hdr;                     /* Shared heap header info                    */
    ck_addr_t addr;                    /* Address of this indirect block on disk     */
    ck_size_t size;                    /* Size of indirect block on disk             */
    unsigned nrows;                    /* Total # of rows in indirect block          */
    unsigned max_rows;                 /* Max. # of rows in indirect block           */
    unsigned nchildren;                /* Number of child blocks                     */
    unsigned max_child;                /* Max. offset used in child entries          */
    ck_hsize_t block_off;              /* Offset of the block within the heap's address space */
    HF_indirect_ent_t *ents;           /* Pointer to block entry table               */
    HF_indirect_filt_ent_t *filt_ents; /* Pointer to filtered information for direct blocks */
} HF_indirect_t;

/* Size of overhead for a direct block */
#define HF_MAN_ABS_DIRECT_OVERHEAD(fs, h) (/* General metadata fields */                                              \
                                           HF_METADATA_PREFIX_SIZE(h->checksum_dblocks)                               \
                                                                                                                      \
                                           /* Fractal heap managed, absolutely mapped direct block specific fields */ \
                                           + (fs)->size_offsets /* File address of heap owning the block */           \
                                           + (h)->heap_off_size /* Offset of the block in the heap */                 \
)

/* A fractal heap direct block */
typedef struct HF_direct_t
{
    HF_hdr_t *hdr;         /* Shared heap header info                    */
    unsigned par_entry;    /* Entry in parent's table                    */
    ck_size_t size;        /* Size of direct block                       */
    unsigned blk_off_size; /* Size of offsets in the block               */
    uint8_t *blk;          /* Pointer to buffer containing block data    */
    ck_hsize_t block_off;  /* Offset of the block within the heap's address space */
} HF_direct_t;

typedef struct HF_parent_t
{
#ifdef LATER
    H5HF_hdr_t *hdr; /* Pointer to heap header info */
#endif
    HF_indirect_t *iblock; /* Pointer to parent indirect block */
    unsigned entry;        /* Location of block in parent's entry table */
} HF_parent_t;

/* Compute the # of bytes required to store an offset into a given buffer size */
#define HF_SIZEOF_OFFSET_BITS(b) (((b) + 7) / 8)
#define HF_SIZEOF_OFFSET_LEN(l) HF_SIZEOF_OFFSET_BITS(V_log2_of2((unsigned)(l)))

/* Free space section types for fractal heap */
#define HF_FSPACE_SECT_SINGLE 0     /* Section is a range of actual bytes in a direct block */
#define HF_FSPACE_SECT_FIRST_ROW 1  /* Section is first range of blocks in an indirect block row */
#define HF_FSPACE_SECT_NORMAL_ROW 2 /* Section is a range of blocks in an indirect block row */
#define HF_FSPACE_SECT_INDIRECT 3   /* Section is a span of blocks in an indirect block */

/* SOHM */

/* Types of message indices */
typedef enum
{
    SM_BADTYPE = -1,
    SM_LIST, /* Index is an unsorted list */
    SM_BTREE /* Index is a sorted B-tree */
} SM_index_type_t;

/* Typedef for a SOHM index header */
typedef struct
{
    unsigned mesg_types;        /* Bit flag vector of message types */
    ck_size_t min_mesg_size;    /* message size sharing threshold for the index */
    ck_size_t list_max;         /* >= this many messages, index with a B-tree */
    ck_size_t btree_min;        /* <= this many messages, index with a list again */
    ck_size_t num_messages;     /* number of messages being tracked */
    SM_index_type_t index_type; /* Is the index a list or a B-tree? */
    ck_addr_t index_addr;       /* Address of the actual index (list or B-tree) */
    ck_addr_t heap_addr;        /* Address of the fheap used to store shared messages */
} SM_index_header_t;

typedef struct SM_master_table_t
{
    unsigned num_indexes;       /* Number of indexes */
    SM_index_header_t *indexes; /* Array of num_indexes indexes */
} SM_master_table_t;

#define SM_SIZEOF_MAGIC 4
#define SM_SIZEOF_CHECKSUM 4

/* Shared Message signatures */
#define SM_TABLE_MAGIC "SMTB" /* Shared Message Table */
#define SM_LIST_MAGIC "SMLI"  /* Shared Message List */
#define SM_TBL_BUF_SIZE 1024
#define SM_LIST_VERSION 0 /* Version of Shared Object Header Message Indexes */

#define SM_TABLE_SIZE(fs) (              \
    SM_SIZEOF_MAGIC      /* Signature */ \
    + SM_SIZEOF_CHECKSUM /* Checksum */  \
)

#define SM_INDEX_HEADER_SIZE(fs) (                                           \
    1                 /* Whether index is a list or B-tree */                \
    + 1               /* Version of index format */                          \
    + 2               /* Type of messages stored in the index */             \
    + 4               /* Minimum size of messages to share */                \
    + (3 * 2)         /* B-tree cutoff, list cutoff, # of shared messages */ \
    + SIZEOF_ADDR(fs) /* Location of list or B-tree */                       \
    + SIZEOF_ADDR(fs) /* Address of heap */                                  \
)

#define SHMESG_NONE_FLAG 0x0000                     /* No shared messages */
#define SHMESG_SDSPACE_FLAG ((unsigned)1 << 0x0001) /* Simple Dataspace Message.  */
#define SHMESG_DTYPE_FLAG ((unsigned)1 << 0x0003)   /* Datatype Message.  */
#define SHMESG_FILL_FLAG ((unsigned)1 << 0x0005)    /* Fill Value Message. */
#define SHMESG_PLINE_FLAG ((unsigned)1 << 0x000b)   /* Filter pipeline message.  */
#define SHMESG_ATTR_FLAG ((unsigned)1 << 0x000c)    /* Attribute Message.  */
#define SHMESG_ALL_FLAG (SHMESG_SDSPACE_FLAG | SHMESG_DTYPE_FLAG | SHMESG_FILL_FLAG | SHMESG_PLINE_FLAG | SHMESG_ATTR_FLAG)

#define ASSIGN_OVERFLOW(var, expr, exprtype, vartype)      \
    {                                                      \
        exprtype _tmp_overflow = (exprtype)(expr);         \
        vartype _tmp_overflow2 = (vartype)(_tmp_overflow); \
        assert((vartype)_tmp_overflow == _tmp_overflow2);  \
        (var) = _tmp_overflow2;                            \
    }

/* 
 * Free Space Manager 
 */
#define FS_SIZEOF_MAGIC 4
#define FS_HDR_MAGIC "FSHD"   /* Header */
#define FS_SINFO_MAGIC "FSSE" /* Serialized sections */
#define FS_SIZEOF_CHKSUM 4
#define FS_HDR_BUF_SIZE 256
#define FS_HDR_VERSION 0   /* Header */
#define FS_SINFO_VERSION 0 /* Serialized sections */

typedef enum FS_client_t
{
    FS_CLIENT_FHEAP_ID = 0,
    FS_NUM_CLIENT_ID
} FS_client_t;

#define FS_METADATA_PREFIX_SIZE (              \
    FS_SIZEOF_MAGIC    /* Signature */         \
    + 1                /* Version */           \
    + FS_SIZEOF_CHKSUM /* Metadata checksum */ \
)

/* Size of the fractal heap header on disk */
#define FS_HEADER_SIZE(f) (                                                   \
    FS_METADATA_PREFIX_SIZE                                                   \
                                                                              \
    /* Free space header specific fields */                                   \
    + 1              /* Client ID */                                          \
    + SIZEOF_SIZE(f) /* Total free space tracked */                           \
    + SIZEOF_SIZE(f) /* Total # of sections tracked */                        \
    + SIZEOF_SIZE(f) /* # of serializable sections tracked */                 \
    + SIZEOF_SIZE(f) /* # of ghost sections tracked */                        \
    + 2              /* Number of section classes */                          \
    + 2              /* Shrink percent */                                     \
    + 2              /* Expand percent */                                     \
    + 2              /* Size of address space for sections (log2 of value) */ \
    + SIZEOF_SIZE(f) /* Max. size of section to track */                      \
    + SIZEOF_ADDR(f) /* Address of serialized free space sections */          \
    + SIZEOF_SIZE(f) /* Size of serialized free space sections used */        \
    + SIZEOF_SIZE(f) /* Allocated size of serialized free space sections */   \
)

/* Size of the free space serialized sections on disk */
#define FS_SINFO_PREFIX_SIZE(f) (                                          \
    FS_METADATA_PREFIX_SIZE                                                \
                                                                           \
    /* Free space serialized sections specific fields */                   \
    + SIZEOF_ADDR(f) /* Address of free space header for these sections */ \
)

typedef struct FS_section_class_t
{
    const unsigned type; /* Type of free space section */
    size_t serial_size;  /* Size of serialized form of section */
    ck_err_t (*init_cls)(struct FS_section_class_t *, HF_hdr_t *);
} FS_section_class_t;

typedef struct FS_hdr_t
{
    ck_hsize_t tot_space;         /* Total amount of space tracked              */
    ck_hsize_t tot_sect_count;    /* Total # of sections tracked                */
    ck_hsize_t serial_sect_count; /* # of serializable sections tracked         */
    ck_hsize_t ghost_sect_count;  /* # of un-serializable sections tracked      */

    FS_client_t client;       /* Type of user of this free space manager    */
    unsigned nclasses;        /* Number of section classes handled          */
    unsigned shrink_percent;  /* Percent of "normal" serialized size to shrink serialized space at */
    unsigned expand_percent;  /* Percent of "normal" serialized size to expand serialized space at */
    unsigned max_sect_addr;   /* Size of address space free sections are within (log2 of actual value) */
    ck_hsize_t max_sect_size; /* Maximum size of section to track */

    ck_addr_t sect_addr;        /* Address of the section info in the file    */
    ck_hsize_t sect_size;       /* Size of the section info in the file       */
    ck_hsize_t alloc_sect_size; /* Allocated size of the section info in the file */

    ck_addr_t addr;               /* Address of free space header on disk       */
    FS_section_class_t *sect_cls; /* Array of section classes for this free list */
} FS_hdr_t;

/*
 *  Virtual file drivers
 */
#define SEC2_DRIVER 1
#define MULTI_DRIVER 2
#define FAMILY_DRIVER 3

struct driver_class_t
{
    const char *name;
    ck_err_t (*decode_driver)(global_shared_t *shared, const unsigned char *p);
    driver_t *(*open)(const char *name, global_shared_t *shared, int driver_id);
    ck_err_t (*close)(driver_t *file);
    ck_err_t (*read)(driver_t *file, ck_addr_t addr, size_t size, void *buffer);
    ck_addr_t (*get_eof)(driver_t *file);
    char *(*get_fname)(driver_t *file, ck_addr_t logi_addr);
};

#ifdef WORKING
typedef struct fd_t
{
    int driver_id;             /* driver ID for this file   */
    const driver_class_t *cls; /* constant class info       */
} fd_t;

typedef struct file_t
{
    char *fname;             /* file name */
    fd_t *lf;                /* file handle */
    global_shared_t *shared; /* shared info */
} file_t;
#endif

struct driver_t
{
    int driver_id; /* driver ID for this file   */
    global_shared_t *shared;
    const driver_class_t *cls; /* constant class info       */
};

typedef struct driver_sec2_t
{
    driver_t pub;  /* public stuff, must be first    */
    int fd;        /* the unix file                  */
    ck_addr_t eof; /* end of file; current file size */
    char *name;    /* name passed to Fopen */
} driver_sec2_t;

typedef enum driver_mem_t
{
    FD_MEM_NOLIST = -1, /* must be negative*/
    FD_MEM_DEFAULT = 0, /* must be zero*/
    FD_MEM_SUPER = 1,
    FD_MEM_BTREE = 2,
    FD_MEM_DRAW = 3,
    FD_MEM_GHEAP = 4,
    FD_MEM_LHEAP = 5,
    FD_MEM_OHDR = 6,

    FD_MEM_NTYPES /*must be last*/
} driver_mem_t;

typedef struct driver_multi_fapl_t
{
    driver_mem_t memb_map[FD_MEM_NTYPES]; /* memory usage map           */
    char *memb_name[FD_MEM_NTYPES];       /* name generators            */
    ck_addr_t memb_addr[FD_MEM_NTYPES];   /* starting addr per member   */
} driver_multi_fapl_t;

typedef struct driver_multi_t
{
    driver_t pub;                       /* public stuff, must be first            */
    driver_multi_fapl_t fa;             /* driver-specific file access properties */
    ck_addr_t memb_next[FD_MEM_NTYPES]; /* addr of next member          */
    driver_t *memb[FD_MEM_NTYPES];      /* member pointers              */
    ck_addr_t eoa;                      /* end of allocated addresses            */
    char *name;                         /* name passed to H5Fopen or H5Fcreate   */
} driver_multi_t;

typedef struct driver_fami_fapl_t
{
    ck_hsize_t memb_size; /* size of each member 	*/
} driver_fami_fapl_t;

typedef struct driver_fami_t
{
    driver_t pub;          /* public stuff, must be first            */
    driver_fami_fapl_t fa; /* driver-specific file access properties */
    unsigned nmembs;       /* number of family members */
    unsigned amembs;       /* number of member slots allocated */
    driver_t **memb;       /* array of member pointers */
    ck_addr_t eoa;
    char *name; /* name generator printf format */
} driver_fami_t;

/* virtual file drivers */
driver_t *FD_open(const char *, global_shared_t *, int);
ck_err_t FD_close(driver_t *);
ck_addr_t FD_get_eof(driver_t *);
char *FD_get_fname(driver_t *, ck_addr_t);
void free_driver_fa(global_shared_t *shared);

/* command line option */
int g_verbose_num;
int g_format_num;
ck_addr_t g_obj_addr;
ck_bool_t g_follow_ext;
void print_version(const char *);
void usage(char *);
void leave(int);

table_t *g_ext_tbl;

/* for handling names */
int name_list_init(name_list_t **name_list);
ck_bool_t name_list_search(name_list_t *nl, char *name);
ck_err_t name_list_insert(name_list_t *nl, char *name);
void name_list_dest(name_list_t *nl);

/* Validation routines */
ck_err_t check_superblock(driver_t *);
ck_err_t check_obj_header(driver_t *, ck_addr_t, OBJ_t **);

/* entering via h5checker_obj() API */
int g_obj_api;
int g_obj_api_err;
void process_err(ck_errmsg_t *);

const obj_class_t *const message_type_g[MSG_TYPES];
const B2_class_t HF_BT2_INDIR[1];
const B2_class_t HF_BT2_FILT_INDIR[1];
const B2_class_t HF_BT2_DIR[1];
const B2_class_t HF_BT2_FILT_DIR[1];
const B2_class_t G_BT2_CORDER[1];
const B2_class_t G_BT2_NAME[1];
const B2_class_t SM_INDEX[1];
const B2_class_t A_BT2_NAME[1];
const B2_class_t A_BT2_CORDER[1];
B2_class_t D_BT2_CHUNK[1];
B2_class_t D_BT2_FILT_CHUNK[1];

/* Define the check indexed message (group & attribute) callback function from check_btree2() */
typedef ck_err_t (*ck_op_t)(driver_t *file, const void *record, void *_ck_udata);

ck_err_t check_fheap(driver_t *, ck_addr_t);
ck_err_t check_SOHM(driver_t *, ck_addr_t, unsigned);
ck_err_t check_btree2(driver_t *, ck_addr_t, const B2_class_t *, ck_op_t ck_op, void *ck_udata);
HF_hdr_t *HF_open(driver_t *, ck_addr_t fh_addr);
ck_err_t HF_close(HF_hdr_t *fhdr);
ck_err_t HF_get_obj_info(driver_t *, HF_hdr_t *, const void *, obj_info_t *);
ck_err_t HF_read(driver_t *, HF_hdr_t *, const void *, void * /*out*/, obj_info_t *);
ck_err_t SM_get_fheap_addr(driver_t *, unsigned, ck_addr_t *);
unsigned V_log2_gen(uint64_t);

ck_err_t FD_read(driver_t *, ck_addr_t, size_t, void * /*out*/);
void addr_decode(global_shared_t *, const uint8_t **, ck_addr_t *);
ck_addr_t get_logical_addr(const uint8_t *, const uint8_t *, ck_addr_t);
int debug_verbose(void);
int object_api(void);

uint32_t checksum_metadata(const void *, ck_size_t, uint32_t);
uint32_t checksum_lookup3(const void *, ck_size_t, uint32_t);

driver_t *file_init(char *fname);
void free_file_shared(driver_t *thefile);

#define TYPE_HARD_LINK 1
#define TYPE_EXT_FILE 2

ck_err_t table_init(table_t **tbl, int type);
ck_err_t table_insert(table_t *tbl, void *_id, int type);
void table_free(table_t *tbl);

/*
 * Copied from the HDF tools library
 */
enum
{
    no_arg = 0,  /* doesn't take an argument     */
    require_arg, /* requires an argument         */
    optional_arg /* argument is optional         */
};

typedef struct long_options
{
    const char *name; /* name of the long option              */
    int has_arg;      /* whether we should look for an arg    */
    char shortval;    /* the shortname equivalent of long arg
                                 * this gets returned from get_option   */
} long_options;

int get_option(int argc, const char **argv, const char *opts, const struct long_options *l_opts);

#define MAX_PATH_LEN 1024
#define DIR_SEPC '/'
#define DIR_SEPS "/"
#define CHECK_DELIMITER(SS) (SS == DIR_SEPC)
#define CHECK_ABSOLUTE(NAME) (CHECK_DELIMITER(*NAME))
#define GET_LAST_DELIMITER(NAME, ptr) ptr = strrchr(NAME, DIR_SEPC);
