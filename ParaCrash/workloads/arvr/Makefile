all : init workload

init : src/init.c
	g++ src/init.c -o bin/init

workload : src/atomic_replace.c
	g++ src/atomic_replace.c -o bin/workload

clean : 
	rm -f bin/init bin/workload

.PHONY: all clean
