import os
from pfs_utils import *
from pfs import *
from typing import Callable, Iterator, Union, Optional, List
import graphviz
import networkx as nx
from collections import defaultdict
import itertools
import subprocess
import progressbar
import numpy as np
import functools

from tsp_solver.util import path_cost as tsp_path_cost
from tsp_solver.greedy import solve_tsp

import time

prefix_syscall = ['creat', 'pwrite64', 'rename', 'unlink', 'link', 'fsync', 'mkdir']
dir_ops = ['rename', 'unlink', 'link', 'setxattr', 'creat', 'mkdir']


class CrashStateGenerator(object):
    def __init__(self, pfs: PFS, result_dir, lfs='weakfs', distributed=True):
        self.pfs = pfs
        self.result_dir = result_dir
        self.mpi_lib = None
        if config.parallel:
            self.mpi_lib = MPI(pfs.workload, pfs.op_mapping)
        self.distributed = distributed
        self.crash_states = []
        # generate dependencies between syscall and dump the dependency graph
        self.exec_graph = ExecGraph(self.pfs.services)
        self.exec_graph.dependencies()
        self.exec_graph.dump(os.path.join(self.result_dir, "exec_graph_full.gv"))
        self.exec_graph.reduction()
        self.exec_graph.dump(os.path.join(self.result_dir, "exec_graph.gv"))
        self.causality = Causality(lfs, self.exec_graph.graph)

    # combine and sort all disk ops from each service and perform them sequentially
    def replay(self):
        syscalls = []
        for service in self.pfs.services:
            syscalls += service.syscalls
        syscalls = list(filter(lambda x: not isinstance(x, Recvfrom) and not isinstance(x, Sendto), syscalls))
        syscalls = sorted(syscalls, key=lambda x: x.timestamp)
        for syscall in syscalls:
            syscall.perform()

    def crash(self, crash_func=None):
        G = self.exec_graph.graph

        # all prefixes of all topologic sort of execution graph
        syscall_map = defaultdict(lambda: list([None]))
        for syscall in G.nodes():
            syscall_map[syscall.service.name].append(syscall)
        all_combinations = list(itertools.product(*list(syscall_map.values())))
        # all combinations of frontier calls
        mask = dict([(all_combinations[-1][i].service.name, i) for i in range(len(all_combinations[-1]))])
 
        for dependency in filter(lambda x: x[0].service.name != x[1].service.name, G.edges()):
            src, dest = dependency
            all_combinations = list(
                filter(
                    lambda
                    x:
                    not(
                        (x[mask[src.service.name]] is None or x[mask[src.service.name]].timestamp < src.timestamp)
                        and x[mask[dest.service.name]] is not None and x[mask[dest.service.name]].timestamp >= dest.timestamp),
                    all_combinations))

        if config.verbose:
            print("Total %d frontier combinations" % len(all_combinations))

        # generate all crash states
        for i, frontier in enumerate(all_combinations):
            all_syscalls = list(filter(lambda x: frontier[mask[x.service.name]] is not None and
                                       x.timestamp <= frontier[mask[x.service.name]].timestamp, G.nodes()))
            all_syscalls = sorted(all_syscalls, key=lambda x: x.timestamp)
            new_state = CrashState(all_syscalls)
            self.crash_states.append(new_state)

            for k in range(1, 3):
                victim_combination = list(itertools.combinations(filter(lambda x: not isinstance(x, Fsync), all_syscalls[:-1]), k))

                for victims in reversed(victim_combination):
                #for i, syscall in reversed(list(enumerate(all_syscalls[:-1]))):
                    dependent_calls = []
                    append = True
                    for victim in victims:
                        dependencies = self.causality.persists_before_all(victim, all_syscalls[all_syscalls.index(victim)+1:])
                        if dependencies != None:
                            dependent_calls += dependencies
                        else:
                            append = False
                            #self.crash_states.append(CrashState([s for s in all_syscalls if s not in dependent_calls], base_state=new_state,
                            #                                   reorder=(victim, all_syscalls[-1], "FS"), dependent_calls=dependent_calls))

                        if self.mpi_lib:
                            retval, dependencies = self.mpi_lib.persists_before_all(victim, all_syscalls[i+1:])
                            if retval is Causality.RE_ORDERED:
                                dependent_calls += dependencies
                                #print(syscall.short(), all_syscalls[-1].short(), "MPI")
                    
                    if append:
                        #print(victims, dependent_calls)
                        self.crash_states.append(CrashState([s for s in all_syscalls if s not in dependent_calls], base_state=new_state,
                                                                reorder=(dependent_calls, all_syscalls[-1], "FS"),
                                                                dependent_calls=victims))

        #exit(0)
        # remove duplicated states
        to_remove = []
        total_crash_states = len(self.crash_states)
        print("Total %d crash states" % len(self.crash_states))

        # pruning with hdf5 info
        if config.is_hdf5 and config.pruning:
            for state in self.crash_states:
                if state.reorder and \
                    (self.pfs.obj_mapping.is_datachunk(state.reorder[0]) or self.pfs.obj_mapping.is_datachunk(state.reorder[1])):
                    to_remove.append(state)
                elif len(state.syscalls) >= 1 and self.pfs.obj_mapping.is_datachunk(state.syscalls[-1]):
                   to_remove.append(state)
                # elif len(state.syscalls) >= 1 and isinstance(state.syscalls[-1], Setxattr):
                #    to_remove.append(state)
        if config.pruning:
            for state in self.crash_states:
                if len(state.syscalls) >= 1 and isinstance(state.syscalls[-1], Fsync):
                    to_remove.append(state)
                    continue
                # elif state.reorder:
                #     for call in state.dependent_calls:
                #         if isinstance(call, Fsync):
                #             to_remove.append(state)
                #             break
                # elif len(state.syscalls) >= 1 and isinstance(state.syscalls[-1], Truncate):
                #    to_remove.append(state)
        for r in set(to_remove):
            self.crash_states.remove(r)
        to_remove = []
        total_crash_states = len(self.crash_states)
        print("Total %d crash states" % len(self.crash_states))

        for i in range(len(self.crash_states)):
            #if i % 10 == 0:
            #    print(i)
            for j in range(i+1, len(self.crash_states)):
                if self.crash_states[i].same_as(self.crash_states[j]):
                    state_i = self.crash_states[i]
                    state_j = self.crash_states[j]
                    if (state_i.reorder and state_i.reorder[2] == "MPI") or state_i in to_remove:
                        to_remove.append(state_j)
                    elif state_i.reorder and state_j.reorder:
                        if len(state_i.reorder[0]) < len(state_j.reorder[0]): 
                            to_remove.append(state_j)
                        else:
                            to_remove.append(state_i)
                    else:
                        to_remove.append(state_i)
                    break

        # print(to_remove)
        for r in to_remove:
            self.crash_states.remove(r)

        # for cs in self.crash_states:
        #     print(cs)
        # exit(0)
        print("Total %d crash states to explore" % len(self.crash_states))

        #calculate transition matrix & compute optimal exploration path
        
        if config.opt_explore:
            num_states = len(self.crash_states)
            transition = np.zeros((num_states,num_states))
            for i in range(num_states):
                for j in range(num_states):
                    transition[i][j], _ = self.crash_states[i].diff_servers(self.crash_states[j])

            #np.set_printoptions(threshold=np.inf)
            #print(transition)
            naive = 0
            for i in range(1,num_states):
                naive += transition[i][i - 1]

            opt_path = solve_tsp(transition, endpoints=(0,num_states-1))
            #print(opt_path)
            print(naive, tsp_path_cost(transition, opt_path))

            new_states = [self.crash_states[opt_path[i]] for i in range(num_states)]
            self.crash_states = new_states

        if not config.verbose:
            widgets = ['Progress: ', progressbar.Bar(), ' ', progressbar.Percentage()]
            bar = progressbar.ProgressBar(widgets=widgets, maxval=len(self.crash_states)).start()

        # generate crash states for each distributed frontier
        time_start = time.time()
        time_deducted = 0  # time deducted for waiting timeout

        num_bugs = 0
        num_states_explored = 0
        reorder_bugs = []
        atomic_bugs = []
        i = 0
        for state in self.crash_states:
            #print(state)
            i += 1
            if not config.verbose:
                bar.update(i)

            # check similar states
            # if last_state != None and

            # non-reordered crash state
            if not state.has_reorder():
                num_states_explored += 1
                retval = crash_func(state)
                if retval:
                    succs = list(self.exec_graph.graph.successors(state.syscalls[-1]))
                    atomic_bugs.append(state)
                    if retval == "PFS Unavailable":
                        time_deducted += config.timeout

            # reordered crash state (call_1 -> call_2)
            else:
                state_pruned = False
                # a) no bugs in base state
                if state.base_state in atomic_bugs:
                   state_pruned = True
                # b) are not data chunks, should be removed in previous steps
                if config.is_hdf5:
                    if self.pfs.obj_mapping.is_datachunk(state.reorder[1]):
                        state_pruned = True
                    else:
                        for call in state.reorder[0]:
                            if self.pfs.obj_mapping.is_datachunk(call):
                                state_pruned = True
                                break
                # c) call_1 has no reorder bug before
                # if list(zip(*reorder_bugs)) != [] and state.reorder[0] in list(zip(*reorder_bugs))[0]:
                #    state_pruned = True
                # c) dependent calls (including reordered call itself) has no reorder bug before
                if list(zip(*reorder_bugs)) != []:
                    for call in state.reorder[0]:
                        if call in list(zip(*reorder_bugs))[0]:
                            state_pruned = True
                            break
                if state_pruned and config.pruning:
                    continue

                num_states_explored += 1
                retval = crash_func(state)
                if retval:
                    #reorder_bugs.append(state.reorder[0])
                    real_reorder = self.auxiliary_explore(crash_func, state)
                    for call in real_reorder:
                        reorder_bugs.append((call, state.reorder[1]))
                    #reorder_bugs += state.reorder[0]
                    print(reorder_bugs)
                    if retval == "PFS Unavailable":
                        time_deducted += config.timeout


        if not config.verbose:
            bar.finish()

        time_end = time.time()


        #weakfs_bugs = len(reorder_bugs) + len(atomic_bugs)
        ext3_dj_bugs = len(atomic_bugs) + len([pair for pair in reorder_bugs if pair[0].service.name != pair[1].service.name])

        #def is_overwrite(syscall):
        #    return isinstance(syscall, Pwrite) and syscall.func_name != 'append'
        #ext3_oj_bugs = len(atomic_bugs) + len([pair for pair in reorder_bugs if pair[0].service.name != pair[1].service.name
        #                                       or (is_overwrite(pair[0]) and is_overwrite(pair[1]))])

        print("Total %d crash states, %d states explored, %d vulnerabilities in ext3-dj"
              % (total_crash_states, num_states_explored, ext3_dj_bugs))
        print("Total exploration time: %.2f, deducted time %.2f" % (((time_end - time_start) - time_deducted), time_deducted))
        #self.exec_graph.dump(os.path.join(self.result_dir, "exec_graph.gv"), reorder_bugs, atomic_bugs)

    def auxiliary_explore(self, crash_func, state):
        real_reorder = []
        reordering = state.reorder[0]
        
        for call in reordering:
            new_calls = state.base_state.syscalls
            aux_state = CrashState([s for s in new_calls if s!=call], base_state=state.base_state, reorder=[call], dependent_calls=[call])
            print("Aux:", end="")
            retval = crash_func(aux_state)
            if retval:
                real_reorder.append(call)
        print(real_reorder)
        return real_reorder



