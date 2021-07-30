import re
import io
import os
import codecs
import subprocess
import time
from config import config

from re import compile as recompile

# operations that update the existing disk state
list_of_ops = ['link', 'unlink', 'rename', 'setxattr', 'pwrite64', 'creat', 'fsync', 'mkdir', 'sendto', 'recvfrom']
files_to_ignore = ['/dev/null']
time_epsilon = 1e-6
O_CREAT = 64
MPI_MODE_CREATE = 1

class Syscall(object):
    non_supported_calls = []

    def __init__(self, service, timestamp, func_name, args, retval, errmsg):
        self.service = service
        self.timestamp = timestamp
        self.func_name = func_name
        self.__orig_args = args
        self.retval = retval
        self.errmsg = errmsg
        self.gid = -1
        # self.formalize = False     # fixed: formalized once a syscall is initiated

    def perform(self, with_error=False):
        # assert(self.formalize)
        if self.errmsg != None or int(self.retval) < 0:
            return self.real_perform_with_err(self.retval)
        else:
            return self.real_perform()

    def real_perform(self):
        pass

    def real_perform_with_err(self, errno):
        try:
            retval = self.real_perform()
            #print("Retval for %s: %s" % (self.func_name, retval))
            return retval
        except Exception as e:
            #print(e)
            pass

    def __str__(self):
        return self.raw_trace()

    def __repr__(self):
        return self.short()  # self.short()

    def short(self):
        return "%s:%d" % (self.func_name, self.gid)

    def raw_trace(self):
        return '%.6f %s(%s) = %s' % (self.timestamp, self.func_name, ", ".join(str(arg) for arg in self.__orig_args), str(self.retval))

    # 1576297195.408111 openat(AT_FDCWD, "dentries/38/51/root/#fSiDs#/1-5DF462DB-EA60", O_WRONLY|O_CREAT|O_EXCL, 0644) = 20
    @staticmethod
    def parse_strace(line):
        START_RE = '(\d+\.\d+)[ ]+'    # match timestamp
        CALL_RE = '(\w+)\((.*)\)'    # match syscall name & parameters
        END_RE = '[ ]+= (?:(\d+)|(-\d+) (.*))'  # match return value

        RE = recompile(START_RE + CALL_RE + END_RE)

        match = RE.match(line)
        if not match:
            print(line[:100], "Line matching failed:")
            exit(0)
        groups = match.groups()

        return groups

    # 1594588407.464766 1594588407.464938 0 read 16 0x1a8ee70 80
    @staticmethod
    def parse_recorder(line):
        groups = line.strip().split(" ")
        return groups

    @staticmethod
    def add_syscall(line, process, mode="strace", is_workload=False, mpi_only=False):
        new_syscalls = None
        path = process.path
        get_desc = process.get_desc
        set_desc = process.set_desc
        remove_desc = process.remove_desc
        get_socket = process.get_socket
        get_offset = process.get_offset
        set_offset = process.set_offset
        get_size = process.get_size
        set_size = process.set_size

        # parse trace
        if ("---" in line and "SIG" in line) or ("+++" in line and "exited with" in line) or "<detached ...>" in line:
            return

        if mode == "strace":
            groups = Syscall.parse_strace(line)
            time_start = float(groups[0])
            func_name = groups[1]
            args = groups[2].replace(" ", "").replace("\"", "").split(",")
            if groups[3] is not None:
                retval = int(groups[3])
            else:
                retval = int(groups[4])
            errmsg = groups[5]

        elif mode == "recorder":
            groups = Syscall.parse_recorder(line)
            time_start = float(groups[0])
            time_end = float(groups[1])
            retval = int(groups[2])
            func_name = groups[3]
            args = groups[4:]
            errmsg = ""

        if not mpi_only:
            # formalize arguments
            if func_name in ['unlinkat', 'unlink']:
                if func_name == 'unlinkat':
                    path = get_abs_path(args[0], args[1], path, get_desc)
                else:
                    path = os.path.join(path, args[0])
                new_syscalls = [Unlink(process, time_start, func_name, args, retval, errmsg, path)]

            elif func_name == 'mkdir':
                path = os.path.join(path, args[0])
                mode = args[1]
                new_syscalls = [Mkdir(process, time_start, func_name, args, retval, errmsg, path, mode)]

            elif func_name == 'mkdirat':
                path = os.path.join(get_desc(args[0]), args[1])
                mode = args[2]
                new_syscalls = [Mkdir(process, time_start, func_name, args, retval, errmsg, path, mode)]

            elif func_name in ['setxattr', 'fsetxattr', 'lsetxattr']:
                if func_name in ['setxattr', 'lsetxattr']:
                    path = get_abs_path("AT_FDCWD", args[0], path, get_desc)

                elif func_name == 'fsetxattr':
                    path = get_desc(args[0])

                hex_value = parse_string(args[2])
                new_syscalls = [Setxattr(process, time_start, func_name, args, retval, errmsg, path, args[1], hex_value)]

            elif func_name in ['lremovexattr']:
                path = get_abs_path("AT_FDCWD", args[0], path, get_desc)
                new_syscalls = [Removexattr(process, time_start, func_name, args, retval, errmsg, path, args[1])]

            elif func_name in ['rename', 'link']:
                srcname = os.path.join(path, args[0])
                destname = os.path.join(path, args[1])
                if func_name == 'rename':
                    new_syscalls = [Rename(process, time_start, func_name, args, retval, errmsg, srcname, destname)]
                else:
                    new_syscalls = [Link(process, time_start, func_name, args, retval, errmsg, srcname, destname)]

            elif func_name in ['linkat']:
                srcname = get_abs_path(args[0], args[1], path, get_desc)
                destname = get_abs_path(args[2], args[3], path, get_desc)
                new_syscalls = [Link(process, time_start, func_name, args, retval, errmsg, srcname, destname)]

            elif func_name in ['openat']:
                filename = get_abs_path(args[0], args[1], path, get_desc)
                set_desc(retval, filename)
                # only cares about create
                if ('O_CREAT' in args[2] or (args[2].isdigit() and int(args[2]) & O_CREAT != 0)) and filename not in files_to_ignore:
                    func_name = 'creat'
                    new_syscalls = [Creat(process, time_start, func_name, args, retval, errmsg, filename)]

            elif func_name in ["open64", "open"]:
                filename = args[0]
                set_desc(retval, filename)
                # only cares about create
                if ('O_CREAT' in args[1] or (args[1].isdigit() and int(args[1]) & O_CREAT != 0)) and filename not in files_to_ignore:
                    func_name = 'creat'
                    new_syscalls = [Creat(process, time_start, func_name, args, retval, errmsg, filename)]

            elif func_name in ["lseek", "lseek64"]:
                fd = int(args[0])
                offset = int(args[1])

                if args[2] == "SEEK_SET" or (args[2].isdigit() and int(args[2]) == 0):
                    mode = "SEEK_SET"
                elif args[2] == "SEEK_CUR" or (args[2].isdigit() and int(args[2]) == 1):
                    mode = "SEEK_CUR"
                elif args[2] == "SEEK_END" or (args[2].isdigit() and int(args[2]) == 2):
                    mode = "SEEK_END"

                set_offset(fd, offset, mode)

            # remove fdtable entry when close; nothing to create
            elif func_name in ['close']:
                remove_desc(args[0])

            # ftruncate
            elif func_name == "ftruncate":
                path = get_desc(args[0])
                length = int(args[1])
                new_syscalls = [Truncate(process, time_start, func_name, args, retval, errmsg, path, length)]

            # replace fd with fname
            elif func_name in ["pwrite64", "pwrite", "write"]:
                path = get_desc(args[0])

                if path == None:
                    return

                # content = content.replace('\\n','\n')
                # content = content.encode('ascii')
                content = parse_string(args[1])
                length = int(args[2])
                if func_name in ["pwrite64", "pwrite"]:
                    offset = int(args[3])
                elif func_name == "write":
                    offset = get_offset(args[0])

                # write() will adjust offset
                # NOTE had a bug here once, take care
                if func_name == "write":
                    set_offset(args[0], offset+length)

                cur_size = get_size(path)
                if offset + length > cur_size:
                    func_name = "append"
                    set_size(path, offset + length)

                new_syscalls = [Pwrite(process, time_start, func_name, args, retval, errmsg, path, content, length, offset)]


            elif func_name in ['fsync', 'fdatasync']:
                path = get_desc(args[0])
                new_syscalls = [Fsync(process, time_start, func_name, args, retval, errmsg, path)]

            elif not is_workload and func_name in ["sendto", "recvfrom"]:

                dest = process.get_socket(args[0])
                content = parse_string(args[1][:1000])

                #print(args ,args[-2])
                if "MSG_PEEK" in args[-3]:
                    return None

                if func_name == "sendto":
                    new_syscalls = [Sendto(process, time_start, func_name, args, retval, errmsg, dest, content)]
                else:
                    new_syscalls = [Recvfrom(process, time_start, func_name, args, retval, errmsg, dest, content)]

            elif not is_workload and func_name == "writev":
                dest = process.get_socket(args[0])
                content = "Unknown"
                new_syscalls = [Sendto(process, time_start, "sendto", args, retval, errmsg, dest, content)]

            elif not is_workload and func_name == "readv":
                dest = process.get_socket(args[0])
                content = "Unknown"
                new_syscalls = [Recvfrom(process, time_start, "recvfrom", args, retval, errmsg, dest, content)]

        # mpi_only
        else:
            if func_name == "PMPI_File_open":
                path = args[1]
                fd = int(args[-1], 16)
                set_desc(fd, path)
                if int(args[2]) & MPI_MODE_CREATE != 0:
                    func_name = 'creat'
                    new_syscalls = [Creat(process, time_start, func_name, args, retval, errmsg, path)]

            elif func_name == "PMPI_File_write_at_all":
                new_syscalls = [Barrier(process, time_start, func_name, args, retval, errmsg)]
                path = get_desc(int(args[0], 16))
                offset = int(args[1])
                content = parse_string(args[2])
                length = int(args[3])
                assert (length == 1 and "PMPI_File_write_at_all write count is not 1")
                length *= int(args[4])
                new_syscalls += [Pwrite(process, time_start+time_epsilon, func_name, args, length, errmsg, path, content, length, offset)]

            elif func_name == "PMPI_File_write_at":
                path = get_desc(int(args[0], 16))
                offset = int(args[1])
                content = parse_string(args[2])
                length = int(args[3])
                assert (args[4] == "MPI_BYTE" and "PMPI_File_write_at data type is not byte")
                new_syscalls = [Pwrite(process, time_start, func_name, args, length, errmsg, path, content, length, offset)]

            elif func_name in ["PMPI_File_sync"]:
                path = get_desc(int(args[0], 16))
                new_syscalls = [Fsync(process, time_start, func_name, args, retval, errmsg, path)]


        if func_name in ["PMPI_Barrier", "PMPI_Bcast", "PMPI_File_close", "PMPI_File_set_view"]:
            new_syscalls = [Barrier(process, time_start, func_name, args, retval, errmsg)]

        if new_syscalls == None or len(new_syscalls) == 0:
            if func_name not in Syscall.non_supported_calls:
                Syscall.non_supported_calls.append(func_name)
                # if config.verbose:
                #     print("%s not generating useful syscall" % func_name)

        if not new_syscalls:
            return

        for syscall in new_syscalls:
            process.syscalls.append(syscall)

        return new_syscalls


