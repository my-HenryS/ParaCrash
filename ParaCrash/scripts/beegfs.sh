set -x
n=$1
prefix=/data/beegfs/

sudo service beegfs-mgmtd stop
sudo rm -rf $prefix/mgmtd
sudo service beegfs-mgmtd start

for i in $(seq 1 $n) 
do
	sudo service beegfs-meta@node${i} stop 
	sudo rm -rf $prefix/meta${i}
	mkdir $prefix/meta${i}
	sudo /opt/beegfs/sbin/beegfs-setup-meta -p $prefix/meta${i} -S node${i} -C -x 
	sudo service beegfs-meta@node${i} start 
done

for i in $(seq 1 $n) 
do
	sudo service beegfs-storage@node${i} stop 
	sudo rm -rf $prefix/store${i}
	mkdir $prefix/store${i}
	sudo /opt/beegfs/sbin/beegfs-setup-storage -p $prefix/store${i} -S node${i} -C
	sudo service beegfs-storage@node${i} start 
done

sudo beegfs-ctl --setpattern --chunksize=128k /mnt/beegfs

set +x