class CrashState(object):
    def __init__(self, syscalls, base_state=None, reorder=None, dependent_calls=[]):
        self.syscalls = syscalls
        self.reorder = reorder
        self.dependent_calls = dependent_calls
        self.base_state = base_state
        self.servers = {s.service for s in self.syscalls}

    def __str__(self):
        __str = ""
        if self.reorder:
            __str += str(self.dependent_calls)+" "+str(self.reorder)+" "

        __str += str(self.syscalls)
        return __str

    def emulate(self, start=0, whitelist=[], blocklist=None):
        assert not blocklist or len(whitelist) == 0
        if blocklist:
            to_restart = list(filter(lambda s: s.name in blocklist, self.servers))
        else:
            to_restart = list(filter(lambda s: s.name not in whitelist, self.servers))

        #print("cs"+str(to_restart))
        for syscall in self.syscalls[start:]:
            #print(syscall.short(), syscall.service.name)
            #print(syscall.service in to_restart)
            if syscall.service in to_restart:
                #print(syscall.short())
                syscall.perform()

    def has_reorder(self):
        return self.reorder is not None

    # check if cs_2 is contained in self
    def contains(self, cs_2):
        # if cs_2 == None:
        #     return None
        # _diff = (set(cs_2.syscalls) - set(self.syscalls))
        # if len(_diff) != 0:
        #     return None

        # _diff = (set(self.syscalls) - set(cs_2.syscalls))
        # _min = float('inf')
        # for call in _diff:
        #     index = self.syscalls.index(call)
        #     _min = min(_min, index)
               
        # return _min
        concat = len(cs_2.syscalls)
        is_prefix = len((set(self.syscalls[:concat]) - set(cs_2.syscalls)) | (set(cs_2.syscalls) - set(self.syscalls[:concat]))) == 0
        if is_prefix:
            return concat
        else:
            return None

    def diff_servers(self, cs_2):
        _diff = (set(self.syscalls) - set(cs_2.syscalls)) | (set(cs_2.syscalls) - set(self.syscalls))
        _diff_servers = {s.service for s in _diff}
        return len(_diff_servers), _diff_servers

    def same_as(self, cs_2):
        if len(self.syscalls) != len(cs_2.syscalls):
            return False
        _diff = (set(self.syscalls) - set(cs_2.syscalls)) | (set(cs_2.syscalls) - set(self.syscalls))
        return len(_diff) == 0


