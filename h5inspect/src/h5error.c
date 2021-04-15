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

#include "h5_check.h"
#include "h5_error.h"

static	int	nerrors=0;	/* number of errors found */
static  const char *get_prim_err(primary_err_t);
static  const char *get_sec_err(secondary_err_t);

/* error handling */
static  const   primary_err_mesg_t primary_err_mesg_g[] = {
        {ERR_NONE_PRIM, "none"},
        {ERR_LEV_0,     "Disk Format Level 0-File Metadata"},
        {ERR_LEV_1,     "Disk Format Level 1-File Infrastructure"},
        {ERR_LEV_2,     "Disk Format Level 2-Data Objects"},
        {ERR_FILE,  	"File Handling"},
        {ERR_INTERNAL, 	"Internal Error"},
};

static  const   secondary_err_mesg_t secondary_err_mesg_g[] = {
        {ERR_NONE_SEC, "none"},
        {ERR_LEV_0A,   "0A-File Signature and Super Block"},
        {ERR_LEV_0B,   "0B-File Driver Info"},
        {ERR_LEV_0C,   "0C-Superblock Extension"},
        {ERR_LEV_1A1,   "1A1-Version 1 B-Trees (B-link trees)"},
        {ERR_LEV_1A2,   "1A2-Version 2 B-Trees"},
        {ERR_LEV_1B,    "1B-Group Symbol Table"},
        {ERR_LEV_1C,    "1C-Group Symbol Table Entry"},
        {ERR_LEV_1D,    "1D-Local Heaps"},
        {ERR_LEV_1E,    "1E-Global Heap"},
        {ERR_LEV_1F,    "1F-Fractal Heap"},
        {ERR_LEV_1G,    "1G-Free-space Manager"},
        {ERR_LEV_1H,    "1H-Shared Object Header Message Table"},
        {ERR_LEV_2A,    "2A-Data Object Headers"},
        {ERR_LEV_2A1a,  "2A1a-Version 1 Data Object Header Prefix"},
        {ERR_LEV_2A1b,  "2A1b-Version 2 Data Object Header Prefix"},
        {ERR_LEV_2A2,   "2A2-Shared Message"},
        {ERR_LEV_2A2a,  "2A2a-Header Message: NIL"},
        {ERR_LEV_2A2b,  "2A2b-Header Message: Dataspace"},
        {ERR_LEV_2A2c,  "2A2c-Header Message: Link Info"},
        {ERR_LEV_2A2d,  "2A2d-Header Message: Datatype"},
        {ERR_LEV_2A2e,  "2A2e-Header Message: Data Storage-Fill Value(Old)"},
        {ERR_LEV_2A2f,  "2A2f-Header Message: Data Storage-Fill Value"},
        {ERR_LEV_2A2g,  "2A2g-Header Message: Link Message"},
        {ERR_LEV_2A2h,  "2A2h-Header Message: Data Storage-External Data Files"},
        {ERR_LEV_2A2i,  "2A2i-Header Message: Data Storage-Layout"},
        {ERR_LEV_2A2j,  "2A2j-Header Message: Reserved-not assigned yet"},
        {ERR_LEV_2A2k,  "2A2k-Header Message: Group Info"},
        {ERR_LEV_2A2l,  "2A2l-Header Message: Data Storage-Filter Pipeline"},
        {ERR_LEV_2A2m,  "2A2m-Header Message: Attribute"},
        {ERR_LEV_2A2n,  "2A2n-Header Message: Object Comment"},
        {ERR_LEV_2A2o,  "2A2o-Header Message: Object Modification Time(Old)"},
        {ERR_LEV_2A2p,  "2A2p-Header Message: Shared Message Table"},
        {ERR_LEV_2A2q,  "2A2q-Header Message: Object Header Continuation"},
        {ERR_LEV_2A2r,  "2A2r-Header Message: Symbol Table"},
        {ERR_LEV_2A2s,  "2A2s-Header Message: Object Modification Time"},
        {ERR_LEV_2A2t,  "2A2t-Header Message: B-tree 'K' Values"},
        {ERR_LEV_2A2u,  "2A2u-Header Message: Driver Info"},
        {ERR_LEV_2A2v,  "2A2v-Header Message: Attribute Info"},
        {ERR_LEV_2A2w,  "2A2w-Header Message: Reference Count"},
        {ERR_LEV_2B,    "2B-Data Object Data Storage"},
};


