from pfs_utils import *
from collections import defaultdict
import os
import subprocess
import math
import json
from config import config
from sortedcontainers import SortedDict
from re import compile as recompile
import itertools
import progressbar


class PFS:
    def __init__(self, fstype, pfs_services, mount_point, client_name, stripe_size):
        self.services = pfs_services
        self.mount_point = mount_point
        self.client_name = client_name
        self.fstype = fstype
        self.stripe_size = int(stripe_size)
        self.entryinfo_list = defaultdict(list)
        self.entryinfo_hints = None
        self.syscalls = []
        self.g_id = 0
        self.obj_mapping = None
        self.op_mapping = None
        self.workload = None

        if self.fstype == "beegfs":
            self.HEADER_SIZE = 40
            self.HEADER_IDENT = b'\x53\x46\x47\x42'

        elif self.fstype == "orangefs":
            self.HEADER_SIZE = 24
            self.HEADER_IDENT = b'\xbf\xca\x00\x00'

        elif self.fstype == "glusterfs":
            self.HEADER_SIZE = 4
            self.HEADER_IDENT = b'\x80\x00\x00\x80'

        else:
            raise NotImplementedError

        for service in self.services:
            service.pfs = self

    # build fdtable for all services
    def build_fdtable(self):
        # find socket connections
        dict_of_ports = {}
        for service in self.services:
            pid = service.pid
            entries = subprocess.check_output("sudo lsof -i -P | grep %d" % (pid), shell=True).decode().strip().split("\n")
            # only care about entries with "->"
            # e.g. ['beegfs-st', '8962', 'root', '23u', 'IPv4', '6982017', '0t0', 'TCP', 'uiuc-wlspevent-net.gw.uiuc.edu:8010->hpc1:41380', '(ESTABLISHED)']
            for entry in entries:
                if "->" not in entry:
                    continue
                entry = list(filter(None, entry.split(" ")))
                assert(entry[-1] == "(ESTABLISHED)" or entry[-1] == "(CLOSE_WAIT)")
                ports = re.findall(r':(\d+)', entry[-2])  # [src, dest]
                assert(len(ports) == 2)
                src, dest = ports
                service.set_socket(int(entry[3][:-1]), dest)
                dict_of_ports[src] = service.name

        # replace port number with service name
        for service in self.services:
            for conn in service.connections.items():
                fd, dest = conn
                # check who is listening the port
                if dest in dict_of_ports:
                    service.set_socket(fd, dict_of_ports[dest])
                # otherwise the port belongs to client
                else:
                    service.set_socket(fd, self.client_name)

        # build fdtable with useful entries
        for service in self.services:
            pid = service.pid
            entries = subprocess.check_output("sudo ls -l /proc/%d/fd" % (pid), shell=True).decode().strip().split("\n")
            # lr-x------ 1 root root 64 Feb 10 20:44 4 -> /mnt/ssd/data/beegfs/beegfs_mgmtd/lock.pid
            for entry in entries:
                if service.path not in entry or "(deleted)" in entry:
                    continue
                entry = list(filter(None, entry.split(" ")))
                assert(entry[-2] == "->")
                fd, path = int(entry[-3]), entry[-1]
                service.set_desc(fd, path)

        #if config.verbose:
        #    self.printf()

    # build file size table for all services
    def build_fsztable(self):
        for service in self.services:
            files = subprocess.check_output("find %s -type f" % service.path, shell=True).decode().strip().split("\n")
            for file in files:
                service.set_size(file, os.path.getsize(file))

    def create_syscalls(self, files):
        for service in self.services:
            lines = []
            for myfile in filter(lambda f: service.name in f and not f[f.find(service.name)+len(service.name)].isdigit(), files):
                f = open(myfile)
                lines += [line for line in f.readlines()]
                f.close()
            service.create_syscalls(sorted(lines))

        syscalls = sorted([call for service in self.services for call in service.syscalls], key=lambda x: x.timestamp)

        # remove unnecessary network syscalls
        for i, syscall in enumerate(syscalls):
            if isinstance(syscall, Sendto) or isinstance(syscall, Recvfrom):
                syscall.service.syscalls.remove(syscall)
            else:
                break
        format_sendrecv(self)

        self.syscalls = sorted([call for service in self.services for call in service.syscalls], key=lambda x: x.timestamp)
        for syscall in self.syscalls:
            syscall.gid = self.g_id
            self.g_id += 1

    # obtain the round-robin order of file stripe storage
    # FIXME find such entry info for glusterfs
    def entryinfo(self):

        files = [os.path.join(self.mount_point, f) for f in os.listdir(self.mount_point)
                 if os.path.isfile(os.path.join(self.mount_point, f))]
        if self.entryinfo_hints:
            for file in filter(lambda f: f not in self.entryinfo_list, files):
                self.entryinfo_list[file] = self.entryinfo_hints

        else:
            if self.fstype == "beegfs":
                for file in filter(lambda f: f not in self.entryinfo_list, files):
                    entryinfo_rawstring = \
                        subprocess.check_output("beegfs-ctl --getentryinfo %s" % file, shell=True)\
                        .decode().strip().split("\n")
                    #print(self.num_storage_servers)
                    active_servers = int(entryinfo_rawstring[6].strip().split(" ")[-1])
                    print(active_servers)
                    real_entryinfo = entryinfo_rawstring[-active_servers:]
                    #real_entryinfo = entryinfo_rawstring[-self.num_storage_servers:]
                    for entryinfo in real_entryinfo:
                        print(entryinfo)
                        host = recompile('.*\@(.*)\[.*').match(entryinfo).groups()[0].strip()
                        self.entryinfo_list[file].append(self.storage_server_by_host(host))
                # print(self.entryinfo_list)
            elif self.fstype == "orangefs":
                #for file in filter(lambda f: f not in self.entryinfo_list, files):
                for file in files:
                    entryinfo_rawstring = \
                        subprocess.check_output("/opt/orangefs/bin/pvfs2-viewdist -f %s" % file, shell=True)\
                        .decode().strip().split("\n")
                    real_entryinfo = entryinfo_rawstring[-self.num_storage_servers:]
                    #print(entryinfo_rawstring)
                    print(real_entryinfo)
                    for entryinfo in real_entryinfo:
                        host = recompile('.*tcp:\/\/(.*)\,.*').match(entryinfo).groups()[0].strip()
                        self.entryinfo_list[file].append(self.storage_server_by_host(host))

            elif self.fstype == "glusterfs":
                # for file in filter(lambda f: f not in self.entryinfo_list, files):
                #    self.entryinfo_list[file].append(self.storage_server_by_host("hpc1"))
                print("Entryinfo hints should be provided for GlusterFS")
                raise NotImplementedError

    # entryinfo hints for all files
    def set_entryinfo_hints(self, hints):
        self.entryinfo_hints = [self.storage_server_by_name(name.strip()) for name in hints.split(",")]

    @property
    def num_servers(self):
        return len(self.services)

    @property
    def num_storage_servers(self):
        return len(list(filter(lambda s: s.stype == "storage", self.services)))

    def storage_server_by_host(self, host):
        _l = list(filter(lambda s: s.stype == "storage" and s.host == host, self.services))
        assert (len(_l) == 1 and "host name not unique for storage servers")
        return _l[0]

    def storage_server_by_name(self, name):
        _l = list(filter(lambda s: s.stype == "storage" and s.name == name, self.services))
        assert (len(_l) == 1 and "host name not unique for storage servers")
        return _l[0]

    def mapping_with(self, workload, hdf5=False):
        if not hdf5:
            return
        self.workload = workload
        self.op_mapping = OPMapping(self, workload)
        if self.fstype == "orangefs":
            self.op_mapping.match(padding=4096, metadata_filter=".db")
            #self.op_mapping.match(padding=0, metadata_filter=".db")
        else:
            self.op_mapping.match(aggregation=True)

        self.obj_mapping = OBJMapping(self.op_mapping)
        self.obj_mapping.scan()

        # for syscall in filter(lambda o: isinstance(o, Pwrite), self.syscalls):
        #    tag = self.obj_mapping.query(syscall)
        #    print(syscall, tag)

    def replay(self):
        for syscall in self.syscalls:
            syscall.perform()

    def printf(self):
        for service in self.services:
            print(str(service)+"\n")

    def dump(self, storage_dir):
        for service in self.services:
            service.dump(storage_dir+service.name+".strace")


