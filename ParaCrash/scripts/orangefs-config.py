import xml.etree.ElementTree as ET
import re
import sys
import configparser
import os


if len(sys.argv) != 3:
    print("Usage: python3 orangefs-config.py [host] [#_of_servers]")
    exit(0)
host = sys.argv[1]
n = int(sys.argv[2])
paracrash_path = os.environ['PARACRASH_PATH']

with open("orangefs-server.conf") as f:
    xml = f.read()
root = ET.fromstring("<root>" + xml + "</root>")
tree = ET.ElementTree(root)

base_meta_port = 3334
base_storage_port = 3364
base_meta_range = 3
interval = 614891469123651720/4
base_storage_range = base_meta_range+interval*n

for child in root:
    if child.tag == "Aliases":
        child.text = "\n"
        for i in range(1,n+1):
            child.text += "\tAlias meta-%d tcp://%s:%d\n" % (i, host, base_meta_port+i-1)
            child.text += "\tAlias storage-%d tcp://%s:%d\n" % (i, host, base_storage_port+i-1)

    if child.tag == "FileSystem":
        for grandchild in child:
            if grandchild.tag == "MetaHandleRanges":
                grandchild.text = "\n"
                for i in range(1,n+1):
                    grandchild.text += "\t\tRange meta-%d %d-%d\n" \
                                        % (i, base_meta_range, base_meta_range+interval-1)
                    base_meta_range+=interval
                grandchild.text += "\t"
            if grandchild.tag == "DataHandleRanges":
                grandchild.text = "\n"
                for i in range(1,n+1):
                    grandchild.text += "\t\tRange storage-%d %d-%d\n" \
                                        % (i, base_storage_range, base_storage_range+interval-1)
                    base_storage_range+=interval
                grandchild.text += "\t"

for i in range(1,n+1):
    child = ET.Element("ServerOptions")
    meta_port = base_meta_port+i-1
    child.text = "\n\tServer meta-%d\n\tMetadataStorageSpace /data/orangefs/storage-%d/meta\n\tDataStorageSpace /data/orangefs/storage-%d/data\n\tLogFile /var/log/orangefs-server-%d.log\n" % (i, meta_port, meta_port, meta_port)
    child.tail = "\n"
    root.append(child)

    child = ET.Element("ServerOptions")
    storage_port = base_storage_port+i-1
    child.text = "\n\tServer storage-%d\n\tMetadataStorageSpace /data/orangefs/storage-%d/meta\n\tDataStorageSpace /data/orangefs/storage-%d/data\n\tLogFile /var/log/orangefs-server-%d.log\n" % (i, storage_port, storage_port, storage_port)
    child.tail = "\n"
    root.append(child)
  
    
f = open("/opt/orangefs/etc/orangefs-server_%d.conf"  % (2*n), "w+")
f.write(ET.tostring(root, encoding='unicode', method='xml')[6:-7])
f.close()
# tree.write('/home/jhsun/local/opt/orangefs/etc/orangefs-server_%d.conf' % (2*n), pretty_print=True)


# paracrash config


paracrash_config = configparser.ConfigParser()
paracrash_config["global"] = {
    "mount_point" : "/mnt/orangefs",
    "client_name" : "orange-client",
    "type" : "orangefs", 
    "stripe_size" : 131072,
    "services" : ""
}

for i in range(1, n+1):
    paracrash_config["global"]["services"] += "orange-meta-%d,  orange-storage-%d," % (i, i)
    paracrash_config["orange-meta-%d" % i] = {
        "type" : "metadata",
        "exec" : "pvfs2-server",
        "tag" : "meta-%d" % (i),
        "host" : "%s:%d" % (host, base_meta_port+i-1),
        "data_path" : "/data/orangefs/storage-%d" % (base_meta_port+i-1),
        "data_dirs" : "data, meta"
    }
    paracrash_config["orange-storage-%d" % i] = {
        "type" : "storage",
        "exec" : "pvfs2-server",
        "tag" : "storage-%d" % (i),
        "host" : "%s:%d" % (host, base_storage_port+i-1),
        "data_path" : "/data/orangefs/storage-%d" % (base_storage_port+i-1),
        "data_dirs" : "data, meta"
    }

paracrash_config["global"]["services"] = paracrash_config["global"]["services"][:-1]


config_path = "%s/ParaCrash/configs/" % paracrash_path
if not os.path.exists(config_path):
    os.mkdir(config_path)

with open("%s/vm2_orangefs_%d.cfg" % (config_path, n*2), 'w') as configfile:
    paracrash_config.write(configfile)