class Creat(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path

    def real_perform(self):
        # change a to w will cause bugs
        f = open(self.path, 'a').close()
        os.chmod(self.path, 0o777)
        return 0

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.path]), str(self.retval))


class Link(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, srcname, destname):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.srcname = srcname
        self.destname = destname

    def real_perform(self):
        retval = os.link(self.srcname, self.destname)
        return retval

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.srcname, self.destname]), str(self.retval))


class Unlink(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path

    def real_perform(self):
        if os.path.islink(self.path):
            retval = os.unlink(self.path)
        elif os.path.isdir(self.path):
            retval = os.rmdir(self.path)
        else:
            retval = os.unlink(self.path)
        return retval

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.path]), str(self.retval))


class Rename(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, srcname, destname):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.srcname = srcname
        self.destname = destname

    def real_perform(self):
        retval = os.rename(self.srcname, self.destname)
        return retval

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.srcname, self.destname]), str(self.retval))


class Truncate(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path, length):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path
        self.length = int(length)

    def real_perform(self):
        retval = os.truncate(self.path, self.length)
        return retval

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.path, str(self.length)]), str(self.retval))


class Pwrite(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path, content, length, offset):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path
        self.content = content
        self.length = int(length)
        self.offset = int(offset)
        self.append = False

    def real_perform(self):
        if not os.path.exists(self.path):
            f = open(self.path, 'a').close()
            os.chmod(self.path, 0o777)
        f = os.open(self.path, os.O_RDWR)
        bytesWritten = os.pwrite(f, self.content, int(self.offset))
        os.close(f)

        return bytesWritten

    def __str__(self):
        return '%d %s(%s) = %s' \
            % (self.gid, self.func_name, ", ".join([self.path, repr(self.content[:4]), str(self.offset)]), str(self.retval))


