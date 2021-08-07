kill -9 $(pidof pvfs2-server)
sudo kill -9 $(pidof pvfs2-client)
sudo kill -9 $(pidof pvfs2-client-core)

host=$1
n=$2
base_meta_port=3334
base_storage_port=3364
config=/opt/orangefs/etc/orangefs-server_$(($n*2)).conf

rm -rf /data/orangefs/*
for i in $(seq 1 $n) 
do
	/opt/orangefs/sbin/pvfs2-server $config -f -a storage-$i
	/opt/orangefs/sbin/pvfs2-server $config -f -a meta-$i

	/opt/orangefs/sbin/pvfs2-server $config -a storage-$i
	/opt/orangefs/sbin/pvfs2-server $config -a meta-$i
done

#sudo modprobe orangefs
sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH /opt/orangefs/sbin/pvfs2-client
sleep 2
sudo mount -t pvfs2 tcp://$host:3334/orangefs /mnt/orangefs