# ParaCrash

ParaCrash aims at testing crash vulnerabilities of parallel file systems. Currently we implement ParaCrash for BeeGFS/OrangeFS/GlusterFS, and provide a number of test suites. 

ParaCrash does not require BeeGFS to deploy metadata service and storage service on separate machines. However, it requires root privilege as it relies on strace, BeeGFS-ctl and BeeGFS-fsck.

Please refer to https://www.beegfs.io/wiki/ManualInstallation for BeeGFS installation.

To run pfs_record, user shall specify configuration of beegfs and path to workload directory. Snapshots will be in $wkdir/storage/snapshots. Raw traces will be in $wkdir/storage/traces

./pfs_check -f beegfs.cfg -d wkdir -m record

To restore BeeGFS to its initial state, user shall specify configuration of beegfs and path to workload directory.

./pfs_check -f beegfs.cfg -d wkdir -m restore

To run pfs_check, user shall also specify configuration of beegfs and path to workload directory.

./pfs_check -f beegfs.cfg -d wkdir -m check
