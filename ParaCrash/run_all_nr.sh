if [ -z "$VERBOSE" ]; 
then 
    VERBOSE=0 
fi

if [[ "$VERBOSE" == 1 ]]; 
then
    V="-v"
else
    V="-nv"
fi

set -x

## posix
./pfs_check -f configs/beegfs_4.cfg -d workloads/cr/ -m check -r -NR $V
./pfs_check -f configs/beegfs_4.cfg -d workloads/rc/ -m check -r -NR $V
./pfs_check -f configs/beegfs_4.cfg -d workloads/arvr/ -m check -r -NR $V
./pfs_check -f configs/beegfs_4.cfg -d workloads/wal/ -m check -r -NR $V

## hdf5
./pfs_check -f configs/beegfs_4.cfg -d workloads/hdf5-create/ -m check -h5 -r -NR $V
./pfs_check -f configs/beegfs_4.cfg -d workloads/hdf5-resize/ -m check -h5 -r -NR $V
./pfs_check -f configs/beegfs_4.cfg -d workloads/hdf5-resize-large/ -m check -h5 -r -NR $V

set +x