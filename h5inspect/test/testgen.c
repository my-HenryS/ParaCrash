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
   FILE
   testgen.c - HDF5 test generator main file for h5check
 */

#include "testgen.h"

/* Definitions for test generator functions */

#define NUM_GROUPS 512 	/* number of groups and recursion levels for linear structure */
#define HEIGHT 5 	/* height of group trees (recursion levels) */

/* types of group structures */
#define HIERARCHICAL 0
#define MULTIPATH 1
#define CYCLICAL 2

/* name prefixes */
#define GROUP_PREFIX "group"
#define DATASET_PREFIX "dataset"

/* dataset fill status */
#define EMPTY 0
#define PARTIAL 1
#define FULL 2

/* dataspace properties */
#define RANK 2     /* default rank */
#define SIZE 10    /* default size for all dimensions */

/* child nodes of a binary tree */
#define LEFT 0
#define RIGHT 1

#define STR_SIZE 12 /* default size for fixed-length strings */

/* basic datatypes */
#define NTYPES 12
#define NATIVE_TYPE(i)  \
    (i==0)?H5T_NATIVE_CHAR: \
    (i==1)?H5T_NATIVE_SHORT:    \
    (i==2)?H5T_NATIVE_INT:  \
    (i==3)?H5T_NATIVE_UINT: \
    (i==4)?H5T_NATIVE_LONG: \
    (i==5)?H5T_NATIVE_LLONG:    \
    (i==6)?H5T_NATIVE_FLOAT:    \
    (i==7)?H5T_NATIVE_DOUBLE:   \
    (i==8)?H5T_NATIVE_LDOUBLE:    \
    (i==9)?H5T_NATIVE_B8:    \
    (i==10)?H5T_NATIVE_OPAQUE:    \
        string_type1    \

/* Structure definitions for generation of files with compound and
   variable length datatypes */

typedef struct s2_t {
    unsigned int a;
    unsigned int b;
    unsigned int c[4];
    unsigned int d;
    unsigned int e;
} s2_t;

    
typedef struct s1_t {
    unsigned int a;
    unsigned int b;
    unsigned int c[4];
    unsigned int d;
    unsigned int e;
    s2_t        s2;
} s1_t;


/* Definition for generation of file with enumerated
   datatype */
#define CPTR(VAR,CONST)	((VAR)=(CONST),&(VAR))

typedef enum {
    E1_RED,
    E1_GREEN,
    E1_BLUE,
    E1_WHITE,
    E1_BLACK
} c_e1;

#define NUM_VALUES 5 /* number of possible values in enumerated datatype */


/* Definition for generation of a compound datatype that will
   be pointed by a reference datatype */
typedef struct s3_t {
    unsigned int a;
    unsigned int b;
    float c;
} s3_t;


/* Macros for testing filters */
/* Parameters for internal filter test */
#define CHUNKING_FACTOR 10

/* Temporary filter IDs used for testing */
#define H5Z_FILTER_BOGUS	305
#define H5Z_FILTER_CORRUPT	306
#define H5Z_FILTER_BOGUS2	307

#define H5_SZIP_NN_OPTION_MASK          32

unsigned szip_options_mask=H5_SZIP_NN_OPTION_MASK;
unsigned szip_pixels_per_block=4;

/* Macros for testing attributes */
/* Attribute Rank & Dimensions */
#define ATTR1_NAME  "Attr1"
#define ATTR1_RANK	1
#define ATTR1_DIM1	3
int attr_data1[ATTR1_DIM1]={512,-234,98123}; /* Test data for 1st attribute */

/* rank & dimensions for another attribute */
#define ATTR1A_NAME  "Attr1_a"
int attr_data1a[ATTR1_DIM1]={256,11945,-22107};

#define ATTR2_NAME  "Attr2"
#define ATTR2_RANK	2
#define ATTR2_DIM1	2
#define ATTR2_DIM2	2
int attr_data2[ATTR2_DIM1][ATTR2_DIM2]={{7614,-416},{197814,-3}}; /* Test data for 2nd attribute */

/* 1-D array datatype */
#define ARRAY1_RANK	1
#define ARRAY1_DIM1 4

int nerrors=0;

/* Local prototypes for filter functions */
static size_t filter_bogus(unsigned int flags, size_t cd_nelmts,
const unsigned int *cd_values, size_t nbytes, size_t *buf_size, void **buf);

#if H5_LIBVERSION == 18 /* library release >= 1.8 */

#if H5_VERS_RELEASE < 3
#define H5Z_class2_t H5Z_class_t
#endif

const H5Z_class2_t H5Z_BOGUS[1] = {{
    H5Z_CLASS_T_VERS,   /* H5Z_class_t version */
    H5Z_FILTER_BOGUS,	/* Filter id number		*/
    1,			/* encoder_present flag (set to true) */
    1,			/* decoder_present flag (set to true) */
    "bogus",		/* Filter name for debugging	*/
    NULL,               /* The "can apply" callback     */
    NULL,               /* The "set local" callback     */
    filter_bogus,	/* The actual filter function	*/
}};

#else /* 1.6 */

/* This message derives from H5Z */
const H5Z_class_t H5Z_BOGUS[1] = {{
    H5Z_FILTER_BOGUS,	/* Filter id number		*/
    "bogus",		/* Filter name for debugging	*/
    NULL,             	/* The "can apply" callback     */
    NULL,             	/* The "set local" callback     */
    filter_bogus,	/* The actual filter function	*/
}};

#endif


/*
* This function implements a bogus filter that just returns the value
* of an argument.
*/

static size_t 
filter_bogus(unsigned int UNUSED flags, size_t UNUSED cd_nelmts,
    const unsigned int UNUSED *cd_values, size_t nbytes,size_t UNUSED *buf_size,
    void UNUSED **buf)
{
    return nbytes;
}


/*
* This function sets the appropriate file driver.
* Returns: Success:  Set property list
*          Failure:     -1
*/
/*
 * These are the letters that are appended to the file name when generating
 * names for the split and multi drivers. They are:
 *
 *      m: All meta data when using the split driver.
 *      s: The userblock, superblock, and driver info block
 *      b: B-tree nodes
 *      r: Dataset raw data
 *      g: Global heap
 *      l: local heap (object names)
 *      o: object headers
 */
static const char *multi_letters = "msbrglo";

hid_t 
h5_fileaccess(char *driver)
{
    hid_t fapl = -1; /* property list */
    herr_t ret;
    char *logfile = "logfile.txt";

    fapl=H5Pcreate(H5P_FILE_ACCESS);

    if (strstr(driver, "sec2")) {
	    /* Unix read() and write() system calls */
	    ret=H5Pset_fapl_sec2(fapl);
        VRFY((ret>=0), "H5Pset_fapl_sec2");
    } else if (strstr(driver, "stdio")) {
	    /* Standard C fread() and fwrite() system calls */
	    ret=H5Pset_fapl_stdio(fapl);
        VRFY((ret>=0), "H5Pset_fapl_stdio");
    } else if (strstr(driver, "core")) {
	    /* In-core temporary file with 1MB increment */
	    ret=H5Pset_fapl_core(fapl, 1024*1024, FALSE);
        VRFY((ret>=0), "H5Pset_fapl_core");
    } else if (strstr(driver, "split")) {
	    /* Split meta data and raw data each using default driver */
	    ret=H5Pset_fapl_split(fapl,
			      "-m.h5", H5P_DEFAULT,
			      "-r.h5", H5P_DEFAULT);
        VRFY((ret>=0), "H5Pset_fapl_split");
    } else if (strstr(driver, "multi")) {

#ifdef NOTYET
	/* Multi-file driver, general case of the split driver */
        H5FD_mem_t memb_map[H5FD_MEM_NTYPES];
        hid_t memb_fapl[H5FD_MEM_NTYPES];
        const char *memb_name[H5FD_MEM_NTYPES];
        char sv[H5FD_MEM_NTYPES][1024];
        haddr_t memb_addr[H5FD_MEM_NTYPES];
        H5FD_mem_t      mt;

        memset(memb_map, 0, sizeof memb_map);
        memset(memb_fapl, 0, sizeof memb_fapl);
        memset(memb_name, 0, sizeof memb_name);
        memset(memb_addr, 0, sizeof memb_addr);

        for (mt=H5FD_MEM_DEFAULT; mt<H5FD_MEM_NTYPES; H5_INC_ENUM(H5FD_mem_t,mt)) {
            memb_fapl[mt] = H5P_DEFAULT;
            sprintf(sv[mt], "%%s-%c.h5", multi_letters[mt]);
            memb_name[mt] = sv[mt];
            memb_addr[mt] = MAX(mt-1,0)*(HADDR_MAX/10);
        }

        if (H5Pset_fapl_multi(fapl, memb_map, memb_fapl, memb_name,
                              memb_addr, FALSE)<0) {
            return -1;
        }
#endif
	    /* Multi-file driver, general case of the split driver */
	    H5FD_mem_t memb_map[H5FD_MEM_NTYPES];
	    hid_t memb_fapl[H5FD_MEM_NTYPES];
	    const char *memb_name[H5FD_MEM_NTYPES];
	    char sv[H5FD_MEM_NTYPES][1024];
	    haddr_t memb_addr[H5FD_MEM_NTYPES];
        H5FD_mem_t	mt;

	    memset(memb_map, 0, sizeof(memb_map));
	    memset(memb_fapl, 0, sizeof(memb_fapl));
	    memset(memb_name, 0, sizeof(memb_name));
	    memset(memb_addr, 0, sizeof(memb_addr));
        memset(sv, 0, sizeof(sv));
        
	    for (mt=H5FD_MEM_DEFAULT; mt<H5FD_MEM_NTYPES; H5_INC_ENUM(H5FD_mem_t,mt))
            memb_map[mt] = mt;

        memb_map[H5FD_MEM_DEFAULT]=H5FD_MEM_SUPER;

        /* superblock */
	    memb_fapl[H5FD_MEM_SUPER] = H5P_DEFAULT;
	    sprintf(sv[H5FD_MEM_SUPER], "%%s-%c.h5", 's');
	    memb_name[H5FD_MEM_SUPER] = sv[H5FD_MEM_SUPER];
        memb_addr[H5FD_MEM_SUPER] = 0*(HADDR_MAX/6);

        /* b-tree data */
	    memb_fapl[H5FD_MEM_BTREE] = H5P_DEFAULT;
	    sprintf(sv[H5FD_MEM_BTREE], "%%s-%c.h5", 'b');
	    memb_name[H5FD_MEM_BTREE] = sv[H5FD_MEM_BTREE];
        memb_addr[H5FD_MEM_BTREE] = 1*(HADDR_MAX/6);

        /* raw data */
	    memb_fapl[H5FD_MEM_DRAW] = H5P_DEFAULT;
	    sprintf(sv[H5FD_MEM_DRAW], "%%s-%c.h5", 'r');
	    memb_name[H5FD_MEM_DRAW] = sv[H5FD_MEM_DRAW];
        memb_addr[H5FD_MEM_DRAW] = 2*(HADDR_MAX/6);

        /* global heap data */
	    memb_fapl[H5FD_MEM_GHEAP] = H5P_DEFAULT;
	    sprintf(sv[H5FD_MEM_GHEAP], "%%s-%c.h5", 'g');
	    memb_name[H5FD_MEM_GHEAP] = sv[H5FD_MEM_GHEAP];
        memb_addr[H5FD_MEM_GHEAP] = 3*(HADDR_MAX/6);

        /* local heap data */
	    memb_fapl[H5FD_MEM_LHEAP] = H5P_DEFAULT;
	    sprintf(sv[H5FD_MEM_LHEAP], "%%s-%c.h5", 'l');
	    memb_name[H5FD_MEM_LHEAP] = sv[H5FD_MEM_LHEAP];
        memb_addr[H5FD_MEM_LHEAP] = 4*(HADDR_MAX/6);

        /* object headers */
	    memb_fapl[H5FD_MEM_OHDR] = H5P_DEFAULT;
	    sprintf(sv[H5FD_MEM_OHDR], "%%s-%c.h5", 'o');
	    memb_name[H5FD_MEM_OHDR] = sv[H5FD_MEM_OHDR];
        memb_addr[H5FD_MEM_OHDR] = 5*(HADDR_MAX/6);
 
	    ret=H5Pset_fapl_multi(fapl, memb_map, memb_fapl, memb_name,memb_addr, FALSE);
        VRFY((ret>=0), "H5Pset_fapl_multi");

    } else if (strstr(driver, "family")) {
        hsize_t fam_size = 32*1024; /*32 KB*/

	/* Family of files, each 32KB and using the default driver */
	    ret=H5Pset_fapl_family(fapl, fam_size, H5P_DEFAULT);
        VRFY((ret>=0), "H5Pset_fapl_family");
    } else if (strstr(driver, "log")) {
#ifdef H5_WANT_H5_V1_4_COMPAT
        long verbosity = 1;

        /* Log file access */
        ret=H5Pset_fapl_log(fapl, logfile, (int)verbosity);
        VRFY((ret>=0), "H5Pset_fapl_log");

#else /* H5_WANT_H5_V1_4_COMPAT */
        unsigned log_flags = H5FD_LOG_LOC_IO;

        /* Log file access */
        ret=H5Pset_fapl_log(fapl, logfile, log_flags, 0);
        VRFY((ret>=0), "H5Pset_fapl_log");
#endif /* H5_WANT_H5_V1_4_COMPAT */
    } else {
	/* Unknown driver */
    	return -1;
    }

    return fapl;
} /* h5_fileaccess() */



