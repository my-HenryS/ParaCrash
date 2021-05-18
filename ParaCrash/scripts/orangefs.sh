kill -9 $(pidof pvfs2-server)
sudo kill -9 $(pidof pvfs2-client)
sudo kill -9 $(pidof pvfs2-client-core)

n=$1
base_meta_port=3334
base_storage_port=3364
config=$HOME/local/opt/orangefs/etc/orangefs-server_$(($1*2)).conf

rm -rf /data/orangefs/*
for i in $(seq 1 $1) 
do
	pvfs2-server $config -f -a vm2-storage-$i
	pvfs2-server $config -f -a vm2-meta-$i

	pvfs2-server $config -a vm2-storage-$i
	pvfs2-server $config -a vm2-meta-$i
done

#sudo modprobe orangefs
sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH $HOME/local/sbin/pvfs2-client
