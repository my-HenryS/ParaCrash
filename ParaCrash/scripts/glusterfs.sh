kill -9 $(pidof pvfs2-server)
sudo kill -9 $(pidof pvfs2-client)
sudo kill -9 $(pidof pvfs2-client-core)

config=$HOME/local/opt/orangefs/etc/orangefs-server.conf

pvfs2-server $config -f -a vm2-storage-2
pvfs2-server $config -f -a vm2-storage-1
pvfs2-server $config -f -a vm2-meta-1
pvfs2-server $config -f -a vm2-meta-2

pvfs2-server $config -a vm2-storage-2
pvfs2-server $config -a vm2-storage-1
pvfs2-server $config -a vm2-meta-1
pvfs2-server $config -a vm2-meta-2

#sudo modprobe orangefs
sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH $HOME/local/sbin/pvfs2-client