/*
* This function prepares the filename by appending the suffix needed
* by the file driver.
*/

static void h5_fixname(char *fullname, hid_t fapl)
{
    const char     *suffix = ".h5";     /* suffix has default */
    hid_t           driver = -1;


    VRFY((fullname), "");

    /* figure out the suffix */
    if (H5P_DEFAULT != fapl) {

	    driver = H5Pget_driver(fapl);
        VRFY((driver>=0), "H5Pget_driver");

	    if (H5FD_FAMILY == driver)
	        suffix = "%05d.h5";
	    else if (H5FD_CORE == driver || H5FD_MULTI == driver)
	        suffix = NULL;
    }

    /* Append a suffix */
    if (suffix) 
	    strcat(fullname, suffix);

} /* h5_fixname() */


/*
* This function defines a creation property list for a non-standard
* superblock.
*/

static hid_t 
alt_superblock(void)
{
    hid_t fcpl; /* property list */
    herr_t  ret;

    /* alternate superblock parameters values */
    const hsize_t userblock_size = 1024;
    const hsize_t offset_size = 8;
    const hsize_t length_size = 8;
    const hsize_t sym_intern_k = 32;
    const hsize_t sym_leaf_k = 8;
    const hsize_t i_store_k = 64;


    fcpl=H5Pcreate(H5P_FILE_CREATE);
    VRFY((fcpl>=0), "H5Pcreate");

    /* inserts an userblock at the beginning of the file */
    ret = H5Pset_userblock(fcpl, userblock_size);
    VRFY((ret>=0), "H5Pset_userblock");

    /* defines sizes for offset and length fields */
    ret = H5Pset_sizes(fcpl, offset_size, length_size);
    VRFY((ret>=0), "H5Pset_sizes");

    /* define sizes for b-tree parameters */
    ret = H5Pset_sym_k(fcpl, sym_intern_k, sym_leaf_k);
    VRFY((ret>=0), "H5Pset_sym_k");

    /* define k for indexed storage */
    ret = H5Pset_istore_k(fcpl, i_store_k);
    VRFY((ret>=0), "H5Pset_istore_k");

    return fcpl;
} /* alt_superblock() */



/*
* This function creates a file. It returns a file handle.
*/
static hid_t 
create_file(char *name, char *driver, char *superblock)
{
    hid_t fid, fapl, fcpl; /* HDF5 IDs */
    char fname[64]; /* file name */
    herr_t ret;

    strcpy(fname, name);

    VRFY(isalnum(*name),"filename");
    
    /* definition of access property list */
    if ((fapl = h5_fileaccess(driver))<0)
        fapl = H5P_DEFAULT;

    /* definition of creation property list */
    if (!strcmp(superblock, "alternate"))
        fcpl = alt_superblock();
    else
        fcpl = H5P_DEFAULT;

#if H5_LIBVERSION == 18 /* library release >= 1.8 */

    if (!strcmp(superblock, "new")) {
	ret = H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST);
	VRFY((ret>=0), "H5Pset_libver_bounds");
    }

    /* use the parameter "superblock" to set SOHM */
    if (!strcmp(superblock, "sohm")) {

	ret = H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST);
	VRFY((ret>=0), "H5Pset_libver_bounds");

	fcpl= H5Pcreate(H5P_FILE_CREATE);
	ret = H5Pset_shared_mesg_nindexes(fcpl, 1);
	VRFY((ret >= 0), "H5Pset_shared_mesg_nindexes");
	ret = H5Pset_shared_mesg_index(fcpl, 0, H5O_SHMESG_ATTR_FLAG, 2);
	VRFY((ret >= 0), "H5Pset_shared_mesg_index");
    }
#endif

    /* append appropriate suffix to the file name */
    h5_fixname(fname, fapl);

    /* create a file */  
    printf("Create %s: ", fname);
    fid = H5Fcreate(fname, H5F_ACC_TRUNC, fcpl, fapl);
    VRFY((fid>=0), "H5Fcreate");

    /* close file */
    ret = H5Pclose(fapl);
    VRFY((ret>=0), "H5Pclose");

    ret = H5Pclose(fcpl);
    VRFY((ret>=0), "H5Pclose");

    return fid;
} /* create_file() */

/*
 * This function closes a file.
 */
static void 
close_file(hid_t fid, char *options)
{
    herr_t ret;

    /* close file */
    ret = H5Fclose(fid);
    VRFY((ret>=0), "H5Fclose");

} /* close_file() */

/*
 * This fuction generates several (empty) group structures according to 'option':
 * 	HIERARCHICAL: creates a binary tree,
 * 	MULTIPATH: creates a graph in which some groups at the same level can be
 * 		   reached through multiple paths,
 * 	CYCLICAL: creates a graph in which some groups point to their parents or
 * 		  grandparents. All pointers are implemented as HARDLINKS.
 *
 * The names of the groups are generated using 'prefix'. The number of levels in
 * the group structure is specified by 'height'.
 */
static void 
gen_group_struct(hid_t parent_id, char* prefix, int height, int option)
{
    hid_t child_gid0, child_gid1; /* groups IDs */
    herr_t ret;

    char gname0[64]; /* groups names */
    char gname1[64];
    char child_name[64];

    /* create child group 0 */
    sprintf(gname0, "%s_%d", prefix, 0);
    child_gid0 = H5Gcreate(parent_id, gname0, 0);
    VRFY((child_gid0 >= 0), "H5Gcreate");

    /* create child group 1 */
    sprintf(gname1, "%s_%d", prefix, 1);
    child_gid1 = H5Gcreate(parent_id, gname1, 0);
    VRFY((child_gid1 >= 0), "H5Gcreate");

    /* recursive calls for remaining levels */        
    if( height > 1 ){


        /* create subtree at child 0 */
        gen_group_struct(child_gid0, gname0, height-1, option);

        /* create subtree at child 1 */
        gen_group_struct(child_gid1, gname1, height-1, option);

        switch(option){

        case HIERARCHICAL: /* binary tree already constructed */
            break;

        case MULTIPATH:
            /* a third entry in child_gid1 points to a child of child_gid_0*/
            ret = H5Glink2(child_gid0, strcat(gname0,"_0"), H5G_LINK_HARD, child_gid1, strcat(gname1, "_2"));
            VRFY((ret>=0), "H5Glink2");
            break;

        case CYCLICAL:
            /* a third entry in child_gid1 points to its grandparent. However, if
            grandparent is the root, the entry will point to child_gid1. */
            sprintf(child_name, "%s_%d", gname1, 2);
            ret = H5Glink2(parent_id, ".", H5G_LINK_HARD, child_gid1, child_name);
            VRFY((ret>=0), "H5Glink2");

            /* a fourth entry in child_gid1 points its parent */
            sprintf(child_name, "%s_%d", gname1, 3);
            ret = H5Glink2(child_gid1, ".", H5G_LINK_HARD, child_gid1, child_name);
            VRFY((ret>=0), "H5Glink2");
            break;
               
        default:
            
            VRFY(FALSE, "Invalid option");
        }
    }

    /* close groups */
    ret = H5Gclose(child_gid0);
    VRFY((ret>=0), "H5Gclose");

    ret = H5Gclose(child_gid1);
    VRFY((ret>=0), "H5Gclose");
} /* gen_group_struct() */