class Causality(object):

    def __init__(self, lfs_name, G):
        self.lfs = lfs_name
        self.G = G

        self.hbgraph = defaultdict(lambda: defaultdict(bool))
        self.pbgraph = defaultdict(lambda: defaultdict(bool))
        # happens-before graph
        nodes = list(G.nodes())
        TCG = nx.adjacency_matrix(nx.transitive_closure(G))
        for i in range(len(nodes)):
            for j in range(len(nodes)):
                if TCG[i, j] == 1:
                    self.hbgraph[nodes[i]][nodes[j]] = True
                else:
                    self.hbgraph[nodes[i]][nodes[j]] = False

        # persists-before graph
        for call_1 in nodes:
            for call_2 in nodes:
                if call_1.service.name == call_2.service.name:
                    self.pbgraph[call_1][call_2] = self.hbgraph[call_1][call_2]
                else:
                    self.pbgraph[call_1][call_2] = False
                    for sync in nodes:
                        if isinstance(sync, Fsync) and has_same_path(sync, call_1) and self.hbgraph[call_1][sync] \
                            and self.hbgraph[sync][call_2]:
                            self.pbgraph[call_1][call_2] = True

    def persists_before_all(self, base_call, calls):
        pivot_call = calls[-1]
        if self.persists_before(base_call, pivot_call):
            return None

        dependent_calls = [base_call]
        for call in calls[:-1]:
            retval = self.persists_before(base_call, call)
            #print(base_call.short(), call.short(), retval)

            if retval:
                if not self.persists_before(call, pivot_call):
                    dependent_calls.append(call)
                else:
                    return None

        return dependent_calls

    def persists_before(self, call_1, call_2):
        return self.pbgraph[call_1][call_2]
        

    # def persists_before(self, call_1, call_2):

        if isinstance(call_1, Fsync):
            return self.PERSISTED

        if call_1.service.name != call_2.service.name:
            return self.RE_ORDERED

        if isinstance(call_1, Pwrite) and isinstance(call_2, Pwrite):
            range_1 = (call_1.offset, call_1.offset+call_1.length-1)
            range_2 = (call_2.offset, call_2.offset+call_2.length-1)
            if overlap_range(range_1, range_2) and call_1.path == call_2.path:
                return self.ORDERED
            return self.RE_ORDERED

        if isinstance(call_1, Pwrite) and isinstance(call_2, Fsync):
            if call_1.path == call_2.path:
                return self.PERSISTED
            return self.RE_ORDERED

        if isinstance(call_1, Pwrite):
            return self.ORDERED

        if isDirOp(call_1) and isDirOp(call_2):
            # NOTE here we only consider order journaling
            if has_conflict_path(call_1, call_2) or call_1.service.name == call_2.service.name:
                return self.ORDERED
            return self.RE_ORDERED

        if isDirOp(call_1) and isinstance(call_2, Fsync):
            if has_same_path(call_1, call_2):
                return self.PERSISTED
            else:
                return self.RE_ORDERED

        if isDirOp(call_1) and isinstance(call_2, Pwrite):
            if has_conflict_path(call_1, call_2):
                return self.ORDERED
            else:
                return self.RE_ORDERED

        return self.PERSISTED


