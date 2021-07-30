# ParaCrash v1.0

ParaCrash aims at testing crash vulnerabilities of parallel file systems and I/O libraries. Currently we implement ParaCrash for BeeGFS/OrangeFS/GlusterFS, and provide a number of test suites. 
## 1. BeeGFS Installation

```shell
# install BeeGFS packages 

wget -q https://www.beegfs.io/release/latest-stable/gpg/DEB-GPG-KEY-beegfs -O- | sudo apt-key add -
wget https://www.beegfs.io/release/beegfs_7.2.3/dists/beegfs-deb9.list
sudo mv beegfs-deb9.list /etc/apt/sources.list.d
sudo apt install beegfs-mgmtd beegfs-meta beegfs-storage beegfs-client beegfs-utils beegfs-helperd

# generate a four-server BeeGFS config files and ParaCrash config file 
sudo chown -R $USER /etc/beegfs/
cd ~/software/ParaCrash/ParaCrash/scripts
python3 beegfs-config.py 2

# startup beegfs in multi-mode
# set sysMountSanityCheckMS = 0 before start
sudo mkdir -p /data/beegfs
sudo chown -R $USER /data/beegfs/
sudo service beegfs-helperd start
sudo service beegfs-client restart
./beegfs.sh 2
```



## 2. Software Dependencies

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



## 3. Running ParaCrash

### 3.1 POSIX workloads

```shell
## load packages
spack load py-numpy

## run ParaCrash with the generate config files
## check crash-consistency bugs of the ARVR workload
cd ~/software/ParaCrash/ParaCrash
./pfs_check -f configs/vm2_beegfs_4.cfg -d workloads/arvr -m check -r
## add -NR for faster exploration, but it may report false positives
## ./pfs_check -f configs/vm2_beegfs_4.cfg -d workloads/arvr -m check -r -NR

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



### 3.2 HDF5 workloads

```shell
## load packages
spack load py-h5py 

## run ParaCrash with the generate config files
## check crash-consistency bugs of the ARVR workload
cd ~/software/ParaCrash/ParaCrash
./pfs_check -f configs/vm2_beegfs_4.cfg -d workloads/hdf5-create -m check -r -h5

## detailed logs
ls workloads/arvr/result/
# *.strace are the PFS traces
# exec_graph*.gv* are graphs showing the causality order between I/O calls
# errs.log is the err log
```


