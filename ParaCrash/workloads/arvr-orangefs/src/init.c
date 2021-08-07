#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>


int main() {
	int fd = open("file1", O_CREAT | O_RDWR, 0666);
	assert(fd > 0);
	int ret = write(fd, "hello\n", 6);
	assert(ret == 6);
	ret = close(fd);
	assert(ret == 0);
	printf("Initiated\n");
}