class MPI(object):
    ORDERED = 1
    RE_ORDERED = 2

    def __init__(self, workload, op_mapping):
        assert(isinstance(workload, MPI_Workload))
        self.workload = workload
        self.op_mapping = op_mapping

    def persists_before_all(self, base_call, calls):
        dependent_calls = []
        pivot_call = calls[-1]

        if isinstance(base_call, Pwrite) and isinstance(pivot_call, Pwrite):
            base_workload_calls = self.op_mapping.rev_mapping[base_call]
            if len(base_workload_calls) != 1:
                return (self.ORDERED, None)
            base_workload_call = base_workload_calls[0]

            all_base_server_calls = self.op_mapping.mapping[base_workload_call]
            dependent_calls += all_base_server_calls

            pivot_workload_calls = self.op_mapping.rev_mapping[pivot_call]
            if len(pivot_workload_calls) != 1:
                return (self.ORDERED, None)
            pivot_workload_call = pivot_workload_calls[0]

            # FIXME relax this
            all_pivot_server_calls = self.op_mapping.mapping[pivot_workload_call]
            if len(all_pivot_server_calls) != 1:
                return (self.ORDERED, None)

            if not (self.workload.get_barrier_id(base_workload_call) == self.workload.get_barrier_id(pivot_workload_call)
                    and base_workload_call.service.name != pivot_workload_call.service.name):
                return (self.ORDERED, None)

            # add dependencies
            all_call_between_barriers = self.workload.barrier_groups[self.workload.get_barrier_id(base_workload_call)]
            for call in all_call_between_barriers:
                if call.service.name == base_workload_call.service.name and call.gid > base_workload_call.gid \
                        and call.gid < pivot_workload_call.gid:
                    dependent_calls += self.op_mapping.mapping[call]
            
            #print(base_call.short(), pivot_call.short(), base_workload_call.short(), pivot_workload_call.short(), dependent_calls)

            return (self.RE_ORDERED, dependent_calls)
        
        else:
            return (self.ORDERED, None)


