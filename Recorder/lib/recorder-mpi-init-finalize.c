/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted for any purpose (including commercial purposes)
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    and/or materials provided with the distribution.
 *
 * 3. In addition, redistributions of modified forms of the source or binary
 *    code must carry prominent notices stating that the original code was
 *    changed and the date of the change.
 *
 * 4. All publications or advertising materials mentioning features or use of
 *    this software are asked, but not required, to acknowledge that it was
 *    developed by The HDF Group and by the National Center for Supercomputing
 *    Applications at the University of Illinois at Urbana-Champaign and
 *    credit the contributors.
 *
 * 5. Neither the name of The HDF Group, the name of the University, nor the
 *    name of any Contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission from
 *    The HDF Group, the University, or the Contributor, respectively.
 *
 * DISCLAIMER:
 * THIS SOFTWARE IS PROVIDED BY THE HDF GROUP AND THE CONTRIBUTORS
 * "AS IS" WITH NO WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED. In no
 * event shall The HDF Group or the Contributors be liable for any damages
 * suffered by the users arising out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * Portions of Recorder were developed with support from the Lawrence Berkeley
 * National Laboratory (LBNL) and the United States Department of Energy under
 * Prime Contract No. DE-AC02-05CH11231.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE /* for RTLD_NEXT */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>

#include "mpi.h"
#include "recorder.h"


static double local_tstart, local_tend;

void recorder_init(int *argc, char ***argv) {
    MAP_OR_FAIL(PMPI_Comm_size)
    MAP_OR_FAIL(PMPI_Comm_rank)
    MAP_OR_FAIL(PMPI_Wtime)
    MAP_OR_FAIL(PMPI_Reduce)
    RECORDER_REAL_CALL(PMPI_Comm_rank)(MPI_COMM_WORLD, &_recorder_rank);
    RECORDER_REAL_CALL(PMPI_Comm_size)(MPI_COMM_WORLD, &_recorder_nprocs);

    logger_init(_recorder_rank, _recorder_nprocs);
    local_tstart = RECORDER_REAL_CALL(PMPI_Wtime)();
    logger_start_recording();
}

void recorder_exit() {
    logger_exit();

    local_tend = RECORDER_REAL_CALL(PMPI_Wtime)();
    //double min_tstart, max_tend;
    //RECORDER_REAL_CALL(PMPI_Reduce)(&local_tstart, &min_tstart, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    //RECORDER_REAL_CALL(PMPI_Reduce)(&local_tend , &max_tend, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (_recorder_rank == 0)
        printf("[Recorder] elapsed time on rank 0: %.2f\n", local_tend-local_tstart);
}

int PMPI_Init(int *argc, char ***argv) {
    MAP_OR_FAIL(PMPI_Init)
    int ret = RECORDER_REAL_CALL(PMPI_Init) (argc, argv);
    recorder_init(argc, argv);
    return ret;
}

int MPI_Init(int *argc, char ***argv) {
    MAP_OR_FAIL(PMPI_Init)
    int ret = RECORDER_REAL_CALL(PMPI_Init) (argc, argv);
    recorder_init(argc, argv);
    return ret;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided) {
    MAP_OR_FAIL(PMPI_Init_thread)
    int ret = RECORDER_REAL_CALL(PMPI_Init_thread) (argc, argv, required, provided);
    recorder_init(argc, argv);
    return ret;
}
/*
int MPI_Comm_rank(MPI_Comm comm, int *rank){
    int ret = RECORDER_REAL_CALL(MPI_Comm_rank) (comm, rank);
    logger_start_recording();
    return ret;
}

int PMPI_Comm_rank(MPI_Comm comm, int *rank){
    int ret = RECORDER_REAL_CALL(MPI_Comm_rank) (comm, rank);
    logger_start_recording();
    return ret;
}*/

int PMPI_Finalize(void) {
    recorder_exit();
    MAP_OR_FAIL(PMPI_Finalize);
    return RECORDER_REAL_CALL(PMPI_Finalize) ();
}

int MPI_Finalize(void) {
    recorder_exit();
    MAP_OR_FAIL(PMPI_Finalize);
    int res = RECORDER_REAL_CALL(PMPI_Finalize) ();
    return res;
}