class PFS_Service:
    def __init__(self, name, stype, executable, tag, host, path, data_dirs, pid):
        self.pfs = None
        self.stype = stype   # service type
        self.executable = executable  # service executable
        self.tag = tag  # to distinguish different $(pid)s between instances of the same service
        self.host = host     # hostname from PFS's perspective
        self.name = name    # service name
        self.path = path      # path to store service data
        self.data_dirs = data_dirs   # paths to include in service snapshot

        self.pid = int(pid)   # original pid, could change after restart
        self.syscalls = []    # list of system calls, ordered by timestamp
        self.fdtable = dict()  # file descriptor table (key:fd, value:(path,offset))
        self.fsztable = dict()  # file size table (key:path, value:size)
        self.connections = dict()  # socket connection table (key:fd, value:destination)

    def set_socket(self, fd, dest):
        self.connections[int(fd)] = dest

    def get_socket(self, fd):
        if int(fd) in self.connections:
            return self.connections[int(fd)]
        else:
            return self.pfs.client_name

    def set_desc(self, fd, dest):
        self.fdtable[int(fd)] = [dest, 0]

    def get_desc(self, fd):
        if int(fd) not in self.fdtable:
            return None
        return self.fdtable[int(fd)][0]

    def remove_desc(self, fd):
        if int(fd) not in self.fdtable:
            return
        return self.fdtable.pop(int(fd))

    def get_offset(self, fd):
        if int(fd) not in self.fdtable:
            return 0
        return self.fdtable[int(fd)][1]

    def set_offset(self, fd, offset, mode="SEEK_SET"):
        if int(fd) not in self.fdtable:
            return 0
        if mode == "SEEK_SET":
            self.fdtable[int(fd)][1] = int(offset)
        elif mode == "SEEK_CUR":
            self.fdtable[int(fd)][1] += int(offset)
        elif mode == "SEEK_END":
            self.fdtable[int(fd)][1] = self.fsztable[self.fdtable[int(fd)][0]] + int(offset)
        else:
            raise NotImplementedError
            return

    def get_size(self, path):
        if path not in self.fsztable:
            return 0
        else:
            return self.fsztable[path]

    def set_size(self, path, size):
        self.fsztable[path] = size

    def __str__(self):
        return "%s %s %s %s \n fdtable: %s \n sockets: %s" \
            % (self.name, self.path, self.data_dirs, self.pid, self.fdtable, self.connections)

    def __repr__(self):
        return "%s" % (self.name)

    def dump(self, file):
        f = open(file, "w")
        f.write(str(self)+"\n")
        for syscall in self.syscalls:
            f.write(str(syscall)+"\n")
        f.close()

    # create syscalls in time order
    def create_syscalls(self, lines):
        for line in lines:
            new_syscall = Syscall.add_syscall(line, self, mode="strace", is_workload=False)