class Setxattr(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path, key, value):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path
        self.key = key
        self.value = value

    def real_perform(self):
        retval = os.setxattr(self.path, self.key, self.value)

    def __str__(self):
        return '%d %s(%s) = %s' \
            % (self.gid, self.func_name, ", ".join([self.path, self.key, repr(self.value)]), str(self.retval))


class Removexattr(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path, key):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path
        self.key = key

    def real_perform(self):
        os.removexattr(self.path, self.key)

    def __str__(self):
        return '%d %s(%s) = %s' \
            % (self.gid, self.func_name, ", ".join([self.path, self.key]), str(self.retval))


class Mkdir(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path, mode):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path
        self.mode = int(mode)

    def real_perform(self):
        os.makedirs(self.path, self.mode, exist_ok=True)

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.path, str(self.mode)]), str(self.retval))


class Fsync(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, path):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.path = path
        self.is_datasync = False

    def real_perform(self):
        # f = os.open(self.path, os.O_RDWR)
        # if self.is_datasync:
        #     os.fdatasync(f)
        # else:
        #     os.fsync(f)
        # os.close(f)
        pass

    def __str__(self):
        return '%d %s(%s) = %s' % (self.gid, self.func_name, ", ".join([self.path]), str(self.retval))


class Sendto(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, dest, content):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.dest = dest
        self.content = content
        self.correlated_call = None


    def real_perform(self):
        pass

    def __str__(self):
        recv_gid = -1
        if self.correlated_call:
            recv_gid = self.correlated_call.gid
        return '%d %s(%s) = %s' \
            % (self.gid, self.func_name, ", ".join([self.dest, str(recv_gid)]), str(self.retval))


