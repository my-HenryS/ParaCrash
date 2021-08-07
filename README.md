# ParaCrash v1.0

ParaCrash aims at testing crash vulnerabilities of parallel file systems and I/O libraries. We release ParaCrash for BeeGFS/OrangeFS/GlusterFS, and provide a number of test suites. 
## 1. Environment Variables
```shell
# set $PARACRASH_PATH to the root dir of ParaCrash
export PARACRASH_PATH=$HOME/ParaCrash
```

## 2. BeeGFS Installation

```shell
# install BeeGFS packages 

wget -q https://www.beegfs.io/release/latest-stable/gpg/DEB-GPG-KEY-beegfs -O- | sudo apt-key add -
wget https://www.beegfs.io/release/beegfs_7.2.3/dists/beegfs-deb9.list
sudo mv beegfs-deb9.list /etc/apt/sources.list.d
sudo apt update
sudo apt install beegfs-mgmtd beegfs-meta beegfs-storage beegfs-client beegfs-utils beegfs-helperd

# generate a four-server BeeGFS config file and a ParaCrash config file 
# beegfs config files is located at /etc/beegfs and the ParaCrash config files is located at $PARACRASH_PATH/config
pip3 install configobj configparser
sudo chown -R $USER /etc/beegfs/
cd ~/software/ParaCrash/ParaCrash/scripts
python3 beegfs-config.py [hostname] 2

# startup beegfs in multi-mode
sudo mkdir -p /data/beegfs
sudo chown -R $USER /data/beegfs/
sudo mkdir -p /mount/beegfs
./beegfs.sh 2
```



## 3. Software Dependencies

```shell
## install spack dependencies
sudo apt-get install build-essential gfortran unzip python3-dev python3-apt python3-distutils
spack compiler find gfortran
spack external find python

## install I/O libs mpich, hdf5, h5py
spack install py-h5py@2.10.0^py-numpy@1.19.0^hdf5@1.8.12^mpich@3.04

## install ParaCrash dependencies
sudo apt-get install graphviz
pip3 install sortedcontainers progressbar graphviz networkx tsp_solver2 scipy

## compile and install recorder
cd ParaCrash/Recorder/
## recorder that only traces HDF5
./config.sh $(spack find --format "{prefix}" hdf5) $(spack find --format "{prefix}" mpich) -DDISABLE_MPIO_TRACE -DDISABLE_POSIX_TRACE
make install
mv ~/local/lib/librecorder_hdf5.so ~/local/lib/librecorder_hdf5.so
## recorder that traces all layers
./config.sh $(spack find --format "{prefix}" hdf5) $(spack find --format "{prefix}" mpich)
make clean
make install

## install h5inspect
cd ParaCrash/h5inspect
./configure
make
```



## 4. Running ParaCrash

### 4.1 POSIX Workloads

```shell
## load packages
spack load py-numpy

## run ParaCrash with the generate config files
## check crash-consistency bugs of the ARVR workload
cd ~/software/ParaCrash/ParaCrash
./pfs_check -f configs/beegfs_4.cfg -d workloads/arvr -m check -r
## add -NR for faster exploration, but it may report false positives
## ./pfs_check -f configs/beegfs_4.cfg -d workloads/arvr -m check -r -NR

## output
Running in check mode
Initializing PFS state
Initiated
...
Total 25 frontier combinations
Total 977 crash states
Total 977 crash states
Total 180 crash states to explore
...
Test passed:(append:12,) ([append:12], append:20, 'FS') [creat:0, append:1, link:2, creat:3, append:4, rename:5, creat:10, creat:19, append:20]
Test failed:(creat:10,) ([creat:10, append:12], append:20, 'FS') [creat:0, append:1, link:2, creat:3, append:4, rename:5, creat:19, append:20]
...
Total 977 crash states, 90 states explored, 2 vulnerabilities in ext3-dj
Total exploration time: 235.17, deducted time 0.00

## detailed logs
ls workloads/arvr/result/
# *.strace are the PFS traces
# exec_graph*.gv* are graphs showing the causality order between I/O calls
# errs.log is the err log
# errs/ list all the err images
```