class MPI_Workload:
    def __init__(self, pfs, path, num_ranks):
        self.pfs = pfs
        self.num_ranks = 0
        self.path = path
        self.processes = []
        self.gid = 0
        self.syscalls = []
        self.fsztable = dict()  # file size table (key:path, value:size)
        self.barrier_groups = defaultdict(list)
        self.__barrier_map = dict()

        for i in range(num_ranks):
            self.processes.append(POSIX_Workload(self.pfs, self.path, str(i), parent=self))

    def dump(self, storage_dir):
        for process in self.processes:
            process.dump(storage_dir)

    def build_fsztable(self):
        off = 0
        for process in self.processes:
            process.build_fsztable()

    def create_syscalls(self, files):
        lines = []
        for myfile in files:
            process_id = int(os.path.splitext((os.path.basename(myfile)))[0])
            f = open(myfile)
            lines += [(line, process_id) for line in f.readlines() if not "exited with" in line]
            f.close()

        if self.pfs.fstype == "orangefs":
            mpi_only = True
        else:
            mpi_only = False
        for line, process_id in sorted(lines):
            new_syscalls = Syscall.add_syscall(line, self.processes[process_id], mode="recorder", is_workload=True, mpi_only=mpi_only)

        # remove duplicated barriers
        barriers = defaultdict(list)
        duplication = defaultdict(list)
        for process in self.processes:
            for i, syscall in enumerate(process.syscalls):
                if isinstance(syscall, Barrier):
                    barriers[process].append(i)

        # check if same amount of barriers
        assert(len(set(map(len, barriers.values()))) == 1)
        (barrier_count, ) = set(map(len, barriers.values()))

        for i in range(barrier_count):
            is_duplicate = True
            for process in self.processes:
                index = barriers[process][i]
                if index == 0 or index - 1 != barriers[process][i - 1]:
                    is_duplicate = False
                    break
            if is_duplicate:
                for process in self.processes:
                    duplication[process].append(barriers[process][i])

        for process in self.processes:
            process.syscalls = [call for i, call in enumerate(process.syscalls) if i not in duplication[process]]
            for i, barrier in enumerate(filter(lambda call: isinstance(call, Barrier), process.syscalls)):
                barrier.barrier_id = i+1

        self.syscalls = sorted([call for process in self.processes for call in process.syscalls], key=lambda x: x.timestamp)

        for syscall in self.syscalls:
            syscall.gid = self.gid
            self.gid += 1

        for process in self.processes:
            barrier_id = 0
            for syscall in process.syscalls:
                if isinstance(syscall, Barrier):
                    barrier_id = syscall.barrier_id
                else:
                    self.__barrier_map[syscall] = barrier_id
                    self.barrier_groups[barrier_id].append(syscall)

    def get_barrier_id(self, call):
        return self.__barrier_map[call]

    # NOTE more cases in legal replay (? fixed)
    def legal_replay(self, pfs):
        pfs_stop(pfs)
        pfs_restore(pfs)
        pfs_start(pfs)

        all_combinations = []

        # print(self.__barrier_map)
        syscall_map = dict()
        for process in self.processes:
            syscall_map[process.name] = []
        for syscall in self.syscalls:
            syscall_map[syscall.service.name].append(syscall)

        all_combinations = list(itertools.product(*list(syscall_map.values())))

        to_remove = []
        for combination in all_combinations:
            remove = False
            barriers = []
            for call in combination:
                if isinstance(call, Fsync):
                    remove = True
                elif isinstance(call, Barrier):
                    barriers.append(call.barrier_id)
                else:
                    barriers.append(self.__barrier_map[call])

            if len(set(barriers)) > 1:
                remove = True

            if remove:
                to_remove.append(combination)

        for r in to_remove:
            all_combinations.remove(r)

        # for combination in all_combinations:
        #     print(combination)
            # exit(0)
        if config.verbose:
            print("Legal replay involves %d states" % (len(all_combinations)))

        mask = dict([(all_combinations[-1][i].service.name, i) for i in range(len(all_combinations[-1]))])

        widgets = ['Progress: ', progressbar.Bar(), ' ', progressbar.Percentage()]
        bar = progressbar.ProgressBar(widgets=widgets, maxval=len(all_combinations)).start()
        for i in range(len(all_combinations)):
            bar.update(i)
            prefix_dir=os.path.join(config.prefix_replay_dir, "%d" % i)
            os.mkdir(prefix_dir)
            frontier=all_combinations[i]
            all_syscalls=sorted(list(filter(lambda x: frontier[mask[x.service.name]] is not None and
                                            x.timestamp <= frontier[mask[x.service.name]].timestamp, self.syscalls))\
                                , key=lambda x: x.timestamp)
            #print(all_syscalls)
            restore_workload(pfs, self)
            
            for syscall in all_syscalls:
                syscall.perform()
            
            retval = save_workload(pfs, self, prefix_dir)
            # process = subprocess.Popen("exec " + config.checker, cwd=pfs.mount_point, shell=True, stderr=subprocess.PIPE)
            # stdout, stderr = process.communicate(timeout=config.timeout)
            # print(self.syscalls[i-1])
            # print(process.returncode)
            # if process.returncode != 0:
            #     print(stderr.decode())
        bar.finish()
        pfs_stop(pfs)
        pfs_restore(pfs)
        pfs_start(pfs)

        #exit(0)


