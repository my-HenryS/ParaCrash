/* The following is a toy application that (tries to) atomically update a file,
 * then prints a message, and then (tries to) atomically create two links */

#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>


int main() {
	int fd = open("tmp", O_CREAT | O_RDWR, 0666);
	assert(fd > 0);
	int ret = pwrite(fd, "world\n", 6, 0);
	assert(ret == 6);
	//ret = pwrite(fd, "woold\n", 6, 0);
	//assert(ret == 6);
	close(fd);
    //fsync(fd);
	ret = rename("tmp", "file1");
	assert(ret == 0);
}
