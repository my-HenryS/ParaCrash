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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include "h5_check.h"
#include "h5_error.h"
#include "h5_pline.h"
#include "h5_logger.h"

/* 
 * The following coding for handling option is mainly copied from the HDF tools library 
 */
/*
 * Command-line options: The user can specify short or long-named
 * parameters. The long-named ones can be partially spelled. When
 * adding more, make sure that they don't clash with each other.
 *	option followed by ":" indicates that the option requires argument
 */
extern int opt_ind;
extern const char *opt_arg;
extern logger_ctx_t logger;

static const char *s_opts = "o:v:f:l:hVe";
static struct long_options l_opts[] = {
    {"help", no_arg, 'h'},
    {"hel", no_arg, 'h'},
    {"he", no_arg, 'h'},
    {"version", no_arg, 'V'},
    {"versio", no_arg, 'V'},
    {"versi", no_arg, 'V'},
    {"vers", no_arg, 'V'},
    {"external", no_arg, 'e'},
    {"externa", no_arg, 'e'},
    {"extern", no_arg, 'e'},
    {"exter", no_arg, 'e'},
    {"exte", no_arg, 'e'},
    {"ext", no_arg, 'e'},
    {"ex", no_arg, 'e'},
    {"logging", no_arg, 'l'},
    {"object", require_arg, 'o'},
    {"objec", require_arg, 'o'},
    {"obje", require_arg, 'o'},
    {"obj", require_arg, 'o'},
    {"ob", require_arg, 'o'},
    {"verbose", require_arg, 'v'},
    {"verbos", require_arg, 'v'},
    {"verbo", require_arg, 'v'},
    {"verb", require_arg, 'v'},
    {"format", require_arg, 'f'},
    {"forma", require_arg, 'f'},
    {"form", require_arg, 'f'},
    {"for", require_arg, 'f'},
    {"fo", require_arg, 'f'},
    {NULL, 0, '\0'}};

