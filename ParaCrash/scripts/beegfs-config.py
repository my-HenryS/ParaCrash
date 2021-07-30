from configobj import ConfigObj
import os
import configparser
import sys

def clear(config):
    for k, v in config.items():
        if config[k] == "":
            del config[k]

n = int(sys.argv[1])

base_meta_port = 8010
pfs_config = ConfigObj("beegfs-meta.conf")
for i in range(1, n+1):
    pfs_config_new = ConfigObj(pfs_config)
    pfs_config_new.filename = "/etc/beegfs/node%d.d/beegfs-meta.conf" % i
    pfs_config_new["connMetaPortTCP"] = base_meta_port + i - 1
    pfs_config_new["connMetaPortUDP"] = base_meta_port + i - 1
    pfs_config_new["storeMetaDirectory"] = "/data/beegfs/meta%d" % i 
    if not os.path.exists("/etc/beegfs/node%d.d/"  % i):
        os.mkdir("/etc/beegfs/node%d.d/"  % i)
    clear(pfs_config_new)
    pfs_config_new.write()

base_storage_port = 8040
pfs_config = ConfigObj("beegfs-storage.conf")
for i in range(1, n+1):
    pfs_config_new = ConfigObj(pfs_config)
    pfs_config_new.filename = "/etc/beegfs/node%d.d/beegfs-storage.conf" % i
    pfs_config_new["connStoragePortTCP"] = base_storage_port + i - 1
    pfs_config_new["connStoragePortUDP"] = base_storage_port + i - 1
    pfs_config_new["storeStorageDirectory"] = "/data/beegfs/store%d" % i
    if not os.path.exists("/etc/beegfs/node%d.d/" % i):
        os.mkdir("/etc/beegfs/node%d.d/"  % i)
    clear(pfs_config_new)
    pfs_config_new.write()


paracrash_config = configparser.ConfigParser()
paracrash_config["global"] = {
    "mount_point" : "/mnt/beegfs",
    "client_name" : "beegfs-client",
    "type" : "beegfs", 
    "stripe_size" : 131072,
    "services" : ""
}

for i in range(1, n+1):
    paracrash_config["global"]["services"] += "beegfs-meta@node%d,  beegfs-storage@node%d," % (i, i)
    paracrash_config["beegfs-meta@node%d" % i] = {
        "type" : "metadata",
        "exec" : "beegfs-meta",
        "tag" : "node%d" % i,
        "host" : "node%d" % i,
        "data_path" : "/data/beegfs/meta%d" % i,
        "data_dirs" : "dentries, inodes"
    }
    paracrash_config["beegfs-storage@node%d" % i] = {
        "type" : "storage",
        "exec" : "beegfs-storage",
        "tag" : "node%d" % i,
        "host" : "node%d" % i,
        "data_path" : "/data/beegfs/store%d" % i,
        "data_dirs" :  "chunks"
    }

paracrash_config["global"]["services"] = paracrash_config["global"]["services"][:-1]

with open("/home/sc21/software/ParaCrash/ParaCrash/configs/vm2_beegfs_%d.cfg" % (n*2), 'w') as configfile:
    paracrash_config.write(configfile)