# execution graph that represents dependency between syscalls
class ExecGraph(object):
    def __init__(self, pfs_services):
        self.graph = nx.DiGraph()
        self.pfs_services = pfs_services

    def dependencies(self):
        G = self.graph

        # generate dependencies within a service
        for service in self.pfs_services:
            G.add_nodes_from(service.syscalls)
            G.add_edges_from(zip(service.syscalls[:-1], service.syscalls[1:]))

        # generate dependencies across services
        # add dependency between each pair of send-recv
        for service in self.pfs_services:
            for send_call in filter(lambda x: isinstance(x, Sendto), service.syscalls):
                if not send_call.correlated_call:
                    continue
                G.add_edge(send_call, send_call.correlated_call)

        # add dependency between communications with client
        calls_from_client = []
        last_send_call = defaultdict(list)
        last_communication = defaultdict(lambda: defaultdict(str))
        for service in self.pfs_services:
            for client_call in filter(lambda x: isinstance(x, Sendto) or isinstance(x, Recvfrom), service.syscalls):
                if not client_call.correlated_call:
                    calls_from_client.append(client_call)
        calls_from_client = sorted(calls_from_client, key=lambda c: c.timestamp)

        for client_call in calls_from_client:
            service_name = client_call.service.name
            if isinstance(client_call, Sendto):
                last_send_call[service_name] = client_call
            else:
                for other_service in filter(lambda x: x != service_name, last_send_call.keys()):
                    send_call = last_send_call[other_service]
                    if not send_call:
                        continue
                    if last_communication[service_name][other_service] == send_call:
                        continue
                    G.add_edge(send_call, client_call)
                    last_communication[service_name][other_service] = send_call

    # nicely remove network calls and perform transitive reduction to simplify the graph
    def reduction(self):
        G = self.graph

        for call in list(filter(lambda x: isinstance(x, Recvfrom), G.nodes())):
            self.nicely_remove_node(call)

        for call in list(filter(lambda x: isinstance(x, Sendto), G.nodes())):
            self.nicely_remove_node(call)

        self.graph = ExecGraph.adapted_transitive_reduction(G)

    # adapted transitive reduction of DAG (edges with in a cluster is kept for visualization purpose)
    @classmethod
    def adapted_transitive_reduction(cls, G):
        intra_edges = [x for x in G.edges() if x[0].service.name == x[1].service.name]
        RG = nx.algorithms.dag.transitive_reduction(G)

        RG.add_edges_from([edge for edge in intra_edges if not RG.has_edge(*edge)])
        return RG

    # remove node while re-connecting its predecessors and successors
    def nicely_remove_node(self, node):
        G = self.graph
        if not G.has_node(node):
            return
        preds = list(G.predecessors(node))
        succs = list(G.successors(node))
        #assert((len(preds) <= 1 and len(succs) <= 1) or len(preds) == 0 or len(succs) == 0)
        G.remove_node(node)
        edges = [(pred, succ) for pred in preds for succ in succs]
        G.add_edges_from(edges)

    def dump(self, filename, reorderings=[], atomicities=[]):
        G = self.graph
        DG = DrawGraph()

        cluster = defaultdict(list)
        for call in G.nodes():
            service_name = call.service.name
            cluster[service_name].append(call)

        for service_name in cluster.keys():
            DG.add_cluster(service_name, cluster[service_name])

        for edge in G.edges():
            DG.add_dependency(*edge)

        for bug in reorderings:
            DG.add_dependency(*bug, "red")

        # for bug in set(atomicities):
        #    DG.add_dependency(*bug, "red", dir="both")

        DG.dump(filename)