/*
* This function generates a binary tree group structure. At each group node, one of the
* child groups will contain a dataset while the other is empty based on the argument
* 'data_child'. The dataset placement criteria reverses at every level of the tree.
* The names of the groups are generated using 'prefix'. The number of levels in
* the group structure is specified by 'height'.
*/
static void 
gen_group_datasets(hid_t parent_id, char* prefix, int height, int data_child)
{
    hid_t child_gid0, child_gid1, gid; /* groups IDs */
    hid_t dset_id, dspace_id;
    hsize_t dims[RANK];
    herr_t ret;

    char gname0[64]; /* groups names */
    char gname1[64];
    char child_name[64];

    int *buffer;
    long int i;

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* create left child group */
    sprintf(gname0, "%s_%d", prefix, 0);
    child_gid0 = H5Gcreate(parent_id, gname0, 0);
    VRFY((child_gid0 >= 0), "H5Gcreate");

    /* create right child group */
    sprintf(gname1, "%s_%d", prefix, 1);
    child_gid1 = H5Gcreate(parent_id, gname1, 0);
    VRFY((child_gid1 >= 0), "H5Gcreate");

    /* selection of the group for placement of dataset */
    gid = data_child ? child_gid1 : child_gid0;

    for(i=0; i < RANK; i++)
        dims[i] = SIZE;

    /* create dataspace */
    dspace_id = H5Screate_simple(RANK, dims, NULL);
    VRFY((dspace_id>=0), "H5Screate_simple");

    /* create dataset */
    dset_id = H5Dcreate(gid, DATASET_PREFIX, H5T_NATIVE_INT, dspace_id, H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");

    /* allocate buffer and assign data */
    buffer = (int *)malloc(pow(SIZE,RANK)*sizeof(int));
    VRFY((buffer!=NULL), "malloc");

    for (i=0; i < pow(SIZE,RANK); i++)
        buffer[i] = i%SIZE;

    /* write buffer into dataset */
    ret = H5Dwrite(dset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
    VRFY((ret>=0), "H5Dwrite");

    free(buffer);

    /* close dataset */
    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* close dataspace */
    ret = H5Sclose(dspace_id);
    VRFY((ret>=0), "H5Sclose");

    /* recursive calls for remaining levels */        
    if( height > 1 ){

        /* dataset placement criteria reversed */
        /* create subtree at child 0 */
        gen_group_datasets(child_gid0, gname0, height-1, !data_child);

        /* create subtree at child 1 */
        gen_group_datasets(child_gid1, gname1, height-1, data_child);
    
    }

    /* close groups */
   ret = H5Gclose(child_gid0);
   VRFY((ret>=0), "H5Gclose");

   ret = H5Gclose(child_gid1);
   VRFY((ret>=0), "H5Gclose");
} /* gen_group_datasets() */

/*
 * This function generates a recursive nesting of 'height' groups. The names of
 * the groups are generated using 'prefix'. The generated structure tests the format
 * validation of a large number of groups.
 */
static void 
gen_linear_rec(hid_t parent_id, char *prefix, unsigned int height)
{
    hid_t child_gid; /* group ID */
    char gname[64]; /* group name */
    herr_t ret;

    /* appending level to the name prefix */
    sprintf(gname, "%s_%d", prefix, height);

    /* create new group within parent_id group */
    child_gid = H5Gcreate(parent_id, gname, 0);
    VRFY((child_gid>=0), "H5Gcreate");

    /* recursive call for the remaining levels */        
    if( height > 1 )
        gen_linear_rec(child_gid, prefix, height-1);

    /* close group */
    ret = H5Gclose(child_gid);
    VRFY((ret>=0), "H5Gclose");
} /* gen_linear_rec() */

/*
 * This function generates NUM_GROUPS empty groups at the root and a recursive
 * nesting of NUM_GROUPS within one group. The generated structure tests
 * the format validation of a large number of groups.
 */
static void 
gen_linear(hid_t fid)
{

    hid_t gid; /* group ID */
    herr_t ret;
    char gname[64]; /* group name */
    int i;

    VRFY((NUM_GROUPS > 0), "");

    /* create NUM_GROUPS at the root */
    for (i=0; i<NUM_GROUPS; i++){
        
        /* append index to a name prefix */
        sprintf(gname, "group%d", i);

        /* create a group */
        gid = H5Gcreate(fid, gname, 0);
        VRFY((gid>=0), "H5Gcreate");

        /* close group*/
        ret = H5Gclose(gid);
        VRFY((ret>=0), "H5Gclose");
    }

    /* create a linear nesting of NUM_GROUPS within
    one group */
    gen_linear_rec(fid, "rec_group", NUM_GROUPS);

} /* gen_linear() */

/*
 *
 * This function generates a dataset per allowed dimensionality, i.e. one
 * dataset is 1D, another one is 2D, and so on, up to dimension H5S_MAX_RANK.
 * The option 'fill_dataset' determines if data is to be written in the datasets.
 */
static void 
gen_rank_datasets(hid_t oid, int fill_dataset)
{

    hid_t dset_id, dspace_id; /* HDF5 IDs */
    hsize_t dims[H5S_MAX_RANK]; /* H5S_MAX_RANK is the maximum rank allowed
                                 in the HDF5 library */
    herr_t ret;                 

    int *buffer;    /* buffer for writing the dataset */
    int size;       /* size for all the dimensions of the current rank */

    int rank, i;

    char dname[64]; /* dataset name */

    /* iterates over all possible ranks (zero rank is not considered) */
    for (rank=1; rank<=H5S_MAX_RANK; rank++){

        /* all the dimensions for a particular rank have the same size */
        size=(rank<=10)?4:((rank<=20)?2:1);

        for(i=0; i < rank; i++)
            dims[i] = size;

        /* generate name for current dataset */
        sprintf(dname, "%s_%d", DATASET_PREFIX, rank);

        /* create dataspace */
        dspace_id = H5Screate_simple(rank, dims, NULL);
        VRFY((dspace_id>=0), "H5Screate_simple");

        /* create dataset */
        dset_id = H5Dcreate(oid, dname, H5T_NATIVE_INT, dspace_id, H5P_DEFAULT);
        VRFY((dset_id>=0), "H5Dcreate");
        
        /* dataset is to be full */
        if (fill_dataset){

            /* allocate buffer and assigns data */
            buffer = (int *)malloc(pow(size,rank)*sizeof(int));
            VRFY((buffer!=NULL), "malloc");

            for (i=0; i < pow(size,rank); i++)
                buffer[i] = i%size;

            /* writes buffer into dataset */
            ret = H5Dwrite(dset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
            VRFY((ret>=0), "H5Dwrite");

            free(buffer);
        }

        /* close dataset */
        ret = H5Dclose(dset_id);
        VRFY((ret>=0), "H5Dclose");

        /* close dataspace */
        ret = H5Sclose(dspace_id);
        VRFY((ret>=0), "H5Sclose");

    }
} /* gen_rank_datasets() */


/*
 *
 * This function generates a dataset per basic datatype , i.e. one dataset is char,
 * another one is int, and so on. The option 'fill_dataset' determines whether data is
 * to be written into the dataset.
 */
static void 
gen_basic_types(hid_t oid, int fill_dataset)
{

    hid_t dset_id, dspace_id; /* HDF5 IDs */
    hid_t datatype, string_type1;
    hsize_t dims[RANK]; 
    herr_t ret;
    unsigned char *uchar_buffer;
    char *string_buffer;
    float *float_buffer;
    long int i;
    char *ntype_dset[]={"char","short","int","uint","long","llong",
        "float","double","ldouble", "bitfield","opaque","string"};

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        dims[i] = SIZE;

    /* create dataspace */
    dspace_id = H5Screate_simple(RANK, dims, NULL);
    VRFY((dspace_id>=0), "H5Screate_simple");

    /* allocate integer buffer and assign data */
    uchar_buffer = (unsigned char *)malloc(pow(SIZE,RANK)*sizeof(unsigned char));
    VRFY((uchar_buffer!=NULL), "malloc");
    
    /* allocate string buffer and assign data */
    string_buffer = (char *)malloc(STR_SIZE*pow(SIZE,RANK)*sizeof(char));
    VRFY((uchar_buffer!=NULL), "malloc");
    
    /* allocate float buffer and assign data */
    float_buffer = (float *)malloc(pow(SIZE,RANK)*sizeof(float));
    VRFY((float_buffer!=NULL), "malloc");

    /* Create a datatype to refer to */
    string_type1 = H5Tcopy (H5T_C_S1);
    VRFY((string_type1>=0), "H5Tcopy");

    ret = H5Tset_size (string_type1, STR_SIZE);
    VRFY((ret>=0), "H5Tset_size");

    /* fill buffer with data */
    for (i=0; i < pow(SIZE,RANK); i++){
        float_buffer[i] = uchar_buffer[i] = i%SIZE;
        strcpy(&string_buffer[i*STR_SIZE], "sample text");
    }

    /* iterate over basic datatypes */            
    for(i=0;i<NTYPES;i++){
 
        /* create dataset */
        dset_id = H5Dcreate(oid, ntype_dset[i], NATIVE_TYPE(i), dspace_id, H5P_DEFAULT);
        VRFY((dset_id>=0), "H5Dcreate");

        /* fill dataset with data */
        if (fill_dataset){
            /* since some datatype conversions are not allowed, selects buffer with
            compatible datatype class */
            switch (H5Tget_class(NATIVE_TYPE(i))) {
            case H5T_INTEGER:
                ret = H5Dwrite(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, uchar_buffer);
                break;
            case H5T_FLOAT:
                ret = H5Dwrite(dset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, float_buffer);
                break;
            case H5T_BITFIELD:
                ret = H5Dwrite(dset_id, H5T_NATIVE_B8, H5S_ALL, H5S_ALL, H5P_DEFAULT, uchar_buffer);
                break;
            case H5T_OPAQUE:
                ret = H5Dwrite(dset_id, H5T_NATIVE_OPAQUE, H5S_ALL, H5S_ALL, H5P_DEFAULT, uchar_buffer);
                break;
            case H5T_STRING:
                ret = H5Dwrite(dset_id, string_type1, H5S_ALL, H5S_ALL, H5P_DEFAULT, string_buffer);
                break;
            default:
                VRFY(FALSE, "Invalid datatype conversion");
            }
            VRFY((ret>=0), "H5Dwrite");
        }

        /* close dataset */
        ret = H5Dclose(dset_id);
        VRFY((ret>=0), "H5Dclose");
    }

    /* close dataspace */
    ret = H5Sclose(dspace_id);
    VRFY((ret>=0), "H5Sclose");

    free(uchar_buffer);
    free(float_buffer);
    free(string_buffer);
} /* gen_basic_types() */

/*
 *
 * This function generates a dataset with a compound datatype. The option
 * 'fill_dataset' determines whether data is to be written into the dataset.
 */
static void 
gen_compound(hid_t oid, int fill_dataset)
{
    s1_t *s1;                   /* compound struct */
    hid_t dset_id, dspace_id;   /* dataset and dataspace IDs */
    hid_t s1_tid, s2_tid, array_dt; /* datatypes IDs */

    hsize_t dims[RANK];         /* dataspace dimension size */
    hsize_t memb_size[1] = {4}; /* array datatype dimension */
    herr_t ret;

    long int i;

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        dims[i] = SIZE;

    /* Create the data space */
    dspace_id = H5Screate_simple (RANK, dims, NULL);
    VRFY((dspace_id>=0),"H5Screate_simple");
    
    /* create compound datatype */
    s2_tid = H5Tcreate (H5T_COMPOUND, sizeof(s2_t));
    VRFY((s2_tid>=0), "H5Tcreate");

    /* compound datatype includes an array */
    array_dt=H5Tarray_create(H5T_NATIVE_INT, 1, memb_size, NULL);
    VRFY((array_dt>=0), "H5Tarray_create");
   
    /* define the fields of the compound datatype */
    if (H5Tinsert (s2_tid, "a", HOFFSET(s2_t,a), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s2_tid, "b", HOFFSET(s2_t,b), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s2_tid, "c", HOFFSET(s2_t,c), array_dt)<0 ||
        H5Tinsert (s2_tid, "d", HOFFSET(s2_t,d), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s2_tid, "e", HOFFSET(s2_t,e), H5T_NATIVE_INT)<0)
        VRFY(FALSE, "H5Tinsert");

    /* create compound datatype */
    s1_tid = H5Tcreate (H5T_COMPOUND, sizeof(s1_t));
    VRFY((s1_tid>=0), "H5Tcreate");
   
    /* define the fields of the compound datatype */
    if (H5Tinsert (s1_tid, "a", HOFFSET(s1_t,a), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s1_tid, "b", HOFFSET(s1_t,b), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s1_tid, "c", HOFFSET(s1_t,c), array_dt)<0 ||
        H5Tinsert (s1_tid, "d", HOFFSET(s1_t,d), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s1_tid, "e", HOFFSET(s1_t,e), H5T_NATIVE_INT)<0 ||
        H5Tinsert (s1_tid, "s2", HOFFSET(s1_t,s2), s2_tid)<0)
        VRFY(FALSE, "H5Tinsert");

    /* close array datatype */
    ret = H5Tclose(array_dt);
    VRFY((ret>=0), "H5Tclose");

    /* Create the dataset */
    dset_id = H5Dcreate (oid, "compound1", s1_tid, dspace_id, H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");

    if (fill_dataset) {
        /* allocate buffer and assign data */
        s1 = (s1_t *)malloc(pow(SIZE,RANK)*sizeof(s1_t));
        VRFY((s1!=NULL), "malloc");
    
        /* Initialize the dataset */
        for (i=0; i<pow(SIZE,RANK); i++) {
	        s1[i].a = 8*i+0;
	        s1[i].b = 2000+2*i;
	        s1[i].c[0] = 8*i+2;
	        s1[i].c[1] = 8*i+3;
	        s1[i].c[2] = 8*i+4;
	        s1[i].c[3] = 8*i+5;
	        s1[i].d = 2001+2*i;
	        s1[i].e = 8*i+7;
	        s1[i].s2.a = 8*i+0;
	        s1[i].s2.b = 2000+2*i;
	        s1[i].s2.c[0] = 8*i+2;
	        s1[i].s2.c[1] = 8*i+3;
	        s1[i].s2.c[2] = 8*i+4;
	        s1[i].s2.c[3] = 8*i+5;
	        s1[i].s2.d = 2001+2*i;
	        s1[i].s2.e = 8*i+7;

        }

        /* Write the data */
        ret = H5Dwrite (dset_id, s1_tid, H5S_ALL, H5S_ALL, H5P_DEFAULT, s1);
        VRFY((ret>=0), "H5Dwrite");

        free(s1);
    }

    /* close dataset */
    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* close dataspace */
    ret = H5Sclose(dspace_id);
    VRFY((ret>=0), "H5Sclose");
} /* gen_compound() */

/*
 *
 * This function generates a dataset with a VL datatype. The option
 * 'fill_dataset' determines whether data is to be written into the dataset.
 */
static void 
gen_vl(hid_t oid, int fill_dataset)
{
    hid_t dset_id, dspace_id;   /* dataset and dataspace IDs */
    hid_t s1_tid, array_dt;     /* datatypes IDs */

    hsize_t dims[RANK];         /* dataspace dimension size */
    hsize_t memb_size[1] = {4}; /* array datatype dimension */
    herr_t ret;

    hvl_t *wdata;   /* Information to write */
    hid_t tid1;         /* Datatype ID			*/
    hsize_t size;       /* Number of bytes which will be used */
    unsigned i,j;       /* counting variables */
    size_t mem_used=0;  /* Memory used during allocation */


    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        dims[i] = SIZE;

    /* Create dataspace for datasets */
    dspace_id = H5Screate_simple(RANK, dims, NULL);
    VRFY((dspace_id>=0), "H5Screate_simple");

    /* Create a datatype to refer to */
    tid1 = H5Tvlen_create(H5T_NATIVE_UINT);
    VRFY((tid1>=0), "H5Tvlen_create");

    /* Create a dataset */
    dset_id = H5Dcreate(oid,"Dataset1", tid1, dspace_id, H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");

    if (fill_dataset) {
        /* allocate buffer */
        wdata = (hvl_t *)malloc(pow(SIZE,RANK)*sizeof(hvl_t));
        VRFY((wdata!=NULL), "malloc");

        /* initialize VL data to write */
        for (i=0; i<pow(SIZE,RANK); i++) {
            wdata[i].p=malloc((i+1)*sizeof(unsigned int));
            wdata[i].len=i+1;
            for(j=0; j<(i+1); j++)
                ((unsigned int *)wdata[i].p)[j]=i*10+j;
        } /* end for */

        /* write dataset */
        ret=H5Dwrite(dset_id,tid1,H5S_ALL,H5S_ALL,H5P_DEFAULT,wdata);
        VRFY((ret>=0), "H5Dwrite");

        free(wdata);
    }

    /* Close Dataset */
    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* Close datatype */
    ret = H5Tclose(tid1);
    VRFY((ret>=0), "H5Tclose");

    /* Close disk dataspace */
    ret = H5Sclose(dspace_id);
    VRFY((ret>=0), "H5Sclose");

} /* gen_vl() */

/*
 *
 * This function generates a dataset with an enumerated datatype. The option
 * 'fill_dataset' determines whether data is to be written into the dataset.
 */
static void 
gen_enum(hid_t oid, int fill_dataset)
{
    hid_t	type, dspace_id, dset_id, ret;  /* HDF5 IDs */
    c_e1 val;
    hsize_t dims[RANK]; /* dataspace dimension size */
    c_e1 *data1;        /* data buffer */
    hsize_t	i, j;

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        dims[i] = SIZE;

    /* create enumerated datatype */
    type = H5Tcreate(H5T_ENUM, sizeof(c_e1));
    VRFY((type>=0), "H5Tcreate");

    /* insertion of enumerated values */
    if ((H5Tenum_insert(type, "RED",   CPTR(val, E1_RED  ))<0) ||
        (H5Tenum_insert(type, "GREEN", CPTR(val, E1_GREEN))<0) ||
        (H5Tenum_insert(type, "BLUE",  CPTR(val, E1_BLUE ))<0) ||
        (H5Tenum_insert(type, "WHITE", CPTR(val, E1_WHITE))<0) ||
        (H5Tenum_insert(type, "BLACK", CPTR(val, E1_BLACK))<0))
        VRFY(FALSE, "H5Tenum_insert");

    /* create dataspace */
    dspace_id=H5Screate_simple(RANK, dims, NULL);
    VRFY((dspace_id>=0), "H5Screate_simple");

    /* create dataset */
    dset_id=H5Dcreate(oid, "color_table", type, dspace_id, H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");
   
    if (fill_dataset) {
        /* allocate buffer */
        data1 = (c_e1 *)malloc(pow(SIZE,RANK)*sizeof(c_e1));
        VRFY((data1!=NULL), "malloc");

        /* allocate and initialize enumerated data to write */
        for (i=0; i<pow(SIZE,RANK); i++) {
            j=i%NUM_VALUES;
            data1[i]=(j<1)?E1_RED:((j<2)?E1_GREEN:((j<3)?E1_BLUE:((j<4)?E1_WHITE:
            E1_BLACK)));
        } /* end for */

        /* write dataset */
        ret=H5Dwrite(dset_id, type, dspace_id, dspace_id, H5P_DEFAULT, data1);
        VRFY((ret>=0), "H5Dwrite");

        free(data1);
    }

    /* close dataset */
    ret=H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* close dataspace */
    ret=H5Sclose(dspace_id);
    VRFY((ret>=0), "H5Sclose");

    /* close datatype */
    ret=H5Tclose(type);
    VRFY((ret>=0), "H5Tclose");
} /* gen_enum() */


/*
 *
 * This function generates 1 group with 2 datasets and a compound datatype.
 * Then it creates a dataset containing references to these 4 objects.
 */
static void 
gen_reference(hid_t oid)
{
    hid_t		dset_id;	/* Dataset ID			*/
    hid_t		group;      /* Group ID             */
    hid_t		dspace_id;       /* Dataspace ID			*/
    hid_t		tid1;       /* Datatype ID			*/
    hsize_t		dims[RANK];
    hobj_ref_t      *wbuf;      /* buffer to write to disk */
    unsigned      *tu32;      /* Temporary pointer to uint32 data */
    int        i;          /* counting variables */
    const char *write_comment="Foo!"; /* Comments for group */
    herr_t		ret;		/* Generic return value		*/

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        dims[i] = SIZE;

    /* Allocate write & read buffers */
    wbuf=malloc(MAX(sizeof(unsigned),sizeof(hobj_ref_t))*pow(SIZE,RANK));

    /* Create dataspace for datasets */
    dspace_id = H5Screate_simple(RANK, dims, NULL);
    VRFY((dspace_id>=0), "H5Screate_simple");

    /* Create a group */
    group=H5Gcreate(oid,"Group1",(size_t)-1);
    VRFY((group>=0), "H5Gcreate");

    /* Set group's comment */
    ret=H5Gset_comment(group,".",write_comment);
    VRFY((ret>=0), "H5Gset_comment");

    /* Create a dataset (inside Group1) */
    dset_id=H5Dcreate(group,"Dataset1",H5T_NATIVE_UINT,dspace_id,H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");

    for(tu32=(unsigned *)wbuf,i=0; i<pow(SIZE,RANK); i++)
        *tu32++=i*3;

    /* Write selection to disk */
    ret=H5Dwrite(dset_id,H5T_NATIVE_UINT,H5S_ALL,H5S_ALL,H5P_DEFAULT,wbuf);
    VRFY((ret>=0), "H5Dwrite");

    /* Close Dataset */
    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* Create another dataset (inside Group1) */
    dset_id=H5Dcreate(group,"Dataset2",H5T_NATIVE_UCHAR,dspace_id,H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");

    /* Close Dataset */
    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* Create a datatype to refer to */
    tid1 = H5Tcreate (H5T_COMPOUND, sizeof(s3_t));
    VRFY((tid1>=0), "H5Tcreate");

    /* Insert fields */
    ret=H5Tinsert (tid1, "a", HOFFSET(s3_t,a), H5T_NATIVE_INT);
    VRFY((ret>=0), "H5Tinsert");

    ret=H5Tinsert (tid1, "b", HOFFSET(s3_t,b), H5T_NATIVE_INT);
    VRFY((ret>=0), "H5Tinsert");

    ret=H5Tinsert (tid1, "c", HOFFSET(s3_t,c), H5T_NATIVE_FLOAT);
    VRFY((ret>=0), "H5Tinsert");

    /* Save datatype for later */
    ret=H5Tcommit (group, "Datatype1", tid1);
    VRFY((ret>=0), "H5Tcommit");

    /* Close datatype */
    ret = H5Tclose(tid1);
    VRFY((ret>=0), "H5Tclose");

    /* Close group */
    ret = H5Gclose(group);
    VRFY((ret>=0), "H5Gclose");

    /* Create a dataset */
    dset_id=H5Dcreate(oid,"Dataset3",H5T_STD_REF_OBJ,dspace_id,H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate");
    
    for (i=0;i<((int)pow(SIZE,RANK))/4;i++){
        /* Create reference to dataset */
        ret = H5Rcreate(&wbuf[i*4+0],oid,"/Group1/Dataset1",H5R_OBJECT,-1);
        VRFY((ret>=0), "H5Rcreate");

        /* Create reference to dataset */
        ret = H5Rcreate(&wbuf[i*4+1],oid,"/Group1/Dataset2",H5R_OBJECT,-1);
        VRFY((ret>=0),"H5Rcreate");

        /* Create reference to group */
        ret = H5Rcreate(&wbuf[i*4+2],oid,"/Group1",H5R_OBJECT,-1);
        VRFY((ret>=0), "H5Rcreate");

        /* Create reference to named datatype */
        ret = H5Rcreate(&wbuf[i*4+3],oid,"/Group1/Datatype1",H5R_OBJECT,-1);
        VRFY((ret>=0), "H5Rcreate");
    }

    /* Write selection to disk */
    ret=H5Dwrite(dset_id,H5T_STD_REF_OBJ,H5S_ALL,H5S_ALL,H5P_DEFAULT,wbuf);
    VRFY((ret>=0), "H5Dwrite");

    /* Close disk dataspace */
    ret = H5Sclose(dspace_id);
    VRFY((ret>=0), "H5Sclose");

    /* Close Dataset */
    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    /* Free memory buffers */
    free(wbuf);

} /* gen_reference() */

/*
 * This function is called by gen_filters in order to create a dataset with
 * the respective filters.
 */
static void 
gen_filter_internal(hid_t fid, const char *name, hid_t dcpl, hsize_t *dset_size)
{
    hid_t		dataset;        /* Dataset ID */
    hid_t		dxpl;           /* Dataset xfer property list ID */
    hid_t		write_dxpl;     /* Dataset xfer property list ID for writing */
    hid_t		sid;            /* Dataspace ID */
    hsize_t	size[RANK];           /* Dataspace dimensions */
    int *points;
    void		*tconv_buf = NULL;      /* Temporary conversion buffer */
    hsize_t		i, j, n;        /* Local index variables */
    herr_t              ret;         /* Error status */


    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        size[i] = SIZE+szip_pixels_per_block;

    /* Create the data space */
    sid = H5Screate_simple(RANK, size, NULL);
    VRFY((sid>=0), "H5Screate_simple");

    /*
     * Create a small conversion buffer to test strip mining. We
     * might as well test all we can!
     */
    dxpl = H5Pcreate (H5P_DATASET_XFER);
    VRFY((dxpl>=0), "H5Pcreate");

    tconv_buf = malloc (1000);
#ifdef H5_WANT_H5_V1_4_COMPAT
    ret=H5Pset_buffer (dxpl, (hsize_t)1000, tconv_buf, NULL);
#else /* H5_WANT_H5_V1_4_COMPAT */
    ret=H5Pset_buffer (dxpl, (size_t)1000, tconv_buf, NULL);
#endif /* H5_WANT_H5_V1_4_COMPAT */
    VRFY((ret>=0), "H5Pset_buffer");

    write_dxpl = H5Pcopy(dxpl);
    VRFY((write_dxpl>=0), "H5Screate_simple");

    /* Check if all the filters are available */
    VRFY((H5Pall_filters_avail(dcpl)==TRUE), "Incorrect filter availability");

    /* Create the dataset */
    dataset = H5Dcreate(fid, name, H5T_NATIVE_INT, sid, dcpl);
    VRFY((dataset>=0), "H5Dcreate");

    /*----------------------------------------------------------------------
     * Test filters by setting up a chunked dataset and writing
     * to it.
     *----------------------------------------------------------------------
     */

    points = (int *)malloc(pow(SIZE,RANK)*sizeof(int));
    VRFY((points!=NULL), "malloc");

    for (i=0; i < pow(SIZE,RANK); i++)
        points[i]= (int)rand();

    ret=H5Dwrite(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, write_dxpl, points);
    VRFY((ret>=0), "H5Dwrite");
	
    *dset_size=H5Dget_storage_size(dataset);
    VRFY((*dset_size!=0), "H5Dget_storage_size");

    ret=H5Dclose(dataset);
    VRFY((ret>=0), "H5Dclose");

    ret=H5Sclose(sid);
    VRFY((ret>=0), "H5Sclose");

    ret=H5Pclose (dxpl);
    VRFY((ret>=0), "H5Pclose");

    free(points);
    free (tconv_buf);

} /* gen_filter_internal() */


/*
 * This function generates several datasets with different filters. Filters are used
 * individually and in combinations. The combination of filters in different order is
 * also tested.
 */
static void 
gen_filters(hid_t file)
{

    hid_t	dc;                 /* Dataset creation property list ID */
    hsize_t chunk_size[RANK]; /* Chunk dimensions */
    hsize_t     null_size;          /* Size of dataset with null filter */
    hsize_t     fletcher32_size;       /* Size of dataset with Fletcher32 checksum */
    hsize_t     deflate_size;       /* Size of dataset with deflate filter */
    hsize_t     szip_size;       /* Size of dataset with szip filter */
    hsize_t     shuffle_size;       /* Size of dataset with shuffle filter */
    hsize_t     combo_size;     /* Size of dataset with shuffle+deflate filter */
    herr_t ret;
    const char *dset_deflate_name = "deflate"; /* datasets names */
    const char *dset_szip_name = "szip";
    const char *dset_shuffle_name = "shuffle";
    const char *dset_fletcher32_name = "fletcher32";
    const char *dset_fletcher32_name_2 = "fletcher32_2";
    const char *dset_fletcher32_name_3 = "fletcher32_3";
    const char *dset_shuf_def_flet_name = "shuffle+deflate+fletcher32";
    const char *dset_shuf_def_flet_name_2 = "shuffle+deflate+fletcher32_2";
    const char *dset_shuf_szip_flet_name = "shuffle+szip+fletcher32";
    const char *dset_shuf_szip_flet_name_2 = "shuffle+szip+fletcher32_2";
    const char *dset_bogus_name	 = "bogus";

    int i,j;

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        chunk_size[i] = SIZE/CHUNKING_FACTOR+szip_pixels_per_block;

    /*----------------------------------------------------------
     * STEP 0: Test null I/O filter by itself.
     *----------------------------------------------------------
     */
    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk(dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

#ifdef H5_WANT_H5_V1_4_COMPAT
    ret=H5Zregister (H5Z_FILTER_BOGUS, "bogus", filter_bogus);
#else /* H5_WANT_H5_V1_4_COMPAT */
    ret=H5Zregister (H5Z_BOGUS);
#endif /* H5_WANT_H5_V1_4_COMPAT */
    VRFY((ret>=0), "H5Zregister");

    ret=H5Pset_filter (dc, H5Z_FILTER_BOGUS, 0, 0, NULL);
    VRFY((ret>=0), "H5Pset_filter");

    gen_filter_internal(file,dset_bogus_name,dc,&null_size);

    /* Clean up objects used for this test */
    ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");


    /*----------------------------------------------------------
     * STEP 1: Test Fletcher32 Checksum by itself.
     *----------------------------------------------------------
     */
    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk(dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

    ret=H5Pset_filter(dc,H5Z_FILTER_FLETCHER32,0,0,NULL);
    VRFY((ret>=0), "H5Pset_filter");

    /* Enable checksum during read */
    gen_filter_internal(file,dset_fletcher32_name,dc,&fletcher32_size);
    
    VRFY((fletcher32_size>null_size), "size after checksumming is incorrect.");

    /* Clean up objects used for this test */
    ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");

    /*----------------------------------------------------------
     * STEP 2: Test deflation by itself.
     *----------------------------------------------------------
     */
    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk(dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

    ret=H5Pset_deflate(dc, 6);
    VRFY((ret>=0), "H5Pset_deflate");

    gen_filter_internal(file,dset_deflate_name,dc,&deflate_size);

    /* Clean up objects used for this test */
    ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");

  
#ifdef HAVE_SZIP

    /*----------------------------------------------------------
     * STEP 3: Test szip compression by itself.
     *----------------------------------------------------------
     */

	/* with encoder */
    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk(dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

	ret=H5Pset_szip(dc, szip_options_mask, szip_pixels_per_block);
    VRFY((ret>=0), "H5Pset_szip");

	gen_filter_internal(file,dset_szip_name,dc,&szip_size);

    ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");

#endif /* HAVE_SZIP */
    	
    /*----------------------------------------------------------
     * STEP 4: Test shuffling by itself.
     *----------------------------------------------------------
     */
    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk(dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

    ret=H5Pset_shuffle(dc);
    VRFY((ret>=0), "H5Pset_shuffle");

    gen_filter_internal(file,dset_shuffle_name,dc,&shuffle_size);

    VRFY((shuffle_size==null_size), "Shuffled size not the same as uncompressed size.");

    /* Clean up objects used for this test */
    ret=H5Pclose (dc);
    VRFY((dc>=0), "H5Pclose");

    /*----------------------------------------------------------
     * STEP 5: Test shuffle + deflate + checksum in any order.
     *----------------------------------------------------------
     */
    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk (dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

    ret=H5Pset_fletcher32(dc);
    VRFY((ret>=0), "H5Pset_fletcher32");

    ret=H5Pset_shuffle(dc);
    VRFY((ret>=0), "H5Pset_shuffle");

    ret=H5Pset_deflate (dc, 6);
    VRFY((ret>=0), "H5Pset_deflate");

    gen_filter_internal(file,dset_shuf_def_flet_name,dc,&combo_size);

    /* Clean up objects used for this test */
    ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");

    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk (dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

    ret=H5Pset_shuffle(dc);
    VRFY((ret>=0), "H5Pset_shuffle");

    ret=H5Pset_deflate (dc, 6);
    VRFY((ret>=0), "H5Pcreate");

    ret=H5Pset_fletcher32(dc);
    VRFY((ret>=0), "H5Pcreate");

    gen_filter_internal(file,dset_shuf_def_flet_name_2,dc,&combo_size);

    /* Clean up objects used for this test */
    ret=H5Pclose (dc);
    VRFY((ret>=0), "H5Pclose");

#ifdef HAVE_SZIP

    /*----------------------------------------------------------
     * STEP 6: Test shuffle + szip + checksum in any order.
     *----------------------------------------------------------
     */

    dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

    ret=H5Pset_chunk (dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

    ret=H5Pset_fletcher32(dc);
    VRFY((ret>=0), "H5Pset_fletcher");

    ret=H5Pset_shuffle(dc);
    VRFY((ret>=0), "H5Pset_shuffle");

	/* Make sure encoding is enabled */
	ret=H5Pset_szip(dc, szip_options_mask, szip_pixels_per_block);
    VRFY((ret>=0), "H5Pset_szip");

	gen_filter_internal(file,dset_shuf_szip_flet_name,dc,&combo_size);

    /* Clean up objects used for this test */
    ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");

    /* Make sure encoding is enabled */
	dc = H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dc>=0), "H5Pcreate");

	ret=H5Pset_chunk (dc, RANK, chunk_size);
    VRFY((ret>=0), "H5Pset_chunk");

	ret=H5Pset_shuffle(dc);
    VRFY((ret>=0), "H5Pset_shuffle");

	ret=H5Pset_szip(dc, szip_options_mask, szip_pixels_per_block);
    VRFY((ret>=0), "H5Pset_szip");

	ret=H5Pset_fletcher32(dc);
    VRFY((ret>=0), "H5Pset_fletcher32");

    gen_filter_internal(file,dset_shuf_szip_flet_name_2,dc,&combo_size);

	/* Clean up objects used for this test */
	ret=H5Pclose(dc);
    VRFY((ret>=0), "H5Pclose");

#endif /* HAVE_SZIP */
} /* gen_filters() */


/*
 * This function generates a dataset and a group with attributes. 
 */
static void 
gen_attr(hid_t fid1)
{
    hid_t		dataset;	/* Dataset ID			*/
    hid_t		group;	    /* Group ID			    */
    hid_t		sid1,sid2;	/* Dataspace ID			*/
    hid_t		attr, attr2;	    /* Attribute ID		*/
    hsize_t             attr_size;  /* storage size for attribute       */
    ssize_t             attr_name_size; /* size of attribute name       */
    char                *attr_name=NULL;    /* name of attribute        */
    hsize_t		dims1[RANK];
    hsize_t		dims2[] = {ATTR1_DIM1};
    hsize_t		dims3[] = {ATTR2_DIM1,ATTR2_DIM2};
    int       read_data1[ATTR1_DIM1]={0}; /* Buffer for reading 1st attribute */
    int         i;
    herr_t		ret;		/* Generic return value		*/

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        dims1[i] = SIZE;

    /* Create dataspace for dataset */
    sid1 = H5Screate_simple(RANK, dims1, NULL);
    VRFY((sid1>=0), "H5Screate_simple");

    /* Create a dataset */
    dataset=H5Dcreate(fid1,DATASET_PREFIX,H5T_NATIVE_UCHAR,sid1,H5P_DEFAULT);
    VRFY((dataset>=0), "H5Dcreate");

    /* Create dataspace for attribute */
    sid2 = H5Screate_simple(ATTR1_RANK, dims2, NULL);
    VRFY((sid2>=0), "H5Screate_simple");

    /* Create an attribute for the dataset */
    attr=H5Acreate(dataset,ATTR1_NAME,H5T_NATIVE_INT,sid2,H5P_DEFAULT);
    VRFY((attr>=0), "H5Acreate");

    /* Write attribute information */
    ret=H5Awrite(attr,H5T_NATIVE_INT,attr_data1);
    VRFY((ret>=0), "H5Awrite");

    /* Create an another attribute for the dataset */
    attr2=H5Acreate(dataset,ATTR1A_NAME,H5T_NATIVE_INT,sid2,H5P_DEFAULT);
    VRFY((attr2>=0), "H5Acreate");

    /* Write attribute information */
    ret=H5Awrite(attr2,H5T_NATIVE_INT,attr_data1a);
    VRFY((ret>=0), "H5Awrite");

    /* Close attribute */
    ret=H5Aclose(attr);
    VRFY((ret>=0), "H5Aclose");

    /* Close attribute */
    ret=H5Aclose(attr2);
    VRFY((ret>=0), "H5Aclose");

    ret = H5Sclose(sid1);
    VRFY((ret>=0), "H5Sclose");

    ret = H5Sclose(sid2);
    VRFY((ret>=0), "H5Sclose");

    /* Close Dataset */
    ret = H5Dclose(dataset);
    VRFY((ret>=0), "H5Dclose");

    /* Create group */
    group = H5Gcreate(fid1, GROUP_PREFIX, 0);
    VRFY((group>=0), "H5Gcreate");

    /* Create dataspace for attribute */
    sid2 = H5Screate_simple(ATTR2_RANK, dims3, NULL);
    VRFY((sid2>=0), "H5Screate_simple");

    /* Create an attribute for the group */
    attr=H5Acreate(group,ATTR2_NAME,H5T_NATIVE_INT,sid2,H5P_DEFAULT);
    VRFY((attr>=0), "H5Acreate");

    /* Write attribute information */
    ret=H5Awrite(attr,H5T_NATIVE_INT,attr_data2);
    VRFY((ret>=0), "H5Awrite");

    /* Close attribute */
    ret=H5Aclose(attr);
    VRFY((ret>=0), "H5Aclose");

    /* Close Attribute dataspace */
    ret = H5Sclose(sid2);
    VRFY((ret>=0), "H5Sclose");

    /* Close Group */
    ret = H5Gclose(group);
    VRFY((ret>=0), "H5Gclose");

}   /* gen_attr() */
/* test_attr_basic_write() */


/*
 *
 * This function commits several time datatypes and creates a dataset with
 * one of the time datatypes. 
 */
static void 
gen_time(hid_t file_id)
{
    hid_t       tid, sid, dsid;  /* identifiers */
    time_t      timenow, timethen;      /* Times */
    herr_t      status;

    tid = H5Tcopy (H5T_UNIX_D32LE);
    VRFY((tid>=0), "H5Tcopy");
    status = H5Tcommit(file_id, "Committed D32LE type", tid);
    VRFY((status>=0), "H5Tcommit");
    status = H5Tclose (tid);
    VRFY((status>=0), "H5Tclose");

    tid = H5Tcopy (H5T_UNIX_D32BE);
    VRFY((tid>=0), "H5Tcopy");
    status = H5Tcommit(file_id, "Committed D32BE type", tid);
    VRFY((status>=0), "H5Tcommit");
    status = H5Tclose (tid);
    VRFY((status>=0), "H5Tclose");

    tid = H5Tcopy (H5T_UNIX_D64LE);
    VRFY((tid>=0), "H5Tcopy");
    status = H5Tcommit(file_id, "Committed D64LE type", tid);
    VRFY((status>=0), "H5Tcommit");
    status = H5Tclose (tid);
    VRFY((status>=0), "H5Tclose");

    tid = H5Tcopy (H5T_UNIX_D64BE);
    VRFY((tid>=0), "H5Tcopy");
    status = H5Tcommit(file_id, "Committed D64BE type", tid);
    VRFY((status>=0), "H5Tcommit");
    status = H5Tclose (tid);
    VRFY((status>=0), "H5Tclose");

    /* Create a scalar dataspace */
    sid = H5Screate(H5S_SCALAR);
    VRFY((sid>=0), "H5Screate");

    /* Create a dataset with a time datatype */
    dsid = H5Dcreate(file_id, DATASET_PREFIX, H5T_UNIX_D32LE, sid, H5P_DEFAULT);
    VRFY((dsid>=0), "H5Dcreate");

    /* Initialize time data value */
    timenow = time(NULL);

    /* Write time to dataset */
    status = H5Dwrite (dsid, H5T_UNIX_D32LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &timenow);
    VRFY((status>=0), "H5Dwrite");

    /* Close objects */
    status = H5Dclose(dsid);
    VRFY((status>=0), "H5Dclose");

    status = H5Sclose(sid);
    VRFY((status>=0), "H5Sclose");
} /* gen_time() */

/*
 * This function creates 2 datasets using external files for raw data. The option
 * 'fill_dataset' determines whether data is to be written into the dataset.
 */
static void 
gen_external(hid_t file, int fill_dataset)
{
    hid_t	dcpl=-1;		/*dataset creation properties	*/
    hid_t	space=-1;		/*data space			*/
    hid_t	dset=-1;		/*dataset			*/
    hsize_t	cur_size[1];		/*data space current size	*/
    hsize_t	max_size[1];		/*data space maximum size	*/
    int		n;			/*number of external files	*/
    char	name[5][64];		/*external file name		*/
    char    prefix[64];
    off_t	file_offset;		/*external file offset		*/
    hsize_t	file_size;		/*sizeof external file segment	*/
    herr_t  ret;

    int     i, whole[100];

    /* Create dataset */
    dcpl=H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dcpl>=0), "H5Pcreate");

    /* generate names for raw data files */
    fill_dataset ? strcpy(prefix, "full") : strcpy(prefix,"empty");

    for (i=0; i<5; i++)
        sprintf(name[i], "%s_%d.data", prefix, i);

    /* define external raw data file with unlimited size */
    ret=H5Pset_external(dcpl, name[0], (off_t)0, H5F_UNLIMITED);
    VRFY((ret>=0), "H5Pset_external");

    cur_size[0] = 100;
    max_size[0] = H5S_UNLIMITED;

    space=H5Screate_simple(1, cur_size, max_size);
    VRFY((space>=0), "H5Screate_simple");

    /* create dataset */
    dset=H5Dcreate(file, "ext_dset1", H5T_NATIVE_INT, space, dcpl);
    VRFY((dset>=0), "H5Dcreate");

    /* Write the entire dataset */
    if (fill_dataset) {
        for (i=0; i<cur_size[0]; i++)
            whole[i] = i;

        ret=H5Dwrite(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,whole);
        VRFY((ret>=0), "H5Dwrite");
    }

	ret=H5Dclose(dset);
    VRFY((ret>=0), "H5Dclose");

    ret=H5Sclose(space);
    VRFY((ret>=0), "H5Sclose");

    ret=H5Pclose(dcpl);
    VRFY((ret>=0), "H5Pclose");

    dcpl=H5Pcreate(H5P_DATASET_CREATE);
    VRFY((dcpl>=0), "H5Pcreate");

    cur_size[0] = max_size[0] = 100;

    /* define multiple raw data files */
    if ((H5Pset_external(dcpl, name[1], (off_t)0,
	    (hsize_t)(max_size[0]*sizeof(int)/4))<0) ||
        (H5Pset_external(dcpl, name[2], (off_t)0,
	    (hsize_t)(max_size[0]*sizeof(int)/4))<0) ||
        (H5Pset_external(dcpl, name[3], (off_t)0,
	    (hsize_t)(max_size[0]*sizeof(int)/4))<0) ||
        (H5Pset_external(dcpl, name[4], (off_t)0,
	    (hsize_t)(max_size[0]*sizeof(int)/4))<0))
        VRFY(FALSE, "H5Pset_external");
    
    space=H5Screate_simple(1, cur_size, max_size);
    VRFY((space>=0), "H5Screate_simple");

    /* create dataset */
    dset=H5Dcreate(file, "ext_dset2", H5T_NATIVE_INT, space, dcpl);
    VRFY((dset>=0), "H5Dcreate");

    /* write the entire dataset */
    if (fill_dataset){
        ret=H5Dwrite(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,whole);
        VRFY((ret>=0), "H5Dwrite");
    }

    ret=H5Dclose(dset);
    VRFY((ret>=0), "H5Dclose");

    ret=H5Sclose(space);
    VRFY((ret>=0), "H5Sclose");

    ret=H5Pclose(dcpl);
    VRFY((ret>=0), "H5Pclose");

} /* gen_external() */


/*
 * This function creates a dataset using an array datatype. Each element of the
 * array is a compound datatype. The option 'fill_dataset' determines whether
 * data is to be written into the dataset.
 */
static void 
gen_array(hid_t fid1, int fill_dataset)
{
    typedef struct {        /* Typedef for compound datatype */
        int i;
        float f;
    } s1_t;

    s1_t *wdata;
    hid_t		dataset;	/* Dataset ID			*/
    hid_t		sid1;       /* Dataspace ID			*/
    hid_t		tid1;       /* Array Datatype ID			*/
    hid_t		tid2;       /* Compound Datatype ID			*/
    hsize_t		sdims1[RANK];
    hsize_t		tdims1[] = {ARRAY1_DIM1};
    int         nmemb;      /* Number of compound members */
    char       *mname;      /* Name of compound field */
    size_t      off;        /* Offset of compound field */
    hid_t       mtid;       /* Datatype ID for field */
    int        i,j;        /* counting variables */
    herr_t		ret;		/* Generic return value		*/

    VRFY((RANK>0), "RANK");
    VRFY((SIZE>0), "SIZE");

    /* define size for dataspace dimensions */
    for(i=0; i < RANK; i++)
        sdims1[i] = SIZE;

    /* Create dataspace for datasets */
    sid1 = H5Screate_simple(RANK, sdims1, NULL);
    VRFY((sid1>=0), "H5Screate_simple");

    /* Create a compound datatype to refer to */
    tid2 = H5Tcreate(H5T_COMPOUND, sizeof(s1_t));
    VRFY((tid2>=0), "H5Tcreate");

    /* Insert integer field */
    ret = H5Tinsert (tid2, "i", HOFFSET(s1_t,i), H5T_NATIVE_INT);
    VRFY((ret>=0), "H5Tinsert");

    /* Insert float field */
    ret = H5Tinsert (tid2, "f", HOFFSET(s1_t,f), H5T_NATIVE_FLOAT);
    VRFY((ret>=0), "H5Tinsert");

    /* Create an array datatype to refer to */
    tid1 = H5Tarray_create (tid2,ARRAY1_RANK,tdims1,NULL);
    VRFY((tid1>=0), "H5Tarray_create");

    /* Close compound datatype */
    ret=H5Tclose(tid2);
    VRFY((ret>=0), "H5Tclose");

    /* Create a dataset */
    dataset=H5Dcreate(fid1,"Dataset1",tid1,sid1,H5P_DEFAULT);
    VRFY((dataset>=0), "H5Dcreate");

    /* write dataset */
    if (fill_dataset) {

        wdata = (s1_t *)malloc(ARRAY1_DIM1*pow(SIZE,RANK)*sizeof(s1_t));
        VRFY((wdata!=NULL), "malloc");

        /* Initialize array data to write */
        for(i=0; i<pow(SIZE,RANK)*ARRAY1_DIM1; i++){
                wdata[i].i=i*10;
                wdata[i].f=(float)(i*2.5);
            } /* end for */

        /* Write dataset to disk */
        ret=H5Dwrite(dataset,tid1,H5S_ALL,H5S_ALL,H5P_DEFAULT,wdata);
        VRFY((ret>=0), "H5Dwrite");
    }

    /* Close Dataset */
    ret = H5Dclose(dataset);
    VRFY((ret>=0), "H5Dclose");

    /* Close datatype */
    ret = H5Tclose(tid1);
    VRFY((ret>=0), "H5Tclose");

    /* Close disk dataspace */
    ret = H5Sclose(sid1);
    VRFY((ret>=0), "H5Sclose");

    free(wdata);

} /* gen_array() */

#if H5_LIBVERSION == 18 /* library release >= 1.8 */

#define NEW_DATASET_NAME    "DATASET_NAME"
#define NEW_GROUP_NAME      "GROUP"
#define NEW_ATTR_NAME       "ATTR"
#define NEW_NUM_GRPS        35000
#define NEW_NUM_ATTRS       100

/*
 * This function creates 1.8 format file that consists of
 * fractal heap direct and indirect blocks.
 *
 */
static void 
gen_newgrat(hid_t file_id, unsigned num_grps, unsigned num_attrs)
{
    int         i;
    hid_t       gid;
    hid_t       type_id, space_id, attr_id, dset_id;
    char        gname[100];
    char        attrname[100];
    herr_t      ret;

    for (i=1; i<= num_grps; i++) {
        sprintf(gname, "%s%d", NEW_GROUP_NAME,i);
        gid = H5Gcreate2(file_id, gname, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        VRFY((gid>=0), "H5Gcreate2");
        ret = H5Gclose(gid);
        VRFY((ret>=0), "H5Gclose");
    }

    /* Create a datatype to commit and use */
    type_id=H5Tcopy(H5T_NATIVE_INT);
    VRFY((type_id>=0), "H5Tcopy");

    /* Create dataspace for dataset */
    space_id=H5Screate(H5S_SCALAR);
    VRFY((space_id>=0), "H5Screate");

    /* Create dataset */
    dset_id = H5Dcreate2(file_id, NEW_DATASET_NAME, type_id, space_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((dset_id>=0), "H5Dcreate2");

    for (i=1; i<= num_attrs; i++) {
        sprintf(attrname, "%s%d", NEW_ATTR_NAME,i);
        attr_id = H5Acreate2(dset_id, attrname, type_id, space_id, H5P_DEFAULT, H5P_DEFAULT);
        VRFY((attr_id>=0), "H5Acreate2");
        ret=H5Aclose(attr_id);
        VRFY((ret>=0), "H5Aclose");
    }

    ret = H5Dclose(dset_id);
    VRFY((ret>=0), "H5Dclose");

    ret = H5Sclose(space_id);
    VRFY((ret>=0), "H5Sclose");

    ret = H5Tclose(type_id);
    VRFY((ret>=0), "H5Tclose");
} /* gen_newgrat() */


/*
 * This function creates 1.8 format file that consists of
 * the shared message table.
 *
 */
static void 
gen_sohm(hid_t file_id)
{
    hid_t	type_id, space_id, group_id, attr_id;
    hsize_t     dims = 2;
    int         wdata[2] = {7, 42};
    herr_t      ret;


    type_id = H5Tcopy(H5T_NATIVE_INT);
    VRFY((type_id >= 0), "H5Tcopy");
    space_id = H5Screate_simple(1, &dims, &dims);
    VRFY((space_id >= 0), "H5Screate_simple");

    group_id = H5Gcreate2(file_id, NEW_GROUP_NAME, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((group_id >= 0), "H5Gcreate2");
    
    attr_id = H5Acreate2(group_id, NEW_ATTR_NAME, type_id, space_id, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((attr_id >= 0), "H5Acreate2");
    
    ret = H5Awrite(attr_id, H5T_NATIVE_INT, wdata);
    VRFY((ret >= 0), "H5Awrite");

    ret = H5Aclose(attr_id);
    VRFY((ret >= 0), "H5Aclose");

    ret = H5Tclose(type_id);
    VRFY((ret >= 0), "H5Tclose");

    ret = H5Gclose(group_id);
    VRFY((ret >= 0), "H5Gclose");
} /* gen_sohm() */

/* 
 * This set of routines for generating external linked files are mainly copied
 * from HDF5 library test/links.c
 */

/*
 * This routine creates files with "dangling" external links.
 */
static void
gen_ext_dangle(hid_t fid1, char *ext_fname1, hid_t fid2, char *ext_fname2)
{
    char fname2[50];	/* file name */
    herr_t ret;

    /* Create dangling external links */
    ret = H5Lcreate_external("missing", "/missing", fid1, "no_file", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    strcpy(fname2, ext_fname2);
    strcat(fname2, ".h5");

    H5Lcreate_external(fname2, "/missing", fid1, "no_object", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

} /* gen_ext_dangle() */

/* 
 * This routine creates files with external link to itself.
 */
static void
gen_ext_self(hid_t fid1, char *ext_fname1, hid_t fid2, char *ext_fname2, hid_t fid3, char *ext_fname3)
{
    hid_t lcpl_id;
    hid_t gid, gid2;	/* group ids */
    char fname1[50];
    char fname3[50];
    herr_t ret;

    /* Create an lcpl with intermediate group creation set */
    lcpl_id = H5Pcreate(H5P_LINK_CREATE);
    VRFY((lcpl_id >= 0), "H5Pcreate");

    ret = H5Pset_create_intermediate_group(lcpl_id, 1);
    VRFY((ret >= 0), "H5Pset_create_intermedidate_group");

    /* Create a series of groups within the file: /A/B and /X/Y/Z */
    gid = H5Gcreate2(fid1, "A/B", lcpl_id, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    gid = H5Gcreate2(fid1, "X/Y", lcpl_id, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    ret = H5Pclose(lcpl_id);
    VRFY((ret >= 0), "H5Pclose");

    strcpy(fname1, ext_fname1);
    strcat(fname1, ".h5");

    /* Create external link to own root group*/
    ret = H5Lcreate_external(fname1, "/X", fid1, "A/B/C", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    /* Open object through external link */
    gid = H5Gopen2(fid1, "A/B/C/", H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gopen2");

    /* Create object through external link */
    gid2 = H5Gcreate2(gid, "new_group", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid2 >= 0), "H5Gcreate2");

    /* Close created group */
    ret = H5Gclose(gid2);
    VRFY((ret >= 0), "H5Gclose");

    /* Close object opened through external link */
    H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");


    /* 
     * Use this file as an intermediate file in a chain
     * of external links that will go: file2 -> file1 -> file1 -> file3
     */

    /* Create in file2 with an external link to file1  */
    ret = H5Lcreate_external(fname1, "/A", fid2, "ext_link", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    /* Create file3 as a target */
    gid = H5Gcreate2(fid3, "end", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    strcpy(fname3, ext_fname3);
    strcat(fname3, ".h5");

    /* Create in file1 an extlink pointing to file3 */
    ret = H5Lcreate_external(fname3, "/", fid1, "/X/Y/Z", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

} /* gen_ext_self() */

/*
 * This routine creates files with external links to objects that crossed
 * several external file links.
 */
static void
gen_ext_mult(hid_t fid1, char *ext_fname1, hid_t fid2, char *ext_fname2, hid_t fid3, char *ext_fname3, hid_t fid4, char *ext_fname4)
{
    hid_t gid = (-1), gid2 = (-1);	/* Group IDs */
    char fname1[50];
    char fname2[50];
    char fname3[50];
    herr_t ret;

    /* Create first file to point to */
    /* fid=H5Fcreate("ext_mult1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, new_fapl); */

    /* Create object down a path */
    gid = H5Gcreate2(fid1, "A", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    gid = H5Gcreate2(fid1, "A/B", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    gid = H5Gcreate2(fid1, "A/B/C", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    /* Create second file to point to */
    /* fid=H5Fcreate("ext_mult2.h5", H5F_ACC_TRUNC, H5P_DEFAULT, new_fapl); */

    /* Create external link down a path */
    gid = H5Gcreate2(fid2, "D", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    gid = H5Gcreate2(fid2, "D/E", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    strcpy(fname1, "/");
    strcat(fname1, ext_fname1);
    strcat(fname1, ".h5");

    /* Create external link to object in first file */
    ret = H5Lcreate_external(fname1, "/A/B/C", gid, "F", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    /* Create third file to point to */
    /* fid=H5Fcreate("ext_mult3.h5", H5F_ACC_TRUNC, H5P_DEFAULT, new_fapl); */

    /* Create external link down a path */
    gid = H5Gcreate2(fid3, "G", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    gid = H5Gcreate2(fid3, "G/H", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    strcpy(fname2, ext_fname2);
    strcat(fname2, ".h5");

    /* Create external link to object in second file */
    ret = H5Lcreate_external(fname2, "/D/E/F", gid, "I", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    /* Create file with link to third file */
    /* fid=H5Fcreate("ext_mult4.h5", H5F_ACC_TRUNC, H5P_DEFAULT, new_fapl); */

    strcpy(fname3, ext_fname3);
    strcat(fname3, ".h5");
    H5Lcreate_external(fname3, "/G/H/I", fid4, "ext_link", H5P_DEFAULT, H5P_DEFAULT);

    gid = H5Gopen2(fid4, "ext_link", H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gopen2");

    /* Create object in external file */
    gid2 = H5Gcreate2(gid, "new_group", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid2 >= 0), "H5Gcreate2");

    /* Close group in external file */
    ret = H5Gclose(gid2);
    VRFY((ret >= 0), "H5Gclose");

    /* Close external object (lets first file close) */
    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

} /* gen_ext_mult() */

/* 
 * This routine creates files with external links to objects that goes back
 * and forth between two files a couple of times.
 */
static void
gen_ext_pingpong(hid_t fid1, char *ext_fname1, hid_t fid2, char *ext_fname2)
{
    hid_t gid = (-1);
    char fname1[50];
    char fname2[50];
    herr_t ret;

    /* Create first file */
    /* fid=H5Fcreate("ext_ping1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); */

    strcpy(fname2, ext_fname2);
    strcat(fname2, ".h5");

    /* Create external links for chain */
    ret = H5Lcreate_external(fname2, "/link2", fid1, "link1", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link4", fid1, "link3", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link6", fid1, "link5", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    /* Create final object */
    gid = H5Gcreate2(fid1, "final", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    /* Create second file */
    /* fid=H5Fcreate("ext_ping2.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); */

    strcpy(fname1, ext_fname1);
    strcat(fname1, ".h5");

    /* Create external links for chain */
    ret = H5Lcreate_external(fname1, "/link3", fid2, "link2", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link5", fid2, "link4", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/final", fid2, "link6", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

} /* gen_ext_pingpong() */

/*
 * This routine creates files with too many external links to objects.
 */
static void
gen_ext_toomany(hid_t fid1, char *ext_fname1, hid_t fid2, char *ext_fname2)
{
    hid_t gid = (-1);
    char fname1[50];
    char fname2[50];
    herr_t ret;

    /* fid=H5Fcreate("ext_many1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, new_fapl); */

    strcpy(fname2, ext_fname2);
    strcat(fname2, ".h5");

    /* Create external links for chain */
    ret = H5Lcreate_external(fname2, "/link2", fid1, "link1", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link4", fid1, "link3", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link6", fid1, "link5", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link8", fid1, "link7", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link10", fid1, "link9", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link12", fid1, "link11", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link14", fid1, "link13", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/link16", fid1, "link15", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname2, "/final", fid1, "link17", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    /* Create second file */
    /* fid=H5Fcreate("ext_many2.h5", H5F_ACC_TRUNC, H5P_DEFAULT, new_fapl); */

    strcpy(fname1, ext_fname1);
    strcat(fname1, ".h5");

    /* Create external links for chain */
    ret = H5Lcreate_external(fname1, "/link3", fid2, "link2", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link5", fid2, "link4", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link7", fid2, "link6", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link9", fid2, "link8", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link11", fid2, "link10", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link13", fid2, "link12", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link15", fid2, "link14", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    ret = H5Lcreate_external(fname1, "/link17", fid2, "link16", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    /* Create final object */
    gid = H5Gcreate2(fid2, "final", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

} /* end external_link_toomany() */

/*
 * This routine creates a file with various link types
 */
static void
gen_ext_links(hid_t fid, char *ext_fname)
{
    hid_t gid = -1, gid2 = -1;          /* Group IDs */
    hid_t sid = (-1);                   /* Dataspace ID */
    hid_t did = (-1);                   /* Dataset ID */
    hid_t tid = (-1);                   /* Datatype ID */
    herr_t ret;

    /* Create group */
    gid = H5Gcreate2(fid, "/Group1", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid >= 0), "H5Gcreate2");

    /* Create nested group */
    gid2 = H5Gcreate2(gid, "Group2", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((gid2 >= 0), "H5Gcreate2");

    /* Close groups */
    ret = H5Gclose(gid2);
    VRFY((ret >= 0), "H5Gclose");

    ret = H5Gclose(gid);
    VRFY((ret >= 0), "H5Gclose");

    /* Create soft links to groups created */
    ret = H5Lcreate_soft("/Group1", fid, "/soft_one", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_soft");

    ret = H5Lcreate_soft("/Group1/Group2", fid, "/soft_two", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_soft");

    /* Create dangling soft link */
    ret = H5Lcreate_soft("nowhere", fid, "/soft_dangle", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_soft");

    /* Create hard links to all groups */
    ret = H5Lcreate_hard(fid, "/", fid, "hard_zero", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_hard");

    ret = H5Lcreate_hard(fid, "/Group1", fid, "hard_one", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_hard");

    ret = H5Lcreate_hard(fid, "/Group1/Group2", fid, "hard_two", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_hard");

    /* Create loops w/hard links */
    ret = H5Lcreate_hard(fid, "/Group1", fid, "/Group1/hard_one", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_hard");

    ret = H5Lcreate_hard(fid, "/", fid, "/Group1/Group2/hard_zero", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_hard");

    /* Create dangling external link to non-existent file */
    ret = H5Lcreate_external("/foo.h5", "/group", fid, "/ext_dangle", H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Lcreate_external");

    /* Create dataset in each group */
    sid = H5Screate(H5S_SCALAR);
    VRFY((sid >= 0), "H5Screate");

    did = H5Dcreate2(fid, "/Dataset_zero", H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((did >= 0), "H5Dcreate2");

    ret = H5Dclose(did);
    VRFY((ret>=0), "H5Dclose");

    did = H5Dcreate2(fid, "/Group1/Dataset_one", H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((did >= 0), "H5Dcreate2");

    ret = H5Dclose(did);
    VRFY((ret>=0), "H5Dclose");

    did = H5Dcreate2(fid, "/Group1/Group2/Dataset_two", H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((did >= 0), "H5Dcreate2");

    ret = H5Dclose(did);
    VRFY((ret>=0), "H5Dclose");

    ret = H5Sclose(sid);
    VRFY((ret >= 0), "H5Sclose");

    /* Create named datatype in each group */
    tid = H5Tcopy(H5T_NATIVE_INT);
    VRFY((tid >= 0), "H5Tcopy");

    ret = H5Tcommit2(fid, "/Type_zero", tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Tcommit2");

    ret = H5Tclose(tid);
    VRFY((ret >= 0), "H5Tclose");

    tid = H5Tcopy(H5T_NATIVE_INT);
    VRFY((tid >= 0), "H5Tcopy");

    ret = H5Tcommit2(fid, "/Group1/Type_one", tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Tcommit2");

    ret = H5Tclose(tid);
    VRFY((ret >= 0), "H5Tclose");

    tid = H5Tcopy(H5T_NATIVE_INT);
    VRFY((tid >= 0), "H5Tcopy");

    ret = H5Tcommit2(fid, "/Group1/Group2/Type_two", tid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((ret >= 0), "H5Tcommit2");

    ret = H5Tclose(tid);
    VRFY((ret >= 0), "H5Tclose");

} /* gen_ext_links() */

#endif /* H5_LIBVERSION == 18 */

char *ext_fname[] = {
    "ext_dangle1",	/* 0 */
    "ext_dangle2",	/* 1 */
    "ext_self1",	/* 2 */
    "ext_self2",	/* 3 */
    "ext_self3",	/* 4 */
    "ext_mult1",	/* 5 */
    "ext_mult2",	/* 6 */
    "ext_mult3",	/* 7 */
    "ext_mult4",	/* 8 */
    "ext_pingpong1",	/* 9 */
    "ext_pingpong2",	/* 10 */
    "ext_toomany1",	/* 11 */
    "ext_toomany2",	/* 12 */
    "ext_links"		/* 13 */
};

/* main function of test generator */
int main(int argc, char *argv[])
{
    hid_t fid;	/* file id */
    hid_t ext_fid1, ext_fid2, ext_fid3, ext_fid4;	/* file ids  */
    FILE *out;
    char tmpname[100];
 
    char *driver = "sec2";
    char *superblock = "standard";
    unsigned i = 0;
    herr_t ret;
    int zero = 0;
    uint32_t chksum;

    /* file names */
    char *fname[] = {
	"root",
	"linear",
	"hierarchical",
	"multipath",
	"cyclical",
	"rank_dsets_empty",
	"rank_dsets_full",
	"group_dsets",
	"basic_types",
	"compound",
	"vl",
	"enum",
	"refer",
	"array",
	"filters",
	"stdio",
	"split",
	"multi",
	"family",
	"log",
	"attr",
	"time",
	"external_empty",
	"external_full",
	"alternate_sb", 
	"new_grat", 
	"sohm"
    }; 

    /* file names for external linked tests */
    char *ext_fname[] = {
	"ext_dangle1",		/* 0 */
	"ext_dangle2",		/* 1 */
	"ext_self1",		/* 2 */
	"ext_self2",		/* 3 */
	"ext_self3",		/* 4 */
	"ext_mult1",		/* 5 */
	"ext_mult2",		/* 6 */
	"ext_mult3",		/* 7 */
	"ext_mult4",		/* 8 */
	"ext_pingpong1",	/* 9 */
	"ext_pingpong2",	/* 10 */
	"ext_toomany1",		/* 11 */
	"ext_toomany2",		/* 12 */
	"ext_links"		/* 13 */
    };
    char *invalid_fname[] = {
	"invalid_grps",		/* 0 */
	"invalid_sym",		/* 1 */
    };

    /* initial message */
    printf("Generating test files for H5check...\n");
    fflush(stdout);
	
    /* root group is created along with the file */
    fid = create_file(fname[i++], driver, superblock);
    printf("just the root group\n");
    close_file(fid, "");

    /* create a linear group structure */
    fid = create_file(fname[i++], driver, superblock);
    printf("a linear group structure\n");
    gen_linear(fid);
    close_file(fid, "");

    /* create a treelike structure */
    fid = create_file(fname[i++], driver, superblock);
    printf("a treelike structure\n");
    gen_group_struct(fid, GROUP_PREFIX, HEIGHT, HIERARCHICAL);
    close_file(fid, "");

    /* create a multipath structure */
    fid = create_file(fname[i++], driver, superblock);
    printf("a multipath structure\n");
    gen_group_struct(fid, GROUP_PREFIX, HEIGHT, MULTIPATH);
    close_file(fid, "");
	
    /* create a cyclical structure */
    fid = create_file(fname[i++], driver, superblock);
    printf("a cyclical structure\n");
    gen_group_struct(fid, GROUP_PREFIX, HEIGHT, CYCLICAL);
    close_file(fid, "");

    /* create an empty dataset for each possible rank */
    fid = create_file(fname[i++], driver, superblock);
    printf("an empty dataset for each possible rank\n");
    gen_rank_datasets(fid, EMPTY);
    close_file(fid, "");

    /* create a full dataset for each possible rank */
    fid = create_file(fname[i++], driver, superblock);
    printf("a full dataset for each possible rank\n");
    gen_rank_datasets(fid, FULL);
    close_file(fid, "");

    /* create a tree like structure where some groups are empty
    while others contain a dataset */
    fid = create_file(fname[i++], driver, superblock);
    printf("a tree like structure where some groups are empty while others contain a dataset\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");

    /* creates a file with datasets using different basic datatypes */
    fid = create_file(fname[i++], driver, superblock);
    printf("datasets using different basic datatypes\n");
    gen_basic_types(fid, FULL);
    close_file(fid, "");

    /* create a file with a dataset using a compound datatype */
    fid = create_file(fname[i++], driver, superblock);
    printf("a dataset using a compound datatype\n");
    gen_compound(fid, FULL);
    close_file(fid, "");

    /* create a file with a dataset using a VL datatype */
    fid = create_file(fname[i++], driver, superblock);
    printf("a dataset using a VL datatype\n");
    gen_vl(fid, FULL);
    close_file(fid, "");

    /* create a file with a dataset using an enumerated datatype */
    fid = create_file(fname[i++], driver, superblock);
    printf("a dataset using an enumerated datatype\n");
    gen_enum(fid, FULL);
    close_file(fid, "");
	
    /* create a file with a dataset using reference datatype */
    fid = create_file(fname[i++], driver, superblock);
    printf("a dataset using reference datatype\n");
    gen_reference(fid);
    close_file(fid, "");

    /* create a file with an array datatype */
    fid = create_file(fname[i++], driver, superblock);
    printf("an array datatype\n");
    gen_array(fid, FULL);
    close_file(fid, "");

    /* create a file with several datasets using different filters */
    fid = create_file(fname[i++], driver, superblock);
    printf("several datasets using different filters\n");
    gen_filters(fid);
    close_file(fid, "");

    /* create a file using stdio file driver */
    fid = create_file(fname[i++], "stdio", superblock);
    printf("using stdio file driver\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");

    /* create a file using split file driver */
    fid = create_file(fname[i++], "split", superblock);
    printf("using split file driver\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");

    /* create a file using multi file driver */
    /* this will have "NCSAmulti" in the superblock */
    fid = create_file(fname[i++], "multi", superblock);
    printf("using multi file driver\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");

    /* create a file using family file driver */
    fid = create_file(fname[i++], "family", superblock);
    printf("using family file driver\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");

    /* create a file using log file driver */
    fid = create_file(fname[i++], "log", superblock);
    printf("using log file driver\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");

    /* create a file with several datasets using attributes */
    fid = create_file(fname[i++], driver, superblock);
    printf("several datasets using attributes\n");
    gen_attr(fid);
    close_file(fid, "");

    /* create a file using time datatype */
    fid = create_file(fname[i++], driver, superblock);
    printf("using time datatype\n");
    gen_time(fid);
    close_file(fid, "");

    /* create an external file without data (no raw data files)*/
    fid = create_file(fname[i++], driver, superblock);
    printf("an external file without data (no raw data files)\n");
    gen_external(fid, EMPTY);
    close_file(fid, "");

    /* create an external file with data */
    fid = create_file(fname[i++], driver, superblock);
    printf("an external file with data\n");
    gen_external(fid, FULL);
    close_file(fid, "");

    /* create a file with a non-standard superblock */
    fid = create_file(fname[i++], driver, "alternate");
    printf("non-standard superblock\n");
    gen_group_datasets(fid, GROUP_PREFIX, HEIGHT, RIGHT);
    close_file(fid, "");
   
#if H5_LIBVERSION == 18 /* for library release > 1.8 */

    fid = create_file(fname[i++], driver, "new");
    printf("1.8 group/attribute file\n");
    gen_newgrat(fid, NEW_NUM_GRPS, NEW_NUM_ATTRS);
    close_file(fid, "");

    /* use the parameter "superblock" to set SOHM property list */
    fid = create_file(fname[i++], driver, "sohm");
    printf("1.8 SOHM file\n");
    gen_sohm(fid);
    close_file(fid, "");

#if H5_VERS_RELEASE > 1
    /*
     * Generate testfiles for validating external links
     */
    /* create files with dangling external links */
    ext_fid1 = create_file(ext_fname[0], driver, superblock);
    ext_fid2 = create_file(ext_fname[1], driver, "new");
    printf("Dangling external links\n");
    gen_ext_dangle(ext_fid1, ext_fname[0], ext_fid2, ext_fname[1]);
    close_file(ext_fid1, "");
    close_file(ext_fid2, "");
    
    /* create files with external links to self */
    ext_fid1 = create_file(ext_fname[2], driver, superblock);
    ext_fid2 = create_file(ext_fname[3], driver, "new");
    ext_fid3 = create_file(ext_fname[4], driver, superblock);
    printf("External link to self\n");
    gen_ext_self(ext_fid1, ext_fname[2], ext_fid2, ext_fname[3], ext_fid3, ext_fname[4]);
    close_file(ext_fid1, "");
    close_file(ext_fid2, "");
    close_file(ext_fid3, "");

    /* create files with external links across multiple files */
    ext_fid1 = create_file(ext_fname[5], driver, superblock);
    ext_fid2 = create_file(ext_fname[6], driver, "new");
    ext_fid3 = create_file(ext_fname[7], driver, superblock);
    ext_fid4 = create_file(ext_fname[8], driver, "new");
    printf("External links across multiple files\n");
    gen_ext_mult(ext_fid1, ext_fname[5], ext_fid2, ext_fname[6], ext_fid3, ext_fname[7], ext_fid4, ext_fname[8]);
    close_file(ext_fid1, "");
    close_file(ext_fid2, "");
    close_file(ext_fid3, "");
    close_file(ext_fid4, "");

    /* create files with external links that go back and forth between 2 files */
    ext_fid1 = create_file(ext_fname[9], driver, superblock);
    ext_fid2 = create_file(ext_fname[10], driver, "new");
    printf("External links that go back and forth between 2 files\n");
    gen_ext_pingpong(ext_fid1, ext_fname[9], ext_fid2, ext_fname[10]);
    close_file(ext_fid1, "");
    close_file(ext_fid2, "");

    /* create files with too many external links to objects */
    ext_fid1 = create_file(ext_fname[11], driver, "new");
    ext_fid2 = create_file(ext_fname[12], driver, superblock);
    printf("Files with too many external links to objects\n");
    gen_ext_toomany(ext_fid1, ext_fname[11], ext_fid2, ext_fname[12]);
    close_file(ext_fid1, "");
    close_file(ext_fid2, "");

    /* create a file with various links */
    ext_fid1 = create_file(ext_fname[13], driver, superblock);
    printf("File with various links\n");
    gen_ext_links(ext_fid1, ext_fname[13]);
    close_file(ext_fid1, "");
#endif
    /* 
     * Generate an invalid file with illegal version # for a link message in fractal heap.
     * See information in invalidfiles/README.
     * The numbers below are specific to generate the invalid condition.
     */
    /* create a file similar to "newgrat.h5" */
    fid = create_file(invalid_fname[0], driver, "new");
    printf("File with invalid version number in link message.....\n");
    gen_newgrat(fid, 50, 30);
    close_file(fid, "");

    memset(tmpname, 0, sizeof(tmpname));
    strcat(tmpname, invalid_fname[0]);
    strcat(tmpname, ".h5");

    /* Make it invalid by writing to a specific address in the heap */
    out = fopen(tmpname, "r+");
    VRFY((out != NULL), "fopen");

    /* Specific checksum to be set in the direct block */
    chksum = 374212020;

    /* Seek to the link message in fractal heap direct block */
    ret = fseek(out, 14118, SEEK_SET);
    VRFY((ret >= 0), "fseek");

    /* Change the verison number to 0 */
    ret = fwrite(&zero, (size_t)1, 1, out);
    VRFY((ret == 1), "fwrite");

    /* Seek to the location of the checksum in the fractal heap direct block */
    ret = fseek(out, 13844, SEEK_SET);
    VRFY((ret >= 0), "fseek");

    /* Write the checksum */
    ret = fwrite(&chksum, (size_t)4, 1, out);
    VRFY((ret == 1), "fwrite");

    /* Close the file */
    ret = fclose(out);
    VRFY((ret == 0), "fclose");

#endif

    /* 
     * Generate an invalid file with duplicate and out of order symbols.
     * See information in invalidfiles/README.
     * The numbers below are specific to generate the invalid condition.
     */
    /* create a file which is the same as "rank_dsets_empty.h5" */
    fid = create_file(invalid_fname[1], driver, superblock);
    printf("File with invalid symbol table entries...\n");
    gen_rank_datasets(fid, EMPTY);
    close_file(fid, "");

    memset(tmpname, 0, sizeof(tmpname));
    strcat(tmpname, invalid_fname[1]);
    strcat(tmpname, ".h5");

    /* Open the file */
    out = fopen(tmpname, "r+");
    VRFY((out != NULL), "fopen");

    /* Seek to the location of the symbol name in the heap */
    ret = fseek(out, 9552, SEEK_SET);
    VRFY((ret >= 0), "fseek");

    /* Make the symbol name the same as the next entry */
    ret = fwrite("4", (size_t)1, 1, out);
    VRFY((ret == 1), "fwrite");

    /* Close the file */
    ret = fclose(out);
    VRFY((ret == 0), "fclose");

    /* successful completion message */
    printf("\rTest files generation for H5check successful!\n");

    return 0;

} /* end main() */
