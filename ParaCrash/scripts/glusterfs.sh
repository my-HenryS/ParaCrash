volname=myvol1
host=$1
n=$2

sudo gluster volume stop $volname --mode=script
sudo gluster volume delete $volname --mode=script
sudo umount /mnt/glusterfs
sudo rm -rf /data/glusterfs/$volname/

for i in $(seq 1 $n);
    do mkdir -p /data/glusterfs/$volname/brick${i}/brick
done

sudo gluster volume create $volname stripe 2 $host:/data/glusterfs/$volname/brick1/brick $host:/data/glusterfs/$volname/brick2/brick force
sudo gluster volume set $volname storage.health-check-interval 0
sudo gluster volume set $volname cluster.stripe-block-size 128KB

sudo gluster volume start $volname
sudo mount -t glusterfs $host:/$volname /mnt/glusterfs

sudo chown -R $USER /mnt/glusterfs
# sudo chmod -R 755 /data/glusterfs/