class Recvfrom(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg, dest, content):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.dest = dest
        self.content = content
        self.correlated_call = None

    def real_perform(self):
        pass

    def __str__(self):
        send_gid = -1
        if self.correlated_call:
            send_gid = self.correlated_call.gid
        return '%d %s(%s) = %s' \
            % (self.gid, self.func_name, ", ".join([self.dest, str(send_gid)]), str(self.retval))


class Barrier(Syscall):
    def __init__(self, service, timestamp, func_name, args, retval, errmsg):
        super().__init__(service, timestamp, func_name, args, retval, errmsg)
        self.barrier_id = -1

    def real_perform(self):
        pass

    def __str__(self):
        return '%d %s(%d) = %s' \
            % (self.gid, self.func_name, self.barrier_id, str(self.retval))

# FIXME: sometimes fail with large number of servers

def format_sendrecv(pfs):
    # merge tcp header with content
    for service in pfs.services:
        header_recvs = []
        to_remove = []
        for syscall in filter(lambda s: isinstance(s, Recvfrom), service.syscalls):
            if (type(syscall.content) is not str) and pfs.HEADER_IDENT in syscall.content \
                    and len(syscall.content) == pfs.HEADER_SIZE:
                header_recvs.append(syscall)
            else:
                check_header = [s for s in header_recvs if s.dest == syscall.dest]
                if len(check_header) != 0:
                    matched_call = check_header[0]

                    # FIXME Support reading readv() writev() content in the future
                    if syscall.content != "Unknown":
                        matched_call.content += syscall.content
                    matched_call.retval += syscall.retval
                    header_recvs.remove(matched_call)
                    to_remove.append(syscall)

        service.syscalls = list(filter(lambda s: s not in to_remove, service.syscalls))

    # match send to recv, exchange dest.gid for each send-recv pair
    recv_calls = dict()
    send_calls = []
    for service in pfs.services:
        #print(service.name)
        recv_calls[service.name] = dict()
        for call in filter(lambda x: isinstance(x, Sendto), service.syscalls):
            send_calls.append(call)

        for call in filter(lambda x: isinstance(x, Recvfrom), service.syscalls):
            dest = call.dest
            if call.dest == pfs.client_name:
                continue
            if dest not in recv_calls[service.name]:
                recv_calls[service.name][dest] = [call]
            else:
                recv_calls[service.name][dest].append(call)

    for call in send_calls:
        if call.dest == pfs.client_name:
            continue
        #print(call.dest, call.service.name)
        if call.dest in recv_calls and call.service.name in recv_calls[call.dest]:
            if len(recv_calls[call.dest][call.service.name]) > 0:
                recv_call = recv_calls[call.dest][call.service.name][0]
                call.correlated_call = recv_call
                recv_call.correlated_call = call
                recv_calls[call.dest][call.service.name].pop(0)
        else:
            print("recv call not found")