class POSIX_Workload:
    def __init__(self, pfs, path, name="workload", parent=None):
        self.pfs = pfs
        self.name = name    # service name
        self.path = path  # path to store service data
        self.parent = parent

        self.syscalls = []    # list of system calls, ordered by timestamp
        self.fdtable = dict()  # file descriptor table (key:fd, value:(path,offset))
        self.fsztable = dict()  # file size table (key:path, value:size)
        if self.parent:
            self.fsztable = self.parent.fsztable
        self.g_id = 0

    def set_socket(self, fd, dest):
        pass

    def get_socket(self, fd):
        return None

    def set_desc(self, fd, dest):
        self.fdtable[int(fd)] = [dest, 0]

    def get_desc(self, fd):
        if int(fd) not in self.fdtable:
            return
        return self.fdtable[int(fd)][0]

    def remove_desc(self, fd):
        if int(fd) not in self.fdtable:
            return
        return self.fdtable.pop(int(fd))

    def get_offset(self, fd):
        if int(fd) not in self.fdtable:
            return 0
        return self.fdtable[int(fd)][1]

    def set_offset(self, fd, offset, mode="SEEK_SET"):
        if int(fd) not in self.fdtable:
            return 0
        if mode == "SEEK_SET":
            self.fdtable[int(fd)][1] = int(offset)
        elif mode == "SEEK_CUR":
            self.fdtable[int(fd)][1] += int(offset)
        elif mode == "SEEK_END":
            self.fdtable[int(fd)][1] = self.fsztable[self.fdtable[int(fd)][0]] + int(offset)
        else:
            raise NotImplementedError
            return

    def get_size(self, path):
        if path not in self.fsztable:
            return 0
        else:
            return self.fsztable[path]

    def set_size(self, path, size):
        self.fsztable[path] = size

        # build file size table for all services

    def build_fsztable(self):
        files = subprocess.check_output("find %s -type f" % self.path, shell=True).decode().strip().split("\n")
        if files[0] == '' and len(files) == 1:
            return
        for file in files:
            self.set_size(file, os.path.getsize(file))

    def length(self):
        return len(self.syscalls)

    def __str__(self):
        return "%s %s \n fdtable: %s" \
            % (self.name, self.path, self.fdtable)

    def dump(self, storage_dir):
        file = storage_dir+self.name+".strace"
        f = open(file, "w")
        f.write(str(self)+"\n")
        for syscall in self.syscalls:
            f.write(str(syscall)+"\n")
        f.close()

    # create syscalls in time order
    def create_syscalls(self, files, mode="strace"):
        lines = []
        for myfile in filter(lambda f: self.name in os.path.basename(f), files):
            f = open(myfile)
            lines += [line for line in f.readlines() if not "exited with" in line]
            f.close()

        for line in sorted(lines):
            new_syscalls = Syscall.add_syscall(line, self, mode, is_workload=True)

        self.syscalls = sorted(self.syscalls, key=lambda x: x.timestamp)

        # remove unncessary open-close
        for syscall in self.syscalls:
            syscall.gid = self.g_id
            self.g_id += 1

    def legal_replay(self, pfs):
        pfs_stop(pfs)
        pfs_restore(pfs)
        pfs_start(pfs)

        for i in range(len(self.syscalls)+1):
            prefix_dir = os.path.join(config.prefix_replay_dir, "%d" % i)
            os.mkdir(prefix_dir)
            if i >= 1:
                self.syscalls[i-1].perform()
            retval = save_workload(pfs, self, prefix_dir)

        pfs_stop(pfs)
        pfs_restore(pfs)
        pfs_start(pfs)


