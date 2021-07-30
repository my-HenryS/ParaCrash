# configuration of PFSCheck

# An example of experiment layout

# -config_file
# -self.work_dir/
#	|-- result/
#	|	  |-- errs.log
#	|	  |-- human-readable traces, figures
# 	|-- storage/
#	|     |-- prefix_replay
#	|     |-- snapshot
#	|     |-- traces
#   |     |-- recorder_traces
#   |-- bin/
#	|     |-- init
#	|     |-- workload
#	|     |-- checker
#
#
import os
import subprocess


class Config:
    def __init__(self):
        self.init = False

    def read_args(self, args):
        self.init = True

        # root directory for experiment
        self.work_dir = os.path.abspath(args.work_dir)

        # raw data storage direcory
        self.storage_dir = os.path.join(self.work_dir, "storage/")
        self.trace_dir = os.path.join(self.storage_dir, "traces/")
        self.recorder_trace_dir = os.path.join(self.storage_dir, "recorder_traces/")
        self.snapshot_dir = os.path.join(self.storage_dir, "snapshot/")
        self.prefix_replay_dir = os.path.join(self.storage_dir, "prefixes/")
        self.prefix_snapshot_dir = os.path.join(self.storage_dir, "prefixes/snapshot/")

        # path to executables
        self.init = os.path.join(self.work_dir, args.init)
        self.workload = os.path.join(self.work_dir, args.workload)
        self.checker = os.path.join(self.work_dir, args.checker)

        # path to recorder library
        self.recorder_path = args.recorder_path

        # path to h5scan library
        self.h5scan_path = args.h5scan_path
        self.is_hdf5 = args.is_hdf5

        # checker settings
        self.verbose = args.verbose
        self.legal_replay = args.legal_replay
        self.restart = args.restart
        self.config_file = args.config_file
        self.timeout = args.timeout
        self.parallel = args.parallel
        self.is_inc = args.is_inc
        self.opt_explore = args.opt_explore
        self.pruning = args.pruning
        self.tracing = args.tracing

        # experiment output directory
        self.result_dir = os.path.join(self.work_dir, "result/")
        self.err_dir = os.path.join(self.result_dir, "errs/")

        if args.log == None:
            args.log = "%s_errs.log" % os.path.splitext(os.path.basename(self.config_file))[0]

        self.errlog = os.path.join(self.result_dir, args.log)

        # PFSCheck mode {config, restore, record, check}
        self.mode = args.mode

        if self.mode in ['config', 'record', 'check']:
            self.check_params()
            self.create_dirs()

    def check_params(self):
        assert(self.work_dir.strip() != "" and self.work_dir.strip() != "/")  # prevent rm -rf /
        assert(os.path.exists(self.init))
        assert(os.path.exists(self.workload))
        if not self.legal_replay:
            assert(os.path.exists(self.checker))

    def create_dirs(self):
        # create snapshot and trace directory

        subprocess.call(("sudo rm -rf %s/" % (self.storage_dir)).split(' '))
        subprocess.call("mkdir -p %s" % (self.result_dir), shell=True)
        subprocess.call("mkdir -p %s" % (self.err_dir), shell=True)
        subprocess.call("mkdir -p %s" % (self.trace_dir), shell=True)
        subprocess.call("mkdir -p %s" % (self.snapshot_dir), shell=True)
        subprocess.call("mkdir -p %s" % (self.prefix_replay_dir), shell=True)
        subprocess.call("mkdir -p %s" % (self.prefix_snapshot_dir), shell=True)


global config
config = Config()
