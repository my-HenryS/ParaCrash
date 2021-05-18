#!/usr/bin/env python3

import argparse
import sys
import os
import subprocess
import configparser
from pfs_utils import pfs_restore, pfs_save, pfs_start, pfs_stop, save_workload, Pwrite
import pfs_config
from pfs_csg import CrashStateGenerator
from pfs import POSIX_Workload, MPI_Workload
from config import config
import time

# record i/o operations and take snapeshot of all PFS services


def pfs_record():
    if config.verbose:
        print('Initializing PFS state')

    # read pfs config file
    pfs = pfs_config.run_config()

    # run init script
    assert (pfs.mount_point != "/")
    subprocess.call("rm -rf %s/*" % pfs.mount_point, shell=True)
    subprocess.call(config.init, cwd=pfs.mount_point, shell=True)
    subprocess.call("sleep 0.1", shell=True)

    # create workload, build fd table and entryinfo table

    if not config.parallel:
        workload = POSIX_Workload(pfs=pfs, path=pfs.mount_point, name="workload")
    else:
        workload = MPI_Workload(pfs=pfs, path=pfs.mount_point, num_ranks=config.parallel)
    workload.build_fsztable()

    #for service in pfs.services:
    #   subprocess.call("sudo chown -R $USER %s" % service.path, shell=True)
    #    subprocess.call("sudo chmod -R 777 %s" % service.path, shell=True)

    pfs.build_fsztable()
    pfs.entryinfo()

    # take snapshots
    if config.legal_replay:
        save_workload(pfs, workload, config.prefix_snapshot_dir)
    pfs_save(pfs)

    if config.verbose:
        print('Running workload with strace')

    filtered_syscall = [
        "epoll_wait", "futex", "restart_syscall", "epoll_ctl", "epoll_create", "getxattr", "access", "statfs", "read", "newfstatat", "poll",
        "gettid", "getpriority", "rt_sigaction", "setpriority", "setsockopt", "socketpair", "getsockopt", "mprotect", "clone", "mmap",
        "munmap", "prctl", "set_robust_list", "accept", "socket", "fcntl", "connect", "geteuid", "utimensat", "pread64", "fstat",
        "getdents", "clock_gettime", "madvise", "exit", "stat", "nanosleep", "flock", "lstat", "rt_sigaction", "sigaltstack", "brk",
        "uname", "getpid", "ioctl", "execve", "arch_prctl", "exit_group", "set_tid_address", "prlimit64", "getrandom", "readlink",
        "sysinfo", "dup", "getgid", "getppid", "getegid", "getuid", "dup2", "pipe2", "wait4", "getdents64", "sched_getaffinity", "bind",
        "listen", "getsockname", "rt_sigprocmask", "getcwd", "rt_sigreturn", "umask", "lgetxattr", "fgetxattr", "chown"]

    filter_string = "-e trace=file,desc -e \!%s" % (",".join(filtered_syscall))

    # start tracing
    pfs_filtered_syscall = filtered_syscall  # + ["write"]
    pfs_filter_string = "-e trace=file,desc -e \!%s" % (",".join(pfs_filtered_syscall))

    start_time = time.time()

    if config.tracing:
        # for service in pfs.services:
        #     print(service.pid)
        for service in pfs.services:
            subprocess.Popen(("sudo strace -q -s 400000 -x -ff -ttt -o %s -p %d %s" %
                          (config.trace_dir+service.name, service.pid, pfs_filter_string)), shell=True)

        # service_string = " -p ".join([str(service.pid) for service in pfs.services])
        # print("sudo strace -q -s 400000 -x -ff -ttt -o %s -p %s %s" % (config.trace_dir+"output.log", service_string, pfs_filter_string))
        # subprocess.Popen(("sudo strace -q -s 400000 -x -ff -ttt -o %s -p %s %s" %
        #                     (config.trace_dir+"output.log", service_string, pfs_filter_string)), shell=True)
        #subprocess.call("sleep 0.5", shell=True)

    # run workload
    if config.parallel:
        if config.tracing:
            subprocess.call(
                "TRACE_DIR=%s RECORDER_COMPRESSION_MODE=0 LD_PRELOAD=%s mpirun -np %d %s" %
                (config.recorder_trace_dir, config.recorder_path, config.parallel, config.workload),
                cwd=pfs.mount_point, shell=True)
        else:
            subprocess.call("mpirun -np %d %s" % (config.parallel, config.workload), cwd=pfs.mount_point, shell=True)
    else:
        # if config.legal_replay:
        if config.tracing:
            subprocess.call(("strace -q -s 1000000 -x -ff -ttt -o %s %s %s" %
                          (config.trace_dir+"workload", filter_string, config.workload)), cwd=pfs.mount_point, shell=True)
        else:
            subprocess.call(config.workload, cwd=pfs.mount_point, shell=True)
    end_time = time.time()
    # sleep longer for larger files
    if config.is_inc:
        subprocess.call("sleep 3", shell=True)
    else:
        subprocess.call("sleep 1", shell=True)
    subprocess.call("sudo killall strace", shell=True)

    print("time recorded: %.3f" % (end_time - start_time))

    if not config.tracing:
        print("PFSCheck stoped: tracing not enabled")
        exit(0)
    #subprocess.call("%s -l a.log %s" % (config.h5scan_path, "/orangefs/create.h5"), shell=True)
    #subprocess.call("h5check %s" % ("/orangefs/create.h5"), shell=True)
    #subprocess.call("h5check %s" % ("/orangefs/create.h5"), shell=True)

    #exit(0)
    for service in pfs.services:
        subprocess.call("sudo chown -R $USER %s" % service.path, shell=True)
        subprocess.call("sudo chmod -R 777 %s" % service.path, shell=True)

    # build fd table and update entryinfo
    pfs.build_fdtable()
    pfs.entryinfo()

    if config.verbose:
        print('Cleaning trace files')

    # filter real working processes
    filter_string = "open|link|sendto|recvfrom|pwrite64|writev|readv"
    useful_files_string = subprocess.check_output("grep -Elr \"%s\" %s/*" % (filter_string, config.trace_dir), shell=True)
    # abs path
    useful_files = useful_files_string.decode().strip().split("\n")

    if config.verbose:
        print("Total %s useful files" % len(useful_files))

    # remove all empty files
    # NOTE: simplify logic
    tmp_dir = config.trace_dir+"tmp/"
    subprocess.call("mkdir %s" % (tmp_dir), shell=True)
    for useful_file in useful_files:
        subprocess.call("cp %s %s" % (useful_file, tmp_dir), shell=True)
    subprocess.call("find %s -maxdepth 1 -type f | xargs rm" % (config.trace_dir), shell=True)
    subprocess.call("mv %s/* %s" % (tmp_dir, config.trace_dir), shell=True)
    subprocess.call(("rm -rf %s" % (tmp_dir)).split(' '))

    if config.verbose:
        print('Formatting system calls')

    # formalize all syscall traces
    pfs.create_syscalls(useful_files)

    # pfs_stop(pfs)
    #pfs_restore(pfs, snapshot_dir)
    # pfs.replay()
    # pfs_restore(pfs, snapshot_dir)
    # pfs_start(pfs)

    # dump trace into human readable
    pfs.dump(config.result_dir)
    # input("")

    if not config.parallel:
        workload.create_syscalls(useful_files)
        workload.dump(config.result_dir)
        pfs.mapping_with(workload, hdf5=config.is_hdf5)
        if config.legal_replay:
            workload.legal_replay(pfs)

    else:
        useful_files = subprocess.check_output("find %s -name *.itf" % (config.recorder_trace_dir), shell=True).decode().strip().split("\n")
        workload.create_syscalls(useful_files)
        workload.dump(config.result_dir)
        pfs.mapping_with(workload, hdf5=config.is_hdf5)
        if config.legal_replay:
            workload.legal_replay(pfs)

    return pfs