class OPMapping:
    def __init__(self, pfs, workload):
        #assert(isinstance(workload, MPI_Workload))
        self.pfs = pfs
        self.workload = workload
        self.mapping = defaultdict(list)   # workload -> pfs
        self.rev_mapping = defaultdict(list)  # pfs->workload
        self.unique_file()

    # ensure that workload targets a unique file
    def unique_file(self):
        self.unique_file = next(filter(lambda o: isinstance(o, Pwrite), self.workload.syscalls)).path
        for write_op in filter(lambda o: isinstance(o, Pwrite), self.workload.syscalls):
            assert(write_op.path == self.unique_file and "File not unique")

    # matches client-side syscall with server-side syscall
    # Supports both 1-to-1, 1-to-N mapping and N-to-1 mapping, neglect N-to-N mapping
    def match(self, padding=0, aggregation=False, metadata_filter=None):
        # NOTE currently supports either padding or aggregation
        assert (padding == 0 or aggregation == False)

        write_ops = dict()
        for service in self.pfs.services:
            write_ops[service] = list(filter(
                lambda s: isinstance(s, Pwrite) and (metadata_filter == None or metadata_filter not in s.path), service.syscalls))

        for syscall in filter(lambda x: isinstance(x, Pwrite), self.workload.syscalls):
            op_list = \
                self.locate(syscall.offset, syscall.length, self.pfs.num_storage_servers, self.pfs.stripe_size, padding=padding)

            for server, l_off, l_length in op_list:
                #print(server, l_off, l_length)
                server = self.pfs.entryinfo_list[self.unique_file][server]
                succeed = False
                for i in range(len(write_ops[server])):
                    candidate = write_ops[server][i]
                    # exact match
                    if candidate.offset == l_off and candidate.length == l_length:
                        self.mapping[syscall].append(candidate)
                        self.rev_mapping[candidate].append(syscall)
                        succeed = True
                        write_ops[server].pop(i)

                    # the range of client update contained by server-side due to aggregation
                    elif aggregation and candidate.offset <= l_off and candidate.offset + candidate.length >= l_off + l_length:
                        succeed = True
                        self.rev_mapping[candidate].append(syscall)
                        #if config.verbose:
                        #    print("Non-exact matching calls!! Consider N-to-1 mapping!!", candidate)
                        # last matched client-side call
                        if candidate.offset + candidate.length == l_off + l_length:
                            write_ops[server].pop(i)
                    if succeed:
                        break

                if not succeed:
                    print("False matching call %s!!" % syscall)
                    exit(-1)

    def locate(self, offset, length, n, stripe_size, padding=0):
        ret_val = []

        if padding > 0:
            # left padding
            length += offset % padding
            offset -= offset % padding

            # right padding
            length += padding - length % padding

        while length > 0:
            server = math.floor(offset / stripe_size) % n
            l_off = math.floor(offset / (stripe_size * n)) * stripe_size + offset % stripe_size
            l_length = min(stripe_size - offset % stripe_size, length)

            ret_val.append((server, l_off, l_length))

            length -= l_length
            offset += l_length

        return ret_val


