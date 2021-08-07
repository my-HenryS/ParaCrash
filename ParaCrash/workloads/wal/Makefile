all : init workload

init : src/init.c
	g++ src/init.c -o bin/init

workload : src/write_ahead_logging.c
	g++ src/write_ahead_logging.c -o bin/workload

clean : 
	rm -f bin/init bin/workload

.PHONY: all clean