void
error_push(primary_err_t prim_err, secondary_err_t sec_err, const char *desc, ck_addr_t logical_addr, int *badinfo)
{
	
    ERR_t *estack = ERR_get_my_stack();
    char *new_func;
    char *new_desc;
    int	desc_len, func_len;

    /*
     * Don't fail if arguments are bad.  Instead, substitute some default
     * value.
     */
    if(!desc) 
	desc = "No description given";

    /*
     * Push the error if there's room.  Otherwise just forget it.
     */
    assert(estack);
    if(estack->nused < H5E_NSLOTS) {
        estack->slot[estack->nused].prim_err = prim_err;
        estack->slot[estack->nused].sec_err = sec_err;
        estack->slot[estack->nused].desc = desc;
        estack->slot[estack->nused].logical_addr = logical_addr;
	if(badinfo != NULL) {
	    estack->slot[estack->nused].err_info.reported = REPORTED;
	    estack->slot[estack->nused].err_info.badinfo = *badinfo;
	} else {
	    estack->slot[estack->nused].err_info.reported = NOT_REP;
	    estack->slot[estack->nused].err_info.badinfo = -1;
	}
        estack->nused++;
    }
}



ck_err_t
error_clear(void)
{
    int	i;
    ERR_t *estack = ERR_get_my_stack();

    for(i = estack->nused-1; i >=0; --i) {
/* NEED CHECK: INTO THIS LATER */
#if 0
	free((void *)estack->slot[i].func_name);
	free((void *)estack->slot[i].desc);
#endif
    }
    if(estack) 
	estack->nused = 0;
    return(0);
}

void
error_print(FILE *stream, driver_t *file)
{
    int	i;
    const char	*prim_str = NULL;
    const char	*sec_str = NULL;
    ERR_t   *estack = ERR_get_my_stack();
    char		*fname;

    if(!stream) 
	stream = stderr; 
    nerrors++;
    if(g_verbose_num) {
	if(estack->nused > 0)
	    fprintf(stream, "***Error***\n");
	for(i = estack->nused-1; i >=0; --i) {
	    int sec_null = 0;
	    fprintf(stream, "%s", estack->slot[i].desc);
	    if((int)estack->slot[i].logical_addr != -1) {
#ifdef TEMP
		fname = FD_get_fname(file, estack->slot[i].logical_addr);
		fprintf(stream, "\n  file=%s;", fname);
#endif
		fprintf(stream, " at addr %llu",
		    (unsigned long long)estack->slot[i].logical_addr);
		if(estack->slot[i].err_info.reported)
		    fprintf(stream, "; Value decoded: %d",
			estack->slot[i].err_info.badinfo);
	    }
	    fprintf(stream, "\n");

#ifdef TEMP
	    prim_str = get_prim_err(estack->slot[i].prim_err);
	    sec_str = get_sec_err(estack->slot[i].sec_err);
	    if(estack->slot[i].sec_err == ERR_NONE_SEC)
		sec_null = 1;
	    if(sec_null)
		fprintf(stream, "  %s\n", prim_str);
	    else
		fprintf(stream, "  %s-->%s\n", prim_str, sec_str);
#endif
	}
	if(estack->nused > 0)
	    fprintf(stream, "***End of Error messages***\n");
    }
}

const char *
get_prim_err(primary_err_t n)
{
    unsigned    i;
    const char *ret_value="Invalid primary error number";

    for(i=0; i < NELMTS(primary_err_mesg_g); i++)
	if(primary_err_mesg_g[i].err_code == n)
	    return(primary_err_mesg_g[i].str);

    return(ret_value);
}

const char *
get_sec_err(secondary_err_t n)
{
    unsigned    i;
    const char *ret_value="Invalid secondary error number";

    for(i=0; i < NELMTS(secondary_err_mesg_g); i++)
	if(secondary_err_mesg_g[i].err_code == n)
	    return(secondary_err_mesg_g[i].str);

    return(ret_value);
}

int
found_error(void)
{
    return(nerrors!=0);
}

void
process_errors(ck_errmsg_t *errbuf)
{
    int i;
    ERR_t *estack = ERR_get_my_stack();

    errbuf->nused = estack->nused;
    for(i = estack->nused-1; i >=0; --i) {
	errbuf->slot[i].desc = strdup(estack->slot[i].desc);
	errbuf->slot[i].addr = estack->slot[i].logical_addr;
    }
}