# graphviz graph to nicely dump dependency graph


class DrawGraph(object):
    def __init__(self):
        self.graph = graphviz.Digraph('exec_graph')
        self.graph.graph_attr['rankdir'] = 'LR'
        self.frame = graphviz.Digraph(name='cluster_frame')
        self.frame.attr(style="invis")
        self.subgraphs = dict()

    # draw dependencies within a node
    def add_cluster(self, service_name, calls):
        cluster = graphviz.Digraph(name='cluster_'+service_name)
        self.subgraphs[service_name] = cluster
        cluster.attr(style='dashed')
        cluster.node_attr.update(style='filled', color='white')
        cluster.attr(label=service_name)

        for syscall in sorted(calls, key=lambda c: c.timestamp):
            curr_node = self.nodename(syscall)
            cluster.node(curr_node, label=syscall.short())

        self.frame.subgraph(cluster)

    # draw dependencies across nodes
    def add_dependency(self, node1, node2, color="black", dir=None):
        if dir != "both":
            self.graph.edge(self.nodename(node1), self.nodename(node2), color=color)
        else:
            self.graph.edge(self.nodename(node1), self.nodename(node2), color=color, dir="both")

    def nodename(self, syscall):
        return "%s-%d" % (syscall.func_name, syscall.gid)

    def dump(self, filename):
        self.graph.subgraph(self.frame)
        self.graph.render(filename, format="pdf")