### 4.2 HDF5 Workloads

```shell
## load packages
spack load py-h5py 

## run ParaCrash with the generate config files
## check crash-consistency bugs of the ARVR workload
cd ~/software/ParaCrash/ParaCrash
./pfs_check -f configs/beegfs_4.cfg -d workloads/hdf5-create -m check -r -h5

## detailed logs
ls workloads/arvr/result/
# *.strace are the PFS traces
# exec_graph*.gv* are graphs showing the causality order between I/O calls
# errs.log is the err log
```

### 4.3 Interpreting Results
For example, the ParaCrash bug report of hdf5-create has the following two sections. It shows that the operation pwrite64:8 has to be persisted before pwrite64:12 to avoid consistency bugs. In the 2nd section, we show the mapping from HDF5 objects to each I/O call reported by h5inspect. Specifically, pwrite64:8 modifies b-tree nodes and local heap; and pwrite64:12 modifies symbol table node. By combining these two sections, ParaCrash shows that writing b-tree nodes and local heap should be persisted before the write to symbol table node. 

```shell
$ ./pfs_check -f configs/beegfs_4.cfg -d workloads/hdf5-create/ -m check -r -NR -h5
...
==== Bug report ====
Re-order bugs
(pwrite64:8, pwrite64:12)
No Atomicity bugs

==== All I/O calls ====
...
pwrite64:8 [('_GROUP /bar/', 'BTREE_NODES'), ('_GROUP /bar/', 'LOCAL_HEAP'), ('GLOBAL', 'FREE_SPACE')]
pwrite64:12 [('_GROUP /bar/', 'SYMBOL_TABLE')]
...
```

## 5. OrangeFS Installation
```shell
# install dependencies
sudo apt install flex bison perl
sudo apt install libdb-dev libattr1-dev linux-headers-`uname -r`
# install OrangeFS packages (linux kernel 4.15 recommended)
wget http://download.orangefs.org/releases/2.9.7/source/orangefs-2.9.7.tar.gz
tar -xf orangefs-2.9.7.tar.gz
cd orangefs-2.9.7/
./configure --prefix=/opt/orangefs --with-kernel=/usr/src/linux-headers-`uname -r`  -enable-shared
make -j 
sudo make install
sudo modprobe orangefs
sudo chown -R $USER /opt/orangefs/
sudo mkdir /mount/orangefs

# add /opt/orangefs/lib to $LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/orangefs/lib

# generate config files
sudo mkdir /data/orangefs/
sudo chown -R $USER /data/orangefs/
sudo chown -R $USER /var/log/

cd $PARACRASH_PATH/ParaCrash/scripts
python3 orangefs-config.py [hostname] 2
./orangefs.sh [hostname] 2
sudo chown -R $USER /mnt/orangefs

# run workload
# do not add -NR for OrangeFS
cd $PARACRASH_PATH/ParaCrash/
./pfs_check -f configs/orangefs_4.cfg -d workloads/wal -m check -r
```

## 6. GlusterFS Installation
```shell
# install GlusterFS packages 
wget -O - https://download.gluster.org/pub/gluster/glusterfs/5/rsa.pub | sudo apt-key add -
sudo apt update
sudo apt install glusterfs-server
sudo mkdir -p /data/glusterfs/
sudo chown -R $USER /data/glusterfs/
sudo mkdir /mount/glusterfs

# start gluster daemon
sudo systemctl start glusterd

# start glusterfs and generate config files
# here we provide a config file for a two-server GlusterFS
cd $PARACRASH_PATH/ParaCrash/scripts/
./glusterfs.sh [hostname] 2
cp gluster_4.cfg $PARACRASH_PATH/ParaCrash/configs

# run workload
# do not add -NR for GlusterFS
# for GlusterFS, ParaCrash has to run in sudo mode
# this is because we have to set some trusted xattr for GlusterFS files during replay
cd $PARACRASH_PATH/ParaCrash/
sudo env PARACRASH_PATH=$PARACRASH_PATH LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./pfs_check -f configs/gluster_4.cfg -d workloads/cr -m check -r 
```