def get_abs_path(fd, path, service_path, get_desc):
    if fd == "AT_FDCWD":
        if os.path.isabs(path):
            filename = path
        else:
            filename = os.path.join(service_path, path)
    else:
        filename = os.path.join(get_desc(fd), path)

    return filename


def parse_string(raw_string):
    def is_strace_hex(s): return "\\x" in s
    def is_recorder_hex(s): return "0x" == s[0:2]
    # consider hex string
    if is_strace_hex(raw_string):
        hex_value = b''
        assert(len(raw_string) % 4 == 0 or print(raw_string))
        for i in range(0, len(raw_string), 4):
            hex_value += codecs.decode(raw_string[i+2:i+4], 'hex')
        return hex_value
    elif is_recorder_hex(raw_string):
        hex_value = b''
        assert(len(raw_string) % 2 == 0 or print(raw_string))
        for i in range(2, len(raw_string), 2):
            hex_value += codecs.decode(raw_string[i:i+2], 'hex')
        return hex_value
    # if ascii string, we only decode escape chars
    else:
        return codecs.decode(raw_string, 'unicode_escape').encode("utf-8")


"""
In record phase: pfs_save
In check phase: pfs_stop -> pfs_restore -> crash emulation -> pfs_start
"""
# start pfs service


def pfs_start(pfs, whitelist=[], blocklist=None):
    if not config.restart:
        return

    assert not blocklist or len(whitelist) == 0
    if blocklist != None:
        to_restart = filter(lambda s: s.name in blocklist, pfs.services)
    else:
        to_restart = filter(lambda s: s.name not in whitelist, pfs.services) 

    if pfs.fstype == "orangefs":
        for service in pfs.services:
            #print(service.name)
            subprocess.call("pvfs2-server /home/jhsun/local/opt/orangefs/etc/orangefs-server_32.conf -a %s" %
                            service.tag, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.call("sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH /home/jhsun/local/sbin/pvfs2-client", shell=True)
        # FIXME generalize the mount command
        time.sleep(0.5)
        subprocess.call("sudo mount -t pvfs2 tcp://vm2:3334/orangefs %s" % pfs.mount_point, shell=True)

    elif pfs.fstype == "beegfs":    
        for service in to_restart:
            #subprocess.call("sudo chown -R $USER %s" % service.path, shell=True)
            #subprocess.call("sudo chmod -R 777 %s" % service.path, shell=True)
            subprocess.call("sudo service %s restart" % service.name, shell=True)
        # avoid PFS unavailable
        time.sleep(0.5)

    elif pfs.fstype == "glusterfs":
        subprocess.call("sudo gluster volume start myvol3 --mode=script",
                        shell=True,  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # FIXME generalize volume string in the mount command
        subprocess.call("sudo mount -t glusterfs hpc1:/myvol3 %s" % pfs.mount_point, shell=True)
        time.sleep(0.1)

# stop pfs service


def pfs_stop(pfs, whitelist=[], blocklist=None):
    if not config.restart:
        return

    assert not blocklist or len(whitelist) == 0
    if blocklist != None:
        to_restart = filter(lambda s: s.name in blocklist, pfs.services)
    else:
        to_restart = filter(lambda s: s.name not in whitelist, pfs.services)

    if pfs.fstype == "orangefs":
        #subprocess.call("sudo fuser -k %s" % pfs.mount_point, shell=True)
        subprocess.call("sudo umount %s" % pfs.mount_point, shell=True)
        subprocess.call("kill -9 $(pidof pvfs2-server)", shell=True)
        subprocess.call("sudo killall pvfs2-client-core", shell=True)

    elif pfs.fstype == "beegfs":
        for service in filter(lambda s: s.name not in whitelist, pfs.services):
            #subprocess.call("sudo service %s stop" % service.name, shell=True)
            pass

    elif pfs.fstype == "glusterfs":
        #subprocess.call("sudo kill -9 $(pidof glusterfs)", shell=True)
        subprocess.call("sudo umount %s" % pfs.mount_point, shell=True)
        subprocess.call("sudo gluster volume stop myvol3 --mode=script",
                        shell=True,  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


# clean & restart pfs service
def pfs_restart_clean(pfs, clean=True):
    if pfs.fstype == "orangefs":
        subprocess.call("sudo fuser -k %s" % pfs.mount_point, shell=True)
        subprocess.call("sudo kill -9 $(pidof pvfs2-server)", shell=True)
        subprocess.call("sudo killall pvfs2-client-core", shell=True)
        time.sleep(0.5)
        for service in pfs.services:
            if clean:
                subprocess.call("rm -rf %s/*" % service.path, shell=True)
                subprocess.call("pvfs2-server /home/jhsun/local/opt/orangefs/etc/orangefs-server_16.conf  -f -a %s" %
                                service.tag, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.call("pvfs2-server /home/jhsun/local/opt/orangefs/etc/orangefs-server_16.conf  -a %s" %
                            service.tag, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.call("sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH /home/jhsun/local/sbin/pvfs2-client", shell=True)
        time.sleep(2)

# save pfs snapshot


def pfs_save(pfs):
    for service in pfs.services:
        save_snapshot(service)

# backup persisted data of pfs-service


def save_snapshot(pfs_service):
    snapshot_dir = config.snapshot_dir
    assert("beegfs" in pfs_service.path or "orangefs" in pfs_service.path or "glusterfs" in pfs_service.path)
    data_dirs = " ".join(pfs_service.data_dirs)
    if pfs_service.pfs.fstype == "glusterfs":
        subprocess.call("sudo tar --xattrs --xattrs-include='*' -cf %s.tar -C %s %s "
                        % (snapshot_dir+pfs_service.name, pfs_service.path, data_dirs), shell=True)
    else:
        subprocess.call("tar --xattrs --xattrs-include='*' -cf %s.tar -C %s %s "
                    % (snapshot_dir+pfs_service.name, pfs_service.path, data_dirs), shell=True)

# restore pfs backup


def pfs_restore(pfs, whitelist=[], blocklist=None):
    assert not blocklist or len(whitelist) == 0
    to_restart = []
    if blocklist != None:
        to_restart = filter(lambda s: s.name in blocklist, pfs.services)
    else:
        to_restart = filter(lambda s: s.name not in whitelist, pfs.services)

    for service in to_restart:
        #print(service.name)
    #for service in pfs.services:
        try:
            restore_snapshot(service)
        # guarantee atomicity of service restoring
        except KeyboardInterrupt:
            restore_snapshot(service)
            exit(0)

# restore pfs-service backup


def restore_snapshot(pfs_service):
    snapshot_dir = config.snapshot_dir
    assert("beegfs" in pfs_service.path or "orangefs" in pfs_service.path or "glusterfs" in pfs_service.path)

    if pfs_service.pfs.fstype == "beegfs":
        for data_dir in pfs_service.data_dirs:
            subprocess.call("find %s -type f | xargs rm -f" % os.path.join(pfs_service.path, data_dir), shell=True)

        # print("tar --xattrs --xattrs-include='*' -xf %s.tar -C %s"
        #     % (snapshot_dir+pfs_service.name, pfs_service.path))
        subprocess.call("tar --xattrs --xattrs-include='*' -xf %s.tar -C %s"
                        % (snapshot_dir+pfs_service.name, pfs_service.path), shell=True)

    elif pfs_service.pfs.fstype == "glusterfs":
        for data_dir in pfs_service.data_dirs:
            subprocess.call("rm -rf %s/*" % os.path.join(pfs_service.path, data_dir), shell=True)
            subprocess.call("sudo rm -rf %s/.glusterfs/" % os.path.join(pfs_service.path, data_dir), shell=True)
        subprocess.call("sudo tar --xattrs --xattrs-include='*' -xf %s.tar -C %s"
                        % (snapshot_dir+pfs_service.name, pfs_service.path), shell=True)

    elif pfs_service.pfs.fstype == "orangefs":
        for data_dir in pfs_service.data_dirs:
            #print("restoring %s %s " % (pfs_service.name, os.path.join(pfs_service.path, data_dir)))
            subprocess.call("rm -rf %s/*" % os.path.join(pfs_service.path, data_dir), shell=True)
        subprocess.call("tar -xf %s.tar -C %s" % (snapshot_dir+pfs_service.name, pfs_service.path), shell=True)

# backup workload


def save_workload(pfs, workload, snapshot_dir):
    try:
        process = subprocess.Popen("cp -r %s/* %s" % (workload.path, snapshot_dir), shell=True, stderr=subprocess.PIPE)
        stdout, stderr = process.communicate(timeout=30)
        #subprocess.call("ls %s/" % (workload.path), shell=True, stderr=subprocess.PIPE)
        #subprocess.call("cp -r %s/* %s" % (workload.path, snapshot_dir), shell=True, stderr=subprocess.PIPE)

    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        print("Workload saving: PFS Unavailable")
        pfs_restart_clean(pfs, clean=False)
        return -1

    except KeyboardInterrupt:
        process.kill()
        pfs_restart_clean(pfs)
        exit(0)

def restore_workload(pfs, workload):
    assert(workload.path.strip() != "" and workload.path.strip() != "/")
    subprocess.call("rm -rf %s/*" % (workload.path), shell=True, stderr=subprocess.PIPE)
    process = subprocess.call("cp -r %s/* %s" % (config.prefix_snapshot_dir, workload.path), shell=True, stderr=subprocess.PIPE)

def overlap_range(range_1, range_2):
    if range_1[0] <= range_2[0] and range_2[0] <= range_1[1]:
        return True

    if range_2[0] <= range_1[0] and range_1[0] <= range_2[1]:
        return True

    return False


def isDirOp(call):
    return isinstance(call, Mkdir) or isinstance(call, Link) or isinstance(call, Unlink) or isinstance(call, Rename) \
        or isinstance(call, Setxattr) or isinstance(call, Removexattr) or isinstance(call, Creat) or isinstance(call, Truncate)


def get_paths(call):
    if isinstance(call, Mkdir) or isinstance(call, Setxattr) or isinstance(call, Creat) or isinstance(call, Unlink) \
            or isinstance(call, Pwrite) or isinstance(call, Truncate) or isinstance(call, Fsync) or isinstance(call, Removexattr):
        return [call.path]
    elif isinstance(call, Link) or isinstance(call, Rename):
        return [call.srcname, call.destname]
    else:
        raise NotImplementedError


def has_same_path(call_1, call_2):
    for path_1 in get_paths(call_1):
        for path_2 in get_paths(call_2):
            if path_1 == path_2:
                return True
    return False


def has_conflict_path(call_1, call_2):
    for path_1 in get_paths(call_1):
        for path_2 in get_paths(call_2):
            if path_1 in path_2 or path_2 in path_1:
                return True
    return False
