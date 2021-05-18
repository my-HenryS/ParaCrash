set -x
n=$1
prefix=/data/beegfs/

sudo service beegfs-mgmtd stop

for i in $(seq 1 $n) 
do
	sudo service beegfs-storage@node${i} stop
done

for i in $(seq 1 $n) 
do
	sudo service beegfs-meta@node${i} stop
done


set +x