int main(int argc, const char **argv)
{
    driver_t *thefile = NULL;
    struct stat statbuf;
    stat_info_t stat_info;
    int opt;
    char *prog_name;
    char *fname;
    char *endptr;
    ck_err_t ret_err = 0;

    if ((prog_name = (char *)strrchr(argv[0], '/')))
        prog_name++;
    else
        prog_name = (char *)argv[0];

    g_verbose_num = DEFAULT_VERBOSE;
    g_format_num = DEFAULT_FORMAT;
    g_obj_addr = CK_ADDR_UNDEF;
    g_follow_ext = FALSE;
    is_logging = FALSE;
    char log_file[100];

    /* no arguments */
    if (argc == 1)
    {
        usage(prog_name);
        leave(EXIT_COMMAND_FAILURE);
    }

    /* parse command line options */
    while ((opt = get_option(argc, argv, s_opts, l_opts)) != EOF)
    {
        switch ((char)opt)
        {
        case 'h':
            usage(prog_name);
            leave(EXIT_COMMAND_SUCCESS);
            break;

        case 'V':
            print_version(prog_name);
            leave(EXIT_COMMAND_SUCCESS);
            break;

        case 'e':
            g_follow_ext = TRUE;
            break;

        case 'o':
            errno = 0;
            g_obj_addr = strtoull(opt_arg, &endptr, 0);
            if ((errno != 0 && g_obj_addr == 0) || (*endptr != '\0'))
            {
                printf("Invalid object address\n");
                usage(prog_name);
                leave(EXIT_COMMAND_FAILURE);
            }
            if (!addr_defined(g_obj_addr))
            {
                printf("Object header address is undefined\n");
                usage(prog_name);
                leave(EXIT_COMMAND_FAILURE);
            }
            printf("CHECK OBJECT_HEADER is true:object address =%llu\n", g_obj_addr);
            break;

        case 'v':
            errno = 0;
            g_verbose_num = strtol(opt_arg, &endptr, 0);
            if ((errno == ERANGE && (g_verbose_num == LONG_MAX || g_verbose_num == LONG_MIN)) ||
                (errno != 0 && g_verbose_num == 0) || (*endptr != '\0'))
            {
                printf("Invalid verbose value\n");
                usage(prog_name);
                leave(EXIT_COMMAND_FAILURE);
            }
            if (g_verbose_num < 0 || g_verbose_num > 2)
            {
                printf("Incorrect verbose value\n");
                usage(prog_name);
                leave(EXIT_COMMAND_FAILURE);
            }
            printf("VERBOSE is true:verbose # = %d\n", g_verbose_num);
            break;

        case 'f':
            errno = 0;
            g_format_num = strtol(opt_arg, &endptr, 0);
            if ((errno == ERANGE && (g_format_num == LONG_MAX || g_format_num == LONG_MIN)) ||
                (errno != 0 && g_format_num == 0) || (*endptr != '\0'))
            {
                printf("Invalid format value\n");
                usage(prog_name);
                leave(EXIT_COMMAND_FAILURE);
            }
            if (g_format_num != FORMAT_ONE_SIX && g_format_num != FORMAT_ONE_EIGHT)
            {
                printf("Incorrect library release version.\n");
                usage(prog_name);
                leave(EXIT_COMMAND_FAILURE);
            }
            printf("FORMAT is true:format version = %d\n", g_format_num);
            break;

        case 'l':
            is_logging = TRUE;
            strcpy(log_file, opt_arg);
            break;

        case '?':
        default:
            usage(prog_name);
            leave(EXIT_COMMAND_FAILURE);
        }
    }

    /* check for file name to be processed */
    if (argc <= opt_ind)
    {
        printf("Missing file name\n");
        usage(prog_name);
        leave(EXIT_COMMAND_FAILURE);
    }

    fname = strdup(argv[opt_ind]);
    g_obj_api = FALSE;

    if (debug_verbose())
    {
        if (g_format_num == FORMAT_ONE_SIX)
            printf("\nVALIDATING %s according to library version 1.6.6 ", fname);
        else if (g_format_num == FORMAT_ONE_EIGHT)
            printf("\nVALIDATING %s according to library version 1.8.0 ", fname);
        else
            printf("...invalid library release version...shouldn't happen.\n");

        if (addr_defined(g_obj_addr)) /* NO NEED TO CHECK FOR THIS LATER */
            printf("at object header address %llu", g_obj_addr);
        printf("\n\n");
        fflush(stdout);
    }

    // init logger
    if (is_logging)
    {
        //char log_file[100];
        //sprintf(log_file, "%s.log", fname);
        logger.file = fopen(log_file, "w");
    }

    // NOTE: file_init checks superblock
    if ((thefile = file_init(fname)) == NULL)
        CK_INC_ERR_DONE

    if (addr_defined(g_obj_addr) && g_obj_addr >= thefile->shared->stored_eoa)
    {
        error_push(ERR_FILE, ERR_NONE_SEC,
                   "Invalid Object header address provided. Validation stopped.",
                   -1, NULL);
        CK_INC_ERR_DONE
    }

    /* Initialize global filters */
    if (pline_init_interface() < 0)
    {
        error_push(ERR_LEV_0, ERR_NONE_SEC, "Problems in initializing filters",
                   -1, NULL);
        CK_INC_ERR_DONE
    }

    if (stat(fname, &statbuf) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1C, "Error in getting stat info", -1, NULL);
        CK_INC_ERR_DONE
    }
    stat_info.st_dev = statbuf.st_dev;
    stat_info.st_ino = statbuf.st_ino;
    stat_info.st_mode = statbuf.st_mode;

    /* Initialize global table of external linked files being visited */
    g_ext_tbl = NULL;
    if (g_follow_ext)
    {
        if (table_init(&g_ext_tbl, TYPE_EXT_FILE) < 0)
        {
            error_push(ERR_INTERNAL, ERR_NONE_SEC, "Errors in initializing table for external linked files", -1, NULL);
            CK_INC_ERR_DONE
        }
        assert(g_ext_tbl);
        if (table_insert(g_ext_tbl, &stat_info, TYPE_EXT_FILE) < 0)
        {
            error_push(ERR_INTERNAL, ERR_NONE_SEC, "Errors in inserting external linked file to table", -1, NULL);
            CK_INC_ERR_DONE
        }
    }

    /* writing to logger */
    logger_obj_t *root_grp = logger_new_obj("");

    logger.root_grp = root_grp;
    logger_set_current_obj(logger.root_grp);

    /* errors should have been flushed already in check_obj_header() */
    if (addr_defined(g_obj_addr))
        check_obj_header(thefile, g_obj_addr, NULL);
    else
        check_obj_header(thefile, thefile->shared->root_grp->header, NULL);

done:
    if (fname)
        free(fname);

    if (!found_error())
    {
        if (is_logging)
        {
            logger_dump();
        }
    }

    (void)pline_free();
    if (g_ext_tbl)
        (void)table_free(g_ext_tbl);

    if (thefile != NULL)
        free_file_shared(thefile);

    if (thefile != NULL && FD_close(thefile) < 0)
    {
        error_push(ERR_FILE, ERR_NONE_SEC, "Errors in closing input file", -1, NULL);
        ++ret_err;
    }

    if (ret_err)
    {
        error_print(stderr, thefile);
        error_clear();
    }

    if (found_error())
    {
        printf("h5inspect: Non-compliance errors found\n");
        leave(EXIT_FORMAT_FAILURE);
    }
    else
    {
        printf("h5inspect: No non-compliance errors found\n");
        leave(EXIT_COMMAND_SUCCESS);
    }
} /* main */
