h5inspect is built on h5check to report the mapping from low-level HDF5 data structures of an HDF5 file to their file locations. h5inspect depends on HDF5, so make sure you have the HDF5 library installed.

To compile, run 

```shell
./configure
make
```

To use h5inspect, run 

```shell
h5check -l mapping.log example.h5
```

An example output:

{"SUPERBLOCK": [0, 96]

,"_GROUP /":{

​        "BASE": 96

​        ,"OBJ_HEADER": [96, 136]

​        ,"BTREE_NODES": [[136, 184]]

​        ,"SYMBOL_TABLE": [[1504, 1832]]

​        ,"LOCAL_HEAP": [680, 712]

​        ,"DATA_SEGMENT": [712, 800]

}}