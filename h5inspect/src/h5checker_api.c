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
#include "h5_pline.h"

ck_err_t
h5checker_obj(char *fname, ck_addr_t obj_addr, int format_num, ck_errmsg_t *errbuf)
{
    driver_t *thefile = NULL;
    global_shared_t *shared;
    table_t *obj_table;
    ck_addr_t ss;

    g_obj_api = TRUE;
    g_obj_api_err = 0;
    g_format_num = format_num;

    if(g_format_num != FORMAT_ONE_SIX && g_format_num != FORMAT_ONE_EIGHT) {
        printf("Invalid library version provided.  Default library version is assumed.\n");
        g_format_num = DEFAULT_FORMAT;
    }

    g_obj_addr = obj_addr;

    if(!addr_defined(g_obj_addr))
	printf("VALIDATING %s ", fname);
    else
	printf("VALIDATING %s at object header address %llu ", fname, g_obj_addr);

    if (g_format_num == FORMAT_ONE_SIX)
	printf("according to library release version 1.6.6...\n");
    else if (g_format_num == FORMAT_ONE_EIGHT)
	printf("according to library release version 1.8.0...\n");
    else
	printf("...invalid library release version...shouldn't happen.\n");
	

/* NEED: to work on this */
    if((shared = calloc(1, sizeof(global_shared_t))) == NULL) {
	error_push(ERR_FILE, ERR_NONE_SEC,
	    "Failure in memory allocation. Validation discontinued.", -1, NULL);
	++g_obj_api_err;
	goto done;
    }

    if(table_init(&obj_table, TYPE_HARD_LINK) < 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Errors in initializing table for hard links", -1, NULL);
	++g_obj_api_err;
    }

    if(shared && obj_table)
        shared->obj_table = obj_table;

    if((thefile = (driver_t *)FD_open(fname, shared, SEC2_DRIVER)) == NULL) {
	error_push(ERR_FILE, ERR_NONE_SEC,
	    "Failure in opening input file using the default driver. Validation discontinued.", -1, NULL);
	++g_obj_api_err;
	goto done;
   }

   /* superblock validation has to be all passed before proceeding further */
   if(check_superblock(thefile) < 0) {
      error_push(ERR_LEV_0, ERR_NONE_SEC,
	    "Errors found when checking superblock. Validation stopped.", -1, NULL);
      ++g_obj_api_err;
      goto done;
    }

    /* not using the default driver */
    if(thefile->shared->driverid != SEC2_DRIVER) {
	if(FD_close(thefile) != SUCCEED) {
	    error_push(ERR_FILE, ERR_NONE_SEC, "Errors in closing input file using the default driver", -1, NULL);
    	    ++g_obj_api_err;
       }

       printf("Switching to new file driver...\n");
       if((thefile = (driver_t *)FD_open(fname, shared, shared->driverid)) == NULL) {
	    error_push(ERR_FILE, ERR_NONE_SEC,
		"Errors in opening input file. Validation stopped.", -1, NULL);
	    ++g_obj_api_err;
	    goto done;
	}
    }

    ss = FD_get_eof(thefile);
    if(!addr_defined(ss) || ss < shared->stored_eoa) {
	error_push(ERR_FILE, ERR_NONE_SEC,
	    "Invalid file size or file size less than superblock eoa. Validation stopped.", 
	    -1, NULL);
	++g_obj_api_err;
	goto done;
    }

    if(addr_defined(g_obj_addr) && g_obj_addr >= shared->stored_eoa) {
	error_push(ERR_FILE, ERR_NONE_SEC,
	    "Invalid Object header address provided. Validation stopped.", -1, NULL);
	++g_obj_api_err;
	goto done;
    }

    if(pline_init_interface() < 0) {
        error_push(ERR_LEV_0, ERR_NONE_SEC, "Problems in initializing filters...later validation may be affected",
            -1, NULL);
	++g_obj_api_err;
    }   

    /* check the whole file or the given g_obj_addr */
    if(!addr_defined(g_obj_addr)) {
	if(check_obj_header(thefile, shared->root_grp->header, NULL) < 0)
	    ++g_obj_api_err;
    } else if(check_obj_header(thefile, g_obj_addr, NULL) < 0)
	++g_obj_api_err;

done:
    if(thefile && thefile->shared)
        (void) table_free(thefile->shared->obj_table);

    (void) pline_free();

    if(thefile && thefile->shared) {
        SM_master_table_t *tbl = thefile->shared->sohm_tbl;

        if(thefile->shared->root_grp)
            free(thefile->shared->root_grp);

        if(thefile->shared->sohm_tbl) {
            SM_master_table_t *tbl = thefile->shared->sohm_tbl;

            if(tbl->indexes) free(tbl->indexes);
            free(tbl);
        }
        if(thefile->shared->fa)
            free_driver_fa(thefile->shared);
        free(thefile->shared);
    }

    if(thefile && FD_close(thefile) < 0) {
	error_push(ERR_FILE, ERR_NONE_SEC, "Errors in closing the input file", -1, NULL);
	++g_obj_api_err;
    }

    if(errbuf != NULL && g_obj_api_err)
	process_errors(errbuf);

    return(g_obj_api_err? -1: 0);
}
