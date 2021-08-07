/* The following is a toy application that performs write ahead logging */

#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>


int main() {
	int fd = open("log", O_CREAT | O_RDWR, 0666);
	assert(fd > 0);

	int off = 65536 * 2 ;
	int ret = write(fd, "2-3-foo\n", 8);
	assert(ret == 8);

	ret = close(fd);
	assert(ret == 0);

	int fd_2 = open("file2", O_RDWR, 0666);
	assert(fd > 0);

	ret = pwrite(fd_2, "bar\n", 4, off+2);
	assert(ret == 4);

	ret = pwrite(fd_2, "boo\n", 4, off + 2);
	assert(ret == 4);

	ret = close(fd_2);
	assert(ret == 0);

	ret = unlink("log");
	assert(ret == 0);

}