class OBJMapping:
    def __init__(self, op_mapping):
        self.raw_mapping = dict()  # (key: group/dataset, value: (offset range, obj))
        self.reverse_mapping = SortedDict()  # (key: start_offset, value: (group, obj))
        self.op_mapping = op_mapping
        self.filename = self.op_mapping.unique_file

    def scan(self):
        #        print(config.h5scan_path)

        self.log_path = os.path.join(config.result_dir, os.path.splitext(os.path.basename(self.filename))[0] + ".json")
        # print(self.log_path)
        subprocess.call("%s -l %s %s" % (config.h5scan_path, self.log_path, self.filename), shell=True)
        self.build()

    def build(self):
        f = open(self.log_path, "r")
        json_string = f.read()
        self.raw_mapping = json.loads(json_string)

        reverse_mapping = dict()
        # build reverse_mapping
        for key, value in self.raw_mapping.items():
            #print(key, value)
            # superblock
            if key == "SUPERBLOCK":
                reverse_mapping[tuple(value)] = ("GLOBAL", "SUPERBLOCK")

            if key == "GLOBAL_HEAP":
                reverse_mapping[tuple(value)] = ("GLOBAL", "GLOBAL_HEAP")

            # group or dataset
            elif "_GROUP" in key or "_DATASET" in key:
                for k, v in value.items():
                    if k in ["OBJ_HEADER", "LOCAL_HEAP", "DATA_SEGMENT"]:
                        reverse_mapping[tuple(v)] = (key, k)
                    elif k in ["BTREE_NODES", "SYMBOL_TABLE", "DATA_CHUNKS"]:
                        for v0 in v:
                            reverse_mapping[tuple(v0)] = (key, k)
                    else:
                        assert(k == "BASE")
        # sort
        for key in sorted(reverse_mapping):
            start_off, end_off = key
            v = reverse_mapping[key]

            if start_off != 0:
                last_off, last_v = self.reverse_mapping.items()[-1]
                if last_off == start_off and v == last_v:
                    self.reverse_mapping.popitem(-1)
                elif last_off == start_off and v != last_v:
                    self.reverse_mapping[start_off] = v
                else:
                    self.reverse_mapping[last_off] = ("GLOBAL", "FREE_SPACE")
                    self.reverse_mapping[start_off] = v
                self.reverse_mapping[end_off] = v

            else:
                self.reverse_mapping[start_off] = v
                self.reverse_mapping[end_off] = v

        self.reverse_mapping[self.reverse_mapping.keys()[-1]] = ("GLOBAL", "END_OF_FILE")

        # for item in self.reverse_mapping.items():
        #     print(item)
        # print(self.reverse_mapping)
        f.close()

    def query(self, pfs_call):
        if not isinstance(pfs_call, Pwrite):
            return None

        workload_calls = self.op_mapping.rev_mapping[pfs_call]
        tags = []

        for workload_call in workload_calls:
            assert(isinstance(workload_call, Pwrite))

            start_off = workload_call.offset
            start_pos = self.reverse_mapping.bisect_right(start_off) - 1

            end_off = start_off + workload_call.length
            end_pos = self.reverse_mapping.bisect_left(end_off) - 1

            if start_pos == end_pos:
                tags += [self.reverse_mapping.values()[start_pos]]
            else:
                tags += self.reverse_mapping.values()[start_pos:end_pos]

        tags = list(set(tags))
        return tags

    def is_datachunk(self, pfs_call):
        tags = self.query(pfs_call)
        return tags != None and len(tags) == 1 and tags[0][1] == "DATA_CHUNKS"
